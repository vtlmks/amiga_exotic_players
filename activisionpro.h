// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Activision Pro (Martin Walker) replayer, ported from NostalgicPlayer's
// C# implementation. Drives a 4-channel Amiga Paula (see paula.h).
// 50Hz tick rate (PAL).
//
// AVP modules are commonly ripped from m68k executables, so the loader
// scans the player code to discover the layout (init function, play
// function, various table offsets, effect/format variants, etc). The
// scanner here mirrors the C# Identify / ExtractInfoFromInitFunction /
// ExtractInfoFromPlayFunction logic byte-for-byte.
//
// Public API:
//   struct activisionpro_state *activisionpro_init(void *data, uint32_t len, int32_t sample_rate);
//   void activisionpro_free(struct activisionpro_state *s);
//   void activisionpro_get_audio(struct activisionpro_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define AVP_TICK_HZ        50
#define AVP_SCAN_BUFFER    4096
#define AVP_NUM_SAMPLES    27
#define AVP_PT_ONLYONE     0
#define AVP_PT_BOTH        1

struct avp_envelope_point {
	uint8_t  ticks_to_wait;
	int8_t   volume_increment;
	uint8_t  times_to_repeat;
};

struct avp_envelope {
	struct avp_envelope_point points[6];
};

struct avp_instrument {
	uint8_t  sample_number;
	uint8_t  envelope_number;
	uint8_t  volume;
	uint8_t  enabled_effect_flags;
	uint8_t  portamento_add;
	int16_t  fine_tune;
	uint8_t  stop_reset_effect_delay;
	uint8_t  sample_number2;
	uint16_t sample_start_offset;
	int8_t   arpeggio_table[4];
	uint8_t  fixed_or_transposed_note;
	int8_t   transpose;
	uint8_t  vibrato_number;
	uint8_t  vibrato_delay;
};

struct avp_sample {
	uint16_t length;
	uint16_t loop_start;
	uint16_t loop_length;
	int8_t  *sample_data;
	uint32_t sample_data_len;
};

struct avp_song_info {
	uint8_t *position_lists[4];
	uint32_t position_list_lengths[4];
	int8_t   speed_variation[8];
};

struct avp_voice_info {
	uint8_t  speed_counter;
	uint8_t  speed_counter2;
	uint8_t  max_speed_counter;
	uint8_t  tick_counter;

	uint8_t *position_list;
	uint32_t position_list_length;
	int8_t   position_list_position;
	uint8_t  position_list_loop_enabled;
	uint8_t  position_list_loop_count;
	int8_t   position_list_loop_start;

	uint8_t  track_number;
	uint8_t  track_position;
	uint8_t  loop_track_counter;

	uint8_t  note;
	int8_t   transpose;
	int16_t  fine_tune;
	uint8_t  note_and_flag;
	uint16_t period;

	int8_t   instrument_number;
	struct avp_instrument *instrument;

	uint8_t  sample_number;
	int8_t  *sample_data;
	uint16_t sample_length;
	uint16_t sample_loop_start;
	uint16_t sample_loop_length;

	uint8_t  enabled_effects_flag;
	uint8_t  stop_reset_effect;
	uint8_t  stop_reset_effect_delay;

	struct avp_envelope *envelope;
	uint8_t  envelope_position;
	int8_t   envelope_wait_counter;
	uint8_t  envelope_loop_count;

	uint8_t  mute;
	uint16_t volume;
	uint8_t  track_volume;

	uint8_t  portamento_value;

	uint16_t vibrato_speed;
	uint8_t  vibrato_delay;
	int8_t   vibrato_depth;
	uint8_t  vibrato_direction;
	uint8_t  vibrato_count_direction;
	uint8_t  vibrato_counter_max;
	uint8_t  vibrato_counter;
};

struct avp_global_playing_info {
	int8_t   speed_variation_counter;
	int8_t   speed_index;
	uint8_t  speed_variation2_counter;
	uint8_t  speed_variation2_speed;

	int8_t   master_volume_fade_counter;
	int8_t   master_volume_fade_speed;
	uint16_t master_volume;

	int8_t   global_transpose;
};

struct activisionpro_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	int32_t  sub_song_list_offset;
	int32_t  position_lists_offset;
	int32_t  track_offsets_offset;
	int32_t  tracks_offset;
	int32_t  envelopes_offset;
	int32_t  instruments_offset;
	int32_t  sample_info_offset;
	int32_t  sample_start_offsets_offset;
	int32_t  sample_data_offset;
	int32_t  speed_variation_speed_increment_offset;

	int32_t  instrument_format_version;
	int32_t  parse_track_version;
	int32_t  speed_variation_version;
	uint8_t  speed_variation_speed_init;
	int32_t  portamento_vibrato_type;
	int32_t  vibrato_version;
	uint8_t  have_separate_sample_info;
	uint8_t  have_set_note;
	uint8_t  have_set_fixed_sample;
	uint8_t  have_set_arpeggio;
	uint8_t  have_set_sample;
	uint8_t  have_arpeggio;
	uint8_t  have_envelope;
	uint8_t  reset_volume;

	struct avp_song_info *song_info_list;
	uint32_t num_sub_songs;

	uint8_t **tracks;
	uint32_t *track_lengths;
	uint32_t  num_tracks;

	struct avp_envelope *envelopes;
	uint32_t num_envelopes;

	struct avp_instrument *instruments;
	uint32_t num_instruments;

	struct avp_sample samples[AVP_NUM_SAMPLES];

	struct avp_song_info *current_song_info;
	struct avp_global_playing_info playing_info;
	struct avp_voice_info voices[4];

	uint8_t ended;
};

// [=]===^=[ avp_periods ]========================================================================[=]
static uint16_t avp_periods[] = {
	1695, 1600, 1505, 1426, 1347, 1268, 1189, 1125, 1062, 1006,  951,  895,
	1695, 1600, 1505, 1426, 1347, 1268, 1189, 1125, 1062, 1006,  951,  895,
	1695, 1600, 1505, 1426, 1347, 1268, 1189, 1125, 1062, 1006,  951, 1790,
	1695, 1600, 1505, 1426, 1347, 1268, 1189, 1125, 1062, 1006,  951,  895,
	 848,  800,  753,  713,  674,  634,  595,  563,  531,  503,  476,  448,
	 424,  400,  377,  357,  337,  317,  298,  282,  266,  252,  238,  224,
	 212,  200,  189,  179,  169,  159,  149,  141,  133,  126,  119,  112,
	 106
};

// [=]===^=[ avp_vibrato_counters ]===============================================================[=]
static uint8_t avp_vibrato_counters[19] = {
	0, 1, 1, 1, 1, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 3, 2, 2, 1
};

// [=]===^=[ avp_vibrato_depths1 ]================================================================[=]
static int8_t avp_vibrato_depths1[19] = {
	0, 5, 4, 3, 2, 5, 5, 4, 4, 4, 3, 1, 3, 2, 0, 1, 1, 0, 0
};

// [=]===^=[ avp_vibrato_depths2 ]================================================================[=]
static int8_t avp_vibrato_depths2[19] = {
	1, 32, 16, 8, 4, 32, 32, 16, 16, 16, 8, 2, 8, 4, 1, 2, 2, 1, 1
};

// [=]===^=[ avp_read_be_u16 ]====================================================================[=]
static uint16_t avp_read_be_u16(uint8_t *p) {
	return (uint16_t)((p[0] << 8) | p[1]);
}

// [=]===^=[ avp_read_be_s16 ]====================================================================[=]
static int16_t avp_read_be_s16(uint8_t *p) {
	return (int16_t)((p[0] << 8) | p[1]);
}

// [=]===^=[ avp_read_be_u32 ]====================================================================[=]
static uint32_t avp_read_be_u32(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ avp_s8_rel ]=========================================================================[=]
// m68k-style 16-bit displacement: signed high byte + unsigned low byte.
static int32_t avp_s8_rel(uint8_t hi, uint8_t lo) {
	return ((int32_t)(int8_t)hi << 8) | (int32_t)lo;
}

// [=]===^=[ avp_find_start_offset ]==============================================================[=]
// Try to find the start of the player code (MOVEM.L D2-D7/A2-A6,-(SP): 48 e7 fc fe).
static int32_t avp_find_start_offset(uint8_t *buf) {
	for(int32_t i = 0; i < 0x400; i++) {
		if((buf[i] == 0x48) && (buf[i + 1] == 0xe7) && (buf[i + 2] == 0xfc) && (buf[i + 3] == 0xfe)) {
			return i;
		}
	}
	return -1;
}

// [=]===^=[ avp_extract_info_from_init ]=========================================================[=]
static int32_t avp_extract_info_from_init(struct activisionpro_state *s, uint8_t *buf, int32_t search_length, int32_t start_offset) {
	int32_t index;

	for(index = start_offset; index < (search_length - 6); index += 2) {
		if((buf[index] == 0xe9) && (buf[index + 1] == 0x41) && (buf[index + 2] == 0x70) && (buf[index + 3] == 0x00) && (buf[index + 4] == 0x41) && (buf[index + 5] == 0xfa)) {
			break;
		}
	}

	if(index >= (search_length - 6)) {
		return 0;
	}

	s->sub_song_list_offset = avp_s8_rel(buf[index + 6], buf[index + 7]) + index + 6;

	for(; index < (search_length - 4); index += 2) {
		if((buf[index] == 0x4e) && (buf[index + 1] == 0x75)) {
			return 0;
		}
		if((buf[index] == 0x61) && (buf[index + 1] == 0x00)) {
			break;
		}
	}

	if(index >= (search_length - 4)) {
		return 0;
	}

	index = avp_s8_rel(buf[index + 2], buf[index + 3]) + index + 2;

	if(index >= search_length) {
		return 0;
	}

	if((buf[index] != 0x7a) || (buf[index + 1] != 0x00)) {
		return 0;
	}

	if((buf[index + 6] != 0x49) || (buf[index + 7] != 0xfa)) {
		return 0;
	}

	s->position_lists_offset = avp_s8_rel(buf[index + 8], buf[index + 9]) + index + 8;

	return 1;
}

// [=]===^=[ avp_find_enabled_effects ]===========================================================[=]
static int32_t avp_find_enabled_effects(struct activisionpro_state *s, uint8_t *buf, int32_t search_length, int32_t start_offset) {
	int32_t index = start_offset;

	for(; index < (search_length - 8); index += 2) {
		if((buf[index] == 0x08) && (buf[index + 1] == 0x31) && (buf[index + 6] == 0x67)) {
			break;
		}
	}

	if(index >= (search_length - 8)) {
		return -1;
	}

	s->have_set_note = 0;
	s->have_set_fixed_sample = 0;
	s->have_set_arpeggio = 0;
	s->have_set_sample = 0;
	s->have_arpeggio = 0;

	do {
		switch(buf[index + 3]) {
			case 0x01: {
				s->have_set_note = 1;
				break;
			}

			case 0x02: {
				s->have_set_fixed_sample = 1;
				break;
			}

			case 0x03: {
				s->have_set_arpeggio = 1;
				break;
			}

			case 0x04: {
				s->have_set_sample = 1;
				break;
			}

			case 0x05: {
				s->have_arpeggio = 1;
				break;
			}
		}

		index += (int32_t)(int8_t)buf[index + 7] + 8;
		if((index < 0) || (index >= search_length)) {
			return -1;
		}
	} while((buf[index] == 0x08) && (buf[index + 1] == 0x31) && (buf[index + 6] == 0x67));

	return index;
}

// [=]===^=[ avp_extract_info_from_play ]=========================================================[=]
static int32_t avp_extract_info_from_play(struct activisionpro_state *s, uint8_t *buf, int32_t search_length, int32_t start_offset) {
	int32_t index;
	int32_t start_of_play;
	int32_t global_offset = 0;
	s->instruments_offset = 0;

	for(index = start_offset; index < (search_length - 8); index += 2) {
		if((buf[index] == 0x2c) && (buf[index + 1] == 0x7c) && (buf[index + 6] == 0x4a) && (buf[index + 7] == 0x29)) {
			break;
		}
	}

	if(index >= (search_length - 8)) {
		return 0;
	}

	start_of_play = index;
	index -= 4;

	for(; index >= 0; index -= 2) {
		if((buf[index] == 0x4b) && (buf[index + 1] == 0xfa)) {
			s->instruments_offset = avp_s8_rel(buf[index + 2], buf[index + 3]) + index + 2;
		} else if((buf[index] == 0x43) && (buf[index + 1] == 0xfa)) {
			global_offset = avp_s8_rel(buf[index + 2], buf[index + 3]) + index + 2;
		}

		if((s->instruments_offset != 0) && (global_offset != 0)) {
			break;
		}
	}

	if((s->instruments_offset == 0) || (global_offset == 0)) {
		return 0;
	}

	for(index = start_of_play; index < (search_length - 16); index += 2) {
		if((buf[index] == 0x53) && (buf[index + 1] == 0x69) && (buf[index + 4] == 0x67)) {
			break;
		}
	}

	if(index >= (search_length - 16)) {
		return 0;
	}

	if((buf[index + 6] == 0x70) && (buf[index + 7] == 0x03)) {
		s->speed_variation_version = 1;
	} else if((buf[index + 6] == 0x7a) && (buf[index + 7] == 0x00)) {
		s->speed_variation_version = 2;

		if((buf[index + 12] != 0xda) || (buf[index + 13] != 0x29)) {
			return 0;
		}

		s->speed_variation_speed_increment_offset = global_offset + avp_s8_rel(buf[index + 14], buf[index + 15]);
	} else {
		return 0;
	}

	index += 8;

	for(; index < (search_length - 12); index += 2) {
		if((buf[index] == 0x7a) && (buf[index + 1] == 0x00) &&
		   (buf[index + 2] == 0x1a) && (buf[index + 3] == 0x31) &&
		   (buf[index + 6] == 0xda) && (buf[index + 7] == 0x45) &&
		   (buf[index + 8] == 0x49) && (buf[index + 9] == 0xfa)) {
			break;
		}
	}

	if(index >= (search_length - 12)) {
		return 0;
	}

	s->track_offsets_offset = avp_s8_rel(buf[index + 10], buf[index + 11]) + index + 10;

	index += 12;

	if(index >= (search_length - 8)) {
		return 0;
	}

	if((buf[index] != 0x3a) || (buf[index + 1] != 0x34) || (buf[index + 4] != 0x49) || (buf[index + 5] != 0xfa)) {
		return 0;
	}

	s->tracks_offset = avp_s8_rel(buf[index + 6], buf[index + 7]) + index + 6;

	index += 8;

	for(; index < (search_length - 6); index += 2) {
		if((buf[index] == 0x18) && (buf[index + 1] == 0x31)) {
			break;
		}
	}

	if(index >= (search_length - 6)) {
		return 0;
	}

	s->reset_volume = (buf[index + 4] == 0x66) ? 1 : 0;

	index += 6;

	for(; index < (search_length - 10); index += 2) {
		if((buf[index] == 0x42) && (buf[index + 1] == 0x31)) {
			break;
		}
	}

	if(index >= (search_length - 10)) {
		return 0;
	}

	index += 8;

	if((buf[index] == 0x08) && (buf[index + 1] == 0x31)) {
		s->parse_track_version = 1;
	} else if((buf[index] == 0x4a) && (buf[index + 1] == 0x34)) {
		s->parse_track_version = 2;
	} else if((buf[index] == 0x1a) && (buf[index + 1] == 0x34)) {
		s->parse_track_version = 3;
	} else if((buf[index] == 0x42) && (buf[index + 1] == 0x30)) {
		s->parse_track_version = 4;

		index += 2;

		for(; index < (search_length - 4); index += 2) {
			if((buf[index] == 0x31) && (buf[index + 1] == 0x85)) {
				break;
			}

			if((buf[index] == 0x0c) && (buf[index + 1] == 0x05) && (buf[index + 2] == 0x00) && (buf[index + 3] == 0x84)) {
				s->parse_track_version = 5;
				break;
			}
		}

		if(index >= (search_length - 4)) {
			return 0;
		}

		index -= 2;
	} else {
		return 0;
	}

	index += 2;

	for(; index < (search_length - 2); index += 2) {
		if((buf[index] == 0x31) && (buf[index + 1] == 0x85)) {
			break;
		}
	}

	if(index >= (search_length - 2)) {
		return 0;
	}

	index += 4;

	s->instrument_format_version = 0;

	if(index >= (search_length - 16)) {
		return 0;
	}

	if((buf[index] == 0x13) && (buf[index + 1] == 0xb5) && (buf[index + 2] == 0x50) && (buf[index + 3] == 0x02) &&
	   (buf[index + 6] == 0x13) && (buf[index + 7] == 0xb5) && (buf[index + 8] == 0x50) && (buf[index + 9] == 0x07) &&
	   (buf[index + 12] == 0x13) && (buf[index + 13] == 0xb5) && (buf[index + 14] == 0x50) && (buf[index + 15] == 0x0f)) {
		s->instrument_format_version = 1;
	} else if((buf[index] == 0x11) && (buf[index + 1] == 0xb5) && (buf[index + 2] == 0x50) && (buf[index + 3] == 0x01) &&
		(buf[index + 6] == 0x13) && (buf[index + 7] == 0xb5) && (buf[index + 8] == 0x50) && (buf[index + 9] == 0x02) &&
		(buf[index + 12] == 0x13) && (buf[index + 13] == 0xb5) && (buf[index + 14] == 0x50) && (buf[index + 15] == 0x07) &&
		(buf[index + 18] == 0x13) && (buf[index + 19] == 0xb5) && (buf[index + 20] == 0x50) && (buf[index + 21] == 0x0f)) {
		s->instrument_format_version = 2;
	} else if((buf[index] == 0x11) && (buf[index + 1] == 0xb5) && (buf[index + 2] == 0x50) && (buf[index + 3] == 0x01) &&
		(buf[index + 6] == 0x13) && (buf[index + 7] == 0xb5) && (buf[index + 8] == 0x50) && (buf[index + 9] == 0x02) &&
		(buf[index + 12] == 0x13) && (buf[index + 13] == 0xb5) && (buf[index + 14] == 0x50) && (buf[index + 15] == 0x03) &&
		(buf[index + 18] == 0x31) && (buf[index + 19] == 0xb5) && (buf[index + 20] == 0x50) && (buf[index + 21] == 0x04) &&
		(buf[index + 24] == 0x33) && (buf[index + 25] == 0x75) && (buf[index + 26] == 0x50) && (buf[index + 27] == 0x06) &&
		(buf[index + 30] == 0x13) && (buf[index + 31] == 0xb5) && (buf[index + 32] == 0x50) && (buf[index + 33] == 0x08) &&
		(buf[index + 36] == 0x13) && (buf[index + 37] == 0xb5) && (buf[index + 38] == 0x50) && (buf[index + 39] == 0x0f)) {
		s->instrument_format_version = 3;
	} else {
		return 0;
	}

	for(; index < (search_length - 14); index += 2) {
		if((buf[index] == 0xe5) && (buf[index + 1] == 0x45) && (buf[index + 2] == 0x45) && (buf[index + 3] == 0xfa)) {
			break;
		}
	}

	if(index >= (search_length - 14)) {
		return 0;
	}

	s->sample_start_offsets_offset = avp_s8_rel(buf[index + 4], buf[index + 5]) + index + 4;

	if((buf[index + 10] != 0x45) || (buf[index + 11] != 0xfa)) {
		return 0;
	}

	s->sample_data_offset = avp_s8_rel(buf[index + 12], buf[index + 13]) + index + 12;

	index += 14;

	if(index >= (search_length - 20)) {
		return 0;
	}

	s->have_separate_sample_info = 0;

	if((buf[index + 12] == 0xca) && (buf[index + 13] == 0xfc) && (buf[index + 16] == 0x45) && (buf[index + 17] == 0xfa)) {
		s->have_separate_sample_info = 1;
		s->sample_info_offset = avp_s8_rel(buf[index + 18], buf[index + 19]) + index + 18;
		index += 18;
	}

	for(; index < (search_length - 12); index += 2) {
		if((buf[index] == 0x6b) && (buf[index + 1] == 0x00) && (buf[index + 4] == 0x4a) && (buf[index + 5] == 0x31)) {
			break;
		}
	}

	if(index >= (search_length - 12)) {
		return 0;
	}

	index += 10;

	if((buf[index] == 0x7a) && (buf[index + 1] == 0x00)) {
		s->portamento_vibrato_type = AVP_PT_ONLYONE;
	} else if((buf[index] == 0x53) && (buf[index + 1] == 0x31)) {
		s->portamento_vibrato_type = AVP_PT_BOTH;
	} else {
		return 0;
	}

	for(; index < (search_length - 2); index += 2) {
		if((buf[index] == 0xda) && (buf[index + 1] == 0x45)) {
			break;
		}
	}

	if(index >= (search_length - 2)) {
		return 0;
	}

	for(; index < (search_length - 10); index += 2) {
		if((buf[index] == 0x9b) && (buf[index + 1] == 0x70)) {
			break;
		}
	}

	if(index >= (search_length - 10)) {
		return 0;
	}

	if((buf[index + 4] == 0x53) && (buf[index + 5] == 0x31)) {
		s->vibrato_version = 1;
	} else if((buf[index + 8] == 0x8a) && (buf[index + 9] == 0xf1)) {
		s->vibrato_version = 2;
	} else {
		return 0;
	}

	index = avp_find_enabled_effects(s, buf, search_length, index + 10);
	if(index == -1) {
		return 0;
	}

	s->have_envelope = 0;

	if(index >= (search_length - 8)) {
		return 0;
	}

	if((buf[index + 4] == 0x6b) && (buf[index + 6] == 0x4a) && (buf[index + 7] == 0x31)) {
		s->have_envelope = 1;

		index += 8;

		for(; index < (search_length - 10); index += 2) {
			if((buf[index] == 0xe9) && (buf[index + 1] == 0x44) && ((buf[index + 2] == 0x31) || (buf[index + 2] == 0x11)) && (buf[index + 3] == 0x84) && (buf[index + 6] == 0x45) && (buf[index + 7] == 0xfa)) {
				break;
			}
		}

		if(index >= (search_length - 10)) {
			return 0;
		}

		s->envelopes_offset = avp_s8_rel(buf[index + 8], buf[index + 9]) + index + 8;
	}

	return 1;
}

// [=]===^=[ avp_test_module ]====================================================================[=]
static int32_t avp_test_module(struct activisionpro_state *s) {
	if(s->module_len < 1024) {
		return 0;
	}

	int32_t search_length = (int32_t)s->module_len;
	if(search_length > AVP_SCAN_BUFFER) {
		search_length = AVP_SCAN_BUFFER;
	}

	uint8_t *buf = s->module_data;

	int32_t start_offset = avp_find_start_offset(buf);
	if(start_offset < 0) {
		return 0;
	}

	if(!avp_extract_info_from_init(s, buf, search_length, start_offset)) {
		return 0;
	}

	if(!avp_extract_info_from_play(s, buf, search_length, start_offset)) {
		return 0;
	}

	return 1;
}

// [=]===^=[ avp_load_position_list ]=============================================================[=]
static uint8_t *avp_load_position_list(struct activisionpro_state *s, int32_t start, uint32_t *out_len) {
	uint32_t cap = 64;
	uint32_t len = 0;
	uint8_t *list = (uint8_t *)malloc(cap);
	if(!list) {
		return 0;
	}

	int32_t pos = start;

	for(;;) {
		if(pos >= (int32_t)s->module_len) {
			free(list);
			return 0;
		}
		uint8_t dat = s->module_data[pos++];

		if(len + 2 > cap) {
			cap *= 2;
			uint8_t *nl = (uint8_t *)realloc(list, cap);
			if(!nl) {
				free(list);
				return 0;
			}
			list = nl;
		}

		list[len++] = dat;

		if((dat >= 0xfd) || ((dat & 0x40) == 0)) {
			if(pos >= (int32_t)s->module_len) {
				free(list);
				return 0;
			}
			list[len++] = s->module_data[pos++];
		}

		if((dat == 0xfe) || (dat == 0xff)) {
			break;
		}
	}

	*out_len = len;
	return list;
}

// [=]===^=[ avp_load_sub_song_info ]=============================================================[=]
static int32_t avp_load_sub_song_info(struct activisionpro_state *s) {
	uint32_t num = (uint32_t)(s->position_lists_offset - s->sub_song_list_offset) / 16;
	if(num == 0) {
		return 0;
	}

	s->song_info_list = (struct avp_song_info *)calloc(num, sizeof(struct avp_song_info));
	if(!s->song_info_list) {
		return 0;
	}
	s->num_sub_songs = num;

	for(uint32_t i = 0; i < num; i++) {
		int32_t rec = s->sub_song_list_offset + (int32_t)i * 16;
		if((rec + 16) > (int32_t)s->module_len) {
			return 0;
		}

		uint16_t pos_offsets[4];
		for(uint32_t k = 0; k < 4; k++) {
			pos_offsets[k] = avp_read_be_u16(&s->module_data[rec + (int32_t)k * 2]);
		}

		struct avp_song_info *si = &s->song_info_list[i];
		for(uint32_t k = 0; k < 8; k++) {
			si->speed_variation[k] = (int8_t)s->module_data[rec + 8 + (int32_t)k];
		}

		for(uint32_t j = 0; j < 4; j++) {
			int32_t pl_start = s->position_lists_offset + (int32_t)pos_offsets[j];
			if(pl_start < 0 || pl_start >= (int32_t)s->module_len) {
				return 0;
			}
			uint32_t pl_len = 0;
			uint8_t *pl = avp_load_position_list(s, pl_start, &pl_len);
			if(!pl) {
				return 0;
			}
			si->position_lists[j] = pl;
			si->position_list_lengths[j] = pl_len;
		}
	}

	return 1;
}

// [=]===^=[ avp_load_single_track ]==============================================================[=]
static uint8_t *avp_load_single_track(struct activisionpro_state *s, int32_t start, uint32_t *out_len) {
	uint32_t cap = 64;
	uint32_t len = 0;
	uint8_t *track = (uint8_t *)malloc(cap);
	if(!track) {
		return 0;
	}

	int32_t pos = start;

	for(;;) {
		if(pos >= (int32_t)s->module_len) {
			free(track);
			return 0;
		}
		uint8_t dat = s->module_data[pos++];

		if(len + 16 > cap) {
			cap *= 2;
			uint8_t *nt = (uint8_t *)realloc(track, cap);
			if(!nt) {
				free(track);
				return 0;
			}
			track = nt;
		}

		track[len++] = dat;

		if(dat == 0xff) {
			break;
		}

		if(s->parse_track_version == 3) {
			while((dat & 0x80) != 0) {
				if(pos >= (int32_t)s->module_len) {
					free(track);
					return 0;
				}
				track[len++] = s->module_data[pos++];
				if(pos >= (int32_t)s->module_len) {
					free(track);
					return 0;
				}
				dat = s->module_data[pos++];
				track[len++] = dat;
				if(len + 16 > cap) {
					cap *= 2;
					uint8_t *nt = (uint8_t *)realloc(track, cap);
					if(!nt) {
						free(track);
						return 0;
					}
					track = nt;
				}
			}
		} else if((s->parse_track_version == 4) || (s->parse_track_version == 5)) {
			if(dat != 0x81) {
				while((dat & 0x80) != 0) {
					if(pos >= (int32_t)s->module_len) {
						free(track);
						return 0;
					}
					track[len++] = s->module_data[pos++];
					if(pos >= (int32_t)s->module_len) {
						free(track);
						return 0;
					}
					dat = s->module_data[pos++];
					track[len++] = dat;
					if(len + 16 > cap) {
						cap *= 2;
						uint8_t *nt = (uint8_t *)realloc(track, cap);
						if(!nt) {
							free(track);
							return 0;
						}
						track = nt;
					}
				}
			}
		} else {
			if((dat & 0x80) != 0) {
				if(pos >= (int32_t)s->module_len) {
					free(track);
					return 0;
				}
				track[len++] = s->module_data[pos++];

				if(s->parse_track_version == 2) {
					if(pos >= (int32_t)s->module_len) {
						free(track);
						return 0;
					}
					track[len++] = s->module_data[pos++];
				}
			}
		}

		if(pos >= (int32_t)s->module_len) {
			free(track);
			return 0;
		}
		track[len++] = s->module_data[pos++];
	}

	*out_len = len;
	return track;
}

// [=]===^=[ avp_load_tracks ]====================================================================[=]
static int32_t avp_load_tracks(struct activisionpro_state *s) {
	uint32_t num = (uint32_t)(s->tracks_offset - s->track_offsets_offset) / 2;
	if(num == 0) {
		return 0;
	}

	s->tracks = (uint8_t **)calloc(num, sizeof(uint8_t *));
	s->track_lengths = (uint32_t *)calloc(num, sizeof(uint32_t));
	if(!s->tracks || !s->track_lengths) {
		return 0;
	}
	s->num_tracks = num;

	int32_t tab = s->track_offsets_offset;
	if(tab + (int32_t)num * 2 > (int32_t)s->module_len) {
		return 0;
	}

	for(uint32_t i = 0; i < num; i++) {
		int16_t tof = avp_read_be_s16(&s->module_data[tab + (int32_t)i * 2]);
		if(tof < 0) {
			continue;
		}

		int32_t start = s->tracks_offset + (int32_t)tof;
		if(start < 0 || start >= (int32_t)s->module_len) {
			return 0;
		}
		uint32_t tl = 0;
		uint8_t *td = avp_load_single_track(s, start, &tl);
		if(!td) {
			return 0;
		}
		s->tracks[i] = td;
		s->track_lengths[i] = tl;
	}

	return 1;
}

// [=]===^=[ avp_load_envelopes ]=================================================================[=]
static int32_t avp_load_envelopes(struct activisionpro_state *s) {
	if(!s->have_envelope) {
		return 1;
	}

	uint32_t num = (uint32_t)(s->instruments_offset - s->envelopes_offset) / 16;
	if(num == 0) {
		return 0;
	}

	s->envelopes = (struct avp_envelope *)calloc(num, sizeof(struct avp_envelope));
	if(!s->envelopes) {
		return 0;
	}
	s->num_envelopes = num;

	int32_t pos = s->envelopes_offset;
	if(pos + (int32_t)num * 16 > (int32_t)s->module_len) {
		return 0;
	}

	for(uint32_t i = 0; i < num; i++) {
		struct avp_envelope *env = &s->envelopes[i];

		for(uint32_t j = 0; j < 5; j++) {
			env->points[j].ticks_to_wait = s->module_data[pos++];
			env->points[j].volume_increment = (int8_t)s->module_data[pos++];
			env->points[j].times_to_repeat = s->module_data[pos++];
		}

		// Extra sentinel point (Dragon Breed quirk).
		env->points[5].ticks_to_wait = s->module_data[pos++];
		env->points[5].volume_increment = 0;
		env->points[5].times_to_repeat = 0xff;
	}

	return 1;
}

// [=]===^=[ avp_load_instrument1 ]===============================================================[=]
static void avp_load_instrument1(struct activisionpro_state *s, struct avp_instrument *ins, int32_t pos) {
	ins->sample_number = s->module_data[pos + 0];
	ins->envelope_number = s->module_data[pos + 1];
	ins->enabled_effect_flags = s->module_data[pos + 2];
	ins->portamento_add = s->module_data[pos + 4];
	ins->stop_reset_effect_delay = s->module_data[pos + 7];
	ins->sample_number2 = s->module_data[pos + 8];
	ins->arpeggio_table[0] = (int8_t)s->module_data[pos + 9];
	ins->arpeggio_table[1] = (int8_t)s->module_data[pos + 10];
	ins->arpeggio_table[2] = (int8_t)s->module_data[pos + 11];
	ins->arpeggio_table[3] = (int8_t)s->module_data[pos + 12];
	ins->fixed_or_transposed_note = s->module_data[pos + 13];
	ins->vibrato_number = s->module_data[pos + 14];
	ins->vibrato_delay = s->module_data[pos + 15];
}

// [=]===^=[ avp_load_instrument2 ]===============================================================[=]
static void avp_load_instrument2(struct activisionpro_state *s, struct avp_instrument *ins, int32_t pos) {
	ins->sample_number = s->module_data[pos + 0];
	ins->volume = s->module_data[pos + 1];
	ins->enabled_effect_flags = s->module_data[pos + 2];
	ins->portamento_add = s->module_data[pos + 4];
	ins->stop_reset_effect_delay = s->module_data[pos + 7];
	ins->sample_number2 = s->module_data[pos + 8];
	ins->arpeggio_table[0] = (int8_t)s->module_data[pos + 9];
	ins->arpeggio_table[1] = (int8_t)s->module_data[pos + 10];
	ins->arpeggio_table[2] = (int8_t)s->module_data[pos + 11];
	ins->arpeggio_table[3] = (int8_t)s->module_data[pos + 12];
	ins->fixed_or_transposed_note = s->module_data[pos + 13];
	ins->vibrato_number = s->module_data[pos + 14];
	ins->vibrato_delay = s->module_data[pos + 15];
}

// [=]===^=[ avp_load_instrument3 ]===============================================================[=]
static void avp_load_instrument3(struct activisionpro_state *s, struct avp_instrument *ins, int32_t pos) {
	ins->sample_number = s->module_data[pos + 0];
	ins->volume = s->module_data[pos + 1];
	ins->enabled_effect_flags = s->module_data[pos + 2];
	ins->transpose = (int8_t)s->module_data[pos + 3];
	ins->fine_tune = avp_read_be_s16(&s->module_data[pos + 4]);
	ins->sample_start_offset = avp_read_be_u16(&s->module_data[pos + 6]);
	ins->stop_reset_effect_delay = s->module_data[pos + 8];
	ins->arpeggio_table[0] = (int8_t)s->module_data[pos + 9];
	ins->arpeggio_table[1] = (int8_t)s->module_data[pos + 10];
	ins->arpeggio_table[2] = (int8_t)s->module_data[pos + 11];
	ins->arpeggio_table[3] = (int8_t)s->module_data[pos + 12];
	ins->fixed_or_transposed_note = s->module_data[pos + 13];
	ins->vibrato_number = s->module_data[pos + 14];
	ins->vibrato_delay = s->module_data[pos + 15];
}

// [=]===^=[ avp_load_instruments ]===============================================================[=]
static int32_t avp_load_instruments(struct activisionpro_state *s) {
	uint32_t num = (uint32_t)(s->track_offsets_offset - s->instruments_offset) / 16;
	if(num == 0) {
		return 0;
	}

	s->instruments = (struct avp_instrument *)calloc(num, sizeof(struct avp_instrument));
	if(!s->instruments) {
		return 0;
	}
	s->num_instruments = num;

	int32_t pos = s->instruments_offset;
	if(pos + (int32_t)num * 16 > (int32_t)s->module_len) {
		return 0;
	}

	for(uint32_t i = 0; i < num; i++) {
		struct avp_instrument *ins = &s->instruments[i];
		int32_t p = pos + (int32_t)i * 16;

		if(s->instrument_format_version == 1) {
			avp_load_instrument1(s, ins, p);
		} else if(s->instrument_format_version == 2) {
			avp_load_instrument2(s, ins, p);
		} else if(s->instrument_format_version == 3) {
			avp_load_instrument3(s, ins, p);
		} else {
			return 0;
		}
	}

	return 1;
}

// [=]===^=[ avp_load_sample_info ]===============================================================[=]
static int32_t avp_load_sample_info(struct activisionpro_state *s) {
	if(!s->have_separate_sample_info) {
		return 1;
	}

	int32_t pos = s->sample_info_offset;
	if(pos + (int32_t)AVP_NUM_SAMPLES * 6 > (int32_t)s->module_len) {
		return 0;
	}

	for(uint32_t i = 0; i < AVP_NUM_SAMPLES; i++) {
		s->samples[i].length = avp_read_be_u16(&s->module_data[pos + 0]);
		s->samples[i].loop_start = avp_read_be_u16(&s->module_data[pos + 2]);
		s->samples[i].loop_length = avp_read_be_u16(&s->module_data[pos + 4]);
		pos += 6;
	}

	return 1;
}

// [=]===^=[ avp_load_sample_data ]===============================================================[=]
static int32_t avp_load_sample_data(struct activisionpro_state *s) {
	int32_t tab = s->sample_start_offsets_offset;
	if(tab + (int32_t)(AVP_NUM_SAMPLES + 1) * 4 > (int32_t)s->module_len) {
		return 0;
	}

	uint32_t start_offsets[AVP_NUM_SAMPLES + 1];
	for(uint32_t i = 0; i < AVP_NUM_SAMPLES + 1; i++) {
		start_offsets[i] = avp_read_be_u32(&s->module_data[tab + (int32_t)i * 4]);
	}

	for(uint32_t i = 0; i < AVP_NUM_SAMPLES; i++) {
		struct avp_sample *sam = &s->samples[i];
		uint32_t length = start_offsets[i + 1] - start_offsets[i];

		if(length == 0) {
			sam->length = 0;
			sam->loop_start = 0;
			sam->loop_length = 1;
		} else {
			int32_t base = s->sample_data_offset + (int32_t)start_offsets[i];
			if(base < 0 || (uint32_t)base + length > s->module_len) {
				return 0;
			}

			if(!s->have_separate_sample_info) {
				if(length < 6) {
					return 0;
				}
				sam->length = avp_read_be_u16(&s->module_data[base + 0]);
				sam->loop_start = avp_read_be_u16(&s->module_data[base + 2]);
				sam->loop_length = avp_read_be_u16(&s->module_data[base + 4]);
				base += 6;
				length -= 6;
			}

			sam->sample_data = (int8_t *)malloc(length);
			if(!sam->sample_data) {
				return 0;
			}
			memcpy(sam->sample_data, &s->module_data[base], length);
			sam->sample_data_len = length;
		}
	}

	return 1;
}

// [=]===^=[ avp_parse_next_position ]============================================================[=]
static void avp_parse_next_position(struct activisionpro_state *s, struct avp_voice_info *v) {
	uint8_t *pl = v->position_list;
	int8_t position = v->position_list_position;
	uint8_t one_more;

	do {
		one_more = 0;

		position++;

		if(pl[position] >= 0xfe) {
			// Song done.
			v->track_number = pl[position];
			position = (int8_t)(pl[position + 1] - 1);
		} else {
			if(pl[position] == 0xfd) {
				// Start master volume fade.
				position++;
				s->playing_info.master_volume_fade_speed = (int8_t)pl[position];
				one_more = 1;
			} else {
				if((pl[position] & 0x40) != 0) {
					if(v->position_list_loop_enabled) {
						v->position_list_loop_count--;

						if(v->position_list_loop_count == 0) {
							v->position_list_loop_enabled = 0;
							one_more = 1;
							continue;
						}

						position = v->position_list_loop_start;
					} else {
						v->position_list_loop_enabled = 1;
						v->position_list_loop_count = pl[position] & 0x3f;
						v->position_list_loop_start = ++position;
					}
				}

				v->loop_track_counter = pl[position++];
				v->track_number = pl[position];

				if(s->parse_track_version == 5) {
					v->max_speed_counter = 255;
				}
			}
		}
	} while(one_more);

	v->position_list_position = position;
}

// [=]===^=[ avp_initialize_voices ]==============================================================[=]
static void avp_initialize_voices(struct activisionpro_state *s) {
	for(int32_t i = 0; i < 4; i++) {
		struct avp_voice_info *v = &s->voices[i];
		memset(v, 0, sizeof(*v));

		v->speed_counter = 1;
		v->position_list = s->current_song_info->position_lists[i];
		v->position_list_length = s->current_song_info->position_list_lengths[i];
		v->position_list_position = -1;
		v->track_volume = 64;

		if((s->parse_track_version == 3) || (s->parse_track_version == 4) || (s->parse_track_version == 5)) {
			v->instrument_number = 0;
			v->instrument = (s->num_instruments > 0) ? &s->instruments[0] : 0;
		} else {
			v->instrument_number = -1;
			v->instrument = 0;
		}

		avp_parse_next_position(s, v);
	}
}

// [=]===^=[ avp_initialize_sound ]===============================================================[=]
static void avp_initialize_sound(struct activisionpro_state *s, uint32_t sub_song) {
	if(sub_song >= s->num_sub_songs) {
		sub_song = 0;
	}
	s->current_song_info = &s->song_info_list[sub_song];

	memset(&s->playing_info, 0, sizeof(s->playing_info));
	s->playing_info.speed_variation2_counter = 255;
	s->playing_info.speed_variation2_speed = s->speed_variation_speed_init;
	s->playing_info.master_volume_fade_speed = -1;
	s->playing_info.master_volume = 64;

	s->ended = 0;
	avp_initialize_voices(s);
}

// [=]===^=[ avp_get_period ]=====================================================================[=]
static uint16_t avp_get_period(uint8_t note, int8_t transpose) {
	int32_t index = (int32_t)note + (int32_t)transpose;

	if(index < 0) {
		index = 0;
	} else if((uint32_t)index >= sizeof(avp_periods) / sizeof(avp_periods[0])) {
		index = 72;
	}

	return avp_periods[index];
}

// [=]===^=[ avp_stop_and_reset ]=================================================================[=]
static void avp_stop_and_reset(struct activisionpro_state *s) {
	s->playing_info.master_volume = 64;
	s->playing_info.master_volume_fade_speed = -1;
	s->playing_info.master_volume_fade_counter = 0;
	s->playing_info.speed_variation_counter = 0;

	for(int32_t i = 0; i < 4; i++) {
		paula_mute(&s->paula, i);
	}

	avp_initialize_voices(s);
	s->ended = 1;
}

// [=]===^=[ avp_do_master_volume_fade ]==========================================================[=]
static void avp_do_master_volume_fade(struct activisionpro_state *s) {
	if(s->playing_info.master_volume_fade_speed >= 0) {
		s->playing_info.master_volume_fade_counter--;

		if(s->playing_info.master_volume_fade_counter < 0) {
			s->playing_info.master_volume_fade_counter = s->playing_info.master_volume_fade_speed;
			s->playing_info.master_volume--;

			if(s->playing_info.master_volume == 0) {
				avp_stop_and_reset(s);
			}
		}
	}
}

// [=]===^=[ avp_do_speed_variation1 ]============================================================[=]
static void avp_do_speed_variation1(struct activisionpro_state *s) {
	if(s->speed_variation_version == 1) {
		s->playing_info.speed_variation_counter--;

		if(s->playing_info.speed_variation_counter < 0) {
			s->playing_info.speed_index--;

			if(s->playing_info.speed_index < 0) {
				s->playing_info.speed_index = 7;
			}

			s->playing_info.speed_variation_counter = s->current_song_info->speed_variation[s->playing_info.speed_index];
		}
	}
}

// [=]===^=[ avp_do_speed_variation2 ]============================================================[=]
static void avp_do_speed_variation2(struct activisionpro_state *s) {
	if(s->speed_variation_version == 2) {
		uint32_t counter = (uint32_t)s->playing_info.speed_variation2_counter + (uint32_t)s->playing_info.speed_variation2_speed;
		if(counter > 255) {
			s->playing_info.speed_variation_counter = 0;
		} else {
			s->playing_info.speed_variation_counter = -1;
		}

		s->playing_info.speed_variation2_counter = (uint8_t)counter;
	}
}

// [=]===^=[ avp_do_counters ]====================================================================[=]
static void avp_do_counters(struct avp_voice_info *v) {
	if(v->stop_reset_effect_delay == 0) {
		v->stop_reset_effect = 1;
	} else {
		v->stop_reset_effect_delay--;
	}

	v->tick_counter++;
}

// [=]===^=[ avp_set_arpeggio ]===================================================================[=]
static void avp_set_arpeggio(struct avp_voice_info *v, int32_t index) {
	uint8_t note;
	int8_t arp = v->instrument->arpeggio_table[index & 3];

	if(arp >= 0) {
		note = (uint8_t)arp;
	} else {
		note = v->note;
		arp &= 0x7f;

		if(arp < 64) {
			note += (uint8_t)arp;
		} else {
			note -= (uint8_t)(arp & 0x3f);
		}

		note = (uint8_t)(note + v->transpose);
	}

	v->period = avp_get_period(note, 0);
}

// [=]===^=[ avp_do_vibrato ]=====================================================================[=]
static void avp_do_vibrato(struct activisionpro_state *s, struct avp_voice_info *v) {
	if(v->vibrato_delay != 0) {
		v->vibrato_delay--;
		return;
	}

	if(v->stop_reset_effect_delay != 0) {
		return;
	}

	struct avp_instrument *ins = v->instrument;
	if(!ins || ins->vibrato_number == 0) {
		return;
	}

	if(v->vibrato_depth >= 0) {
		v->vibrato_counter_max = avp_vibrato_counters[ins->vibrato_number];
		v->vibrato_depth = (s->vibrato_version == 1) ? avp_vibrato_depths1[ins->vibrato_number] : avp_vibrato_depths2[ins->vibrato_number];
		v->vibrato_count_direction = 0;
		v->vibrato_counter = 0;

		v->period = avp_get_period(v->note, v->transpose);
		v->vibrato_speed = (uint16_t)(v->period - avp_get_period((uint8_t)(v->note + 1), v->transpose));

		if(s->vibrato_version == 1) {
			for(;;) {
				v->vibrato_depth--;

				if(v->vibrato_depth < 0) {
					break;
				}

				v->vibrato_speed /= 2;

				if(v->vibrato_speed == 0) {
					v->vibrato_speed = 1;
					v->vibrato_depth = -1;
					break;
				}
			}
		} else {
			int32_t new_speed = (v->vibrato_depth != 0) ? ((int32_t)v->vibrato_speed / (int32_t)v->vibrato_depth) : 0;
			if(new_speed == 0) {
				new_speed = 1;
			}
			v->vibrato_speed = (uint16_t)new_speed;
			v->vibrato_depth = -1;
		}
	} else {
		if(v->vibrato_direction) {
			v->period -= v->vibrato_speed;
		} else {
			v->period += v->vibrato_speed;
		}

		if(v->vibrato_count_direction) {
			v->vibrato_counter--;

			if(v->vibrato_counter == 0) {
				v->vibrato_count_direction = 0;
			}
		} else {
			v->vibrato_counter++;

			if(v->vibrato_counter == v->vibrato_counter_max) {
				v->vibrato_count_direction = 1;
				v->vibrato_direction = !v->vibrato_direction;
			}
		}
	}
}

// [=]===^=[ avp_do_portamento ]==================================================================[=]
static void avp_do_portamento(struct avp_voice_info *v) {
	uint8_t p = v->portamento_value;

	if(p >= 0xc0) {
		v->period += (uint16_t)(p & 0x3f);
	} else {
		v->period -= (uint16_t)(p & 0x3f);
	}
}

// [=]===^=[ avp_do_set_note ]====================================================================[=]
static void avp_do_set_note(struct activisionpro_state *s, struct avp_voice_info *v) {
	if(s->have_set_note && ((v->enabled_effects_flag & 0x02) != 0)) {
		struct avp_instrument *ins = v->instrument;
		uint8_t note;

		if((v->tick_counter % 2) == 0) {
			note = (uint8_t)(v->note + v->transpose);
		} else {
			note = ins->fixed_or_transposed_note;

			if((note & 0x80) != 0) {
				note &= 0x7f;
				note += v->note;
			}
		}

		v->period = avp_get_period(note, 0);
	}
}

// [=]===^=[ avp_do_set_fixed_sample ]============================================================[=]
static void avp_do_set_fixed_sample(struct activisionpro_state *s, struct avp_voice_info *v) {
	if(s->have_set_fixed_sample && ((v->enabled_effects_flag & 0x04) != 0)) {
		struct avp_instrument *ins = v->instrument;
		uint8_t sam;

		if((v->tick_counter % 2) == 0) {
			sam = ins->sample_number;
		} else {
			sam = 2;
		}

		v->sample_number = sam;

		if((int32_t)((int32_t)v->period + (int32_t)ins->portamento_add) < 0x8000) {
			v->period += ins->portamento_add;
		}
	}
}

// [=]===^=[ avp_do_set_sample ]==================================================================[=]
static void avp_do_set_sample(struct activisionpro_state *s, struct avp_voice_info *v) {
	if(s->have_set_sample && ((v->enabled_effects_flag & 0x10) != 0)) {
		struct avp_instrument *ins = v->instrument;

		if(v->stop_reset_effect_delay != 0) {
			v->sample_number = ins->sample_number2;
		} else if(!v->stop_reset_effect) {
			v->sample_number = ins->sample_number;
		}
	}
}

// [=]===^=[ avp_do_arpeggio ]====================================================================[=]
static void avp_do_arpeggio(struct activisionpro_state *s, struct avp_voice_info *v) {
	if(s->have_arpeggio && ((v->enabled_effects_flag & 0x20) != 0)) {
		if(v->stop_reset_effect_delay != 0) {
			avp_set_arpeggio(v, v->tick_counter);
		} else if(!v->stop_reset_effect) {
			v->period = avp_get_period(v->note, v->transpose);
		}
	}
}

// [=]===^=[ avp_do_envelopes ]===================================================================[=]
static void avp_do_envelopes(struct activisionpro_state *s, struct avp_voice_info *v) {
	if(!s->have_envelope) {
		return;
	}

	if(((v->note_and_flag & 0x80) == 0) && (v->tick_counter == 0)) {
		struct avp_instrument *ins = v->instrument;
		if(ins && ins->envelope_number < s->num_envelopes) {
			struct avp_envelope *env = &s->envelopes[ins->envelope_number];

			v->envelope = env;
			v->envelope_loop_count = env->points[0].times_to_repeat;
			v->envelope_position = 0;
			v->envelope_wait_counter = 1;
			v->volume = 0;
		}
	}

	if(v->envelope_wait_counter >= 0) {
		v->envelope_wait_counter--;

		if(v->envelope_wait_counter == 0) {
			struct avp_envelope *env = v->envelope;
			if(!env) {
				return;
			}
			uint8_t position = v->envelope_position;

			int16_t volume = (int16_t)v->volume;
			volume += env->points[position].volume_increment;

			if(volume > 64) {
				volume = 64;
			} else if(volume < 0) {
				volume = 0;
			}

			v->volume = (uint16_t)volume;

			v->envelope_loop_count--;

			if(v->envelope_loop_count == 0) {
				position++;

				if(position > 5) {
					position = 5;
				}

				uint8_t ticks = env->points[position].ticks_to_wait;

				if(ticks >= 0xc0) {
					position = (uint8_t)((ticks & 0x3f) / 3);
					if(position > 5) {
						position = 5;
					}
					ticks = env->points[position].ticks_to_wait;
				}

				v->envelope_wait_counter = (int8_t)ticks;

				if(position != 5) {
					v->envelope_loop_count = env->points[position].times_to_repeat;
				}

				v->envelope_position = position;
			} else {
				v->envelope_wait_counter = (int8_t)env->points[position].ticks_to_wait;
			}
		}
	}
}

// [=]===^=[ avp_run_effects1 ]===================================================================[=]
static void avp_run_effects1(struct activisionpro_state *s, struct avp_voice_info *v) {
	if(s->portamento_vibrato_type == AVP_PT_ONLYONE) {
		if(v->portamento_value != 0) {
			avp_do_portamento(v);
		} else {
			avp_do_vibrato(s, v);
		}
	} else {
		avp_do_vibrato(s, v);

		if(v->portamento_value != 0) {
			avp_do_portamento(v);
		}
	}

	avp_do_set_note(s, v);
	avp_do_set_fixed_sample(s, v);

	if(s->have_set_arpeggio && ((v->enabled_effects_flag & 0x08) != 0)) {
		avp_set_arpeggio(v, v->tick_counter & 3);
	} else {
		avp_do_set_sample(s, v);
		avp_do_arpeggio(s, v);
	}

	avp_do_envelopes(s, v);
	avp_do_counters(v);
}

// [=]===^=[ avp_run_effects2 ]===================================================================[=]
static void avp_run_effects2(struct activisionpro_state *s, struct avp_voice_info *v) {
	avp_do_set_sample(s, v);
	avp_do_arpeggio(s, v);
	avp_do_envelopes(s, v);
	avp_do_counters(v);
}

// [=]===^=[ avp_parse_next_track_position ]======================================================[=]
static void avp_parse_next_track_position(struct activisionpro_state *s, int32_t channel_number, struct avp_voice_info *v) {
	if(v->track_number == 0xfe) {
		avp_stop_and_reset(s);
	}

	if(v->track_number == 0xff) {
		v->track_position = 0;
		avp_parse_next_position(s, v);

		if(v->track_number == 0xff) {
			return;
		}
	}

	if(v->track_number >= s->num_tracks || !s->tracks[v->track_number]) {
		return;
	}

	uint8_t *track = s->tracks[v->track_number];
	uint32_t track_len = s->track_lengths[v->track_number];

	if(s->reset_volume && (v->track_position == 0)) {
		v->track_volume = 64;
	}

	int8_t set_to_instrument_number = -1;

	if(v->track_position >= track_len) {
		return;
	}
	uint8_t track_byte = track[v->track_position];
	v->track_position++;

	if(s->parse_track_version == 1) {
		v->speed_counter = (uint8_t)(track_byte & 0x3f);
	}

	v->portamento_value = 0;
	v->stop_reset_effect = 0;
	v->mute = 0;

	if((s->parse_track_version == 4) || (s->parse_track_version == 5)) {
		v->note_and_flag = 0;
	}

	if((s->parse_track_version == 1) && ((track_byte & 0x40) != 0)) {
		v->envelope_wait_counter = 1;
		v->envelope_loop_count = 1;
		v->envelope_position = 3;
	}

	uint8_t one_more;

	do {
		one_more = 0;

		if((track_byte & 0x80) != 0) {
			switch(s->parse_track_version) {
				case 1: {
					if(v->track_position >= track_len) {
						return;
					}
					if((track[v->track_position] & 0x80) != 0) {
						v->portamento_value = track[v->track_position];
					} else {
						set_to_instrument_number = (int8_t)track[v->track_position];
					}
					v->track_position++;
					break;
				}

				case 2: {
					set_to_instrument_number = (int8_t)(track_byte & 0x7f);
					if(v->track_position >= track_len) {
						return;
					}
					if((track[v->track_position] & 0x80) != 0) {
						v->portamento_value = track[v->track_position];
					} else {
						v->track_volume = track[v->track_position];
					}
					v->track_position++;

					if(v->track_position >= track_len) {
						return;
					}
					track_byte = track[v->track_position];
					v->track_position++;
					break;
				}

				case 3: {
					if(v->track_position >= track_len) {
						return;
					}
					switch(track_byte) {
						case 0x80: {
							set_to_instrument_number = (int8_t)track[v->track_position];
							break;
						}

						case 0x81: {
							v->track_volume = track[v->track_position];
							break;
						}

						case 0x82: {
							v->portamento_value = track[v->track_position];
							break;
						}

						case 0x8a: {
							v->max_speed_counter = track[v->track_position];
							break;
						}

						case 0x8e: {
							v->track_volume += track[v->track_position];
							break;
						}
					}

					v->track_position++;
					if(v->track_position >= track_len) {
						return;
					}
					track_byte = track[v->track_position];
					v->track_position++;
					one_more = 1;
					break;
				}

				case 4: {
					one_more = 1;
					if(v->track_position >= track_len) {
						return;
					}
					switch(track_byte) {
						case 0x80: {
							set_to_instrument_number = (int8_t)track[v->track_position];
							break;
						}

						case 0x81: {
							v->mute = 1;
							v->note = 64;
							v->transpose = 0;
							one_more = 0;
							break;
						}

						case 0x82: {
							v->portamento_value = track[v->track_position];
							break;
						}

						case 0x83: {
							s->playing_info.speed_variation2_speed = track[v->track_position];
							break;
						}

						case 0x87: {
							v->note_and_flag = 0xff;
							break;
						}

						case 0x8a: {
							v->max_speed_counter = track[v->track_position];
							break;
						}

						case 0x8c: {
							v->track_volume = track[v->track_position];
							break;
						}

						case 0x8d: {
							v->track_volume += track[v->track_position];
							break;
						}
					}

					if(one_more) {
						v->track_position++;
						if(v->track_position >= track_len) {
							return;
						}
						track_byte = track[v->track_position];
						v->track_position++;
					}
					break;
				}

				case 5: {
					one_more = 1;
					if(v->track_position >= track_len) {
						return;
					}
					switch(track_byte) {
						case 0x80: {
							set_to_instrument_number = (int8_t)track[v->track_position];
							break;
						}

						case 0x81: {
							v->mute = 1;
							v->note = 64;
							v->transpose = 0;
							one_more = 0;
							break;
						}

						case 0x82: {
							v->portamento_value = track[v->track_position];
							break;
						}

						case 0x83: {
							s->playing_info.speed_variation2_speed = track[v->track_position];
							break;
						}

						case 0x84: {
							v->note_and_flag = 0xff;
							break;
						}

						case 0x85: {
							v->max_speed_counter = track[v->track_position];
							break;
						}

						case 0x86: {
							s->playing_info.global_transpose = (int8_t)track[v->track_position];
							break;
						}

						case 0x87: {
							v->track_volume = track[v->track_position];
							break;
						}

						case 0x8b: {
							v->track_volume += track[v->track_position];
							break;
						}
					}

					if(one_more) {
						v->track_position++;
						if(v->track_position >= track_len) {
							return;
						}
						track_byte = track[v->track_position];
						v->track_position++;
					}
					break;
				}
			}
		} else if((s->parse_track_version == 4) || (s->parse_track_version == 5)) {
			v->note = track_byte;
		}
	} while(one_more);

	if((s->parse_track_version == 4) || (s->parse_track_version == 5)) {
		if(v->track_position >= track_len) {
			return;
		}
		v->speed_counter = track[v->track_position];
		v->speed_counter2 = 0;
		v->track_position++;
	} else {
		if(s->parse_track_version != 1) {
			v->speed_counter = track_byte;
		}

		if(v->track_position >= track_len) {
			return;
		}
		v->note_and_flag = track[v->track_position];
		v->note = (uint8_t)(v->note_and_flag & 0x7f);
		v->track_position++;
	}

	if((set_to_instrument_number >= 0) && (set_to_instrument_number != v->instrument_number)) {
		v->instrument_number = set_to_instrument_number;
		v->note_and_flag = 0;
	}

	if(((v->note_and_flag & 0x80) == 0) && (v->instrument_number >= 0) && ((uint32_t)v->instrument_number < s->num_instruments)) {
		struct avp_instrument *ins = &s->instruments[v->instrument_number];

		v->instrument = ins;
		v->enabled_effects_flag = ins->enabled_effect_flags;
		v->stop_reset_effect_delay = ins->stop_reset_effect_delay;
		v->vibrato_delay = ins->vibrato_delay;
		v->transpose = ins->transpose;
		v->fine_tune = ins->fine_tune;
		v->sample_number = ins->sample_number;

		if(!s->have_envelope) {
			v->volume = ins->volume;
		}

		uint32_t sn = v->sample_number;
		if(sn < AVP_NUM_SAMPLES) {
			struct avp_sample *sm = &s->samples[sn];

			v->sample_data = sm->sample_data;
			v->sample_length = sm->length;
			v->sample_loop_start = sm->loop_start;
			v->sample_loop_length = sm->loop_length;

			paula_set_period(&s->paula, channel_number, 126);
			uint32_t play_len = (uint32_t)v->sample_length * 2u;
			paula_play_sample(&s->paula, channel_number, v->sample_data, ins->sample_start_offset + play_len);

			if(v->sample_loop_length > 1) {
				paula_set_loop(&s->paula, channel_number, v->sample_loop_start, (uint32_t)v->sample_loop_length * 2u);
			} else {
				paula_set_loop(&s->paula, channel_number, 0, 0);
			}
		}
	}

	v->note = (uint8_t)(v->note + s->playing_info.global_transpose);
	v->period = avp_get_period(v->note, v->transpose);

	if(v->track_position < track_len && track[v->track_position] == 0xff) {
		v->track_position = 0;

		v->loop_track_counter--;
		if(v->loop_track_counter == 0) {
			avp_parse_next_position(s, v);
		}
	}

	v->tick_counter = 0;
	v->vibrato_depth = 0;
}

// [=]===^=[ avp_tick ]===========================================================================[=]
static void avp_tick(struct activisionpro_state *s) {
	avp_do_master_volume_fade(s);
	avp_do_speed_variation2(s);

	for(int32_t i = 0; i < 4; i++) {
		struct avp_voice_info *v = &s->voices[i];

		if(s->playing_info.speed_variation_counter == 0) {
			v->speed_counter--;
			v->speed_counter2++;

			if(v->speed_counter == 0) {
				avp_parse_next_track_position(s, i, v);
				avp_run_effects2(s, v);
				continue;
			}
		}

		avp_run_effects1(s, v);
	}

	avp_do_speed_variation1(s);

	for(int32_t i = 0; i < 4; i++) {
		struct avp_voice_info *v = &s->voices[i];

		uint16_t period = (uint16_t)(v->period - v->fine_tune);
		paula_set_period(&s->paula, i, period);

		int32_t volume;

		if(s->have_envelope) {
			volume = ((int32_t)v->volume * (int32_t)s->playing_info.master_volume) / 64;
		} else {
			uint8_t mute_condition = v->mute
				|| ((s->parse_track_version != 5) && (v->speed_counter <= v->max_speed_counter))
				|| ((s->parse_track_version == 5) && (v->speed_counter2 >= v->max_speed_counter));
			if((s->speed_variation_version == 2) && mute_condition) {
				volume = 0;
			} else {
				volume = ((int32_t)v->track_volume * (int32_t)v->volume * (int32_t)s->playing_info.master_volume) / 4096;
			}
		}

		if(volume < 0) {
			volume = 0;
		}
		if(volume > 64) {
			volume = 64;
		}

		paula_set_volume(&s->paula, i, (uint16_t)volume);
	}
}

// [=]===^=[ avp_cleanup ]========================================================================[=]
static void avp_cleanup(struct activisionpro_state *s) {
	if(s->song_info_list) {
		for(uint32_t i = 0; i < s->num_sub_songs; i++) {
			for(uint32_t j = 0; j < 4; j++) {
				if(s->song_info_list[i].position_lists[j]) {
					free(s->song_info_list[i].position_lists[j]);
				}
			}
		}
		free(s->song_info_list);
		s->song_info_list = 0;
	}

	if(s->tracks) {
		for(uint32_t i = 0; i < s->num_tracks; i++) {
			if(s->tracks[i]) {
				free(s->tracks[i]);
			}
		}
		free(s->tracks);
		s->tracks = 0;
	}
	if(s->track_lengths) {
		free(s->track_lengths);
		s->track_lengths = 0;
	}

	if(s->envelopes) {
		free(s->envelopes);
		s->envelopes = 0;
	}

	if(s->instruments) {
		free(s->instruments);
		s->instruments = 0;
	}

	for(uint32_t i = 0; i < AVP_NUM_SAMPLES; i++) {
		if(s->samples[i].sample_data) {
			free(s->samples[i].sample_data);
			s->samples[i].sample_data = 0;
		}
	}
}

// [=]===^=[ activisionpro_init ]=================================================================[=]
static struct activisionpro_state *activisionpro_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 1024 || sample_rate < 8000) {
		return 0;
	}

	struct activisionpro_state *s = (struct activisionpro_state *)calloc(1, sizeof(struct activisionpro_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;

	if(!avp_test_module(s)) {
		free(s);
		return 0;
	}

	// speed_variation_speed_init (version 2 only).
	if(s->speed_variation_version == 2) {
		if(s->speed_variation_speed_increment_offset < 0 || (uint32_t)s->speed_variation_speed_increment_offset >= s->module_len) {
			free(s);
			return 0;
		}
		s->speed_variation_speed_init = s->module_data[s->speed_variation_speed_increment_offset];
	}

	if(!avp_load_sub_song_info(s)) {
		goto fail;
	}
	if(!avp_load_tracks(s)) {
		goto fail;
	}
	if(!avp_load_envelopes(s)) {
		goto fail;
	}
	if(!avp_load_instruments(s)) {
		goto fail;
	}
	if(!avp_load_sample_info(s)) {
		goto fail;
	}
	if(!avp_load_sample_data(s)) {
		goto fail;
	}

	paula_init(&s->paula, sample_rate, AVP_TICK_HZ);
	avp_initialize_sound(s, 0);
	return s;

fail:
	avp_cleanup(s);
	free(s);
	return 0;
}

// [=]===^=[ activisionpro_free ]=================================================================[=]
static void activisionpro_free(struct activisionpro_state *s) {
	if(!s) {
		return;
	}
	avp_cleanup(s);
	free(s);
}

// [=]===^=[ activisionpro_get_audio ]============================================================[=]
static void activisionpro_get_audio(struct activisionpro_state *s, int16_t *output, int32_t frames) {
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
			avp_tick(s);
		}
	}
}

// [=]===^=[ activisionpro_api_init ]=============================================================[=]
static void *activisionpro_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return activisionpro_init(data, len, sample_rate);
}

// [=]===^=[ activisionpro_api_free ]=============================================================[=]
static void activisionpro_api_free(void *state) {
	activisionpro_free((struct activisionpro_state *)state);
}

// [=]===^=[ activisionpro_api_get_audio ]========================================================[=]
static void activisionpro_api_get_audio(void *state, int16_t *output, int32_t frames) {
	activisionpro_get_audio((struct activisionpro_state *)state, output, frames);
}

static const char *activisionpro_extensions[] = { "avp", "mw", 0 };

static struct player_api activisionpro_api = {
	"Activision Pro",
	activisionpro_extensions,
	activisionpro_api_init,
	activisionpro_api_free,
	activisionpro_api_get_audio,
	0,
};
