// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// InStereo 1.0 replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz PAL tick rate.
//
// Public API:
//   struct instereo10_state *instereo10_init(void *data, uint32_t len, int32_t sample_rate);
//   void instereo10_free(struct instereo10_state *s);
//   void instereo10_get_audio(struct instereo10_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define IS10_TICK_HZ                  50
#define IS10_ENVELOPE_GENERATOR_LEN   128
#define IS10_ADSR_TABLE_LEN           256
#define IS10_ARPEGGIO_TABLE_LEN       16
#define IS10_WAVEFORM_LEN             256

enum {
	IS10_EFFECT_NONE                = 0x0,
	IS10_EFFECT_SET_SLIDE_SPEED     = 0x1,
	IS10_EFFECT_RESTART_ADSR        = 0x2,
	IS10_EFFECT_RESTART_EGC         = 0x3,
	IS10_EFFECT_SET_SLIDE_INCREMENT = 0x4,
	IS10_EFFECT_SET_VIBRATO_DELAY   = 0x5,
	IS10_EFFECT_SET_VIBRATO_POS     = 0x6,
	IS10_EFFECT_SET_VOLUME          = 0x7,
	IS10_EFFECT_SKIP_NT             = 0x8,
	IS10_EFFECT_SKIP_ST             = 0x9,
	IS10_EFFECT_SET_TRACK_LEN       = 0xa,
	IS10_EFFECT_SKIP_PORTAMENTO     = 0xb,
	IS10_EFFECT_EFF_C               = 0xc,
	IS10_EFFECT_EFF_D               = 0xd,
	IS10_EFFECT_SET_FILTER          = 0xe,
	IS10_EFFECT_SET_SPEED           = 0xf
};

enum {
	IS10_EGC_OFF    = 0,
	IS10_EGC_ONES   = 1,
	IS10_EGC_REPEAT = 2
};

struct is10_sample {
	int8_t *sample_addr;
	uint32_t length;
};

struct is10_instrument {
	uint8_t waveform_number;
	uint8_t synthesis_enabled;
	uint16_t waveform_length;
	uint16_t repeat_length;
	uint8_t volume;
	int8_t portamento_speed;
	uint8_t adsr_enabled;
	uint8_t adsr_table_number;
	uint16_t adsr_table_length;
	uint8_t portamento_enabled;
	uint8_t vibrato_delay;
	uint8_t vibrato_speed;
	uint8_t vibrato_level;
	uint8_t envelope_generator_counter_offset;
	uint8_t envelope_generator_counter_mode;
	uint8_t envelope_generator_counter_table_number;
	uint16_t envelope_generator_counter_table_length;
};

struct is10_song {
	uint8_t start_speed;
	uint8_t rows_per_track;
	uint16_t first_position;
	uint16_t last_position;
	uint16_t restart_position;
};

struct is10_position_voice {
	uint16_t start_track_row;
	int8_t sound_transpose;
	int8_t note_transpose;
};

struct is10_track_line {
	uint8_t note;
	uint8_t instrument;
	uint8_t arpeggio;
	uint8_t effect;
	uint8_t effect_arg;
};

struct is10_voice {
	uint16_t start_track_row;
	int8_t sound_transpose;
	int8_t note_transpose;

	uint8_t note;
	uint8_t instrument;
	uint8_t arpeggio;
	uint8_t effect;
	uint8_t effect_arg;

	uint8_t use_buffer;
	int8_t synth_sample1[IS10_WAVEFORM_LEN];
	int8_t synth_sample2[IS10_WAVEFORM_LEN];

	uint8_t transposed_note;
	uint8_t previous_transposed_note;
	uint8_t transposed_instrument;

	uint8_t current_volume;

	int8_t slide_speed;
	int16_t slide_increment;

	uint8_t portamento_enabled;
	int8_t portamento_speed;
	int16_t portamento_speed_counter;

	uint8_t vibrato_delay;
	uint8_t vibrato_position;

	uint8_t adsr_enabled;
	uint16_t adsr_position;

	uint8_t envelope_generator_counter_mode;
	uint16_t envelope_generator_counter_position;
};

struct instereo10_state {
	struct paula paula;

	struct is10_song *sub_songs;
	uint32_t num_sub_songs;

	struct is10_position_voice (*positions)[4];
	uint32_t num_positions;

	struct is10_track_line *track_lines;
	uint32_t num_track_lines;

	struct is10_sample *samples;
	uint32_t num_samples;

	int8_t (*waveforms)[IS10_WAVEFORM_LEN];
	uint32_t num_waveforms;

	struct is10_instrument *instruments;
	uint32_t num_instruments;

	uint8_t *envelope_generator_tables;
	uint32_t num_envelope_generator_tables;

	uint8_t *adsr_tables;
	uint32_t num_adsr_tables;

	uint8_t arpeggio_tables[16 * IS10_ARPEGGIO_TABLE_LEN];

	int8_t vibrato_table[256];

	struct is10_song *current_song;
	uint8_t speed_counter;
	uint8_t current_speed;
	uint16_t song_position;
	uint8_t row_position;
	uint8_t rows_per_track;

	struct is10_voice voices[4];
};

// [=]===^=[ is10_periods ]=======================================================================[=]
static uint16_t is10_periods[] = {
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

#define IS10_NUM_PERIODS (sizeof(is10_periods) / sizeof(is10_periods[0]))

// [=]===^=[ is10_read_u16_be ]===================================================================[=]
static uint16_t is10_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ is10_read_u32_be ]===================================================================[=]
static uint32_t is10_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ is10_build_vibrato_table ]===========================================================[=]
static void is10_build_vibrato_table(struct instereo10_state *s) {
	int8_t vib_val = 0;
	int32_t offset = 0;

	for(int32_t i = 0; i < 64; ++i) {
		s->vibrato_table[offset++] = vib_val;
		vib_val = (int8_t)(vib_val + 2);
	}

	vib_val = (int8_t)(vib_val + 1);

	for(int32_t i = 0; i < 128; ++i) {
		vib_val = (int8_t)(vib_val - 2);
		s->vibrato_table[offset++] = vib_val;
	}

	vib_val = (int8_t)(vib_val - 1);

	for(int32_t i = 0; i < 64; ++i) {
		s->vibrato_table[offset++] = vib_val;
		vib_val = (int8_t)(vib_val + 2);
	}
}

// [=]===^=[ is10_cleanup ]=======================================================================[=]
static void is10_cleanup(struct instereo10_state *s) {
	if(!s) {
		return;
	}
	free(s->sub_songs); s->sub_songs = 0;
	free(s->positions); s->positions = 0;
	free(s->track_lines); s->track_lines = 0;
	if(s->samples) {
		for(uint32_t i = 0; i < s->num_samples; ++i) {
			free(s->samples[i].sample_addr);
		}
		free(s->samples);
		s->samples = 0;
	}
	free(s->waveforms); s->waveforms = 0;
	free(s->instruments); s->instruments = 0;
	free(s->envelope_generator_tables); s->envelope_generator_tables = 0;
	free(s->adsr_tables); s->adsr_tables = 0;
}

// [=]===^=[ is10_load ]==========================================================================[=]
static int32_t is10_load(struct instereo10_state *s, uint8_t *data, uint32_t len) {
	if(len < 204) {
		return 0;
	}
	if(memcmp(data, "ISM!V1.2", 8) != 0) {
		return 0;
	}

	uint32_t pos = 8;

	if(pos + 4 + 4 + 6 > len) {
		return 0;
	}
	uint16_t total_positions = is10_read_u16_be(data + pos); pos += 2;
	uint16_t total_track_rows = is10_read_u16_be(data + pos); pos += 2;
	pos += 4;

	uint8_t num_samples = data[pos++];
	uint8_t num_waveforms = data[pos++];
	uint8_t num_instruments = data[pos++];
	uint8_t num_sub_songs = data[pos++];
	uint8_t num_egc_tables = data[pos++];
	uint8_t num_adsr_tables = data[pos++];

	// Skip 14 bytes, 28 byte name, 140 bytes
	pos += 14 + 28 + 140;
	if(pos > len) {
		return 0;
	}

	// Sample info: 28 bytes per sample (1+23+4)
	if((uint32_t)pos + (uint32_t)num_samples * 28 > len) {
		return 0;
	}
	pos += (uint32_t)num_samples * 28;

	// Sample lengths
	if((uint32_t)pos + (uint32_t)num_samples * 4 > len) {
		return 0;
	}
	s->samples = (struct is10_sample *)calloc(num_samples ? num_samples : 1, sizeof(struct is10_sample));
	if(!s->samples) {
		return 0;
	}
	s->num_samples = num_samples;
	for(uint32_t i = 0; i < num_samples; ++i) {
		s->samples[i].length = is10_read_u32_be(data + pos);
		pos += 4;
	}

	// Envelope generator tables
	uint32_t egc_total = (uint32_t)num_egc_tables * IS10_ENVELOPE_GENERATOR_LEN;
	if((uint32_t)pos + egc_total > len) {
		return 0;
	}
	if(egc_total > 0) {
		s->envelope_generator_tables = (uint8_t *)malloc(egc_total);
		if(!s->envelope_generator_tables) {
			return 0;
		}
		memcpy(s->envelope_generator_tables, data + pos, egc_total);
	}
	s->num_envelope_generator_tables = num_egc_tables;
	pos += egc_total;

	// ADSR tables
	uint32_t adsr_total = (uint32_t)num_adsr_tables * IS10_ADSR_TABLE_LEN;
	if((uint32_t)pos + adsr_total > len) {
		return 0;
	}
	if(adsr_total > 0) {
		s->adsr_tables = (uint8_t *)malloc(adsr_total);
		if(!s->adsr_tables) {
			return 0;
		}
		memcpy(s->adsr_tables, data + pos, adsr_total);
	}
	s->num_adsr_tables = num_adsr_tables;
	pos += adsr_total;

	// Instruments: 28 bytes each
	if((uint32_t)pos + (uint32_t)num_instruments * 28 > len) {
		return 0;
	}
	s->instruments = (struct is10_instrument *)calloc(num_instruments ? num_instruments : 1, sizeof(struct is10_instrument));
	if(!s->instruments) {
		return 0;
	}
	s->num_instruments = num_instruments;
	for(uint32_t i = 0; i < num_instruments; ++i) {
		uint8_t *p = data + pos;
		struct is10_instrument *ins = &s->instruments[i];
		ins->waveform_number   = p[0];
		ins->synthesis_enabled = (p[1] != 0) ? 1 : 0;
		ins->waveform_length   = is10_read_u16_be(p + 2);
		ins->repeat_length     = is10_read_u16_be(p + 4);
		ins->volume            = p[6];
		ins->portamento_speed  = (int8_t)p[7];
		ins->adsr_enabled      = (p[8] != 0) ? 1 : 0;
		ins->adsr_table_number = p[9];
		ins->adsr_table_length = is10_read_u16_be(p + 10);
		// p[12], p[13] skip
		ins->portamento_enabled = (p[14] != 0) ? 1 : 0;
		// p[15..19] skip (5 bytes)
		ins->vibrato_delay = p[20];
		ins->vibrato_speed = p[21];
		ins->vibrato_level = p[22];
		ins->envelope_generator_counter_offset       = p[23];
		ins->envelope_generator_counter_mode         = p[24];
		ins->envelope_generator_counter_table_number = p[25];
		ins->envelope_generator_counter_table_length = is10_read_u16_be(p + 26);
		pos += 28;
	}

	// Arpeggio tables: 16 * 16 = 256 bytes
	if((uint32_t)pos + sizeof(s->arpeggio_tables) > len) {
		return 0;
	}
	memcpy(s->arpeggio_tables, data + pos, sizeof(s->arpeggio_tables));
	pos += sizeof(s->arpeggio_tables);

	// Sub-song info: 14 bytes each
	if((uint32_t)pos + (uint32_t)num_sub_songs * 14 > len) {
		return 0;
	}
	s->sub_songs = (struct is10_song *)calloc(num_sub_songs ? num_sub_songs : 1, sizeof(struct is10_song));
	if(!s->sub_songs) {
		return 0;
	}
	s->num_sub_songs = num_sub_songs;
	for(uint32_t i = 0; i < num_sub_songs; ++i) {
		uint8_t *p = data + pos + 4;
		struct is10_song *sg = &s->sub_songs[i];
		sg->start_speed      = p[0];
		sg->rows_per_track   = p[1];
		sg->first_position   = is10_read_u16_be(p + 2);
		sg->last_position    = is10_read_u16_be(p + 4);
		sg->restart_position = is10_read_u16_be(p + 6);
		pos += 14;
	}

	// Skip 14 bytes extra sub-song info
	pos += 14;
	if(pos > len) {
		return 0;
	}

	// Waveforms: 256 bytes each (signed)
	uint32_t wf_total = (uint32_t)num_waveforms * IS10_WAVEFORM_LEN;
	if((uint32_t)pos + wf_total > len) {
		return 0;
	}
	s->waveforms = (int8_t (*)[IS10_WAVEFORM_LEN])calloc(num_waveforms ? num_waveforms : 1, IS10_WAVEFORM_LEN);
	if(!s->waveforms) {
		return 0;
	}
	s->num_waveforms = num_waveforms;
	memcpy(s->waveforms, data + pos, wf_total);
	pos += wf_total;

	// Positions: 4 voices * (u16 + i8 + i8) = 16 bytes per position
	uint32_t pos_size = (uint32_t)total_positions * 16;
	if((uint32_t)pos + pos_size > len) {
		return 0;
	}
	s->positions = (struct is10_position_voice (*)[4])calloc(total_positions ? total_positions : 1, sizeof(struct is10_position_voice) * 4);
	if(!s->positions) {
		return 0;
	}
	s->num_positions = total_positions;
	for(uint32_t i = 0; i < total_positions; ++i) {
		for(uint32_t j = 0; j < 4; ++j) {
			uint8_t *p = data + pos;
			s->positions[i][j].start_track_row = is10_read_u16_be(p);
			s->positions[i][j].sound_transpose = (int8_t)p[2];
			s->positions[i][j].note_transpose  = (int8_t)p[3];
			pos += 4;
		}
	}

	// Track rows: 4 bytes each, plus 64 extra empty rows
	uint32_t total_rows = (uint32_t)total_track_rows + 64;
	if((uint32_t)pos + (uint32_t)total_track_rows * 4 > len) {
		return 0;
	}
	s->track_lines = (struct is10_track_line *)calloc(total_rows ? total_rows : 1, sizeof(struct is10_track_line));
	if(!s->track_lines) {
		return 0;
	}
	s->num_track_lines = total_rows;
	for(uint32_t i = 0; i < total_track_rows; ++i) {
		uint8_t *p = data + pos;
		s->track_lines[i].note       = p[0];
		s->track_lines[i].instrument = p[1];
		s->track_lines[i].arpeggio   = (uint8_t)((p[2] & 0xf0) >> 4);
		s->track_lines[i].effect     = (uint8_t)(p[2] & 0x0f);
		s->track_lines[i].effect_arg = p[3];
		pos += 4;
	}

	// Sample data
	for(uint32_t i = 0; i < num_samples; ++i) {
		uint32_t slen = s->samples[i].length;
		if((uint32_t)pos + slen > len) {
			return 0;
		}
		if(slen > 0) {
			s->samples[i].sample_addr = (int8_t *)malloc(slen);
			if(!s->samples[i].sample_addr) {
				return 0;
			}
			memcpy(s->samples[i].sample_addr, data + pos, slen);
		}
		pos += slen;
	}

	return 1;
}

// [=]===^=[ is10_initialize_sound ]==============================================================[=]
static void is10_initialize_sound(struct instereo10_state *s, uint32_t sub_song) {
	if(sub_song >= s->num_sub_songs) {
		sub_song = 0;
	}
	s->current_song = &s->sub_songs[sub_song];
	s->speed_counter   = s->current_song->start_speed;
	s->current_speed   = s->current_song->start_speed;
	s->song_position   = s->current_song->first_position;
	s->row_position    = s->current_song->rows_per_track;
	s->rows_per_track  = s->current_song->rows_per_track;

	for(int32_t i = 0; i < 4; ++i) {
		memset(&s->voices[i], 0, sizeof(s->voices[i]));
		s->voices[i].envelope_generator_counter_mode = IS10_EGC_OFF;
	}
}

// [=]===^=[ is10_play_row ]======================================================================[=]
static void is10_play_row(struct instereo10_state *s, struct is10_voice *v, int32_t channel) {
	v->current_volume = 0;
	v->slide_speed = 0;

	if(v->effect != IS10_EFFECT_NONE) {
		switch(v->effect) {
			case IS10_EFFECT_SET_SLIDE_SPEED: {
				v->slide_speed = (int8_t)v->effect_arg;
				break;
			}
			case IS10_EFFECT_RESTART_ADSR: {
				v->adsr_position = v->effect_arg;
				v->adsr_enabled = 1;
				break;
			}
			case IS10_EFFECT_RESTART_EGC: {
				v->envelope_generator_counter_position = v->effect_arg;
				v->envelope_generator_counter_mode = IS10_EGC_ONES;
				break;
			}
			case IS10_EFFECT_SET_SLIDE_INCREMENT: {
				v->slide_increment = (int8_t)v->effect_arg;
				break;
			}
			case IS10_EFFECT_SET_VIBRATO_DELAY: {
				v->vibrato_delay = v->effect_arg;
				v->vibrato_position = 0;
				break;
			}
			case IS10_EFFECT_SET_VIBRATO_POS: {
				v->vibrato_position = v->effect_arg;
				break;
			}
			case IS10_EFFECT_SET_VOLUME: {
				v->current_volume = (uint8_t)(v->effect_arg & 0x3f);
				break;
			}
			case IS10_EFFECT_SET_TRACK_LEN: {
				if(v->effect_arg <= 64) {
					s->rows_per_track = v->effect_arg;
				}
				break;
			}
			case IS10_EFFECT_SET_FILTER: {
				// Mirrors C# InStereo10Worker: EffectArg == 0 turns filter on
				// (inverse of the DSS / AMOS convention).
				paula_set_lp_filter(&s->paula, v->effect_arg == 0 ? 1 : 0);
				break;
			}
			case IS10_EFFECT_SET_SPEED: {
				if((v->effect_arg > 0) && (v->effect_arg <= 16)) {
					s->current_speed = v->effect_arg;
				}
				break;
			}
			default: break;
		}
	}

	uint8_t note = v->note;
	if(note == 0) {
		return;
	}

	if(note == 0x7f) {
		paula_mute(&s->paula, channel);
		v->current_volume = 0;
		return;
	}

	if(v->effect != IS10_EFFECT_SKIP_NT) {
		note = (uint8_t)(note + v->note_transpose);
	}

	v->previous_transposed_note = v->transposed_note;
	v->transposed_note = note;

	if(note < IS10_NUM_PERIODS) {
		paula_set_period(&s->paula, channel, is10_periods[note]);
	}

	uint8_t instr_num = v->instrument;
	if(instr_num == 0) {
		return;
	}

	if(v->effect != IS10_EFFECT_SKIP_ST) {
		instr_num = (uint8_t)(instr_num + v->sound_transpose);
	}

	v->transposed_instrument = instr_num;

	if(instr_num == 0 || (uint32_t)(instr_num - 1) >= s->num_instruments) {
		return;
	}
	struct is10_instrument *instr = &s->instruments[instr_num - 1];

	v->adsr_enabled = 0;
	v->adsr_position = 0;

	v->vibrato_delay = 0;
	v->vibrato_position = 0;

	v->envelope_generator_counter_mode = IS10_EGC_OFF;
	v->envelope_generator_counter_position = 0;

	v->slide_increment = 0;

	v->portamento_enabled = instr->portamento_enabled;
	v->portamento_speed = instr->portamento_speed;
	v->portamento_speed_counter = instr->portamento_speed;

	if(v->effect == IS10_EFFECT_SKIP_PORTAMENTO) {
		v->portamento_enabled = 0;
		v->portamento_speed = 0;
		v->portamento_speed_counter = 0;
	}

	v->vibrato_delay = instr->vibrato_delay;

	if(instr->adsr_enabled) {
		v->adsr_enabled = 1;
	}

	if(instr->synthesis_enabled) {
		if(instr->waveform_number >= s->num_waveforms) {
			paula_mute(&s->paula, channel);
			return;
		}
		int8_t *waveform = s->waveforms[instr->waveform_number];

		v->use_buffer = 1;
		v->envelope_generator_counter_mode = instr->envelope_generator_counter_mode;

		if(instr->envelope_generator_counter_mode == IS10_EGC_OFF) {
			uint16_t length = instr->waveform_length;
			if(length > IS10_WAVEFORM_LEN) {
				length = IS10_WAVEFORM_LEN;
			}
			memcpy(v->synth_sample1, waveform, length);
			if(instr->envelope_generator_counter_offset != 0) {
				uint32_t lim = instr->envelope_generator_counter_offset;
				if(lim > length) {
					lim = length;
				}
				for(uint32_t i = 0; i < lim; ++i) {
					v->synth_sample1[i] = (int8_t)-v->synth_sample1[i];
				}
			}
			paula_play_sample(&s->paula, channel, v->synth_sample1, length);
			paula_set_loop(&s->paula, channel, 0, length);
		}

		v->current_volume = instr->volume;
		paula_set_volume(&s->paula, channel, v->current_volume);
	} else {
		uint8_t sample_num = (uint8_t)(instr->waveform_number & 0x3f);
		if(sample_num >= s->num_samples) {
			paula_mute(&s->paula, channel);
			return;
		}
		struct is10_sample *sample = &s->samples[sample_num];
		uint32_t play_length = instr->waveform_length;
		uint32_t loop_start = 0;
		uint32_t loop_length = 0;

		if(instr->repeat_length == 0) {
			loop_length = instr->waveform_length;
		} else if(instr->repeat_length != 2) {
			play_length += instr->repeat_length;
			loop_start = instr->waveform_length;
			loop_length = instr->repeat_length;
		}

		if(play_length > sample->length) {
			play_length = sample->length;
		}

		paula_play_sample(&s->paula, channel, sample->sample_addr, play_length);
		if(loop_length != 0) {
			paula_set_loop(&s->paula, channel, loop_start, loop_length);
		}
		paula_set_volume(&s->paula, channel, instr->volume);

		if(sample_num == 7) {
			v->current_volume = (uint8_t)(v->effect_arg & 0x3f);
		} else {
			v->current_volume = instr->volume;
		}
	}
}

// [=]===^=[ is10_get_next_row ]==================================================================[=]
static void is10_get_next_row(struct instereo10_state *s) {
	s->speed_counter = 0;

	if(s->row_position >= s->rows_per_track) {
		s->row_position = 0;

		if(s->song_position > s->current_song->last_position) {
			s->song_position = s->current_song->restart_position;
		}

		s->song_position++;

		uint32_t pos_idx = (uint32_t)(s->song_position - 1);
		if(pos_idx >= s->num_positions) {
			pos_idx = 0;
		}

		for(int32_t i = 0; i < 4; ++i) {
			s->voices[i].start_track_row = s->positions[pos_idx][i].start_track_row;
			s->voices[i].sound_transpose = s->positions[pos_idx][i].sound_transpose;
			s->voices[i].note_transpose  = s->positions[pos_idx][i].note_transpose;
		}
	}

	for(int32_t i = 0; i < 4; ++i) {
		struct is10_voice *v = &s->voices[i];
		uint32_t position = (uint32_t)v->start_track_row + (uint32_t)s->row_position;
		struct is10_track_line tl;
		if(position < s->num_track_lines) {
			tl = s->track_lines[position];
		} else {
			memset(&tl, 0, sizeof(tl));
		}
		v->note       = tl.note;
		v->instrument = tl.instrument;
		v->arpeggio   = tl.arpeggio;
		v->effect     = tl.effect;
		v->effect_arg = tl.effect_arg;
	}

	for(int32_t i = 0; i < 4; ++i) {
		is10_play_row(s, &s->voices[i], i);
	}

	s->row_position++;
}

// [=]===^=[ is10_do_arpeggio ]===================================================================[=]
static void is10_do_arpeggio(struct instereo10_state *s, struct is10_voice *v, uint16_t *period_out, uint16_t *prev_period_out) {
	uint8_t note = v->transposed_note;
	uint8_t prev = v->previous_transposed_note;
	uint8_t arp_val = s->arpeggio_tables[v->arpeggio * 16 + s->speed_counter];
	note = (uint8_t)(note + arp_val);
	prev = (uint8_t)(prev + arp_val);
	*period_out = (note < IS10_NUM_PERIODS) ? is10_periods[note] : 0;
	*prev_period_out = (prev < IS10_NUM_PERIODS) ? is10_periods[prev] : 0;
}

// [=]===^=[ is10_do_portamento ]=================================================================[=]
static void is10_do_portamento(struct is10_voice *v, uint16_t *period_io, uint16_t *prev_io) {
	if(!v->portamento_enabled || (v->portamento_speed_counter == 0) || (*period_io == *prev_io)) {
		return;
	}
	v->portamento_speed_counter--;

	uint16_t a = *period_io;
	uint16_t b = *prev_io;
	*period_io = b;
	*prev_io = a;

	int32_t new_period = ((int32_t)*period_io - (int32_t)*prev_io) * (int32_t)v->portamento_speed_counter;
	if(v->portamento_speed != 0) {
		new_period /= (int32_t)v->portamento_speed;
	}
	new_period += (int32_t)*prev_io;
	*period_io = (uint16_t)new_period;
}

// [=]===^=[ is10_do_vibrato ]====================================================================[=]
static int32_t is10_do_vibrato(struct instereo10_state *s, struct is10_voice *v, uint16_t *period_io) {
	if(v->vibrato_delay == 0) {
		if(v->transposed_instrument == 0) {
			return 0;
		}
		uint32_t inst_idx = (uint32_t)(v->transposed_instrument - 1);
		if(inst_idx >= s->num_instruments) {
			return 0;
		}
		struct is10_instrument *instr = &s->instruments[inst_idx];

		int8_t vib_val = s->vibrato_table[v->vibrato_position];
		uint8_t vib_level = instr->vibrato_level;

		if(vib_val < 0) {
			if(vib_level != 0) {
				*period_io = (uint16_t)(*period_io - (uint16_t)(((-vib_val) * 4) / vib_level));
			}
		} else {
			if(vib_level != 0) {
				*period_io = (uint16_t)(*period_io + (uint16_t)((vib_val * 4) / vib_level));
			} else {
				*period_io = 0;
			}
		}

		v->vibrato_position = (uint8_t)(v->vibrato_position + instr->vibrato_speed);
	} else {
		v->vibrato_delay--;
	}
	return 1;
}

// [=]===^=[ is10_do_adsr ]=======================================================================[=]
static void is10_do_adsr(struct instereo10_state *s, struct is10_voice *v, int32_t channel) {
	uint32_t inst_idx = (uint32_t)(v->transposed_instrument - 1);
	if((v->transposed_instrument == 0) || (inst_idx >= s->num_instruments)) {
		return;
	}
	struct is10_instrument *instr = &s->instruments[inst_idx];

	if(!v->adsr_enabled) {
		return;
	}
	if(v->adsr_position >= instr->adsr_table_length) {
		v->adsr_enabled = 0;
	}

	uint32_t adsr_idx = (uint32_t)instr->adsr_table_number * IS10_ADSR_TABLE_LEN + (uint32_t)v->adsr_position;
	uint8_t raw = (adsr_idx < (uint32_t)s->num_adsr_tables * IS10_ADSR_TABLE_LEN) ? s->adsr_tables[adsr_idx] : 0;
	uint16_t adsr_val = (uint16_t)(raw + 1);

	uint16_t volume = instr->volume;
	volume = (uint16_t)(((uint32_t)volume * (uint32_t)adsr_val) / 128);
	if(volume > 64) {
		volume = 64;
	}
	paula_set_volume(&s->paula, channel, volume);

	v->adsr_position++;
}

// [=]===^=[ is10_do_envelope_generator_counter ]=================================================[=]
static void is10_do_envelope_generator_counter(struct instereo10_state *s, struct is10_voice *v, int32_t channel) {
	if(v->envelope_generator_counter_mode == IS10_EGC_OFF) {
		return;
	}
	uint32_t inst_idx = (uint32_t)(v->transposed_instrument - 1);
	if((v->transposed_instrument == 0) || (inst_idx >= s->num_instruments)) {
		return;
	}
	struct is10_instrument *instr = &s->instruments[inst_idx];
	if(instr->waveform_number >= s->num_waveforms) {
		return;
	}
	int8_t *waveform = s->waveforms[instr->waveform_number];
	int8_t *synth_buf;

	v->use_buffer ^= 1;
	if(v->use_buffer == 0) {
		synth_buf = v->synth_sample2;
	} else {
		synth_buf = v->synth_sample1;
	}

	uint16_t length = instr->waveform_length;
	if(length > IS10_WAVEFORM_LEN) {
		length = IS10_WAVEFORM_LEN;
	}

	memcpy(synth_buf, waveform, length);

	uint32_t egc_idx = (uint32_t)instr->envelope_generator_counter_table_number * IS10_ENVELOPE_GENERATOR_LEN + (uint32_t)v->envelope_generator_counter_position;
	uint8_t egc_raw = (egc_idx < (uint32_t)s->num_envelope_generator_tables * IS10_ENVELOPE_GENERATOR_LEN) ? s->envelope_generator_tables[egc_idx] : 0;
	uint8_t egc_val = (uint8_t)(egc_raw + instr->envelope_generator_counter_offset);

	if(egc_val != 0) {
		uint32_t lim = egc_val;
		if(lim > length) {
			lim = length;
		}
		for(uint32_t i = 0; i < lim; ++i) {
			synth_buf[i] = (int8_t)-synth_buf[i];
		}
	}

	paula_queue_sample(&s->paula, channel, synth_buf, 0, length);
	paula_set_loop(&s->paula, channel, 0, length);

	v->envelope_generator_counter_position++;

	if(v->envelope_generator_counter_position >= instr->envelope_generator_counter_table_length) {
		if(v->envelope_generator_counter_mode == IS10_EGC_ONES) {
			v->envelope_generator_counter_mode = IS10_EGC_OFF;
		} else {
			v->envelope_generator_counter_position = 0;
		}
	}
}

// [=]===^=[ is10_do_effects ]====================================================================[=]
static void is10_do_effects(struct instereo10_state *s, struct is10_voice *v, int32_t channel) {
	uint16_t period;
	uint16_t prev_period;
	is10_do_arpeggio(s, v, &period, &prev_period);
	is10_do_portamento(v, &period, &prev_period);

	if(is10_do_vibrato(s, v, &period)) {
		period = (uint16_t)(period + v->slide_increment);
		paula_set_period(&s->paula, channel, period);

		v->slide_increment = (int16_t)(v->slide_increment - v->slide_speed);
		is10_do_adsr(s, v, channel);
	}

	is10_do_envelope_generator_counter(s, v, channel);
}

// [=]===^=[ is10_tick ]==========================================================================[=]
static void is10_tick(struct instereo10_state *s) {
	s->speed_counter++;
	if(s->speed_counter >= s->current_speed) {
		is10_get_next_row(s);
	}
	for(int32_t i = 0; i < 4; ++i) {
		is10_do_effects(s, &s->voices[i], i);
	}
}

// [=]===^=[ instereo10_init ]====================================================================[=]
static struct instereo10_state *instereo10_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 204 || sample_rate < 8000) {
		return 0;
	}

	struct instereo10_state *s = (struct instereo10_state *)calloc(1, sizeof(struct instereo10_state));
	if(!s) {
		return 0;
	}

	if(!is10_load(s, (uint8_t *)data, len)) {
		is10_cleanup(s);
		free(s);
		return 0;
	}

	is10_build_vibrato_table(s);
	paula_init(&s->paula, sample_rate, IS10_TICK_HZ);
	is10_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ instereo10_free ]====================================================================[=]
static void instereo10_free(struct instereo10_state *s) {
	if(!s) {
		return;
	}
	is10_cleanup(s);
	free(s);
}

// [=]===^=[ instereo10_get_audio ]===============================================================[=]
static void instereo10_get_audio(struct instereo10_state *s, int16_t *output, int32_t frames) {
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
			is10_tick(s);
		}
	}
}

// [=]===^=[ instereo10_api_init ]================================================================[=]
static void *instereo10_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return instereo10_init(data, len, sample_rate);
}

// [=]===^=[ instereo10_api_free ]================================================================[=]
static void instereo10_api_free(void *state) {
	instereo10_free((struct instereo10_state *)state);
}

// [=]===^=[ instereo10_api_get_audio ]===========================================================[=]
static void instereo10_api_get_audio(void *state, int16_t *output, int32_t frames) {
	instereo10_get_audio((struct instereo10_state *)state, output, frames);
}

static const char *instereo10_extensions[] = { "is", "is10", 0 };

static struct player_api instereo10_api = {
	"InStereo 1.0",
	instereo10_extensions,
	instereo10_api_init,
	instereo10_api_free,
	instereo10_api_get_audio,
	0,
};
