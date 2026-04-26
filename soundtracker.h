// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// 15-sample Soundtracker replayer.
//
// Targets the family of pre-ProTracker .mod variants (Ultimate Soundtracker
// 1987, D.O.C. SoundTracker II/III/IV 1988, MasterSoundtracker, etc.). All
// share the same on-disk layout but differ in the effect command table.
// ProTracker mods use 31 samples and a 4-byte signature at offset 1080
// (M.K., M!K!, FLT4, 4CHN, ...); files without that signature and a valid
// 15-sample structure are dispatched here.
//
// On-disk layout (15-sample, all multi-byte fields big-endian):
//   0..19      song title (20 bytes, zero-padded)
//   20..469    15 sample headers, 30 bytes each:
//                 0..21   name (22 bytes, zero-padded)
//                 22..23  length in words
//                 24      finetune (low nibble, signed; usually 0 in UST)
//                 25      volume (0..64)
//                 26..27  loop start in BYTES (UST quirk; ProTracker uses words)
//                 28..29  loop length in words (1 = "no loop" sentinel)
//   470        song length (positions used; 1..128)
//   471        restart byte (0x78 in UST = no real restart; loop from 0)
//   472..599   pattern table (128 bytes, only the first <song_length> entries used)
//   600..      pattern data, 1024 bytes per pattern, 64 rows of 4 voices x 4 bytes:
//                 byte 0 high nibble: zero in 15-sample (no high sample bits)
//                 byte 0 low nibble + byte 1: 12-bit period
//                 byte 2 high nibble: sample number (0 = "no trigger this row")
//                 byte 2 low nibble:  effect command
//                 byte 3:             effect argument
//   ...        sample data follows pattern data (signed 8-bit PCM)
//
// Variant selection:
//   The host picks the effect-command interpretation at compile time via the
//   SOUNDTRACKER_VARIANT macro. Default is SOUNDTRACKER_VARIANT_UST. Other
//   variant constants are reserved; they fall through to UST behavior until
//   their effect tables are added.
//
// Effect commands (UST):
//   0xy : no effect
//   1xy : arpeggio (cycle through row note, +x semitones, +y semitones in
//         the chromatic period table). This is THE reason ProTracker
//         replayers mangle Obarski mods: PT reads 1xy as slide-up (period
//         decrement) which forces the sample toward maximum frequency.
//   2xy : pitch slide. If high nibble x is non-zero, period -= x per
//         non-first tick (slide up). Else period += y per non-first tick
//         (slide down). One direction per row.
//
// Public API:
//   struct soundtracker_state *soundtracker_init(void *data, uint32_t len, int32_t sample_rate);
//   void soundtracker_free(struct soundtracker_state *s);
//   void soundtracker_get_audio(struct soundtracker_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

enum {
	SOUNDTRACKER_VARIANT_UST     = 0,  // Ultimate Soundtracker (Karsten Obarski, 1987)
	SOUNDTRACKER_VARIANT_DOCST2  = 1,  // reserved -- D.O.C. SoundTracker II
	SOUNDTRACKER_VARIANT_DOCST24 = 2,  // reserved -- D.O.C. SoundTracker IV
	SOUNDTRACKER_VARIANT_MASTER  = 3,  // reserved -- MasterSoundtracker
	SOUNDTRACKER_VARIANT_NOISE   = 4,  // reserved -- NoiseTracker
};

#ifndef SOUNDTRACKER_VARIANT
#define SOUNDTRACKER_VARIANT SOUNDTRACKER_VARIANT_UST
#endif

#define ST_VOICES         4
#define ST_NUM_SAMPLES    15
#define ST_SAMPLE_HDR_LEN 30
#define ST_TITLE_LEN      20
#define ST_SAMPLES_OFF    20
#define ST_SONG_LEN_OFF   470
#define ST_RESTART_OFF    471
#define ST_PAT_TABLE_OFF  472
#define ST_PAT_TABLE_LEN  128
#define ST_PAT_DATA_OFF   600
#define ST_PAT_BYTES      1024
#define ST_ROWS_PER_PAT   64
#define ST_BYTES_PER_ROW  16
#define ST_TICKS_PER_ROW  6
#define ST_TICK_HZ        50
#define ST_PT_MAGIC_OFF   1080
#define ST_PERIOD_MIN     113
#define ST_PERIOD_MAX     1023
#define ST_PERIOD_TBL_LEN 36

// ProTracker chromatic period table, 3 octaves x 12 semitones, no finetune.
// Index 0 = lowest pitch (C-1), index 35 = highest (B-3).
static uint16_t st_period_table[ST_PERIOD_TBL_LEN] = {
	856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
	428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
	214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
};

struct st_sample {
	uint32_t data_off;          // file offset of sample bytes
	uint32_t length_bytes;      // sample length in bytes
	uint32_t loop_start_bytes;
	uint32_t loop_length_bytes;
	uint8_t  volume;            // 0..64
	uint8_t  has_loop;
};

struct st_voice {
	uint8_t  cur_sample;        // 1..15, or 0 if none cached
	uint16_t row_period;        // unmodified period from this row
	uint16_t cur_period;        // period currently fed to paula
	int8_t   period_index;      // index in st_period_table for the row period (-1 if not in table)
	uint8_t  effect;
	uint8_t  effect_arg;
	uint8_t  volume;
};

struct soundtracker_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	struct st_sample samples[ST_NUM_SAMPLES + 1];   // index 1..15; entry 0 is the "no sample" sentinel
	uint8_t  song_length;
	uint8_t  pattern_table[ST_PAT_TABLE_LEN];
	uint32_t num_patterns;

	struct st_voice voices[ST_VOICES];

	uint32_t cur_position;
	uint8_t  cur_row;
	uint8_t  tick_in_row;
	uint8_t  ticks_per_row;         // mutable via Fxx effect; default ST_TICKS_PER_ROW
	uint8_t  end_reached;

	// Position-changing effects (Dxx pattern break, Bxx position jump):
	// queued at row trigger time and committed when the current row finishes.
	uint8_t  pat_break_pending;
	uint8_t  pat_break_row;
	uint8_t  pos_jump_pending;
	uint8_t  pos_jump_target;
};

// [=]===^=[ st_read_u16_be ]=====================================================================[=]
static uint16_t st_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ st_read_u32_be ]=====================================================================[=]
static uint32_t st_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ st_is_pt_magic ]=====================================================================[=]
// Recognises the four-byte signatures ProTracker and its descendants stamp at
// offset 1080. If any of these match, the file is NOT a 15-sample mod.
static int32_t st_is_pt_magic(uint8_t *p) {
	uint32_t v = st_read_u32_be(p);
	switch(v) {
		case 0x4d2e4b2eu:  // "M.K."
		case 0x4d214b21u:  // "M!K!"
		case 0x4d264b21u:  // "M&K!"
		case 0x464c5434u:  // "FLT4"
		case 0x464c5438u:  // "FLT8"
		case 0x3443484eu:  // "4CHN"
		case 0x3643484eu:  // "6CHN"
		case 0x3843484eu:  // "8CHN"
		case 0x4f435441u:  // "OCTA"
		case 0x43443831u:  // "CD81"
		case 0x4f4b5441u:  // "OKTA"
		case 0x4e2e542eu:  // "N.T."
		case 0x4e534d53u:  // "NSMS"
		case 0x4c415244u:  // "LARD"
		case 0x50415454u:  // "PATT"
		case 0x46413034u:  // "FA04"
		case 0x46413036u:  // "FA06"
		case 0x46413038u:  // "FA08"
			return 1;
		default:
			break;
	}
	return 0;
}

// [=]===^=[ st_identify ]========================================================================[=]
// Structural detection of a 15-sample mod. Verified by the file size matching
// EXACTLY: header (600 bytes) + num_patterns * 1024 + sum(sample bytes). That
// equality is rare to hit by accident, so it's the strongest single signal.
static int32_t st_identify(uint8_t *data, uint32_t len) {
	if(len < ST_PAT_DATA_OFF) {
		return 0;
	}
	if(len >= ST_PT_MAGIC_OFF + 4 && st_is_pt_magic(data + ST_PT_MAGIC_OFF)) {
		return 0;
	}

	uint32_t total_sample_bytes = 0;
	for(uint32_t i = 0; i < ST_NUM_SAMPLES; ++i) {
		uint8_t *h = data + ST_SAMPLES_OFF + i * ST_SAMPLE_HDR_LEN;
		uint32_t length_w = st_read_u16_be(h + 22);
		uint8_t  volume   = h[25];
		if(volume > 64) {
			return 0;
		}
		total_sample_bytes += length_w * 2u;
	}

	uint8_t song_length = data[ST_SONG_LEN_OFF];
	if(song_length == 0 || song_length > ST_PAT_TABLE_LEN) {
		return 0;
	}

	uint32_t max_pat = 0;
	for(uint32_t i = 0; i < song_length; ++i) {
		uint8_t pat = data[ST_PAT_TABLE_OFF + i];
		if(pat >= ST_PAT_TABLE_LEN) {
			return 0;
		}
		if(pat > max_pat) {
			max_pat = pat;
		}
	}
	uint32_t num_patterns_referenced = max_pat + 1;

	// File size = 600 + N*1024 + total_sample_bytes, where N is the number of
	// patterns physically present in storage. N must be >= the highest pattern
	// number referenced by the position table; some authoring tools leave
	// extra unused patterns in memory (e.g. song.mod ships 10 patterns but
	// only references 4). Real-world rippers also pad or leave trailing
	// garbage after the last sample (uade itself warns and plays through it),
	// so we allow up to 256 trailing bytes of slack -- generous enough to
	// absorb common padding / footer / metadata trailers without weakening
	// the byte-0-high-nibble structural check below.
	if(len < ST_PAT_DATA_OFF + total_sample_bytes) {
		return 0;
	}
	uint32_t pat_storage_bytes = len - ST_PAT_DATA_OFF - total_sample_bytes;
	if(pat_storage_bytes < num_patterns_referenced * ST_PAT_BYTES) {
		return 0;
	}
	uint32_t slack = pat_storage_bytes % ST_PAT_BYTES;
	if(slack > 256) {
		return 0;
	}
	uint32_t num_patterns_storage = pat_storage_bytes / ST_PAT_BYTES;
	if(num_patterns_storage > ST_PAT_TABLE_LEN) {
		return 0;
	}

	// 15-sample mods set byte 0 high nibble to zero on every cell. ProTracker
	// uses bits 4-5 of byte 0 for sample bits 4-5, so a non-zero high nibble
	// in cells reliably indicates a 31-sample mod even if the magic was stripped.
	uint32_t pat_total = num_patterns_referenced * ST_PAT_BYTES;
	for(uint32_t i = 0; i < pat_total; i += 4) {
		if((data[ST_PAT_DATA_OFF + i] & 0xf0) != 0) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ st_load ]============================================================================[=]
// Parses the 15-sample header into a runtime sample table and copies the
// position table into state. Pattern data and sample data are read directly
// from module_data via stored offsets, so no allocation is needed beyond state.
//
// The number of patterns physically stored can exceed the referenced count
// (some authoring tools leave unused patterns in memory). Sample data starts
// after ALL stored patterns, so the sample cursor is computed from total
// sample bytes counting backward from end-of-file rather than forward through
// the (referenced-only) pattern count.
static int32_t st_load(struct soundtracker_state *s) {
	uint8_t *d = s->module_data;
	uint32_t len = s->module_len;

	s->song_length = d[ST_SONG_LEN_OFF];
	memcpy(s->pattern_table, d + ST_PAT_TABLE_OFF, ST_PAT_TABLE_LEN);

	uint32_t max_pat = 0;
	for(uint32_t i = 0; i < s->song_length; ++i) {
		if(s->pattern_table[i] > max_pat) {
			max_pat = s->pattern_table[i];
		}
	}
	s->num_patterns = max_pat + 1u;

	uint32_t total_sample_bytes = 0;
	for(uint32_t i = 0; i < ST_NUM_SAMPLES; ++i) {
		uint8_t *h = d + ST_SAMPLES_OFF + i * ST_SAMPLE_HDR_LEN;
		uint32_t length_w = st_read_u16_be(h + 22);
		total_sample_bytes += length_w * 2u;
	}
	if(len < ST_PAT_DATA_OFF + total_sample_bytes) {
		return 0;
	}
	uint32_t pat_storage_bytes = len - ST_PAT_DATA_OFF - total_sample_bytes;
	uint32_t num_patterns_storage = pat_storage_bytes / ST_PAT_BYTES;
	uint32_t sample_data_off = ST_PAT_DATA_OFF + num_patterns_storage * ST_PAT_BYTES;
	uint32_t cursor = sample_data_off;

	if(s->num_patterns > num_patterns_storage) {
		// Position table references a pattern index that exceeds physical storage.
		return 0;
	}

	s->samples[0].volume = 0;
	for(uint32_t i = 0; i < ST_NUM_SAMPLES; ++i) {
		uint8_t *h = d + ST_SAMPLES_OFF + i * ST_SAMPLE_HDR_LEN;
		uint32_t length_w = st_read_u16_be(h + 22);
		uint8_t  volume   = h[25];
		uint32_t loop_start_b = st_read_u16_be(h + 26);    // UST: bytes
		uint32_t loop_length_w = st_read_u16_be(h + 28);   // words
		uint32_t length_b = length_w * 2u;

		struct st_sample *sm = &s->samples[i + 1];
		sm->data_off = (length_b > 0) ? cursor : 0;
		sm->length_bytes = length_b;
		sm->volume = volume;
		sm->loop_start_bytes = loop_start_b;
		sm->loop_length_bytes = loop_length_w * 2u;
		// loop_length_w == 1 (= 2 bytes) is the no-loop sentinel.
		sm->has_loop = (loop_length_w > 1u);
		if(sm->has_loop) {
			if(sm->loop_start_bytes + sm->loop_length_bytes > length_b) {
				// Clamp truncated loops so paula doesn't read past the sample.
				if(sm->loop_start_bytes >= length_b) {
					sm->has_loop = 0;
				} else {
					sm->loop_length_bytes = length_b - sm->loop_start_bytes;
				}
			}
		}

		cursor += length_b;
		if(cursor > len + 16) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ st_resolve_period_index ]============================================================[=]
// Locate `period` in the 36-entry chromatic table. Periods stored in the
// pattern can be exact table values (typical) or sample-finetuned variants;
// for UST with finetune always 0 the table match is exact. Returns -1 on
// miss so arpeggio can no-op rather than walk off random data.
static int32_t st_resolve_period_index(uint16_t period) {
	for(uint32_t i = 0; i < ST_PERIOD_TBL_LEN; ++i) {
		if(st_period_table[i] == period) {
			return (int32_t)i;
		}
	}
	return -1;
}

// [=]===^=[ st_play_cached_sample ]==============================================================[=]
// Retrigger this voice's currently-cached sample from offset 0. Used both for
// rows that name a fresh sample number and for the UST "period only -> retrigger
// cached sample" path.
static void st_play_cached_sample(struct soundtracker_state *s, int32_t voice, struct st_voice *v) {
	if(v->cur_sample == 0) {
		return;
	}
	struct st_sample *sm = &s->samples[v->cur_sample];
	if(sm->length_bytes == 0) {
		return;
	}
	int8_t *sdata = (int8_t *)(s->module_data + sm->data_off);
	paula_play_sample(&s->paula, voice, sdata, sm->length_bytes);
	paula_set_volume(&s->paula, voice, v->volume);
	if(sm->has_loop) {
		paula_set_loop(&s->paula, voice, sm->loop_start_bytes, sm->loop_length_bytes);
	} else {
		paula_set_loop(&s->paula, voice, 0, 0);
	}
}

// [=]===^=[ st_trigger_voice ]===================================================================[=]
// Apply a row's per-voice cell.
//
// UST retrigger semantics (different from ProTracker): a row that supplies a
// period RETRIGGERS the voice -- using the freshly named sample if one is
// given, otherwise the cached sample on this voice. Pre-PT trackers expected
// composers to omit the sample number when reusing the same instrument, so
// "period-only" rows are how repeated bass/drum hits are encoded. PT changed
// this to "period-only = pitch change without retrigger"; treating a UST mod
// with PT semantics drops every "period-only" hit, which is the audible bug
// rallyemaster.mod's voice-0 bass riff exposes.
//
// A row with no period and a fresh sample number does NOT retrigger; it just
// updates the cached sample (matches PT). A row with no period and no sample
// is a pure effect/continuation row.
static void st_trigger_voice(struct soundtracker_state *s, int32_t voice, struct st_voice *v, uint8_t *cell) {
	uint16_t period = (uint16_t)((((uint16_t)cell[0] & 0x0f) << 8) | (uint16_t)cell[1]);
	uint8_t  sample_no = (uint8_t)(cell[2] >> 4);
	uint8_t  effect    = (uint8_t)(cell[2] & 0x0f);
	uint8_t  effect_arg = cell[3];

	if(sample_no != 0 && sample_no <= ST_NUM_SAMPLES) {
		struct st_sample *sm = &s->samples[sample_no];
		v->cur_sample = sample_no;
		v->volume = sm->volume;
		paula_set_volume(&s->paula, voice, v->volume);
	}

	v->effect = effect;
	v->effect_arg = effect_arg;

	if(period != 0) {
		v->row_period = period;
		v->cur_period = period;
		v->period_index = (int8_t)st_resolve_period_index(period);
		paula_set_period(&s->paula, voice, period);
		st_play_cached_sample(s, voice, v);
		// Effect 9xx (sample offset) takes effect at trigger time: the
		// freshly-started sample skips forward by arg * 256 bytes.
		if(effect == 0x9 && effect_arg != 0) {
			paula_set_pos(&s->paula, voice, (uint32_t)effect_arg * 256u);
		}
	}
}

// [=]===^=[ st_apply_arpeggio ]==================================================================[=]
// UST effect 1: cycle period through (row, row+x_semi, row+y_semi). Applied
// once per tick; the cycle repeats every 3 ticks. If the row period isn't in
// the chromatic table, leave the period alone (silently skip) -- transposing
// a free-period note doesn't have a defined meaning.
static void st_apply_arpeggio(struct soundtracker_state *s, int32_t voice, struct st_voice *v, uint32_t tick) {
	if(v->period_index < 0) {
		return;
	}
	uint8_t x = (uint8_t)(v->effect_arg >> 4);
	uint8_t y = (uint8_t)(v->effect_arg & 0x0f);
	uint8_t shift = 0;
	switch(tick % 3) {
		case 0: shift = 0; break;
		case 1: shift = x; break;
		case 2: shift = y; break;
	}
	int32_t idx = (int32_t)v->period_index + (int32_t)shift;
	if(idx >= ST_PERIOD_TBL_LEN) {
		idx = ST_PERIOD_TBL_LEN - 1;
	}
	v->cur_period = st_period_table[idx];
	paula_set_period(&s->paula, voice, v->cur_period);
}

// [=]===^=[ st_apply_pitchbend ]=================================================================[=]
// UST effect 2: high nibble = up amount per tick (period decrement); low
// nibble = down amount per tick (period increment). Applied on non-first
// ticks so the row's authored period is heard for one tick before the slide.
static void st_apply_pitchbend(struct soundtracker_state *s, int32_t voice, struct st_voice *v) {
	uint8_t up   = (uint8_t)(v->effect_arg >> 4);
	uint8_t down = (uint8_t)(v->effect_arg & 0x0f);
	int32_t p = (int32_t)v->cur_period;
	if(up != 0) {
		p -= (int32_t)up;
	} else if(down != 0) {
		p += (int32_t)down;
	} else {
		return;
	}
	if(p < ST_PERIOD_MIN) {
		p = ST_PERIOD_MIN;
	}
	if(p > ST_PERIOD_MAX) {
		p = ST_PERIOD_MAX;
	}
	v->cur_period = (uint16_t)p;
	paula_set_period(&s->paula, voice, v->cur_period);
}

// [=]===^=[ st_per_tick_effect ]=================================================================[=]
// Effect dispatch. Runs on every tick including tick 0; per-effect handlers
// decide whether tick 0 is meaningful (arpeggio: yes, resets to base period
// via shift=0; pitch slide: no, the row's authored period must be heard
// before the slide starts).
//
// Variant-sensitive effects (0/1/2 differ between UST and post-UST variants)
// are guarded by SOUNDTRACKER_VARIANT. Effects that ProTracker/NoiseTracker
// numbered 0xC/0xD/0xF (set volume, pattern break, set speed) use opcodes
// UST never assigns, so they are dispatched unconditionally -- adding them
// is harmless for pure UST mods and lets post-UST 15-sample mods (D.O.C.
// SoundTracker, MasterSoundtracker, NoiseTracker, etc.) play correctly.
static void st_per_tick_effect(struct soundtracker_state *s, int32_t voice, struct st_voice *v, uint32_t tick) {
#if SOUNDTRACKER_VARIANT == SOUNDTRACKER_VARIANT_UST
	switch(v->effect) {
		case 1:
			st_apply_arpeggio(s, voice, v, tick);
			break;
		case 2:
			if(tick != 0) {
				st_apply_pitchbend(s, voice, v);
			}
			break;
		default:
			break;
	}
#endif

	// Post-UST extensions (universal in PT/NT/D.O.C./Master variants).
	switch(v->effect) {
		case 0xa: {
			// Axy: volume slide. On non-first ticks: x adds to volume,
			// y subtracts (x takes precedence if both non-zero, matching PT).
			if(tick != 0) {
				uint8_t up = (uint8_t)(v->effect_arg >> 4);
				uint8_t down = (uint8_t)(v->effect_arg & 0x0f);
				int32_t newv = (int32_t)v->volume;
				if(up != 0) {
					newv += up;
				} else if(down != 0) {
					newv -= down;
				} else {
					break;
				}
				if(newv < 0) {
					newv = 0;
				}
				if(newv > 64) {
					newv = 64;
				}
				v->volume = (uint8_t)newv;
				paula_set_volume(&s->paula, voice, v->volume);
			}
			break;
		}
		case 0xb: {
			// Bxx: position jump to position xx. Queued like Dxx so the
			// current row plays out fully before the jump takes effect.
			if(tick == 0) {
				s->pos_jump_pending = 1;
				s->pos_jump_target = v->effect_arg;
			}
			break;
		}
		case 0xc: {
			// Cxx: set volume to xx (clamped to 64).
			if(tick == 0) {
				uint16_t vol = v->effect_arg;
				if(vol > 64) {
					vol = 64;
				}
				v->volume = (uint8_t)vol;
				paula_set_volume(&s->paula, voice, vol);
			}
			break;
		}
		case 0xd: {
			// Dxx: pattern break -- finish current row, then jump to the
			// next position starting at row (xx decoded as BCD: high*10 + low).
			if(tick == 0) {
				uint8_t hi = (uint8_t)(v->effect_arg >> 4);
				uint8_t lo = (uint8_t)(v->effect_arg & 0x0f);
				uint8_t target = (uint8_t)(hi * 10u + lo);
				if(target >= ST_ROWS_PER_PAT) {
					target = 0;
				}
				s->pat_break_pending = 1;
				s->pat_break_row = target;
			}
			break;
		}
		case 0xf: {
			// Fxx: set ticks-per-row. PT also overloads this for BPM when
			// arg >= 0x20, but pre-PT 15-sample variants only use the
			// ticks-per-row form, so values that high are silently ignored.
			if(tick == 0) {
				if(v->effect_arg > 0 && v->effect_arg < 0x20) {
					s->ticks_per_row = v->effect_arg;
				}
			}
			break;
		}
		default:
			break;
	}
}

// [=]===^=[ st_advance_row ]=====================================================================[=]
// Position-changing effects (Bxx, Dxx) take precedence over the natural row
// advance. If both are queued in the same row -- Bxx on one voice, Dxx on
// another -- Bxx provides the target position and Dxx provides the target
// row, matching ProTracker's documented combination behavior.
static void st_advance_row(struct soundtracker_state *s) {
	if(s->pos_jump_pending || s->pat_break_pending) {
		uint8_t target_row = s->pat_break_pending ? s->pat_break_row : 0;
		uint32_t target_pos;
		if(s->pos_jump_pending) {
			target_pos = s->pos_jump_target;
		} else {
			target_pos = s->cur_position + 1;
		}
		s->pos_jump_pending = 0;
		s->pat_break_pending = 0;
		if(target_pos >= s->song_length) {
			target_pos = 0;
			s->end_reached = 1;
		}
		s->cur_position = target_pos;
		s->cur_row = target_row;
		return;
	}
	s->cur_row++;
	if(s->cur_row >= ST_ROWS_PER_PAT) {
		s->cur_row = 0;
		s->cur_position++;
		if(s->cur_position >= s->song_length) {
			s->cur_position = 0;
			s->end_reached = 1;
		}
	}
}

// [=]===^=[ st_tick ]============================================================================[=]
// One playback tick. Tick 0 triggers fresh notes; effects then run on every
// tick (including tick 0, which gives arpeggio its "reset to base period"
// boundary). After the last tick, advance to the next row.
static void st_tick(struct soundtracker_state *s) {
	if(s->tick_in_row == 0) {
		uint8_t pat = s->pattern_table[s->cur_position];
		uint32_t row_off = ST_PAT_DATA_OFF + (uint32_t)pat * ST_PAT_BYTES + (uint32_t)s->cur_row * ST_BYTES_PER_ROW;
		for(int32_t voice = 0; voice < ST_VOICES; ++voice) {
			uint8_t *cell = s->module_data + row_off + voice * 4;
			st_trigger_voice(s, voice, &s->voices[voice], cell);
		}
	}

	for(int32_t voice = 0; voice < ST_VOICES; ++voice) {
		st_per_tick_effect(s, voice, &s->voices[voice], (uint32_t)s->tick_in_row);
	}

	s->tick_in_row++;
	if(s->tick_in_row >= s->ticks_per_row) {
		s->tick_in_row = 0;
		st_advance_row(s);
	}
}

// [=]===^=[ soundtracker_init ]==================================================================[=]
static struct soundtracker_state *soundtracker_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < ST_PAT_DATA_OFF || sample_rate < 8000) {
		return 0;
	}
	if(!st_identify((uint8_t *)data, len)) {
		return 0;
	}
	struct soundtracker_state *s = (struct soundtracker_state *)calloc(1, sizeof(struct soundtracker_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;
	if(!st_load(s)) {
		free(s);
		return 0;
	}
	paula_init(&s->paula, sample_rate, ST_TICK_HZ);
	for(int32_t i = 0; i < ST_VOICES; ++i) {
		s->voices[i].period_index = -1;
	}
	s->cur_position = 0;
	s->cur_row = 0;
	s->tick_in_row = 0;
	s->ticks_per_row = ST_TICKS_PER_ROW;
	s->pat_break_pending = 0;
	s->pat_break_row = 0;
	s->pos_jump_pending = 0;
	s->pos_jump_target = 0;
	return s;
}

// [=]===^=[ soundtracker_free ]==================================================================[=]
static void soundtracker_free(struct soundtracker_state *s) {
	if(s) {
		free(s);
	}
}

// [=]===^=[ soundtracker_get_audio ]=============================================================[=]
static void soundtracker_get_audio(struct soundtracker_state *s, int16_t *output, int32_t frames) {
	while(frames > 0) {
		int32_t remain = s->paula.samples_per_tick - s->paula.tick_offset;
		if(remain > frames) {
			remain = frames;
		}
		paula_mix_frames(&s->paula, output, remain);
		output += remain * 2;
		s->paula.tick_offset += remain;
		frames -= remain;
		if(s->paula.tick_offset >= s->paula.samples_per_tick) {
			s->paula.tick_offset = 0;
			st_tick(s);
		}
	}
}

// [=]===^=[ soundtracker_api_init ]==============================================================[=]
static void *soundtracker_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return soundtracker_init(data, len, sample_rate);
}

// [=]===^=[ soundtracker_api_free ]==============================================================[=]
static void soundtracker_api_free(void *state) {
	soundtracker_free((struct soundtracker_state *)state);
}

// [=]===^=[ soundtracker_api_get_audio ]=========================================================[=]
static void soundtracker_api_get_audio(void *state, int16_t *output, int32_t frames) {
	soundtracker_get_audio((struct soundtracker_state *)state, output, frames);
}

static const char *soundtracker_extensions[] = { "mod", 0 };

static struct player_api soundtracker_api = {
	"Soundtracker (15-sample)",
	soundtracker_extensions,
	soundtracker_api_init,
	soundtracker_api_free,
	soundtracker_api_get_audio,
	0,
};
