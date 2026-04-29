// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Minimal Amiga Paula emulator for custom replayers.
// Reads 8-bit signed samples; writes interleaved float stereo frames.
// Hard-panned LRRL (Amiga native). All functions static.
//
// Output is ACCUMULATED into the caller's float buffer, nominally [-1.0, 1.0].
// One channel at full volume (vol=64), hard pan, peak sample produces |1.0|.
// No saturation is performed; the host is responsible for any final clipping.

#pragma once

#include <stdint.h>
#include <string.h>

// PAULA_NUM_CHANNELS sets the number of virtual channels. Real Amiga has 4,
// but several formats fake more via CPU mixing: 7-voice Hippel, 8-channel
// OctaMed/Oktalyzer, up to 32 channels in DBM3. We expose all as virtual
// channels and let the player drive them; the output mixer sums them all.
#define PAULA_NUM_CHANNELS 32
#define PAULA_PAL_CLOCK    3546895
#define PAULA_FP_SHIFT     14
#define PAULA_FP_ONE       (1u << PAULA_FP_SHIFT)
// Real Amiga Paula register minimum; matches NostalgicPlayer Channel.SetAmigaPeriod.
// Software-mixer-only paths that need higher rates should use paula_set_freq_hz.
#define PAULA_MIN_PERIOD   113

struct paula_channel {
	int8_t *sample;
	uint32_t length_fp;        // length << FP_SHIFT
	uint32_t loop_start_fp;
	uint32_t loop_length_fp;   // 0 => no loop (one-shot)
	uint32_t pos_fp;
	uint32_t step_fp;
	int8_t *pending_sample;    // deferred switch on next wrap (Paula AUDxLC trick)
	uint32_t pending_length_fp;
	uint32_t pending_pos_fp;
	uint16_t period;
	uint16_t volume;           // 0..64 Amiga scale
	uint8_t active;
	uint8_t muted;
	uint8_t pan;               // 0..127, 0=left 127=right
	uint8_t has_pending;
	uint8_t backwards;         // 1 -> step DOWN through sample (DBP E3, etc.)
};

struct paula {
	struct paula_channel ch[PAULA_NUM_CHANNELS];
	int32_t sample_rate;
	int32_t samples_per_tick;
	int32_t tick_offset;
	// Amiga LED low-pass filter (~3.3-5 kHz): a 1-pole IIR applied to the
	// final stereo output. Off unless paula_set_lp_filter(p, 1) is called.
	// Coefficient is computed in paula_init from sample_rate.
	int32_t lp_filter_on;
	float lp_alpha;                // smoothing coefficient
	float lp_state_l;              // last-output sample (left)
	float lp_state_r;
};

// [=]===^=[ paula_init ]=========================================================================[=]
static void paula_init(struct paula *p, int32_t sample_rate, int32_t tick_rate_hz) {
	memset(p, 0, sizeof(*p));
	p->sample_rate = sample_rate;
	p->samples_per_tick = sample_rate / tick_rate_hz;
	// LRRL repeating for any extra virtual channels (4..7 etc).
	for(int32_t i = 0; i < PAULA_NUM_CHANNELS; ++i) {
		p->ch[i].pan = ((i & 2) != 0) ^ ((i & 1) != 0) ? 127 : 0;
	}
	// Pre-compute the LED-filter coefficient for ~4 kHz cutoff:
	// alpha = dt / (RC + dt), where dt = 1 / sample_rate, RC = 1 / (2 PI fc).
	// At sr=48000, fc=4000: alpha ~= 0.343.
	{
		double fc = 4000.0;
		double dt = 1.0 / (double)sample_rate;
		double rc = 1.0 / (2.0 * 3.14159265358979 * fc);
		double a = dt / (rc + dt);
		if(a < 0.0) {
			a = 0.0;
		}
		if(a > 1.0) {
			a = 1.0;
		}
		p->lp_alpha = (float)a;
	}
}

// [=]===^=[ paula_set_lp_filter ]================================================================[=]
// Enable or disable the Amiga LED filter. The filter is a 1-pole low-pass
// (~4 kHz cutoff) applied to the final stereo output. State persists across
// toggle calls so brief flips don't reset the smoothing.
static void paula_set_lp_filter(struct paula *p, int32_t on) {
	p->lp_filter_on = on ? 1 : 0;
}

// [=]===^=[ paula_set_period ]===================================================================[=]
static void paula_set_period(struct paula *p, int32_t idx, uint16_t period) {
	struct paula_channel *c = &p->ch[idx];
	if(period < PAULA_MIN_PERIOD) {
		period = PAULA_MIN_PERIOD;
	}
	c->period = period;
	uint64_t freq_fp = ((uint64_t)PAULA_PAL_CLOCK << PAULA_FP_SHIFT) / period;
	c->step_fp = (uint32_t)(freq_fp / (uint32_t)p->sample_rate);
}

// [=]===^=[ paula_set_freq_hz ]==================================================================[=]
// Set channel playback rate directly in Hz, bypassing the Amiga-Paula period
// model. Useful for replayers (DigiBoosterPro, FaceTheMusic, etc.) that drive
// the mixer at frequencies above Paula's hardware MIN_PERIOD limit -- a
// software mixer has no such cap. paula_set_period clamps at MIN_PERIOD,
// which silently drops sample-rate to ~28.6 kHz; this routine does not.
static void paula_set_freq_hz(struct paula *p, int32_t idx, uint32_t freq_hz) {
	struct paula_channel *c = &p->ch[idx];
	if(freq_hz == 0 || p->sample_rate <= 0) {
		c->step_fp = 0;
		return;
	}
	c->period = 0;
	c->step_fp = (uint32_t)(((uint64_t)freq_hz << PAULA_FP_SHIFT) / (uint32_t)p->sample_rate);
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
	volume >>= 2;
	if(volume > 64) {
		volume = 64;
	}
	p->ch[idx].volume = volume;
}

// [=]===^=[ paula_play_sample ]==================================================================[=]
static void paula_play_sample(struct paula *p, int32_t idx, int8_t *sample, uint32_t length) {
	struct paula_channel *c = &p->ch[idx];
	c->sample = sample;
	c->length_fp = length << PAULA_FP_SHIFT;
	// Backwards-mode initial position is one fractional step below the end so
	// the first read fetches the last sample. Forward-mode starts at 0.
	c->pos_fp = c->backwards
	    ? ((length > 0) ? (uint32_t)((length << PAULA_FP_SHIFT) - 1) : 0)
	    : 0;
	c->loop_start_fp = 0;
	c->loop_length_fp = 0;
	c->has_pending = 0;
	c->pending_sample = 0;
	c->active = (sample != 0) && (length > 0);
}

// [=]===^=[ paula_set_backwards ]================================================================[=]
// Set or clear the backwards-playback flag for a channel. Takes effect on the
// next paula_play_sample call (which seeds pos_fp at the high end of the
// sample) and changes the per-step direction in paula_mix_frames.
static void paula_set_backwards(struct paula *p, int32_t idx, int32_t on) {
	p->ch[idx].backwards = on ? 1 : 0;
}

// [=]===^=[ paula_set_pos ]======================================================================[=]
// Move the channel's read position to `byte_offset` within the current sample.
// Used by tracker effects like ProTracker 9xx (sample offset) that retrigger
// playback at a non-zero starting offset. Clamps to [0, length-1] so a bad
// offset can't drive the mixer past end-of-sample.
static void paula_set_pos(struct paula *p, int32_t idx, uint32_t byte_offset) {
	struct paula_channel *c = &p->ch[idx];
	uint32_t len_bytes = c->length_fp >> PAULA_FP_SHIFT;
	if(len_bytes == 0) {
		c->pos_fp = 0;
		return;
	}
	if(byte_offset >= len_bytes) {
		byte_offset = len_bytes - 1;
	}
	c->pos_fp = byte_offset << PAULA_FP_SHIFT;
}

// [=]===^=[ paula_queue_sample ]=================================================================[=]
// If the channel is currently active (playing), the new sample takes effect
// when the current one reaches length (Amiga "write AUDxLC/AUDxLEN mid-DMA").
// If the channel is inactive (DMA off), the new sample starts immediately
// (matches NostalgicPlayer's mixer semantics: SetSample on an inactive
// channel triggers playback right away).
//
// Plays from sample[start_offset] for `length` samples, then wraps using the
// channel's current loop_start / loop_length (set via paula_set_loop).
static void paula_queue_sample(struct paula *p, int32_t idx, int8_t *sample, uint32_t start_offset, uint32_t length) {
	struct paula_channel *c = &p->ch[idx];
	if(!c->active && sample != 0 && length > 0) {
		c->sample = sample;
		c->pos_fp = start_offset << PAULA_FP_SHIFT;
		c->length_fp = (start_offset + length) << PAULA_FP_SHIFT;
		c->has_pending = 0;
		c->pending_sample = 0;
		c->active = 1;
		return;
	}
	c->pending_sample = sample;
	c->pending_pos_fp = start_offset << PAULA_FP_SHIFT;
	c->pending_length_fp = (start_offset + length) << PAULA_FP_SHIFT;
	c->has_pending = (sample != 0) && (length > 0);
}

// [=]===^=[ paula_set_loop ]=====================================================================[=]
static void paula_set_loop(struct paula *p, int32_t idx, uint32_t start, uint32_t length) {
	struct paula_channel *c = &p->ch[idx];
	c->loop_start_fp = start << PAULA_FP_SHIFT;
	c->loop_length_fp = length << PAULA_FP_SHIFT;
}

// [=]===^=[ paula_mute ]=========================================================================[=]
static void paula_mute(struct paula *p, int32_t idx) {
	p->ch[idx].active = 0;
}

// [=]===^=[ paula_mix_frames ]===================================================================[=]
// Accumulates `frames` float stereo frames into `output`. Caller must pre-clear.
// One channel at full volume (vol=64), hard pan, peak sample (|s|=128) -> |1.0|.
// No clipping is applied; the host saturates at the final output stage.
static void paula_mix_frames(struct paula *p, float *output, int32_t frames) {
	// Combined scaling: sample/128 (int8 -> [-1,1]) * volume/64 * pan/127.
	const float vol_scale = 1.0f / (128.0f * 64.0f * 127.0f);
	for(int32_t ci = 0; ci < PAULA_NUM_CHANNELS; ++ci) {
		struct paula_channel *c = &p->ch[ci];
		if(!c->active || c->muted || c->sample == 0 || c->step_fp == 0) {
			continue;
		}
		float lvol = (float)c->volume * (float)(127 - c->pan) * vol_scale;
		float rvol = (float)c->volume * (float)c->pan         * vol_scale;
		uint32_t step = c->step_fp;
		uint32_t length = c->length_fp;
		uint32_t loop_start = c->loop_start_fp;
		uint32_t loop_length = c->loop_length_fp;
		int8_t *sdat = c->sample;
		float *out = output;
		if(!c->backwards) {
			uint32_t pos = c->pos_fp;
			for(int32_t i = 0; i < frames; ++i) {
				if(pos >= length) {
					if(c->has_pending) {
						sdat = c->pending_sample;
						pos = c->pending_pos_fp;
						length = c->pending_length_fp;
						c->sample = sdat;
						c->length_fp = length;
						c->has_pending = 0;
						c->pending_sample = 0;
					} else if(loop_length > 0) {
						uint32_t over = pos - length;
						pos = loop_start + (over % loop_length);
						length = loop_start + loop_length;
						c->length_fp = length;
					} else {
						c->active = 0;
						break;
					}
				}
				float s = (float)sdat[pos >> PAULA_FP_SHIFT];
				out[0] += s * lvol;
				out[1] += s * rvol;
				out += 2;
				pos += step;
			}
			c->pos_fp = pos;
		} else {
			// Backwards path: pos counted in int64_t so underflow is plain
			// arithmetic. Wrap on pos < loop_start (or < 0 if no loop).
			int64_t pos = (int64_t)c->pos_fp;
			int64_t lo = (int64_t)loop_start;
			int64_t llen = (int64_t)loop_length;
			for(int32_t i = 0; i < frames; ++i) {
				if((llen > 0 && pos < lo) || (llen == 0 && pos < 0)) {
					if(llen > 0) {
						int64_t hi = lo + llen;
						int64_t under = lo - pos;
						pos = hi - 1 - (under % llen);
					} else {
						c->active = 0;
						break;
					}
				}
				float s = (float)sdat[(uint32_t)pos >> PAULA_FP_SHIFT];
				out[0] += s * lvol;
				out[1] += s * rvol;
				out += 2;
				pos -= (int64_t)step;
			}
			c->pos_fp = (uint32_t)((pos < 0) ? 0 : pos);
		}
	}

	// Amiga LED filter: 1-pole IIR over the final stereo output.
	// y[n] = y[n-1] + alpha * (x[n] - y[n-1]).
	if(p->lp_filter_on) {
		float a = p->lp_alpha;
		float yl = p->lp_state_l;
		float yr = p->lp_state_r;
		float *out = output;
		for(int32_t i = 0; i < frames; ++i) {
			yl += (out[0] - yl) * a;
			yr += (out[1] - yr) * a;
			out[0] = yl;
			out[1] = yr;
			out += 2;
		}
		p->lp_state_l = yl;
		p->lp_state_r = yr;
	}
}
