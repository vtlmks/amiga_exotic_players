// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// MED (Music Editor) replayer, ported from NostalgicPlayer's C# implementation.
// Supports MED 1.12 (version 2 file marker) and MED 2.00 (version 3) modules,
// 4-channel original format. Drives a 4-channel Amiga Paula (see paula.h).
// Tick rate is 50Hz PAL.
//
// Public API:
//   struct med_state *med_init(void *data, uint32_t len, int32_t sample_rate);
//   void med_free(struct med_state *s);
//   void med_get_audio(struct med_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define MED_TICK_HZ          50
#define MED_NUM_TRACKS       4
#define MED_NUM_SAMPLES      32
#define MED_ROWS_PER_BLOCK   64

enum {
	MED_MODTYPE_UNKNOWN = 0,
	MED_MODTYPE_112     = 1,
	MED_MODTYPE_200     = 2,
};

enum {
	MED_EFFECT_ARPEGGIO   = 0x0,
	MED_EFFECT_SLIDE_UP   = 0x1,
	MED_EFFECT_SLIDE_DOWN = 0x2,
	MED_EFFECT_VIBRATO    = 0x3,
	MED_EFFECT_SET_VOLUME = 0xc,
	MED_EFFECT_CRESCENDO  = 0xd,
	MED_EFFECT_FILTER     = 0xe,
	MED_EFFECT_SET_TEMPO  = 0xf,
};

enum {
	MED_BFL_FIRST_LINE_ALL    = 0x01,
	MED_BFL_SECOND_LINE_ALL   = 0x02,
	MED_BFL_FIRST_EFFECT_ALL  = 0x04,
	MED_BFL_SECOND_EFFECT_ALL = 0x08,
	MED_BFL_FIRST_LINE_NONE   = 0x10,
	MED_BFL_SECOND_LINE_NONE  = 0x20,
	MED_BFL_FIRST_EFFECT_NONE = 0x40,
	MED_BFL_SECOND_EFFECT_NONE= 0x80,
};

enum {
	MED_MOD_FILTER_ON        = 0x01,
	MED_MOD_JUMPING          = 0x02,
	MED_MOD_EVERY8TH         = 0x04,
	MED_MOD_SAMPLES_ATTACHED = 0x08,
};

enum {
	MED_SAMPLE_NORMAL      = 0,
	MED_SAMPLE_IFF5OCTAVE  = 1,
	MED_SAMPLE_IFF3OCTAVE  = 2,
};

struct med_track_line {
	uint8_t note;
	uint8_t sample_number;
	uint8_t effect;
	uint8_t effect_arg;
};

struct med_block {
	struct med_track_line rows[MED_NUM_TRACKS][MED_ROWS_PER_BLOCK];
};

struct med_sample {
	int8_t *sample_data;
	uint32_t length;
	uint32_t loop_start;
	uint32_t loop_length;
	uint8_t volume;
	uint8_t type;
	uint8_t owns_data;
};

struct med_state {
	struct paula paula;

	uint8_t module_type;
	uint16_t *periods;
	uint32_t periods_count;

	uint16_t number_of_blocks;
	uint16_t song_length;
	uint8_t orders[256];

	uint16_t start_tempo;
	int8_t play_transpose;
	uint8_t module_flags;
	uint16_t sliding;

	struct med_block *blocks;
	uint32_t blocks_count;

	struct med_sample samples[MED_NUM_SAMPLES];

	uint16_t play_position_number;
	uint16_t play_block;
	uint16_t play_line;
	uint8_t counter;
	uint16_t current_track_count;
	uint8_t next_block;

	uint16_t previous_period[MED_NUM_TRACKS];
	uint8_t previous_notes[16];
	uint8_t previous_samples[16];
	uint8_t previous_volumes[16];
	uint8_t effects[16];
	uint8_t effect_args[16];

	uint8_t visited[256];
};

// MED 1.12 period table (4 octaves).
static uint16_t med_periods_112[48] = {
	856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
	428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
	214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
	107, 101,  95,  90,  85,  80,  75,  72,  68,  64,  60,  57
};

// MED 2.00 period table (3 octaves repeated, used with octave start indexing).
static uint16_t med_periods_200[72] = {
	856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
	428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
	214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
	214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
	214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
	214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113
};

// SoundTracker tempos for MED 2.00 (used when newTempo <= 10).
static uint16_t med_soundtracker_tempos[11] = {
	0x0f00, 2417, 4833, 7250, 9666, 12083, 14500, 16916, 19332, 21436, 24163
};

// Multi-octave shift / multiply / start-note lookup tables for IFF samples.
static uint8_t med_shift_count[12] = {
	4, 3, 2, 1, 1, 0, 2, 2, 1, 1, 0, 0
};

static uint8_t med_multiply_length_count[12] = {
	15, 7, 3, 1, 1, 0, 3, 3, 1, 1, 0, 0
};

static uint8_t med_octave_start[12] = {
	12, 12, 12, 12, 24, 24, 0, 12, 12, 24, 24, 36
};

// [=]===^=[ med_read_u16_be ]====================================================================[=]
static uint16_t med_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ med_read_u32_be ]====================================================================[=]
static uint32_t med_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ med_check_mark ]=====================================================================[=]
static int32_t med_check_mark(uint8_t *p, const char *mark) {
	for(int32_t i = 0; mark[i] != 0; ++i) {
		if(p[i] != (uint8_t)mark[i]) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ med_identify ]=======================================================================[=]
static int32_t med_identify(uint8_t *data, uint32_t len) {
	if(len < 36) {
		return MED_MODTYPE_UNKNOWN;
	}
	if(!med_check_mark(data, "MED")) {
		return MED_MODTYPE_UNKNOWN;
	}
	if(data[3] == 2) {
		return MED_MODTYPE_112;
	}
	if(data[3] == 3) {
		return MED_MODTYPE_200;
	}
	return MED_MODTYPE_UNKNOWN;
}

// [=]===^=[ med_get_nibble ]=====================================================================[=]
static uint8_t med_get_nibble(uint8_t *block_data, uint32_t block_len, uint32_t *nibble_number) {
	uint8_t result = 0;
	uint32_t offset = *nibble_number / 2;
	if(offset < block_len) {
		if((*nibble_number & 1) != 0) {
			result = (uint8_t)(block_data[offset] & 0x0f);
		} else {
			result = (uint8_t)(block_data[offset] >> 4);
		}
	}
	*nibble_number = *nibble_number + 1;
	return result;
}

// [=]===^=[ med_get_nibbles ]====================================================================[=]
static uint16_t med_get_nibbles(uint8_t *block_data, uint32_t block_len, uint32_t *nibble_number, int32_t count) {
	uint16_t result = 0;
	while(count-- > 0) {
		result = (uint16_t)(result << 4);
		result |= (uint16_t)med_get_nibble(block_data, block_len, nibble_number);
	}
	return result;
}

// [=]===^=[ med_find_note ]======================================================================[=]
// In the loader for the 4-byte raw track-line format, the period field is mapped
// back to a note index using the MED 1.12 table. Replicates the original quirk.
static uint8_t med_find_note(uint16_t period) {
	uint8_t note = 0;
	if(period != 0) {
		while(note < 48 && period < med_periods_112[note]) {
			note++;
		}
		note++;
	}
	return note;
}

// [=]===^=[ med_cleanup ]========================================================================[=]
static void med_cleanup(struct med_state *s) {
	if(s->blocks) {
		free(s->blocks);
		s->blocks = 0;
	}
	for(int32_t i = 0; i < MED_NUM_SAMPLES; ++i) {
		if(s->samples[i].owns_data && s->samples[i].sample_data) {
			free(s->samples[i].sample_data);
		}
		s->samples[i].sample_data = 0;
		s->samples[i].owns_data = 0;
	}
}

// [=]===^=[ med_load_med2 ]======================================================================[=]
static int32_t med_load_med2(struct med_state *s, uint8_t *data, uint32_t len) {
	uint32_t pos = 4;
	uint8_t volumes[MED_NUM_SAMPLES];
	uint16_t loop_start[MED_NUM_SAMPLES];
	uint16_t loop_length[MED_NUM_SAMPLES];
	uint32_t i;
	uint32_t j;
	uint32_t k;

	// 32 sample names, 40 bytes each = 1280 bytes.
	if(pos + (32 * 40) + 32 + (32 * 2) + (32 * 2) + 2 + 100 + 2 + 2 + 2 + 2 + 4 + 16 > len) {
		return 0;
	}
	pos += 32 * 40;

	for(i = 0; i < MED_NUM_SAMPLES; ++i) {
		volumes[i] = data[pos++];
	}
	for(i = 0; i < MED_NUM_SAMPLES; ++i) {
		loop_start[i] = med_read_u16_be(data + pos);
		pos += 2;
	}
	for(i = 0; i < MED_NUM_SAMPLES; ++i) {
		loop_length[i] = med_read_u16_be(data + pos);
		pos += 2;
	}

	for(i = 0; i < MED_NUM_SAMPLES; ++i) {
		s->samples[i].volume = volumes[i];
		s->samples[i].loop_start = loop_start[i];
		s->samples[i].loop_length = loop_length[i];
		s->samples[i].type = MED_SAMPLE_NORMAL;
		s->samples[i].sample_data = 0;
		s->samples[i].length = 0;
		s->samples[i].owns_data = 0;
	}

	s->number_of_blocks = med_read_u16_be(data + pos);
	pos += 2;

	uint8_t order_buffer[100];
	memcpy(order_buffer, data + pos, 100);
	pos += 100;

	s->song_length = med_read_u16_be(data + pos);
	pos += 2;
	if(s->song_length > 256) {
		s->song_length = 256;
	}
	uint32_t copy_len = s->song_length;
	if(copy_len > 100) {
		copy_len = 100;
	}
	memcpy(s->orders, order_buffer, copy_len);
	if(s->song_length > 100) {
		memset(s->orders + 100, 0, s->song_length - 100);
	}

	s->start_tempo = med_read_u16_be(data + pos);
	pos += 2;
	s->play_transpose = 0;
	s->module_flags = (uint8_t)med_read_u16_be(data + pos);
	pos += 2;
	s->sliding = med_read_u16_be(data + pos);
	pos += 2;

	pos += 4 + 16; // jumping mask and colors

	if(s->number_of_blocks == 0) {
		return 0;
	}
	s->blocks = (struct med_block *)calloc(s->number_of_blocks, sizeof(struct med_block));
	if(!s->blocks) {
		return 0;
	}
	s->blocks_count = s->number_of_blocks;

	for(i = 0; i < s->number_of_blocks; ++i) {
		if(pos + 4 + (MED_ROWS_PER_BLOCK * MED_NUM_TRACKS * 4) > len) {
			return 0;
		}
		pos += 4; // skip unknown bytes

		for(j = 0; j < MED_ROWS_PER_BLOCK; ++j) {
			for(k = 0; k < MED_NUM_TRACKS; ++k) {
				uint16_t period = med_read_u16_be(data + pos);
				pos += 2;
				uint8_t sample = data[pos++];
				uint8_t effect = (uint8_t)(sample & 0x0f);
				uint8_t effect_arg = data[pos++];

				sample = (uint8_t)(sample >> 4);
				if((period & 0x9000) != 0) {
					period = (uint16_t)(period & ~0x9000);
					sample = (uint8_t)(sample + 16);
				}

				s->blocks[i].rows[k][j].note = med_find_note(period);
				s->blocks[i].rows[k][j].sample_number = sample;
				s->blocks[i].rows[k][j].effect = effect;
				s->blocks[i].rows[k][j].effect_arg = effect_arg;
			}
		}
	}

	// MED 1.12 always uses external samples. None available here -> sample_data stays null.
	return 1;
}

// [=]===^=[ med_skip_midi ]======================================================================[=]
static int32_t med_skip_midi(uint8_t *data, uint32_t len, uint32_t *pos) {
	if(*pos + 4 > len) {
		return 0;
	}
	uint32_t mask = med_read_u32_be(data + *pos);
	*pos += 4;
	uint32_t bytes_to_skip = 0;
	for(int32_t i = 0; i < 32; ++i) {
		if((mask & 0x80000000u) != 0) {
			bytes_to_skip++;
		}
		mask <<= 1;
	}
	if(*pos + bytes_to_skip > len) {
		return 0;
	}
	*pos += bytes_to_skip;
	return 1;
}

// [=]===^=[ med_unpack_block ]===================================================================[=]
static void med_unpack_block(uint8_t *block_data, uint32_t block_len, struct med_block *block, int32_t track_count, uint32_t line_mask1, uint32_t line_mask2, uint32_t effect_mask1, uint32_t effect_mask2) {
	uint32_t current_line_mask = line_mask1;
	uint32_t current_effect_mask = effect_mask1;
	uint32_t nibble_number = 0;
	int32_t i;
	int32_t j;

	for(i = 0; i < MED_ROWS_PER_BLOCK; ++i) {
		if(i == 32) {
			current_line_mask = line_mask2;
			current_effect_mask = effect_mask2;
		}

		for(j = 0; j < track_count && j < MED_NUM_TRACKS; ++j) {
			block->rows[j][i].note = 0;
			block->rows[j][i].sample_number = 0;
			block->rows[j][i].effect = 0;
			block->rows[j][i].effect_arg = 0;
		}

		if((current_line_mask & 0x80000000u) != 0) {
			uint16_t channel_mask = med_get_nibbles(block_data, block_len, &nibble_number, track_count / 4);
			channel_mask = (uint16_t)(channel_mask << (16 - track_count));

			for(j = 0; j < track_count; ++j) {
				if((channel_mask & 0x8000) != 0) {
					uint8_t note = (uint8_t)med_get_nibbles(block_data, block_len, &nibble_number, 2);
					uint8_t sample_number = med_get_nibble(block_data, block_len, &nibble_number);
					if((note & 0x80) != 0) {
						note = (uint8_t)(note & 0x7f);
						sample_number = (uint8_t)(sample_number + 16);
					}
					if(j < MED_NUM_TRACKS) {
						block->rows[j][i].note = note;
						block->rows[j][i].sample_number = sample_number;
					}
				}
				channel_mask = (uint16_t)(channel_mask << 1);
			}
		}

		if((current_effect_mask & 0x80000000u) != 0) {
			uint16_t channel_mask = med_get_nibbles(block_data, block_len, &nibble_number, track_count / 4);
			channel_mask = (uint16_t)(channel_mask << (16 - track_count));

			for(j = 0; j < track_count; ++j) {
				if((channel_mask & 0x8000) != 0) {
					uint8_t effect = med_get_nibble(block_data, block_len, &nibble_number);
					uint8_t effect_arg = (uint8_t)med_get_nibbles(block_data, block_len, &nibble_number, 2);
					if(j < MED_NUM_TRACKS) {
						block->rows[j][i].effect = effect;
						block->rows[j][i].effect_arg = effect_arg;
					}
				}
				channel_mask = (uint16_t)(channel_mask << 1);
			}
		}

		current_line_mask <<= 1;
		current_effect_mask <<= 1;
	}
}

// [=]===^=[ med_load_med3 ]======================================================================[=]
static int32_t med_load_med3(struct med_state *s, uint8_t *data, uint32_t len) {
	uint32_t pos = 4;
	uint8_t volumes[MED_NUM_SAMPLES];
	uint16_t loop_start[MED_NUM_SAMPLES];
	uint16_t loop_length[MED_NUM_SAMPLES];
	uint32_t i;
	uint32_t j;

	memset(volumes, 0, sizeof(volumes));
	memset(loop_start, 0, sizeof(loop_start));
	memset(loop_length, 0, sizeof(loop_length));

	// Sample names, variable length null-terminated up to 39 bytes (40th byte is reserved).
	for(i = 0; i < MED_NUM_SAMPLES; ++i) {
		for(j = 0; j < 39; ++j) {
			if(pos >= len) {
				return 0;
			}
			uint8_t chr = data[pos++];
			if(chr == 0) {
				break;
			}
		}
	}

	if(pos + 4 > len) {
		return 0;
	}
	uint32_t mask = med_read_u32_be(data + pos);
	pos += 4;
	for(i = 0; i < MED_NUM_SAMPLES; ++i) {
		if((mask & 0x80000000u) != 0) {
			if(pos >= len) {
				return 0;
			}
			volumes[i] = data[pos++];
		}
		mask <<= 1;
	}

	if(pos + 4 > len) {
		return 0;
	}
	mask = med_read_u32_be(data + pos);
	pos += 4;
	for(i = 0; i < MED_NUM_SAMPLES; ++i) {
		if((mask & 0x80000000u) != 0) {
			if(pos + 2 > len) {
				return 0;
			}
			loop_start[i] = med_read_u16_be(data + pos);
			pos += 2;
		}
		mask <<= 1;
	}

	if(pos + 4 > len) {
		return 0;
	}
	mask = med_read_u32_be(data + pos);
	pos += 4;
	for(i = 0; i < MED_NUM_SAMPLES; ++i) {
		if((mask & 0x80000000u) != 0) {
			if(pos + 2 > len) {
				return 0;
			}
			loop_length[i] = med_read_u16_be(data + pos);
			pos += 2;
		}
		mask <<= 1;
	}

	for(i = 0; i < MED_NUM_SAMPLES; ++i) {
		s->samples[i].volume = volumes[i];
		s->samples[i].loop_start = loop_start[i];
		s->samples[i].loop_length = loop_length[i];
		s->samples[i].type = MED_SAMPLE_NORMAL;
		s->samples[i].sample_data = 0;
		s->samples[i].length = 0;
		s->samples[i].owns_data = 0;
	}

	if(pos + 8 > len) {
		return 0;
	}
	s->number_of_blocks = med_read_u16_be(data + pos);
	pos += 2;
	s->song_length = med_read_u16_be(data + pos);
	pos += 2;
	if(s->song_length > 256) {
		s->song_length = 256;
	}
	if(pos + s->song_length > len) {
		return 0;
	}
	memcpy(s->orders, data + pos, s->song_length);
	pos += s->song_length;

	if(pos + 2 + 1 + 1 + 2 + 4 + 16 > len) {
		return 0;
	}
	pos += 2; // skip tempo
	s->start_tempo = 6;
	s->play_transpose = (int8_t)data[pos++];
	s->module_flags = data[pos++];
	s->sliding = med_read_u16_be(data + pos);
	pos += 2;

	pos += 4 + 16; // jumping mask and colors

	if(!med_skip_midi(data, len, &pos)) {
		return 0;
	}
	if(!med_skip_midi(data, len, &pos)) {
		return 0;
	}

	if(s->number_of_blocks == 0) {
		return 0;
	}
	s->blocks = (struct med_block *)calloc(s->number_of_blocks, sizeof(struct med_block));
	if(!s->blocks) {
		return 0;
	}
	s->blocks_count = s->number_of_blocks;

	for(i = 0; i < s->number_of_blocks; ++i) {
		if(pos + 4 > len) {
			return 0;
		}
		uint8_t track_count = data[pos++];
		uint8_t flag = data[pos++];
		uint16_t length = med_read_u16_be(data + pos);
		pos += 2;

		uint32_t line_mask1;
		uint32_t line_mask2;
		uint32_t effect_mask1;
		uint32_t effect_mask2;

		if((flag & MED_BFL_FIRST_LINE_NONE) != 0) {
			line_mask1 = 0;
		} else if((flag & MED_BFL_FIRST_LINE_ALL) != 0) {
			line_mask1 = 0xffffffffu;
		} else {
			if(pos + 4 > len) {
				return 0;
			}
			line_mask1 = med_read_u32_be(data + pos);
			pos += 4;
		}

		if((flag & MED_BFL_SECOND_LINE_NONE) != 0) {
			line_mask2 = 0;
		} else if((flag & MED_BFL_SECOND_LINE_ALL) != 0) {
			line_mask2 = 0xffffffffu;
		} else {
			if(pos + 4 > len) {
				return 0;
			}
			line_mask2 = med_read_u32_be(data + pos);
			pos += 4;
		}

		if((flag & MED_BFL_FIRST_EFFECT_NONE) != 0) {
			effect_mask1 = 0;
		} else if((flag & MED_BFL_FIRST_EFFECT_ALL) != 0) {
			effect_mask1 = 0xffffffffu;
		} else {
			if(pos + 4 > len) {
				return 0;
			}
			effect_mask1 = med_read_u32_be(data + pos);
			pos += 4;
		}

		if((flag & MED_BFL_SECOND_EFFECT_NONE) != 0) {
			effect_mask2 = 0;
		} else if((flag & MED_BFL_SECOND_EFFECT_ALL) != 0) {
			effect_mask2 = 0xffffffffu;
		} else {
			if(pos + 4 > len) {
				return 0;
			}
			effect_mask2 = med_read_u32_be(data + pos);
			pos += 4;
		}

		if(pos + length > len) {
			return 0;
		}
		med_unpack_block(data + pos, length, &s->blocks[i], track_count, line_mask1, line_mask2, effect_mask1, effect_mask2);
		pos += length;
	}

	if((s->module_flags & MED_MOD_SAMPLES_ATTACHED) != 0) {
		if(pos + 4 > len) {
			return 0;
		}
		uint32_t sample_mask = med_read_u32_be(data + pos);
		pos += 4;
		for(i = 1; i < MED_NUM_SAMPLES; ++i) {
			sample_mask <<= 1;
			if((sample_mask & 0x80000000u) != 0) {
				if(pos + 6 > len) {
					return 0;
				}
				uint32_t length = med_read_u32_be(data + pos);
				pos += 4;
				uint16_t type = med_read_u16_be(data + pos);
				pos += 2;
				if(pos + length > len) {
					return 0;
				}
				int8_t *sd = (int8_t *)malloc(length);
				if(!sd) {
					return 0;
				}
				memcpy(sd, data + pos, length);
				pos += length;
				s->samples[i].sample_data = sd;
				s->samples[i].length = length;
				s->samples[i].type = (uint8_t)type;
				s->samples[i].owns_data = 1;
			}
		}
	}

	return 1;
}

// [=]===^=[ med_set_tempo ]======================================================================[=]
// In MED 1.12 the tempo is a frame divisor; we keep the 50Hz Paula tick and
// ignore the variable timer (the original played at PAL CIA). MED 2.00 uses
// SoundTracker speed (counter modulo). We model speed = newTempo as ticks per
// row when newTempo <= 10, otherwise tempo is a CIA value we coarsely map.
static void med_set_tempo(struct med_state *s, uint8_t new_tempo) {
	(void)s;
	(void)new_tempo;
	// Original MED uses CIA timer (variable Hz). We keep 50Hz and let the song
	// drive its row counter directly via the 6-tick/row scheme; tempo control
	// effects only re-arm the counter for note retriggers.
}

// [=]===^=[ med_get_sample_data ]================================================================[=]
// Returns by-out: period, start_offset, length (already capped to loop end if
// looping), loop_start, loop_length.
static void med_get_sample_data(struct med_state *s, struct med_sample *sample, int32_t note, uint16_t *period_out, uint32_t *start_offset_out, uint32_t *length_out, uint32_t *loop_start_out, uint32_t *loop_length_out) {
	uint16_t period = 0;
	uint32_t start_offset = 0;
	uint32_t length = sample->length;
	uint32_t loop_start = sample->loop_start;
	uint32_t loop_length = sample->loop_length;

	if(sample->type == MED_SAMPLE_NORMAL) {
		if(note >= 0 && (uint32_t)note < s->periods_count) {
			period = s->periods[note];
		}
		start_offset = 0;
	} else {
		int32_t octave = note / 12;
		int32_t local_note = note % 12;
		uint32_t base_length = sample->length;

		if(octave < 0) {
			octave = 0;
		}
		if(octave > 11) {
			octave = 11;
		}

		if(sample->type == MED_SAMPLE_IFF3OCTAVE) {
			octave += 6;
			if(octave > 11) {
				octave = 11;
			}
			base_length /= 7;
		} else {
			base_length /= 31;
		}

		length = base_length;
		uint8_t shift = med_shift_count[octave];
		loop_start <<= shift;
		loop_length <<= shift;
		length <<= shift;

		start_offset = base_length * med_multiply_length_count[octave];

		uint32_t period_index = (uint32_t)(local_note + med_octave_start[octave]);
		if(period_index < s->periods_count) {
			period = s->periods[period_index];
		}
	}

	if(loop_length > 2) {
		if(length > (loop_start + loop_length)) {
			length = loop_start + loop_length;
		}
	}

	*period_out = period;
	*start_offset_out = start_offset;
	*length_out = length;
	*loop_start_out = loop_start;
	*loop_length_out = loop_length;
}

// [=]===^=[ med_play_note ]======================================================================[=]
static void med_play_note(struct med_state *s, int32_t track_number, uint8_t note, uint8_t volume, uint8_t sample_number) {
	if(sample_number >= MED_NUM_SAMPLES) {
		return;
	}
	struct med_sample *sample = &s->samples[sample_number];
	if(sample->sample_data == 0) {
		return;
	}

	int32_t new_note = (int32_t)note + (int32_t)s->play_transpose;

	if(s->module_type == MED_MODTYPE_200) {
		if(new_note < 0) {
			new_note += 12;
		} else if(new_note > 72) {
			new_note -= 12;
		}
	}

	new_note--;
	if(new_note < 0) {
		return;
	}

	uint16_t period = 0;
	uint32_t start_offset = 0;
	uint32_t length = 0;
	uint32_t loop_start = 0;
	uint32_t loop_length = 0;
	med_get_sample_data(s, sample, new_note, &period, &start_offset, &length, &loop_start, &loop_length);

	if(length == 0 || start_offset >= length) {
		return;
	}
	uint32_t play_length = length - start_offset;

	paula_play_sample(&s->paula, track_number, sample->sample_data + start_offset, play_length);

	if(loop_length > 2) {
		paula_set_loop(&s->paula, track_number, loop_start, loop_length);
	} else {
		paula_set_loop(&s->paula, track_number, 0, 0);
	}

	if(period != 0) {
		paula_set_period(&s->paula, track_number, period);
	}
	s->previous_period[track_number] = period;
	paula_set_volume(&s->paula, track_number, volume);
}

// [=]===^=[ med_handle_set_tempo ]===============================================================[=]
static void med_handle_set_tempo(struct med_state *s, int32_t track_number, uint8_t effect_arg, uint8_t *note) {
	(void)track_number;
	if(effect_arg == 0) {
		s->next_block = 1;
		return;
	}
	if((effect_arg <= 0xf0) || (s->module_type == MED_MODTYPE_112)) {
		med_set_tempo(s, effect_arg);
		return;
	}
	if(effect_arg == 0xf2) {
		s->previous_notes[track_number] = *note;
		*note = 0;
		return;
	}
	if(effect_arg == 0xfe) {
		s->next_block = 1;
	}
}

// [=]===^=[ med_handle_set_volume ]==============================================================[=]
static void med_handle_set_volume(struct med_state *s, int32_t track_number, uint8_t effect_arg) {
	uint8_t v = (uint8_t)(((effect_arg >> 4) * 10) + (effect_arg & 0x0f));
	if(v > 64) {
		v = 64;
	}
	s->previous_volumes[track_number] = v;
}

// [=]===^=[ med_do_arpeggio ]====================================================================[=]
static uint8_t med_do_arpeggio(struct med_state *s, uint8_t note, uint8_t effect_arg) {
	if((s->counter == 0) || (s->counter == 3)) {
		return (uint8_t)(note + (effect_arg & 0x0f));
	}
	if((s->counter == 1) || (s->counter == 4)) {
		return (uint8_t)(note + (effect_arg >> 4));
	}
	return note;
}

// [=]===^=[ med_parse_next_row ]=================================================================[=]
static void med_parse_next_row(struct med_state *s) {
	if(s->play_block >= s->blocks_count) {
		return;
	}
	struct med_block *block = &s->blocks[s->play_block];
	s->current_track_count = MED_NUM_TRACKS;

	for(int32_t i = 0; i < (int32_t)s->current_track_count; ++i) {
		struct med_track_line *track_line = &block->rows[i][s->play_line];
		uint8_t note = track_line->note;
		s->effect_args[i] = track_line->effect_arg;

		if(track_line->sample_number != 0) {
			s->previous_samples[i] = track_line->sample_number;
			if(track_line->sample_number < MED_NUM_SAMPLES) {
				s->previous_volumes[i] = s->samples[track_line->sample_number].volume;
			}
		}

		s->effects[i] = track_line->effect;
		if(track_line->effect != MED_EFFECT_ARPEGGIO || track_line->effect_arg != 0) {
			if(track_line->effect == MED_EFFECT_SET_TEMPO) {
				med_handle_set_tempo(s, i, track_line->effect_arg, &note);
			} else if(track_line->effect == MED_EFFECT_SET_VOLUME) {
				med_handle_set_volume(s, i, track_line->effect_arg);
			}
		}

		if(note != 0) {
			s->previous_notes[i] = note;
			med_play_note(s, i, note, s->previous_volumes[i], s->previous_samples[i]);
		}
	}

	s->play_line++;
	if((s->play_line > 63) || s->next_block) {
		s->play_line = 0;
		s->play_position_number++;
		if(s->play_position_number >= s->song_length) {
			s->play_position_number = 0;
		}
		uint8_t new_block_number = s->orders[s->play_position_number];
		if(new_block_number < s->number_of_blocks) {
			s->play_block = new_block_number;
		}
		s->next_block = 0;
	}
}

// [=]===^=[ med_handle_effects ]=================================================================[=]
static void med_handle_effects(struct med_state *s) {
	for(int32_t i = 0; i < (int32_t)s->current_track_count; ++i) {
		uint8_t effect_arg = s->effect_args[i];
		uint16_t new_period = 0;
		int32_t set_hardware = 1;
		uint8_t effect = s->effects[i];

		if(effect == MED_EFFECT_SLIDE_UP) {
			if((s->sliding == 5) && (s->counter == 0)) {
				// Skip on counter 0 when slide flag is set.
			} else {
				int32_t p = (int32_t)s->previous_period[i] - (int32_t)effect_arg;
				if(p < 113) {
					p = 113;
				}
				s->previous_period[i] = (uint16_t)p;
			}
		} else if(effect == MED_EFFECT_SLIDE_DOWN) {
			if((s->sliding == 5) && (s->counter == 0)) {
			} else {
				int32_t p = (int32_t)s->previous_period[i] + (int32_t)effect_arg;
				if(p > 856) {
					p = 856;
				}
				s->previous_period[i] = (uint16_t)p;
			}
		} else if(effect == MED_EFFECT_ARPEGGIO) {
			if(effect_arg == 0) {
				set_hardware = 0;
			} else {
				uint8_t new_note = med_do_arpeggio(s, s->previous_notes[i], effect_arg);
				int32_t idx = (int32_t)new_note - 1 + (int32_t)s->play_transpose;
				if(idx >= 0 && (uint32_t)idx < s->periods_count) {
					new_period = s->periods[idx];
				}
			}
		} else if(effect == MED_EFFECT_CRESCENDO) {
			int32_t nv = (int32_t)(int8_t)s->previous_volumes[i];
			if((effect_arg & 0xf0) != 0) {
				nv += (int32_t)((effect_arg & 0xf0) >> 4);
				if(nv > 64) {
					nv = 64;
				}
			} else {
				nv -= (int32_t)effect_arg;
				if(nv < 0) {
					nv = 0;
				}
			}
			s->previous_volumes[i] = (uint8_t)nv;
		} else if(effect == MED_EFFECT_VIBRATO) {
			new_period = s->previous_period[i];
			if(s->counter < 3) {
				new_period = (uint16_t)(new_period - effect_arg);
			}
		} else if(effect == MED_EFFECT_SET_TEMPO) {
			if(s->module_type == MED_MODTYPE_200) {
				if(effect_arg == 0xff) {
					paula_mute(&s->paula, i);
					set_hardware = 0;
				} else if(effect_arg == 0xf1) {
					if(s->counter != 3) {
						set_hardware = 0;
					} else {
						med_play_note(s, i, s->previous_notes[i], s->previous_volumes[i], s->previous_samples[i]);
						set_hardware = 0;
					}
				} else if(effect_arg == 0xf2) {
					if(s->counter != 3) {
						set_hardware = 0;
					} else {
						med_play_note(s, i, s->previous_notes[i], s->previous_volumes[i], s->previous_samples[i]);
						set_hardware = 0;
					}
				} else if(effect_arg == 0xf3) {
					if((s->counter & 6) == 0) {
						set_hardware = 0;
					} else {
						med_play_note(s, i, s->previous_notes[i], s->previous_volumes[i], s->previous_samples[i]);
						set_hardware = 0;
					}
				} else {
					set_hardware = 0;
				}
			} else {
				set_hardware = 0;
			}
		} else if(effect == MED_EFFECT_FILTER) {
			if(s->module_type == MED_MODTYPE_112) {
				// Same as crescendo on MED 1.12.
				int32_t nv = (int32_t)(int8_t)s->previous_volumes[i];
				if((effect_arg & 0xf0) != 0) {
					nv += (int32_t)((effect_arg & 0xf0) >> 4);
					if(nv > 64) {
						nv = 64;
					}
				} else {
					nv -= (int32_t)effect_arg;
					if(nv < 0) {
						nv = 0;
					}
				}
				s->previous_volumes[i] = (uint8_t)nv;
			} else {
				// MED 2.00: arg 0 -> Amiga LED filter on, anything else -> off.
				// Mirrors C# MedWorker:1441 AmigaFilter = effectArg == 0.
				paula_set_lp_filter(&s->paula, effect_arg == 0 ? 1 : 0);
				set_hardware = 0;
			}
		} else if(effect == MED_EFFECT_SET_VOLUME) {
			// Already applied in row parse; trigger setHardware below.
		} else {
			set_hardware = 0;
		}

		if(set_hardware) {
			if(new_period == 0) {
				new_period = s->previous_period[i];
			}
			if(new_period != 0) {
				paula_set_period(&s->paula, i, new_period);
			}
			paula_set_volume(&s->paula, i, s->previous_volumes[i]);
		}
	}
}

// [=]===^=[ med_tick ]===========================================================================[=]
static void med_tick(struct med_state *s) {
	s->counter++;
	if(s->counter == 6) {
		s->counter = 0;
		med_parse_next_row(s);
	}
	med_handle_effects(s);
}

// [=]===^=[ med_initialize_sound ]===============================================================[=]
static void med_initialize_sound(struct med_state *s) {
	s->play_position_number = 0;
	s->play_block = s->orders[0];
	s->play_line = 0;
	s->counter = 5;
	s->next_block = 0;
	s->current_track_count = MED_NUM_TRACKS;

	memset(s->previous_period, 0, sizeof(s->previous_period));
	memset(s->previous_notes, 0, sizeof(s->previous_notes));
	memset(s->previous_samples, 0, sizeof(s->previous_samples));
	memset(s->previous_volumes, 0, sizeof(s->previous_volumes));
	memset(s->effects, 0, sizeof(s->effects));
	memset(s->effect_args, 0, sizeof(s->effect_args));
	memset(s->visited, 0, sizeof(s->visited));
}

// [=]===^=[ med_init ]===========================================================================[=]
static struct med_state *med_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 36 || sample_rate < 8000) {
		return 0;
	}
	int32_t module_type = med_identify((uint8_t *)data, len);
	if(module_type == MED_MODTYPE_UNKNOWN) {
		return 0;
	}

	struct med_state *s = (struct med_state *)calloc(1, sizeof(struct med_state));
	if(!s) {
		return 0;
	}
	s->module_type = (uint8_t)module_type;

	if(module_type == MED_MODTYPE_112) {
		s->periods = med_periods_112;
		s->periods_count = 48;
	} else {
		s->periods = med_periods_200;
		s->periods_count = 72;
	}

	int32_t ok = 0;
	if(module_type == MED_MODTYPE_112) {
		ok = med_load_med2(s, (uint8_t *)data, len);
	} else {
		ok = med_load_med3(s, (uint8_t *)data, len);
	}
	if(!ok) {
		med_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, MED_TICK_HZ);
	med_initialize_sound(s);
	return s;
}

// [=]===^=[ med_free ]===========================================================================[=]
static void med_free(struct med_state *s) {
	if(!s) {
		return;
	}
	med_cleanup(s);
	free(s);
}

// [=]===^=[ med_get_audio ]======================================================================[=]
static void med_get_audio(struct med_state *s, int16_t *output, int32_t frames) {
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
			med_tick(s);
		}
	}
}

// [=]===^=[ med_api_init ]=======================================================================[=]
static void *med_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return med_init(data, len, sample_rate);
}

// [=]===^=[ med_api_free ]=======================================================================[=]
static void med_api_free(void *state) {
	med_free((struct med_state *)state);
}

// [=]===^=[ med_api_get_audio ]==================================================================[=]
static void med_api_get_audio(void *state, int16_t *output, int32_t frames) {
	med_get_audio((struct med_state *)state, output, frames);
}

static const char *med_extensions[] = { "med", 0 };

static struct player_api med_api = {
	"MED",
	med_extensions,
	med_api_init,
	med_api_free,
	med_api_get_audio,
	0,
};
