// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Jochen Hippel replayer, ported from NostalgicPlayer's C# implementation.
// Drives an Amiga Paula (see paula.h) at 50 Hz tick rate (PAL).
//
// Hippel modules come in three flavors which this player auto-detects:
//   - Hippel       (.hip)  -- TFMX-marked replayer with embedded m68k code
//   - Hippel COSO  (.hipc) -- packed/relocated header-only variant
//   - Hippel 7-V   (.hip7) -- seven-voice variant (TFMX or COSO base)
//
// The non-COSO variants are typically embedded in a ripped m68k executable,
// so the loader scans the player code to discover feature flags (effect set,
// vibrato/portamento variant, period table, etc.). The scanner mirrors the
// C# Identify and FindFeatures byte-pattern logic.
//
// Public API:
//   struct hippel_state *hippel_init(void *data, uint32_t len, int32_t sample_rate);
//   void hippel_free(struct hippel_state *s);
//   void hippel_get_audio(struct hippel_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define HIP_TICK_HZ          50
#define HIP_MAX_CHANNELS     7

enum {
	HIP_TYPE_UNKNOWN = 0,
	HIP_TYPE_HIPPEL,
	HIP_TYPE_HIPPEL_COSO,
	HIP_TYPE_HIPPEL_7V
};

struct hip_envelope {
	uint8_t envelope_speed;
	uint8_t frequency_number;
	uint8_t vibrato_speed;
	uint8_t vibrato_depth;
	uint8_t vibrato_delay;
	uint8_t *envelope_table;     // owned (may be 0 if empty/extended)
	uint32_t envelope_table_length;
};

struct hip_byte_seq {
	uint8_t *data;
	uint32_t length;
};

struct hip_sample {
	int8_t *sample_data;          // points into module buffer (caller-owned)
	uint32_t length;
	uint16_t volume;
	uint32_t loop_start;
	uint32_t loop_length;
};

struct hip_single_position_info {
	uint8_t track;
	int8_t note_transpose;
	int8_t envelope_transpose;
	uint8_t command;
};

struct hip_position {
	struct hip_single_position_info info[HIP_MAX_CHANNELS];
};

struct hip_song_info {
	uint16_t start_position;
	uint16_t last_position;
	uint16_t start_speed;
};

struct hip_global_playing_info {
	uint16_t speed_counter;
	uint16_t speed;
	uint16_t random;
};

struct hip_voice_info {
	uint8_t *envelope_table;       // points into hip_envelope.envelope_table OR default
	uint32_t envelope_table_length;
	int32_t envelope_position;
	int32_t original_envelope_number;
	int32_t current_envelope_number;
	uint8_t *envelope_table_alloc; // if non-null, this voice owns a private extended copy

	uint8_t *frequency_table;      // points into freq[i] data
	uint32_t frequency_table_length;
	int32_t frequency_position;
	int32_t original_frequency_number;
	int32_t current_frequency_number;

	uint32_t next_position;

	uint8_t *track;
	uint32_t track_length;
	uint32_t track_position;
	int8_t track_transpose;
	int8_t envelope_transpose;
	uint8_t current_track_number;

	uint8_t transpose;
	uint8_t current_note;
	uint8_t current_info;
	uint8_t previous_info;

	uint8_t sample;

	uint8_t tick;

	int8_t coso_counter;
	int8_t coso_speed;

	uint8_t volume;
	uint8_t envelope_counter;
	uint8_t envelope_speed;
	uint8_t envelope_sustain;

	uint8_t vibrato_flag;
	uint8_t vibrato_speed;
	uint8_t vibrato_delay;
	uint8_t vibrato_depth;
	uint8_t vibrato_delta;

	uint32_t porta_delta;

	uint8_t slide;
	uint8_t slide_sample;
	int32_t slide_end_position;
	uint16_t slide_loop_position;
	uint16_t slide_length;
	int16_t slide_delta;
	int8_t slide_counter;
	uint8_t slide_speed;
	uint8_t slide_active;
	uint8_t slide_done;

	uint8_t volume_fade;
	uint8_t volume_variation_depth;
	uint8_t volume_variation;
};

struct hippel_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	int32_t module_type;
	int32_t start_offset;
	uint8_t coso_mode_enabled;

	int32_t number_of_channels;
	int32_t number_of_positions;

	struct hip_byte_seq *frequencies;
	uint32_t frequencies_count;

	struct hip_envelope *envelopes;
	uint32_t envelopes_count;

	struct hip_byte_seq *tracks;
	uint32_t tracks_count;

	struct hip_position *position_list;
	uint32_t position_list_count;
	uint8_t *position_visited;            // 1 byte per position; 0 = not yet visited
	                                      // Mirrors C# HasPositionBeenVisited /
	                                      // MarkPositionAsVisited used to detect
	                                      // module-end on songs with non-linear
	                                      // jumps (only voice 0 marks).

	struct hip_song_info *song_info_list;
	uint32_t song_info_count;

	struct hip_sample *samples;
	uint32_t samples_count;

	int32_t current_song;

	struct hip_global_playing_info playing_info;
	struct hip_voice_info voices[HIP_MAX_CHANNELS];

	uint8_t enable_mute;
	uint8_t enable_frequency_previous_info;
	uint8_t enable_position_effect;
	uint8_t enable_frequency_reset_check;
	uint8_t enable_volume_fade;
	uint8_t enable_effect_loop;
	uint8_t convert_effects;
	uint8_t skip_id_check;
	uint8_t e9_ands;
	uint8_t e9_fix_sample;
	uint8_t reset_sustain;
	int32_t vibrato_version;
	int32_t portamento_version;

	int32_t effects_enabled[16];
	int32_t speed_init_value;

	uint16_t *period_table;
	uint32_t period_table_length;

	float playing_frequency;       // tempo for 7-voice variant
	uint8_t end_reached;
};

// [=]===^=[ hip_default_command_table ]==========================================================[=]
static uint8_t hip_default_command_table[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe1
};

// [=]===^=[ hip_periods1 ]=======================================================================[=]
static uint16_t hip_periods1[] = {
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  906,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
	 113,  113,  113,  113,  113,  113
};

// [=]===^=[ hip_periods2 ]=======================================================================[=]
static uint16_t hip_periods2[] = {
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  906,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
	 113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,
	3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1812
};

// [=]===^=[ hip_pan4 ]===========================================================================[=]
static uint8_t hip_pan4[] = { 0, 127, 127, 0 };

// [=]===^=[ hip_pan7 ]===========================================================================[=]
static uint8_t hip_pan7[] = { 0, 127, 127, 0, 0, 0, 0 };

// [=]===^=[ hip_read_b_u16 ]=====================================================================[=]
static uint16_t hip_read_b_u16(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ hip_read_b_u32 ]=====================================================================[=]
static uint32_t hip_read_b_u32(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ hip_read_b_i32 ]=====================================================================[=]
static int32_t hip_read_b_i32(uint8_t *p) {
	return (int32_t)hip_read_b_u32(p);
}

// [=]===^=[ hip_read_s8 ]========================================================================[=]
static int32_t hip_read_s8(uint8_t v) {
	return (int32_t)(int8_t)v;
}

// [=]===^=[ hip_bounded ]========================================================================[=]
// Returns 1 if [off, off+len) lies fully within the module buffer.
static int32_t hip_bounded(struct hippel_state *s, uint32_t off, uint32_t len) {
	if(off > s->module_len) {
		return 0;
	}
	if((off + len) > s->module_len) {
		return 0;
	}
	return 1;
}

// [=]===^=[ hip_play_sample_at ]=================================================================[=]
// Like paula_play_sample but starts playback at `start_offset` within the
// sample buffer and treats `length` as the number of bytes to play before
// looping. Loop bounds (set via paula_set_loop) reference absolute offsets
// in the same buffer.
static void hip_play_sample_at(struct paula *p, int32_t idx, int8_t *sample, uint32_t start_offset, uint32_t length) {
	struct paula_channel *c = &p->ch[idx];
	c->sample = sample;
	c->length_fp = (start_offset + length) << PAULA_FP_SHIFT;
	c->pos_fp = start_offset << PAULA_FP_SHIFT;
	c->loop_start_fp = 0;
	c->loop_length_fp = 0;
	c->has_pending = 0;
	c->pending_sample = 0;
	c->active = (sample != 0) && (length > 0);
}

// [=]===^=[ hip_has_7voices_structures ]=========================================================[=]
// Validates that the TFMX-marked structure at `header_index` (relative to module
// start) contains seven-voice position records. Mirrors C# Has7VoicesStructures.
static int32_t hip_has_7voices_structures(struct hippel_state *s, int32_t header_index) {
	if(!hip_bounded(s, (uint32_t)(header_index + 4), 16)) {
		return 0;
	}
	uint8_t *p = s->module_data + header_index + 4;

	int32_t offset = 0x20;

	uint16_t value = hip_read_b_u16(p);          // frequency tables
	offset += (value + 1) * 64;
	p += 2;

	value = hip_read_b_u16(p);                    // envelope tables
	offset += (value + 1) * 64;
	p += 2;

	uint16_t trks = hip_read_b_u16(p);
	p += 2;
	uint16_t positions = hip_read_b_u16(p);
	p += 2;
	uint16_t bytes_per_track = hip_read_b_u16(p);
	p += 2;
	p += 2;
	uint16_t sub_songs = hip_read_b_u16(p);

	offset += (trks + 1) * bytes_per_track;
	offset += (positions + 1) * 4 * 7;
	offset += sub_songs * 8;

	if(!hip_bounded(s, (uint32_t)(header_index + offset), 10)) {
		return 0;
	}

	uint8_t *q = s->module_data + header_index + offset;
	if((hip_read_b_u32(q) != 0) || (hip_read_b_u32(q + 4) != 0)) {
		return 0;
	}

	uint8_t c1 = q[8];
	uint8_t c2 = q[9];
	if((c1 < 0x20) || (c2 < 0x20) || (c1 > 0x7f) || (c2 > 0x7f)) {
		return 0;
	}
	return 1;
}

// [=]===^=[ hip_has_7voices_structures_in_coso ]=================================================[=]
static int32_t hip_has_7voices_structures_in_coso(struct hippel_state *s, int32_t header_index) {
	if(!hip_bounded(s, (uint32_t)(header_index + 28), 4)) {
		return 0;
	}
	int32_t sub_song_offset = hip_read_b_i32(s->module_data + header_index + 20);
	int32_t sample_data_offset = hip_read_b_i32(s->module_data + header_index + 24);

	if(!hip_bounded(s, (uint32_t)(header_index + 50), 2)) {
		return 0;
	}
	uint16_t sub_songs = hip_read_b_u16(s->module_data + header_index + 48);

	sub_song_offset += sub_songs * 8;

	if(!hip_bounded(s, (uint32_t)(header_index + sub_song_offset), 8)) {
		return 0;
	}

	if(hip_read_b_u16(s->module_data + header_index + 50) == 0) {
		return 0;
	}

	uint8_t *q = s->module_data + header_index + sub_song_offset;
	if((hip_read_b_u32(q) != 0) || (hip_read_b_u32(q + 4) != 0)) {
		return 0;
	}
	if((header_index + sub_song_offset + 8) != sample_data_offset) {
		return 0;
	}
	return 1;
}

// [=]===^=[ hip_extract_from_read_next_row ]=====================================================[=]
static int32_t hip_extract_from_read_next_row(struct hippel_state *s, uint8_t *buf, int32_t search_length) {
	int32_t index;
	int32_t start_index;

	for(index = 0; index < search_length - 6; index += 2) {
		if((buf[index] == 0x3e) && (buf[index + 1] == 0x3a) && (buf[index + 4] == 0x22) && (buf[index + 5] == 0x68)) {
			break;
		}
	}
	if(index >= (search_length - 6)) {
		return 0;
	}

	for(; index < search_length - 4; index += 2) {
		if((buf[index] == 0x4a) && (buf[index + 1] == 0x00) && (buf[index + 2] == 0x6b)) {
			break;
		}
	}
	if(index >= (search_length - 4)) {
		return 0;
	}

	index += 4;
	s->enable_mute = (buf[index] == 0x14) && (buf[index + 1] == 0x28);

	start_index = index;
	for(; index < search_length; index += 2) {
		if((buf[index] == 0x4e) && (buf[index + 1] == 0x75)) {
			break;
		}
	}
	if(index >= (search_length - 2)) {
		return 0;
	}

	for(; index >= start_index; index -= 2) {
		if((buf[index] == 0x08) && (buf[index + 1] == 0x28)) {
			break;
		}
	}

	s->enable_frequency_previous_info = index > start_index;
	return 1;
}

// [=]===^=[ hip_extract_from_do_effects ]========================================================[=]
static int32_t hip_extract_from_do_effects(struct hippel_state *s, uint8_t *buf, int32_t search_length) {
	int32_t index;
	int32_t run_effect_index;
	int32_t start_index;
	int32_t period_index;

	for(index = 0; index < search_length - 8; index += 2) {
		if((buf[index] == 0x4a) && (buf[index + 1] == 0x28) && (buf[index + 6] == 0x53) && (buf[index + 7] == 0x28)) {
			break;
		}
	}
	if(index >= (search_length - 8)) {
		return 0;
	}

	index += 10;
	if((buf[index] != 0x60) || (buf[index + 1] != 0x00)) {
		return 0;
	}

	run_effect_index = (hip_read_s8(buf[index + 2]) << 8) | (int32_t)buf[index + 3];
	run_effect_index += index + 2;

	for(int32_t i = 0; i < 16; ++i) {
		s->effects_enabled[i] = 0;
	}

	start_index = index;
	for(; index < search_length - 10; index += 2) {
		if((buf[index] == 0x0c) && (buf[index + 4] == 0x67) && (buf[index + 8] == 0x0c)) {
			break;
		}
	}

	if(index < (search_length - 10)) {
		index += 8;

		while((index < (search_length - 8)) && (buf[index] == 0x0c)) {
			int32_t effect = (hip_read_s8(buf[index + 2]) << 8) | (int32_t)buf[index + 3];
			if((effect < 0xe0) || (effect > 0xef)) {
				return 0;
			}

			s->effects_enabled[effect - 0xe0] = 1;

			if(effect == 0xe6) {
				if((buf[index + 6] == 0x0c) && (buf[index + 7] == 0x00)) {
					s->effects_enabled[6] = 0;
				}
			} else if(effect == 0xe7) {
				if((buf[index + 12] == 0xb2) && (buf[index + 13] == 0x28)) {
					s->effects_enabled[7] = 2;
				}
			}

			int32_t offset = (buf[index + 5] == 0x00)
				? ((hip_read_s8(buf[index + 6]) << 8) | (int32_t)buf[index + 7])
				: (int32_t)buf[index + 5];
			index += offset + 6;
		}

		if(index >= (search_length - 8)) {
			return 0;
		}

		s->skip_id_check = 0;
		s->e9_ands = 0;
		s->e9_fix_sample = 0;
		s->enable_effect_loop = 0;
	} else {
		for(index = start_index; index < search_length - 4; index += 2) {
			if((buf[index] == 0x4e) && (buf[index + 1] == 0xfb)) {
				break;
			}
		}
		if(index >= (search_length - 4)) {
			return 0;
		}

		int32_t first_code_after_jump_table = (hip_read_s8(buf[index + 2]) << 8) | (int32_t)buf[index + 3];
		first_code_after_jump_table += index + 2;

		for(int32_t i = 0, j = index + 4; j < first_code_after_jump_table; j += 2, ++i) {
			if(i < 16) {
				s->effects_enabled[i] = 1;
			}
		}

		if(s->effects_enabled[5] != 0) {
			s->effects_enabled[5] = 2;
		}
		if(s->effects_enabled[7] != 0) {
			s->effects_enabled[7] = 2;
		}
		if(s->effects_enabled[9] != 0) {
			s->e9_ands = 1;
			s->e9_fix_sample = 1;
		}

		s->skip_id_check = 1;
		s->enable_effect_loop = 1;
	}

	for(index = run_effect_index; index < search_length - 4; index += 2) {
		if((buf[index] == 0x43) && (buf[index + 1] == 0xfa)) {
			break;
		}
	}
	if(index >= (search_length - 4)) {
		return 0;
	}

	period_index = (hip_read_s8(buf[index + 2]) << 8) | (int32_t)buf[index + 3];
	period_index += index + 2;
	if(period_index > (search_length - 116)) {
		return 0;
	}

	if((buf[period_index] != 0x06) || (buf[period_index + 1] != 0xb0)) {
		return 0;
	}

	if((period_index < (search_length - 128))
		&& (buf[period_index + 118] == 0x00) && (buf[period_index + 119] == 0x71)
		&& (buf[period_index + 120] == 0x0d) && (buf[period_index + 121] == 0x60)) {
		s->period_table = hip_periods2;
		s->period_table_length = sizeof(hip_periods2) / sizeof(hip_periods2[0]);
	} else {
		s->period_table = hip_periods1;
		s->period_table_length = sizeof(hip_periods1) / sizeof(hip_periods1[0]);
	}

	for(; index < search_length - 6; index += 2) {
		if(((buf[index] == 0x53) && (buf[index + 1] == 0x28)) || (buf[index + 4] == 0x60)) {
			break;
		}
	}
	if(index >= (search_length - 6)) {
		return 0;
	}

	index += 6;
	if((buf[index] == 0x1a) && (buf[index + 1] == 0x01) && (buf[index + 2] == 0x18) && (buf[index + 3] == 0x28)) {
		s->vibrato_version = 1;
	} else if((buf[index] == 0x78) && (buf[index + 1] == 0x00) && (buf[index + 2] == 0x7a) && (buf[index + 3] == 0x00)) {
		s->vibrato_version = 2;
	} else if((buf[index] == 0x72) && (buf[index + 1] == 0x00) && (buf[index + 2] == 0x78) && (buf[index + 3] == 0x00)) {
		s->vibrato_version = 3;
	} else {
		return 0;
	}

	return 1;
}

// [=]===^=[ hip_extract_from_initialize_structures ]=============================================[=]
static int32_t hip_extract_from_initialize_structures(struct hippel_state *s, uint8_t *buf, int32_t search_length) {
	int32_t index;

	for(index = 0; index < search_length - 6; index += 2) {
		if(((buf[index] == 0x3c) && (buf[index + 1] == 0xd9) && (buf[index + 2] == 0x61) && (buf[index + 3] == 0x00))
			|| ((buf[index] == 0x3c) && (buf[index + 1] == 0xe9) && (buf[index + 4] == 0x61) && (buf[index + 5] == 0x00))) {
			break;
		}
	}
	if(index >= (search_length - 6)) {
		return 0;
	}

	if(buf[index + 1] == 0xe9) {
		index += 2;
	}

	int32_t base_index = index;
	index = (hip_read_s8(buf[base_index + 4]) << 8) | (int32_t)buf[base_index + 5];
	index += base_index + 4;
	if((index < 0) || (index >= search_length)) {
		return 0;
	}

	for(; index < search_length - 12; index += 2) {
		if((buf[index] == 0x51) && (buf[index + 1] == 0xc8)) {
			break;
		}
	}
	if(index >= (search_length - 12)) {
		return 0;
	}

	index += 8;
	if((buf[index] == 0x30) && (buf[index + 1] == 0xbc)) {
		s->speed_init_value = ((int32_t)buf[index + 2] << 8) | (int32_t)buf[index + 3];
	} else {
		s->speed_init_value = -1;
	}

	return 1;
}

// [=]===^=[ hip_find_features ]==================================================================[=]
static int32_t hip_find_features(struct hippel_state *s, uint8_t *buf, int32_t search_length) {
	if(!hip_extract_from_read_next_row(s, buf, search_length)) {
		return 0;
	}
	if(!hip_extract_from_do_effects(s, buf, search_length)) {
		return 0;
	}
	if(!hip_extract_from_initialize_structures(s, buf, search_length)) {
		return 0;
	}
	return 1;
}

// [=]===^=[ hip_set_default_7v_features ]========================================================[=]
static void hip_set_default_7v_features(struct hippel_state *s) {
	for(int32_t i = 0; i < 16; ++i) {
		s->effects_enabled[i] = 0;
	}

	s->enable_mute = 0;
	s->enable_frequency_previous_info = 1;
	s->enable_position_effect = 0;
	s->enable_frequency_reset_check = 1;
	s->enable_volume_fade = 1;
	s->enable_effect_loop = 1;
	s->convert_effects = 0;
	s->skip_id_check = 1;
	s->e9_ands = 1;
	s->e9_fix_sample = 1;
	s->reset_sustain = 1;

	s->vibrato_version = 3;
	s->portamento_version = 2;
	s->speed_init_value = 1;

	s->effects_enabled[2] = 1;
	s->effects_enabled[3] = 1;
	s->effects_enabled[4] = 1;
	s->effects_enabled[5] = 2;
	s->effects_enabled[6] = 1;
	s->effects_enabled[7] = 2;
	s->effects_enabled[8] = 1;
	s->effects_enabled[9] = 1;
	s->effects_enabled[10] = 1;

	s->period_table = hip_periods2;
	s->period_table_length = sizeof(hip_periods2) / sizeof(hip_periods2[0]);
}

// [=]===^=[ hip_test_module ]====================================================================[=]
// Search for a TFMX-marked replayer struct embedded in an m68k binary
// (via "lea xxx,a0" / 0x41 0xfa addressing). Returns 1 on match.
static int32_t hip_test_module(struct hippel_state *s, uint8_t *buf, int32_t search_length) {
	for(int32_t i = 0; i < search_length - 4; i += 2) {
		if((buf[i] == 0x41) && (buf[i + 1] == 0xfa)) {
			int32_t index = (hip_read_s8(buf[i + 2]) << 8) | (int32_t)buf[i + 3];
			index += i + 2;
			if((index < 0) || (index >= (search_length - 4))) {
				return 0;
			}
			if((buf[index] == 'T') && (buf[index + 1] == 'F') && (buf[index + 2] == 'M') && (buf[index + 3] == 'X')) {
				if(hip_has_7voices_structures(s, index)) {
					return 0;
				}

				s->start_offset = index;
				s->coso_mode_enabled = 0;

				if(!hip_find_features(s, buf, search_length)) {
					return 0;
				}

				s->enable_position_effect = 0;
				s->enable_frequency_reset_check = 0;
				s->enable_volume_fade = 0;
				s->convert_effects = 0;
				s->reset_sustain = 1;
				s->portamento_version = 1;
				s->module_type = HIP_TYPE_HIPPEL;
				return 1;
			}
		}
	}
	return 0;
}

// [=]===^=[ hip_test_module_for_coso ]===========================================================[=]
static int32_t hip_test_module_for_coso(struct hippel_state *s) {
	if(s->module_len < 64) {
		return 0;
	}
	uint8_t *d = s->module_data;
	if((d[0] != 'C') || (d[1] != 'O') || (d[2] != 'S') || (d[3] != 'O')) {
		return 0;
	}
	if((d[32] != 'T') || (d[33] != 'F') || (d[34] != 'M') || (d[35] != 'X')) {
		return 0;
	}

	uint16_t number_of_samples = hip_read_b_u16(d + 50);
	if(number_of_samples == 0) {
		return 0;
	}
	if(hip_read_b_u16(d + 52) != 0) {
		return 0;
	}

	uint32_t offset = hip_read_b_u32(d + 24);                  // sample info offset

	uint32_t check_off = offset + (uint32_t)number_of_samples * 4u;
	if(!hip_bounded(s, check_off, 4)) {
		return 0;
	}
	if((hip_read_b_u16(d + check_off) == 0) && (hip_read_b_u16(d + check_off + 2) == 0)) {
		return 0;
	}

	if(hip_has_7voices_structures_in_coso(s, 0)) {
		return 0;
	}

	s->start_offset = 0;
	s->coso_mode_enabled = 1;
	s->module_type = HIP_TYPE_HIPPEL_COSO;
	return 1;
}

// [=]===^=[ hip_test_module_for_7voices ]========================================================[=]
static int32_t hip_test_module_for_7voices(struct hippel_state *s, uint8_t *buf, int32_t search_length) {
	uint8_t *d = s->module_data;

	if((s->module_len > 4) && (d[0] == 'T') && (d[1] == 'F') && (d[2] == 'M') && (d[3] == 'X') && (d[4] != ' ')) {
		if(!hip_has_7voices_structures(s, 0)) {
			return 0;
		}
		s->start_offset = 0;
		s->coso_mode_enabled = 0;
		hip_set_default_7v_features(s);
		s->module_type = HIP_TYPE_HIPPEL_7V;
		return 1;
	}

	if((s->module_len > 36)
		&& (d[0] == 'C') && (d[1] == 'O') && (d[2] == 'S') && (d[3] == 'O')
		&& (d[32] == 'T') && (d[33] == 'F') && (d[34] == 'M') && (d[35] == 'X')) {
		if(!hip_has_7voices_structures_in_coso(s, 0)) {
			return 0;
		}
		s->start_offset = 0;
		s->coso_mode_enabled = 1;
		hip_set_default_7v_features(s);
		s->module_type = HIP_TYPE_HIPPEL_7V;
		return 1;
	}

	for(int32_t i = 0; i < search_length - 4; i += 2) {
		if((buf[i] == 0x41) && (buf[i + 1] == 0xfa)) {
			int32_t index = (hip_read_s8(buf[i + 2]) << 8) | (int32_t)buf[i + 3];
			index += i + 2;
			if((index < 0) || (index >= (search_length - 4))) {
				return 0;
			}
			if((buf[index] == 'T') && (buf[index + 1] == 'F') && (buf[index + 2] == 'M') && (buf[index + 3] == 'X')) {
				if(!hip_has_7voices_structures(s, index)) {
					return 0;
				}
				s->start_offset = index;
				s->coso_mode_enabled = 0;
				if(!hip_find_features(s, buf, search_length)) {
					return 0;
				}
				s->enable_position_effect = 0;
				s->enable_frequency_reset_check = 1;
				s->enable_volume_fade = 1;
				s->convert_effects = 0;
				s->reset_sustain = 1;
				s->portamento_version = 2;
				s->module_type = HIP_TYPE_HIPPEL_7V;
				return 1;
			}
		}
	}
	return 0;
}

// [=]===^=[ hip_identify ]=======================================================================[=]
// Top-level identifier. Tries 7-voice, then COSO, then plain Hippel.
static int32_t hip_identify(struct hippel_state *s) {
	if(s->module_len < 1024) {
		return 0;
	}

	uint8_t *d = s->module_data;
	// SC68 prefix: not handled here.
	if((d[0] == 0x53) && (d[1] == 0x43) && (d[2] == 0x36) && (d[3] == 0x38)) {
		return 0;
	}

	int32_t scan7 = 32768;
	if((uint32_t)scan7 > s->module_len) {
		scan7 = (int32_t)s->module_len;
	}
	int32_t scan = 16384;
	if((uint32_t)scan > s->module_len) {
		scan = (int32_t)s->module_len;
	}

	if(hip_test_module_for_7voices(s, d, scan7)) {
		s->number_of_channels = 7;
		return 1;
	}
	if(hip_test_module_for_coso(s)) {
		s->number_of_channels = 4;
		return 1;
	}
	if(hip_test_module(s, d, scan)) {
		s->number_of_channels = 4;
		return 1;
	}
	return 0;
}

// [=]===^=[ hip_load_frequencies ]===============================================================[=]
static int32_t hip_load_frequencies(struct hippel_state *s, uint32_t number_of_frequencies, uint32_t *cur, uint32_t coso_freq_off, uint32_t coso_env_off) {
	s->frequencies = (struct hip_byte_seq *)calloc(number_of_frequencies, sizeof(struct hip_byte_seq));
	if(!s->frequencies) {
		return 0;
	}
	s->frequencies_count = number_of_frequencies;

	if(s->coso_mode_enabled) {
		if((coso_freq_off == 0) || (coso_env_off == 0)) {
			return 0;
		}
		uint32_t off_table = (uint32_t)s->start_offset + coso_freq_off;
		if(!hip_bounded(s, off_table, number_of_frequencies * 2u)) {
			return 0;
		}

		for(uint32_t i = 0; i < number_of_frequencies; ++i) {
			uint16_t cur_off = hip_read_b_u16(s->module_data + off_table + i * 2u);
			uint16_t next_off;
			int32_t length = 0;

			for(uint32_t j = 1; j < (number_of_frequencies - i + 1); ++j) {
				if((i + j) < number_of_frequencies) {
					next_off = hip_read_b_u16(s->module_data + off_table + (i + j) * 2u);
				} else {
					next_off = (uint16_t)coso_env_off;
				}
				length = (int32_t)next_off - (int32_t)cur_off;
				if(length != 0) {
					break;
				}
			}

			if(length <= 0) {
				s->frequencies[i].data = 0;
				s->frequencies[i].length = 0;
			} else {
				uint32_t src = (uint32_t)s->start_offset + cur_off;
				if(!hip_bounded(s, src, (uint32_t)length)) {
					return 0;
				}
				s->frequencies[i].data = (uint8_t *)malloc((uint32_t)length);
				if(!s->frequencies[i].data) {
					return 0;
				}
				memcpy(s->frequencies[i].data, s->module_data + src, (uint32_t)length);
				s->frequencies[i].length = (uint32_t)length;
			}
		}
	} else {
		for(uint32_t i = 0; i < number_of_frequencies; ++i) {
			if(!hip_bounded(s, *cur, 64)) {
				return 0;
			}
			s->frequencies[i].data = (uint8_t *)malloc(64);
			if(!s->frequencies[i].data) {
				return 0;
			}
			memcpy(s->frequencies[i].data, s->module_data + *cur, 64);
			s->frequencies[i].length = 64;
			*cur += 64;
		}
	}
	return 1;
}

// [=]===^=[ hip_read_envelope ]==================================================================[=]
static int32_t hip_read_envelope(struct hippel_state *s, struct hip_envelope *env, uint32_t src, int32_t length) {
	if(length < 5) {
		// Empty / placeholder envelope.
		env->envelope_table = 0;
		env->envelope_table_length = 0;
		return 1;
	}
	if(!hip_bounded(s, src, (uint32_t)length)) {
		return 0;
	}
	uint8_t *p = s->module_data + src;
	env->envelope_speed = p[0];
	env->frequency_number = p[1];
	env->vibrato_speed = p[2];
	env->vibrato_depth = p[3];
	env->vibrato_delay = p[4];

	uint32_t tlen = (uint32_t)(length - 5);
	if(tlen == 0) {
		env->envelope_table = 0;
		env->envelope_table_length = 0;
	} else {
		env->envelope_table = (uint8_t *)malloc(tlen);
		if(!env->envelope_table) {
			return 0;
		}
		memcpy(env->envelope_table, p + 5, tlen);
		env->envelope_table_length = tlen;
	}
	return 1;
}

// [=]===^=[ hip_load_envelopes ]=================================================================[=]
static int32_t hip_load_envelopes(struct hippel_state *s, uint32_t number_of_envelopes, uint32_t *cur, uint32_t coso_env_off, uint32_t coso_tracks_off) {
	s->envelopes = (struct hip_envelope *)calloc(number_of_envelopes, sizeof(struct hip_envelope));
	if(!s->envelopes) {
		return 0;
	}
	s->envelopes_count = number_of_envelopes;

	if(s->coso_mode_enabled) {
		if((coso_env_off == 0) || (coso_tracks_off == 0)) {
			return 0;
		}
		uint32_t off_table = (uint32_t)s->start_offset + coso_env_off;
		if(!hip_bounded(s, off_table, number_of_envelopes * 2u)) {
			return 0;
		}

		for(uint32_t i = 0; i < number_of_envelopes; ++i) {
			uint16_t cur_off = hip_read_b_u16(s->module_data + off_table + i * 2u);
			uint16_t next_off;
			int32_t length = 0;

			for(uint32_t j = 1; j < (number_of_envelopes - i + 1); ++j) {
				if((i + j) < number_of_envelopes) {
					next_off = hip_read_b_u16(s->module_data + off_table + (i + j) * 2u);
				} else {
					next_off = (uint16_t)coso_tracks_off;
				}
				length = (int32_t)next_off - (int32_t)cur_off;
				if(length != 0) {
					break;
				}
			}

			if(length <= 0) {
				s->envelopes[i].envelope_table = 0;
				s->envelopes[i].envelope_table_length = 0;
			} else {
				uint32_t src = (uint32_t)s->start_offset + cur_off;
				if(!hip_read_envelope(s, &s->envelopes[i], src, length)) {
					return 0;
				}
			}
		}
	} else {
		for(uint32_t i = 0; i < number_of_envelopes; ++i) {
			if(!hip_read_envelope(s, &s->envelopes[i], *cur, 64)) {
				return 0;
			}
			*cur += 64;
		}
	}
	return 1;
}

// [=]===^=[ hip_load_tracks ]====================================================================[=]
static int32_t hip_load_tracks(struct hippel_state *s, uint32_t number_of_tracks, uint16_t bytes_per_track, uint32_t *cur, uint32_t coso_tracks_off, uint32_t coso_pos_off) {
	s->tracks = (struct hip_byte_seq *)calloc(number_of_tracks, sizeof(struct hip_byte_seq));
	if(!s->tracks) {
		return 0;
	}
	s->tracks_count = number_of_tracks;

	if(s->coso_mode_enabled) {
		if((coso_tracks_off == 0) || (coso_pos_off == 0)) {
			return 0;
		}
		uint32_t off_table = (uint32_t)s->start_offset + coso_tracks_off;
		if(!hip_bounded(s, off_table, number_of_tracks * 2u)) {
			return 0;
		}

		for(uint32_t i = 0; i < number_of_tracks; ++i) {
			uint16_t cur_off = hip_read_b_u16(s->module_data + off_table + i * 2u);
			uint16_t next_off;
			if((i + 1) < number_of_tracks) {
				next_off = hip_read_b_u16(s->module_data + off_table + (i + 1) * 2u);
			} else {
				next_off = (uint16_t)coso_pos_off;
			}
			int32_t length = (int32_t)next_off - (int32_t)cur_off;
			if(length < 0) {
				return 0;
			}
			s->tracks[i].length = (uint32_t)length;
			if(length == 0) {
				s->tracks[i].data = 0;
			} else {
				uint32_t src = (uint32_t)s->start_offset + cur_off;
				if(!hip_bounded(s, src, (uint32_t)length)) {
					return 0;
				}
				s->tracks[i].data = (uint8_t *)malloc((uint32_t)length);
				if(!s->tracks[i].data) {
					return 0;
				}
				memcpy(s->tracks[i].data, s->module_data + src, (uint32_t)length);
			}
		}
	} else {
		for(uint32_t i = 0; i < number_of_tracks; ++i) {
			if(!hip_bounded(s, *cur, bytes_per_track)) {
				return 0;
			}
			s->tracks[i].data = (uint8_t *)malloc(bytes_per_track);
			if(!s->tracks[i].data) {
				return 0;
			}
			memcpy(s->tracks[i].data, s->module_data + *cur, bytes_per_track);
			s->tracks[i].length = bytes_per_track;
			*cur += bytes_per_track;
		}
	}
	return 1;
}

// [=]===^=[ hip_load_position_list ]=============================================================[=]
static int32_t hip_load_position_list(struct hippel_state *s, uint32_t num_positions, uint32_t *cur, uint32_t coso_pos_off) {
	s->position_list = (struct hip_position *)calloc(num_positions, sizeof(struct hip_position));
	if(!s->position_list) {
		return 0;
	}
	s->position_list_count = num_positions;
	s->position_visited = (uint8_t *)calloc(num_positions, 1);
	if(!s->position_visited) {
		return 0;
	}

	uint32_t off;
	if(s->coso_mode_enabled) {
		if(coso_pos_off == 0) {
			return 0;
		}
		off = (uint32_t)s->start_offset + coso_pos_off;
	} else {
		off = *cur;
	}

	int32_t per_voice = (s->module_type == HIP_TYPE_HIPPEL_7V) ? 4 : 3;
	uint32_t total = num_positions * (uint32_t)s->number_of_channels * (uint32_t)per_voice;
	if(!hip_bounded(s, off, total)) {
		return 0;
	}

	uint8_t *p = s->module_data + off;
	for(uint32_t i = 0; i < num_positions; ++i) {
		for(int32_t j = 0; j < s->number_of_channels; ++j) {
			s->position_list[i].info[j].track = p[0];
			s->position_list[i].info[j].note_transpose = (int8_t)p[1];
			s->position_list[i].info[j].envelope_transpose = (int8_t)p[2];
			if(per_voice == 4) {
				s->position_list[i].info[j].command = p[3];
			} else {
				s->position_list[i].info[j].command = 0;
			}
			p += per_voice;
		}
	}

	if(!s->coso_mode_enabled) {
		*cur = off + total;
	}
	return 1;
}

// [=]===^=[ hip_load_song_info ]=================================================================[=]
static int32_t hip_load_song_info(struct hippel_state *s, uint32_t number_of_sub_songs, uint32_t *cur, uint32_t coso_subsong_off) {
	if(number_of_sub_songs == 0) {
		return 0;
	}

	uint32_t off;
	if(s->coso_mode_enabled) {
		if(coso_subsong_off == 0) {
			return 0;
		}
		off = (uint32_t)s->start_offset + coso_subsong_off;
	} else {
		off = *cur;
	}

	int32_t entry = (s->module_type == HIP_TYPE_HIPPEL_7V) ? 8 : 6;
	uint32_t total = number_of_sub_songs * (uint32_t)entry;
	if(!hip_bounded(s, off, total)) {
		return 0;
	}

	s->song_info_list = (struct hip_song_info *)calloc(number_of_sub_songs, sizeof(struct hip_song_info));
	if(!s->song_info_list) {
		return 0;
	}
	s->song_info_count = 0;

	uint8_t *p = s->module_data + off;
	for(uint32_t i = 0; i < number_of_sub_songs; ++i) {
		uint16_t start_position = hip_read_b_u16(p);
		uint16_t last_position = (uint16_t)(hip_read_b_u16(p + 2) + 1);
		uint16_t start_speed;
		if(entry == 8) {
			start_speed = hip_read_b_u16(p + 6);
		} else {
			start_speed = hip_read_b_u16(p + 4);
		}

		if((start_speed != 0) && (start_position <= last_position)) {
			s->song_info_list[s->song_info_count].start_position = start_position;
			s->song_info_list[s->song_info_count].last_position = last_position;
			s->song_info_list[s->song_info_count].start_speed = start_speed;
			++s->song_info_count;
		}

		p += entry;
	}

	if(s->song_info_count == 0) {
		return 0;
	}

	// Total positions = max(last_position).
	uint16_t max_last = 0;
	for(uint32_t i = 0; i < s->song_info_count; ++i) {
		if(s->song_info_list[i].last_position > max_last) {
			max_last = s->song_info_list[i].last_position;
		}
	}
	s->number_of_positions = max_last;

	if(!s->coso_mode_enabled) {
		uint32_t pad = (s->module_type == HIP_TYPE_HIPPEL_7V) ? 8 : 6;
		*cur = off + total + pad;
	}
	return 1;
}

// [=]===^=[ hip_load_sample_info ]===============================================================[=]
// Returns array of sample data offsets (caller must free) on success, 0 on failure.
static uint32_t *hip_load_sample_info(struct hippel_state *s, uint32_t number_of_samples, uint32_t *cur, uint32_t coso_sampleinfo_off) {
	if(number_of_samples == 0) {
		return 0;
	}

	s->samples = (struct hip_sample *)calloc(number_of_samples, sizeof(struct hip_sample));
	if(!s->samples) {
		return 0;
	}
	s->samples_count = number_of_samples;

	uint32_t *sample_offsets = (uint32_t *)calloc(number_of_samples, sizeof(uint32_t));
	if(!sample_offsets) {
		return 0;
	}

	uint32_t off;
	if(s->coso_mode_enabled) {
		if(coso_sampleinfo_off == 0) {
			free(sample_offsets);
			return 0;
		}
		off = (uint32_t)s->start_offset + coso_sampleinfo_off;
	} else {
		off = *cur;
	}

	uint32_t entry = s->coso_mode_enabled ? 12u : 30u;     // CO: 4+2+2+2 = 12; non-CO: 18+4+2+2+2+2 = 30
	uint32_t total = number_of_samples * entry;
	if(!hip_bounded(s, off, total)) {
		free(sample_offsets);
		return 0;
	}

	uint8_t *p = s->module_data + off;
	for(uint32_t i = 0; i < number_of_samples; ++i) {
		struct hip_sample *sm = &s->samples[i];
		if(!s->coso_mode_enabled) {
			p += 18;                                       // skip name
		}
		sample_offsets[i] = hip_read_b_u32(p);
		p += 4;
		sm->length = (uint32_t)hip_read_b_u16(p) * 2u;
		p += 2;
		if(s->coso_mode_enabled) {
			sm->volume = 64;
		} else {
			sm->volume = hip_read_b_u16(p);
			if(s->module_type == HIP_TYPE_HIPPEL_7V) {
				sm->volume = 64;
			}
			p += 2;
		}
		sm->loop_start = (uint32_t)(hip_read_b_u16(p) & ~1u);
		p += 2;
		sm->loop_length = (uint32_t)hip_read_b_u16(p) * 2u;
		p += 2;

		if((sm->loop_start + sm->loop_length) > sm->length) {
			if(sm->loop_start <= sm->length) {
				sm->loop_length = sm->length - sm->loop_start;
			} else {
				sm->loop_length = 0;
			}
		}
	}

	if(!s->coso_mode_enabled) {
		*cur = off + total;
	}
	return sample_offsets;
}

// [=]===^=[ hip_load_sample_data ]===============================================================[=]
// Resolves each sample's pointer into the module buffer (no copy; caller-owned).
static int32_t hip_load_sample_data(struct hippel_state *s, uint32_t *sample_offsets, uint32_t cur, uint32_t coso_sampledata_off) {
	uint32_t base;
	if(s->coso_mode_enabled) {
		if((int32_t)coso_sampledata_off < 0) {
			// Sample data lives in an external file; not supported here.
			return 0;
		}
		base = (uint32_t)s->start_offset + coso_sampledata_off;
	} else {
		base = cur;
	}

	for(uint32_t i = 0; i < s->samples_count; ++i) {
		struct hip_sample *sm = &s->samples[i];
		uint32_t at = base + sample_offsets[i];
		if(sm->length == 0) {
			sm->sample_data = 0;
			continue;
		}
		if(!hip_bounded(s, at, sm->length)) {
			// Tolerate by clamping to whatever is available.
			if(at >= s->module_len) {
				return 0;
			}
			sm->length = s->module_len - at;
			if((sm->loop_start + sm->loop_length) > sm->length) {
				if(sm->loop_start <= sm->length) {
					sm->loop_length = sm->length - sm->loop_start;
				} else {
					sm->loop_length = 0;
				}
			}
		}
		sm->sample_data = (int8_t *)(s->module_data + at);
	}
	return 1;
}

// [=]===^=[ hip_enable_coso_features ]===========================================================[=]
// Apply per-module quirk flags based on the COSO offset checksum (table from Flod).
static void hip_enable_coso_features(struct hippel_state *s, uint32_t freq_off, uint32_t env_off, uint32_t tracks_off, uint32_t pos_off, uint32_t subsongs_off, uint32_t sampleinfo_off, int32_t sampledata_off) {
	int64_t checksum = (int64_t)freq_off + (int64_t)env_off + (int64_t)tracks_off + (int64_t)pos_off
		+ (int64_t)subsongs_off + (int64_t)sampleinfo_off + (int64_t)sampledata_off;

	if(hip_bounded(s, (uint32_t)(s->start_offset + 47), 1)) {
		checksum += s->module_data[s->start_offset + 47];
	}

	for(int32_t i = 0; i < 16; ++i) {
		s->effects_enabled[i] = 0;
	}

	switch(checksum) {
		case 22660:
		case 22670:
		case 18845:
		case 30015:
		case 22469:
		case 3549: {
			s->enable_mute = 1;
			s->enable_position_effect = 0;
			s->enable_frequency_reset_check = 0;
			s->enable_volume_fade = 0;
			s->convert_effects = 0;
			s->vibrato_version = 1;
			s->portamento_version = 1;
			s->effects_enabled[2] = 1;
			s->effects_enabled[3] = 1;
			s->effects_enabled[4] = 1;
			s->effects_enabled[7] = 1;
			s->effects_enabled[8] = 1;
			break;
		}

		case 16948:
		case 18337:
		case 13704: {
			s->enable_mute = 0;
			s->enable_position_effect = 0;
			s->enable_frequency_reset_check = 1;
			s->enable_volume_fade = 0;
			s->convert_effects = 0;
			s->vibrato_version = 1;
			s->portamento_version = 1;
			s->effects_enabled[2] = 1;
			s->effects_enabled[3] = 1;
			s->effects_enabled[4] = 1;
			s->effects_enabled[5] = 1;
			s->effects_enabled[7] = 2;
			s->effects_enabled[8] = 1;
			s->effects_enabled[9] = 1;
			break;
		}

		case 18548:
		case 13928:
		case 8764:
		case 17244:
		case 11397:
		case 14496:
		case 14394:
		case 13578:
		case 6524: {
			s->enable_mute = 0;
			s->enable_position_effect = 0;
			s->enable_frequency_reset_check = 1;
			s->enable_volume_fade = 0;
			s->convert_effects = 1;
			s->vibrato_version = 2;
			s->portamento_version = 1;
			s->effects_enabled[2] = 1;
			s->effects_enabled[3] = 1;
			s->effects_enabled[4] = 1;
			s->effects_enabled[5] = 2;
			s->effects_enabled[6] = 1;
			s->effects_enabled[7] = 2;
			s->effects_enabled[8] = 1;
			s->effects_enabled[9] = 1;
			break;
		}

		default: {
			s->enable_mute = 0;
			s->enable_position_effect = 1;
			s->enable_frequency_reset_check = 1;
			s->enable_volume_fade = 1;
			s->convert_effects = 0;
			s->vibrato_version = 3;
			s->portamento_version = 2;
			s->effects_enabled[2] = 1;
			s->effects_enabled[3] = 1;
			s->effects_enabled[4] = 1;
			s->effects_enabled[5] = 2;
			s->effects_enabled[6] = 1;
			s->effects_enabled[7] = 2;
			s->effects_enabled[8] = 1;
			s->effects_enabled[9] = 1;
			s->effects_enabled[10] = 1;
			break;
		}
	}

	s->enable_frequency_previous_info = 1;
	s->skip_id_check = 1;
	s->e9_ands = 1;
	s->e9_fix_sample = 1;
	s->enable_effect_loop = 1;
	s->reset_sustain = 0;
	s->speed_init_value = 1;
	s->period_table = hip_periods2;
	s->period_table_length = sizeof(hip_periods2) / sizeof(hip_periods2[0]);
}

// [=]===^=[ hip_load ]===========================================================================[=]
// Parses the module after identification.
static int32_t hip_load(struct hippel_state *s) {
	uint32_t cur = (uint32_t)s->start_offset;

	uint32_t coso_freq_off = 0, coso_env_off = 0, coso_tracks_off = 0;
	uint32_t coso_pos_off = 0, coso_subsongs_off = 0, coso_sampleinfo_off = 0;
	int32_t coso_sampledata_off = 0;

	if(s->coso_mode_enabled) {
		// COSO header: "COSO" + 7x uint32 offsets.
		if(!hip_bounded(s, cur, 32)) {
			return 0;
		}
		cur += 4;                                      // skip "COSO"
		coso_freq_off = hip_read_b_u32(s->module_data + cur);
		cur += 4;
		coso_env_off = hip_read_b_u32(s->module_data + cur);
		cur += 4;
		coso_tracks_off = hip_read_b_u32(s->module_data + cur);
		cur += 4;
		coso_pos_off = hip_read_b_u32(s->module_data + cur);
		cur += 4;
		coso_subsongs_off = hip_read_b_u32(s->module_data + cur);
		cur += 4;
		coso_sampleinfo_off = hip_read_b_u32(s->module_data + cur);
		cur += 4;
		coso_sampledata_off = hip_read_b_i32(s->module_data + cur);
		cur += 4;

		if((uint32_t)coso_sampledata_off == s->module_len) {
			coso_sampledata_off = -1;
		}
	}

	// Header: skip ID (4 bytes) + 5 uint16 fields (NumFreq..BytesPerTrack)
	// + skip 2 + NumberOfSubSongs + NumberOfSamples + skip 12.
	if(!hip_bounded(s, cur, 28)) {
		return 0;
	}
	cur += 4;                                          // skip "TFMX" or padding
	uint16_t number_of_frequencies = hip_read_b_u16(s->module_data + cur);
	cur += 2;
	uint16_t number_of_envelopes = hip_read_b_u16(s->module_data + cur);
	cur += 2;
	uint16_t number_of_tracks = hip_read_b_u16(s->module_data + cur);
	cur += 2;
	uint16_t number_of_positions = hip_read_b_u16(s->module_data + cur);
	cur += 2;
	uint16_t bytes_per_track = hip_read_b_u16(s->module_data + cur);
	cur += 2;
	cur += 2;                                          // skip
	uint16_t number_of_sub_songs = hip_read_b_u16(s->module_data + cur);
	cur += 2;
	uint16_t number_of_samples = hip_read_b_u16(s->module_data + cur);
	cur += 2;
	cur += 12;                                         // skip

	if(!hip_load_frequencies(s, (uint32_t)number_of_frequencies + 1u, &cur, coso_freq_off, coso_env_off)) {
		return 0;
	}
	if(!hip_load_envelopes(s, (uint32_t)number_of_envelopes + 1u, &cur, coso_env_off, coso_tracks_off)) {
		return 0;
	}
	if(!hip_load_tracks(s, (uint32_t)number_of_tracks + 1u, bytes_per_track, &cur, coso_tracks_off, coso_pos_off)) {
		return 0;
	}
	if(!hip_load_position_list(s, (uint32_t)number_of_positions + 1u, &cur, coso_pos_off)) {
		return 0;
	}
	if(!hip_load_song_info(s, (uint32_t)number_of_sub_songs, &cur, coso_subsongs_off)) {
		return 0;
	}

	uint32_t *sample_offsets = hip_load_sample_info(s, (uint32_t)number_of_samples, &cur, coso_sampleinfo_off);
	if(!sample_offsets) {
		return 0;
	}
	int32_t ok = hip_load_sample_data(s, sample_offsets, cur, (uint32_t)coso_sampledata_off);
	free(sample_offsets);
	if(!ok) {
		return 0;
	}

	if(s->coso_mode_enabled) {
		hip_enable_coso_features(s, coso_freq_off, coso_env_off, coso_tracks_off, coso_pos_off, coso_subsongs_off, coso_sampleinfo_off, coso_sampledata_off);
	}
	return 1;
}

// [=]===^=[ hip_set_voice_envelope_table ]=======================================================[=]
// Releases any private extended copy attached to a voice and points it at a
// shared envelope table.
static void hip_set_voice_envelope_table(struct hip_voice_info *vi, uint8_t *data, uint32_t length) {
	if(vi->envelope_table_alloc) {
		free(vi->envelope_table_alloc);
		vi->envelope_table_alloc = 0;
	}
	vi->envelope_table = data;
	vi->envelope_table_length = length;
}

// [=]===^=[ hip_set_tempo ]======================================================================[=]
// Hippel 7V tempo formula: 3546895 / ((tempo+256)*14318) Hz, scaled to ticks/sec.
static void hip_set_tempo(struct hippel_state *s, int8_t tempo) {
	double freq_hz = (3546895.0 / ((double)(tempo + 256) * 14318.0)) * 50.0;
	if(freq_hz < 1.0) {
		freq_hz = 50.0;
	}
	s->playing_frequency = (float)freq_hz;
	s->paula.samples_per_tick = (int32_t)((double)s->paula.sample_rate / freq_hz);
	if(s->paula.samples_per_tick < 1) {
		s->paula.samples_per_tick = 1;
	}
}

// [=]===^=[ hip_initialize_sound ]===============================================================[=]
static void hip_initialize_sound(struct hippel_state *s, int32_t sub_song) {
	s->current_song = sub_song;
	struct hip_song_info *si = &s->song_info_list[sub_song];

	s->playing_info.speed_counter = (s->speed_init_value < 0) ? si->start_speed : (uint16_t)s->speed_init_value;
	s->playing_info.speed = si->start_speed;
	s->playing_info.random = 0;

	for(int32_t i = 0; i < HIP_MAX_CHANNELS; ++i) {
		struct hip_voice_info *vi = &s->voices[i];
		if(vi->envelope_table_alloc) {
			free(vi->envelope_table_alloc);
		}
		memset(vi, 0, sizeof(*vi));
	}

	for(int32_t i = 0; i < s->number_of_channels; ++i) {
		struct hip_voice_info *vi = &s->voices[i];
		struct hip_single_position_info *pi = &s->position_list[si->start_position].info[i];

		vi->envelope_table = hip_default_command_table;
		vi->envelope_table_length = sizeof(hip_default_command_table);
		vi->envelope_position = 0;
		vi->original_envelope_number = 0;
		vi->current_envelope_number = 0;

		vi->frequency_table = hip_default_command_table;
		vi->frequency_table_length = sizeof(hip_default_command_table);
		vi->frequency_position = 0;
		vi->original_frequency_number = 0;
		vi->current_frequency_number = 0;

		vi->next_position = (uint32_t)si->start_position + ((s->module_type == HIP_TYPE_HIPPEL_7V) ? 0u : 1u);

		vi->current_track_number = pi->track;
		vi->track = s->tracks[pi->track].data;
		vi->track_length = s->tracks[pi->track].length;
		vi->track_position = (s->module_type == HIP_TYPE_HIPPEL_7V) ? vi->track_length : 0;
		vi->track_transpose = pi->note_transpose;
		vi->envelope_transpose = pi->envelope_transpose;

		vi->sample = 0xff;
		vi->envelope_counter = 1;
		vi->envelope_speed = 1;
		vi->volume_fade = 100;
	}

	if(s->module_type == HIP_TYPE_HIPPEL_7V) {
		hip_set_tempo(s, s->position_list[0].info[0].envelope_transpose);
	}
	// Reset the visited-position bitmap, then mark the starting position as
	// visited so a song that jumps back to start is detected.
	if(s->position_visited && s->position_list_count > 0) {
		memset(s->position_visited, 0, s->position_list_count);
		if((uint32_t)si->start_position < s->position_list_count) {
			s->position_visited[(uint32_t)si->start_position] = 1;
		}
	}
	s->end_reached = 0;
}

// [=]===^=[ hip_setup_envelope ]=================================================================[=]
static void hip_setup_envelope(struct hippel_state *s, int32_t voice) {
	struct hip_voice_info *vi = &s->voices[voice];

	if(s->enable_mute) {
		paula_mute(&s->paula, voice);
	}

	uint8_t val = (uint8_t)((vi->current_info & 0x1f) + (uint32_t)vi->envelope_transpose);
	if(val >= s->envelopes_count) {
		val = 0;
	}

	struct hip_envelope *env = &s->envelopes[val];

	vi->envelope_counter = env->envelope_speed;
	vi->envelope_speed = env->envelope_speed;

	vi->vibrato_flag = 0x40;
	vi->vibrato_speed = env->vibrato_speed;
	vi->vibrato_depth = env->vibrato_depth;
	vi->vibrato_delta = env->vibrato_depth;
	vi->vibrato_delay = env->vibrato_delay;

	hip_set_voice_envelope_table(vi, env->envelope_table, env->envelope_table_length);
	vi->envelope_position = 0;
	vi->original_envelope_number = val;
	vi->current_envelope_number = val;

	uint8_t frequency_number = env->frequency_number;
	if(!s->enable_frequency_reset_check || (frequency_number != 0x80)) {
		if(s->enable_frequency_previous_info && ((vi->current_info & 0x40) != 0)) {
			frequency_number = vi->previous_info;
		}

		if(frequency_number < s->frequencies_count) {
			vi->frequency_table = s->frequencies[frequency_number].data;
			vi->frequency_table_length = s->frequencies[frequency_number].length;
		} else {
			vi->frequency_table = hip_default_command_table;
			vi->frequency_table_length = sizeof(hip_default_command_table);
		}
		if(!vi->frequency_table) {
			vi->frequency_table = hip_default_command_table;
			vi->frequency_table_length = sizeof(hip_default_command_table);
		}
		vi->frequency_position = 0;
		vi->original_frequency_number = frequency_number;
		vi->current_frequency_number = frequency_number;
		vi->tick = 0;

		if(s->reset_sustain) {
			vi->envelope_sustain = 0;
		}
	}
}

// [=]===^=[ hip_parse_position_command ]=========================================================[=]
// 7-voice position command (high nibble of pi->command).
static void hip_parse_position_command(struct hippel_state *s, int32_t voice, struct hip_single_position_info **pi_ref) {
	struct hip_voice_info *vi = &s->voices[voice];
	struct hip_single_position_info *pi = *pi_ref;

	switch(pi->command & 0xf0) {
		case 0x80: {
			// Song stopped
			s->end_reached = 1;
			vi->next_position = s->song_info_list[s->current_song].start_position;
			pi = &s->position_list[vi->next_position].info[voice];
			vi->track_transpose = pi->note_transpose;
			vi->envelope_transpose = pi->envelope_transpose;
			*pi_ref = pi;
			break;
		}

		case 0xd0: {
			struct hip_single_position_info *get_from;
			if(voice == 6) {
				get_from = &s->position_list[vi->next_position + 1].info[0];
			} else {
				get_from = &s->position_list[vi->next_position].info[voice + 1];
			}
			hip_set_tempo(s, (int8_t)get_from->command);
			break;
		}

		case 0xe0: {
			s->playing_info.speed = (uint16_t)(pi->command & 0x0f);
			break;
		}

		case 0xf0: {
			uint8_t cmd_arg = (uint8_t)(pi->command & 0x0f);
			vi->volume_fade = (uint8_t)((cmd_arg == 0) ? 64 : (15 - cmd_arg + 1) * 6);
			break;
		}
	}
}

// [=]===^=[ hip_read_next_row ]==================================================================[=]
static void hip_read_next_row(struct hippel_state *s, int32_t voice) {
	struct hip_voice_info *vi = &s->voices[voice];
	struct hip_song_info *si = &s->song_info_list[s->current_song];

	if((vi->track_position == vi->track_length) || ((vi->track[vi->track_position] & 0x7f) == 1)) {
		if(vi->next_position == si->last_position) {
			vi->next_position = si->start_position;
			s->end_reached = 1;
		}

		struct hip_single_position_info *pi = &s->position_list[vi->next_position].info[voice];

		vi->track_transpose = pi->note_transpose;
		vi->envelope_transpose = pi->envelope_transpose;

		if(pi->envelope_transpose == -128) {
			s->end_reached = 1;
			vi->next_position = si->start_position;
			pi = &s->position_list[vi->next_position].info[voice];
			vi->track_transpose = pi->note_transpose;
			vi->envelope_transpose = pi->envelope_transpose;
		}

		vi->current_track_number = pi->track;
		vi->track = s->tracks[pi->track].data;
		vi->track_length = s->tracks[pi->track].length;
		vi->track_position = 0;

		if(voice == 0 && s->position_visited && vi->next_position < s->position_list_count) {
			if(s->position_visited[vi->next_position]) {
				s->end_reached = 1;
			}
			s->position_visited[vi->next_position] = 1;
		}
		++vi->next_position;
	}

	if(vi->track_length == 0) {
		return;
	}

	uint8_t val = vi->track[vi->track_position];
	uint8_t note = (uint8_t)(val & 0x7f);

	if(note != 0) {
		vi->porta_delta = 0;
		vi->current_note = note;

		uint32_t prev_idx = (vi->track_position == 0) ? (vi->track_length - 1) : (vi->track_position - 1);
		vi->previous_info = vi->track[prev_idx];
		vi->current_info = (vi->track_position + 1 < vi->track_length) ? vi->track[vi->track_position + 1] : 0;

		if(val < 128) {
			hip_setup_envelope(s, voice);
		}
	}

	vi->track_position += 2;
}

// [=]===^=[ hip_read_next_row_7voices ]==========================================================[=]
static void hip_read_next_row_7voices(struct hippel_state *s, int32_t voice) {
	struct hip_voice_info *vi = &s->voices[voice];
	struct hip_song_info *si = &s->song_info_list[s->current_song];

	if((vi->track_position == vi->track_length) || ((vi->track_length > 0) && ((vi->track[vi->track_position] & 0x7f) == 1))) {
		if(vi->next_position == si->last_position) {
			vi->next_position = si->start_position;
			s->end_reached = 1;
		}

		struct hip_single_position_info *pi = &s->position_list[vi->next_position].info[voice];

		vi->track_transpose = pi->note_transpose;
		vi->envelope_transpose = pi->envelope_transpose;

		hip_parse_position_command(s, voice, &pi);

		vi->current_track_number = pi->track;
		vi->track = s->tracks[pi->track].data;
		vi->track_length = s->tracks[pi->track].length;
		vi->track_position = 0;

		if(voice == 0 && s->position_visited && vi->next_position < s->position_list_count) {
			if(s->position_visited[vi->next_position]) {
				s->end_reached = 1;
			}
			s->position_visited[vi->next_position] = 1;
		}
		++vi->next_position;
	}

	if(vi->track_length == 0) {
		return;
	}

	uint8_t val = vi->track[vi->track_position];
	uint8_t note = (uint8_t)(val & 0x7f);

	if(note != 0) {
		vi->porta_delta = 0;
		vi->current_note = note;

		uint32_t prev_idx = (vi->track_position == 0) ? (vi->track_length - 1) : (vi->track_position - 1);
		vi->previous_info = vi->track[prev_idx];
		vi->current_info = (vi->track_position + 1 < vi->track_length) ? vi->track[vi->track_position + 1] : 0;

		if(val < 128) {
			hip_setup_envelope(s, voice);
		}
	}

	vi->track_position += 2;
}

// [=]===^=[ hip_read_next_row_coso ]=============================================================[=]
static void hip_read_next_row_coso(struct hippel_state *s, int32_t voice) {
	struct hip_voice_info *vi = &s->voices[voice];
	struct hip_song_info *si = &s->song_info_list[s->current_song];

	--vi->coso_counter;
	if(vi->coso_counter >= 0) {
		return;
	}

	vi->coso_counter = vi->coso_speed;

	int32_t one_more = 1;
	while(one_more) {
		one_more = 0;

		uint8_t val;
		if((s->module_type == HIP_TYPE_HIPPEL_7V) && (vi->track_position == vi->track_length)) {
			val = 0xff;
		} else if(vi->track_length == 0) {
			val = 0xff;
		} else {
			val = vi->track[vi->track_position];
		}

		switch(val) {
			case 0xff: {
				if(vi->next_position == si->last_position) {
					vi->next_position = si->start_position;
					s->end_reached = 1;
				}

				struct hip_single_position_info *pi = &s->position_list[vi->next_position].info[voice];

				vi->track_transpose = pi->note_transpose;
				val = (uint8_t)pi->envelope_transpose;

				if(s->enable_position_effect && (val > 127)) {
					uint8_t val2 = (uint8_t)((val >> 4) & 15);
					val &= 15;

					if(val2 == 15) {
						uint8_t newfade;
						if(val != 0) {
							newfade = (uint8_t)((15 - val + 1) * 6);
						} else {
							newfade = 100;
						}
						vi->volume_fade = newfade;
					} else if(val2 == 8) {
						s->end_reached = 1;
						vi->next_position = si->start_position;
						pi = &s->position_list[vi->next_position].info[voice];
						vi->track_transpose = pi->note_transpose;
						vi->envelope_transpose = pi->envelope_transpose;
					} else if(val2 == 14) {
						s->playing_info.speed = (uint16_t)(val & 15);
					}
				} else {
					vi->envelope_transpose = (int8_t)val;
				}

				if(s->module_type == HIP_TYPE_HIPPEL_7V) {
					hip_parse_position_command(s, voice, &pi);
				}

				vi->current_track_number = pi->track;
				vi->track = s->tracks[pi->track].data;
				vi->track_length = s->tracks[pi->track].length;
				vi->track_position = 0;

				if(voice == 0 && s->position_visited && vi->next_position < s->position_list_count) {
					if(s->position_visited[vi->next_position]) {
						s->end_reached = 1;
					}
					s->position_visited[vi->next_position] = 1;
				}
				++vi->next_position;
				one_more = 1;
				break;
			}

			case 0xfe: {
				if((vi->track_position + 2) > vi->track_length) {
					return;
				}
				vi->coso_speed = (int8_t)vi->track[vi->track_position + 1];
				vi->coso_counter = vi->coso_speed;
				vi->track_position += 2;
				one_more = 1;
				break;
			}

			case 0xfd: {
				if((vi->track_position + 2) > vi->track_length) {
					return;
				}
				vi->coso_speed = (int8_t)vi->track[vi->track_position + 1];
				vi->coso_counter = vi->coso_speed;
				vi->track_position += 2;
				return;
			}

			default: {
				vi->current_note = val;
				if((vi->track_position + 1) >= vi->track_length) {
					vi->current_info = 0;
				} else {
					vi->current_info = vi->track[vi->track_position + 1];
				}

				if((vi->current_info & 0xe0) != 0) {
					if((vi->track_position + 2) < vi->track_length) {
						vi->previous_info = vi->track[vi->track_position + 2];
					}
					vi->track_position += 3;
				} else {
					vi->track_position += 2;
				}

				vi->porta_delta = 0;

				if(val < 128) {
					if(s->enable_mute) {
						paula_mute(&s->paula, voice);
					}

					uint8_t val_e = (uint8_t)((vi->current_info & 0x1f) + (uint32_t)vi->envelope_transpose);
					if(val_e >= s->envelopes_count) {
						val_e = 0;
					}

					struct hip_envelope *env = &s->envelopes[val_e];

					vi->envelope_counter = env->envelope_speed;
					vi->envelope_speed = env->envelope_speed;
					vi->envelope_sustain = 0;

					vi->vibrato_flag = 0x40;
					vi->vibrato_speed = env->vibrato_speed;
					vi->vibrato_depth = env->vibrato_depth;
					vi->vibrato_delta = env->vibrato_depth;
					vi->vibrato_delay = env->vibrato_delay;

					hip_set_voice_envelope_table(vi, env->envelope_table, env->envelope_table_length);
					vi->envelope_position = 0;
					vi->original_envelope_number = val_e;
					vi->current_envelope_number = val_e;

					uint8_t frequency_number = env->frequency_number;
					if(!s->enable_frequency_reset_check || (frequency_number != 0x80)) {
						if((vi->current_info & 0x40) != 0) {
							frequency_number = vi->previous_info;
						}

						if(frequency_number < s->frequencies_count) {
							vi->frequency_table = s->frequencies[frequency_number].data;
							vi->frequency_table_length = s->frequencies[frequency_number].length;
						} else {
							vi->frequency_table = hip_default_command_table;
							vi->frequency_table_length = sizeof(hip_default_command_table);
						}
						if(!vi->frequency_table) {
							vi->frequency_table = hip_default_command_table;
							vi->frequency_table_length = sizeof(hip_default_command_table);
						}
						vi->frequency_position = 0;
						vi->original_frequency_number = frequency_number;
						vi->current_frequency_number = frequency_number;
						vi->tick = 0;
					}
				}
				break;
			}
		}
	}
}

// [=]===^=[ hip_get_random_variation ]===========================================================[=]
static uint16_t hip_get_random_variation(struct hippel_state *s) {
	uint32_t v = (uint32_t)s->playing_info.random + 0x4793u;
	uint32_t rotated = ((v >> 6) | (v << (16 - 6))) & 0xffffu;
	uint16_t out = (uint16_t)(rotated ^ v);
	s->playing_info.random = out;
	return out;
}

// [=]===^=[ hip_do_vibrato_v1 ]==================================================================[=]
static uint16_t hip_do_vibrato_v1(struct hip_voice_info *vi, uint8_t note, uint16_t period) {
	uint8_t vibrato_flag = vi->vibrato_flag;

	if(vi->vibrato_delay == 0) {
		int32_t vibrato_value = (int32_t)note * 2;
		int32_t vibrato_depth = (int32_t)vi->vibrato_depth * 2;
		int32_t vibrato_delta = (int32_t)vi->vibrato_delta;

		if(((vibrato_flag & 0x80) == 0) || ((vibrato_flag & 0x01) == 0)) {
			if((vibrato_flag & 0x20) == 0) {
				vibrato_delta -= (int32_t)vi->vibrato_speed;
				if(vibrato_delta < 0) {
					vibrato_flag |= 0x20;
					vibrato_delta = 0;
				}
			} else {
				vibrato_delta += (int32_t)vi->vibrato_speed;
				if(vibrato_delta >= vibrato_depth) {
					vibrato_flag = (uint8_t)(vibrato_flag & ~0x20);
					vibrato_delta = vibrato_depth;
				}
			}
			vi->vibrato_delta = (uint8_t)vibrato_delta;
		}

		vibrato_delta -= (int32_t)vi->vibrato_depth;
		if(vibrato_delta != 0) {
			vibrato_value += 160;
			while(vibrato_value < 256) {
				vibrato_delta += vibrato_delta;
				vibrato_value += 24;
			}
			period = (uint16_t)(period + (uint16_t)vibrato_delta);
		}
	} else {
		--vi->vibrato_delay;
	}

	vibrato_flag ^= 0x01;
	vi->vibrato_flag = vibrato_flag;
	return period;
}

// [=]===^=[ hip_do_vibrato_v2 ]==================================================================[=]
static uint16_t hip_do_vibrato_v2(struct hip_voice_info *vi, uint16_t period) {
	if(vi->vibrato_delay == 0) {
		uint8_t vibrato_flag = vi->vibrato_flag;
		int32_t vibrato_depth = (int32_t)vi->vibrato_depth * 2;
		int32_t vibrato_speed = (int32_t)vi->vibrato_speed;
		int32_t vibrato_delta = (int32_t)vi->vibrato_delta;

		if(vibrato_speed >= 128) {
			vibrato_speed &= 0x7f;
			vibrato_flag ^= 0x01;
		}

		if((vibrato_flag & 0x01) == 0) {
			if((vibrato_flag & 0x20) == 0) {
				vibrato_delta -= vibrato_speed;
				if(vibrato_delta < 0) {
					vibrato_flag |= 0x20;
					vibrato_delta = 0;
				}
			} else {
				vibrato_delta += vibrato_speed;
				if(vibrato_delta > vibrato_depth) {
					vibrato_flag = (uint8_t)(vibrato_flag & ~0x20);
					vibrato_delta = vibrato_depth;
				}
			}
		}

		vi->vibrato_delta = (uint8_t)vibrato_delta;
		vi->vibrato_flag = vibrato_flag;
		period = (uint16_t)(period + (uint16_t)(vibrato_delta - (int32_t)vi->vibrato_depth));
	} else {
		--vi->vibrato_delay;
	}
	return period;
}

// [=]===^=[ hip_do_vibrato_v3 ]==================================================================[=]
static uint16_t hip_do_vibrato_v3(struct hip_voice_info *vi, uint16_t period) {
	if(vi->vibrato_delay == 0) {
		uint8_t vibrato_flag = vi->vibrato_flag;
		int32_t vibrato_depth = (int32_t)vi->vibrato_depth;
		int32_t vibrato_speed = (int32_t)vi->vibrato_speed;
		int32_t vibrato_delta = (int32_t)vi->vibrato_delta;

		if((vibrato_flag & 0x20) == 0) {
			vibrato_delta -= vibrato_speed;
			if(vibrato_delta < 0) {
				vibrato_flag |= 0x20;
				vibrato_delta = 0;
			}
		} else {
			vibrato_delta += vibrato_speed;
			if(vibrato_delta > vibrato_depth) {
				vibrato_flag = (uint8_t)(vibrato_flag & ~0x20);
				vibrato_delta = vibrato_depth;
			}
		}

		vi->vibrato_delta = (uint8_t)vibrato_delta;
		vi->vibrato_flag = vibrato_flag;

		int32_t shift = (((vibrato_delta - ((int32_t)vi->vibrato_depth >> 1)) * (int32_t)period) >> 10);
		period = (uint16_t)((int32_t)period + shift);
	} else {
		--vi->vibrato_delay;
	}
	return period;
}

// [=]===^=[ hip_do_portamento_v1 ]===============================================================[=]
static uint16_t hip_do_portamento_v1(struct hip_voice_info *vi, uint16_t period) {
	if((vi->current_info & 0x20) != 0) {
		int32_t val = (int32_t)(int8_t)vi->previous_info;
		if(val >= 0) {
			vi->porta_delta += (uint32_t)(val << 11);
			period = (uint16_t)(period - (uint16_t)(vi->porta_delta >> 16));
		} else {
			vi->porta_delta -= (uint32_t)(val << 11);
			period = (uint16_t)(period + (uint16_t)(vi->porta_delta >> 16));
		}
	}
	return period;
}

// [=]===^=[ hip_do_portamento_v2 ]===============================================================[=]
static uint16_t hip_do_portamento_v2(struct hip_voice_info *vi, uint16_t period) {
	if((vi->current_info & 0x20) != 0) {
		int32_t val = (int32_t)(int8_t)vi->previous_info;
		if(val >= 0) {
			vi->porta_delta = (uint32_t)((int32_t)vi->porta_delta + val);
			int32_t v = (int32_t)(vi->porta_delta * (uint32_t)period);
			period = (uint16_t)(period - (uint16_t)(v >> 10));
		} else {
			vi->porta_delta = (uint32_t)((int32_t)vi->porta_delta - val);
			int32_t v = (int32_t)(vi->porta_delta * (uint32_t)period);
			period = (uint16_t)(period + (uint16_t)(v >> 10));
		}
	}
	return period;
}

// [=]===^=[ hip_extend_envelope ]================================================================[=]
// Concatenate the next envelope (incl. its 5 header bytes) onto the voice's
// current envelope buffer to produce a private extended copy.
static int32_t hip_extend_envelope(struct hippel_state *s, struct hip_voice_info *vi) {
	if((uint32_t)(vi->current_envelope_number + 1) >= s->envelopes_count) {
		return 0;
	}
	struct hip_envelope *env = &s->envelopes[++vi->current_envelope_number];

	uint32_t newlen = 5 + env->envelope_table_length;
	uint8_t *buf = (uint8_t *)malloc(newlen);
	if(!buf) {
		return 0;
	}
	buf[0] = env->envelope_speed;
	buf[1] = env->frequency_number;
	buf[2] = env->vibrato_speed;
	buf[3] = env->vibrato_depth;
	buf[4] = env->vibrato_delay;
	if(env->envelope_table_length > 0) {
		memcpy(buf + 5, env->envelope_table, env->envelope_table_length);
	}

	if(vi->envelope_table_alloc) {
		free(vi->envelope_table_alloc);
	}
	vi->envelope_table_alloc = buf;
	vi->envelope_table = buf;
	vi->envelope_table_length = newlen;
	vi->envelope_position = 0;
	return 1;
}

// [=]===^=[ hip_parse_effects ]==================================================================[=]
static void hip_parse_effects(struct hippel_state *s, int32_t voice) {
	struct hip_voice_info *vi = &s->voices[voice];

	int32_t one_more_big_loop = 1;
	while(one_more_big_loop) {
		one_more_big_loop = 0;

		if(vi->tick == 0) {
			int32_t one_more = 1;
			while(one_more) {
				one_more = 0;

				if((uint32_t)vi->frequency_position == vi->frequency_table_length) {
					if((uint32_t)(vi->current_frequency_number + 1) < s->frequencies_count) {
						++vi->current_frequency_number;
						vi->frequency_table = s->frequencies[vi->current_frequency_number].data;
						vi->frequency_table_length = s->frequencies[vi->current_frequency_number].length;
					}
					if(!vi->frequency_table) {
						vi->frequency_table = hip_default_command_table;
						vi->frequency_table_length = sizeof(hip_default_command_table);
					}
					vi->frequency_position = 0;
				}

				if(vi->frequency_table_length == 0) {
					return;
				}

				uint8_t val = vi->frequency_table[vi->frequency_position];

				if(val == 0xe1) {
					return;
				}

				if(val == 0xe0) {
					if(((uint32_t)vi->frequency_position + 1) >= vi->frequency_table_length) {
						return;
					}
					vi->frequency_position = vi->frequency_table[vi->frequency_position + 1] & 0x3f;

					if((uint32_t)vi->original_frequency_number < s->frequencies_count) {
						vi->frequency_table = s->frequencies[vi->original_frequency_number].data;
						vi->frequency_table_length = s->frequencies[vi->original_frequency_number].length;
					}
					vi->current_frequency_number = vi->original_frequency_number;
					if(!vi->frequency_table) {
						vi->frequency_table = hip_default_command_table;
						vi->frequency_table_length = sizeof(hip_default_command_table);
					}
					if((uint32_t)vi->frequency_position >= vi->frequency_table_length) {
						return;
					}
					val = vi->frequency_table[vi->frequency_position];
				}

				if(s->convert_effects) {
					if(val == 0xe5) {
						val = 0xe2;
					} else if(val == 0xe6) {
						val = 0xe4;
					}
				}

				switch(val) {
					case 0xe2: {
						if(s->effects_enabled[2] != 0) {
							if(((uint32_t)vi->frequency_position + 1) < vi->frequency_table_length) {
								uint8_t sample_number = vi->frequency_table[vi->frequency_position + 1];
								if(sample_number < s->samples_count) {
									vi->volume_variation_depth = 0;
									vi->sample = 0xff;
									struct hip_sample *sm = &s->samples[sample_number];
									paula_play_sample(&s->paula, voice, sm->sample_data, sm->length);
									if(sm->loop_length > 2) {
										paula_set_loop(&s->paula, voice, sm->loop_start, sm->loop_length);
									} else {
										paula_set_loop(&s->paula, voice, 0, 0);
									}
									vi->envelope_position = 0;
									vi->envelope_counter = 1;
									vi->slide = 0;
								}
							}
							vi->frequency_position += 2;
							one_more = s->enable_effect_loop;
						}
						break;
					}

					case 0xe3: {
						if(s->effects_enabled[3] != 0) {
							if(((uint32_t)vi->frequency_position + 2) < vi->frequency_table_length) {
								vi->vibrato_speed = vi->frequency_table[vi->frequency_position + 1];
								vi->vibrato_depth = vi->frequency_table[vi->frequency_position + 2];
							}
							vi->frequency_position += 3;
							one_more = s->enable_effect_loop;
						}
						break;
					}

					case 0xe4: {
						if(s->effects_enabled[4] != 0) {
							if(((uint32_t)vi->frequency_position + 1) < vi->frequency_table_length) {
								uint8_t sample_number = vi->frequency_table[vi->frequency_position + 1];
								if(sample_number < s->samples_count) {
									struct hip_sample *sm = &s->samples[sample_number];
									uint32_t loop_len = (sm->loop_length > 2) ? sm->loop_length : sm->length;
									uint32_t loop_start = (sm->loop_length > 2) ? sm->loop_start : 0;
									paula_queue_sample(&s->paula, voice, sm->sample_data, loop_start, loop_len);
									if(sm->loop_length > 2) {
										paula_set_loop(&s->paula, voice, sm->loop_start, sm->loop_length);
									} else {
										paula_set_loop(&s->paula, voice, 0, sm->length);
									}
									vi->slide = 0;
								}
							}
							vi->frequency_position += 2;
							one_more = s->enable_effect_loop;
						}
						break;
					}

					case 0xe5: {
						if(((uint32_t)vi->frequency_position + 1) >= vi->frequency_table_length) {
							return;
						}
						uint8_t sample_number = vi->frequency_table[vi->frequency_position + 1];
						if(sample_number >= s->samples_count) {
							vi->frequency_position += 2;
							break;
						}
						struct hip_sample *sm = &s->samples[sample_number];

						if(s->effects_enabled[5] == 1) {
							if(((uint32_t)vi->frequency_position + 2) < vi->frequency_table_length) {
								uint32_t offset = (uint32_t)vi->frequency_table[vi->frequency_position + 2] * sm->loop_length;
								uint32_t play_len = (sm->loop_length > offset) ? (sm->loop_length - offset) : 0;
								if(play_len > 0) {
									hip_play_sample_at(&s->paula, voice, sm->sample_data, offset, play_len);
								} else {
									paula_play_sample(&s->paula, voice, sm->sample_data, sm->length);
								}
								if(sm->loop_length > 2) {
									paula_set_loop(&s->paula, voice, sm->loop_start, sm->loop_length);
								} else {
									paula_set_loop(&s->paula, voice, 0, 0);
								}
							}
							vi->frequency_position += 3;
						} else if(s->effects_enabled[5] == 2) {
							paula_mute(&s->paula, voice);
							vi->volume_variation_depth = 0;
							vi->slide_sample = sample_number;
							vi->slide_end_position = (int32_t)sm->length;

							if(((uint32_t)vi->frequency_position + 8) < vi->frequency_table_length) {
								uint16_t loop_position = (uint16_t)((vi->frequency_table[vi->frequency_position + 2] << 8) | vi->frequency_table[vi->frequency_position + 3]);
								if(loop_position == 0xffff) {
									loop_position = (uint16_t)(sm->length / 2);
								}
								vi->slide_loop_position = loop_position;
								vi->slide_length = (uint16_t)((vi->frequency_table[vi->frequency_position + 4] << 8) | vi->frequency_table[vi->frequency_position + 5]);
								vi->slide_delta = (int16_t)((vi->frequency_table[vi->frequency_position + 6] << 8) | vi->frequency_table[vi->frequency_position + 7]);
								vi->slide_speed = vi->frequency_table[vi->frequency_position + 8];
							}
							vi->slide_counter = 0;
							vi->slide_active = 0;
							vi->slide_done = 0;
							vi->slide = 1;

							vi->frequency_position += 9;
							one_more = 1;
						}

						vi->envelope_position = 0;
						vi->envelope_counter = 1;
						break;
					}

					case 0xe6: {
						if(s->effects_enabled[6] != 0) {
							if(((uint32_t)vi->frequency_position + 5) < vi->frequency_table_length) {
								vi->slide_length = (uint16_t)((vi->frequency_table[vi->frequency_position + 1] << 8) | vi->frequency_table[vi->frequency_position + 2]);
								vi->slide_delta = (int16_t)((vi->frequency_table[vi->frequency_position + 3] << 8) | vi->frequency_table[vi->frequency_position + 4]);
								vi->slide_speed = vi->frequency_table[vi->frequency_position + 5];
							}
							vi->slide_counter = 0;
							vi->slide_active = 0;
							vi->slide_done = 0;
							vi->frequency_position += 6;
							one_more = 1;
						}
						break;
					}

					case 0xe7: {
						if(s->effects_enabled[7] == 1) {
							if(((uint32_t)vi->frequency_position + 1) < vi->frequency_table_length) {
								uint8_t frequency_number = vi->frequency_table[vi->frequency_position + 1];
								if(frequency_number < s->frequencies_count) {
									vi->frequency_table = s->frequencies[frequency_number].data;
									vi->frequency_table_length = s->frequencies[frequency_number].length;
									if(!vi->frequency_table) {
										vi->frequency_table = hip_default_command_table;
										vi->frequency_table_length = sizeof(hip_default_command_table);
									}
									vi->frequency_position = 0;
									vi->original_frequency_number = frequency_number;
									vi->current_frequency_number = frequency_number;
								}
							}
							one_more = 1;
						} else if(s->effects_enabled[7] == 2) {
							if(((uint32_t)vi->frequency_position + 1) < vi->frequency_table_length) {
								uint8_t sample_number = vi->frequency_table[vi->frequency_position + 1];
								if((sample_number < s->samples_count) && (vi->sample != sample_number)) {
									vi->sample = sample_number;
									struct hip_sample *sm = &s->samples[sample_number];
									paula_play_sample(&s->paula, voice, sm->sample_data, sm->length);
									if(sm->loop_length > 2) {
										paula_set_loop(&s->paula, voice, sm->loop_start, sm->loop_length);
									} else {
										paula_set_loop(&s->paula, voice, 0, 0);
									}
								}
							}
							vi->envelope_position = 0;
							vi->envelope_counter = 1;
							vi->slide = 0;
							vi->frequency_position += 2;
							one_more = s->enable_effect_loop;
						}
						break;
					}

					case 0xe8: {
						if(s->effects_enabled[8] != 0) {
							if(((uint32_t)vi->frequency_position + 1) < vi->frequency_table_length) {
								vi->tick = vi->frequency_table[vi->frequency_position + 1];
							}
							vi->frequency_position += 2;
							one_more_big_loop = 1;
						}
						break;
					}

					case 0xe9: {
						if(s->effects_enabled[9] != 0) {
							if(((uint32_t)vi->frequency_position + 2) < vi->frequency_table_length) {
								uint8_t sample_number = vi->frequency_table[vi->frequency_position + 1];
								uint8_t sub_sample_number = vi->frequency_table[vi->frequency_position + 2];

								if(sample_number < s->samples_count) {
									vi->volume_variation_depth = 0;
									vi->sample = 0xff;
									struct hip_sample *sm = &s->samples[sample_number];
									int8_t *sd = sm->sample_data;
									uint32_t sd_len = sm->length;

									if((sd != 0) && (sd_len >= 8)
										&& (s->skip_id_check
											|| ((sd[0] == 'S') && (sd[1] == 'S') && (sd[2] == 'M') && (sd[3] == 'P')))) {
										int32_t number_of_sub_samples = ((int32_t)(uint8_t)sd[4] << 8) | (int32_t)(uint8_t)sd[5];
										int32_t val2 = ((int32_t)(uint8_t)sd[6] << 8) | (int32_t)(uint8_t)sd[7];
										int32_t sub_sample_start_offset = 8 + number_of_sub_samples * 24 + val2 * 4;

										int32_t sub_sample_header_index = 8 + (int32_t)sub_sample_number * 24;
										if((uint32_t)(sub_sample_header_index + 8) <= sd_len) {
											int32_t start =
												  ((int32_t)(uint8_t)sd[sub_sample_header_index] << 24)
												| ((int32_t)(uint8_t)sd[sub_sample_header_index + 1] << 16)
												| ((int32_t)(uint8_t)sd[sub_sample_header_index + 2] << 8)
												|  (int32_t)(uint8_t)sd[sub_sample_header_index + 3];
											int32_t end =
												  ((int32_t)(uint8_t)sd[sub_sample_header_index + 4] << 24)
												| ((int32_t)(uint8_t)sd[sub_sample_header_index + 5] << 16)
												| ((int32_t)(uint8_t)sd[sub_sample_header_index + 6] << 8)
												|  (int32_t)(uint8_t)sd[sub_sample_header_index + 7];

											if(s->e9_ands) {
												start &= -2;
												end &= -2;
											}

											uint32_t sample_start_offset = (uint32_t)(sub_sample_start_offset + start);

											if(s->e9_fix_sample && ((sample_start_offset + 1) < sd_len)) {
												sd[sample_start_offset + 1] = sd[sample_start_offset];
											}

											int32_t length = end - start;
											if((sample_start_offset + (uint32_t)length) > sd_len) {
												length = (int32_t)(sd_len - sample_start_offset);
											}
											if(length > 0) {
												hip_play_sample_at(&s->paula, voice, sd, sample_start_offset, (uint32_t)length);
												paula_set_loop(&s->paula, voice, sample_start_offset, (uint32_t)length);
											}

											vi->envelope_position = 0;
											vi->envelope_counter = 1;
											vi->slide = 0;
										}
									}
								}
							}
							vi->frequency_position += 3;
							one_more = s->enable_effect_loop;
						}
						break;
					}

					case 0xea: {
						if(s->effects_enabled[10] != 0) {
							if(((uint32_t)vi->frequency_position + 1) < vi->frequency_table_length) {
								vi->volume_variation_depth = vi->frequency_table[vi->frequency_position + 1];
							}
							vi->volume_variation = 0;
							vi->frequency_position += 2;
							one_more = 1;
						}
						break;
					}

					default: {
						break;
					}
				}

				if(!one_more && !one_more_big_loop) {
					if((uint32_t)vi->frequency_position < vi->frequency_table_length) {
						vi->transpose = vi->frequency_table[vi->frequency_position];
					}
					++vi->frequency_position;
				}
			}
		} else {
			--vi->tick;
		}
	}
}

// [=]===^=[ hip_run_effects ]====================================================================[=]
static void hip_run_effects(struct hippel_state *s, int32_t voice) {
	struct hip_voice_info *vi = &s->voices[voice];

	// Slide
	if(vi->slide && !vi->slide_done) {
		--vi->slide_counter;
		if(vi->slide_counter < 0) {
			vi->slide_counter = (int8_t)vi->slide_speed;

			if(vi->slide_active) {
				int32_t slide_value = (int32_t)vi->slide_loop_position + (int32_t)vi->slide_delta;
				if(slide_value < 0) {
					vi->slide_done = 1;
					slide_value = vi->slide_loop_position;
				} else {
					int32_t slide_position = slide_value * 2 + (int32_t)vi->slide_length * 2;
					if(slide_position > vi->slide_end_position) {
						vi->slide_done = 1;
						slide_value = vi->slide_loop_position;
					}
				}
				vi->slide_loop_position = (uint16_t)slide_value;
			} else {
				vi->slide_active = 1;
			}

			if(vi->slide_sample < s->samples_count) {
				struct hip_sample *sm = &s->samples[vi->slide_sample];
				uint32_t start = (uint32_t)vi->slide_loop_position * 2u;
				uint32_t len = (uint32_t)vi->slide_length * 2u;
				if((start + len) > sm->length) {
					len = (start <= sm->length) ? (sm->length - start) : 0;
				}
				if(len == 0) {
					len = (start <= sm->length) ? (sm->length - start) : 0;
				}
				if(sm->sample_data && (len > 0)) {
					paula_queue_sample(&s->paula, voice, sm->sample_data, start, len);
					paula_set_loop(&s->paula, voice, start, len);
				}
			}
		}
	}

	// Envelope
	int32_t one_more_big_loop = 1;
	while(one_more_big_loop) {
		one_more_big_loop = 0;

		if(vi->envelope_sustain == 0) {
			--vi->envelope_counter;
			if(vi->envelope_counter == 0) {
				vi->envelope_counter = vi->envelope_speed;

				int32_t one_more = 1;
				while(one_more) {
					one_more = 0;

					if((uint32_t)vi->envelope_position == vi->envelope_table_length) {
						if(!hip_extend_envelope(s, vi)) {
							return;
						}
					}

					if(vi->envelope_table_length == 0) {
						return;
					}

					uint8_t val = vi->envelope_table[vi->envelope_position];

					if(val == 0xe0) {
						if(((uint32_t)vi->envelope_position + 1) >= vi->envelope_table_length) {
							return;
						}
						vi->envelope_position = (vi->envelope_table[vi->envelope_position + 1] & 0x3f) - 5;
						if((uint32_t)vi->original_envelope_number < s->envelopes_count) {
							hip_set_voice_envelope_table(vi,
								s->envelopes[vi->original_envelope_number].envelope_table,
								s->envelopes[vi->original_envelope_number].envelope_table_length);
						}
						vi->current_envelope_number = vi->original_envelope_number;
						one_more = 1;
					} else if(val == 0xe1) {
						// noop
					} else if(val == 0xe8) {
						if(((uint32_t)vi->envelope_position + 1) < vi->envelope_table_length) {
							vi->envelope_sustain = vi->envelope_table[vi->envelope_position + 1];
						}
						vi->envelope_position += 2;
						one_more_big_loop = 1;
					} else {
						vi->volume = val;
						++vi->envelope_position;
					}
				}
			}
		} else {
			--vi->envelope_sustain;
		}
	}

	// Period
	uint8_t note = vi->transpose;
	if(note < 128) {
		note = (uint8_t)(note + vi->current_note + (uint8_t)vi->track_transpose);
	}
	note &= 0x7f;
	uint32_t period_idx = note;
	if(period_idx >= s->period_table_length) {
		period_idx = s->period_table_length - 1;
	}
	uint16_t period = s->period_table[period_idx];

	if(s->vibrato_version == 1) {
		period = hip_do_vibrato_v1(vi, note, period);
	} else if(s->vibrato_version == 2) {
		period = hip_do_vibrato_v2(vi, period);
	} else {
		period = hip_do_vibrato_v3(vi, period);
	}

	if(s->portamento_version == 1) {
		period = hip_do_portamento_v1(vi, period);
	} else {
		period = hip_do_portamento_v2(vi, period);
	}

	paula_set_period(&s->paula, voice, period);

	uint16_t out_volume;
	if(s->enable_volume_fade) {
		uint8_t volume = vi->volume;
		uint8_t vol_fade = vi->volume_fade;
		uint8_t vol_depth = vi->volume_variation_depth;

		if(vol_depth != 0) {
			uint8_t vol_variation = vi->volume_variation;
			if(vol_variation == 0) {
				vi->volume_variation_depth = 0;
				int32_t rnd = (int32_t)hip_get_random_variation(s) & 0xff;
				vi->volume_variation = (uint8_t)((int32_t)vol_depth * rnd / 255);
				vol_variation = vi->volume_variation;
			}
			vol_fade = (uint8_t)(vol_fade - vol_variation);
		}
		if(vol_fade > 127) {
			vol_fade = 0;
		}
		out_volume = (uint16_t)((uint32_t)vol_fade * (uint32_t)volume / 100u);
	} else {
		out_volume = vi->volume;
	}

	paula_set_volume(&s->paula, voice, out_volume);
}

// [=]===^=[ hip_do_effects ]=====================================================================[=]
static void hip_do_effects(struct hippel_state *s, int32_t voice) {
	hip_parse_effects(s, voice);
	hip_run_effects(s, voice);

	uint8_t pan = (s->number_of_channels == 7) ? hip_pan7[voice] : hip_pan4[voice];
	s->paula.ch[voice].pan = pan;
}

// [=]===^=[ hip_play ]===========================================================================[=]
static void hip_play(struct hippel_state *s) {
	--s->playing_info.speed_counter;

	int32_t max_voice = (s->number_of_channels < PAULA_NUM_CHANNELS) ? s->number_of_channels : PAULA_NUM_CHANNELS;

	if(s->playing_info.speed_counter == 0) {
		s->playing_info.speed_counter = s->playing_info.speed;

		if(s->coso_mode_enabled) {
			for(int32_t i = 0; i < max_voice; ++i) {
				hip_read_next_row_coso(s, i);
			}
		} else if(s->module_type == HIP_TYPE_HIPPEL_7V) {
			for(int32_t i = 0; i < max_voice; ++i) {
				hip_read_next_row_7voices(s, i);
			}
		} else {
			for(int32_t i = 0; i < max_voice; ++i) {
				hip_read_next_row(s, i);
			}
		}
	}

	for(int32_t i = 0; i < max_voice; ++i) {
		hip_do_effects(s, i);
	}

	if(s->end_reached) {
		s->end_reached = 0;
	}
}

// [=]===^=[ hip_cleanup ]========================================================================[=]
static void hip_cleanup(struct hippel_state *s) {
	if(!s) {
		return;
	}
	if(s->frequencies) {
		for(uint32_t i = 0; i < s->frequencies_count; ++i) {
			free(s->frequencies[i].data);
		}
		free(s->frequencies);
		s->frequencies = 0;
	}
	if(s->envelopes) {
		for(uint32_t i = 0; i < s->envelopes_count; ++i) {
			free(s->envelopes[i].envelope_table);
		}
		free(s->envelopes);
		s->envelopes = 0;
	}
	if(s->tracks) {
		for(uint32_t i = 0; i < s->tracks_count; ++i) {
			free(s->tracks[i].data);
		}
		free(s->tracks);
		s->tracks = 0;
	}
	if(s->position_list) {
		free(s->position_list);
		s->position_list = 0;
	}
	if(s->position_visited) {
		free(s->position_visited);
		s->position_visited = 0;
	}
	if(s->song_info_list) {
		free(s->song_info_list);
		s->song_info_list = 0;
	}
	if(s->samples) {
		// sample_data points into module buffer (caller-owned). Do not free.
		free(s->samples);
		s->samples = 0;
	}
	for(int32_t i = 0; i < HIP_MAX_CHANNELS; ++i) {
		if(s->voices[i].envelope_table_alloc) {
			free(s->voices[i].envelope_table_alloc);
			s->voices[i].envelope_table_alloc = 0;
		}
	}
}

// [=]===^=[ hippel_init ]========================================================================[=]
static struct hippel_state *hippel_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || (len < 1024) || (sample_rate < 8000)) {
		return 0;
	}

	struct hippel_state *s = (struct hippel_state *)calloc(1, sizeof(struct hippel_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;
	s->module_type = HIP_TYPE_UNKNOWN;
	s->reset_sustain = 1;
	s->portamento_version = 1;
	s->period_table = hip_periods1;
	s->period_table_length = sizeof(hip_periods1) / sizeof(hip_periods1[0]);

	if(!hip_identify(s)) {
		free(s);
		return 0;
	}
	if(!hip_load(s)) {
		hip_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, HIP_TICK_HZ);
	hip_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ hippel_free ]========================================================================[=]
static void hippel_free(struct hippel_state *s) {
	if(!s) {
		return;
	}
	hip_cleanup(s);
	free(s);
}

// [=]===^=[ hippel_get_audio ]===================================================================[=]
static void hippel_get_audio(struct hippel_state *s, int16_t *output, int32_t frames) {
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
			hip_play(s);
		}
	}
}

// [=]===^=[ hippel_api_init ]====================================================================[=]
static void *hippel_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return hippel_init(data, len, sample_rate);
}

// [=]===^=[ hippel_api_free ]====================================================================[=]
static void hippel_api_free(void *state) {
	hippel_free((struct hippel_state *)state);
}

// [=]===^=[ hippel_api_get_audio ]===============================================================[=]
static void hippel_api_get_audio(void *state, int16_t *output, int32_t frames) {
	hippel_get_audio((struct hippel_state *)state, output, frames);
}

static const char *hippel_extensions[] = { "hip", "hipc", "hip7", 0 };

static struct player_api hippel_api = {
	"Jochen Hippel",
	hippel_extensions,
	hippel_api_init,
	hippel_api_free,
	hippel_api_get_audio,
	0,
};
