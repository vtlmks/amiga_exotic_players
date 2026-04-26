// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Ben Daglish replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct bendaglish_state *bendaglish_init(void *data, uint32_t len, int32_t sample_rate);
//   void bendaglish_free(struct bendaglish_state *s);
//   void bendaglish_get_audio(struct bendaglish_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define BENDAGLISH_TICK_HZ        50
#define BENDAGLISH_FINETUNE_START 48
#define BENDAGLISH_MAX_SUBSONGS   64
#define BENDAGLISH_MAX_POSLISTS   256

struct bendaglish_features {
	int32_t master_volume_fade_version; // -1 = none
	uint8_t set_dma_in_sample_handlers;
	uint8_t enable_counter;
	uint8_t enable_sample_effects;
	uint8_t enable_final_volume_slide;
	uint8_t enable_volume_fade;
	uint8_t enable_portamento;
	uint8_t check_for_ticks;
	uint8_t extra_tick_arg;
	uint8_t uses_9x_track_effects;
	uint8_t uses_cx_track_effects;

	uint8_t max_track_value;
	uint8_t enable_c0_track_loop;
	uint8_t enable_f0_track_loop;

	uint8_t max_sample_mapping_value;
	int32_t get_sample_mapping_version;
	int32_t set_sample_mapping_version;
};

struct bendaglish_song_info {
	uint16_t position_lists[4];
};

struct bendaglish_position_list {
	uint16_t key;          // offset key from sub-song record
	uint8_t *bytes;
	uint32_t length;
};

struct bendaglish_sample {
	int16_t sample_number;
	int8_t *sample_data;   // ref into module
	uint16_t length;       // in words (multiply by 2 for bytes)
	uint32_t loop_offset;
	uint16_t loop_length;  // in words

	uint16_t volume;
	int16_t volume_fade_speed;

	int16_t portamento_duration;
	int16_t portamento_add_value;

	uint16_t vibrato_depth;
	uint16_t vibrato_add_value;

	int16_t note_transpose;
	uint16_t fine_tune_period;
};

enum {
	BENDAGLISH_CB_PLAY_ONCE   = 0,
	BENDAGLISH_CB_LOOP        = 1,
	BENDAGLISH_CB_VOLUME_FADE = 2,
};

struct bendaglish_voice {
	uint8_t channel_enabled;

	uint8_t *position_list;
	uint32_t position_list_len;
	int32_t current_position;
	int32_t next_position;

	int32_t playing_track;
	uint8_t *track;
	uint32_t track_len;
	int32_t next_track_position;

	uint8_t switch_to_next_position;
	uint8_t track_loop_counter;
	uint8_t ticks_left_for_next_track_command;

	int8_t transpose;
	uint8_t transposed_note;
	uint8_t previous_transposed_note;
	uint8_t use_new_note;

	uint8_t portamento_1_enabled;
	uint8_t portamento_2_enabled;
	uint8_t portamento_start_delay;
	uint8_t portamento_duration;
	int8_t portamento_delta_note_number;

	uint8_t portamento_control_flag;
	uint8_t portamento_start_delay_counter;
	uint8_t portamento_duration_counter;
	int32_t portamento_add_value;

	uint8_t volume_fade_enabled;
	uint8_t volume_fade_init_speed;
	uint8_t volume_fade_duration;
	int16_t volume_fade_init_add_value;

	uint8_t volume_fade_running;
	uint8_t volume_fade_speed;
	uint8_t volume_fade_speed_counter;
	uint8_t volume_fade_duration_counter;
	int16_t volume_fade_add_value;
	int16_t volume_fade_value;

	uint16_t channel_volume;
	uint16_t channel_volume_slide_speed;
	int16_t channel_volume_slide_add_value;

	struct bendaglish_sample *sample_info;
	struct bendaglish_sample *sample_info2;

	uint8_t sample_mapping[10];
};

struct bendaglish_voice_playback {
	struct bendaglish_sample *playing_sample;
	uint8_t sample_play_ticks_counter;

	uint16_t note_period;
	int16_t final_volume;

	uint16_t final_volume_slide_speed;
	uint16_t final_volume_slide_speed_counter;
	int16_t final_volume_slide_add_value;

	uint16_t loop_delay_counter;

	int16_t portamento_add_value;

	int16_t sample_portamento_duration;
	int16_t sample_portamento_add_value;

	uint16_t sample_vibrato_depth;
	int16_t sample_vibrato_add_value;

	int16_t sample_period_add_value;

	uint8_t handle_sample_callback;

	uint8_t dma_enabled;
	int16_t sample_number;
	int8_t *sample_data;
	uint16_t sample_length;
};

struct bendaglish_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	int32_t sub_song_list_offset;
	int32_t track_offset_table_offset;
	int32_t tracks_offset;
	int32_t sample_info_offset_table_offset;

	struct bendaglish_features features;

	struct bendaglish_song_info *sub_songs;
	uint32_t num_sub_songs;

	struct bendaglish_position_list *position_lists;
	uint32_t num_position_lists;

	uint8_t **tracks;
	uint32_t *track_lens;
	uint32_t num_tracks;

	struct bendaglish_sample *samples;
	uint32_t num_samples;

	uint8_t enable_playing;
	int16_t master_volume;
	int16_t master_volume_fade_speed;
	int16_t master_volume_fade_speed_counter;
	uint16_t counter;

	struct bendaglish_voice voices[4];
	struct bendaglish_voice_playback playback[4];

	uint32_t selected_sub_song;
};

// Empty sample reference data for stopping playback (8-bit PCM zeros).
static int8_t bendaglish_empty_sample[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// [=]===^=[ bendaglish_fine_tune ]===============================================================[=]
// 32-bit period table (16.16 fixed point). Indexed via FineTuneStartIndex == 48.
static int32_t bendaglish_fine_tune[10 * 12] = {
	0x00001000, 0x000010f3, 0x000011f5, 0x00001306, 0x00001429, 0x0000155b, 0x000016a0, 0x000017f9, 0x00001965, 0x00001ae9, 0x00001c82, 0x00001e34,
	0x00002000, 0x000021e7, 0x000023eb, 0x0000260d, 0x00002851, 0x00002ab7, 0x00002d41, 0x00002ff2, 0x000032cc, 0x000035d1, 0x00003904, 0x00003c68,
	0x00004000, 0x000043ce, 0x000047d6, 0x00004c1b, 0x000050a2, 0x0000556e, 0x00005a82, 0x00005fe4, 0x00006597, 0x00006ba2, 0x00007208, 0x000078d0,
	0x00008000, 0x0000879c, 0x00008fac, 0x00009837, 0x0000a145, 0x0000aadc, 0x0000b504, 0x0000bfc8, 0x0000cb2f, 0x0000d744, 0x0000e411, 0x0000f1a1,
	0x00010000, 0x00010f38, 0x00011f59, 0x0001306f, 0x0001428a, 0x000155b8, 0x00016a09, 0x00017f91, 0x0001965f, 0x0001ae89, 0x0001c823, 0x0001e343,
	0x00020000, 0x00021e71, 0x00023eb3, 0x000260df, 0x00028514, 0x0002ab70, 0x0002d413, 0x0002ff22, 0x00032cbf, 0x00035d13, 0x00039047, 0x0003c686,
	0x00040000, 0x00043ce3, 0x00047d66, 0x0004c1bf, 0x00050a28, 0x000556e0, 0x0005a827, 0x0005fe44, 0x0006597f, 0x0006ba27, 0x0007208f, 0x00078d0d,
	0x00080000, 0x000879c7, 0x0008facd, 0x0009837f, 0x000a1451, 0x000aadc0, 0x000b504f, 0x000bfc88, 0x000cb2ff, 0x000d7450, 0x000e411f, 0x000f1a1b,
	0x00100000, 0x0010f38f, 0x0011f59a, 0x001307b2, 0x001428a2, 0x00155b81, 0x0016a09e, 0x0017f910, 0x001965fe, 0x001ae8a0, 0x001c823e, 0x001e3438,
	0x00200000, 0x0021e71f, 0x0023eb35, 0x00260dfc, 0x00285145, 0x002ab702, 0x002d413c, 0x002ff221, 0x0032cbfd, 0x0035d13f, 0x0039047c, 0x003c6870,
};

// [=]===^=[ bendaglish_read_u32_be ]=============================================================[=]
static uint32_t bendaglish_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ bendaglish_read_i32_be ]=============================================================[=]
static int32_t bendaglish_read_i32_be(uint8_t *p) {
	return (int32_t)bendaglish_read_u32_be(p);
}

// [=]===^=[ bendaglish_read_u16_be ]=============================================================[=]
static uint16_t bendaglish_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ bendaglish_read_i16_be ]=============================================================[=]
static int16_t bendaglish_read_i16_be(uint8_t *p) {
	return (int16_t)bendaglish_read_u16_be(p);
}

// [=]===^=[ bendaglish_period_for_index ]========================================================[=]
// Compute period using 32-bit FineTune[i] * fine_tune_period >> 16 with 32-bit overflow semantics.
static uint16_t bendaglish_period_for_index(int32_t index, uint16_t fine_tune_period) {
	if((index < 0) || (index >= (int32_t)(sizeof(bendaglish_fine_tune) / sizeof(bendaglish_fine_tune[0])))) {
		return 0;
	}
	uint32_t ft = (uint32_t)bendaglish_fine_tune[index];
	uint32_t lo = (ft & 0xffff) * (uint32_t)fine_tune_period;
	uint32_t hi = (ft >> 16) * (uint32_t)fine_tune_period;
	uint32_t res = (lo >> 16) + hi;
	return (uint16_t)(res & 0xffff);
}

// [=]===^=[ bendaglish_extract_init_info ]=======================================================[=]
static int32_t bendaglish_extract_init_info(struct bendaglish_state *s, uint8_t *buf, int32_t len, int32_t start_of_init) {
	int32_t index;

	for(index = start_of_init; index < (len - 6); index += 2) {
		if((buf[index] == 0x41) && (buf[index + 1] == 0xfa) && (buf[index + 4] == 0x22) && (buf[index + 5] == 0x08)) {
			break;
		}
	}
	if(index >= (len - 6)) {
		return 0;
	}

	s->sub_song_list_offset = (int32_t)((int8_t)buf[index + 2] << 8 | buf[index + 3]) + index + 2;
	index += 4;

	for(; index < (len - 6); index += 2) {
		if((buf[index] == 0x41) && (buf[index + 1] == 0xfa) && (buf[index + 4] == 0x23) && (buf[index + 5] == 0x48)) {
			break;
		}
	}
	if(index >= (len - 6)) {
		return 0;
	}

	s->sample_info_offset_table_offset = (int32_t)((int8_t)buf[index + 2] << 8 | buf[index + 3]) + index + 2;
	return 1;
}

// [=]===^=[ bendaglish_extract_play_info ]=======================================================[=]
static int32_t bendaglish_extract_play_info(struct bendaglish_state *s, uint8_t *buf, int32_t len, int32_t start_of_play) {
	int32_t index;

	for(index = start_of_play; index < (len - 6); index += 2) {
		if((buf[index] == 0x47) && (buf[index + 1] == 0xfa) &&
		   (((buf[index + 4] == 0x48) && (buf[index + 5] == 0x80)) ||
		    ((buf[index + 4] == 0xd0) && (buf[index + 5] == 0x40)))) {
			break;
		}
	}
	if(index >= (len - 6)) {
		return 0;
	}

	s->track_offset_table_offset = (int32_t)((int8_t)buf[index + 2] << 8 | buf[index + 3]) + index + 2;
	index += 4;

	for(; index < (len - 6); index += 2) {
		if((buf[index] == 0x47) && (buf[index + 1] == 0xfa) && (buf[index + 4] == 0xd6) && (buf[index + 5] == 0xc0)) {
			break;
		}
	}
	if(index >= (len - 6)) {
		return 0;
	}

	s->tracks_offset = (int32_t)((int8_t)buf[index + 2] << 8 | buf[index + 3]) + index + 2;
	return 1;
}

// [=]===^=[ bendaglish_find_features_play ]======================================================[=]
static int32_t bendaglish_find_features_play(struct bendaglish_state *s, uint8_t *buf, int32_t len, int32_t start_of_play) {
	struct bendaglish_features *f = &s->features;
	int32_t index;

	f->enable_counter = 0;
	if(start_of_play >= (len - 16)) {
		return 0;
	}

	if((buf[start_of_play + 4] == 0x10) && (buf[start_of_play + 5] == 0x3a) && (buf[start_of_play + 8] == 0x67) &&
	   (buf[start_of_play + 14] == 0x53) && (buf[start_of_play + 15] == 0x50)) {
		index = (int32_t)((int8_t)buf[start_of_play + 6] << 8 | buf[start_of_play + 7]) + start_of_play + 6;
		if(index >= len) {
			return 0;
		}
		f->enable_counter = (buf[index] != 0) ? 1 : 0;
	}

	f->enable_portamento = 0;
	f->enable_volume_fade = 0;

	for(index = start_of_play; index < (len - 2); index += 2) {
		if((buf[index] == 0x53) && (buf[index + 1] == 0x2c)) {
			break;
		}
	}
	if(index >= (len - 2)) {
		return 0;
	}

	for(; index >= start_of_play; index -= 2) {
		if((buf[index] == 0x49) && (buf[index + 1] == 0xfa)) {
			break;
		}
		if((buf[index] == 0x61) && (buf[index + 1] == 0x00)) {
			int32_t method_index = (int32_t)((int8_t)buf[index + 2] << 8 | buf[index + 3]) + index + 2;
			if(method_index >= (len - 14)) {
				return 0;
			}
			if((buf[method_index] == 0x4a) && (buf[method_index + 1] == 0x2c) && (buf[method_index + 4] == 0x67) &&
			   (buf[method_index + 6] == 0x6a) && (buf[method_index + 8] == 0x30) && (buf[method_index + 9] == 0x29)) {
				f->enable_portamento = 1;
			} else if((buf[method_index] == 0x4a) && (buf[method_index + 1] == 0x2c) && (buf[method_index + 4] == 0x67) &&
			          (buf[method_index + 6] == 0x4a) && (buf[method_index + 7] == 0x2c) && (buf[method_index + 10] == 0x67)) {
				f->enable_volume_fade = 1;
			} else {
				return 0;
			}
		}
	}

	f->max_track_value = 0x80;

	for(index = start_of_play; index < (len - 6); index += 2) {
		if((buf[index] == 0x10) && (buf[index + 1] == 0x1b)) {
			break;
		}
	}
	if(index >= (len - 6)) {
		return 0;
	}

	if(((buf[index + 2] == 0xb0) && (buf[index + 3] == 0x3c)) ||
	   ((buf[index + 2] == 0x0c) && (buf[index + 3] == 0x00))) {
		f->max_track_value = buf[index + 5];
	}

	for(index += 4; index < (len - 6); index += 2) {
		if((((buf[index] == 0xb0) && (buf[index + 1] == 0x3c)) ||
		    ((buf[index] == 0x0c) && (buf[index + 1] == 0x00))) && (buf[index + 4] == 0x6c)) {
			break;
		}
	}
	if(index >= (len - 6)) {
		return 0;
	}

	int32_t effect = ((int32_t)buf[index + 2] << 8) | buf[index + 3];
	f->enable_c0_track_loop = (effect == 0x00c0) ? 1 : 0;
	f->enable_f0_track_loop = (effect == 0x00f0) ? 1 : 0;

	index = buf[index + 5] + index + 6;
	if(index >= (len - 4)) {
		return 0;
	}

	if((buf[index] == 0x02) && (buf[index + 1] == 0x40)) {
		f->set_sample_mapping_version = 1;
	} else if((buf[index] == 0x04) && (buf[index + 1] == 0x00)) {
		f->set_sample_mapping_version = 2;
	} else {
		return 0;
	}

	return 1;
}

// [=]===^=[ bendaglish_find_features_handle_effects ]============================================[=]
static int32_t bendaglish_find_features_handle_effects(struct bendaglish_state *s, uint8_t *buf, int32_t len, int32_t start_of_play) {
	struct bendaglish_features *f = &s->features;
	int32_t index;

	if((buf[start_of_play] != 0x61) || (buf[start_of_play + 1] != 0x00)) {
		return 0;
	}

	int32_t start_of_handle_effects = (int32_t)((int8_t)buf[start_of_play + 2] << 8 | buf[start_of_play + 3]) + start_of_play + 2;

	for(index = start_of_handle_effects; index < (len - 2); index += 2) {
		if((buf[index] == 0x4e) && (buf[index + 1] == 0x90)) {
			break;
		}
	}
	if(index >= (len - 2)) {
		return 0;
	}

	int32_t callback_index = index;

	f->enable_sample_effects = 0;
	f->enable_final_volume_slide = 0;

	for(; index >= start_of_handle_effects; index -= 2) {
		if((buf[index] == 0x4e) && (buf[index + 1] == 0x75)) {
			break;
		}
		if((buf[index] == 0x61) && (buf[index + 1] == 0x00)) {
			int32_t method_index = (int32_t)((int8_t)buf[index + 2] << 8 | buf[index + 3]) + index + 2;
			if(method_index >= (len - 14)) {
				return 0;
			}
			if((buf[method_index] == 0x30) && (buf[method_index + 1] == 0x2b) && (buf[method_index + 4] == 0x67) &&
			   (((buf[method_index + 6] == 0xb0) && (buf[method_index + 7] == 0x7c)) ||
			    ((buf[method_index + 6] == 0x0c) && (buf[method_index + 7] == 0x40))) &&
			   (buf[method_index + 8] == 0xff) && (buf[method_index + 9] == 0xff)) {
				f->enable_sample_effects = 1;
			} else if((buf[method_index] == 0x30) && (buf[method_index + 1] == 0x2b) && (buf[method_index + 4] == 0x67) &&
			          (buf[method_index + 6] == 0x53) && (buf[method_index + 7] == 0x6b)) {
				f->enable_final_volume_slide = 1;
			} else {
				return 0;
			}
		}
	}

	if((buf[callback_index + 6] != 0x6e) && (buf[callback_index + 6] != 0x66)) {
		return 0;
	}

	index = buf[callback_index + 7] + callback_index + 8;
	if(index >= (len - 6)) {
		return 0;
	}

	f->set_dma_in_sample_handlers = 1;

	for(; index < len; ++index) {
		if((buf[index] == 0x4e) && (buf[index + 1] == 0x75)) {
			break;
		}
	}
	if(index >= len) {
		return 0;
	}

	if((buf[index - 2] == 0x00) && (buf[index - 1] == 0x96)) {
		f->set_dma_in_sample_handlers = 0;
	}

	if((buf[start_of_handle_effects] == 0x61) && (buf[start_of_handle_effects + 1] == 0x00)) {
		index = (int32_t)((int8_t)buf[start_of_handle_effects + 2] << 8 | buf[start_of_handle_effects + 3]) + start_of_handle_effects + 2;
		if(index >= (len - 24)) {
			return 0;
		}

		f->master_volume_fade_version = -1;

		if((buf[index] == 0x30) && (buf[index + 1] == 0x3a) && (buf[index + 4] == 0x67) && (buf[index + 5] == 0) &&
		   (buf[index + 8] == 0x41) && (buf[index + 9] == 0xfa) && (buf[index + 18] == 0x30) && (buf[index + 19] == 0x80)) {
			f->master_volume_fade_version = 1;
		} else if((buf[index] == 0x30) && (buf[index + 1] == 0x39) && (buf[index + 6] == 0x67) && (buf[index + 7] == 0) &&
		          (buf[index + 10] == 0x41) && (buf[index + 11] == 0xf9) && (buf[index + 22] == 0x30) && (buf[index + 23] == 0x80)) {
			f->master_volume_fade_version = 1;
		} else if((buf[index] == 0x10) && (buf[index + 1] == 0x3a) && (buf[index + 4] == 0x67) && (buf[index + 5] == 0) &&
		          (buf[index + 8] == 0x41) && (buf[index + 9] == 0xfa) && (buf[index + 18] == 0x53) && (buf[index + 19] == 0x00)) {
			f->master_volume_fade_version = 2;
		} else {
			return 0;
		}
	}

	return 1;
}

// [=]===^=[ bendaglish_find_features_parse_track_effect ]========================================[=]
static int32_t bendaglish_find_features_parse_track_effect(struct bendaglish_state *s, uint8_t *buf, int32_t len, int32_t start_of_method) {
	struct bendaglish_features *f = &s->features;
	int32_t index;

	if(((buf[start_of_method + 2] != 0xb0) || (buf[start_of_method + 3] != 0x3c)) &&
	   ((buf[start_of_method + 2] != 0x0c) || (buf[start_of_method + 3] != 0x00))) {
		return 0;
	}

	f->max_sample_mapping_value = buf[start_of_method + 5];
	index = start_of_method + 8;
	if(index >= (len - 4)) {
		return 0;
	}

	if((buf[index] != 0x02) || (buf[index + 1] != 0x40) || (buf[index + 2] != 0x00)) {
		return 0;
	}

	if(buf[index + 3] == 0x07) {
		f->get_sample_mapping_version = 1;
	} else if(buf[index + 3] == 0xff) {
		f->get_sample_mapping_version = 2;
	} else {
		return 1;
	}

	for(index += 4; index < (len - 6); index += 2) {
		if((((buf[index] == 0xb0) && (buf[index + 1] == 0x3c)) ||
		    ((buf[index] == 0x0c) && (buf[index + 1] == 0x00))) && (buf[index + 4] == 0x6c)) {
			break;
		}
	}
	if(index >= (len - 6)) {
		return 0;
	}

	f->uses_9x_track_effects = ((buf[index + 3] & 0xf0) == 0x90) ? 1 : 0;
	f->uses_cx_track_effects = ((buf[index + 3] & 0xf0) == 0xc0) ? 1 : 0;
	return 1;
}

// [=]===^=[ bendaglish_find_features_parse_track ]===============================================[=]
static int32_t bendaglish_find_features_parse_track(struct bendaglish_state *s, uint8_t *buf, int32_t len, int32_t start_of_play) {
	struct bendaglish_features *f = &s->features;
	int32_t index;

	for(index = start_of_play; index < (len - 4); index += 2) {
		if((buf[index] == 0x60) && (buf[index + 1] == 0x00)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	index = (int32_t)((int8_t)buf[index + 2] << 8 | buf[index + 3]) + index + 2;
	if(index >= len) {
		return 0;
	}

	int32_t start_of_parse_track = index;

	for(; index < (len - 8); index += 2) {
		if((buf[index] == 0x4a) && (buf[index + 1] == 0x2c) && (buf[index + 4] == 0x67)) {
			break;
		}
	}
	if(index >= (len - 8)) {
		return 0;
	}

	f->check_for_ticks = ((buf[index + 6] == 0x4a) && (buf[index + 7] == 0x2c)) ? 1 : 0;

	for(index += 8; index < (len - 6); index += 2) {
		if((buf[index] == 0x72) && (buf[index + 1] == 0x00) && (buf[index + 2] == 0x12) && (buf[index + 3] == 0x1b)) {
			break;
		}
	}
	if(index >= (len - 6)) {
		return 0;
	}

	f->extra_tick_arg = (buf[index + 4] == 0x66) ? 1 : 0;

	for(index = start_of_parse_track; index < (len - 4); index += 2) {
		if((buf[index] == 0x61) && (buf[index + 1] == 0x00)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	index = (int32_t)((int8_t)buf[index + 2] << 8 | buf[index + 3]) + index + 2;
	if(index >= len) {
		return 0;
	}

	return bendaglish_find_features_parse_track_effect(s, buf, len, index);
}

// [=]===^=[ bendaglish_find_features ]===========================================================[=]
static int32_t bendaglish_find_features(struct bendaglish_state *s, uint8_t *buf, int32_t len, int32_t start_of_play) {
	memset(&s->features, 0, sizeof(s->features));
	if(!bendaglish_find_features_play(s, buf, len, start_of_play)) {
		return 0;
	}
	if(!bendaglish_find_features_handle_effects(s, buf, len, start_of_play)) {
		return 0;
	}
	if(!bendaglish_find_features_parse_track(s, buf, len, start_of_play)) {
		return 0;
	}
	return 1;
}

// [=]===^=[ bendaglish_test_module ]=============================================================[=]
static int32_t bendaglish_test_module(struct bendaglish_state *s, uint8_t *buf, int32_t len) {
	if(len < 0x1600) {
		return 0;
	}

	int32_t scan_len = (len < 0x3000) ? len : 0x3000;

	if((buf[0] != 0x60) || (buf[1] != 0x00) || (buf[4] != 0x60) || (buf[5] != 0x00) ||
	   (buf[10] != 0x60) || (buf[11] != 0x00)) {
		return 0;
	}

	int32_t start_of_init = (int32_t)((int8_t)buf[2] << 8 | buf[3]) + 2;
	if(start_of_init >= (scan_len - 14)) {
		return 0;
	}

	if((buf[start_of_init] != 0x3f) || (buf[start_of_init + 1] != 0x00) ||
	   (buf[start_of_init + 2] != 0x61) || (buf[start_of_init + 3] != 0x00) ||
	   (buf[start_of_init + 6] != 0x3d) || (buf[start_of_init + 7] != 0x7c) ||
	   (buf[start_of_init + 12] != 0x41) || (buf[start_of_init + 13] != 0xfa)) {
		return 0;
	}

	int32_t start_of_play = (int32_t)((int8_t)buf[6] << 8 | buf[7]) + 4 + 2;
	if(start_of_play >= scan_len) {
		return 0;
	}

	if(!bendaglish_extract_init_info(s, buf, scan_len, start_of_init)) {
		return 0;
	}
	if(!bendaglish_extract_play_info(s, buf, scan_len, start_of_play)) {
		return 0;
	}
	if(!bendaglish_find_features(s, buf, scan_len, start_of_play)) {
		return 0;
	}
	return 1;
}

// [=]===^=[ bendaglish_find_position_command_arg_count ]=========================================[=]
static int32_t bendaglish_find_position_command_arg_count(struct bendaglish_state *s, uint8_t cmd) {
	if(cmd < s->features.max_track_value) {
		return 0;
	}

	if(s->features.enable_c0_track_loop) {
		if(cmd < 0xa0) {
			return 0;
		}
		if(cmd < 0xc8) {
			return 1;
		}
	}

	if(s->features.enable_f0_track_loop) {
		if(cmd < 0xf0) {
			return 0;
		}
		if(cmd < 0xf8) {
			return 1;
		}
	}

	if((cmd == 0xfd) && (s->features.master_volume_fade_version > 0)) {
		return 1;
	}
	if(cmd == 0xfe) {
		return 1;
	}
	return -1;
}

// [=]===^=[ bendaglish_find_track_command_arg_count ]============================================[=]
static int32_t bendaglish_find_track_command_arg_count(struct bendaglish_state *s, uint8_t cmd, uint8_t next_byte) {
	struct bendaglish_features *f = &s->features;

	if(cmd < 0x7f) {
		if(f->extra_tick_arg && (next_byte == 0)) {
			return 2;
		}
		return 1;
	}
	if(cmd == 0x7f) {
		return 1;
	}
	if(cmd <= f->max_sample_mapping_value) {
		return 0;
	}
	if((f->uses_cx_track_effects && (cmd < 0xc0)) ||
	   (f->uses_9x_track_effects && (cmd < 0x9b))) {
		return 0;
	}

	if((f->uses_cx_track_effects && (cmd == 0xc0) && f->enable_portamento) ||
	   (f->uses_9x_track_effects && (cmd == 0x9b) && f->enable_portamento)) {
		return 3;
	}
	if((f->uses_cx_track_effects && (cmd == 0xc1) && f->enable_portamento) ||
	   (f->uses_9x_track_effects && (cmd == 0x9c) && f->enable_portamento)) {
		return 0;
	}
	if((f->uses_cx_track_effects && (cmd == 0xc2) && f->enable_volume_fade) ||
	   (f->uses_9x_track_effects && (cmd == 0x9d) && f->enable_volume_fade)) {
		return 3;
	}
	if((f->uses_cx_track_effects && (cmd == 0xc3) && f->enable_volume_fade) ||
	   (f->uses_9x_track_effects && (cmd == 0x9e) && f->enable_volume_fade)) {
		return 0;
	}
	if((f->uses_cx_track_effects && (cmd == 0xc4) && f->enable_portamento) ||
	   (f->uses_9x_track_effects && (cmd == 0x9f) && f->enable_portamento)) {
		return 1;
	}
	if((f->uses_cx_track_effects && (cmd == 0xc5) && f->enable_portamento) ||
	   (f->uses_9x_track_effects && (cmd == 0xa0) && f->enable_portamento)) {
		return 0;
	}
	if((f->uses_cx_track_effects && (cmd == 0xc6) && f->enable_volume_fade) ||
	   (f->uses_9x_track_effects && (cmd == 0xa1) && f->enable_volume_fade)) {
		return f->enable_final_volume_slide ? 3 : 1;
	}
	if((f->uses_cx_track_effects && (cmd == 0xc7) && f->enable_final_volume_slide) ||
	   (f->uses_9x_track_effects && (cmd == 0xa2) && f->enable_final_volume_slide)) {
		return 0;
	}
	return -1;
}

// [=]===^=[ bendaglish_load_sub_song_info ]======================================================[=]
static int32_t bendaglish_load_sub_song_info(struct bendaglish_state *s) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	uint32_t pos = (uint32_t)s->sub_song_list_offset;
	uint32_t first_position_list = 0xffffffffu;

	struct bendaglish_song_info tmp[BENDAGLISH_MAX_SUBSONGS];
	uint32_t count = 0;

	while(count < BENDAGLISH_MAX_SUBSONGS) {
		if((pos + 8) > data_len) {
			return 0;
		}
		struct bendaglish_song_info *song = &tmp[count++];
		for(int32_t i = 0; i < 4; ++i) {
			song->position_lists[i] = bendaglish_read_u16_be(data + pos + i * 2);
			if(song->position_lists[i] < first_position_list) {
				first_position_list = song->position_lists[i];
			}
		}
		pos += 8;

		if(pos >= ((uint32_t)s->sub_song_list_offset + first_position_list)) {
			break;
		}
	}

	if(count == 0) {
		return 0;
	}

	s->sub_songs = (struct bendaglish_song_info *)calloc(count, sizeof(struct bendaglish_song_info));
	if(!s->sub_songs) {
		return 0;
	}
	memcpy(s->sub_songs, tmp, count * sizeof(struct bendaglish_song_info));
	s->num_sub_songs = count;
	return 1;
}

// [=]===^=[ bendaglish_load_single_position_list ]===============================================[=]
static int32_t bendaglish_load_single_position_list(struct bendaglish_state *s, uint32_t pos, uint8_t **out_bytes, uint32_t *out_len) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	uint32_t capacity = 64;
	uint8_t *bytes = (uint8_t *)malloc(capacity);
	if(!bytes) {
		return 0;
	}
	uint32_t n = 0;

	for(;;) {
		if(pos >= data_len) {
			free(bytes);
			return 0;
		}
		if(n == capacity) {
			capacity *= 2;
			uint8_t *nb = (uint8_t *)realloc(bytes, capacity);
			if(!nb) {
				free(bytes);
				return 0;
			}
			bytes = nb;
		}
		uint8_t cmd = data[pos++];
		bytes[n++] = cmd;

		if(cmd == 0xff) {
			break;
		}

		int32_t arg_count = bendaglish_find_position_command_arg_count(s, cmd);
		if(arg_count == -1) {
			free(bytes);
			return 0;
		}

		while(arg_count > 0) {
			if(pos >= data_len) {
				free(bytes);
				return 0;
			}
			if(n == capacity) {
				capacity *= 2;
				uint8_t *nb = (uint8_t *)realloc(bytes, capacity);
				if(!nb) {
					free(bytes);
					return 0;
				}
				bytes = nb;
			}
			bytes[n++] = data[pos++];
			--arg_count;
		}
	}

	*out_bytes = bytes;
	*out_len = n;
	return 1;
}

// [=]===^=[ bendaglish_find_position_list ]======================================================[=]
static struct bendaglish_position_list *bendaglish_find_position_list(struct bendaglish_state *s, uint16_t key) {
	for(uint32_t i = 0; i < s->num_position_lists; ++i) {
		if(s->position_lists[i].key == key) {
			return &s->position_lists[i];
		}
	}
	return 0;
}

// [=]===^=[ bendaglish_load_position_lists ]=====================================================[=]
static int32_t bendaglish_load_position_lists(struct bendaglish_state *s) {
	s->position_lists = (struct bendaglish_position_list *)calloc(BENDAGLISH_MAX_POSLISTS, sizeof(struct bendaglish_position_list));
	if(!s->position_lists) {
		return 0;
	}
	s->num_position_lists = 0;

	for(uint32_t si = 0; si < s->num_sub_songs; ++si) {
		struct bendaglish_song_info *song = &s->sub_songs[si];
		for(int32_t i = 0; i < 4; ++i) {
			uint16_t key = song->position_lists[i];
			if(bendaglish_find_position_list(s, key)) {
				continue;
			}
			if(s->num_position_lists >= BENDAGLISH_MAX_POSLISTS) {
				return 0;
			}
			uint32_t pos = (uint32_t)s->sub_song_list_offset + key;
			uint8_t *bytes = 0;
			uint32_t blen = 0;
			if(!bendaglish_load_single_position_list(s, pos, &bytes, &blen)) {
				return 0;
			}
			struct bendaglish_position_list *pl = &s->position_lists[s->num_position_lists++];
			pl->key = key;
			pl->bytes = bytes;
			pl->length = blen;
		}
	}
	return 1;
}

// [=]===^=[ bendaglish_load_single_track ]=======================================================[=]
static int32_t bendaglish_load_single_track(struct bendaglish_state *s, uint32_t pos, uint8_t **out_bytes, uint32_t *out_len) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	uint32_t capacity = 64;
	uint8_t *bytes = (uint8_t *)malloc(capacity);
	if(!bytes) {
		return 0;
	}
	uint32_t n = 0;

	for(;;) {
		if(pos >= data_len) {
			free(bytes);
			return 0;
		}
		if(n == capacity) {
			capacity *= 2;
			uint8_t *nb = (uint8_t *)realloc(bytes, capacity);
			if(!nb) {
				free(bytes);
				return 0;
			}
			bytes = nb;
		}
		uint8_t cmd = data[pos++];
		bytes[n++] = cmd;

		if(cmd == 0xff) {
			break;
		}

		if(pos >= data_len) {
			free(bytes);
			return 0;
		}
		uint8_t next_byte = data[pos++];

		int32_t arg_count = bendaglish_find_track_command_arg_count(s, cmd, next_byte);
		if(arg_count == -1) {
			free(bytes);
			return 0;
		}

		if(arg_count > 0) {
			if(n == capacity) {
				capacity *= 2;
				uint8_t *nb = (uint8_t *)realloc(bytes, capacity);
				if(!nb) {
					free(bytes);
					return 0;
				}
				bytes = nb;
			}
			bytes[n++] = next_byte;

			for(arg_count--; arg_count > 0; --arg_count) {
				if(pos >= data_len) {
					free(bytes);
					return 0;
				}
				if(n == capacity) {
					capacity *= 2;
					uint8_t *nb = (uint8_t *)realloc(bytes, capacity);
					if(!nb) {
						free(bytes);
						return 0;
					}
					bytes = nb;
				}
				bytes[n++] = data[pos++];
			}
		} else {
			--pos;
		}
	}

	*out_bytes = bytes;
	*out_len = n;
	return 1;
}

// [=]===^=[ bendaglish_load_tracks ]=============================================================[=]
static int32_t bendaglish_load_tracks(struct bendaglish_state *s) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	int32_t num_tracks = (s->sub_song_list_offset - s->track_offset_table_offset) / 2;
	if(num_tracks <= 0) {
		return 0;
	}

	if((uint32_t)(s->track_offset_table_offset + num_tracks * 2) > data_len) {
		return 0;
	}

	s->tracks = (uint8_t **)calloc((size_t)num_tracks, sizeof(uint8_t *));
	s->track_lens = (uint32_t *)calloc((size_t)num_tracks, sizeof(uint32_t));
	if(!s->tracks || !s->track_lens) {
		return 0;
	}
	s->num_tracks = (uint32_t)num_tracks;

	for(int32_t i = 0; i < num_tracks; ++i) {
		uint16_t track_offset = bendaglish_read_u16_be(data + s->track_offset_table_offset + i * 2);
		uint32_t pos = (uint32_t)s->tracks_offset + (uint32_t)track_offset;
		if(!bendaglish_load_single_track(s, pos, &s->tracks[i], &s->track_lens[i])) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ bendaglish_load_sample_info ]========================================================[=]
static int32_t bendaglish_load_sample_info(struct bendaglish_state *s, uint32_t **out_data_offsets) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	uint32_t pos = (uint32_t)s->sample_info_offset_table_offset;
	uint32_t first_sample_info = 0xffffffffu;

	uint32_t capacity = 32;
	uint32_t *offsets = (uint32_t *)malloc(capacity * sizeof(uint32_t));
	if(!offsets) {
		return 0;
	}
	uint32_t count = 0;

	for(;;) {
		if((pos + 4) > data_len) {
			free(offsets);
			return 0;
		}
		uint32_t off = bendaglish_read_u32_be(data + pos);
		pos += 4;

		if(off < first_sample_info) {
			first_sample_info = off;
		}

		if(count == capacity) {
			capacity *= 2;
			uint32_t *no = (uint32_t *)realloc(offsets, capacity * sizeof(uint32_t));
			if(!no) {
				free(offsets);
				return 0;
			}
			offsets = no;
		}
		offsets[count++] = off;

		if(pos >= ((uint32_t)s->sample_info_offset_table_offset + first_sample_info)) {
			break;
		}
	}

	s->samples = (struct bendaglish_sample *)calloc(count, sizeof(struct bendaglish_sample));
	if(!s->samples) {
		free(offsets);
		return 0;
	}
	s->num_samples = count;

	uint32_t *data_offsets = (uint32_t *)calloc(count, sizeof(uint32_t));
	if(!data_offsets) {
		free(offsets);
		return 0;
	}

	for(uint32_t i = 0; i < count; ++i) {
		uint32_t info_pos = (uint32_t)s->sample_info_offset_table_offset + offsets[i];
		if((info_pos + 28) > data_len) {
			free(offsets);
			free(data_offsets);
			return 0;
		}
		struct bendaglish_sample *smp = &s->samples[i];
		smp->sample_number = (int16_t)i;

		uint32_t sample_data_offset = bendaglish_read_u32_be(data + info_pos +  0);
		smp->loop_offset           = bendaglish_read_u32_be(data + info_pos +  4);
		if(smp->loop_offset > 0) {
			smp->loop_offset -= sample_data_offset;
		}
		smp->length              = bendaglish_read_u16_be(data + info_pos +  8);
		smp->loop_length         = bendaglish_read_u16_be(data + info_pos + 10);
		smp->volume              = bendaglish_read_u16_be(data + info_pos + 12);
		smp->volume_fade_speed   = bendaglish_read_i16_be(data + info_pos + 14);
		smp->portamento_duration = bendaglish_read_i16_be(data + info_pos + 16);
		smp->portamento_add_value= bendaglish_read_i16_be(data + info_pos + 18);
		smp->vibrato_depth       = bendaglish_read_u16_be(data + info_pos + 20);
		smp->vibrato_add_value   = bendaglish_read_u16_be(data + info_pos + 22);
		smp->note_transpose      = bendaglish_read_i16_be(data + info_pos + 24);
		smp->fine_tune_period    = bendaglish_read_u16_be(data + info_pos + 26);

		data_offsets[i] = sample_data_offset;
	}

	free(offsets);
	*out_data_offsets = data_offsets;
	return 1;
}

// [=]===^=[ bendaglish_load_sample_data ]========================================================[=]
static int32_t bendaglish_load_sample_data(struct bendaglish_state *s, uint32_t *data_offsets) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	for(uint32_t i = 0; i < s->num_samples; ++i) {
		struct bendaglish_sample *smp = &s->samples[i];
		uint32_t pos = (uint32_t)s->sample_info_offset_table_offset + data_offsets[i];
		uint32_t end1 = (uint32_t)smp->length * 2;
		uint32_t end2 = smp->loop_offset + (uint32_t)smp->loop_length * 2;
		uint32_t length = (end1 > end2) ? end1 : end2;
		if(length == 0) {
			smp->sample_data = bendaglish_empty_sample;
			continue;
		}
		if((pos + length) > data_len) {
			return 0;
		}
		smp->sample_data = (int8_t *)(data + pos);
	}
	return 1;
}

// [=]===^=[ bendaglish_cleanup ]=================================================================[=]
static void bendaglish_cleanup(struct bendaglish_state *s) {
	if(!s) {
		return;
	}
	free(s->sub_songs); s->sub_songs = 0;
	if(s->position_lists) {
		for(uint32_t i = 0; i < s->num_position_lists; ++i) {
			free(s->position_lists[i].bytes);
		}
		free(s->position_lists);
		s->position_lists = 0;
	}
	if(s->tracks) {
		for(uint32_t i = 0; i < s->num_tracks; ++i) {
			free(s->tracks[i]);
		}
		free(s->tracks);
		s->tracks = 0;
	}
	free(s->track_lens); s->track_lens = 0;
	free(s->samples); s->samples = 0;
}

// [=]===^=[ bendaglish_initialize_sound ]========================================================[=]
static void bendaglish_initialize_sound(struct bendaglish_state *s, uint32_t sub_song) {
	if(sub_song >= s->num_sub_songs) {
		sub_song = 0;
	}
	s->selected_sub_song = sub_song;
	struct bendaglish_song_info *song = &s->sub_songs[sub_song];

	s->enable_playing = 1;
	s->master_volume = 64;
	s->master_volume_fade_speed = 0;
	s->master_volume_fade_speed_counter = 1;
	s->counter = 6;

	for(int32_t i = 0; i < 4; ++i) {
		struct bendaglish_voice *v = &s->voices[i];
		struct bendaglish_voice_playback *pb = &s->playback[i];

		memset(v, 0, sizeof(*v));
		memset(pb, 0, sizeof(*pb));

		struct bendaglish_position_list *pl = bendaglish_find_position_list(s, song->position_lists[i]);
		v->channel_enabled = 1;
		v->position_list = pl ? pl->bytes : 0;
		v->position_list_len = pl ? pl->length : 0;
		v->current_position = 0;
		v->next_position = 0;
		v->playing_track = 0;
		v->track = 0;
		v->track_len = 0;
		v->next_track_position = 0;
		v->switch_to_next_position = 1;
		v->track_loop_counter = 1;
		v->ticks_left_for_next_track_command = 1;
		v->use_new_note = 1;
		v->channel_volume = 0xffff;

		for(uint8_t j = 0; j < 10; ++j) {
			v->sample_mapping[j] = j;
		}

		pb->sample_number = -1;
		pb->handle_sample_callback = BENDAGLISH_CB_PLAY_ONCE;
	}
}

// [=]===^=[ bendaglish_enable_dma ]==============================================================[=]
static void bendaglish_enable_dma(struct bendaglish_state *s, struct bendaglish_voice_playback *pb, int32_t voice_idx) {
	if(!pb->dma_enabled) {
		uint32_t sample_length = (uint32_t)pb->sample_length * 2u;
		paula_play_sample(&s->paula, voice_idx, pb->sample_data, sample_length);
		paula_set_loop(&s->paula, voice_idx, 0, sample_length);
		pb->dma_enabled = 1;
	}
}

// [=]===^=[ bendaglish_handle_sample_loop ]======================================================[=]
static void bendaglish_handle_sample_loop(struct bendaglish_state *s, struct bendaglish_voice_playback *pb, struct bendaglish_sample *sample, int32_t voice_idx) {
	if(pb->loop_delay_counter < 0x8000) {
		if(s->features.set_dma_in_sample_handlers) {
			bendaglish_enable_dma(s, pb, voice_idx);
		}

		pb->loop_delay_counter--;
		if(pb->loop_delay_counter == 0) {
			pb->loop_delay_counter = (uint16_t)~pb->loop_delay_counter;

			uint32_t loop_length = (uint32_t)sample->loop_length * 2u;
			if(loop_length > 0) {
				paula_queue_sample(&s->paula, voice_idx, sample->sample_data, sample->loop_offset, loop_length);
				paula_set_loop(&s->paula, voice_idx, sample->loop_offset, loop_length);
			} else {
				paula_mute(&s->paula, voice_idx);
			}
		}
	} else {
		if(pb->sample_play_ticks_counter <= 1) {
			pb->handle_sample_callback = BENDAGLISH_CB_VOLUME_FADE;
		}
	}
}

// [=]===^=[ bendaglish_handle_sample_volume_fade ]===============================================[=]
static void bendaglish_handle_sample_volume_fade(struct bendaglish_voice_playback *pb, struct bendaglish_sample *sample) {
	pb->loop_delay_counter = 0x8000;
	pb->final_volume = (int16_t)(pb->final_volume + sample->volume_fade_speed);
}

// [=]===^=[ bendaglish_handle_sample_play_once ]=================================================[=]
static void bendaglish_handle_sample_play_once(struct bendaglish_state *s, struct bendaglish_voice_playback *pb, int32_t voice_idx) {
	if(pb->loop_delay_counter < 0x8000) {
		pb->loop_delay_counter--;
		if(pb->loop_delay_counter == 0) {
			pb->loop_delay_counter = (uint16_t)~pb->loop_delay_counter;
			// Simulate stopping the sample by switching to silence buffer.
			paula_queue_sample(&s->paula, voice_idx, bendaglish_empty_sample, 0, sizeof(bendaglish_empty_sample));
			paula_set_loop(&s->paula, voice_idx, 0, sizeof(bendaglish_empty_sample));
		} else {
			if(s->features.set_dma_in_sample_handlers) {
				bendaglish_enable_dma(s, pb, voice_idx);
			}
		}
	} else {
		if(pb->sample_play_ticks_counter == 1) {
			pb->loop_delay_counter = 0x8000;
		}
		if(!s->paula.ch[voice_idx].active) {
			pb->final_volume = 0;
		}
	}
}

// [=]===^=[ bendaglish_do_sample_effects ]=======================================================[=]
static void bendaglish_do_sample_effects(struct bendaglish_voice_playback *pb, struct bendaglish_sample *sample) {
	if(pb->sample_portamento_duration != 0) {
		if(pb->sample_portamento_duration == -1) {
			return;
		}
		pb->sample_period_add_value = (int16_t)(pb->sample_period_add_value + pb->sample_portamento_add_value);
		pb->sample_portamento_duration--;
		if(pb->sample_portamento_duration != 0) {
			return;
		}
	}

	pb->sample_period_add_value = (int16_t)(pb->sample_period_add_value + pb->sample_vibrato_add_value);
	pb->sample_vibrato_depth--;

	if(pb->sample_vibrato_depth == 0) {
		if(sample->vibrato_depth != 0) {
			pb->sample_vibrato_depth = sample->vibrato_depth;
			pb->sample_vibrato_add_value = (int16_t)-pb->sample_vibrato_add_value;
		}
	}
}

// [=]===^=[ bendaglish_do_final_volume_slide ]===================================================[=]
static void bendaglish_do_final_volume_slide(struct bendaglish_voice_playback *pb) {
	if(pb->final_volume_slide_speed != 0) {
		pb->final_volume_slide_speed_counter--;
		if(pb->final_volume_slide_speed_counter == 0) {
			pb->final_volume_slide_speed_counter = pb->final_volume_slide_speed;
			pb->final_volume = (int16_t)(pb->final_volume + pb->final_volume_slide_add_value);
		}
	}
}

// [=]===^=[ bendaglish_do_portamento ]===========================================================[=]
static void bendaglish_do_portamento(struct bendaglish_voice *v, struct bendaglish_voice_playback *pb) {
	if(v->portamento_control_flag != 0) {
		if(v->portamento_control_flag >= 0x80) {
			uint32_t period = pb->note_period;
			int32_t pav = v->portamento_add_value;
			int32_t hi = pav >> 16;                              // arithmetic shift (signed)
			uint32_t lo = (uint32_t)pav & 0xffffu;
			int32_t mul = (int32_t)((period * lo) >> 16) + (int32_t)(period * (uint32_t)hi);
			int32_t temp = (int32_t)(int16_t)mul - (int32_t)pb->note_period;
			if(v->portamento_duration_counter != 0) {
				v->portamento_add_value = temp / (int32_t)v->portamento_duration_counter;
			}
			v->portamento_control_flag &= 0x7f;
		}

		if(v->portamento_start_delay_counter == 0) {
			if(v->portamento_duration_counter != 0) {
				v->portamento_duration_counter--;
				pb->portamento_add_value = (int16_t)(pb->portamento_add_value + v->portamento_add_value);
			}
		} else {
			v->portamento_start_delay_counter--;
		}
	}
}

// Forward of setup_sample required for do_volume_fade. We avoid forward declaration by ordering:
// bendaglish_setup_sample must be defined before bendaglish_do_volume_fade. Done below.

// [=]===^=[ bendaglish_setup_sample ]============================================================[=]
static void bendaglish_setup_sample(struct bendaglish_state *s, struct bendaglish_voice_playback *pb, int32_t voice_idx, struct bendaglish_sample *sample, uint8_t transposed_note, uint8_t play_ticks, int16_t volume, uint16_t volume_slide_speed, int16_t volume_slide_add_value) {
	paula_mute(&s->paula, voice_idx);

	pb->dma_enabled = 0;
	pb->sample_number = sample->sample_number;
	pb->sample_data = sample->sample_data;
	pb->sample_length = sample->length;

	pb->playing_sample = sample;
	pb->sample_play_ticks_counter = play_ticks;

	int32_t period_index = -((int32_t)transposed_note & 0x7f) + (int32_t)sample->note_transpose + BENDAGLISH_FINETUNE_START;
	pb->note_period = bendaglish_period_for_index(period_index, sample->fine_tune_period);

	pb->sample_portamento_duration = sample->portamento_duration;

	if(pb->sample_portamento_duration >= 0) {
		pb->sample_vibrato_depth = (uint16_t)(sample->vibrato_depth / 2);
		if((sample->vibrato_depth & 1) != 0) {
			pb->sample_vibrato_depth++;
		}
		pb->sample_portamento_add_value = (int16_t)((int32_t)sample->portamento_add_value * (int32_t)pb->note_period / 32768);
		pb->sample_vibrato_add_value    = (int16_t)((int32_t)sample->vibrato_add_value    * (int32_t)pb->note_period / 32768);
	}

	pb->sample_period_add_value = 0;
	pb->handle_sample_callback = (sample->volume_fade_speed == 0) ? BENDAGLISH_CB_PLAY_ONCE : BENDAGLISH_CB_LOOP;
	pb->final_volume = volume;

	int16_t set_vol = volume;
	if(set_vol > s->master_volume) {
		set_vol = s->master_volume;
	}
	if(set_vol < 0) {
		set_vol = 0;
	}
	paula_set_volume(&s->paula, voice_idx, (uint16_t)set_vol);

	if(s->features.enable_final_volume_slide) {
		pb->final_volume_slide_speed = volume_slide_speed;
		pb->final_volume_slide_speed_counter = volume_slide_speed;
		pb->final_volume_slide_add_value = volume_slide_add_value;
	}

	pb->loop_delay_counter = 2;
}

// [=]===^=[ bendaglish_do_volume_fade ]==========================================================[=]
static void bendaglish_do_volume_fade(struct bendaglish_state *s, struct bendaglish_voice *v, struct bendaglish_voice_playback *pb, int32_t voice_idx) {
	if(v->volume_fade_running && (v->volume_fade_duration_counter != 0)) {
		v->volume_fade_speed_counter--;
		if(v->volume_fade_speed_counter == 0) {
			v->volume_fade_duration_counter--;
			v->volume_fade_speed_counter = v->volume_fade_speed;

			int32_t volume = (int32_t)v->volume_fade_value + (int32_t)v->volume_fade_add_value;
			v->volume_fade_value = (int16_t)volume;

			if(v->sample_info2) {
				volume += (int32_t)v->sample_info2->volume;
				if(volume < 0) {
					v->volume_fade_duration_counter = 0;
				} else {
					if(volume > 64) {
						volume = 64;
					}
					bendaglish_setup_sample(s, pb, voice_idx, v->sample_info2, v->transposed_note, v->volume_fade_speed, (int16_t)volume, 0, 0);
				}
			}
		}
	}
}

// [=]===^=[ bendaglish_take_next_position ]======================================================[=]
static int32_t bendaglish_take_next_position(struct bendaglish_state *s, struct bendaglish_voice *v, struct bendaglish_voice_playback *pb) {
	v->current_position = v->next_position;
	int32_t position = v->next_position;
	uint8_t cmd;

	for(;;) {
		if((uint32_t)position >= v->position_list_len) {
			v->channel_enabled = 0;
			return 1;
		}
		cmd = v->position_list[position++];
		if(cmd < s->features.max_track_value) {
			break;
		}

		if(cmd == 0xfe) {
			v->transpose = (int8_t)v->position_list[position++];
		} else if(cmd == 0xff) {
			if((pb->loop_delay_counter == 0) || (pb->loop_delay_counter == 0x8000)) {
				v->channel_enabled = 0;
			}
			return 1;
		} else if((cmd == 0xfd) && (s->features.master_volume_fade_version > 0)) {
			s->master_volume_fade_speed = (int8_t)v->position_list[position++];
		} else if(s->features.enable_f0_track_loop && (cmd < 0xf0)) {
			v->track_loop_counter = (uint8_t)(cmd - 0xc8);
		} else if(s->features.enable_c0_track_loop && (cmd < 0xc0)) {
			v->track_loop_counter = (uint8_t)(cmd & 0x1f);
		} else {
			int32_t map_index = (s->features.set_sample_mapping_version == 1) ? (cmd & 0x07) : (cmd - 0xf0);
			if((map_index >= 0) && (map_index < 10)) {
				v->sample_mapping[map_index] = (uint8_t)(v->position_list[position++] / 4);
			} else {
				position++;
			}
		}
	}

	v->switch_to_next_position = 0;
	v->next_position = position;

	v->playing_track = cmd;
	if((uint32_t)cmd < s->num_tracks) {
		v->track = s->tracks[cmd];
		v->track_len = s->track_lens[cmd];
	} else {
		v->track = 0;
		v->track_len = 0;
	}
	v->next_track_position = 0;
	return 0;
}

// [=]===^=[ bendaglish_parse_track_effect ]======================================================[=]
static void bendaglish_parse_track_effect(struct bendaglish_state *s, struct bendaglish_voice *v, int32_t *pos_io) {
	struct bendaglish_features *f = &s->features;
	int32_t position = *pos_io;
	uint8_t cmd = v->track[position++];

	if(cmd <= f->max_sample_mapping_value) {
		int32_t index = (f->get_sample_mapping_version == 1) ? (cmd & 0x07) : (cmd - 0x80);
		if((index >= 0) && (index < 10)) {
			uint32_t smp = v->sample_mapping[index];
			if(smp < s->num_samples) {
				v->sample_info = &s->samples[smp];
			}
		}
	} else if(cmd == 0xff) {
		v->switch_to_next_position = 1;
		position--;
	} else if((f->uses_cx_track_effects && (cmd < 0xc0)) || (f->uses_9x_track_effects && (cmd < 0x9b))) {
		// no-op
	} else {
		uint8_t is_cx = f->uses_cx_track_effects;
		uint8_t is_9x = f->uses_9x_track_effects;

		if(((is_cx && (cmd == 0xc0)) || (is_9x && (cmd == 0x9b))) && f->enable_portamento) {
			v->portamento_1_enabled = 255;
			v->portamento_2_enabled = 0;
			v->portamento_start_delay = v->track[position++];
			v->portamento_duration = v->track[position++];
			v->portamento_delta_note_number = (int8_t)v->track[position++];
		} else if(((is_cx && (cmd == 0xc1)) || (is_9x && (cmd == 0x9c))) && f->enable_portamento) {
			v->portamento_1_enabled = 0;
		} else if(((is_cx && (cmd == 0xc2)) || (is_9x && (cmd == 0x9d))) && f->enable_volume_fade) {
			v->volume_fade_enabled = 1;
			v->volume_fade_init_speed = v->track[position++];
			v->volume_fade_duration = v->track[position++];
			v->volume_fade_init_add_value = (int16_t)(int8_t)v->track[position++];
		} else if(((is_cx && (cmd == 0xc3)) || (is_9x && (cmd == 0x9e))) && f->enable_volume_fade) {
			v->volume_fade_enabled = 0;
		} else if(((is_cx && (cmd == 0xc4)) || (is_9x && (cmd == 0x9f))) && f->enable_portamento) {
			v->portamento_2_enabled = 1;
			v->portamento_1_enabled = 0;
			v->portamento_duration = v->track[position++];
		} else if(((is_cx && (cmd == 0xc5)) || (is_9x && (cmd == 0xa0))) && f->enable_portamento) {
			v->portamento_2_enabled = 0;
		} else if(((is_cx && (cmd == 0xc6)) || (is_9x && (cmd == 0xa1))) && f->enable_volume_fade) {
			v->channel_volume = (uint16_t)(((uint16_t)v->track[position++] << 8) | 0xff);
			if(f->enable_final_volume_slide) {
				v->channel_volume_slide_speed = v->track[position++];
				v->channel_volume_slide_add_value = (int16_t)(int8_t)v->track[position++];
			}
		} else if(((is_cx && (cmd == 0xc7)) || (is_9x && (cmd == 0xa2))) && f->enable_final_volume_slide) {
			v->channel_volume_slide_speed = 0;
			v->channel_volume = 0xffff;
		}
	}

	*pos_io = position;
}

// [=]===^=[ bendaglish_parse_track ]=============================================================[=]
static int32_t bendaglish_parse_track(struct bendaglish_state *s, struct bendaglish_voice *v, struct bendaglish_voice_playback *pb, int32_t voice_idx) {
	if(!v->track) {
		v->channel_enabled = 0;
		return 0;
	}

	int32_t position = v->next_track_position;
	v->ticks_left_for_next_track_command--;

	if(v->ticks_left_for_next_track_command != 0) {
		if(((uint32_t)position < v->track_len) && (v->track[position] >= 0x80)) {
			bendaglish_parse_track_effect(s, v, &position);
		}
		v->next_track_position = position;
		return 0;
	}

	for(;;) {
		if((uint32_t)position >= v->track_len) {
			v->channel_enabled = 0;
			return 0;
		}
		if(v->track[position] < 0x80) {
			break;
		}
		bendaglish_parse_track_effect(s, v, &position);
		if(v->switch_to_next_position) {
			if(s->features.check_for_ticks && (v->ticks_left_for_next_track_command == 0)) {
				v->ticks_left_for_next_track_command = 1;
			}
			return 1;
		}
	}

	uint8_t note = v->track[position++];

	if(note == 0x7f) {
		v->ticks_left_for_next_track_command = v->track[position++];
		v->next_track_position = position;
		return 0;
	}

	pb->portamento_add_value = 0;

	note = (uint8_t)(note + (uint8_t)v->transpose);
	v->transposed_note = note;

	if(s->features.enable_portamento) {
		v->portamento_control_flag = v->portamento_1_enabled;
		if(v->portamento_control_flag != 0) {
			v->portamento_start_delay_counter = v->portamento_start_delay;
			v->portamento_duration_counter = v->portamento_duration;
			int32_t ft_idx = BENDAGLISH_FINETUNE_START - (int32_t)v->portamento_delta_note_number;
			if((ft_idx >= 0) && ((uint32_t)ft_idx < (sizeof(bendaglish_fine_tune) / sizeof(bendaglish_fine_tune[0])))) {
				v->portamento_add_value = bendaglish_fine_tune[ft_idx];
			} else {
				v->portamento_add_value = 0;
			}
		}
	}

	if(s->features.enable_volume_fade) {
		v->volume_fade_running = v->volume_fade_enabled;
		if(v->volume_fade_running) {
			v->volume_fade_speed = v->volume_fade_init_speed;
			v->volume_fade_speed_counter = v->volume_fade_init_speed;
			v->volume_fade_duration_counter = v->volume_fade_duration;
			v->volume_fade_add_value = v->volume_fade_init_add_value;
			v->volume_fade_value = 0;
		}
	}

	uint8_t ticks = v->track[position++];
	if(s->features.extra_tick_arg && (ticks == 0)) {
		v->ticks_left_for_next_track_command = v->track[position++];
		ticks = 0xff;
	} else {
		v->ticks_left_for_next_track_command = ticks;
	}

	if(s->features.enable_portamento && v->portamento_2_enabled) {
		v->portamento_control_flag = 0xff;
		v->portamento_start_delay_counter = 0;
		v->portamento_duration_counter = v->portamento_duration;

		uint8_t note1 = note;
		if(!v->use_new_note) {
			note = v->previous_transposed_note;
		}

		v->transposed_note = note;
		int32_t ft_idx = BENDAGLISH_FINETUNE_START - (int32_t)((int8_t)(note1 - note));
		if((ft_idx >= 0) && ((uint32_t)ft_idx < (sizeof(bendaglish_fine_tune) / sizeof(bendaglish_fine_tune[0])))) {
			v->portamento_add_value = bendaglish_fine_tune[ft_idx];
		} else {
			v->portamento_add_value = 0;
		}
	}

	v->previous_transposed_note = v->transposed_note;
	v->next_track_position = position;
	v->use_new_note = 0;

	struct bendaglish_sample *sample = v->sample_info;
	v->sample_info2 = sample;

	if(sample) {
		int32_t volume;
		if(s->features.enable_volume_fade) {
			volume = ((int32_t)sample->volume * (int32_t)v->channel_volume) / 16384;
		} else {
			volume = (int32_t)sample->volume;
		}
		bendaglish_setup_sample(s, pb, voice_idx, sample, note, ticks, (int16_t)volume,
		                        v->channel_volume_slide_speed, v->channel_volume_slide_add_value);
	}
	return 0;
}

// [=]===^=[ bendaglish_handle_voice ]============================================================[=]
static void bendaglish_handle_voice(struct bendaglish_state *s, struct bendaglish_voice *v, struct bendaglish_voice_playback *pb, int32_t voice_idx) {
	if(!v->channel_enabled) {
		return;
	}

	if(s->features.enable_portamento) {
		bendaglish_do_portamento(v, pb);
	}
	if(s->features.enable_volume_fade) {
		bendaglish_do_volume_fade(s, v, pb, voice_idx);
	}

	for(;;) {
		if(v->switch_to_next_position) {
			v->track_loop_counter--;
			if(v->track_loop_counter == 0) {
				v->track_loop_counter = 1;
				if(bendaglish_take_next_position(s, v, pb)) {
					return;
				}
			} else {
				v->next_track_position = 0;
				v->switch_to_next_position = 0;
			}
		}

		if(!bendaglish_parse_track(s, v, pb, voice_idx)) {
			break;
		}
	}
}

// [=]===^=[ bendaglish_handle_voice_effects ]====================================================[=]
static void bendaglish_handle_voice_effects(struct bendaglish_state *s, struct bendaglish_voice_playback *pb, int32_t voice_idx) {
	if(pb->loop_delay_counter != 0) {
		struct bendaglish_sample *sample = pb->playing_sample;
		if(!sample) {
			return;
		}

		if(pb->sample_play_ticks_counter > 0) {
			pb->sample_play_ticks_counter--;
		}

		if(s->features.enable_sample_effects) {
			bendaglish_do_sample_effects(pb, sample);
		}
		if(s->features.enable_final_volume_slide) {
			bendaglish_do_final_volume_slide(pb);
		}

		switch(pb->handle_sample_callback) {
			case BENDAGLISH_CB_LOOP: {
				bendaglish_handle_sample_loop(s, pb, sample, voice_idx);
				break;
			}

			case BENDAGLISH_CB_VOLUME_FADE: {
				bendaglish_handle_sample_volume_fade(pb, sample);
				break;
			}

			default: {
				bendaglish_handle_sample_play_once(s, pb, voice_idx);
				break;
			}
		}

		int16_t volume = pb->final_volume;
		if(volume > 0) {
			if(volume > s->master_volume) {
				volume = s->master_volume;
			}
			if(volume < 0) {
				volume = 0;
			}
			paula_set_volume(&s->paula, voice_idx, (uint16_t)volume);

			uint16_t period = (uint16_t)(pb->note_period + pb->sample_period_add_value + pb->portamento_add_value);
			paula_set_period(&s->paula, voice_idx, period);

			if(!s->features.set_dma_in_sample_handlers) {
				bendaglish_enable_dma(s, pb, voice_idx);
			}
		} else {
			paula_mute(&s->paula, voice_idx);
			pb->loop_delay_counter = 0;
		}
	}
}

// [=]===^=[ bendaglish_do_master_volume_fade ]===================================================[=]
static void bendaglish_do_master_volume_fade(struct bendaglish_state *s) {
	if(s->master_volume_fade_speed != 0) {
		s->master_volume_fade_speed_counter--;
		if(s->master_volume_fade_speed_counter == 0) {
			s->master_volume_fade_speed_counter = s->master_volume_fade_speed;
			if(s->features.master_volume_fade_version == 2) {
				s->master_volume_fade_speed_counter--;
			}
			s->master_volume--;
			if(s->master_volume < 0) {
				s->enable_playing = 0;
				for(int32_t i = 0; i < 4; ++i) {
					paula_mute(&s->paula, i);
				}
			}
		}
	}
}

// [=]===^=[ bendaglish_handle_effects ]==========================================================[=]
static void bendaglish_handle_effects(struct bendaglish_state *s) {
	if(s->features.master_volume_fade_version > 0) {
		bendaglish_do_master_volume_fade(s);
	}
	for(int32_t i = 0; i < 4; ++i) {
		bendaglish_handle_voice_effects(s, &s->playback[i], i);
	}
}

// [=]===^=[ bendaglish_restart_song ]============================================================[=]
static void bendaglish_restart_song(struct bendaglish_state *s) {
	bendaglish_initialize_sound(s, s->selected_sub_song);
}

// [=]===^=[ bendaglish_tick ]====================================================================[=]
static void bendaglish_tick(struct bendaglish_state *s) {
	bendaglish_handle_effects(s);

	if(s->features.enable_counter) {
		s->counter--;
		if(s->counter == 0) {
			s->counter = 6;
			return;
		}
	}

	if(!s->enable_playing) {
		s->enable_playing = 1;
		bendaglish_restart_song(s);
		for(int32_t i = 0; i < 4; ++i) {
			paula_mute(&s->paula, i);
		}
		return;
	}

	uint8_t any_active = 0;
	for(int32_t i = 0; i < 4; ++i) {
		if(s->voices[i].channel_enabled) {
			any_active = 1;
			break;
		}
	}
	s->enable_playing = any_active;

	if(s->enable_playing) {
		for(int32_t i = 0; i < 4; ++i) {
			bendaglish_handle_voice(s, &s->voices[i], &s->playback[i], i);
		}
	}
}

// [=]===^=[ bendaglish_init ]====================================================================[=]
static struct bendaglish_state *bendaglish_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || (len < 0x1600) || (sample_rate < 8000)) {
		return 0;
	}

	struct bendaglish_state *s = (struct bendaglish_state *)calloc(1, sizeof(struct bendaglish_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;

	uint32_t *sample_data_offsets = 0;

	if(!bendaglish_test_module(s, s->module_data, (int32_t)len)) {
		free(s);
		return 0;
	}

	if(!bendaglish_load_sub_song_info(s)) {
		goto fail;
	}
	if(!bendaglish_load_position_lists(s)) {
		goto fail;
	}
	if(!bendaglish_load_tracks(s)) {
		goto fail;
	}

	if(!bendaglish_load_sample_info(s, &sample_data_offsets)) {
		goto fail;
	}
	if(!bendaglish_load_sample_data(s, sample_data_offsets)) {
		free(sample_data_offsets);
		goto fail;
	}
	free(sample_data_offsets);

	paula_init(&s->paula, sample_rate, BENDAGLISH_TICK_HZ);
	bendaglish_initialize_sound(s, 0);
	return s;

fail:
	bendaglish_cleanup(s);
	free(s);
	return 0;
}

// [=]===^=[ bendaglish_free ]====================================================================[=]
static void bendaglish_free(struct bendaglish_state *s) {
	if(!s) {
		return;
	}
	bendaglish_cleanup(s);
	free(s);
}

// [=]===^=[ bendaglish_get_audio ]===============================================================[=]
static void bendaglish_get_audio(struct bendaglish_state *s, int16_t *output, int32_t frames) {
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
			bendaglish_tick(s);
		}
	}
}

// [=]===^=[ bendaglish_api_init ]================================================================[=]
static void *bendaglish_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return bendaglish_init(data, len, sample_rate);
}

// [=]===^=[ bendaglish_api_free ]================================================================[=]
static void bendaglish_api_free(void *state) {
	bendaglish_free((struct bendaglish_state *)state);
}

// [=]===^=[ bendaglish_api_get_audio ]===========================================================[=]
static void bendaglish_api_get_audio(void *state, int16_t *output, int32_t frames) {
	bendaglish_get_audio((struct bendaglish_state *)state, output, frames);
}

static const char *bendaglish_extensions[] = { "bd", 0 };

static struct player_api bendaglish_api = {
	"Ben Daglish",
	bendaglish_extensions,
	bendaglish_api_init,
	bendaglish_api_free,
	bendaglish_api_get_audio,
	0,
};
