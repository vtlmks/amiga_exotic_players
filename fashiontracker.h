// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Fashion Tracker replayer.
//
// Fashion Tracker was an internal m68k tracker by the French demogroup
// "Fashion" (1988); roughly 14 modules ever produced. Each module file is
// the assembled m68k replayer with the song data and samples concatenated
// after it. There is no NostalgicPlayer C# port to reference; this port is
// derived from the EaglePlayer source in side_project/Fashion Tracker.asm
// (Wanted Team's adaption, 2004).
//
// Module layout:
//   - Bytes 0..25: a fixed m68k prologue (DMA-wait + Play start) used as
//     the identification signature.
//   - First ~1000 bytes contain assembled player code with embedded
//     immediate-address pointers to the song's tables. The loader scans
//     for known opcodes (2379, 23D1, C0FC0400, 0C790400) at known
//     positions to recover those pointers.
//   - origin    : value extracted at -4 of the first 2379. Subtracted
//                 from m68k absolute addresses to convert them to
//                 file-relative offsets.
//   - sample_lens   : lbL000412 -- u32[num_samples], length in WORDS
//   - sample_addrs  : lbL000432 -- u32[num_samples], m68k absolute ptrs
//   - sample_reps   : lbL000452 -- u32[num_samples], byte 3 of each
//                                  is the loop flag (1 = full loop)
//   - sample_nv     : lbW000476 -- 16 entries x 4 bytes
//                                  (sample_no_w, volume_w)
//   - pattern_data  : lbL000FF0 -- 1024 bytes per pattern, 64 rows,
//                                  16 bytes per row, 4 bytes per voice
//                                  (period u16 BE, byte 2 = sample-slot
//                                  high nibble + effect low nibble,
//                                  byte 3 = effect arg)
//   - position_table: lbL002480 -- u8[song_length], pattern numbers
//
// Playback:
//   - 4 voices, 6 ticks per row, ~50 Hz tick rate
//   - Effect 1 (slide-via-period-table): tick 1/4 -> shift forward by
//     (arg >> 4) entries in lbW0003C8 (higher pitch); tick 2/5 -> shift
//     forward by (arg & 0x0f) entries; tick 3 -> restore row period.
//   - Sample loops: byte 3 of sample_reps[i] == 1 means the sample
//     loops over its full body; otherwise the sample is one-shot.
//
// Public API:
//   struct fashiontracker_state *fashiontracker_init(void *data, uint32_t len, int32_t sample_rate);
//   void fashiontracker_free(struct fashiontracker_state *s);
//   void fashiontracker_get_audio(struct fashiontracker_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define FT_TICK_HZ        50
#define FT_VOICES         4
#define FT_ROWS_PER_PAT   64
#define FT_BYTES_PER_PAT  1024
#define FT_BYTES_PER_ROW  16
#define FT_TICKS_PER_ROW  6
#define FT_NUM_SLOTS      16
#define FT_PERIOD_TBL_LEN 36

// 36-entry period table from the Fashion Tracker player (lbW0003C8).
// Sorted descending: index 0 = lowest pitch, index 35 = highest pitch.
static uint16_t ft_period_table[FT_PERIOD_TBL_LEN] = {
	0x358, 0x328, 0x2fa, 0x2d0, 0x2a6, 0x280, 0x25c, 0x23a,
	0x21a, 0x1fc, 0x1e0, 0x1c5, 0x1ac, 0x194, 0x17d, 0x168,
	0x153, 0x140, 0x12e, 0x11d, 0x10d, 0x0fe, 0x0f0, 0x0e2,
	0x0d6, 0x0ca, 0x0be, 0x0b4, 0x0aa, 0x0a0, 0x097, 0x08f,
	0x087, 0x07f, 0x078, 0x071,
};

struct ft_voice {
	uint8_t cached_slot4;          // last sample-slot << 2 cached for "no-slot" rows
	int8_t  *sample_data;          // current sample's data (file-relative, owned by module buffer)
	uint32_t sample_length_bytes;  // current sample's length in bytes
	uint8_t  sample_loops;         // 1 if current sample loops fully
	uint16_t row_period;           // unmodified period from this row
	uint16_t cur_period;            // period actually fed to paula
	uint8_t  effect;
	uint8_t  effect_arg;
	uint8_t  volume;                // 0..64
};

struct fashiontracker_state {
	struct paula paula;

	uint8_t *module_data;           // caller-owned
	uint32_t module_len;

	uint32_t origin;
	uint32_t sample_lens_off;       // offsets into module_data
	uint32_t sample_addrs_off;
	uint32_t sample_reps_off;
	uint32_t sample_nv_off;
	uint32_t pattern_data_off;
	uint32_t position_table_off;

	uint32_t num_samples;
	uint32_t num_patterns;
	uint32_t song_length;

	struct ft_voice voices[FT_VOICES];

	uint32_t cur_position;          // index into position table
	uint16_t cur_row_offset_bytes;  // 0..FT_BYTES_PER_PAT-1 in steps of FT_BYTES_PER_ROW
	uint8_t  tick_in_row;           // 0..FT_TICKS_PER_ROW-1
	uint8_t  end_reached;
};

// [=]===^=[ ft_read_u16_be ]=====================================================================[=]
static uint16_t ft_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ ft_read_u32_be ]=====================================================================[=]
static uint32_t ft_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ ft_identify ]========================================================================[=]
// 5 fixed-position signature checks at module start; mirrors the Check2
// callback in Fashion Tracker.asm.
static int32_t ft_identify(uint8_t *data, uint32_t len) {
	if(len < 32) {
		return 0;
	}
	if(ft_read_u32_be(data + 0) != 0x13fc0040u) {
		return 0;
	}
	if(ft_read_u32_be(data + 8) != 0x4e710439u) {
		return 0;
	}
	if(ft_read_u16_be(data + 12) != 1) {
		return 0;
	}
	if(ft_read_u32_be(data + 18) != 0x66f44e75u) {
		return 0;
	}
	if(ft_read_u32_be(data + 22) != 0x48e7fffeu) {
		return 0;
	}
	return 1;
}

// [=]===^=[ ft_find_marker ]=====================================================================[=]
// Locate the first occurrence of a 2- or 4-byte marker at an even offset
// within the first `max_off` bytes of the file. Returns offset or -1.
static int32_t ft_find_marker(uint8_t *data, uint32_t len, uint8_t *marker, uint32_t mlen, uint32_t max_off) {
	if(max_off > len) {
		max_off = len;
	}
	if(mlen > max_off) {
		return -1;
	}
	for(uint32_t i = 0; i + mlen <= max_off; i += 2) {
		if(memcmp(data + i, marker, mlen) == 0) {
			return (int32_t)i;
		}
	}
	return -1;
}

// [=]===^=[ ft_resolve_ptr ]=====================================================================[=]
// Convert an m68k absolute address (as embedded in the module) into a
// file-relative offset by subtracting the module's recovered origin.
// Returns 0xFFFFFFFF on out-of-bounds.
static uint32_t ft_resolve_ptr(struct fashiontracker_state *s, uint32_t abs_addr) {
	uint32_t off = abs_addr - s->origin;
	if(off >= s->module_len) {
		return 0xffffffffu;
	}
	return off;
}

// [=]===^=[ ft_load ]============================================================================[=]
// Mirrors the Fashion Tracker.asm InitPlayer: scan for marker opcodes in
// the first 1000 bytes of the module, recover origin + table pointers.
// Returns 1 on success, 0 on corrupt module.
static int32_t ft_load(struct fashiontracker_state *s) {
	uint8_t *d = s->module_data;
	uint32_t len = s->module_len;

	uint8_t mark_2379[2] = { 0x23, 0x79 };
	uint8_t mark_23d1[2] = { 0x23, 0xd1 };
	uint8_t mark_c0fc[4] = { 0xc0, 0xfc, 0x04, 0x00 };
	uint8_t mark_0c79[4] = { 0x0c, 0x79, 0x04, 0x00 };

	int32_t a2379 = ft_find_marker(d, len, mark_2379, 2, 1000);
	int32_t a23d1 = ft_find_marker(d, len, mark_23d1, 2, 1000);
	int32_t ac0fc = ft_find_marker(d, len, mark_c0fc, 4, 1000);
	int32_t a0c79 = ft_find_marker(d, len, mark_0c79, 4, 1000);
	if(a2379 < 4 || a23d1 < 30 || ac0fc < 0 || a0c79 < 0) {
		return 0;
	}

	s->origin = ft_read_u32_be(d + a2379 - 4);

	uint32_t lens_abs  = ft_read_u32_be(d + a23d1 + 8);
	uint32_t addrs_abs = ft_read_u32_be(d + a23d1 - 6);
	uint32_t reps_abs  = ft_read_u32_be(d + a23d1 + 24);
	uint32_t nv_abs    = ft_read_u32_be(d + a23d1 - 30);
	uint32_t pat_abs   = ft_read_u32_be(d + ac0fc + 6);
	uint32_t pos_abs   = ft_read_u32_be(d + a0c79 + 12);
	s->song_length     = ft_read_u32_be(d + a0c79 + 34);

	s->sample_lens_off    = ft_resolve_ptr(s, lens_abs);
	s->sample_addrs_off   = ft_resolve_ptr(s, addrs_abs);
	s->sample_reps_off    = ft_resolve_ptr(s, reps_abs);
	s->sample_nv_off      = ft_resolve_ptr(s, nv_abs);
	s->pattern_data_off   = ft_resolve_ptr(s, pat_abs);
	s->position_table_off = ft_resolve_ptr(s, pos_abs);

	uint32_t any_bad = s->sample_lens_off | s->sample_addrs_off | s->sample_reps_off
	                 | s->sample_nv_off | s->pattern_data_off | s->position_table_off;
	if(any_bad == 0xffffffffu) {
		return 0;
	}

	// num_samples = (sample_addrs - sample_lens) / 4 (the three sample
	// tables are contiguous and identical-stride per the original asm).
	if(s->sample_addrs_off <= s->sample_lens_off) {
		return 0;
	}
	s->num_samples = (s->sample_addrs_off - s->sample_lens_off) / 4u;
	if(s->num_samples == 0 || s->num_samples > 64) {
		return 0;
	}

	// Bounds-check the tables so later accesses are safe.
	if(s->sample_lens_off + s->num_samples * 4u > s->module_len) {
		return 0;
	}
	if(s->sample_addrs_off + s->num_samples * 4u > s->module_len) {
		return 0;
	}
	if(s->sample_reps_off + s->num_samples * 4u > s->module_len) {
		return 0;
	}
	if(s->sample_nv_off + (uint32_t)FT_NUM_SLOTS * 4u > s->module_len) {
		return 0;
	}
	if(s->song_length == 0 || s->song_length > 256) {
		return 0;
	}
	if(s->position_table_off + s->song_length > s->module_len) {
		return 0;
	}

	// num_patterns = max(position_table) + 1.
	uint32_t max_pat = 0;
	for(uint32_t i = 0; i < s->song_length; ++i) {
		uint8_t pat = d[s->position_table_off + i];
		if(pat > max_pat) {
			max_pat = pat;
		}
	}
	s->num_patterns = max_pat + 1u;
	// The pointer recovered from the C0FC0400 marker lands 16 bytes before
	// the actual first row of pattern 0; the asm Play loop compensates with
	// an unconditional ADDI.W #$10 on every row advance, so the very first
	// audible read is at base+$10. Patterns themselves are 64 full rows of
	// music with no headers; we just correct the base pointer once at load
	// time and the runtime row indexing matches the asm.
	s->pattern_data_off += FT_BYTES_PER_ROW;
	if(s->pattern_data_off + s->num_patterns * (uint32_t)FT_BYTES_PER_PAT > s->module_len) {
		return 0;
	}
	return 1;
}

// [=]===^=[ ft_resolve_period_index ]============================================================[=]
// Find `period` in ft_period_table; on miss returns -1. Used by the slide
// effect to walk forward in the table from the row's authored period.
static int32_t ft_resolve_period_index(uint16_t period) {
	for(uint32_t i = 0; i < FT_PERIOD_TBL_LEN; ++i) {
		if(ft_period_table[i] == period) {
			return (int32_t)i;
		}
	}
	return -1;
}

// [=]===^=[ ft_apply_slide ]=====================================================================[=]
// Effect 1 helper: walk forward `steps` entries in the period table from
// the row's original period and feed the resulting period to paula.
static void ft_apply_slide(struct fashiontracker_state *s, int32_t voice, struct ft_voice *v, int32_t steps) {
	int32_t base = ft_resolve_period_index(v->row_period);
	if(base < 0) {
		return;
	}
	int32_t idx = base + steps;
	if(idx < 0) {
		idx = 0;
	}
	if(idx >= (int32_t)FT_PERIOD_TBL_LEN) {
		idx = FT_PERIOD_TBL_LEN - 1;
	}
	v->cur_period = ft_period_table[idx];
	paula_set_period(&s->paula, voice, v->cur_period);
}

// [=]===^=[ ft_trigger_voice ]===================================================================[=]
// Process a row entry on a single voice: trigger any sample that's named,
// set the period, and queue paula to play it.
static void ft_trigger_voice(struct fashiontracker_state *s, int32_t voice, struct ft_voice *v, uint8_t *row_voice) {
	uint16_t period = ft_read_u16_be(row_voice);
	uint8_t b2 = row_voice[2];
	uint8_t b3 = row_voice[3];

	if(period == 0) {
		// "No note this row": leave voice playing as-is.
		v->effect = 0;
		v->effect_arg = 0;
		return;
	}

	uint8_t slot4_byte = (uint8_t)(b2 & 0xf0);
	uint8_t slot4 = (uint8_t)(slot4_byte >> 2);   // sample-slot << 2 (already aligned for u16-pair stride)
	if(slot4 == 0) {
		// Period given but no slot -> use last-cached slot for this voice.
		slot4 = v->cached_slot4;
	} else {
		v->cached_slot4 = slot4;
	}

	uint16_t sample_no = ft_read_u16_be(s->module_data + s->sample_nv_off + slot4);
	uint16_t volume = ft_read_u16_be(s->module_data + s->sample_nv_off + slot4 + 2);
	if(volume > 64) {
		volume = 64;
	}

	if(sample_no < s->num_samples) {
		uint32_t sample_abs = ft_read_u32_be(s->module_data + s->sample_addrs_off + sample_no * 4u);
		uint32_t sample_off = ft_resolve_ptr(s, sample_abs);
		uint32_t sample_words = ft_read_u32_be(s->module_data + s->sample_lens_off + sample_no * 4u);
		uint32_t sample_bytes = sample_words * 2u;
		if(sample_bytes > 0x10000u) {
			sample_bytes = 0x10000u;       // Paula register limit (u16 length in words).
		}
		uint8_t loop_flag = s->module_data[s->sample_reps_off + sample_no * 4u + 3];
		if(sample_off != 0xffffffffu && sample_off + sample_bytes <= s->module_len) {
			v->sample_data = (int8_t *)(s->module_data + sample_off);
			v->sample_length_bytes = sample_bytes;
			v->sample_loops = (loop_flag == 1) ? 1 : 0;
			paula_play_sample(&s->paula, voice, v->sample_data, sample_bytes);
			if(v->sample_loops) {
				paula_set_loop(&s->paula, voice, 0, sample_bytes);
			} else {
				paula_set_loop(&s->paula, voice, 0, 0);
			}
		}
	}

	v->row_period = period;
	v->cur_period = period;
	v->volume = (uint8_t)volume;
	v->effect = (uint8_t)(b2 & 0x0f);
	v->effect_arg = b3;

	paula_set_period(&s->paula, voice, period);
	paula_set_volume(&s->paula, voice, volume);
}

// [=]===^=[ ft_step_row ]========================================================================[=]
// Advance to the next row, wrapping pattern/position as needed.
static void ft_step_row(struct fashiontracker_state *s) {
	s->cur_row_offset_bytes += FT_BYTES_PER_ROW;
	if(s->cur_row_offset_bytes >= FT_BYTES_PER_PAT) {
		s->cur_row_offset_bytes = 0;
		s->cur_position++;
		if(s->cur_position >= s->song_length) {
			s->cur_position = 0;
			s->end_reached = 1;
		}
	}
}

// [=]===^=[ ft_tick ]============================================================================[=]
// One playback tick. Per Fashion Tracker.asm: ticks 1, 2, 4, 5 run effect-1
// pitch-slide; tick 3 restores the row's authored period; tick 0 (counter==6
// rollover) advances to the next row and triggers fresh notes.
static void ft_tick(struct fashiontracker_state *s) {
	s->tick_in_row++;

	if(s->tick_in_row >= FT_TICKS_PER_ROW) {
		s->tick_in_row = 0;
		uint8_t pat = s->module_data[s->position_table_off + s->cur_position];
		if(pat >= s->num_patterns) {
			pat = 0;
		}
		uint32_t row_off = s->pattern_data_off + (uint32_t)pat * FT_BYTES_PER_PAT + s->cur_row_offset_bytes;
		for(int32_t voice = 0; voice < FT_VOICES; ++voice) {
			uint8_t *rv = s->module_data + row_off + voice * 4;
			ft_trigger_voice(s, voice, &s->voices[voice], rv);
		}
		ft_step_row(s);
		return;
	}

	// Per-tick effect handling.
	for(int32_t voice = 0; voice < FT_VOICES; ++voice) {
		struct ft_voice *v = &s->voices[voice];
		if(v->effect != 1) {
			continue;
		}
		switch(s->tick_in_row) {
			case 1:
			case 4: {
				ft_apply_slide(s, voice, v, (int32_t)(v->effect_arg >> 4));
				break;
			}
			case 2:
			case 5: {
				ft_apply_slide(s, voice, v, (int32_t)(v->effect_arg & 0x0f));
				break;
			}
			case 3: {
				v->cur_period = v->row_period;
				paula_set_period(&s->paula, voice, v->row_period);
				break;
			}
			default:
				break;
		}
	}
}

// [=]===^=[ fashiontracker_init ]================================================================[=]
static struct fashiontracker_state *fashiontracker_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 32 || sample_rate < 8000) {
		return 0;
	}
	if(!ft_identify((uint8_t *)data, len)) {
		return 0;
	}
	struct fashiontracker_state *s = (struct fashiontracker_state *)calloc(1, sizeof(struct fashiontracker_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;
	if(!ft_load(s)) {
		free(s);
		return 0;
	}
	paula_init(&s->paula, sample_rate, FT_TICK_HZ);
	s->cur_position = 0;
	s->cur_row_offset_bytes = 0;
	s->tick_in_row = (uint8_t)(FT_TICKS_PER_ROW - 1);
	return s;
}

// [=]===^=[ fashiontracker_free ]================================================================[=]
static void fashiontracker_free(struct fashiontracker_state *s) {
	if(s) {
		free(s);
	}
}

// [=]===^=[ fashiontracker_get_audio ]===========================================================[=]
static void fashiontracker_get_audio(struct fashiontracker_state *s, int16_t *output, int32_t frames) {
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
			ft_tick(s);
		}
	}
}

// [=]===^=[ fashiontracker_api_init ]============================================================[=]
static void *fashiontracker_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return fashiontracker_init(data, len, sample_rate);
}

// [=]===^=[ fashiontracker_api_free ]============================================================[=]
static void fashiontracker_api_free(void *state) {
	fashiontracker_free((struct fashiontracker_state *)state);
}

// [=]===^=[ fashiontracker_api_get_audio ]=======================================================[=]
static void fashiontracker_api_get_audio(void *state, int16_t *output, int32_t frames) {
	fashiontracker_get_audio((struct fashiontracker_state *)state, output, frames);
}

static const char *fashiontracker_extensions[] = { "ex", 0 };

static struct player_api fashiontracker_api = {
	"Fashion Tracker",
	fashiontracker_extensions,
	fashiontracker_api_init,
	fashiontracker_api_free,
	fashiontracker_api_get_audio,
	0,
};
