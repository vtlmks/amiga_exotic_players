// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Synthesis 4.0 / 4.2 replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct synthesis_state *synthesis_init(void *data, uint32_t len, int32_t sample_rate);
//   void synthesis_free(struct synthesis_state *s);
//   void synthesis_get_audio(struct synthesis_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define SYN_TICK_HZ                  50
#define SYN_EG_TABLE_LEN             128
#define SYN_ADSR_TABLE_LEN           256
#define SYN_ARP_TABLE_LEN            16
#define SYN_NUM_ARP_TABLES           16
#define SYN_WAVEFORM_LEN             256

// Effect (track) opcodes
#define SYN_EFF_NONE                 0x0
#define SYN_EFF_SLIDE                0x1
#define SYN_EFF_RESTART_ADSR         0x2
#define SYN_EFF_RESTART_EGC          0x3
#define SYN_EFF_SET_TRACK_LEN        0x4
#define SYN_EFF_SKIP_STNT            0x5
#define SYN_EFF_SYNC_MARK            0x6
#define SYN_EFF_SET_FILTER           0x7
#define SYN_EFF_SET_SPEED            0x8
#define SYN_EFF_ENABLE_FX            0x9
#define SYN_EFF_CHANGE_FX            0xa
#define SYN_EFF_CHANGE_ARG1          0xb
#define SYN_EFF_CHANGE_ARG2          0xc
#define SYN_EFF_CHANGE_ARG3          0xd
#define SYN_EFF_EGC_OFF              0xe
#define SYN_EFF_SET_VOLUME           0xf

// Synthesis FX opcodes
#define SYN_FX_NONE                  0
#define SYN_FX_ROTATE1               1
#define SYN_FX_ROTATE2               2
#define SYN_FX_ALIEN                 3
#define SYN_FX_NEGATOR               4
#define SYN_FX_POLYNEG               5
#define SYN_FX_SHAKER1               6
#define SYN_FX_SHAKER2               7
#define SYN_FX_AMFLFO                8
#define SYN_FX_LASER                 9
#define SYN_FX_OCTFX1                10
#define SYN_FX_OCTFX2                11
#define SYN_FX_ALISING               12
#define SYN_FX_EGFX1                 13
#define SYN_FX_EGFX2                 14
#define SYN_FX_CHANGER               15
#define SYN_FX_FMDRUM                16

// EGC modes
#define SYN_EGC_OFF                  0
#define SYN_EGC_ONES                 1
#define SYN_EGC_REPEAT               2

// Transpose modes
#define SYN_TR_ENABLED               0
#define SYN_TR_SOUND_DISABLED        1
#define SYN_TR_NOTE_DISABLED         2
#define SYN_TR_ALL_DISABLED          3

struct syn_sample {
	int8_t *data;            // points into module buffer
	uint32_t length;
};

struct syn_instrument {
	uint8_t waveform_number;
	uint8_t synthesis_enabled;
	uint16_t waveform_length;
	uint16_t repeat_length;
	uint8_t volume;
	int8_t portamento_speed;
	uint8_t adsr_enabled;
	uint8_t adsr_table_number;
	uint16_t adsr_table_length;
	uint8_t arpeggio_start;
	uint8_t arpeggio_length;
	uint8_t arpeggio_repeat_length;
	uint8_t effect;
	uint8_t effect_arg1;
	uint8_t effect_arg2;
	uint8_t effect_arg3;
	uint8_t vibrato_delay;
	uint8_t vibrato_speed;
	uint8_t vibrato_level;
	uint8_t egc_offset;
	uint8_t egc_mode;
	uint8_t egc_table_number;
	uint16_t egc_table_length;
};

struct syn_song {
	uint8_t start_speed;
	uint8_t rows_per_track;
	uint16_t first_position;
	uint16_t last_position;
	uint16_t restart_position;
};

struct syn_position {
	uint16_t start_track_row;
	int8_t sound_transpose;
	int8_t note_transpose;
};

struct syn_track_line {
	uint8_t note;
	uint8_t instrument;
	uint8_t arpeggio;
	uint8_t effect;
	uint8_t effect_arg;
};

struct syn_voice {
	uint16_t start_track_row;
	int8_t sound_transpose;
	int8_t note_transpose;

	uint8_t note;
	uint8_t instrument;
	uint8_t arpeggio;
	uint8_t effect;
	uint8_t effect_arg;

	uint8_t use_buffer;
	int8_t synth_sample1[SYN_WAVEFORM_LEN];
	int8_t synth_sample2[SYN_WAVEFORM_LEN];

	uint8_t transposed_note;
	uint8_t previous_transposed_note;

	uint8_t transposed_instrument;

	uint8_t current_volume;
	uint8_t new_volume;

	uint8_t arpeggio_position;

	int8_t slide_speed;
	int16_t slide_increment;

	int8_t portamento_speed;
	int16_t portamento_speed_counter;

	uint8_t vibrato_delay;
	uint8_t vibrato_position;

	uint8_t adsr_enabled;
	uint16_t adsr_position;

	uint8_t egc_disabled;
	uint8_t egc_mode;
	uint16_t egc_position;

	uint8_t synth_effect_disabled;
	uint8_t synth_effect;
	uint8_t synth_effect_arg1;
	uint8_t synth_effect_arg2;
	uint8_t synth_effect_arg3;

	uint8_t synth_position;
	uint8_t slow_motion_counter;
};

struct synthesis_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint32_t start_offset;

	struct syn_sample *samples;
	uint32_t num_samples;

	int8_t (*waveforms)[SYN_WAVEFORM_LEN];
	uint32_t num_waveforms;

	struct syn_instrument *instruments;
	uint32_t num_instruments;

	uint8_t *envelope_generator_tables;
	uint32_t num_eg_tables;

	uint8_t *adsr_tables;
	uint32_t num_adsr_tables;

	uint8_t arpeggio_tables[SYN_NUM_ARP_TABLES * SYN_ARP_TABLE_LEN];

	struct syn_song *sub_songs;
	uint32_t num_sub_songs;

	struct syn_position *positions;       // [num_positions * 4]
	uint32_t num_positions;

	struct syn_track_line *track_lines;
	uint32_t num_track_lines;

	int8_t vibrato_table[256];

	struct syn_song *current_song;

	// Global playing info
	uint8_t sync_mark;
	uint8_t speed_counter;
	uint8_t current_speed;
	uint16_t song_position;
	uint8_t row_position;
	uint8_t rows_per_track;
	uint8_t transpose_enable_status;
	uint8_t amiga_filter;

	struct syn_voice voices[4];
};

// [=]===^=[ syn_periods ]========================================================================[=]
static uint16_t syn_periods[] = {
	    0,
	13696, 12928, 12192, 11520, 10848, 10240,  9664,  9120,  8608,  8128,  7680,  7248,
	 6848,  6464,  6096,  5760,  5424,  5120,  4832,  4560,  4304,  4064,  3840,  3624,
	 3424,  3232,  3048,  2880,  2712,  2560,  2416,  2280,  2152,  2032,  1920,  1812,
	 1712,  1616,  1524,  1440,  1356,  1280,  1208,  1140,  1076,  1016,   960,   906,
	  856,   808,   762,   720,   678,   640,   604,   570,   538,   508,   480,   453,
	  428,   404,   381,   360,   339,   320,   302,   285,   269,   254,   240,   226,
	  214,   202,   190,   180,   170,   160,   151,   143,   135,   127,   120,   113,
	  107,   101,    95,    90,    85,    80,    75,    71,    67,    63,    60,    56,
	   53,    50,    47,    45,    42,    40,    37,    35,    33,    31,    30,    28
};

// [=]===^=[ syn_read_u16_be ]====================================================================[=]
static uint16_t syn_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ syn_read_u32_be ]====================================================================[=]
static uint32_t syn_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ syn_check_mark ]=====================================================================[=]
static int32_t syn_check_mark(uint8_t *data, uint32_t pos, uint32_t len, const char *mark) {
	uint32_t mlen = (uint32_t)strlen(mark);
	if(pos + mlen > len) {
		return 0;
	}
	for(uint32_t i = 0; i < mlen; ++i) {
		if((char)data[pos + i] != mark[i]) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ syn_identify ]=======================================================================[=]
static int32_t syn_identify(uint8_t *data, uint32_t len, uint32_t *out_start) {
	if(len < 204) {
		return 0;
	}
	if(syn_check_mark(data, 0, len, "Synth4.0")) {
		*out_start = 0;
		return 1;
	}
	if(len < 0x1f0e + 204) {
		return 0;
	}
	if(syn_check_mark(data, 0x1f0e, len, "Synth4.2")) {
		*out_start = 0x1f0e;
		return 1;
	}
	return 0;
}

// [=]===^=[ syn_build_vibrato_table ]============================================================[=]
static void syn_build_vibrato_table(struct synthesis_state *s) {
	int8_t vib_val = 0;
	int32_t offset = 0;

	for(int32_t i = 0; i < 64; ++i) {
		s->vibrato_table[offset++] = vib_val;
		vib_val += 2;
	}
	vib_val++;
	for(int32_t i = 0; i < 128; ++i) {
		vib_val -= 2;
		s->vibrato_table[offset++] = vib_val;
	}
	vib_val--;
	for(int32_t i = 0; i < 64; ++i) {
		s->vibrato_table[offset++] = vib_val;
		vib_val += 2;
	}
}

// [=]===^=[ syn_cleanup ]========================================================================[=]
static void syn_cleanup(struct synthesis_state *s) {
	if(!s) {
		return;
	}
	free(s->samples); s->samples = 0;
	free(s->waveforms); s->waveforms = 0;
	free(s->instruments); s->instruments = 0;
	free(s->envelope_generator_tables); s->envelope_generator_tables = 0;
	free(s->adsr_tables); s->adsr_tables = 0;
	free(s->sub_songs); s->sub_songs = 0;
	free(s->positions); s->positions = 0;
	free(s->track_lines); s->track_lines = 0;
}

// [=]===^=[ syn_load ]===========================================================================[=]
static int32_t syn_load(struct synthesis_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = s->start_offset + 8;

	if(pos + 12 > len) {
		return 0;
	}

	uint16_t total_positions = syn_read_u16_be(data + pos); pos += 2;
	uint16_t total_track_rows = syn_read_u16_be(data + pos); pos += 2;
	pos += 4;
	uint8_t num_samples = data[pos++];
	uint8_t num_waveforms = data[pos++];
	uint8_t num_instruments = data[pos++];
	uint8_t num_sub_songs = data[pos++];
	uint8_t num_eg_tables = data[pos++];
	uint8_t num_adsr_tables = data[pos++];
	pos++;	// noise length unused

	// Skip 13 bytes + 28 module name + 140 description
	pos += 13 + 28 + 140;

	if(pos > len) {
		return 0;
	}

	s->num_samples = num_samples;
	s->num_waveforms = num_waveforms;
	s->num_instruments = num_instruments;
	s->num_sub_songs = num_sub_songs;
	s->num_eg_tables = num_eg_tables;
	s->num_adsr_tables = num_adsr_tables;
	s->num_positions = total_positions;
	s->num_track_lines = (uint32_t)total_track_rows + 64;

	// Sample headers: 1 + 27 = 28 bytes name block per sample
	if(pos + (uint32_t)num_samples * 28 > len) {
		return 0;
	}
	s->samples = (struct syn_sample *)calloc(num_samples ? num_samples : 1, sizeof(struct syn_sample));
	if(!s->samples) {
		return 0;
	}
	pos += (uint32_t)num_samples * 28;

	// Sample lengths: u32 BE per sample
	if(pos + (uint32_t)num_samples * 4 > len) {
		return 0;
	}
	for(uint32_t i = 0; i < num_samples; ++i) {
		s->samples[i].length = syn_read_u32_be(data + pos);
		pos += 4;
	}

	// Envelope generator tables
	uint32_t eg_size = (uint32_t)num_eg_tables * SYN_EG_TABLE_LEN;
	if(pos + eg_size > len) {
		return 0;
	}
	s->envelope_generator_tables = (uint8_t *)malloc(eg_size ? eg_size : 1);
	if(!s->envelope_generator_tables) {
		return 0;
	}
	if(eg_size > 0) {
		memcpy(s->envelope_generator_tables, data + pos, eg_size);
		pos += eg_size;
	}

	// ADSR tables
	uint32_t adsr_size = (uint32_t)num_adsr_tables * SYN_ADSR_TABLE_LEN;
	if(pos + adsr_size > len) {
		return 0;
	}
	s->adsr_tables = (uint8_t *)malloc(adsr_size ? adsr_size : 1);
	if(!s->adsr_tables) {
		return 0;
	}
	if(adsr_size > 0) {
		memcpy(s->adsr_tables, data + pos, adsr_size);
		pos += adsr_size;
	}

	// Instruments: 24 bytes each
	if(pos + (uint32_t)num_instruments * 24 > len) {
		return 0;
	}
	s->instruments = (struct syn_instrument *)calloc(num_instruments ? num_instruments : 1, sizeof(struct syn_instrument));
	if(!s->instruments) {
		return 0;
	}
	for(uint32_t i = 0; i < num_instruments; ++i) {
		struct syn_instrument *ins = &s->instruments[i];
		ins->waveform_number          = data[pos++];
		ins->synthesis_enabled        = (data[pos++] != 0);
		ins->waveform_length          = syn_read_u16_be(data + pos); pos += 2;
		ins->repeat_length            = syn_read_u16_be(data + pos); pos += 2;
		ins->volume                   = data[pos++];
		ins->portamento_speed         = (int8_t)data[pos++];
		ins->adsr_enabled             = (data[pos++] != 0);
		ins->adsr_table_number        = data[pos++];
		ins->adsr_table_length        = syn_read_u16_be(data + pos); pos += 2;
		pos++;
		ins->arpeggio_start           = data[pos++];
		ins->arpeggio_length          = data[pos++];
		ins->arpeggio_repeat_length   = data[pos++];
		ins->effect                   = data[pos++];
		ins->effect_arg1              = data[pos++];
		ins->effect_arg2              = data[pos++];
		ins->effect_arg3              = data[pos++];
		ins->vibrato_delay            = data[pos++];
		ins->vibrato_speed            = data[pos++];
		ins->vibrato_level            = data[pos++];
		ins->egc_offset               = data[pos++];
		ins->egc_mode                 = data[pos++];
		ins->egc_table_number         = data[pos++];
		ins->egc_table_length         = syn_read_u16_be(data + pos); pos += 2;
	}

	// Arpeggio tables: 16 * 16 bytes
	if(pos + sizeof(s->arpeggio_tables) > len) {
		return 0;
	}
	memcpy(s->arpeggio_tables, data + pos, sizeof(s->arpeggio_tables));
	pos += (uint32_t)sizeof(s->arpeggio_tables);

	// Sub-songs: 4 skip + 9 read + 2 skip = 15 bytes
	if(pos + (uint32_t)num_sub_songs * 15 > len) {
		return 0;
	}
	s->sub_songs = (struct syn_song *)calloc(num_sub_songs ? num_sub_songs : 1, sizeof(struct syn_song));
	if(!s->sub_songs) {
		return 0;
	}
	for(uint32_t i = 0; i < num_sub_songs; ++i) {
		pos += 4;
		struct syn_song *song = &s->sub_songs[i];
		song->start_speed       = data[pos++];
		song->rows_per_track    = data[pos++];
		song->first_position    = syn_read_u16_be(data + pos); pos += 2;
		song->last_position     = syn_read_u16_be(data + pos); pos += 2;
		song->restart_position  = syn_read_u16_be(data + pos); pos += 2;
		pos += 2;
	}
	// Skip extra sub-song info
	if(pos + 14 > len) {
		return 0;
	}
	pos += 14;

	// Waveforms: 256 bytes each (signed)
	if(pos + (uint32_t)num_waveforms * SYN_WAVEFORM_LEN > len) {
		return 0;
	}
	s->waveforms = (int8_t (*)[SYN_WAVEFORM_LEN])calloc(num_waveforms ? num_waveforms : 1, SYN_WAVEFORM_LEN);
	if(!s->waveforms) {
		return 0;
	}
	for(uint32_t i = 0; i < num_waveforms; ++i) {
		memcpy(s->waveforms[i], data + pos, SYN_WAVEFORM_LEN);
		pos += SYN_WAVEFORM_LEN;
	}

	// Positions: 4 entries per row, each 4 bytes
	if(pos + (uint32_t)total_positions * 4 * 4 > len) {
		return 0;
	}
	s->positions = (struct syn_position *)calloc(total_positions ? (size_t)total_positions * 4 : 1, sizeof(struct syn_position));
	if(!s->positions) {
		return 0;
	}
	for(uint32_t i = 0; i < total_positions; ++i) {
		for(uint32_t j = 0; j < 4; ++j) {
			struct syn_position *p = &s->positions[i * 4 + j];
			p->start_track_row  = syn_read_u16_be(data + pos); pos += 2;
			p->sound_transpose  = (int8_t)data[pos++];
			p->note_transpose   = (int8_t)data[pos++];
		}
	}

	// Track rows: 4 bytes each, with 64 extra empty rows at end
	uint32_t file_track_rows = total_track_rows + 64;
	s->track_lines = (struct syn_track_line *)calloc(file_track_rows ? file_track_rows : 1, sizeof(struct syn_track_line));
	if(!s->track_lines) {
		return 0;
	}
	if(pos + file_track_rows * 4 > len) {
		return 0;
	}
	for(uint32_t i = 0; i < file_track_rows; ++i) {
		uint8_t b1 = data[pos++];
		uint8_t b2 = data[pos++];
		uint8_t b3 = data[pos++];
		uint8_t b4 = data[pos++];
		s->track_lines[i].note       = b1;
		s->track_lines[i].instrument = b2;
		s->track_lines[i].arpeggio   = (uint8_t)((b3 & 0xf0) >> 4);
		s->track_lines[i].effect     = (uint8_t)(b3 & 0x0f);
		s->track_lines[i].effect_arg = b4;
	}

	// Sample data
	for(uint32_t i = 0; i < num_samples; ++i) {
		uint32_t slen = s->samples[i].length;
		if(pos + slen > len) {
			return 0;
		}
		s->samples[i].data = (int8_t *)(data + pos);
		pos += slen;
	}

	return 1;
}

// [=]===^=[ syn_initialize_sound ]===============================================================[=]
static void syn_initialize_sound(struct synthesis_state *s, uint32_t sub_song) {
	if(sub_song >= s->num_sub_songs) {
		sub_song = 0;
	}
	s->current_song = &s->sub_songs[sub_song];

	s->sync_mark = 0;
	s->speed_counter = s->current_song->start_speed;
	s->current_speed = s->current_song->start_speed;
	s->song_position = s->current_song->first_position;
	s->row_position = s->current_song->rows_per_track;
	s->rows_per_track = s->current_song->rows_per_track;
	s->transpose_enable_status = SYN_TR_ENABLED;

	memset(s->voices, 0, sizeof(s->voices));
}

// [=]===^=[ syn_get_next_row ]===================================================================[=]
static void syn_get_next_row(struct synthesis_state *s) {
	s->speed_counter = 0;

	if(s->row_position >= s->rows_per_track) {
		s->row_position = 0;

		if(s->song_position > s->current_song->last_position) {
			s->song_position = s->current_song->restart_position;
		}
		s->song_position++;

		uint32_t row_idx = (uint32_t)(s->song_position - 1);
		if(row_idx >= s->num_positions) {
			row_idx = 0;
		}
		struct syn_position *position_row = &s->positions[row_idx * 4];

		for(int32_t i = 0; i < 4; ++i) {
			s->voices[i].start_track_row = position_row[i].start_track_row;
			s->voices[i].sound_transpose = position_row[i].sound_transpose;
			s->voices[i].note_transpose  = position_row[i].note_transpose;
		}
	}

	for(int32_t i = 0; i < 4; ++i) {
		struct syn_voice *v = &s->voices[i];
		uint32_t position = (uint32_t)v->start_track_row + s->row_position;
		struct syn_track_line tl = { 0, 0, 0, 0, 0 };
		if(position < s->num_track_lines) {
			tl = s->track_lines[position];
		}
		v->note       = tl.note;
		v->instrument = tl.instrument;
		v->arpeggio   = tl.arpeggio;
		v->effect     = tl.effect;
		v->effect_arg = tl.effect_arg;
	}
}

// [=]===^=[ syn_play_row ]=======================================================================[=]
static void syn_play_row(struct synthesis_state *s, struct syn_voice *v, int32_t ch) {
	v->current_volume = 0;
	v->slide_speed = 0;

	if(v->effect != SYN_EFF_NONE) {
		v->synth_effect_disabled = 0;
		v->synth_effect = SYN_FX_NONE;
		v->synth_effect_arg1 = 0;
		v->synth_effect_arg2 = 0;
		v->synth_effect_arg3 = 0;

		v->egc_disabled = 0;
		v->new_volume = 0;

		switch(v->effect) {
			case SYN_EFF_SLIDE: {
				v->slide_speed = (int8_t)v->effect_arg;
				break;
			}

			case SYN_EFF_RESTART_ADSR: {
				v->adsr_position = v->effect_arg;
				v->adsr_enabled = 1;
				break;
			}

			case SYN_EFF_RESTART_EGC: {
				v->egc_position = v->effect_arg;
				v->egc_mode = SYN_EGC_ONES;
				break;
			}

			case SYN_EFF_SET_TRACK_LEN: {
				if(v->effect_arg <= 64) {
					s->rows_per_track = v->effect_arg;
				}
				break;
			}

			case SYN_EFF_SKIP_STNT: {
				s->transpose_enable_status = v->effect_arg;
				break;
			}

			case SYN_EFF_SYNC_MARK: {
				s->sync_mark = v->effect_arg;
				break;
			}

			case SYN_EFF_SET_FILTER: {
				s->amiga_filter = (v->effect_arg == 0);
				break;
			}

			case SYN_EFF_SET_SPEED: {
				if((v->effect_arg > 0) && (v->effect_arg <= 16)) {
					s->current_speed = v->effect_arg;
				}
				break;
			}

			case SYN_EFF_ENABLE_FX: {
				v->synth_effect_disabled = (v->effect_arg != 0);
				break;
			}

			case SYN_EFF_CHANGE_FX: {
				v->synth_effect = v->effect_arg;
				break;
			}

			case SYN_EFF_CHANGE_ARG1: {
				v->synth_effect_arg1 = v->effect_arg;
				break;
			}

			case SYN_EFF_CHANGE_ARG2: {
				v->synth_effect_arg2 = v->effect_arg;
				break;
			}

			case SYN_EFF_CHANGE_ARG3: {
				v->synth_effect_arg3 = v->effect_arg;
				break;
			}

			case SYN_EFF_EGC_OFF: {
				v->egc_disabled = (v->effect_arg != 0);
				break;
			}

			case SYN_EFF_SET_VOLUME: {
				v->new_volume = v->effect_arg;
				break;
			}
		}
	}

	uint8_t note = v->note;
	if(note == 0) {
		return;
	}

	if(note == 0x7f) {
		paula_mute(&s->paula, ch);
		v->current_volume = 0;
		return;
	}

	if((s->transpose_enable_status != SYN_TR_NOTE_DISABLED) && (s->transpose_enable_status != SYN_TR_ALL_DISABLED)) {
		note = (uint8_t)(note + v->note_transpose);
	}

	v->previous_transposed_note = v->transposed_note;
	v->transposed_note = note;

	paula_set_period(&s->paula, ch, syn_periods[note]);

	uint8_t instr_num = v->instrument;
	if(instr_num == 0) {
		return;
	}

	if((s->transpose_enable_status != SYN_TR_SOUND_DISABLED) && (s->transpose_enable_status != SYN_TR_ALL_DISABLED)) {
		instr_num = (uint8_t)(instr_num + v->sound_transpose);
	}
	v->transposed_instrument = instr_num;

	if(instr_num == 0 || instr_num > s->num_instruments) {
		return;
	}

	struct syn_instrument *ins = &s->instruments[instr_num - 1];

	v->adsr_enabled = 0;
	v->adsr_position = 0;
	v->vibrato_delay = 0;
	v->vibrato_position = 0;
	v->egc_mode = SYN_EGC_OFF;
	v->egc_position = 0;
	v->slide_increment = 0;
	v->arpeggio_position = 0;
	v->portamento_speed = ins->portamento_speed;
	v->portamento_speed_counter = ins->portamento_speed;

	if(v->effect == SYN_EFF_CHANGE_ARG1) {
		v->portamento_speed = 0;
		v->portamento_speed_counter = 0;
	}

	v->vibrato_delay = ins->vibrato_delay;

	if(ins->adsr_enabled) {
		v->adsr_enabled = 1;
	}

	if(ins->synthesis_enabled) {
		if(ins->waveform_number >= s->num_waveforms) {
			return;
		}
		int8_t *waveform = s->waveforms[ins->waveform_number];

		if(ins->effect != SYN_FX_NONE) {
			v->slow_motion_counter = 0;
			v->synth_position = 0;
		}

		v->use_buffer = 1;
		v->egc_mode = ins->egc_mode;

		if(ins->egc_mode == SYN_EGC_OFF) {
			v->slow_motion_counter = 0;

			uint16_t length = ins->waveform_length;
			if(length > SYN_WAVEFORM_LEN) {
				length = SYN_WAVEFORM_LEN;
			}

			memcpy(v->synth_sample1, waveform, length);
			if(ins->egc_offset != 0) {
				uint32_t cnt = ins->egc_offset;
				if(cnt > length) {
					cnt = length;
				}
				for(uint32_t i = 0; i < cnt; ++i) {
					v->synth_sample1[i] = (int8_t)-v->synth_sample1[i];
				}
			}

			paula_play_sample(&s->paula, ch, v->synth_sample1, length);
			paula_set_loop(&s->paula, ch, 0, length);
		}

		v->current_volume = (v->new_volume != 0) ? v->new_volume : ins->volume;
		paula_set_volume(&s->paula, ch, v->current_volume);
	} else {
		if(ins->waveform_length != 0) {
			v->slow_motion_counter = 0;
			v->synth_position = 0;

			uint8_t sample_num = (uint8_t)(ins->waveform_number & 0x3f);
			if(sample_num >= s->num_samples) {
				paula_mute(&s->paula, ch);
				return;
			}
			struct syn_sample *samp = &s->samples[sample_num];

			uint32_t play_length = ins->waveform_length;
			uint32_t loop_start = 0;
			uint32_t loop_length = 0;

			if(ins->repeat_length == 0) {
				loop_length = ins->waveform_length;
			} else if(ins->repeat_length != 2) {
				play_length += ins->repeat_length;
				loop_start = ins->waveform_length;
				loop_length = ins->repeat_length;
			}

			if(play_length > samp->length) {
				play_length = samp->length;
			}

			paula_play_sample(&s->paula, ch, samp->data, play_length);
			if(loop_length != 0) {
				paula_set_loop(&s->paula, ch, loop_start, loop_length);
			}

			uint8_t volume = (v->new_volume != 0) ? v->new_volume : ins->volume;
			paula_set_volume(&s->paula, ch, volume);

			if(sample_num == 7) {
				v->current_volume = (uint8_t)(v->effect_arg & 0x3f);
			} else {
				v->current_volume = volume;
			}
		}
	}
}

// [=]===^=[ syn_do_arpeggio ]====================================================================[=]
static int32_t syn_do_arpeggio(struct synthesis_state *s, struct syn_instrument *ins, struct syn_voice *v, int32_t ch, uint16_t *period_out, uint16_t *previous_period_out) {
	uint16_t period;
	uint16_t previous_period;

	uint8_t arp_num = v->arpeggio;
	if(arp_num == 0) {
		uint8_t note = v->transposed_note;
		if(note == 0) {
			paula_mute(&s->paula, ch);
			return 0;
		}
		uint8_t prev_note = v->previous_transposed_note;

		uint8_t arp_len = (uint8_t)(ins->arpeggio_length + ins->arpeggio_repeat_length);
		if(arp_len != 0) {
			uint32_t idx = (uint32_t)ins->arpeggio_start + v->arpeggio_position;
			uint8_t arp_val = 0;
			if(idx < sizeof(s->arpeggio_tables)) {
				arp_val = s->arpeggio_tables[idx];
			}

			if(v->arpeggio_position == arp_len) {
				v->arpeggio_position = ins->arpeggio_length;
			} else {
				v->arpeggio_position++;
			}
			note      = (uint8_t)(note + arp_val);
			prev_note = (uint8_t)(prev_note + arp_val);
		}
		period = syn_periods[note];
		previous_period = syn_periods[prev_note];
	} else {
		uint8_t note = v->transposed_note;
		uint8_t prev_note = v->previous_transposed_note;
		uint32_t idx = (uint32_t)arp_num * 16 + s->speed_counter;
		uint8_t arp_val = 0;
		if(idx < sizeof(s->arpeggio_tables)) {
			arp_val = s->arpeggio_tables[idx];
		}
		note      = (uint8_t)(note + arp_val);
		prev_note = (uint8_t)(prev_note + arp_val);

		period = syn_periods[note];
		previous_period = syn_periods[prev_note];
	}

	*period_out = period;
	*previous_period_out = previous_period;
	return 1;
}

// [=]===^=[ syn_do_portamento ]==================================================================[=]
static void syn_do_portamento(uint16_t *period, uint16_t *previous_period, struct syn_voice *v) {
	if((v->portamento_speed_counter != 0) && (*period != *previous_period)) {
		v->portamento_speed_counter--;

		uint16_t tmp = *period;
		*period = *previous_period;
		*previous_period = tmp;

		int32_t new_period = ((int32_t)*period - (int32_t)*previous_period) * v->portamento_speed_counter;
		if(v->portamento_speed != 0) {
			new_period /= v->portamento_speed;
		}
		new_period += *previous_period;
		*period = (uint16_t)new_period;
	}
}

// [=]===^=[ syn_do_vibrato ]=====================================================================[=]
static void syn_do_vibrato(struct synthesis_state *s, uint16_t *period, struct syn_instrument *ins, struct syn_voice *v) {
	if(ins->vibrato_level != 0) {
		if(v->vibrato_delay == 0) {
			int8_t vib_val = s->vibrato_table[v->vibrato_position];
			uint8_t vib_level = ins->vibrato_level;

			if(vib_val < 0) {
				if(vib_level != 0) {
					*period = (uint16_t)(*period - ((-vib_val * 4) / vib_level));
				} else {
					*period = 124;
				}
			} else {
				if(vib_level != 0) {
					*period = (uint16_t)(*period + ((vib_val * 4) / vib_level));
				} else {
					*period = 124;
				}
			}
			v->vibrato_position = (uint8_t)(v->vibrato_position + ins->vibrato_speed);
		} else {
			v->vibrato_delay--;
		}
	}
}

// [=]===^=[ syn_do_adsr ]========================================================================[=]
static int32_t syn_do_adsr(struct synthesis_state *s, struct syn_instrument *ins, struct syn_voice *v, int32_t ch) {
	if(v->adsr_enabled) {
		if(v->adsr_position >= ins->adsr_table_length) {
			v->transposed_instrument = 0;
			paula_mute(&s->paula, ch);
			return 0;
		}

		uint32_t idx = (uint32_t)ins->adsr_table_number * SYN_ADSR_TABLE_LEN + v->adsr_position;
		uint8_t raw = 0;
		if(idx < (uint32_t)s->num_adsr_tables * SYN_ADSR_TABLE_LEN) {
			raw = s->adsr_tables[idx];
		}
		uint16_t adsr_val = (uint16_t)(raw + 1);

		uint16_t volume = (v->new_volume != 0) ? v->new_volume : ins->volume;
		volume = (uint16_t)(((uint32_t)volume * adsr_val) / 128);
		if(volume > 64) {
			volume = 64;
		}
		paula_set_volume(&s->paula, ch, volume);
		v->adsr_position++;
	}
	return 1;
}

// [=]===^=[ syn_do_egc ]=========================================================================[=]
static void syn_do_egc(struct synthesis_state *s, struct syn_instrument *ins, struct syn_voice *v, int32_t ch) {
	if((v->egc_mode == SYN_EGC_OFF) || v->egc_disabled) {
		return;
	}
	if(ins->waveform_number >= s->num_waveforms) {
		return;
	}

	int8_t *waveform = s->waveforms[ins->waveform_number];
	int8_t *synth_buf;

	v->use_buffer ^= 1;
	if(v->use_buffer == 0) {
		synth_buf = v->synth_sample2;
	} else {
		synth_buf = v->synth_sample1;
	}

	uint32_t wlen = ins->waveform_length;
	if(wlen > SYN_WAVEFORM_LEN) {
		wlen = SYN_WAVEFORM_LEN;
	}

	uint32_t copy_len = (wlen / 16) * 16;
	memcpy(synth_buf, waveform, copy_len);

	uint32_t eg_idx = (uint32_t)ins->egc_table_number * SYN_EG_TABLE_LEN + v->egc_position;
	uint8_t egc_raw = 0;
	if(eg_idx < (uint32_t)s->num_eg_tables * SYN_EG_TABLE_LEN) {
		egc_raw = s->envelope_generator_tables[eg_idx];
	}
	uint8_t egc_val = (uint8_t)(egc_raw + ins->egc_offset);

	if(egc_val != 0) {
		uint32_t cnt = egc_val;
		if(cnt > wlen) {
			cnt = wlen;
		}
		for(uint32_t i = 0; i < cnt; ++i) {
			synth_buf[i] = (int8_t)-synth_buf[i];
		}
	}

	paula_queue_sample(&s->paula, ch, synth_buf, 0, wlen);
	paula_set_loop(&s->paula, ch, 0, wlen);

	v->egc_position++;
	if(v->egc_position >= ins->egc_table_length) {
		if(v->egc_mode == SYN_EGC_ONES) {
			v->egc_mode = SYN_EGC_OFF;
		} else {
			v->egc_position = 0;
		}
	}
}

// [=]===^=[ syn_fx_rotate1 ]=====================================================================[=]
static void syn_fx_rotate1(struct syn_voice *v, uint8_t start, uint8_t end, uint8_t speed) {
	if(start <= end) {
		int32_t count = end - start;
		for(int32_t i = start; i <= count; ++i) {
			v->synth_sample1[i] = (int8_t)(v->synth_sample1[i] + (int8_t)speed);
		}
	}
}

// [=]===^=[ syn_fx_rotate2 ]=====================================================================[=]
static void syn_fx_rotate2(struct syn_voice *v, uint8_t start, uint8_t end, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		if(start <= end) {
			int32_t count = end - start;
			int8_t first = v->synth_sample1[start];
			for(int32_t i = start; i < count; ++i) {
				v->synth_sample1[i] = v->synth_sample1[i + 1];
			}
			v->synth_sample1[count] = first;
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_alien ]=======================================================================[=]
static void syn_fx_alien(struct synthesis_state *s, struct syn_voice *v, uint8_t source, uint8_t end, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		if(source >= s->num_waveforms) {
			return;
		}
		int8_t *waveform = s->waveforms[source];
		for(int32_t i = 0; i <= end; ++i) {
			v->synth_sample1[i] = (int8_t)(v->synth_sample1[i] + waveform[i]);
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_negator ]=====================================================================[=]
static void syn_fx_negator(struct syn_voice *v, uint8_t start, uint8_t end, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		if(start <= end) {
			int32_t count = end - start;
			int32_t offset = start + v->synth_position;
			if(offset < SYN_WAVEFORM_LEN) {
				v->synth_sample1[offset] = (int8_t)-v->synth_sample1[offset];
			}
			if(offset == count) {
				v->synth_position = 0;
			} else {
				v->synth_position++;
			}
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_polyneg ]=====================================================================[=]
static void syn_fx_polyneg(struct syn_voice *v, uint8_t start, uint8_t end, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		if(start <= end) {
			int32_t count = end - start;
			if(v->synth_position == 0) {
				int32_t k = start + count;
				if(k < SYN_WAVEFORM_LEN) {
					v->synth_sample1[k] = (int8_t)-v->synth_sample1[k];
				}
			} else {
				int32_t k = start + v->synth_position - 1;
				if(k < SYN_WAVEFORM_LEN) {
					v->synth_sample1[k] = (int8_t)-v->synth_sample1[k];
				}
			}
			int32_t k = start + v->synth_position;
			if(k < SYN_WAVEFORM_LEN) {
				v->synth_sample1[k] = (int8_t)-v->synth_sample1[k];
			}
			if(v->synth_position == count) {
				v->synth_position = 0;
			} else {
				v->synth_position++;
			}
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_shaker1 ]=====================================================================[=]
static void syn_fx_shaker1(struct synthesis_state *s, struct syn_voice *v, uint8_t source, uint8_t mix_in, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		if(source >= s->num_waveforms) {
			return;
		}
		int8_t mix_byte = s->waveforms[source][v->synth_position];
		for(int32_t i = 0; i <= mix_in; ++i) {
			v->synth_sample1[i] = (int8_t)(v->synth_sample1[i] + mix_byte);
		}
		if(v->synth_position == mix_in) {
			v->synth_position = 0;
		} else {
			v->synth_position++;
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_shaker2 ]=====================================================================[=]
static void syn_fx_shaker2(struct synthesis_state *s, struct syn_voice *v, uint8_t source, uint8_t mix_in, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		if(source >= s->num_waveforms) {
			return;
		}
		int8_t mix_byte = s->waveforms[source][v->synth_position];
		for(int32_t i = 0; i <= mix_in; ++i) {
			v->synth_sample1[i] = (int8_t)(v->synth_sample1[i] + mix_byte);
			if(i == v->synth_position) {
				v->synth_sample1[i] = (int8_t)-v->synth_sample1[i];
			}
		}
		if(v->synth_position == mix_in) {
			v->synth_position = 0;
		} else {
			v->synth_position++;
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_amflfo ]======================================================================[=]
static void syn_fx_amflfo(struct synthesis_state *s, struct syn_voice *v, uint8_t source, uint8_t end, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		if(source >= s->num_waveforms) {
			return;
		}
		int8_t *waveform = s->waveforms[source];
		v->slide_increment = (int16_t)-waveform[v->synth_position];
		if(v->synth_position == end) {
			v->synth_position = 0;
		} else {
			v->synth_position++;
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_laser ]=======================================================================[=]
static void syn_fx_laser(struct syn_voice *v, uint8_t laser_speed, uint8_t laser_time, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		v->slide_increment = (int16_t)(v->slide_increment + (int16_t)-(int8_t)laser_speed);
		if(v->synth_position == laser_time) {
			v->synth_position = 0;
			v->slide_increment = 0;
		} else {
			v->synth_position++;
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_octfx1 ]======================================================================[=]
static void syn_fx_octfx1(struct syn_voice *v, uint8_t mix_in, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		int32_t count = mix_in / 2;
		int32_t j = 0;
		for(int32_t i = 0; j <= count; i += 2, j++) {
			if(j < SYN_WAVEFORM_LEN && i < SYN_WAVEFORM_LEN) {
				v->synth_sample1[j] = v->synth_sample1[i];
			}
		}
		for(int32_t i = 0; i <= count; i++, j++) {
			if(j < SYN_WAVEFORM_LEN) {
				v->synth_sample1[j] = v->synth_sample1[i];
			}
		}
	} else {
		if(mix_in != 0) {
			v->slow_motion_counter--;
		}
	}
}

// [=]===^=[ syn_fx_octfx2 ]======================================================================[=]
static void syn_fx_octfx2(struct syn_voice *v, uint8_t mix_in, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		int32_t mi = mix_in;
		for(int32_t i = mi / 2; i >= 0; --i) {
			int8_t sample = v->synth_sample1[i];
			--mi;
			if(mi >= 0 && mi < SYN_WAVEFORM_LEN) {
				v->synth_sample1[mi] = sample;
			}
			--mi;
			if(mi >= 0 && mi < SYN_WAVEFORM_LEN) {
				v->synth_sample1[mi] = sample;
			}
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_alising ]=====================================================================[=]
static void syn_fx_alising(struct syn_voice *v, uint8_t mix_in, uint8_t alising_level, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		int32_t offset = 0;
		for(int32_t i = 0; i <= mix_in; ++i) {
			if(i + 1 >= SYN_WAVEFORM_LEN || offset >= SYN_WAVEFORM_LEN) {
				break;
			}
			int8_t s1 = v->synth_sample1[offset];
			int8_t s2 = v->synth_sample1[i + 1];
			if(s2 > s1) {
				v->synth_sample1[offset++] = (int8_t)(s1 + alising_level);
			} else if(s2 < s1) {
				v->synth_sample1[offset++] = (int8_t)(s1 - alising_level);
			}
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_egfx1 ]=======================================================================[=]
static void syn_fx_egfx1(struct synthesis_state *s, struct syn_voice *v, uint8_t mix_in, uint8_t eg, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		if(eg >= s->num_eg_tables) {
			return;
		}
		uint8_t *eg_table = s->envelope_generator_tables + (uint32_t)eg * SYN_EG_TABLE_LEN;
		uint32_t mi = mix_in;
		if(mi >= SYN_EG_TABLE_LEN) {
			mi = SYN_EG_TABLE_LEN - 1;
		}
		for(uint32_t i = 0; i <= mi; ++i) {
			if(i + 1 >= SYN_WAVEFORM_LEN) {
				break;
			}
			int8_t s1 = v->synth_sample1[i];
			int8_t s2 = v->synth_sample1[i + 1];
			if(s2 > s1) {
				v->synth_sample1[i] = (int8_t)(s1 + eg_table[v->synth_position]);
			} else if(s2 < s1) {
				v->synth_sample1[i] = (int8_t)(s1 - eg_table[v->synth_position]);
			}
		}
		if(v->synth_position == mi) {
			v->synth_position = 0;
		} else {
			v->synth_position++;
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_egfx2 ]=======================================================================[=]
static void syn_fx_egfx2(struct synthesis_state *s, struct syn_voice *v, uint8_t mix_in, uint8_t eg, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		if(eg >= s->num_eg_tables) {
			return;
		}
		uint8_t *eg_table = s->envelope_generator_tables + (uint32_t)eg * SYN_EG_TABLE_LEN;
		int32_t j = mix_in;
		for(int32_t i = 0; i <= mix_in; ++i, --j) {
			if(i + 1 >= SYN_WAVEFORM_LEN) {
				break;
			}
			if(j < 0 || j >= SYN_EG_TABLE_LEN) {
				continue;
			}
			int8_t s1 = v->synth_sample1[i];
			int8_t s2 = v->synth_sample1[i + 1];
			if(s2 > s1) {
				v->synth_sample1[i] = (int8_t)(s1 + eg_table[j]);
			} else if(s2 < s1) {
				v->synth_sample1[i] = (int8_t)(s1 - eg_table[j]);
			}
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_changer ]=====================================================================[=]
static void syn_fx_changer(struct synthesis_state *s, struct syn_voice *v, uint8_t dest, uint8_t mix_in, uint8_t slow) {
	if(v->slow_motion_counter == 0) {
		v->slow_motion_counter = slow;
		if(dest >= s->num_waveforms) {
			return;
		}
		int8_t *waveform = s->waveforms[dest];
		for(int32_t i = 0; i <= mix_in; ++i) {
			if(i >= SYN_WAVEFORM_LEN) {
				break;
			}
			uint8_t s1 = (uint8_t)v->synth_sample1[i];
			uint8_t s2 = (uint8_t)waveform[i];
			if(s2 > s1) {
				v->synth_sample1[i] = (int8_t)(s1 + 1);
			} else if(s2 < s1) {
				v->synth_sample1[i] = (int8_t)(s1 - 1);
			}
		}
	} else {
		v->slow_motion_counter--;
	}
}

// [=]===^=[ syn_fx_fmdrum ]======================================================================[=]
static void syn_fx_fmdrum(struct syn_voice *v, uint8_t mod_level, uint8_t mod_factor, uint8_t mod_depth) {
	v->slide_increment = (int16_t)(v->slide_increment + (int16_t)((int32_t)mod_level * mod_factor));
	if(v->synth_position == mod_depth) {
		v->synth_position = 0;
		v->slide_increment = 0;
	} else {
		v->synth_position++;
	}
}

// [=]===^=[ syn_do_synth_effects ]===============================================================[=]
static void syn_do_synth_effects(struct synthesis_state *s, struct syn_instrument *ins, struct syn_voice *v) {
	if(ins->effect == SYN_FX_NONE) {
		return;
	}

	uint8_t effect = (v->synth_effect != SYN_FX_NONE) ? v->synth_effect : ins->effect;
	uint8_t a1 = (v->synth_effect_arg1 != 0) ? v->synth_effect_arg1 : ins->effect_arg1;
	uint8_t a2 = (v->synth_effect_arg2 != 0) ? v->synth_effect_arg2 : ins->effect_arg2;
	uint8_t a3 = (v->synth_effect_arg3 != 0) ? v->synth_effect_arg3 : ins->effect_arg3;

	switch(effect) {
		case SYN_FX_ROTATE1: {
			syn_fx_rotate1(v, a1, a2, a3);
			break;
		}

		case SYN_FX_ROTATE2: {
			syn_fx_rotate2(v, a1, a2, a3);
			break;
		}

		case SYN_FX_ALIEN: {
			syn_fx_alien(s, v, a1, a2, a3);
			break;
		}

		case SYN_FX_NEGATOR: {
			syn_fx_negator(v, a1, a2, a3);
			break;
		}

		case SYN_FX_POLYNEG: {
			syn_fx_polyneg(v, a1, a2, a3);
			break;
		}

		case SYN_FX_SHAKER1: {
			syn_fx_shaker1(s, v, a1, a2, a3);
			break;
		}

		case SYN_FX_SHAKER2: {
			syn_fx_shaker2(s, v, a1, a2, a3);
			break;
		}

		case SYN_FX_AMFLFO: {
			syn_fx_amflfo(s, v, a1, a2, a3);
			break;
		}

		case SYN_FX_LASER: {
			syn_fx_laser(v, a1, a2, a3);
			break;
		}

		case SYN_FX_OCTFX1: {
			syn_fx_octfx1(v, a1, a3);
			break;
		}

		case SYN_FX_OCTFX2: {
			syn_fx_octfx2(v, a1, a3);
			break;
		}

		case SYN_FX_ALISING: {
			syn_fx_alising(v, a1, a2, a3);
			break;
		}

		case SYN_FX_EGFX1: {
			syn_fx_egfx1(s, v, a1, a2, a3);
			break;
		}

		case SYN_FX_EGFX2: {
			syn_fx_egfx2(s, v, a1, a2, a3);
			break;
		}

		case SYN_FX_CHANGER: {
			syn_fx_changer(s, v, a1, a2, a3);
			break;
		}

		case SYN_FX_FMDRUM: {
			syn_fx_fmdrum(v, a1, a2, a3);
			break;
		}
	}
}

// [=]===^=[ syn_do_effects ]=====================================================================[=]
static void syn_do_effects(struct synthesis_state *s, struct syn_voice *v, int32_t ch) {
	if(v->transposed_instrument == 0 || v->transposed_instrument > s->num_instruments) {
		paula_mute(&s->paula, ch);
		return;
	}

	struct syn_instrument *ins = &s->instruments[v->transposed_instrument - 1];

	uint16_t period = 0;
	uint16_t previous_period = 0;
	if(!syn_do_arpeggio(s, ins, v, ch, &period, &previous_period)) {
		return;
	}

	if(ins->effect != SYN_FX_NONE) {
		if(((ins->egc_mode == SYN_EGC_OFF) || v->egc_disabled) && !v->synth_effect_disabled) {
			syn_do_synth_effects(s, ins, v);
		}
	}

	syn_do_portamento(&period, &previous_period, v);
	syn_do_vibrato(s, &period, ins, v);

	period = (uint16_t)(period + v->slide_increment);
	paula_set_period(&s->paula, ch, period);

	v->slide_increment = (int16_t)(v->slide_increment - v->slide_speed);

	if(syn_do_adsr(s, ins, v, ch)) {
		syn_do_egc(s, ins, v, ch);
	}
}

// [=]===^=[ syn_tick ]===========================================================================[=]
static void syn_tick(struct synthesis_state *s) {
	s->speed_counter++;

	if(s->speed_counter >= s->current_speed) {
		syn_get_next_row(s);
		for(int32_t i = 0; i < 4; ++i) {
			syn_play_row(s, &s->voices[i], i);
		}
		s->row_position++;
	}

	for(int32_t i = 0; i < 4; ++i) {
		syn_do_effects(s, &s->voices[i], i);
	}
}

// [=]===^=[ synthesis_init ]=====================================================================[=]
static struct synthesis_state *synthesis_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 204 || sample_rate < 8000) {
		return 0;
	}

	uint32_t start = 0;
	if(!syn_identify((uint8_t *)data, len, &start)) {
		return 0;
	}

	struct synthesis_state *s = (struct synthesis_state *)calloc(1, sizeof(struct synthesis_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;
	s->start_offset = start;

	if(!syn_load(s)) {
		syn_cleanup(s);
		free(s);
		return 0;
	}

	syn_build_vibrato_table(s);

	paula_init(&s->paula, sample_rate, SYN_TICK_HZ);
	syn_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ synthesis_free ]=====================================================================[=]
static void synthesis_free(struct synthesis_state *s) {
	if(!s) {
		return;
	}
	syn_cleanup(s);
	free(s);
}

// [=]===^=[ synthesis_get_audio ]================================================================[=]
static void synthesis_get_audio(struct synthesis_state *s, int16_t *output, int32_t frames) {
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
			syn_tick(s);
		}
	}
}

// [=]===^=[ synthesis_api_init ]=================================================================[=]
static void *synthesis_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return synthesis_init(data, len, sample_rate);
}

// [=]===^=[ synthesis_api_free ]=================================================================[=]
static void synthesis_api_free(void *state) {
	synthesis_free((struct synthesis_state *)state);
}

// [=]===^=[ synthesis_api_get_audio ]============================================================[=]
static void synthesis_api_get_audio(void *state, int16_t *output, int32_t frames) {
	synthesis_get_audio((struct synthesis_state *)state, output, frames);
}

static const char *synthesis_extensions[] = { "syn", 0 };

static struct player_api synthesis_api = {
	"Synthesis",
	synthesis_extensions,
	synthesis_api_init,
	synthesis_api_free,
	synthesis_api_get_audio,
	0,
};
