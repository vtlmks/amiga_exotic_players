// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// David Whittaker replayer, ported from NostalgicPlayer's C# implementation.
// Drives an Amiga Paula (see paula.h) at 50Hz tick rate (PAL).
//
// DW modules are typically embedded in a ripped m68k executable, so the loader
// must scan the player code to discover the layout (init function, play function,
// sample table offset, sub-song table, etc). The scanner here mirrors the C#
// Identify and ExtractInfoFromInitFunction / ExtractInfoFromPlayFunction logic
// byte-for-byte.
//
// Public API:
//   struct davidwhittaker_state *davidwhittaker_init(void *data, uint32_t len, int32_t sample_rate);
//   void davidwhittaker_free(struct davidwhittaker_state *s);
//   void davidwhittaker_get_audio(struct davidwhittaker_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define DW_TICK_HZ          50
#define DW_MAX_CHANNELS     8
#define DW_SCAN_BUFFER_LEN  16384

enum {
	DW_EFFECT_END_OF_TRACK         = 0,
	DW_EFFECT_SLIDE                = 1,
	DW_EFFECT_MUTE                 = 2,
	DW_EFFECT_WAIT_UNTIL_NEXT_ROW  = 3,
	DW_EFFECT_STOP_SONG            = 4,
	DW_EFFECT_GLOBAL_TRANSPOSE     = 5,
	DW_EFFECT_START_VIBRATO        = 6,
	DW_EFFECT_STOP_VIBRATO         = 7,
	DW_EFFECT_8                    = 8,
	DW_EFFECT_9                    = 9,
	DW_EFFECT_SET_SPEED            = 10,
	DW_EFFECT_GLOBAL_VOLUME_FADE   = 11,
	DW_EFFECT_SET_GLOBAL_VOLUME    = 12,
	DW_EFFECT_START_OR_STOP_SOUNDFX= 13,
	DW_EFFECT_STOP_SOUNDFX         = 14
};

struct dw_sample {
	int8_t *sample_data;          // owned by player (copy for square write)
	uint32_t length;
	int32_t loop_start;           // -1 == no loop
	uint16_t volume;
	uint16_t fine_tune_period;
	int8_t transpose;
	int16_t sample_number;
};

struct dw_track {
	uint32_t offset;              // module-relative offset key
	uint8_t *data;
	uint32_t length;
};

struct dw_position_list {
	uint32_t *track_offsets;
	uint32_t length;
	uint16_t restart_position;
};

struct dw_song_info {
	uint16_t speed;
	uint8_t delay_counter_speed;
	struct dw_position_list position_lists[DW_MAX_CHANNELS];
};

struct dw_byte_seq {
	uint8_t *data;
	uint32_t length;
};

struct dw_global_playing_info {
	int8_t transpose;
	uint16_t volume_fade_speed;
	uint16_t global_volume;
	uint8_t global_volume_fade_speed;
	uint8_t global_volume_fade_counter;
	uint16_t square_change_position;
	uint8_t square_change_direction;
	uint8_t extra_counter;
	uint8_t delay_counter_speed;
	uint16_t delay_counter;
	uint16_t speed;
};

struct dw_channel_info {
	int32_t channel_number;
	uint32_t *position_list;
	uint32_t position_list_length;
	uint16_t current_position;
	uint16_t restart_position;
	uint8_t *track_data;
	uint32_t track_data_length;
	int32_t track_data_position;
	struct dw_sample *current_sample_info;
	uint8_t note;
	int8_t transpose;
	uint8_t enable_half_volume;
	uint16_t speed;
	uint16_t speed_counter;
	uint8_t *arpeggio_list;
	uint32_t arpeggio_list_length;
	int32_t arpeggio_list_position;
	uint8_t *envelope_list;
	uint32_t envelope_list_length;
	int32_t envelope_list_position;
	uint8_t envelope_speed;
	int8_t envelope_counter;
	uint8_t slide_enabled;
	int8_t slide_speed;
	uint8_t slide_counter;
	int16_t slide_value;
	int8_t vibrato_direction;
	uint8_t vibrato_speed;
	uint8_t vibrato_value;
	uint8_t vibrato_max_value;
};

struct davidwhittaker_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint8_t old_player;
	uint8_t uses_32bit_pointers;
	int32_t start_offset;

	int32_t sample_info_offset;
	int32_t sample_data_offset;
	int32_t sub_song_list_offset;
	int32_t arpeggio_list_offset;
	int32_t envelope_list_offset;
	int32_t channel_volume_offset;

	int32_t number_of_samples;
	int32_t number_of_channels;

	uint8_t enable_sample_transpose;
	uint8_t enable_channel_transpose;
	uint8_t enable_global_transpose;

	uint8_t new_sample_cmd;
	uint8_t new_envelope_cmd;
	uint8_t new_arpeggio_cmd;

	uint8_t enable_arpeggio;
	uint8_t enable_envelopes;
	uint8_t enable_vibrato;

	uint8_t enable_volume_fade;
	uint8_t enable_half_volume;
	uint8_t enable_global_volume_fade;
	uint8_t enable_set_global_volume;

	uint8_t enable_square_waveform;
	int32_t square_waveform_sample_number;
	uint32_t square_waveform_sample_length;
	uint16_t square_change_min_position;
	uint16_t square_change_max_position;
	uint8_t square_change_speed;
	int8_t square_byte1;
	int8_t square_byte2;

	uint8_t use_extra_counter;

	uint8_t enable_delay_counter;
	uint8_t enable_delay_multiply;
	uint8_t enable_delay_speed;

	uint16_t *periods;
	uint32_t periods_count;

	struct dw_song_info *song_info_list;
	uint32_t song_info_count;

	struct dw_track *tracks;
	uint32_t tracks_count;
	uint32_t tracks_capacity;

	struct dw_byte_seq *arpeggios;
	uint32_t arpeggios_count;

	struct dw_byte_seq *envelopes;
	uint32_t envelopes_count;

	struct dw_sample *samples;
	uint32_t samples_count;

	uint16_t channel_volumes[DW_MAX_CHANNELS];

	struct dw_global_playing_info playing_info;
	struct dw_channel_info channels[DW_MAX_CHANNELS];

	int32_t current_song;
	uint8_t end_reached;

	// Static fallback for empty tracks.
	uint8_t empty_track[1];
};

// [=]===^=[ dw_periods1 ]========================================================================[=]
static uint16_t dw_periods1[] = {
	                                                256,  242,  228,
	 215,  203,  192,  181,  171,  161,  152,  144,  136
};

// [=]===^=[ dw_periods2 ]========================================================================[=]
static uint16_t dw_periods2[] = {
	                                               4096, 3864, 3648,
	3444, 3252, 3068, 2896, 2732, 2580, 2436, 2300, 2168, 2048, 1932, 1824,
	1722, 1626, 1534, 1448, 1366, 1290, 1218, 1150, 1084, 1024,  966,  912,
	 861,  813,  767,  724,  683,  645,  609,  575,  542,  512,  483,  456,
	 430,  406,  383,  362,  341,  322,  304,  287,  271,
	                                                 256,  241,  228
};

// [=]===^=[ dw_periods3 ]========================================================================[=]
static uint16_t dw_periods3[] = {
	                                               8192, 7728, 7296,
	6888, 6504, 6136, 5792, 5464, 5160, 4872, 4600, 4336, 4096, 3864, 3648,
	3444, 3252, 3068, 2896, 2732, 2580, 2436, 2300, 2168, 2048, 1932, 1824,
	1722, 1626, 1534, 1448, 1366, 1290, 1218, 1150, 1084, 1024,  966,  912,
	 861,  813,  767,  724,  683,  645,  609,  575,  542,  512,  483,  456,
	 430,  406,  383,  362,  341,  322,  304,  287,  271,  256,  241,  228,
	 215,  203,  191,  181,  170,  161,  152,  143,  135
};

// [=]===^=[ dw_read_b_u16 ]======================================================================[=]
static uint16_t dw_read_b_u16(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ dw_read_b_u32 ]======================================================================[=]
static uint32_t dw_read_b_u32(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ dw_read_b_i32 ]======================================================================[=]
static int32_t dw_read_b_i32(uint8_t *p) {
	return (int32_t)dw_read_b_u32(p);
}

// [=]===^=[ dw_read_s8 ]=========================================================================[=]
static int32_t dw_read_s8(uint8_t v) {
	return (int32_t)(int8_t)v;
}

// [=]===^=[ dw_extract_info_from_init ]==========================================================[=]
// Mirrors C# ExtractInfoFromInitFunction. Returns 1 on success, 0 on failure.
static int32_t dw_extract_info_from_init(struct davidwhittaker_state *s, uint8_t *buf, int32_t search_length) {
	int32_t index;

	for(index = 0; index < search_length - 2; index += 2) {
		if((buf[index] == 0x47) && (buf[index + 1] == 0xfa) && ((buf[index + 2] & 0xf0) == 0xf0)) {
			break;
		}
	}
	if(index >= (search_length - 6)) {
		return 0;
	}

	s->start_offset = (dw_read_s8(buf[index + 2]) << 8) | (int32_t)buf[index + 3];
	s->start_offset += index + 2;

	for(; index < search_length - 2; index += 2) {
		if((buf[index] == 0x61) && (buf[index + 1] == 0x00)) {
			break;
		}
	}
	if(index >= (search_length - 8)) {
		return 0;
	}

	int32_t start_of_init = index;
	int32_t start_of_sample_init = index;

	if((buf[index + 4] == 0x61) && (buf[index + 5] == 0x00)) {
		start_of_sample_init = (dw_read_s8(buf[index + 6]) << 8) | (int32_t)buf[index + 7];
		start_of_sample_init += index + 6;
	}

	for(index = start_of_sample_init; index < search_length - 2; index += 2) {
		if((buf[index] == 0x4a) && (buf[index + 1] == 0x2b)) {
			break;
		}
	}
	if(index >= (search_length - 36)) {
		return 0;
	}

	if(buf[index + 4] != 0x66) {
		// Old-style player (QBall): sample init not in sub-function.
		for(index = start_of_init; index < search_length - 2; index += 2) {
			if((buf[index] == 0x41) && (buf[index + 1] == 0xeb)) {
				break;
			}
		}
		if(index >= (search_length - 36)) {
			return 0;
		}

		s->sample_data_offset = (dw_read_s8(buf[index + 2]) << 8) | (int32_t)buf[index + 3];
		s->sample_data_offset += s->start_offset;
		index += 4;

		if(buf[index + 4] != 0x72) {
			return 0;
		}

		s->number_of_samples = ((int32_t)buf[index + 5] & 0xff) + 1;

		for(; index < search_length - 4; index += 2) {
			if((buf[index] == 0x41) && (buf[index + 1] == 0xeb) && (buf[index + 4] == 0xe3) && (buf[index + 5] == 0x4f)) {
				break;
			}
		}
		if(index >= (search_length - 4)) {
			return 0;
		}

		s->channel_volume_offset = (dw_read_s8(buf[index + 2]) << 8) | (int32_t)buf[index + 3];
		s->channel_volume_offset += s->start_offset;

		for(index = start_of_init; index < search_length - 4; index += 2) {
			if((buf[index] == 0x41) && (buf[index + 1] == 0xeb) && (buf[index + 4] == 0x17)) {
				break;
			}
		}
		if(index >= (search_length - 4)) {
			return 0;
		}

		s->sub_song_list_offset = (dw_read_s8(buf[index + 2]) << 8) | (int32_t)buf[index + 3];
		s->sub_song_list_offset += s->start_offset;

		s->uses_32bit_pointers = 1;
		s->old_player = 1;
	} else {
		s->old_player = 0;

		if(buf[index + 5] == 0x00) {
			index += 2;
		}

		if((buf[index + 6] != 0x41) || (buf[index + 7] != 0xfa)) {
			return 0;
		}

		s->sample_data_offset = (dw_read_s8(buf[index + 8]) << 8) | (int32_t)buf[index + 9];
		s->sample_data_offset += index + 8;
		index += 10;

		if((buf[index] == 0x27) && (buf[index + 1] == 0x48) && (buf[index + 4] == 0xd0) && (buf[index + 5] == 0xfc)) {
			s->sample_data_offset += (int32_t)dw_read_b_u16(buf + index + 6);
			index += 12;

			if((buf[index] != 0xd0) || (buf[index + 1] != 0xfc)) {
				return 0;
			}

			s->sample_data_offset += (int32_t)dw_read_b_u16(buf + index + 2);
			index += 4;
		}

		if((buf[index] != 0x4b) || (buf[index + 1] != 0xfa) || (buf[index + 4] != 0x72)) {
			return 0;
		}

		s->sample_info_offset = (dw_read_s8(buf[index + 2]) << 8) | (int32_t)buf[index + 3];
		s->sample_info_offset += index + 2;
		s->number_of_samples = ((int32_t)buf[index + 5] & 0xff) + 1;

		index += 8;

		for(; index < search_length - 4; index += 2) {
			if((buf[index] == 0x37) && (buf[index + 1] == 0x7c)) {
				s->square_waveform_sample_length = (uint32_t)((((uint32_t)buf[index + 2] << 8) | (uint32_t)buf[index + 3]) * 2);
				break;
			}
		}

		for(index = start_of_init; index < search_length - 4; index += 2) {
			if((buf[index] == 0x41) && (buf[index + 1] == 0xfa) && (buf[index + 4] != 0x4b)) {
				break;
			}
		}
		if(index >= (search_length - 4)) {
			return 0;
		}

		if(((buf[index + 4] != 0x12) || (buf[index + 5] != 0x30)) && ((buf[index + 4] != 0x37) || (buf[index + 5] != 0x70))) {
			return 0;
		}

		s->sub_song_list_offset = (dw_read_s8(buf[index + 2]) << 8) | (int32_t)buf[index + 3];
		s->sub_song_list_offset += index + 2;
		index += 4;

		for(; index < search_length - 8; index += 2) {
			if((buf[index] == 0x41) && (buf[index + 1] == 0xfa) && (buf[index + 4] != 0x23)) {
				break;
			}
		}
		if(index >= (search_length - 8)) {
			return 0;
		}

		if((buf[index + 4] == 0x20) && (buf[index + 5] == 0x70)) {
			s->uses_32bit_pointers = 1;
		} else if((buf[index + 4] == 0x30) && (buf[index + 5] == 0x70)) {
			s->uses_32bit_pointers = 0;
		} else {
			return 0;
		}
	}

	return 1;
}

// [=]===^=[ dw_extract_info_from_play ]==========================================================[=]
// Mirrors C# ExtractInfoFromPlayFunction.
static int32_t dw_extract_info_from_play(struct davidwhittaker_state *s, uint8_t *buf, int32_t search_length) {
	int32_t index;
	int32_t offset;

	for(index = 0; index < search_length - 12; index += 2) {
		if((buf[index] == 0x47) && (buf[index + 1] == 0xfa)) {
			if(index >= (search_length - 12)) {
				return 0;
			}
			if((buf[index + 4] == 0x4a) && (buf[index + 5] == 0x2b) && (buf[index + 8] == 0x67)) {
				if((buf[index + 10] == 0x33) && (buf[index + 11] == 0xfc)) {
					continue;
				}
				if((buf[index + 10] == 0x17) && (buf[index + 11] == 0x7c)) {
					continue;
				}
				if((buf[index + 10] == 0x08) && (buf[index + 11] == 0xb9)) {
					continue;
				}
				break;
			}
		}
	}

	int32_t start_of_play = index;

	s->enable_delay_counter = 0;
	s->enable_delay_multiply = 0;

	for(index = start_of_play; (index < start_of_play + 100) && (index < search_length - 8); index += 2) {
		if((buf[index] == 0x10) && (buf[index + 1] == 0x3a)) {
			s->enable_delay_counter = 1;
			if((buf[index + 6] == 0xc0) && (buf[index + 7] == 0xfc)) {
				s->enable_delay_multiply = 1;
			}
			break;
		}
	}

	s->use_extra_counter = 0;

	for(index = start_of_play; (index < start_of_play + 100) && (index < search_length - 8); index += 2) {
		if((buf[index] == 0x53) && (buf[index + 1] == 0x2b) && (buf[index + 4] == 0x66)) {
			if(buf[index + 6] == 0x17) {
				if(index >= 4) {
					offset = ((int32_t)buf[index - 4] << 8) | (int32_t)buf[index - 3];
					int32_t pos = offset + s->start_offset;
					if((pos >= 0) && (pos < (int32_t)s->module_len)) {
						s->use_extra_counter = (s->module_data[pos] != 0) ? 1 : 0;
					}
				}
			}
			break;
		}
	}

	s->enable_square_waveform = 0;

	for(index = start_of_play; (index < start_of_play + 100) && (index < search_length - 52); index += 2) {
		if((buf[index] == 0x20) && (buf[index + 1] == 0x7a) && (buf[index + 4] == 0x30) && (buf[index + 5] == 0x3a)) {
			s->enable_square_waveform = 1;

			offset = (dw_read_s8(buf[index + 2]) << 8) | (int32_t)buf[index + 3];
			offset += index + 2;
			s->square_waveform_sample_number = (offset - s->sample_info_offset) / 12;

			if(((buf[index + 14] != 0x31) && (buf[index + 14] != 0x11)) || (buf[index + 15] != 0xbc)) {
				return 0;
			}
			s->square_byte1 = (int8_t)buf[index + 17];

			if(((buf[index + 20] & 0xf0) != 0x50) || (buf[index + 21] != 0x6b)) {
				return 0;
			}
			s->square_change_speed = (uint8_t)((buf[index + 20] & 0x0e) >> 1);

			if((buf[index + 24] != 0x0c) || (buf[index + 25] != 0x6b)) {
				return 0;
			}
			s->square_change_max_position = (uint16_t)(((uint16_t)buf[index + 26] << 8) | (uint16_t)buf[index + 27]);

			if(((buf[index + 38] != 0x31) && (buf[index + 38] != 0x11)) || (buf[index + 39] != 0xbc)) {
				return 0;
			}
			s->square_byte2 = (int8_t)buf[index + 41];

			if((buf[index + 48] != 0x0c) || (buf[index + 49] != 0x6b)) {
				return 0;
			}
			s->square_change_min_position = (uint16_t)(((uint16_t)buf[index + 50] << 8) | (uint16_t)buf[index + 51]);
			break;
		}
	}

	s->number_of_channels = 0;

	for(index = start_of_play; (index < start_of_play + 200) && (index < search_length - 4); index += 2) {
		if(buf[index] == 0x7e) {
			s->number_of_channels = (int32_t)buf[index + 1];
			if(s->number_of_channels == 0) {
				for(; (index < start_of_play + 500) && (index < search_length - 4); index += 2) {
					if((buf[index] == 0xbe) && ((buf[index + 1] == 0x7c) || (buf[index + 1] == 0x3c))) {
						s->number_of_channels = (int32_t)buf[index + 3];
						break;
					}
				}
			} else {
				s->number_of_channels++;
			}
			break;
		}
	}

	if((s->number_of_channels == 0) || (s->number_of_channels > DW_MAX_CHANNELS)) {
		return 0;
	}

	int32_t read_track_commands_offset;
	int32_t do_frame_stuff_offset;

	if(s->old_player) {
		for(index = start_of_play; index < search_length - 4; index += 2) {
			if((buf[index] == 0x70) && (buf[index + 1] == 0x00)) {
				break;
			}
		}
		read_track_commands_offset = index;

		if(buf[index + 2] != 0x10) {
			return 0;
		}
		do_frame_stuff_offset = -1;
	} else {
		for(index = start_of_play; index < search_length - 16; index += 2) {
			if((buf[index] == 0x53) && (buf[index + 1] == 0x68)) {
				break;
			}
		}
		if(index >= (search_length - 16)) {
			return 0;
		}

		if(buf[index + 4] != 0x67) {
			return 0;
		}

		read_track_commands_offset = (int32_t)buf[index + 5] + index + 6;

		if(buf[index + 12] != 0x66) {
			return 0;
		}

		if(buf[index + 13] == 0x00) {
			do_frame_stuff_offset = ((int32_t)buf[index + 14] << 8) | (int32_t)buf[index + 15];
			do_frame_stuff_offset += index + 14;
		} else {
			do_frame_stuff_offset = (int32_t)buf[index + 13] + index + 14;
		}
	}

	for(index = read_track_commands_offset; index < search_length - 4; index += 2) {
		if((buf[index] == 0x6b) && (buf[index + 1] == 0x00)) {
			break;
		}
	}
	if(index >= (search_length - 4)) {
		return 0;
	}

	int32_t do_commands_offset = ((int32_t)buf[index + 2] << 8) | (int32_t)buf[index + 3];
	do_commands_offset += index + 2;

	if(s->old_player) {
		s->periods = dw_periods1;
		s->periods_count = sizeof(dw_periods1) / sizeof(dw_periods1[0]);
	} else {
		for(index = read_track_commands_offset; index < search_length - 6; index += 2) {
			if((buf[index] == 0x45) && (buf[index + 1] == 0xfa) && (buf[index + 4] == 0x32) && (buf[index + 5] == 0x2d)) {
				break;
			}
		}
		if(index >= (search_length - 6)) {
			return 0;
		}

		offset = ((int32_t)buf[index + 2] << 8) | (int32_t)buf[index + 3];
		offset += index + 2;
		if(offset >= (search_length - 72 * 2)) {
			return 0;
		}

		if((buf[offset] == 0x10) && (buf[offset + 1] == 0x00)) {
			s->periods = dw_periods2;
			s->periods_count = sizeof(dw_periods2) / sizeof(dw_periods2[0]);
		} else if((buf[offset] == 0x20) && (buf[offset + 1] == 0x00)) {
			s->periods = dw_periods3;
			s->periods_count = sizeof(dw_periods3) / sizeof(dw_periods3[0]);
		} else {
			return 0;
		}
	}

	for(index = read_track_commands_offset; index < search_length - 6; index += 2) {
		if((buf[index] == 0x6b) && (buf[index + 1] == 0x00)) {
			break;
		}
	}
	if(index >= (search_length - 6)) {
		return 0;
	}

	s->enable_sample_transpose = 0;
	if((buf[index + 4] == 0xd0) && (buf[index + 5] == 0x2d)) {
		s->enable_sample_transpose = 1;
	}

	s->enable_global_transpose = 0;
	s->enable_channel_transpose = 0;
	if((do_frame_stuff_offset != -1) && (do_frame_stuff_offset < (search_length - 12)) &&
	   (buf[do_frame_stuff_offset] == 0x10) && (buf[do_frame_stuff_offset + 1] == 0x28)) {
		if((buf[do_frame_stuff_offset + 4] == 0xd0) && (buf[do_frame_stuff_offset + 5] == 0x3a)) {
			s->enable_global_transpose = 1;
		}
		if((buf[do_frame_stuff_offset + 8] == 0xd0) && (buf[do_frame_stuff_offset + 9] == 0x28)) {
			s->enable_channel_transpose = 1;
		}
	}

	s->enable_arpeggio = 0;
	s->enable_envelopes = 0;
	s->new_sample_cmd = 0;
	s->new_arpeggio_cmd = 0;
	s->new_envelope_cmd = 0;
	s->arpeggio_list_offset = 0;
	s->envelope_list_offset = 0;

	for(index = do_commands_offset; index < search_length - 28; index += 2) {
		if((buf[index] == 0x4e) && ((buf[index + 1] == 0xd2) || (buf[index + 1] == 0xf3))) {
			break;
		}

		if(((buf[index] == 0xb0) && (buf[index + 1] == 0x3c)) || ((buf[index] == 0x0c) && (buf[index + 1] == 0x00))) {
			if((buf[index + 2] == 0x00) && ((buf[index + 4] == 0x65) || (buf[index + 4] == 0x6d))) {
				if((buf[index + 10] == 0xd0) && (buf[index + 11] == 0x40) && (buf[index + 12] == 0x45) && (buf[index + 13] == 0xfa)) {
					if((buf[index + 22] == 0x21) && (buf[index + 23] == 0x4a) && (buf[index + 26] == 0x21) && (buf[index + 27] == 0x4a)) {
						s->enable_arpeggio = 1;
						s->new_arpeggio_cmd = buf[index + 3];
						s->arpeggio_list_offset = (dw_read_s8(buf[index + 14]) << 8) | (int32_t)buf[index + 15];
						s->arpeggio_list_offset += index + 14;
					} else if((buf[index + 22] == 0x21) && (buf[index + 23] == 0x4a) && (buf[index + 26] == 0x11) && (buf[index + 27] == 0x6a)) {
						s->enable_envelopes = 1;
						s->new_envelope_cmd = buf[index + 3];
						s->envelope_list_offset = (dw_read_s8(buf[index + 14]) << 8) | (int32_t)buf[index + 15];
						s->envelope_list_offset += index + 14;
					}
				} else if((buf[index + 10] == 0x4b) && (buf[index + 11] == 0xfa) && (buf[index + 14] == 0xc0) && (buf[index + 15] == 0xfc)) {
					s->new_sample_cmd = buf[index + 3];
				}
			}
		}
	}

	if(!s->old_player && (s->new_sample_cmd == 0)) {
		return 0;
	}

	int32_t jump_table_offset;

	if((index >= 10) && (buf[index - 10] == 0x45) && (buf[index - 9] == 0xfa)) {
		jump_table_offset = (dw_read_s8(buf[index - 8]) << 8) | (int32_t)buf[index - 7];
		jump_table_offset += index - 8;
	} else if((index >= 8) && (buf[index - 8] == 0x45) && (buf[index - 7] == 0xfa)) {
		jump_table_offset = (dw_read_s8(buf[index - 6]) << 8) | (int32_t)buf[index - 5];
		jump_table_offset += index - 6;
	} else if((index >= 10) && (buf[index - 10] == 0x45) && (buf[index - 9] == 0xeb)) {
		jump_table_offset = (dw_read_s8(buf[index - 8]) << 8) | (int32_t)buf[index - 7];
		jump_table_offset += s->start_offset;
	} else {
		return 0;
	}

	s->enable_vibrato = 0;
	int32_t effect_offset;

	if((jump_table_offset + 6 * 2 + 1) < search_length) {
		effect_offset = ((int32_t)buf[jump_table_offset + 6 * 2] << 8) | (int32_t)buf[jump_table_offset + 6 * 2 + 1];
		effect_offset += s->start_offset;
		if((effect_offset >= 0) && (effect_offset < (search_length - 6)) &&
		   (buf[effect_offset] == 0x50) && (buf[effect_offset + 1] == 0xe8) &&
		   (buf[effect_offset + 4] == 0x11) && (buf[effect_offset + 5] == 0x59)) {
			s->enable_vibrato = 1;
		}
	}

	s->enable_volume_fade = 0;
	if((jump_table_offset + 8 * 2 + 1) < search_length) {
		effect_offset = ((int32_t)buf[jump_table_offset + 8 * 2] << 8) | (int32_t)buf[jump_table_offset + 8 * 2 + 1];
		effect_offset += s->start_offset;
		if((effect_offset >= 0) && (effect_offset < (search_length - 2)) &&
		   (buf[effect_offset] == 0x17) && (buf[effect_offset + 1] == 0x59)) {
			s->enable_volume_fade = 1;
		}
	}

	s->enable_half_volume = 0;
	if((jump_table_offset + 9 * 2 + 1) < search_length) {
		effect_offset = ((int32_t)buf[jump_table_offset + 8 * 2] << 8) | (int32_t)buf[jump_table_offset + 8 * 2 + 1];
		effect_offset += s->start_offset;
		if((effect_offset >= 0) && (effect_offset < (search_length - 2)) &&
		   (buf[effect_offset] == 0x50) && (buf[effect_offset + 1] == 0xe8)) {
			effect_offset = ((int32_t)buf[jump_table_offset + 9 * 2] << 8) | (int32_t)buf[jump_table_offset + 9 * 2 + 1];
			effect_offset += s->start_offset;
			if((effect_offset < (search_length - 2)) &&
			   (buf[effect_offset] == 0x51) && (buf[effect_offset + 1] == 0xe8)) {
				s->enable_half_volume = 1;
			}
		}
	}

	s->enable_global_volume_fade = 0;
	if((jump_table_offset + 11 * 2 + 1) < search_length) {
		effect_offset = ((int32_t)buf[jump_table_offset + 11 * 2] << 8) | (int32_t)buf[jump_table_offset + 11 * 2 + 1];
		effect_offset += s->start_offset;
		if((effect_offset >= 0) && (effect_offset < (search_length - 2)) &&
		   (buf[effect_offset] == 0x17) && (buf[effect_offset + 1] == 0x59)) {
			s->enable_global_volume_fade = 1;
		}
	}

	s->enable_delay_speed = 0;
	if((jump_table_offset + 10 * 2 + 1) < search_length) {
		effect_offset = ((int32_t)buf[jump_table_offset + 10 * 2] << 8) | (int32_t)buf[jump_table_offset + 10 * 2 + 1];
		effect_offset += s->start_offset;
		if((effect_offset >= 0) && (effect_offset < (search_length - 4)) &&
		   (buf[effect_offset] == 0x10) && (buf[effect_offset + 1] == 0x19) &&
		   (buf[effect_offset + 2] == 0x17) && (buf[effect_offset + 3] == 0x40)) {
			s->enable_delay_speed = 1;
		}
	}

	s->enable_set_global_volume = 1;
	if((jump_table_offset + 12 * 2 + 1) < search_length) {
		effect_offset = ((int32_t)buf[jump_table_offset + 12 * 2] << 8) | (int32_t)buf[jump_table_offset + 12 * 2 + 1];
		effect_offset += s->start_offset;
		if((effect_offset >= 0) && (effect_offset < (search_length - 4)) &&
		   (buf[effect_offset + 2] == 0x42) && (buf[effect_offset + 3] == 0x41)) {
			s->enable_set_global_volume = 0;
		}
	}

	return 1;
}

// [=]===^=[ dw_test_module ]=====================================================================[=]
static int32_t dw_test_module(struct davidwhittaker_state *s) {
	if(s->module_len < 2048) {
		return 0;
	}

	uint8_t *buf = s->module_data;
	int32_t search_length = (int32_t)s->module_len;
	if(search_length > DW_SCAN_BUFFER_LEN) {
		search_length = DW_SCAN_BUFFER_LEN;
	}

	// SC68 modules are handled by a converter elsewhere; ignore them here.
	if((buf[0] == 0x53) && (buf[1] == 0x43) && (buf[2] == 0x36) && (buf[3] == 0x38)) {
		return 0;
	}

	if(!dw_extract_info_from_init(s, buf, search_length)) {
		return 0;
	}
	if(!dw_extract_info_from_play(s, buf, search_length)) {
		return 0;
	}
	return 1;
}

// [=]===^=[ dw_find_effect_byte_count ]==========================================================[=]
// Returns extra byte count for an effect, or -1 to stop parsing.
static int32_t dw_find_effect_byte_count(struct davidwhittaker_state *s, uint8_t effect) {
	if(s->old_player) {
		switch((int32_t)effect) {
			case 0:
			case 1: {
				return -1;
			}
			case 2: {
				return 1;
			}
		}
		return -2;
	}

	switch(effect) {
		case DW_EFFECT_END_OF_TRACK:
		case DW_EFFECT_STOP_SONG: {
			return -1;
		}

		case DW_EFFECT_MUTE:
		case DW_EFFECT_WAIT_UNTIL_NEXT_ROW:
		case DW_EFFECT_STOP_VIBRATO:
		case DW_EFFECT_STOP_SOUNDFX: {
			return 0;
		}

		case DW_EFFECT_GLOBAL_TRANSPOSE:
		case DW_EFFECT_8:
		case DW_EFFECT_SET_SPEED:
		case DW_EFFECT_GLOBAL_VOLUME_FADE:
		case DW_EFFECT_SET_GLOBAL_VOLUME: {
			return 1;
		}

		case DW_EFFECT_SLIDE:
		case DW_EFFECT_START_VIBRATO: {
			return 2;
		}

		case DW_EFFECT_9: {
			if(s->enable_half_volume) {
				return 0;
			}
			return 2;
		}

		case DW_EFFECT_START_OR_STOP_SOUNDFX: {
			if(s->enable_set_global_volume) {
				return 1;
			}
			return 0;
		}
	}

	return -2;
}

// [=]===^=[ dw_find_track ]======================================================================[=]
static struct dw_track *dw_find_track(struct davidwhittaker_state *s, uint32_t offset) {
	for(uint32_t i = 0; i < s->tracks_count; ++i) {
		if(s->tracks[i].offset == offset) {
			return &s->tracks[i];
		}
	}
	return 0;
}

// [=]===^=[ dw_add_track ]=======================================================================[=]
static struct dw_track *dw_add_track(struct davidwhittaker_state *s, uint32_t offset, uint8_t *data, uint32_t length) {
	if(s->tracks_count == s->tracks_capacity) {
		uint32_t new_cap = s->tracks_capacity ? s->tracks_capacity * 2 : 64;
		struct dw_track *nt = (struct dw_track *)realloc(s->tracks, new_cap * sizeof(struct dw_track));
		if(!nt) {
			return 0;
		}
		s->tracks = nt;
		s->tracks_capacity = new_cap;
	}
	struct dw_track *t = &s->tracks[s->tracks_count++];
	t->offset = offset;
	t->data = data;
	t->length = length;
	return t;
}

// [=]===^=[ dw_load_track ]======================================================================[=]
// Reads a track starting at module-absolute position `start`, returns malloced bytes.
static uint8_t *dw_load_track(struct davidwhittaker_state *s, uint32_t start, uint32_t *out_length) {
	uint32_t pos = start;
	uint32_t cap = 64;
	uint32_t len = 0;
	uint8_t *buf = (uint8_t *)malloc(cap);
	if(!buf) {
		return 0;
	}

	for(;;) {
		if(pos >= s->module_len) {
			free(buf);
			return 0;
		}
		uint8_t byt = s->module_data[pos++];

		if(len == cap) {
			cap *= 2;
			uint8_t *nb = (uint8_t *)realloc(buf, cap);
			if(!nb) {
				free(buf);
				return 0;
			}
			buf = nb;
		}
		buf[len++] = byt;

		if((byt & 0x80) != 0) {
			if(byt >= 0xe0) {
				continue;
			}
			if(!s->old_player && (byt >= s->new_sample_cmd)) {
				continue;
			}
			if(s->enable_envelopes && (byt >= s->new_envelope_cmd)) {
				continue;
			}
			if(s->enable_arpeggio && (byt >= s->new_arpeggio_cmd)) {
				continue;
			}

			uint8_t effect = (uint8_t)(byt & 0x7f);
			int32_t effect_count = dw_find_effect_byte_count(s, effect);
			if(effect_count == -1) {
				break;
			}
			if(effect_count < 0) {
				free(buf);
				return 0;
			}

			while(effect_count > 0) {
				if(pos >= s->module_len) {
					free(buf);
					return 0;
				}
				if(len == cap) {
					cap *= 2;
					uint8_t *nb = (uint8_t *)realloc(buf, cap);
					if(!nb) {
						free(buf);
						return 0;
					}
					buf = nb;
				}
				buf[len++] = s->module_data[pos++];
				effect_count--;
			}
		}
	}

	*out_length = len;
	return buf;
}

// [=]===^=[ dw_parse_track_for_meta ]============================================================[=]
// Walks an already-loaded track to find max arpeggio/envelope numbers and any
// new-position-list offset hidden in Effect9 (when half-volume is disabled).
static int32_t dw_parse_track_for_meta(struct davidwhittaker_state *s, uint8_t *trk, uint32_t trk_len, int32_t *new_position_list_offset, int32_t *out_arp, int32_t *out_env) {
	*new_position_list_offset = 0;
	*out_arp = 0;
	*out_env = 0;

	uint32_t index = 0;
	while(index < trk_len) {
		uint8_t byt = trk[index++];
		if((byt & 0x80) == 0) {
			continue;
		}

		if(byt >= 0xe0) {
			continue;
		}
		if(!s->old_player && (byt >= s->new_sample_cmd)) {
			continue;
		}
		if(s->enable_envelopes && (byt >= s->new_envelope_cmd)) {
			int32_t v = (int32_t)byt - (int32_t)s->new_envelope_cmd + 1;
			if(v > *out_env) {
				*out_env = v;
			}
			continue;
		}
		if(s->enable_arpeggio && (byt >= s->new_arpeggio_cmd)) {
			int32_t v = (int32_t)byt - (int32_t)s->new_arpeggio_cmd + 1;
			if(v > *out_arp) {
				*out_arp = v;
			}
			continue;
		}

		uint8_t effect = (uint8_t)(byt & 0x7f);
		int32_t effect_count = dw_find_effect_byte_count(s, effect);
		if(effect_count == -1) {
			break;
		}
		if(effect_count < 0) {
			return 0;
		}

		index += (uint32_t)effect_count;
		if(index > trk_len) {
			return 0;
		}

		if((effect == DW_EFFECT_9) && !s->enable_half_volume) {
			*new_position_list_offset = ((int32_t)trk[index - 2] << 8) | (int32_t)trk[index - 1];
		}
	}

	return 1;
}

// [=]===^=[ dw_load_or_get_track ]===============================================================[=]
static struct dw_track *dw_load_or_get_track(struct davidwhittaker_state *s, uint32_t offset) {
	struct dw_track *t = dw_find_track(s, offset);
	if(t) {
		return t;
	}
	uint32_t len = 0;
	uint32_t abs_pos = offset + (uint32_t)s->start_offset;
	if(abs_pos >= s->module_len) {
		return 0;
	}
	uint8_t *bytes = dw_load_track(s, abs_pos, &len);
	if(!bytes) {
		return 0;
	}
	t = dw_add_track(s, offset, bytes, len);
	if(!t) {
		free(bytes);
		return 0;
	}
	return t;
}

// [=]===^=[ dw_load_position_list ]==============================================================[=]
// Loads a position list and triggers loading of all referenced tracks.
static int32_t dw_load_position_list(struct davidwhittaker_state *s, uint32_t start_position, struct dw_position_list *out_list, int32_t *max_arpeggios, int32_t *max_envelopes) {
	out_list->track_offsets = 0;
	out_list->length = 0;
	out_list->restart_position = 0;
	*max_arpeggios = 0;
	*max_envelopes = 0;

	if(start_position == 0) {
		return 0;
	}

	int64_t min_position = (int64_t)start_position;
	int64_t max_position = (int64_t)start_position;
	int64_t pos = (int64_t)start_position + s->start_offset;

	uint32_t cap = 16;
	uint32_t count = 0;
	uint32_t *list = (uint32_t *)malloc(cap * sizeof(uint32_t));
	if(!list) {
		return 0;
	}
	uint16_t restart_position = 0;

	for(;;) {
		uint32_t track_offset;
		if(s->uses_32bit_pointers) {
			if((uint64_t)(pos + 4) > (uint64_t)s->module_len) {
				free(list);
				return 0;
			}
			track_offset = dw_read_b_u32(s->module_data + pos);
			pos += 4;
		} else {
			if((uint64_t)(pos + 2) > (uint64_t)s->module_len) {
				free(list);
				return 0;
			}
			track_offset = (uint32_t)dw_read_b_u16(s->module_data + pos);
			pos += 2;
		}

		if((track_offset == 0) || (track_offset >= s->module_len) || ((track_offset & 0x8000) != 0)) {
			break;
		}

		int64_t current_position = pos;

		if(count == cap) {
			cap *= 2;
			uint32_t *nl = (uint32_t *)realloc(list, cap * sizeof(uint32_t));
			if(!nl) {
				free(list);
				return 0;
			}
			list = nl;
		}
		list[count++] = track_offset;

		struct dw_track *trk = dw_load_or_get_track(s, track_offset);
		if(!trk) {
			free(list);
			return 0;
		}

		int32_t new_pos_list_offset = 0;
		int32_t arp_count = 0;
		int32_t env_count = 0;
		if(!dw_parse_track_for_meta(s, trk->data, trk->length, &new_pos_list_offset, &arp_count, &env_count)) {
			free(list);
			return 0;
		}

		if(arp_count > *max_arpeggios) {
			*max_arpeggios = arp_count;
		}
		if(env_count > *max_envelopes) {
			*max_envelopes = env_count;
		}

		if((new_pos_list_offset != 0) && ((new_pos_list_offset < min_position) || (new_pos_list_offset > max_position))) {
			restart_position = (uint16_t)count;
			current_position = (int64_t)new_pos_list_offset + s->start_offset;
		}

		if(current_position < min_position) {
			min_position = current_position;
		}
		if(current_position > max_position) {
			max_position = current_position;
		}

		pos = current_position;
	}

	out_list->track_offsets = list;
	out_list->length = count;
	out_list->restart_position = restart_position;
	return 1;
}

// [=]===^=[ dw_load_sub_song_info_and_tracks ]===================================================[=]
static int32_t dw_load_sub_song_info_and_tracks(struct davidwhittaker_state *s, int32_t *max_arpeggios, int32_t *max_envelopes) {
	*max_arpeggios = 0;
	*max_envelopes = 0;

	uint32_t cap = 4;
	s->song_info_count = 0;
	s->song_info_list = (struct dw_song_info *)calloc(cap, sizeof(struct dw_song_info));
	if(!s->song_info_list) {
		return 0;
	}

	int64_t pos = s->sub_song_list_offset;
	int64_t min_position_offset = 0x7fffffffffffffffLL;

	for(;;) {
		uint16_t song_speed;
		uint8_t delay_speed;

		if((uint64_t)(pos + 8) >= (uint64_t)min_position_offset) {
			break;
		}

		if(s->enable_delay_counter) {
			if((uint64_t)(pos + 2) > (uint64_t)s->module_len) {
				break;
			}
			song_speed = (uint16_t)s->module_data[pos];
			delay_speed = s->module_data[pos + 1];
			pos += 2;
		} else {
			if((uint64_t)(pos + 2) > (uint64_t)s->module_len) {
				break;
			}
			song_speed = dw_read_b_u16(s->module_data + pos);
			delay_speed = 0;
			pos += 2;
		}

		if(song_speed > 255) {
			break;
		}

		if((uint64_t)(pos + (uint64_t)s->number_of_channels * (s->uses_32bit_pointers ? 4 : 2)) > (uint64_t)s->module_len) {
			return 0;
		}

		uint32_t position_offsets[DW_MAX_CHANNELS];
		for(int32_t i = 0; i < s->number_of_channels; ++i) {
			if(s->uses_32bit_pointers) {
				position_offsets[i] = dw_read_b_u32(s->module_data + pos);
				pos += 4;
			} else {
				position_offsets[i] = (uint32_t)dw_read_b_u16(s->module_data + pos);
				pos += 2;
			}
			int64_t cand = (int64_t)position_offsets[i] + s->start_offset;
			if(cand < min_position_offset) {
				min_position_offset = cand;
			}
		}

		int64_t saved_pos = pos;

		if(s->song_info_count == cap) {
			cap *= 2;
			struct dw_song_info *ni = (struct dw_song_info *)realloc(s->song_info_list, cap * sizeof(struct dw_song_info));
			if(!ni) {
				return 0;
			}
			memset(ni + s->song_info_count, 0, (cap - s->song_info_count) * sizeof(struct dw_song_info));
			s->song_info_list = ni;
		}

		struct dw_song_info *song = &s->song_info_list[s->song_info_count];
		memset(song, 0, sizeof(*song));
		song->speed = song_speed;
		song->delay_counter_speed = delay_speed;

		for(int32_t i = 0; i < s->number_of_channels; ++i) {
			int32_t arp = 0;
			int32_t env = 0;
			if(!dw_load_position_list(s, position_offsets[i], &song->position_lists[i], &arp, &env)) {
				return 0;
			}
			if(arp > *max_arpeggios) {
				*max_arpeggios = arp;
			}
			if(env > *max_envelopes) {
				*max_envelopes = env;
			}
		}

		s->song_info_count++;
		pos = saved_pos;
	}

	return (s->song_info_count > 0) ? 1 : 0;
}

// [=]===^=[ dw_load_byte_seq ]===================================================================[=]
// Reads a 0x80-terminated byte sequence starting at `pos`. Returns malloced data.
static uint8_t *dw_load_byte_seq(struct davidwhittaker_state *s, uint32_t pos, uint32_t *out_length) {
	uint32_t cap = 16;
	uint32_t len = 0;
	uint8_t *buf = (uint8_t *)malloc(cap);
	if(!buf) {
		return 0;
	}
	for(;;) {
		if(pos >= s->module_len) {
			free(buf);
			return 0;
		}
		uint8_t v = s->module_data[pos++];

		if(len == cap) {
			cap *= 2;
			uint8_t *nb = (uint8_t *)realloc(buf, cap);
			if(!nb) {
				free(buf);
				return 0;
			}
			buf = nb;
		}
		buf[len++] = v;

		if((v & 0x80) != 0) {
			break;
		}
	}
	*out_length = len;
	return buf;
}

// [=]===^=[ dw_load_arpeggios ]==================================================================[=]
static int32_t dw_load_arpeggios(struct davidwhittaker_state *s, int32_t number_of_arpeggios) {
	if(number_of_arpeggios == 0) {
		s->arpeggios_count = 1;
		s->arpeggios = (struct dw_byte_seq *)calloc(1, sizeof(struct dw_byte_seq));
		if(!s->arpeggios) {
			return 0;
		}
		s->arpeggios[0].data = (uint8_t *)malloc(1);
		if(!s->arpeggios[0].data) {
			return 0;
		}
		s->arpeggios[0].data[0] = 0x80;
		s->arpeggios[0].length = 1;
		return 1;
	}

	s->arpeggios = (struct dw_byte_seq *)calloc((size_t)number_of_arpeggios, sizeof(struct dw_byte_seq));
	if(!s->arpeggios) {
		return 0;
	}
	s->arpeggios_count = (uint32_t)number_of_arpeggios;

	if((uint64_t)(s->arpeggio_list_offset + number_of_arpeggios * 2) > (uint64_t)s->module_len) {
		return 0;
	}

	for(int32_t i = 0; i < number_of_arpeggios; ++i) {
		uint16_t off = dw_read_b_u16(s->module_data + s->arpeggio_list_offset + i * 2);
		uint32_t abs_pos = (uint32_t)off + (uint32_t)s->start_offset;
		uint32_t len = 0;
		uint8_t *bytes = dw_load_byte_seq(s, abs_pos, &len);
		if(!bytes) {
			return 0;
		}
		s->arpeggios[i].data = bytes;
		s->arpeggios[i].length = len;
	}

	return 1;
}

// [=]===^=[ dw_load_envelopes ]==================================================================[=]
static int32_t dw_load_envelopes(struct davidwhittaker_state *s, int32_t number_of_envelopes) {
	if(number_of_envelopes == 0) {
		s->envelopes = 0;
		s->envelopes_count = 0;
		return 1;
	}

	s->envelopes = (struct dw_byte_seq *)calloc((size_t)number_of_envelopes, sizeof(struct dw_byte_seq));
	if(!s->envelopes) {
		return 0;
	}
	s->envelopes_count = (uint32_t)number_of_envelopes;

	if((uint64_t)(s->envelope_list_offset + number_of_envelopes * 2) > (uint64_t)s->module_len) {
		return 0;
	}

	for(int32_t i = 0; i < number_of_envelopes; ++i) {
		uint16_t off = dw_read_b_u16(s->module_data + s->envelope_list_offset + i * 2);
		// Envelope: first byte is the speed (read at offset-1), then the body up to high-bit-set byte.
		int64_t base = (int64_t)off + s->start_offset - 1;
		if((base < 0) || (base >= (int64_t)s->module_len)) {
			return 0;
		}

		uint32_t cap = 16;
		uint32_t len = 0;
		uint8_t *buf = (uint8_t *)malloc(cap);
		if(!buf) {
			return 0;
		}
		buf[len++] = s->module_data[base];

		uint32_t pos = (uint32_t)(base + 1);
		for(;;) {
			if(pos >= s->module_len) {
				free(buf);
				return 0;
			}
			uint8_t v = s->module_data[pos++];

			if(len == cap) {
				cap *= 2;
				uint8_t *nb = (uint8_t *)realloc(buf, cap);
				if(!nb) {
					free(buf);
					return 0;
				}
				buf = nb;
			}
			buf[len++] = v;

			if((v & 0x80) != 0) {
				break;
			}
		}

		s->envelopes[i].data = buf;
		s->envelopes[i].length = len;
	}

	return 1;
}

// [=]===^=[ dw_load_channel_volumes ]============================================================[=]
static int32_t dw_load_channel_volumes(struct davidwhittaker_state *s) {
	if(!s->old_player) {
		return 1;
	}
	if((uint64_t)(s->channel_volume_offset + s->number_of_channels * 2) > (uint64_t)s->module_len) {
		return 0;
	}
	for(int32_t i = 0; i < s->number_of_channels; ++i) {
		s->channel_volumes[i] = dw_read_b_u16(s->module_data + s->channel_volume_offset + i * 2);
	}
	return 1;
}

// [=]===^=[ dw_load_sample_info ]================================================================[=]
static int32_t dw_load_sample_info(struct davidwhittaker_state *s) {
	s->samples = (struct dw_sample *)calloc((size_t)s->number_of_samples, sizeof(struct dw_sample));
	if(!s->samples) {
		return 0;
	}
	s->samples_count = (uint32_t)s->number_of_samples;

	if(s->old_player) {
		for(int32_t i = 0; i < s->number_of_samples; ++i) {
			struct dw_sample *sm = &s->samples[i];
			sm->sample_number = (int16_t)i;
			sm->loop_start = -1;
			sm->volume = 64;
		}
		return 1;
	}

	int64_t pos = s->sample_info_offset;

	for(int32_t i = 0; i < s->number_of_samples; ++i) {
		struct dw_sample *sm = &s->samples[i];
		sm->sample_number = (int16_t)i;

		// Skip 4-byte sample data pointer.
		pos += 4;

		if((uint64_t)(pos + 6) > (uint64_t)s->module_len) {
			return 0;
		}
		sm->loop_start = dw_read_b_i32(s->module_data + pos);
		pos += 4;
		sm->length = (uint32_t)dw_read_b_u16(s->module_data + pos) * 2u;
		pos += 2;

		// Fix for Jaws.
		if((sm->loop_start != -1) && (sm->loop_start > 64 * 1024)) {
			sm->loop_start = -1;
		}

		if(s->uses_32bit_pointers) {
			pos += 2; // padding

			if((uint64_t)(pos + 4) > (uint64_t)s->module_len) {
				return 0;
			}
			sm->fine_tune_period = dw_read_b_u16(s->module_data + pos);
			pos += 2;
			sm->volume = dw_read_b_u16(s->module_data + pos);
			pos += 2;
			sm->transpose = 0;
		} else {
			if((uint64_t)(pos + 2) > (uint64_t)s->module_len) {
				return 0;
			}
			sm->fine_tune_period = dw_read_b_u16(s->module_data + pos);
			pos += 2;

			if(!s->enable_envelopes) {
				if((uint64_t)(pos + 4) > (uint64_t)s->module_len) {
					return 0;
				}
				sm->volume = dw_read_b_u16(s->module_data + pos);
				pos += 2;
				sm->transpose = (int8_t)s->module_data[pos];
				pos += 2; // includes 1 byte padding
			} else {
				sm->volume = 64;
				sm->transpose = 0;
			}
		}
	}

	return 1;
}

// [=]===^=[ dw_load_sample_data ]================================================================[=]
static int32_t dw_load_sample_data(struct davidwhittaker_state *s) {
	int64_t pos = s->sample_data_offset;

	for(int32_t i = 0; i < s->number_of_samples; ++i) {
		struct dw_sample *sm = &s->samples[i];

		if((uint64_t)(pos + 6) > (uint64_t)s->module_len) {
			return 0;
		}
		sm->length = dw_read_b_u32(s->module_data + pos);
		pos += 4;

		uint16_t frequency = dw_read_b_u16(s->module_data + pos);
		pos += 2;
		if(frequency == 0) {
			return 0;
		}
		sm->fine_tune_period = (uint16_t)(3579545u / frequency);

		if((uint64_t)(pos + sm->length) > (uint64_t)s->module_len) {
			return 0;
		}

		// Allocate writable copy because square waveform sample is mutated.
		sm->sample_data = (int8_t *)malloc(sm->length > 0 ? sm->length : 1);
		if(!sm->sample_data) {
			return 0;
		}
		memcpy(sm->sample_data, s->module_data + pos, sm->length);
		pos += sm->length;
	}

	if(s->enable_square_waveform && (s->square_waveform_sample_number >= 0) &&
	   ((uint32_t)s->square_waveform_sample_number < s->samples_count)) {
		struct dw_sample *sm = &s->samples[s->square_waveform_sample_number];
		// Reallocate to the square length if larger.
		if(s->square_waveform_sample_length > sm->length) {
			int8_t *nd = (int8_t *)realloc(sm->sample_data, s->square_waveform_sample_length);
			if(!nd) {
				return 0;
			}
			memset(nd + sm->length, 0, s->square_waveform_sample_length - sm->length);
			sm->sample_data = nd;
		}
		sm->length = s->square_waveform_sample_length;
	}

	return 1;
}

// [=]===^=[ dw_initialize_square_waveform ]======================================================[=]
static void dw_initialize_square_waveform(struct davidwhittaker_state *s) {
	if(!s->enable_square_waveform) {
		return;
	}
	if((s->square_waveform_sample_number < 0) || ((uint32_t)s->square_waveform_sample_number >= s->samples_count)) {
		return;
	}
	struct dw_sample *sm = &s->samples[s->square_waveform_sample_number];
	int32_t half_length = (int32_t)sm->length / 2;
	for(int32_t i = 0; i < half_length; ++i) {
		sm->sample_data[i] = s->square_byte1;
		sm->sample_data[i + half_length] = s->square_byte2;
	}
	s->playing_info.square_change_position = s->square_change_min_position;
	s->playing_info.square_change_direction = 0;
}

// [=]===^=[ dw_change_square_waveform ]==========================================================[=]
static void dw_change_square_waveform(struct davidwhittaker_state *s) {
	if(!s->enable_square_waveform) {
		return;
	}
	if((s->square_waveform_sample_number < 0) || ((uint32_t)s->square_waveform_sample_number >= s->samples_count)) {
		return;
	}
	int8_t *square_waveform = s->samples[s->square_waveform_sample_number].sample_data;
	uint32_t sample_length = s->samples[s->square_waveform_sample_number].length;

	if(s->playing_info.square_change_direction) {
		for(uint8_t i = 0; i < s->square_change_speed; ++i) {
			uint16_t p = (uint16_t)(s->playing_info.square_change_position + i);
			if(p < sample_length) {
				square_waveform[p] = s->square_byte2;
			}
		}
		s->playing_info.square_change_position = (uint16_t)(s->playing_info.square_change_position - s->square_change_speed);
		if(s->playing_info.square_change_position == s->square_change_min_position) {
			s->playing_info.square_change_direction = 0;
		}
	} else {
		for(uint8_t i = 0; i < s->square_change_speed; ++i) {
			uint16_t p = (uint16_t)(s->playing_info.square_change_position + i);
			if(p < sample_length) {
				square_waveform[p] = s->square_byte1;
			}
		}
		s->playing_info.square_change_position = (uint16_t)(s->playing_info.square_change_position + s->square_change_speed);
		if(s->playing_info.square_change_position == s->square_change_max_position) {
			s->playing_info.square_change_direction = 1;
		}
	}
}

// [=]===^=[ dw_initialize_channel_info ]=========================================================[=]
static void dw_initialize_channel_info(struct davidwhittaker_state *s, int32_t sub_song) {
	struct dw_song_info *song = &s->song_info_list[sub_song];

	for(int32_t i = 0; i < s->number_of_channels; ++i) {
		struct dw_channel_info *ci = &s->channels[i];
		memset(ci, 0, sizeof(*ci));

		ci->channel_number = i;
		ci->speed_counter = 1;
		ci->slide_enabled = 0;
		ci->vibrato_direction = 0;
		ci->transpose = 0;
		ci->enable_half_volume = 0;
		ci->envelope_list = 0;
		ci->envelope_list_length = 0;
		ci->position_list = song->position_lists[i].track_offsets;
		ci->position_list_length = song->position_lists[i].length;
		ci->current_position = 1;
		ci->restart_position = song->position_lists[i].restart_position;

		if(s->enable_arpeggio && (s->arpeggios_count > 0)) {
			ci->arpeggio_list = s->arpeggios[0].data;
			ci->arpeggio_list_length = s->arpeggios[0].length;
			ci->arpeggio_list_position = 0;
		}

		if(ci->position_list_length == 0) {
			ci->track_data = s->empty_track;
			ci->track_data_length = 1;
		} else {
			struct dw_track *trk = dw_find_track(s, ci->position_list[0]);
			if(trk) {
				ci->track_data = trk->data;
				ci->track_data_length = trk->length;
			} else {
				ci->track_data = s->empty_track;
				ci->track_data_length = 1;
			}
		}
		ci->track_data_position = 0;
	}
}

// [=]===^=[ dw_initialize_sound ]================================================================[=]
static void dw_initialize_sound(struct davidwhittaker_state *s, int32_t sub_song) {
	struct dw_song_info *song = &s->song_info_list[sub_song];

	memset(&s->playing_info, 0, sizeof(s->playing_info));
	s->playing_info.transpose = 0;
	s->playing_info.speed = song->speed;
	s->playing_info.volume_fade_speed = 0;
	s->playing_info.global_volume = 64;
	s->playing_info.global_volume_fade_speed = 0;
	s->playing_info.extra_counter = 1;
	s->playing_info.delay_counter = 0;
	s->playing_info.delay_counter_speed = song->delay_counter_speed;

	if(s->enable_delay_multiply) {
		s->playing_info.delay_counter_speed = (uint8_t)(s->playing_info.delay_counter_speed * 16);
	}

	dw_initialize_channel_info(s, sub_song);
	dw_initialize_square_waveform(s);

	s->end_reached = 0;
}

// [=]===^=[ dw_handle_end_of_track_effect ]======================================================[=]
static void dw_handle_end_of_track_effect(struct davidwhittaker_state *s, struct dw_channel_info *ci) {
	if(ci->current_position >= ci->position_list_length) {
		ci->current_position = (uint16_t)(ci->restart_position + 1);
		s->end_reached = 1;

		s->playing_info.transpose = 0;
		s->playing_info.volume_fade_speed = 0;
		s->playing_info.global_volume_fade_speed = 0;
		if(s->playing_info.global_volume == 0) {
			s->playing_info.global_volume = 64;
		}
	} else {
		ci->current_position++;
	}

	if(ci->position_list_length == 0) {
		ci->track_data = s->empty_track;
		ci->track_data_length = 1;
	} else {
		uint16_t idx = (uint16_t)(ci->current_position - 1);
		if(idx >= ci->position_list_length) {
			idx = 0;
		}
		struct dw_track *trk = dw_find_track(s, ci->position_list[idx]);
		if(trk) {
			ci->track_data = trk->data;
			ci->track_data_length = trk->length;
		} else {
			ci->track_data = s->empty_track;
			ci->track_data_length = 1;
		}
	}
	ci->track_data_position = 0;
}

// [=]===^=[ dw_stop_module ]=====================================================================[=]
static void dw_stop_module(struct davidwhittaker_state *s) {
	s->playing_info.transpose = 0;
	s->playing_info.volume_fade_speed = 0;
	s->playing_info.global_volume = 64;
	s->playing_info.global_volume_fade_speed = 0;

	for(int32_t i = 0; i < s->number_of_channels; ++i) {
		paula_mute(&s->paula, i);
	}

	dw_initialize_channel_info(s, s->current_song);
	dw_initialize_square_waveform(s);
	s->end_reached = 1;
}

// [=]===^=[ dw_do_effects ]======================================================================[=]
// Returns 1 to stop processing track commands for this row, 0 to continue.
static int32_t dw_do_effects(struct davidwhittaker_state *s, struct dw_channel_info *ci, int32_t channel_idx, uint8_t effect) {
	if(s->old_player) {
		switch((int32_t)effect) {
			case 0: {
				dw_handle_end_of_track_effect(s, ci);
				break;
			}
			case 1: {
				dw_stop_module(s);
				return 1;
			}
			case 2: {
				break;
			}
		}
		return 0;
	}

	switch(effect) {
		case DW_EFFECT_END_OF_TRACK: {
			dw_handle_end_of_track_effect(s, ci);
			break;
		}

		case DW_EFFECT_SLIDE: {
			if((ci->track_data_position + 2) > (int32_t)ci->track_data_length) {
				return 1;
			}
			ci->slide_value = 0;
			ci->slide_speed = (int8_t)ci->track_data[ci->track_data_position++];
			ci->slide_counter = ci->track_data[ci->track_data_position++];
			ci->slide_enabled = 1;
			break;
		}

		case DW_EFFECT_MUTE: {
			paula_mute(&s->paula, channel_idx);
			return 1;
		}

		case DW_EFFECT_WAIT_UNTIL_NEXT_ROW: {
			return 1;
		}

		case DW_EFFECT_STOP_SONG: {
			dw_stop_module(s);
			return 1;
		}

		case DW_EFFECT_GLOBAL_TRANSPOSE: {
			if(s->enable_global_transpose) {
				if(ci->track_data_position < (int32_t)ci->track_data_length) {
					s->playing_info.transpose = (int8_t)ci->track_data[ci->track_data_position++];
				}
			}
			break;
		}

		case DW_EFFECT_START_VIBRATO: {
			if(s->enable_vibrato) {
				if((ci->track_data_position + 2) > (int32_t)ci->track_data_length) {
					return 1;
				}
				ci->vibrato_direction = -1;
				ci->vibrato_speed = ci->track_data[ci->track_data_position++];
				ci->vibrato_max_value = ci->track_data[ci->track_data_position++];
				ci->vibrato_value = 0;
			}
			break;
		}

		case DW_EFFECT_STOP_VIBRATO: {
			if(s->enable_vibrato) {
				ci->vibrato_direction = 0;
			}
			break;
		}

		case DW_EFFECT_8: {
			if(s->enable_volume_fade) {
				if(ci->track_data_position < (int32_t)ci->track_data_length) {
					s->playing_info.volume_fade_speed = ci->track_data[ci->track_data_position++];
				}
			} else if(s->enable_channel_transpose) {
				if(ci->track_data_position < (int32_t)ci->track_data_length) {
					ci->transpose = (int8_t)ci->track_data[ci->track_data_position++];
				}
			} else if(s->enable_half_volume) {
				ci->enable_half_volume = 1;
			}
			break;
		}

		case DW_EFFECT_9: {
			if(s->enable_half_volume) {
				ci->enable_half_volume = 0;
			} else {
				ci->track_data_position += 2;
			}
			break;
		}

		case DW_EFFECT_SET_SPEED: {
			if(s->enable_delay_speed) {
				if(ci->track_data_position < (int32_t)ci->track_data_length) {
					s->playing_info.delay_counter_speed = ci->track_data[ci->track_data_position++];
				}
			} else {
				if(ci->track_data_position < (int32_t)ci->track_data_length) {
					s->playing_info.speed = ci->track_data[ci->track_data_position++];
				}
			}
			break;
		}

		case DW_EFFECT_GLOBAL_VOLUME_FADE: {
			if(ci->track_data_position < (int32_t)ci->track_data_length) {
				s->playing_info.global_volume_fade_speed = ci->track_data[ci->track_data_position++];
			}
			s->playing_info.global_volume_fade_counter = s->playing_info.global_volume_fade_speed;
			break;
		}

		case DW_EFFECT_SET_GLOBAL_VOLUME: {
			if(s->enable_set_global_volume) {
				if(ci->track_data_position < (int32_t)ci->track_data_length) {
					s->playing_info.global_volume = ci->track_data[ci->track_data_position++];
				}
			} else {
				ci->track_data_position++;
			}
			break;
		}

		case DW_EFFECT_START_OR_STOP_SOUNDFX: {
			if(s->enable_set_global_volume) {
				ci->track_data_position++;
			}
			break;
		}

		case DW_EFFECT_STOP_SOUNDFX: {
			break;
		}
	}

	return 0;
}

// [=]===^=[ dw_do_track_command ]================================================================[=]
// Returns 1 to break out of the read-track loop (row consumed), 0 to keep parsing.
static int32_t dw_do_track_command(struct davidwhittaker_state *s, struct dw_channel_info *ci, int32_t channel_idx, uint8_t track_command) {
	if(track_command >= 0xe0) {
		ci->speed = (uint16_t)(((uint16_t)track_command - 0xdf) * s->playing_info.speed);
		return 0;
	}
	if(!s->old_player && (track_command >= s->new_sample_cmd)) {
		uint32_t idx = (uint32_t)track_command - (uint32_t)s->new_sample_cmd;
		if(idx < s->samples_count) {
			ci->current_sample_info = &s->samples[idx];
		}
		return 0;
	}
	if(s->enable_envelopes && (track_command >= s->new_envelope_cmd)) {
		uint32_t idx = (uint32_t)track_command - (uint32_t)s->new_envelope_cmd;
		if(idx < s->envelopes_count) {
			ci->envelope_list = s->envelopes[idx].data;
			ci->envelope_list_length = s->envelopes[idx].length;
			ci->envelope_list_position = 1;
			ci->envelope_speed = ci->envelope_list[0];
		}
		return 0;
	}
	if(s->enable_arpeggio && (track_command >= s->new_arpeggio_cmd)) {
		uint32_t idx = (uint32_t)track_command - (uint32_t)s->new_arpeggio_cmd;
		if(idx < s->arpeggios_count) {
			ci->arpeggio_list = s->arpeggios[idx].data;
			ci->arpeggio_list_length = s->arpeggios[idx].length;
			ci->arpeggio_list_position = 0;
		}
		return 0;
	}

	return dw_do_effects(s, ci, channel_idx, (uint8_t)(track_command & 0x7f));
}

// [=]===^=[ dw_read_track_commands ]=============================================================[=]
static void dw_read_track_commands(struct davidwhittaker_state *s, struct dw_channel_info *ci, int32_t channel_idx) {
	ci->slide_enabled = 0;

	for(;;) {
		if(ci->track_data_position >= (int32_t)ci->track_data_length) {
			break;
		}
		uint8_t track_byte = ci->track_data[ci->track_data_position++];

		if((track_byte & 0x80) != 0) {
			if(dw_do_track_command(s, ci, channel_idx, track_byte)) {
				break;
			}
			continue;
		}

		// Note byte.
		ci->note = track_byte;

		if(s->old_player) {
			int32_t sample_number = (int32_t)track_byte / 12;
			int32_t note = (int32_t)track_byte % 12;

			if((sample_number < 0) || ((uint32_t)sample_number >= s->samples_count)) {
				break;
			}
			struct dw_sample *sm = &s->samples[sample_number];

			if(ci->note != 0) {
				if((note >= 0) && ((uint32_t)note < s->periods_count)) {
					paula_set_period(&s->paula, channel_idx, s->periods[note]);
				}
				paula_play_sample(&s->paula, channel_idx, sm->sample_data, sm->length);
				paula_set_volume(&s->paula, channel_idx, s->channel_volumes[ci->channel_number]);
			} else {
				paula_mute(&s->paula, channel_idx);
			}
		} else {
			if(s->enable_sample_transpose && ci->current_sample_info) {
				track_byte = (uint8_t)(track_byte + ci->current_sample_info->transpose);
			}

			if(s->enable_channel_transpose) {
				track_byte = (uint8_t)(track_byte + ci->transpose);
			} else {
				ci->note = track_byte;
			}

			if(!ci->current_sample_info) {
				break;
			}

			struct dw_sample *sm = ci->current_sample_info;

			paula_play_sample(&s->paula, channel_idx, sm->sample_data, sm->length);

			if(sm->loop_start >= 0) {
				paula_set_loop(&s->paula, channel_idx, (uint32_t)sm->loop_start, sm->length - (uint32_t)sm->loop_start);
			}

			int32_t new_volume = (int32_t)sm->volume;

			if(s->enable_envelopes && ci->envelope_list && (ci->envelope_list_length > 1)) {
				new_volume = ci->envelope_list[1] & 0x7f;
				ci->envelope_list_position = (ci->envelope_list_length > 2) ? 2 : 1;
				ci->envelope_counter = (int8_t)ci->envelope_speed;
			}

			if(s->enable_half_volume && ci->enable_half_volume) {
				new_volume /= 2;
			}

			if(s->enable_volume_fade) {
				new_volume -= (int32_t)s->playing_info.volume_fade_speed;
				if(new_volume < 0) {
					new_volume = 0;
				}
			}

			new_volume = new_volume * (int32_t)s->playing_info.global_volume / 64;
			paula_set_volume(&s->paula, channel_idx, (uint16_t)new_volume);

			if(track_byte >= 128) {
				track_byte = 0;
			} else if((uint32_t)track_byte >= s->periods_count) {
				track_byte = (uint8_t)(s->periods_count - 1);
			}

			uint32_t period = (uint32_t)(((uint32_t)s->periods[track_byte] * (uint32_t)sm->fine_tune_period) >> 10);
			if(period > 0xffff) {
				period = 0xffff;
			}
			paula_set_period(&s->paula, channel_idx, (uint16_t)period);
		}
		break;
	}

	ci->speed_counter = ci->speed;
}

// [=]===^=[ dw_do_frame_stuff ]==================================================================[=]
static void dw_do_frame_stuff(struct davidwhittaker_state *s, struct dw_channel_info *ci, int32_t channel_idx) {
	if(!ci->current_sample_info) {
		return;
	}

	int32_t note = (int32_t)(int8_t)ci->note;

	if(s->enable_global_transpose) {
		note += s->playing_info.transpose;
	}
	if(s->enable_channel_transpose) {
		note += ci->transpose;
	}

	if(s->enable_arpeggio && ci->arpeggio_list && (ci->arpeggio_list_length > 0)) {
		uint8_t arp = ci->arpeggio_list[ci->arpeggio_list_position++];
		if((arp & 0x80) != 0) {
			ci->arpeggio_list_position = 0;
			arp &= 0x7f;
		}
		note += (int32_t)(int8_t)arp;
		if(note < 0) {
			note = 0;
		} else if((uint32_t)note >= s->periods_count) {
			note = (int32_t)s->periods_count - 1;
		}
	}

	if((note < 0) || ((uint32_t)note >= s->periods_count)) {
		return;
	}

	uint32_t period = (uint32_t)(((uint32_t)s->periods[note] * (uint32_t)ci->current_sample_info->fine_tune_period) >> 10);

	if(ci->slide_enabled) {
		if(ci->slide_counter == 0) {
			ci->slide_value = (int16_t)(ci->slide_value + ci->slide_speed);
			period = (uint32_t)((int32_t)period - (int32_t)ci->slide_value);
		} else {
			ci->slide_counter--;
		}
	}

	if(s->enable_vibrato) {
		if(ci->vibrato_direction != 0) {
			if(ci->vibrato_direction < 0) {
				ci->vibrato_value = (uint8_t)(ci->vibrato_value + ci->vibrato_speed);
				if(ci->vibrato_value == ci->vibrato_max_value) {
					ci->vibrato_direction = (int8_t)-ci->vibrato_direction;
				}
			} else {
				ci->vibrato_value = (uint8_t)(ci->vibrato_value - ci->vibrato_speed);
				if(ci->vibrato_value == 0) {
					ci->vibrato_direction = (int8_t)-ci->vibrato_direction;
				}
			}

			if(ci->vibrato_value == 0) {
				ci->vibrato_direction ^= 0x01;
			}

			if((ci->vibrato_direction & 0x01) != 0) {
				period += ci->vibrato_value;
			} else {
				period -= ci->vibrato_value;
			}
		}
	}

	if(period > 0xffff) {
		period = 0xffff;
	}
	paula_set_period(&s->paula, channel_idx, (uint16_t)period);

	if(s->enable_envelopes && ci->envelope_list && (ci->envelope_list_length > 0)) {
		ci->envelope_counter--;
		if(ci->envelope_counter < 0) {
			ci->envelope_counter = (int8_t)ci->envelope_speed;

			if(ci->envelope_list_position >= (int32_t)ci->envelope_list_length) {
				ci->envelope_list_position = (int32_t)ci->envelope_list_length - 1;
			}
			int32_t new_volume = ci->envelope_list[ci->envelope_list_position];
			if((new_volume & 0x80) == 0) {
				ci->envelope_list_position++;
			}

			new_volume &= 0x7f;

			if(s->enable_half_volume && ci->enable_half_volume) {
				new_volume /= 2;
			}

			if(s->enable_volume_fade) {
				new_volume -= (int32_t)s->playing_info.volume_fade_speed;
				if(new_volume < 0) {
					new_volume = 0;
				}
			}

			new_volume = new_volume * (int32_t)s->playing_info.global_volume / 64;
			paula_set_volume(&s->paula, channel_idx, (uint16_t)new_volume);
		}
	}
}

// [=]===^=[ dw_play ]============================================================================[=]
static void dw_play(struct davidwhittaker_state *s) {
	if(s->enable_delay_counter) {
		s->playing_info.delay_counter = (uint16_t)(s->playing_info.delay_counter + s->playing_info.delay_counter_speed);
		if(s->playing_info.delay_counter > 255) {
			s->playing_info.delay_counter = (uint16_t)(s->playing_info.delay_counter - 256);
			return;
		}
	}

	if(s->use_extra_counter) {
		s->playing_info.extra_counter--;
		if(s->playing_info.extra_counter == 0) {
			s->playing_info.extra_counter = 6;
			return;
		}
	}

	if(s->enable_global_volume_fade) {
		if(s->playing_info.global_volume_fade_speed != 0) {
			if(s->playing_info.global_volume > 0) {
				s->playing_info.global_volume_fade_counter--;
				if(s->playing_info.global_volume_fade_counter == 0) {
					s->playing_info.global_volume--;
					if(s->playing_info.global_volume > 0) {
						s->playing_info.global_volume_fade_counter = s->playing_info.global_volume_fade_speed;
					}
				}
			}
		}
	}

	dw_change_square_waveform(s);

	for(int32_t i = 0; i < s->number_of_channels; ++i) {
		struct dw_channel_info *ci = &s->channels[i];
		ci->speed_counter--;

		if(ci->speed_counter == 0) {
			dw_read_track_commands(s, ci, i);
		} else if(ci->speed_counter > 1) {
			dw_do_frame_stuff(s, ci, i);
		}
	}
}

// [=]===^=[ dw_cleanup ]=========================================================================[=]
static void dw_cleanup(struct davidwhittaker_state *s) {
	if(!s) {
		return;
	}
	if(s->song_info_list) {
		for(uint32_t i = 0; i < s->song_info_count; ++i) {
			for(int32_t j = 0; j < DW_MAX_CHANNELS; ++j) {
				free(s->song_info_list[i].position_lists[j].track_offsets);
				s->song_info_list[i].position_lists[j].track_offsets = 0;
			}
		}
		free(s->song_info_list);
		s->song_info_list = 0;
	}
	if(s->tracks) {
		for(uint32_t i = 0; i < s->tracks_count; ++i) {
			free(s->tracks[i].data);
		}
		free(s->tracks);
		s->tracks = 0;
	}
	if(s->arpeggios) {
		for(uint32_t i = 0; i < s->arpeggios_count; ++i) {
			free(s->arpeggios[i].data);
		}
		free(s->arpeggios);
		s->arpeggios = 0;
	}
	if(s->envelopes) {
		for(uint32_t i = 0; i < s->envelopes_count; ++i) {
			free(s->envelopes[i].data);
		}
		free(s->envelopes);
		s->envelopes = 0;
	}
	if(s->samples) {
		for(uint32_t i = 0; i < s->samples_count; ++i) {
			free(s->samples[i].sample_data);
		}
		free(s->samples);
		s->samples = 0;
	}
}

// [=]===^=[ davidwhittaker_init ]================================================================[=]
static struct davidwhittaker_state *davidwhittaker_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || (len < 2048) || (sample_rate < 8000)) {
		return 0;
	}

	struct davidwhittaker_state *s = (struct davidwhittaker_state *)calloc(1, sizeof(struct davidwhittaker_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;
	s->empty_track[0] = 0x80;

	if(!dw_test_module(s)) {
		free(s);
		return 0;
	}

	int32_t max_arpeggios = 0;
	int32_t max_envelopes = 0;

	if(!dw_load_sub_song_info_and_tracks(s, &max_arpeggios, &max_envelopes)) {
		goto fail;
	}
	if(!dw_load_arpeggios(s, max_arpeggios)) {
		goto fail;
	}
	if(!dw_load_envelopes(s, max_envelopes)) {
		goto fail;
	}
	if(!dw_load_channel_volumes(s)) {
		goto fail;
	}
	if(!dw_load_sample_info(s)) {
		goto fail;
	}
	if(!dw_load_sample_data(s)) {
		goto fail;
	}

	paula_init(&s->paula, sample_rate, DW_TICK_HZ);
	s->current_song = 0;
	dw_initialize_sound(s, 0);
	return s;

fail:
	dw_cleanup(s);
	free(s);
	return 0;
}

// [=]===^=[ davidwhittaker_free ]================================================================[=]
static void davidwhittaker_free(struct davidwhittaker_state *s) {
	if(!s) {
		return;
	}
	dw_cleanup(s);
	free(s);
}

// [=]===^=[ davidwhittaker_get_audio ]===========================================================[=]
static void davidwhittaker_get_audio(struct davidwhittaker_state *s, int16_t *output, int32_t frames) {
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
			dw_play(s);
		}
	}
}

// [=]===^=[ davidwhittaker_api_init ]============================================================[=]
static void *davidwhittaker_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return davidwhittaker_init(data, len, sample_rate);
}

// [=]===^=[ davidwhittaker_api_free ]============================================================[=]
static void davidwhittaker_api_free(void *state) {
	davidwhittaker_free((struct davidwhittaker_state *)state);
}

// [=]===^=[ davidwhittaker_api_get_audio ]=======================================================[=]
static void davidwhittaker_api_get_audio(void *state, int16_t *output, int32_t frames) {
	davidwhittaker_get_audio((struct davidwhittaker_state *)state, output, frames);
}

static const char *davidwhittaker_extensions[] = { "dw", "dwold", 0 };

static struct player_api davidwhittaker_api = {
	"David Whittaker",
	davidwhittaker_extensions,
	davidwhittaker_api_init,
	davidwhittaker_api_free,
	davidwhittaker_api_get_audio,
	0,
};
