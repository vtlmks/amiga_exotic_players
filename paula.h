// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Amiga 500 Paula emulator for custom replayers.
//
// This is a hardware model, not a resampler. The channel mixer runs in the
// Paula clock domain (3546895 Hz PAL / 3579545 Hz NTSC). Each hardware
// channel has a period counter that, when it expires, latches the next 8-bit
// sample byte; between latches the channel holds that byte (the zero-order-
// hold staircase a real Paula produces). Volume is the real 6-bit PWM over a
// 64-clock window, not a multiply, so its quantization noise is reproduced.
// Channels 0+3 are summed to the left output, 1+2 to the right, hard-panned,
// in the Paula clock domain. The analog filter chain (always-on RC low-pass --
// ~4.4 kHz on A500, ~34 kHz on A1200 -- plus the switchable ~3.3 kHz LED
// Butterworth) runs at the Paula clock rate. Only the final stage decimates
// to the host rate, by box-filter integration of the Paula-clock samples that
// fall in each output window. Aliasing and quantization noise that a real
// Amiga produces are preserved.
//
// Paula has exactly four hardware channels (0..3). There is no software-
// mixer extension and no side bus: a real Amiga has no extra channel in its
// signal path. Every format with more than four voices built a mixed buffer
// on the CPU and DMA'd THAT through these four channels, so any such mixdown
// is the replayer's job and its output IS Paula channel sample data, <= 4
// channels, passing through this same hardware path.
//
// Output is ACCUMULATED into the caller's float buffer. The hardware output
// chain is modelled end to end, to the RCA jack, not just to the summer node:
//
//   1. Resistive averaging summer: the two channels on each side (0+3 left,
//      1+2 right) join through equal board resistors, so the per-side node
//      is (ch_a + ch_b) / 2 (a ~6 dB attenuation).
//   2. The A500 analog filter chain (fixed ~4.4 kHz RC, switchable ~3.3 kHz
//      LED Butterworth) acts on that node.
//   3. Output buffer/amp: normalises int8 full scale to unity (no make-up
//      gain over the resistive divider), then soft saturation into the
//      supply rails. The divider is left uncompensated on purpose: it puts
//      a single full-scale channel at ~0.5 (well clear of the 0.8 knee,
//      fully linear) and two correlated full-scale channels on the same
//      side at ~1.0 (just into the knee). So the saturator engages only on
//      genuinely hot correlated multi-channel content -- as a real A500
//      measurably does -- not on ordinary single/normal-level material.
//      It is a soft knee, not a hard clip (which would synthesise harmonics
//      the machine never produces) and not clip-free. Absolute level is the
//      host's concern; this trades ~6 dB of headroom for a faithful
//      saturation onset.

#pragma once

#include <stdint.h>
#include <string.h>
#include <math.h>

// L+R packed double, used through the filter chain to halve biquad cost: all
// three filter stages use identical coefficients per side, only state differs,
// so each biquad line becomes one packed instruction (one packed FMA on
// x86-64-v3). GCC/Clang vector extension; arithmetic operators are overloaded
// to the right SIMD ops per -march.
typedef double paula_v2df __attribute__((vector_size(16)));

// Opt-in mixer profiler. Compiled in only when PAULA_PROFILE is defined, so
// normal builds carry zero footprint. Accumulates process CPU time spent
// strictly inside paula_mix_frames (not replayer tick work) and the number of
// frames produced; paula_profile_report() turns that into a realtime factor.
#ifdef PAULA_PROFILE
#include <stdio.h>
#include <time.h>
static double paula_profile_cpu_ns = 0.0;
static uint64_t paula_profile_frames = 0;
#endif

// Paula has exactly four hardware channels. Formats with more voices must
// CPU-mix down to <= 4 themselves; there is no extra channel here.
#define PAULA_NUM_CHANNELS 4
#define PAULA_PAL_CLOCK    3546895
#define PAULA_NTSC_CLOCK   3579545
// Real Paula audio DMA floor. The Hardware Reference Manual's period-124
// figure is the rate at which all four channels can DMA without the bus
// falling behind during display fetch; a single channel goes lower. The true
// hardware floor is period 113 -- the ProTracker/Soundtracker note table
// bottoms at exactly 113 (B-3) because that is where Paula stops. Clamping to
// 124 detunes the whole top octave flat (period 113 -> ~160 cents). All four
// hardware channels clamp to this.
#define PAULA_DMA_MIN_PERIOD 113

// Period accumulator fixed-point: one Paula clock advances the accumulator by
// PAULA_PERIOD_ONE; a channel consumes one sample byte every period_q of
// these. period_q is integer-exact for the Paula register path and fractional
// for the Hz path, so both keep exact pitch.
#define PAULA_PERIOD_SHIFT 16
#define PAULA_PERIOD_ONE   (1ull << PAULA_PERIOD_SHIFT)

struct paula_channel {
	int8_t *sample;
	uint32_t length;           // bytes (becomes loop_start+loop_length after first wrap)
	uint32_t loop_start;       // bytes
	uint32_t loop_length;      // bytes, 0 => one-shot
	uint32_t pos;              // current byte index into sample
	uint64_t period_q;         // Paula clocks per sample byte, Q16
	uint64_t period_acc;       // period accumulator, Q16
	int8_t *pending_sample;    // deferred switch on next wrap (Paula AUDxLC trick)
	uint32_t pending_pos;
	uint32_t pending_length;
	int8_t cur;                // latched sample byte (zero-order-hold output)
	uint16_t volume;           // 0..64 Amiga scale
	uint8_t pwm_cnt;           // 0..63 volume-PWM phase
	uint8_t active;
	uint8_t muted;
	uint8_t has_pending;
	uint8_t backwards;         // 1 -> step DOWN through sample (DBP E3, etc.)
};

// Amiga model. Selects the always-on post-DAC RC low-pass corner: ~4.4 kHz on
// A500 (the classic muffled top end), ~34 kHz on A1200 (bright but not brick-
// walled -- the slight roll into the top octave that real hardware has,
// neither aliasing brightness nor A500 muffling). The LED filter exists on
// both. Default is the A500.
#define PAULA_MODEL_A500   0
#define PAULA_MODEL_A1200  1

struct paula {
	struct paula_channel ch[PAULA_NUM_CHANNELS];
	int32_t sample_rate;       // host output rate
	int32_t clock;             // Paula clock (PAL/NTSC), internal mix rate
	int32_t samples_per_tick;
	int32_t tick_offset;
	int32_t model;

	// Box-filter decimation from the Paula clock domain to the host rate.
	// Each output sample averages the decim_step (Q16) Paula clocks that
	// fall in its window; decim_phase carries the fraction across calls so
	// the clock count alternates with no pitch drift.
	uint64_t decim_step;
	uint64_t decim_phase;

	// Always-on 1-pole RC low-pass, at the Paula clock rate. Corner depends
	// on model: ~4.4 kHz for A500, ~34 kHz for A1200. State is L+R packed.
	double fixed_lp_a;
	paula_v2df fixed_lp;

	// Switchable LED filter: 2-pole Butterworth low-pass (~3.3 kHz,
	// Q=1/sqrt(2)), at the Paula clock rate, RBJ bilinear coefficients.
	// Driven by the replayer via paula_set_lp_filter; biquad state (TDF-II,
	// L+R packed) persists across toggles so flips don't click.
	int32_t lp_filter_on;
	double led_b0;
	double led_b1;
	double led_b2;
	double led_a1;
	double led_a2;
	paula_v2df led_z1;
	paula_v2df led_z2;

	// Decimation anti-alias low-pass: 8th-order Butterworth (4 cascaded RBJ
	// biquads) at 0.45*host_rate, run in the Paula clock domain just before the
	// rate drop. This is a resampler reconstruction filter, NOT modelled
	// hardware: it bandlimits to below the host Nyquist so the box-average
	// decimation cannot fold ultrasonic ZOH images down into the audible band.
	// Keyed to host_rate, so it runs for both models; on the A500 the analog
	// chain has already removed everything near Nyquist, making it a no-op.
	// State is L+R packed per stage.
	double aa_b0[4];
	double aa_a1[4];
	double aa_a2[4];
	paula_v2df aa_z1[4];
	paula_v2df aa_z2[4];
};

// [=]===^=[ paula_recalc ]=======================================================================[=]
// Recompute every rate-dependent coefficient from p->clock and
// p->sample_rate. The analog filters run at the Paula clock, so their
// coefficients are bilinear-transformed for that rate, not the host rate.
static void paula_recalc(struct paula *p) {
	double fs = (double)p->clock;
	double dt = 1.0 / fs;

	// Always-on RC low-pass. Corner is model-dependent: A500 ~4.4 kHz,
	// A1200 ~34 kHz. a = dt / (RC + dt).
	double lp_fc = (p->model == PAULA_MODEL_A1200) ? 34000.0 : 4400.0;
	double lp_rc = 1.0 / (2.0 * 3.14159265358979323846 * lp_fc);
	p->fixed_lp_a = dt / (lp_rc + dt);

	// LED filter: 2-pole Butterworth low-pass, ~3.3 kHz, Q = 1/sqrt(2),
	// RBJ cookbook low-pass mapped via the bilinear transform at fs.
	double fc = 3300.0;
	double q = 0.70710678118654752440;
	double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
	double cw = cos(w0);
	double sw = sin(w0);
	double alpha = sw / (2.0 * q);
	double a0 = 1.0 + alpha;
	p->led_b0 = ((1.0 - cw) * 0.5) / a0;
	p->led_b1 = (1.0 - cw) / a0;
	p->led_b2 = ((1.0 - cw) * 0.5) / a0;
	p->led_a1 = (-2.0 * cw) / a0;
	p->led_a2 = (1.0 - alpha) / a0;

	// Decimation anti-alias: 8th-order Butterworth low-pass at 0.45*host_rate,
	// mapped via RBJ bilinear at the Paula clock. The four sections carry the
	// standard 8th-order Butterworth section Q's; cascaded DC gain is unity.
	double aa_q[4] = {0.50979558, 0.60134489, 0.89997622, 2.56291545};
	double aa_w0 = 2.0 * 3.14159265358979323846 * (0.45 * (double)p->sample_rate) / fs;
	double aa_cw = cos(aa_w0);
	double aa_sw = sin(aa_w0);
	for(uint32_t st = 0; st < 4; ++st) {
		double al = aa_sw / (2.0 * aa_q[st]);
		double a0 = 1.0 + al;
		p->aa_b0[st] = ((1.0 - aa_cw) * 0.5) / a0;
		p->aa_a1[st] = (-2.0 * aa_cw) / a0;
		p->aa_a2[st] = (1.0 - al) / a0;
	}

	// Box-filter decimation step: Paula clocks per host output sample, Q16.
	p->decim_step = ((uint64_t)p->clock << PAULA_PERIOD_SHIFT) / (uint64_t)p->sample_rate;
}

// [=]===^=[ paula_init ]=========================================================================[=]
static void paula_init(struct paula *p, int32_t sample_rate, int32_t tick_rate_hz) {
	memset(p, 0, sizeof(*p));
	p->sample_rate = sample_rate;
	p->clock = PAULA_PAL_CLOCK;
	p->samples_per_tick = sample_rate / tick_rate_hz;
	p->model = PAULA_MODEL_A500;
	// Hard-panned: channels 0+3 -> left, 1+2 -> right (fixed Paula wiring).
	paula_recalc(p);
}

// [=]===^=[ paula_set_clock ]====================================================================[=]
// Select the Paula clock (PAULA_PAL_CLOCK / PAULA_NTSC_CLOCK). Recomputes the
// rate-dependent coefficients. Default after paula_init is PAL.
static void paula_set_clock(struct paula *p, int32_t clock_hz) {
	p->clock = clock_hz > 0 ? clock_hz : PAULA_PAL_CLOCK;
	paula_recalc(p);
}

// [=]===^=[ paula_set_model ]====================================================================[=]
// Select the emulated machine. The always-on post-DAC RC low-pass corner
// changes with model (A500 ~4.4 kHz, A1200 ~34 kHz); the LED filter exists on
// both. Default is the A500.
static void paula_set_model(struct paula *p, int32_t model) {
	p->model = (model == PAULA_MODEL_A1200) ? PAULA_MODEL_A1200 : PAULA_MODEL_A500;
	paula_recalc(p);
}

// [=]===^=[ paula_set_lp_filter ]================================================================[=]
// Enable or disable the switchable Amiga LED filter (the power-LED-gated
// 2-pole low-pass). Replayers call this to mirror the module's own filter
// state. The fixed RC low-pass is not affected and always runs (A500).
static void paula_set_lp_filter(struct paula *p, int32_t on) {
	p->lp_filter_on = on ? 1 : 0;
}

// [=]===^=[ paula_set_period ]===================================================================[=]
// Amiga AUDxPER (DMA) period. All four hardware channels clamp to the real
// Paula DMA minimum period.
static void paula_set_period(struct paula *p, int32_t idx, uint16_t period) {
	if(period != 0 && period < PAULA_DMA_MIN_PERIOD) {
		period = PAULA_DMA_MIN_PERIOD;
	}
	if(period == 0) {
		p->ch[idx].period_q = 0;
		return;
	}
	p->ch[idx].period_q = (uint64_t)period << PAULA_PERIOD_SHIFT;
}

// [=]===^=[ paula_set_freq_hz ]==================================================================[=]
// Set a channel's DMA rate directly in Hz, for replayers that DMA a
// CPU-built mixdown buffer through a Paula channel (DigiBoosterPro, FaceThe-
// Music). The period is fractional in the Paula clock domain so pitch stays
// exact. No DMA period floor: mixdown rates are well above it anyway.
static void paula_set_freq_hz(struct paula *p, int32_t idx, uint32_t freq_hz) {
	if(freq_hz == 0) {
		p->ch[idx].period_q = 0;
		return;
	}
	p->ch[idx].period_q = ((uint64_t)p->clock << PAULA_PERIOD_SHIFT) / (uint64_t)freq_hz;
}

// [=]===^=[ paula_set_volume ]===================================================================[=]
static void paula_set_volume(struct paula *p, int32_t idx, uint16_t volume) {
	if(volume > 64) {
		volume = 64;
	}
	p->ch[idx].volume = volume;
}

// Volume is passed in 0..256 range in NostalgicPlayer convention; divide to 0..64.
// [=]===^=[ paula_set_volume_256 ]===============================================================[=]
static void paula_set_volume_256(struct paula *p, int32_t idx, uint16_t volume) {
	if(volume > 256) {
		volume = 256;
	}
	p->ch[idx].volume = volume >> 2;
}

// [=]===^=[ paula_play_sample ]==================================================================[=]
static void paula_play_sample(struct paula *p, int32_t idx, int8_t *sample, uint32_t length) {
	struct paula_channel *c = &p->ch[idx];
	c->sample = sample;
	c->length = length;
	c->pos = (c->backwards && length > 0) ? (length - 1) : 0;
	c->loop_start = 0;
	c->loop_length = 0;
	c->has_pending = 0;
	c->pending_sample = 0;
	c->period_acc = 0;
	c->active = (sample != 0) && (length > 0);
	c->cur = c->active ? sample[c->pos] : 0;
}

// [=]===^=[ paula_set_backwards ]================================================================[=]
// Set or clear the backwards-playback flag for a channel. Takes effect on the
// next paula_play_sample (which seeds pos at the high end) and reverses the
// per-byte advance direction.
static void paula_set_backwards(struct paula *p, int32_t idx, int32_t on) {
	p->ch[idx].backwards = on ? 1 : 0;
}

// [=]===^=[ paula_set_pos ]======================================================================[=]
// Move the channel's read position to `byte_offset` within the current sample
// and re-latch the held byte. Used by effects like ProTracker 9xx (sample
// offset). Clamps to [0, length-1].
static void paula_set_pos(struct paula *p, int32_t idx, uint32_t byte_offset) {
	struct paula_channel *c = &p->ch[idx];
	if(c->sample == 0 || c->length == 0) {
		c->pos = 0;
		c->cur = 0;
		return;
	}
	if(byte_offset >= c->length) {
		byte_offset = c->length - 1;
	}
	c->pos = byte_offset;
	c->cur = c->sample[byte_offset];
}

// [=]===^=[ paula_queue_sample ]=================================================================[=]
// If the channel is active, the new sample takes effect when the current one
// reaches length (Amiga "write AUDxLC/AUDxLEN mid-DMA"). If inactive, it
// starts immediately. Plays from sample[start_offset] for `length` bytes,
// then wraps using the channel's current loop_start / loop_length.
static void paula_queue_sample(struct paula *p, int32_t idx, int8_t *sample, uint32_t start_offset, uint32_t length) {
	struct paula_channel *c = &p->ch[idx];
	if(!c->active && sample != 0 && length > 0) {
		c->sample = sample;
		c->pos = start_offset;
		c->length = start_offset + length;
		c->has_pending = 0;
		c->pending_sample = 0;
		c->period_acc = 0;
		c->active = 1;
		c->cur = sample[start_offset];
		return;
	}
	c->pending_sample = sample;
	c->pending_pos = start_offset;
	c->pending_length = start_offset + length;
	c->has_pending = (sample != 0) && (length > 0);
}

// [=]===^=[ paula_set_loop ]=====================================================================[=]
static void paula_set_loop(struct paula *p, int32_t idx, uint32_t start, uint32_t length) {
	struct paula_channel *c = &p->ch[idx];
	c->loop_start = start;
	c->loop_length = length;
}

// [=]===^=[ paula_mute ]=========================================================================[=]
static void paula_mute(struct paula *p, int32_t idx) {
	p->ch[idx].active = 0;
}

// [=]===^=[ paula_ch_advance ]===================================================================[=]
// Consume one sample byte for a channel: step the read position one byte
// (forward or backward), apply the pending-sample swap / loop wrap / one-shot
// stop exactly as Paula DMA does, and re-latch the held byte.
static void paula_ch_advance(struct paula_channel *c) {
	if(!c->backwards) {
		uint32_t np = c->pos + 1;
		if(np >= c->length) {
			if(c->has_pending) {
				c->sample = c->pending_sample;
				np = c->pending_pos;
				c->length = c->pending_length;
				c->has_pending = 0;
				c->pending_sample = 0;
			} else if(c->loop_length > 0) {
				uint32_t over = np - c->length;
				np = c->loop_start + (over % c->loop_length);
				c->length = c->loop_start + c->loop_length;
			} else {
				c->active = 0;
				return;
			}
		}
		c->pos = np;
	} else {
		if(c->pos == 0 || (c->loop_length > 0 && c->pos <= c->loop_start)) {
			if(c->has_pending) {
				c->sample = c->pending_sample;
				c->length = c->pending_length;
				c->pos = c->pending_length - 1;
				c->has_pending = 0;
				c->pending_sample = 0;
			} else if(c->loop_length > 0) {
				c->pos = c->loop_start + c->loop_length - 1;
			} else {
				c->active = 0;
				return;
			}
		} else {
			c->pos = c->pos - 1;
		}
	}
	c->cur = c->sample[c->pos];
}

// [=]===^=[ paula_ch_sample ]====================================================================[=]
// Consume one Paula clock for a single channel: bump the period accumulator,
// advance the read position by as many bytes as the accumulator demands (may
// deactivate a one-shot channel), then return the PWM-gated sample value the
// channel contributes this clock. Returns 0.0 for a channel that is or just
// went inactive, so the caller's accumulator can stay branch-free.
static double paula_ch_sample(struct paula_channel *c) {
	if(!c->active) {
		return 0.0;
	}
	c->period_acc += PAULA_PERIOD_ONE;
	while(c->period_acc >= c->period_q) {
		c->period_acc -= c->period_q;
		paula_ch_advance(c);
		if(!c->active) {
			return 0.0;
		}
	}
	c->pwm_cnt = (uint8_t)((c->pwm_cnt + 1) & 63);
	int32_t v = (c->pwm_cnt < c->volume) ? (int32_t)c->cur : 0;
	return (double)v;
}

// [=]===^=[ paula_softclip ]=====================================================================[=]
// Soft saturation of the A500 output buffer/amp into its supply rails. Unity
// (identity) for |x| <= t so single-channel and typical multi-channel levels
// are unaffected; above t it bends smoothly (C1-continuous, slope 1 at the
// knee) and asymptotes to +/-1. Aggressive correlated multi-channel content
// is compressed into the rails the way a real A500 does -- not a hard clip,
// which would synthesise harmonics the machine never produces.
static double paula_softclip(double x) {
	double t = 0.8;
	double a = (x < 0.0) ? -x : x;
	if(a <= t) {
		return x;
	}
	double s = (x < 0.0) ? -1.0 : 1.0;
	return s * (t + (1.0 - t) * tanh((a - t) / (1.0 - t)));
}

// [=]===^=[ paula_mix_frames ]===================================================================[=]
// Accumulates `frames` float stereo frames into `output`. Caller must
// pre-clear. The inner loop runs at the Paula clock; each output frame is the
// box-filter average of the Paula-clock samples in its window.
//
// L+R run packed as paula_v2df through the analog/AA chain: all three filter
// stages share coefficients across sides, only state differs, so each biquad
// line is one packed instruction (one packed FMA on x86-64-v3). Softclip is
// the only extract/repack point (conditional + tanh, doesn't vectorise).
static void paula_mix_frames(struct paula *p, float *output, int32_t frames) {
#ifdef PAULA_PROFILE
	struct timespec prof_t0;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &prof_t0);
#endif
	int32_t led = p->lp_filter_on;
	paula_v2df fa = {p->fixed_lp_a, p->fixed_lp_a};
	paula_v2df fl = p->fixed_lp;
	paula_v2df lb0 = {p->led_b0, p->led_b0};
	paula_v2df lb1 = {p->led_b1, p->led_b1};
	paula_v2df lb2 = {p->led_b2, p->led_b2};
	paula_v2df la1 = {p->led_a1, p->led_a1};
	paula_v2df la2 = {p->led_a2, p->led_a2};
	paula_v2df lz1 = p->led_z1;
	paula_v2df lz2 = p->led_z2;
	paula_v2df ab0[4];
	paula_v2df aa1[4];
	paula_v2df aa2[4];
	paula_v2df az1[4];
	paula_v2df az2[4];
	for(uint32_t st = 0; st < 4; ++st) {
		ab0[st] = (paula_v2df){p->aa_b0[st], p->aa_b0[st]};
		aa1[st] = (paula_v2df){p->aa_a1[st], p->aa_a1[st]};
		aa2[st] = (paula_v2df){p->aa_a2[st], p->aa_a2[st]};
		az1[st] = p->aa_z1[st];
		az2[st] = p->aa_z2[st];
	}
	paula_v2df two  = {2.0, 2.0};
	paula_v2df half = {0.5, 0.5};
	// amp_gain normalises int8 full scale (128) to 1.0 and deliberately does
	// NOT make up the resistive divider's 6 dB: a single full-scale channel
	// lands at ~0.5 (linear, clear of the 0.8 soft knee) and two correlated
	// full-scale channels on a side at ~1.0 (just into the knee), so the
	// saturator only engages on genuinely hot correlated content the way a
	// real A500 does -- not on ordinary single/normal-level material. Output
	// is the box-filter average over the window (/n).
	paula_v2df amp = {1.0 / 128.0, 1.0 / 128.0};
	uint64_t phase = p->decim_phase;
	uint64_t dstep = p->decim_step;

	// Active-channel working set, split by side. The selection predicate
	// (active / unmuted / has sample / nonzero period) is stable within a
	// mix call: only `active` can drop when a one-shot sample ends mid-call,
	// which paula_ch_sample handles per channel. Splitting by side kills the
	// per-Paula-clock "ci == 0 || ci == 3" branch -- each per-side scalar
	// accumulator now stays in a register through its sweep. Output is bit-
	// identical because pl and pr are separate accumulators with fixed
	// channel assignments (0+3 -> pl, 1+2 -> pr): the per-side sum only
	// depends on which channels are active, not on iteration order.
	struct paula_channel *hw_l[PAULA_NUM_CHANNELS];
	struct paula_channel *hw_r[PAULA_NUM_CHANNELS];
	uint32_t nl = 0;
	uint32_t nr = 0;
	for(int32_t ci = 0; ci < PAULA_NUM_CHANNELS; ++ci) {
		struct paula_channel *c = &p->ch[ci];
		if(!c->active || c->muted || c->sample == 0 || c->period_q == 0) {
			continue;
		}
		if(ci == 0 || ci == 3) {
			hw_l[nl++] = c;
		} else {
			hw_r[nr++] = c;
		}
	}

	for(int32_t i = 0; i < frames; ++i) {
		phase += dstep;
		uint32_t n = (uint32_t)(phase >> PAULA_PERIOD_SHIFT);
		phase &= (PAULA_PERIOD_ONE - 1);
		if(n == 0) {
			n = 1;
		}
		paula_v2df s = {0.0, 0.0};
		for(uint32_t k = 0; k < n; ++k) {
			double pl = 0.0;
			double pr = 0.0;
			for(uint32_t j = 0; j < nl; ++j) {
				pl += paula_ch_sample(hw_l[j]);
			}
			for(uint32_t j = 0; j < nr; ++j) {
				pr += paula_ch_sample(hw_r[j]);
			}
			// Passive resistive averaging summer: the per-side filter node
			// is (ch_a + ch_b) / 2, so it cannot exceed a single channel's
			// full scale and the hardware path never clips.
			paula_v2df x = (paula_v2df){pl, pr} * half;
			// Always-on RC pole (model-dependent corner baked into fa).
			fl = fl + (x - fl) * fa;
			x = fl;
			if(led) {
				paula_v2df y = lb0 * x + lz1;
				lz1 = lb1 * x - la1 * y + lz2;
				lz2 = lb2 * x - la2 * y;
				x = y;
			}
			// Downstream output buffer/amp: gain compensation then soft
			// saturation into the rails. Softclip is the only scalar point.
			x = x * amp;
			x = (paula_v2df){paula_softclip(x[0]), paula_softclip(x[1])};
			// Anti-alias before the rate drop: 4 cascaded Butterworth biquads
			// (TDF-II), L+R packed. Bandlimits below host Nyquist so the box-
			// average decimation below cannot fold ultrasonic images down.
			// RBJ low-pass identities baked in here: b1 = 2*b0 and b2 = b0,
			// so only ab0[] is stored. Do not reuse this loop for a non-LP
			// section -- it will silently produce wrong output.
			for(uint32_t st = 0; st < 4; ++st) {
				paula_v2df y = ab0[st] * x + az1[st];
				az1[st] = two * ab0[st] * x - aa1[st] * y + az2[st];
				az2[st] = ab0[st] * x - aa2[st] * y;
				x = y;
			}
			s = s + x;
		}
		double inv = 1.0 / (double)n;
		paula_v2df out = s * (paula_v2df){inv, inv};
		output[2 * i]     += (float)out[0];
		output[2 * i + 1] += (float)out[1];
	}

	p->fixed_lp = fl;
	p->led_z1 = lz1;
	p->led_z2 = lz2;
	for(uint32_t st = 0; st < 4; ++st) {
		p->aa_z1[st] = az1[st];
		p->aa_z2[st] = az2[st];
	}
	p->decim_phase = phase;

#ifdef PAULA_PROFILE
	struct timespec prof_t1;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &prof_t1);
	paula_profile_cpu_ns += (double)(prof_t1.tv_sec - prof_t0.tv_sec) * 1.0e9 + (double)(prof_t1.tv_nsec - prof_t0.tv_nsec);
	paula_profile_frames += (uint64_t)frames;
#endif
}

#ifdef PAULA_PROFILE
// [=]===^=[ paula_profile_report ]===============================================================[=]
// Print the accumulated mixer cost as a realtime factor. Call once at exit.
static void paula_profile_report(int32_t sample_rate) {
	if(paula_profile_frames == 0) {
		fprintf(stderr, "paula_mix_frames: never called (this player has its own mixer, not paula.h)\n");
		return;
	}
	double cpu_s = paula_profile_cpu_ns * 1.0e-9;
	double audio_s = (sample_rate > 0) ? (double)paula_profile_frames / (double)sample_rate : 0.0;
	double rt = (cpu_s > 0.0) ? audio_s / cpu_s : 0.0;
	double core_pct = (audio_s > 0.0) ? 100.0 * cpu_s / audio_s : 0.0;
	fprintf(stderr, "paula_mix_frames: %.3fs CPU for %.1fs audio -> %.1fx realtime (%.2f%% of one core)\n",
		cpu_s, audio_s, rt, core_pct);
}
#endif
