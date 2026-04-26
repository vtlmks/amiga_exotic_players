// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Sonic Arranger replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz PAL tick rate.
//
// Only the uncompressed SOARV1.0 form is supported. The lh.library compressed
// @OARV1.0 variant is rejected at identify time (no decompressor here).
//
// Public API:
//   struct sonicarranger_state *sonicarranger_init(void *data, uint32_t len, int32_t sample_rate);
//   void sonicarranger_free(struct sonicarranger_state *s);
//   void sonicarranger_get_audio(struct sonicarranger_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define SA_TICK_HZ       50
#define SA_NUM_CHANNELS  4

// Effect codes (Effect.cs)
enum {
	SA_EFF_ARPEGGIO         = 0x0,
	SA_EFF_SET_SLIDE_SPEED  = 0x1,
	SA_EFF_RESTART_ADSR     = 0x2,
	SA_EFF_SET_VIBRATO      = 0x4,
	SA_EFF_SYNC             = 0x5,
	SA_EFF_SET_MASTER_VOL   = 0x6,
	SA_EFF_SET_PORTAMENTO   = 0x7,
	SA_EFF_SKIP_PORTAMENTO  = 0x8,
	SA_EFF_SET_TRACK_LEN    = 0x9,
	SA_EFF_VOLUME_SLIDE     = 0xa,
	SA_EFF_POSITION_JUMP    = 0xb,
	SA_EFF_SET_VOLUME       = 0xc,
	SA_EFF_TRACK_BREAK      = 0xd,
	SA_EFF_SET_FILTER       = 0xe,
	SA_EFF_SET_SPEED        = 0xf,
};

// Synthesis effect codes (SynthesisEffect.cs)
enum {
	SA_SYN_NONE              = 0,
	SA_SYN_WAVE_NEGATOR      = 1,
	SA_SYN_FREE_NEGATOR      = 2,
	SA_SYN_ROTATE_VERTICAL   = 3,
	SA_SYN_ROTATE_HORIZONTAL = 4,
	SA_SYN_ALIEN_VOICE       = 5,
	SA_SYN_POLY_NEGATOR      = 6,
	SA_SYN_SHACK_WAVE_1      = 7,
	SA_SYN_SHACK_WAVE_2      = 8,
	SA_SYN_METAMORPH         = 9,
	SA_SYN_LASER             = 10,
	SA_SYN_WAVE_ALIAS        = 11,
	SA_SYN_NOISE_GEN_1       = 12,
	SA_SYN_LOW_PASS_1        = 13,
	SA_SYN_LOW_PASS_2        = 14,
	SA_SYN_OSZILATOR         = 15,
	SA_SYN_NOISE_GEN_2       = 16,
	SA_SYN_FM_DRUM           = 17,
};

enum {
	SA_INSTR_SAMPLE = 0,
	SA_INSTR_SYNTH  = 1,
};

struct sa_arpeggio {
	uint8_t length;
	uint8_t repeat;
	int8_t values[14];
};

struct sa_instrument {
	uint16_t type;
	uint16_t waveform_number;
	uint16_t waveform_length;
	uint16_t repeat_length;
	uint16_t volume;
	int16_t fine_tuning;
	uint16_t portamento_speed;
	uint16_t vibrato_delay;
	uint16_t vibrato_speed;
	uint16_t vibrato_level;
	uint16_t amf_number;
	uint16_t amf_delay;
	uint16_t amf_length;
	uint16_t amf_repeat;
	uint16_t adsr_number;
	uint16_t adsr_delay;
	uint16_t adsr_length;
	uint16_t adsr_repeat;
	uint16_t sustain_point;
	uint16_t sustain_delay;
	uint16_t effect_arg1;
	uint16_t effect;
	uint16_t effect_arg2;
	uint16_t effect_arg3;
	uint16_t effect_delay;
	struct sa_arpeggio arpeggios[3];
};

struct sa_track_line {
	uint8_t note;
	uint8_t instrument;
	uint8_t disable_sound_transpose;
	uint8_t disable_note_transpose;
	uint8_t arpeggio;
	uint8_t effect;
	uint8_t effect_arg;
};

struct sa_position {
	uint16_t start_track_row;
	int8_t sound_transpose;
	int8_t note_transpose;
};

struct sa_song_info {
	uint16_t start_speed;
	uint16_t rows_per_track;
	uint16_t first_position;
	uint16_t last_position;
	uint16_t restart_position;
	uint16_t tempo;
};

struct sa_voice {
	uint16_t start_track_row;
	int8_t sound_transpose;
	int8_t note_transpose;

	uint8_t note;
	uint8_t instrument;
	uint8_t disable_sound_transpose;
	uint8_t disable_note_transpose;
	uint8_t arpeggio;
	uint8_t effect;
	uint8_t effect_arg;

	uint16_t transposed_note;
	uint16_t previous_transposed_note;

	uint16_t instrument_type;
	struct sa_instrument *instrument_info;
	uint16_t transposed_instrument;

	uint16_t current_volume;
	int16_t volume_slide_speed;

	uint16_t vibrato_position;
	uint16_t vibrato_delay;
	uint16_t vibrato_speed;
	uint16_t vibrato_level;

	uint16_t portamento_speed;
	uint16_t portamento_period;

	uint16_t arpeggio_position;

	int16_t slide_speed;
	int16_t slide_value;

	uint16_t adsr_position;
	uint16_t adsr_delay_counter;
	uint16_t sustain_delay_counter;

	uint16_t amf_position;
	uint16_t amf_delay_counter;

	uint16_t synth_effect_position;
	uint16_t synth_effect_wave_position;
	uint16_t effect_delay_counter;

	uint8_t flag;

	int8_t waveform_buffer[128];
};

struct sonicarranger_state {
	struct paula paula;

	struct sa_song_info *sub_songs;
	int32_t num_sub_songs;

	struct sa_position *positions;        // flat: num_positions * 4
	int32_t num_positions;

	struct sa_track_line *track_lines;
	int32_t num_track_lines;

	struct sa_instrument *instruments;
	int32_t num_instruments;

	int8_t **sample_data;                 // pointers into module buffer (or 0)
	uint32_t *sample_lengths;
	int32_t num_samples;

	int8_t **waveform_data;               // 128 bytes each, into module buffer
	int32_t num_waveforms;

	uint8_t **adsr_tables;                // 128 bytes each, into module buffer
	int32_t num_adsr_tables;

	int8_t **amf_tables;                  // 128 bytes each, into module buffer
	int32_t num_amf_tables;

	uint8_t *owned_data;                  // non-null when we own the buffer
	                                      // (set when SA Final m68k-embedded
	                                      // input was converted to SOARV1.0)

	struct sa_song_info *current_song_info;

	uint16_t master_volume;
	uint16_t speed_counter;
	uint16_t current_speed;
	int16_t song_position;
	uint16_t row_position;
	uint16_t rows_per_track;

	struct sa_voice voices[SA_NUM_CHANNELS];

	uint32_t random_state;
};

// [=]===^=[ sa_periods ]=========================================================================[=]
static uint16_t sa_periods[109] = {
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

// [=]===^=[ sa_vibrato ]=========================================================================[=]
static int8_t sa_vibrato[256] = {
	   0,    3,    6,    9,   12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,
	  49,   52,   54,   57,   60,   63,   66,   68,   71,   73,   76,   78,   81,   83,   86,   88,
	  90,   92,   94,   96,   98,  100,  102,  104,  106,  108,  109,  111,  112,  114,  115,  116,
	 118,  119,  120,  121,  122,  123,  123,  124,  125,  125,  126,  126,  126,  127,  127,  127,
	 127,  127,  127,  127,  126,  126,  125,  125,  124,  124,  123,  122,  121,  120,  119,  118,
	 117,  116,  114,  113,  112,  110,  108,  107,  105,  103,  101,   99,   97,   95,   93,   91,
	  89,   87,   84,   82,   80,   77,   75,   72,   69,   67,   64,   61,   59,   56,   53,   50,
	  47,   44,   41,   39,   36,   32,   29,   26,   23,   20,   17,   14,   11,    8,    5,    2,
	  -1,   -4,   -7,  -10,  -14,  -17,  -20,  -23,  -26,  -29,  -32,  -35,  -38,  -41,  -44,  -47,
	 -50,  -53,  -55,  -58,  -61,  -64,  -66,  -69,  -72,  -74,  -77,  -79,  -82,  -84,  -86,  -88,
	 -91,  -93,  -95,  -97,  -99, -101, -103, -104, -106, -108, -109, -111, -112, -114, -115, -116,
	-118, -119, -120, -121, -122, -122, -123, -124, -124, -125, -125, -126, -126, -126, -126, -126,
	-126, -126, -126, -126, -126, -125, -125, -124, -124, -123, -122, -121, -120, -119, -118, -117,
	-116, -115, -113, -112, -110, -109, -107, -105, -104, -102, -100,  -98,  -96,  -94,  -92,  -90,
	 -87,  -85,  -83,  -80,  -78,  -75,  -73,  -70,  -68,  -65,  -62,  -60,  -57,  -54,  -51,  -48,
	 -45,  -42,  -39,  -37,  -34,  -30,  -27,  -24,  -21,  -18,  -15,  -12,   -9,   -6,   -3,    0
};

// [=]===^=[ sa_read_u16_be ]=====================================================================[=]
static uint16_t sa_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ sa_read_u32_be ]=====================================================================[=]
static uint32_t sa_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ sa_check_mark ]======================================================================[=]
static int32_t sa_check_mark(uint8_t *p, const char *mark) {
	return (p[0] == (uint8_t)mark[0]) && (p[1] == (uint8_t)mark[1]) && (p[2] == (uint8_t)mark[2]) && (p[3] == (uint8_t)mark[3]);
}

// [=]===^=[ sa_random ]==========================================================================[=]
static int32_t sa_random(struct sonicarranger_state *s) {
	s->random_state = s->random_state * 1103515245u + 12345u;
	return (int32_t)((int8_t)((s->random_state >> 16) & 0xff));
}

// [=]===^=[ sa_cleanup ]=========================================================================[=]
static void sa_cleanup(struct sonicarranger_state *s) {
	if(!s) {
		return;
	}
	free(s->sub_songs); s->sub_songs = 0;
	free(s->positions); s->positions = 0;
	free(s->track_lines); s->track_lines = 0;
	free(s->instruments); s->instruments = 0;
	free(s->sample_data); s->sample_data = 0;
	free(s->sample_lengths); s->sample_lengths = 0;
	free(s->waveform_data); s->waveform_data = 0;
	free(s->adsr_tables); s->adsr_tables = 0;
	free(s->amf_tables); s->amf_tables = 0;
	if(s->owned_data) {
		free(s->owned_data);
		s->owned_data = 0;
	}
}

// [=]===^=[ sa_load_sub_songs ]==================================================================[=]
static int32_t sa_load_sub_songs(struct sonicarranger_state *s, uint8_t *data, uint32_t len, uint32_t *offset) {
	if(*offset + 4 > len) {
		return 0;
	}
	uint32_t count = sa_read_u32_be(data + *offset);
	*offset += 4;
	if(*offset + count * 12 > len) {
		return 0;
	}

	struct sa_song_info *songs = (struct sa_song_info *)calloc(count + 1, sizeof(struct sa_song_info));
	if(!songs) {
		return 0;
	}

	uint32_t kept = 0;
	for(uint32_t i = 0; i < count; ++i) {
		struct sa_song_info ss;
		ss.start_speed      = sa_read_u16_be(data + *offset); *offset += 2;
		ss.rows_per_track   = sa_read_u16_be(data + *offset); *offset += 2;
		ss.first_position   = sa_read_u16_be(data + *offset); *offset += 2;
		ss.last_position    = sa_read_u16_be(data + *offset); *offset += 2;
		ss.restart_position = sa_read_u16_be(data + *offset); *offset += 2;
		ss.tempo            = sa_read_u16_be(data + *offset); *offset += 2;

		if((ss.last_position == 0xffff) || (ss.restart_position == 0xffff)) {
			continue;
		}
		uint32_t dup = 0;
		for(uint32_t j = 0; j < kept; ++j) {
			if(songs[j].first_position == ss.first_position && songs[j].last_position == ss.last_position) {
				dup = 1;
				break;
			}
		}
		if(dup) {
			continue;
		}
		songs[kept++] = ss;
	}

	s->sub_songs = songs;
	s->num_sub_songs = (int32_t)kept;
	return 1;
}

// [=]===^=[ sa_load_positions ]==================================================================[=]
static int32_t sa_load_positions(struct sonicarranger_state *s, uint8_t *data, uint32_t len, uint32_t *offset) {
	if(*offset + 4 > len) {
		return 0;
	}
	uint32_t count = sa_read_u32_be(data + *offset);
	*offset += 4;
	if(*offset + count * 4 * 4 > len) {
		return 0;
	}

	s->positions = (struct sa_position *)calloc((size_t)count * 4, sizeof(struct sa_position));
	if(!s->positions) {
		return 0;
	}
	s->num_positions = (int32_t)count;

	for(uint32_t i = 0; i < count; ++i) {
		for(uint32_t j = 0; j < 4; ++j) {
			struct sa_position *p = &s->positions[i * 4 + j];
			p->start_track_row = sa_read_u16_be(data + *offset); *offset += 2;
			p->sound_transpose = (int8_t)data[(*offset)++];
			p->note_transpose  = (int8_t)data[(*offset)++];
		}
	}
	return 1;
}

// [=]===^=[ sa_load_track_lines ]================================================================[=]
static int32_t sa_load_track_lines(struct sonicarranger_state *s, uint8_t *data, uint32_t len, uint32_t *offset) {
	if(*offset + 4 > len) {
		return 0;
	}
	uint32_t count = sa_read_u32_be(data + *offset);
	*offset += 4;
	if(*offset + count * 4 > len) {
		return 0;
	}

	s->track_lines = (struct sa_track_line *)calloc((size_t)count, sizeof(struct sa_track_line));
	if(!s->track_lines) {
		return 0;
	}
	s->num_track_lines = (int32_t)count;

	for(uint32_t i = 0; i < count; ++i) {
		uint8_t b1 = data[(*offset)++];
		uint8_t b2 = data[(*offset)++];
		uint8_t b3 = data[(*offset)++];
		uint8_t b4 = data[(*offset)++];

		struct sa_track_line *t = &s->track_lines[i];
		t->note                    = b1;
		t->instrument              = b2;
		t->disable_sound_transpose = (b3 & 0x80) ? 1 : 0;
		t->disable_note_transpose  = (b3 & 0x40) ? 1 : 0;
		t->arpeggio                = (uint8_t)((b3 & 0x30) >> 4);
		t->effect                  = (uint8_t)(b3 & 0x0f);
		t->effect_arg              = b4;
	}
	return 1;
}

// [=]===^=[ sa_load_instruments ]================================================================[=]
static int32_t sa_load_instruments(struct sonicarranger_state *s, uint8_t *data, uint32_t len, uint32_t *offset) {
	if(*offset + 4 > len) {
		return 0;
	}
	uint32_t count = sa_read_u32_be(data + *offset);
	*offset += 4;
	if(*offset + count * 152 > len) {
		return 0;
	}

	s->instruments = (struct sa_instrument *)calloc((size_t)count, sizeof(struct sa_instrument));
	if(!s->instruments) {
		return 0;
	}
	s->num_instruments = (int32_t)count;

	for(uint32_t i = 0; i < count; ++i) {
		struct sa_instrument *ins = &s->instruments[i];

		ins->type            = sa_read_u16_be(data + *offset); *offset += 2;
		ins->waveform_number = sa_read_u16_be(data + *offset); *offset += 2;
		ins->waveform_length = sa_read_u16_be(data + *offset); *offset += 2;
		ins->repeat_length   = sa_read_u16_be(data + *offset); *offset += 2;

		*offset += 8;

		ins->volume      = (uint16_t)(sa_read_u16_be(data + *offset) & 0xff); *offset += 2;
		ins->fine_tuning = (int16_t)(int8_t)sa_read_u16_be(data + *offset); *offset += 2;

		ins->portamento_speed = sa_read_u16_be(data + *offset); *offset += 2;

		ins->vibrato_delay = sa_read_u16_be(data + *offset); *offset += 2;
		ins->vibrato_speed = sa_read_u16_be(data + *offset); *offset += 2;
		ins->vibrato_level = sa_read_u16_be(data + *offset); *offset += 2;

		ins->amf_number = sa_read_u16_be(data + *offset); *offset += 2;
		ins->amf_delay  = sa_read_u16_be(data + *offset); *offset += 2;
		ins->amf_length = sa_read_u16_be(data + *offset); *offset += 2;
		ins->amf_repeat = sa_read_u16_be(data + *offset); *offset += 2;

		ins->adsr_number   = sa_read_u16_be(data + *offset); *offset += 2;
		ins->adsr_delay    = sa_read_u16_be(data + *offset); *offset += 2;
		ins->adsr_length   = sa_read_u16_be(data + *offset); *offset += 2;
		ins->adsr_repeat   = sa_read_u16_be(data + *offset); *offset += 2;
		ins->sustain_point = sa_read_u16_be(data + *offset); *offset += 2;
		ins->sustain_delay = sa_read_u16_be(data + *offset); *offset += 2;

		*offset += 16;

		ins->effect_arg1  = sa_read_u16_be(data + *offset); *offset += 2;
		ins->effect       = sa_read_u16_be(data + *offset); *offset += 2;
		ins->effect_arg2  = sa_read_u16_be(data + *offset); *offset += 2;
		ins->effect_arg3  = sa_read_u16_be(data + *offset); *offset += 2;
		ins->effect_delay = sa_read_u16_be(data + *offset); *offset += 2;

		for(int32_t j = 0; j < 3; ++j) {
			ins->arpeggios[j].length = data[(*offset)++];
			ins->arpeggios[j].repeat = data[(*offset)++];
			for(int32_t k = 0; k < 14; ++k) {
				ins->arpeggios[j].values[k] = (int8_t)data[(*offset)++];
			}
		}

		// Skip 30-byte name field
		*offset += 30;
	}
	return 1;
}

// [=]===^=[ sa_load_sample_info ]================================================================[=]
static int32_t sa_load_sample_info(struct sonicarranger_state *s, uint8_t *data, uint32_t len, uint32_t *offset) {
	if(*offset + 4 > len) {
		return 0;
	}
	uint32_t count = sa_read_u32_be(data + *offset);
	*offset += 4;
	s->num_samples = (int32_t)count;
	if(count != 0) {
		uint32_t skip = count * 4 * 2 + count * 30;
		if(*offset + skip > len) {
			return 0;
		}
		*offset += skip;
	}
	return 1;
}

// [=]===^=[ sa_load_sample_data ]================================================================[=]
static int32_t sa_load_sample_data(struct sonicarranger_state *s, uint8_t *data, uint32_t len, uint32_t *offset) {
	if(s->num_samples == 0) {
		return 1;
	}

	s->sample_data = (int8_t **)calloc((size_t)s->num_samples, sizeof(int8_t *));
	s->sample_lengths = (uint32_t *)calloc((size_t)s->num_samples, sizeof(uint32_t));
	if(!s->sample_data || !s->sample_lengths) {
		return 0;
	}

	if(*offset + (uint32_t)s->num_samples * 4 > len) {
		return 0;
	}
	for(int32_t i = 0; i < s->num_samples; ++i) {
		s->sample_lengths[i] = sa_read_u32_be(data + *offset);
		*offset += 4;
	}

	for(int32_t i = 0; i < s->num_samples; ++i) {
		uint32_t slen = s->sample_lengths[i];
		if(slen > 0) {
			if(*offset + slen > len) {
				return 0;
			}
			s->sample_data[i] = (int8_t *)(data + *offset);
			*offset += slen;
		}
	}
	return 1;
}

// [=]===^=[ sa_load_waveforms ]==================================================================[=]
static int32_t sa_load_waveforms(struct sonicarranger_state *s, uint8_t *data, uint32_t len, uint32_t *offset) {
	if(*offset + 4 > len) {
		return 0;
	}
	uint32_t count = sa_read_u32_be(data + *offset);
	*offset += 4;
	if(*offset + count * 128 > len) {
		return 0;
	}

	s->waveform_data = (int8_t **)calloc((size_t)count, sizeof(int8_t *));
	if(!s->waveform_data) {
		return 0;
	}
	s->num_waveforms = (int32_t)count;
	for(uint32_t i = 0; i < count; ++i) {
		s->waveform_data[i] = (int8_t *)(data + *offset);
		*offset += 128;
	}
	return 1;
}

// [=]===^=[ sa_load_adsr_tables ]================================================================[=]
static int32_t sa_load_adsr_tables(struct sonicarranger_state *s, uint8_t *data, uint32_t len, uint32_t *offset) {
	if(*offset + 4 > len) {
		return 0;
	}
	uint32_t count = sa_read_u32_be(data + *offset);
	*offset += 4;
	if(*offset + count * 128 > len) {
		return 0;
	}

	s->adsr_tables = (uint8_t **)calloc((size_t)count, sizeof(uint8_t *));
	if(!s->adsr_tables) {
		return 0;
	}
	s->num_adsr_tables = (int32_t)count;
	for(uint32_t i = 0; i < count; ++i) {
		s->adsr_tables[i] = data + *offset;
		*offset += 128;
	}
	return 1;
}

// [=]===^=[ sa_load_amf_tables ]=================================================================[=]
static int32_t sa_load_amf_tables(struct sonicarranger_state *s, uint8_t *data, uint32_t len, uint32_t *offset) {
	if(*offset + 4 > len) {
		return 0;
	}
	uint32_t count = sa_read_u32_be(data + *offset);
	*offset += 4;
	if(*offset + count * 128 > len) {
		return 0;
	}

	s->amf_tables = (int8_t **)calloc((size_t)count, sizeof(int8_t *));
	if(!s->amf_tables) {
		return 0;
	}
	s->num_amf_tables = (int32_t)count;
	for(uint32_t i = 0; i < count; ++i) {
		s->amf_tables[i] = (int8_t *)(data + *offset);
		*offset += 128;
	}
	return 1;
}

// [=]===^=[ sa_identify ]========================================================================[=]
static int32_t sa_identify(uint8_t *data, uint32_t len) {
	if(len < 16) {
		return 0;
	}
	if(data[0] == 'S' && data[1] == 'O' && data[2] == 'A' && data[3] == 'R'
		&& data[4] == 'V' && data[5] == '1' && data[6] == '.' && data[7] == '0') {
		return 1;
	}
	return 0;
}

// [=]===^=[ sa_test_final ]======================================================================[=]
// Detect Sonic Arranger Final format (m68k-executable embedded module).
// Mirrors C# SonicArrangerFinalFormat.Identify (the JMP-table branch only).
// On success returns the offset within `data` to the embedded SA module
// header (8 u32 BE section offsets); on failure returns 0.
static uint32_t sa_test_final(uint8_t *data, uint32_t len) {
	if(len < 0x1630) {
		return 0;
	}
	// First 4 bytes must be 4EFA xxxx (m68k JMP pc-relative).
	if(data[0] != 0x4e || data[1] != 0xfa) {
		return 0;
	}
	int16_t init_disp = (int16_t)(((uint16_t)data[2] << 8) | data[3]);
	int32_t init_off = init_disp + 2;       // C# adds 2 for the bra.w base
	// 6 more JMP instructions follow at offsets 4, 8, 12, 16, 20, 24.
	for(uint32_t i = 1; i < 7; ++i) {
		uint32_t at = i * 4;
		if(data[at] != 0x4e || data[at + 1] != 0xfa) {
			return 0;
		}
	}
	// At init_off, the init routine begins with movem.l + lea.
	if(init_off < 0 || (uint32_t)init_off + 8 > len) {
		return 0;
	}
	if(data[init_off + 0] != 0x48 || data[init_off + 1] != 0xe7
	   || data[init_off + 2] != 0xff || data[init_off + 3] != 0xfe) {
		return 0;
	}
	if(data[init_off + 4] != 0x41 || data[init_off + 5] != 0xfa) {
		return 0;
	}
	int16_t lea_disp = (int16_t)(((uint16_t)data[init_off + 6] << 8) | data[init_off + 7]);
	int64_t mod_off = (int64_t)init_off + 6 + lea_disp;
	if(mod_off <= 0 || mod_off >= (int64_t)len) {
		return 0;
	}
	return (uint32_t)mod_off;
}

// [=]===^=[ sa_be_w_u32 ]========================================================================[=]
static void sa_be_w_u32(uint8_t *p, uint32_t v) {
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

// [=]===^=[ sa_convert_final ]===================================================================[=]
// Convert Sonic Arranger Final embedded data at `data + mod_off` to a fresh
// SOARV1.0 buffer. Mirrors C# SonicArrangerFinalFormat.Convert. On success
// returns a malloc'd buffer (caller frees) and writes its size to *out_len.
// On failure returns 0.
static uint8_t *sa_convert_final(uint8_t *data, uint32_t len, uint32_t mod_off, uint32_t *out_len) {
	if(mod_off + 32 > len) {
		return 0;
	}
	uint32_t off[8];
	for(uint32_t i = 0; i < 8; ++i) {
		off[i] = sa_read_u32_be(data + mod_off + i * 4);
	}
	// Section deltas must form a non-decreasing sequence of in-bounds offsets.
	for(uint32_t i = 0; i < 7; ++i) {
		if(off[i] > off[i + 1]) {
			return 0;
		}
	}
	if(mod_off + off[7] + 4 > len) {
		return 0;
	}
	if(((off[1] - off[0]) % 12) != 0
	   || ((off[2] - off[1]) % 16) != 0
	   || ((off[3] - off[2]) % 4) != 0
	   || ((off[4] - off[3]) % 152) != 0
	   || ((off[5] - off[4]) % 128) != 0
	   || ((off[6] - off[5]) % 128) != 0
	   || ((off[7] - off[6]) % 128) != 0) {
		return 0;
	}
	uint32_t cs = (off[1] - off[0]) / 12;       // sub-songs
	uint32_t cp = (off[2] - off[1]) / 16;       // positions
	uint32_t ct = (off[3] - off[2]) / 4;        // track rows
	uint32_t ci = (off[4] - off[3]) / 152;      // instruments
	uint32_t cw = (off[5] - off[4]) / 128;      // waveforms
	uint32_t ca = (off[6] - off[5]) / 128;      // adsr tables
	uint32_t cf = (off[7] - off[6]) / 128;      // amf tables
	if(cs == 0 || cp == 0 || ct == 0 || ci == 0) {
		return 0;
	}
	uint32_t cn = sa_read_u32_be(data + mod_off + off[7]);
	if(cn > 0x10000) {
		return 0;
	}
	if(mod_off + off[7] + 4 + cn * 4 > len) {
		return 0;
	}
	uint8_t *smp_lens_ptr = data + mod_off + off[7] + 4;
	uint64_t total_smp = 0;
	for(uint32_t i = 0; i < cn; ++i) {
		uint32_t sl = sa_read_u32_be(smp_lens_ptr + i * 4);
		total_smp += sl;
	}
	if(total_smp >= 0x40000000ULL) {
		return 0;
	}
	if(mod_off + off[7] + 4 + cn * 4 + total_smp > len) {
		return 0;
	}

	uint64_t out_size = 8                                 // SOARV1.0
	    + 4 + 4 + (uint64_t)cs * 12                        // STBL
	    + 4 + 4 + (uint64_t)cp * 16                        // OVTB
	    + 4 + 4 + (uint64_t)ct * 4                         // NTBL
	    + 4 + 4 + (uint64_t)ci * 152                       // INST
	    + 4 + 4 + (uint64_t)cn * 8 + (uint64_t)cn * 30     // SD8B info
	    + (uint64_t)cn * 4 + total_smp                     // SD8B data
	    + 4 + 4 + (uint64_t)cw * 128                       // SYWT
	    + 4 + 4 + (uint64_t)ca * 128                       // SYAR
	    + 4 + 4 + (uint64_t)cf * 128;                      // SYAF
	uint8_t *out = (uint8_t *)calloc(1, (size_t)out_size);
	if(!out) {
		return 0;
	}
	uint32_t op = 0;
	memcpy(out + op, "SOARV1.0", 8); op += 8;

	memcpy(out + op, "STBL", 4); op += 4;
	sa_be_w_u32(out + op, cs); op += 4;
	memcpy(out + op, data + mod_off + off[0], cs * 12); op += cs * 12;

	memcpy(out + op, "OVTB", 4); op += 4;
	sa_be_w_u32(out + op, cp); op += 4;
	memcpy(out + op, data + mod_off + off[1], cp * 16); op += cp * 16;

	memcpy(out + op, "NTBL", 4); op += 4;
	sa_be_w_u32(out + op, ct); op += 4;
	memcpy(out + op, data + mod_off + off[2], ct * 4); op += ct * 4;

	memcpy(out + op, "INST", 4); op += 4;
	sa_be_w_u32(out + op, ci); op += 4;
	memcpy(out + op, data + mod_off + off[3], ci * 152); op += ci * 152;

	// SD8B header + per-sample oneshot/repeat lengths derived from instruments
	// of type==0 with SampleNumber == i. Holes (no instrument referencing
	// sample i) get oneshot=0/repeat=0.
	memcpy(out + op, "SD8B", 4); op += 4;
	sa_be_w_u32(out + op, cn); op += 4;
	{
		uint32_t op_oneshot = op;
		uint32_t op_repeat  = op + cn * 4;
		op += cn * 8;
		// Names: cn * 30 zero bytes (calloc already zero).
		op += cn * 30;
		// Instrument scan.
		for(uint32_t i = 0; i < ci; ++i) {
			uint8_t *ip = data + mod_off + off[3] + i * 152;
			uint16_t type = (uint16_t)((ip[0] << 8) | ip[1]);
			uint16_t number = (uint16_t)((ip[2] << 8) | ip[3]);
			uint16_t oneshot = (uint16_t)((ip[4] << 8) | ip[5]);
			uint16_t repeat = (uint16_t)((ip[6] << 8) | ip[7]);
			if(type == 0 && number < cn) {
				sa_be_w_u32(out + op_oneshot + number * 4, oneshot);
				sa_be_w_u32(out + op_repeat + number * 4, repeat);
			}
		}
	}
	// Sample lengths from file + raw sample data.
	memcpy(out + op, smp_lens_ptr, cn * 4); op += cn * 4;
	{
		uint8_t *src = smp_lens_ptr + cn * 4;
		for(uint32_t i = 0; i < cn; ++i) {
			uint32_t sl = sa_read_u32_be(smp_lens_ptr + i * 4);
			if(sl > 0) {
				memcpy(out + op, src, sl);
				src += sl;
				op += sl;
			}
		}
	}

	memcpy(out + op, "SYWT", 4); op += 4;
	sa_be_w_u32(out + op, cw); op += 4;
	if(cw > 0) {
		memcpy(out + op, data + mod_off + off[4], cw * 128);
		op += cw * 128;
	}

	memcpy(out + op, "SYAR", 4); op += 4;
	sa_be_w_u32(out + op, ca); op += 4;
	if(ca > 0) {
		memcpy(out + op, data + mod_off + off[5], ca * 128);
		op += ca * 128;
	}

	memcpy(out + op, "SYAF", 4); op += 4;
	sa_be_w_u32(out + op, cf); op += 4;
	if(cf > 0) {
		memcpy(out + op, data + mod_off + off[6], cf * 128);
		op += cf * 128;
	}

	*out_len = op;
	return out;
}

// [=]===^=[ sa_load ]============================================================================[=]
static int32_t sa_load(struct sonicarranger_state *s, uint8_t *data, uint32_t len) {
	uint32_t off = 8;

	if(off + 4 > len || !sa_check_mark(data + off, "STBL")) {
		return 0;
	}
	off += 4;
	if(!sa_load_sub_songs(s, data, len, &off)) {
		return 0;
	}

	if(off + 4 > len || !sa_check_mark(data + off, "OVTB")) {
		return 0;
	}
	off += 4;
	if(!sa_load_positions(s, data, len, &off)) {
		return 0;
	}

	if(off + 4 > len || !sa_check_mark(data + off, "NTBL")) {
		return 0;
	}
	off += 4;
	if(!sa_load_track_lines(s, data, len, &off)) {
		return 0;
	}

	if(off + 4 > len || !sa_check_mark(data + off, "INST")) {
		return 0;
	}
	off += 4;
	if(!sa_load_instruments(s, data, len, &off)) {
		return 0;
	}

	if(off + 4 > len || !sa_check_mark(data + off, "SD8B")) {
		return 0;
	}
	off += 4;
	if(!sa_load_sample_info(s, data, len, &off)) {
		return 0;
	}
	if(!sa_load_sample_data(s, data, len, &off)) {
		return 0;
	}

	if(off + 4 > len || !sa_check_mark(data + off, "SYWT")) {
		return 0;
	}
	off += 4;
	if(!sa_load_waveforms(s, data, len, &off)) {
		return 0;
	}

	if(off + 4 > len || !sa_check_mark(data + off, "SYAR")) {
		return 0;
	}
	off += 4;
	if(!sa_load_adsr_tables(s, data, len, &off)) {
		return 0;
	}

	if(off + 4 > len || !sa_check_mark(data + off, "SYAF")) {
		return 0;
	}
	off += 4;
	if(!sa_load_amf_tables(s, data, len, &off)) {
		return 0;
	}

	return 1;
}

// [=]===^=[ sa_initialize_sound ]================================================================[=]
static void sa_initialize_sound(struct sonicarranger_state *s, int32_t sub_song) {
	if(sub_song < 0 || sub_song >= s->num_sub_songs) {
		sub_song = 0;
	}
	s->current_song_info = &s->sub_songs[sub_song];

	s->master_volume   = 64;
	s->speed_counter   = s->current_song_info->start_speed;
	s->current_speed   = s->current_song_info->start_speed;
	s->song_position   = (int16_t)(s->current_song_info->first_position - 1);
	s->row_position    = s->current_song_info->rows_per_track;
	s->rows_per_track  = s->current_song_info->rows_per_track;

	for(int32_t i = 0; i < SA_NUM_CHANNELS; ++i) {
		struct sa_voice *v = &s->voices[i];
		memset(v, 0, sizeof(*v));
		v->effect = SA_EFF_ARPEGGIO;
		v->instrument_type = SA_INSTR_SAMPLE;
		v->flag = 0x01;
	}
}

// [=]===^=[ sa_force_quiet ]=====================================================================[=]
static void sa_force_quiet(struct sonicarranger_state *s, struct sa_voice *v, int32_t chan) {
	paula_mute(&s->paula, chan);
	v->current_volume = 0;
	v->transposed_instrument = 0;
	v->instrument_info = 0;
	v->instrument_type = SA_INSTR_SAMPLE;
}

// [=]===^=[ sa_set_synth_instrument ]============================================================[=]
static void sa_set_synth_instrument(struct sa_voice *v, struct sa_instrument *ins) {
	v->slide_value = ins->fine_tuning;
	v->adsr_delay_counter = ins->adsr_delay;
	v->adsr_position = 0;
	v->amf_delay_counter = ins->amf_delay;
	v->amf_position = 0;
	v->synth_effect_position = ins->effect_arg2;
	v->synth_effect_wave_position = 0;
	v->effect_delay_counter = ins->effect_delay;
	v->arpeggio_position = 0;
	v->flag = 0x00;
}

// [=]===^=[ sa_set_instrument ]==================================================================[=]
static struct sa_instrument *sa_set_instrument(struct sonicarranger_state *s, struct sa_voice *v, uint16_t instr_num) {
	v->transposed_instrument = (uint16_t)(instr_num & 0xff);
	if(v->transposed_instrument == 0 || (int32_t)v->transposed_instrument > s->num_instruments) {
		return 0;
	}
	struct sa_instrument *ins = &s->instruments[v->transposed_instrument - 1];
	v->instrument_info = ins;

	v->current_volume = ins->volume;

	v->portamento_speed = ins->portamento_speed;
	v->portamento_period = 0;

	v->vibrato_position = 0;
	v->vibrato_delay = ins->vibrato_delay;
	v->vibrato_speed = ins->vibrato_speed;
	v->vibrato_level = ins->vibrato_level;

	sa_set_synth_instrument(v, ins);
	return ins;
}

// [=]===^=[ sa_play_sample_instrument ]==========================================================[=]
static void sa_play_sample_instrument(struct sonicarranger_state *s, struct sa_voice *v, int32_t chan, struct sa_instrument *ins) {
	if(ins->waveform_number >= (uint16_t)s->num_samples) {
		v->flag |= 0x01;
		sa_force_quiet(s, v, chan);
		return;
	}
	int8_t *data = s->sample_data[ins->waveform_number];
	if(!data) {
		v->flag |= 0x01;
		sa_force_quiet(s, v, chan);
		return;
	}

	uint32_t play_length = ins->waveform_length;
	uint32_t loop_start = 0;
	uint32_t loop_length = 0;
	uint32_t data_len = s->sample_lengths[ins->waveform_number];

	if(ins->repeat_length == 0) {
		loop_length = ins->waveform_length;
	} else if(ins->repeat_length != 1) {
		play_length += ins->repeat_length;
		loop_start = (uint32_t)ins->waveform_length * 2u;
		loop_length = (uint32_t)ins->repeat_length * 2u;
	}

	play_length *= 2u;
	if(play_length > data_len) {
		play_length = data_len;
	}
	if((loop_start + loop_length) > play_length) {
		loop_length = (loop_start <= play_length) ? (play_length - loop_start) : 0;
	}

	if(play_length > 0) {
		paula_play_sample(&s->paula, chan, data, play_length);
		if(loop_length != 0) {
			paula_set_loop(&s->paula, chan, loop_start, loop_length);
		}
	} else {
		v->flag |= 0x01;
		sa_force_quiet(s, v, chan);
	}
}

// [=]===^=[ sa_play_synth_instrument ]===========================================================[=]
static void sa_play_synth_instrument(struct sonicarranger_state *s, struct sa_voice *v, int32_t chan, struct sa_instrument *ins) {
	if(ins->waveform_number >= (uint16_t)s->num_waveforms) {
		v->flag |= 0x01;
		sa_force_quiet(s, v, chan);
		return;
	}
	int8_t *src = s->waveform_data[ins->waveform_number];
	uint32_t length = (uint32_t)ins->waveform_length * 2u;
	if(length > 128) {
		length = 128;
	}

	memcpy(v->waveform_buffer, src, length);

	paula_play_sample(&s->paula, chan, v->waveform_buffer, length);
	paula_set_loop(&s->paula, chan, 0, length);
}

// [=]===^=[ sa_set_effects ]=====================================================================[=]
static void sa_set_effects(struct sonicarranger_state *s, struct sa_voice *v) {
	v->slide_speed = 0;
	v->volume_slide_speed = 0;

	switch(v->effect) {
		case SA_EFF_SET_SLIDE_SPEED: {
			v->slide_speed = (int8_t)v->effect_arg;
			break;
		}

		case SA_EFF_RESTART_ADSR: {
			v->adsr_position = v->effect_arg;
			break;
		}

		case SA_EFF_SET_VIBRATO: {
			v->vibrato_delay = 0;
			v->vibrato_speed = (uint16_t)((v->effect_arg & 0xf0) >> 3);
			v->vibrato_level = (uint16_t)(-((v->effect_arg & 0x0f) << 4) + 160);
			break;
		}

		case SA_EFF_SET_MASTER_VOL: {
			s->master_volume = (uint16_t)((v->effect_arg == 64) ? 64 : (v->effect_arg & 0x3f));
			break;
		}

		case SA_EFF_SET_PORTAMENTO: {
			v->portamento_speed = v->effect_arg;
			break;
		}

		case SA_EFF_SKIP_PORTAMENTO: {
			v->portamento_speed = 0;
			break;
		}

		case SA_EFF_SET_TRACK_LEN: {
			if(v->effect_arg <= 64) {
				s->rows_per_track = v->effect_arg;
			}
			break;
		}

		case SA_EFF_VOLUME_SLIDE: {
			v->volume_slide_speed = (int16_t)(int8_t)v->effect_arg;
			break;
		}

		case SA_EFF_POSITION_JUMP: {
			s->song_position = (int16_t)((int32_t)v->effect_arg - 1);
			s->row_position = s->rows_per_track;
			break;
		}

		case SA_EFF_SET_VOLUME: {
			uint8_t nv = v->effect_arg;
			if(nv > 64) {
				nv = 64;
			}
			v->current_volume = nv;
			break;
		}

		case SA_EFF_TRACK_BREAK: {
			s->row_position = s->rows_per_track;
			break;
		}

		case SA_EFF_SET_FILTER: {
			break;
		}

		case SA_EFF_SET_SPEED: {
			if((v->effect_arg > 0) && (v->effect_arg <= 16)) {
				s->current_speed = v->effect_arg;
			}
			break;
		}
	}
}

// [=]===^=[ sa_play_voice ]======================================================================[=]
static void sa_play_voice(struct sonicarranger_state *s, struct sa_voice *v, int32_t chan) {
	uint16_t note = v->note;
	int16_t instr_num = (int16_t)v->instrument;

	if(note == 0) {
		if(instr_num != 0) {
			sa_set_instrument(s, v, (uint16_t)instr_num);
		}
	} else if(note != 0x80) {
		if(note == 0x7f) {
			sa_force_quiet(s, v, chan);
		} else {
			if(!v->disable_note_transpose) {
				note = (uint16_t)((int32_t)note + v->note_transpose);
			}
			if((instr_num != 0) && !v->disable_sound_transpose) {
				instr_num = (int16_t)(instr_num + v->sound_transpose);
			}

			v->previous_transposed_note = v->transposed_note;
			v->transposed_note = note;
			if(v->previous_transposed_note == 0) {
				v->previous_transposed_note = note;
			}

			struct sa_instrument *ins;

			if(instr_num < 0) {
				v->flag |= 0x01;
				sa_force_quiet(s, v, chan);
				return;
			}

			if(instr_num == 0) {
				ins = v->instrument_info;
				if(!ins) {
					sa_force_quiet(s, v, chan);
					return;
				}
				sa_set_synth_instrument(v, ins);
			} else {
				ins = sa_set_instrument(s, v, (uint16_t)instr_num);
				if(!ins) {
					sa_force_quiet(s, v, chan);
					return;
				}
			}

			v->instrument_type = ins->type;

			if(v->instrument_type == SA_INSTR_SAMPLE) {
				sa_play_sample_instrument(s, v, chan, ins);
			} else {
				sa_play_synth_instrument(s, v, chan, ins);
			}
		}
	}

	sa_set_effects(s, v);
}

// [=]===^=[ sa_get_new_notes ]===================================================================[=]
static void sa_get_new_notes(struct sonicarranger_state *s) {
	for(int32_t i = 0; i < SA_NUM_CHANNELS; ++i) {
		struct sa_voice *v = &s->voices[i];
		int32_t pos = (int32_t)v->start_track_row + (int32_t)s->row_position;

		struct sa_track_line t;
		if(pos < s->num_track_lines) {
			t = s->track_lines[pos];
		} else {
			memset(&t, 0, sizeof(t));
		}

		v->note                    = t.note;
		v->instrument              = t.instrument;
		v->disable_sound_transpose = t.disable_sound_transpose;
		v->disable_note_transpose  = t.disable_note_transpose;
		v->arpeggio                = t.arpeggio;
		v->effect                  = t.effect;
		v->effect_arg              = t.effect_arg;

		sa_play_voice(s, v, i);
	}
}

// [=]===^=[ sa_get_next_position ]===============================================================[=]
static void sa_get_next_position(struct sonicarranger_state *s) {
	s->song_position++;

	if((s->song_position > (int16_t)s->current_song_info->last_position) || (s->song_position >= s->num_positions)) {
		s->song_position = (int16_t)s->current_song_info->restart_position;
	}

	if(s->song_position < 0) {
		s->song_position = 0;
	}

	struct sa_position *row = &s->positions[s->song_position * 4];
	for(int32_t i = 0; i < SA_NUM_CHANNELS; ++i) {
		s->voices[i].start_track_row = row[i].start_track_row;
		s->voices[i].sound_transpose = row[i].sound_transpose;
		s->voices[i].note_transpose  = row[i].note_transpose;
	}
}

// [=]===^=[ sa_get_next_row ]====================================================================[=]
static void sa_get_next_row(struct sonicarranger_state *s) {
	s->row_position++;
	if(s->row_position >= s->rows_per_track) {
		s->row_position = 0;
		sa_get_next_position(s);
	}
	sa_get_new_notes(s);
}

// [=]===^=[ sa_do_arpeggio ]=====================================================================[=]
static void sa_do_arpeggio(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins, uint16_t *out_period, uint16_t *out_prev_period) {
	uint16_t note = v->transposed_note;
	uint16_t prev_note = v->previous_transposed_note;

	if(v->arpeggio != 0) {
		struct sa_arpeggio *arp = &ins->arpeggios[v->arpeggio - 1];
		int8_t arp_val = arp->values[v->arpeggio_position];
		note = (uint16_t)((int32_t)note + arp_val);
		prev_note = (uint16_t)((int32_t)prev_note + arp_val);

		v->arpeggio_position++;
		int32_t max_length = (int32_t)arp->length + (int32_t)arp->repeat;
		if(max_length > 13) {
			max_length = 13;
		}
		if((int32_t)v->arpeggio_position > max_length) {
			v->arpeggio_position = arp->length;
		}
	} else if((v->effect == SA_EFF_ARPEGGIO) && (v->effect_arg != 0)) {
		uint16_t arp_val;
		switch(s->speed_counter % 3) {
			default:
			case 0: {
				arp_val = 0;
				break;
			}

			case 1: {
				arp_val = (uint16_t)(v->effect_arg >> 4);
				break;
			}

			case 2: {
				arp_val = (uint16_t)(v->effect_arg & 0x0f);
				break;
			}
		}
		note += arp_val;
		prev_note += arp_val;
	}

	if(note >= 109) {
		note = 0;
	}
	if(prev_note >= 109) {
		prev_note = 0;
	}

	*out_period = sa_periods[note];
	*out_prev_period = sa_periods[prev_note];
}

// [=]===^=[ sa_do_portamento ]===================================================================[=]
static void sa_do_portamento(struct sa_voice *v, uint16_t *period, uint16_t prev_period) {
	if(v->portamento_speed == 0) {
		return;
	}
	if(v->portamento_period == 0) {
		v->portamento_period = prev_period;
	}

	int32_t diff = (int32_t)*period - (int32_t)v->portamento_period;
	if(diff < 0) {
		diff = -diff;
	}
	diff -= v->portamento_speed;
	if(diff < 0) {
		v->portamento_speed = 0;
	} else {
		uint16_t new_period = v->portamento_period;
		if(new_period >= *period) {
			new_period -= v->portamento_speed;
		} else {
			new_period += v->portamento_speed;
		}
		v->portamento_period = new_period;
		*period = new_period;
	}
}

// [=]===^=[ sa_do_vibrato ]======================================================================[=]
static void sa_do_vibrato(struct sa_voice *v, uint16_t *period, struct sa_instrument *ins) {
	if(v->vibrato_delay == 255) {
		return;
	}
	if(v->vibrato_delay == 0) {
		int8_t vib_val = sa_vibrato[v->vibrato_position & 0xff];
		uint16_t vib_level = ins->vibrato_level;
		if(vib_val != 0 && vib_level != 0) {
			*period = (uint16_t)((int32_t)*period + ((int32_t)vib_val * 4) / (int32_t)vib_level);
		}
		v->vibrato_position = (uint16_t)((v->vibrato_position + ins->vibrato_speed) & 0xff);
	} else {
		v->vibrato_delay--;
	}
}

// [=]===^=[ sa_do_amf ]==========================================================================[=]
static void sa_do_amf(struct sonicarranger_state *s, struct sa_voice *v, uint16_t *period, struct sa_instrument *ins) {
	if((ins->amf_length + ins->amf_repeat) == 0) {
		return;
	}
	if(ins->amf_number >= (uint16_t)s->num_amf_tables) {
		return;
	}
	int8_t *amf_table = s->amf_tables[ins->amf_number];
	int8_t amf_val = amf_table[v->amf_position & 0x7f];

	*period = (uint16_t)((int32_t)*period - amf_val);

	v->amf_delay_counter--;
	if(v->amf_delay_counter == 0) {
		v->amf_delay_counter = ins->amf_delay;
		v->amf_position++;
		if(v->amf_position >= (ins->amf_length + ins->amf_repeat)) {
			v->amf_position = ins->amf_length;
			if(ins->amf_repeat == 0) {
				v->amf_position--;
			}
		}
	}
}

// [=]===^=[ sa_do_slide ]========================================================================[=]
static void sa_do_slide(struct sonicarranger_state *s, struct sa_voice *v, uint16_t *period) {
	int32_t p = (int32_t)*period - (int32_t)v->slide_value;
	if(p < 113) {
		p = 113;
	}
	*period = (uint16_t)p;

	if(s->speed_counter != 0) {
		v->slide_value = (int16_t)(v->slide_value + v->slide_speed);
	}
}

// [=]===^=[ sa_do_adsr ]=========================================================================[=]
static void sa_do_adsr(struct sonicarranger_state *s, struct sa_voice *v, int32_t chan, struct sa_instrument *ins) {
	if((ins->adsr_length + ins->adsr_repeat) == 0) {
		uint16_t vol = (uint16_t)((v->current_volume * s->master_volume) / 64);
		paula_set_volume(&s->paula, chan, vol);
		return;
	}
	if(ins->adsr_number >= (uint16_t)s->num_adsr_tables) {
		paula_set_volume(&s->paula, chan, 0);
		return;
	}

	uint8_t *adsr_table = s->adsr_tables[ins->adsr_number];
	uint8_t adsr_val = adsr_table[v->adsr_position & 0x7f];

	uint32_t vol = ((uint32_t)s->master_volume * (uint32_t)v->current_volume * (uint32_t)adsr_val) / 4096u;
	if(vol > 64) {
		vol = 64;
	}
	paula_set_volume(&s->paula, chan, (uint16_t)vol);

	if((v->note == 0x80) && (v->adsr_position >= ins->sustain_point)) {
		if(ins->sustain_delay == 0) {
			return;
		}
		if(v->sustain_delay_counter != 0) {
			v->sustain_delay_counter--;
			return;
		}
		v->sustain_delay_counter = ins->sustain_delay;
	}

	v->adsr_delay_counter--;
	if(v->adsr_delay_counter == 0) {
		v->adsr_delay_counter = ins->adsr_delay;
		v->adsr_position++;
		if(v->adsr_position >= (ins->adsr_length + ins->adsr_repeat)) {
			v->adsr_position = ins->adsr_length;
			if(ins->adsr_repeat == 0) {
				v->adsr_position--;
			}
			if((ins->adsr_repeat == 0) && (vol == 0)) {
				v->flag |= 0x01;
			}
		}
	}
}

// [=]===^=[ sa_do_volume_slide ]=================================================================[=]
static void sa_do_volume_slide(struct sa_voice *v) {
	int32_t vol = (int32_t)v->current_volume + (int32_t)v->volume_slide_speed;
	if(vol < 0) {
		vol = 0;
	} else if(vol > 64) {
		vol = 64;
	}
	v->current_volume = (uint16_t)vol;
}

// [=]===^=[ sa_increment_synth_effect_position ]=================================================[=]
static void sa_increment_synth_effect_position(struct sa_voice *v, struct sa_instrument *ins) {
	uint16_t start_position = ins->effect_arg2;
	uint16_t stop_position = ins->effect_arg3;

	v->synth_effect_position++;
	if(v->synth_effect_position >= stop_position) {
		v->synth_effect_position = start_position;
	}
}

// [=]===^=[ sa_syn_wave_negator ]================================================================[=]
static void sa_syn_wave_negator(struct sa_voice *v, struct sa_instrument *ins) {
	uint32_t pos = v->synth_effect_position & 0x7f;
	v->waveform_buffer[pos] = (int8_t)-v->waveform_buffer[pos];
	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_syn_free_negator ]================================================================[=]
static void sa_syn_free_negator(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	if((v->flag & 0x04) != 0) {
		return;
	}
	if(ins->effect_arg1 >= (uint16_t)s->num_waveforms || ins->waveform_number >= (uint16_t)s->num_waveforms) {
		return;
	}

	int8_t *dst = v->waveform_buffer;
	uint16_t wave_length = ins->effect_arg2;
	uint16_t wave_repeat = ins->effect_arg3;

	int8_t *waveform = s->waveform_data[ins->effect_arg1];
	int16_t wave_val = (int16_t)(waveform[v->synth_effect_wave_position & 0x7f] & 0x7f);

	int8_t *src = s->waveform_data[ins->waveform_number];
	int16_t count = (int16_t)((int32_t)ins->waveform_length * 2);
	if(count > 128) {
		count = 128;
	}

	int32_t buffer_offset = count;
	while((count > 0) && (count >= wave_val)) {
		buffer_offset--;
		dst[buffer_offset] = src[buffer_offset];
		count--;
	}

	int16_t left = (int16_t)(wave_val - count);
	count += left;
	buffer_offset += left;

	while(count > 0) {
		buffer_offset--;
		if(buffer_offset >= 0 && buffer_offset < 128) {
			dst[buffer_offset] = (int8_t)-src[buffer_offset];
		}
		count--;
	}

	v->synth_effect_wave_position++;
	if(v->synth_effect_wave_position > (wave_length + wave_repeat)) {
		v->synth_effect_wave_position = wave_length;
		if((wave_repeat == 0) && (wave_val == 0)) {
			v->flag |= 0x04;
		}
	}
}

// [=]===^=[ sa_syn_rotate_vertical ]=============================================================[=]
static void sa_syn_rotate_vertical(struct sa_voice *v, struct sa_instrument *ins) {
	int8_t *dst = v->waveform_buffer;
	int8_t delta_value = (int8_t)ins->effect_arg1;
	uint16_t start = ins->effect_arg2;
	uint16_t stop = ins->effect_arg3;
	if(start >= 128 || stop >= 128 || stop < start) {
		sa_increment_synth_effect_position(v, ins);
		return;
	}

	int16_t count = (int16_t)(stop - start);
	int32_t buf = start;
	while(count >= 0) {
		dst[buf] = (int8_t)(dst[buf] + delta_value);
		buf++;
		count--;
	}

	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_syn_rotate_horizontal ]===========================================================[=]
static void sa_syn_rotate_horizontal(struct sa_voice *v, struct sa_instrument *ins) {
	int8_t *dst = v->waveform_buffer;
	uint16_t start = ins->effect_arg2;
	uint16_t stop = ins->effect_arg3;
	if(start >= 128 || stop >= 128 || stop < start) {
		sa_increment_synth_effect_position(v, ins);
		return;
	}

	int16_t count = (int16_t)(stop - start);
	int32_t buf = start;
	int8_t first_byte = dst[buf];

	do {
		if(buf + 1 < 128) {
			dst[buf] = dst[buf + 1];
		}
		buf++;
		count--;
	} while(count >= 0);

	if(buf < 128) {
		dst[buf] = first_byte;
	}

	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_syn_alien_voice ]=================================================================[=]
static void sa_syn_alien_voice(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	if(ins->effect_arg1 >= (uint16_t)s->num_waveforms) {
		return;
	}
	int8_t *dst = v->waveform_buffer;
	uint16_t start = ins->effect_arg2;
	uint16_t stop = ins->effect_arg3;
	if(start >= 128 || stop >= 128 || stop < start) {
		return;
	}

	int8_t *src = s->waveform_data[ins->effect_arg1];
	int32_t buf = start;
	int16_t count = (int16_t)(stop - start);

	while(count >= 0) {
		dst[buf] = (int8_t)(dst[buf] + src[buf]);
		buf++;
		count--;
	}
}

// [=]===^=[ sa_syn_poly_negator ]================================================================[=]
static void sa_syn_poly_negator(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	if(ins->waveform_number >= (uint16_t)s->num_waveforms) {
		sa_increment_synth_effect_position(v, ins);
		return;
	}
	int8_t *dst = v->waveform_buffer;
	int8_t *src = s->waveform_data[ins->waveform_number];

	uint16_t pos = v->synth_effect_position;
	uint16_t start = ins->effect_arg2;
	uint16_t stop = ins->effect_arg3;

	if(pos < 128) {
		dst[pos] = src[pos];
	}

	if(pos >= stop) {
		pos = (uint16_t)(start - 1);
	}

	if((pos + 1) < 128) {
		dst[pos + 1] = (int8_t)-dst[pos + 1];
	}

	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_shack_wave_helper ]===============================================================[=]
static void sa_shack_wave_helper(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	if(ins->effect_arg1 >= (uint16_t)s->num_waveforms) {
		return;
	}
	int8_t *dst = v->waveform_buffer;
	uint16_t start = ins->effect_arg2;
	uint16_t stop = ins->effect_arg3;
	if(start >= 128 || stop >= 128 || stop < start) {
		return;
	}

	int8_t *src = s->waveform_data[ins->effect_arg1];
	uint32_t src_idx = (uint32_t)start + (uint32_t)v->synth_effect_position;
	if(src_idx >= 128) {
		src_idx &= 0x7f;
	}
	int8_t delta_value = src[src_idx];

	int32_t buf = start;
	int16_t count = (int16_t)(stop - start);
	while(count >= 0) {
		dst[buf] = (int8_t)(dst[buf] + delta_value);
		buf++;
		count--;
	}
}

// [=]===^=[ sa_syn_shack_wave_1 ]================================================================[=]
static void sa_syn_shack_wave_1(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	sa_shack_wave_helper(s, v, ins);
	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_syn_shack_wave_2 ]================================================================[=]
static void sa_syn_shack_wave_2(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	sa_shack_wave_helper(s, v, ins);

	int8_t *dst = v->waveform_buffer;
	uint16_t start = ins->effect_arg2;
	uint16_t stop = ins->effect_arg3;
	uint32_t idx = ((uint32_t)start + (uint32_t)v->synth_effect_wave_position) & 0x7f;
	dst[idx] = (int8_t)-dst[idx];

	v->synth_effect_wave_position++;
	if(stop >= start && v->synth_effect_wave_position > (stop - start)) {
		v->synth_effect_wave_position = 0;
	}

	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_metamorph_oszilator_helper ]======================================================[=]
static void sa_metamorph_oszilator_helper(struct sa_voice *v, struct sa_instrument *ins, int8_t *src) {
	int8_t *dst = v->waveform_buffer;
	uint16_t start = ins->effect_arg2;
	uint16_t stop = ins->effect_arg3;
	if(start >= 128 || stop >= 128 || stop < start) {
		return;
	}

	int32_t buf = start;
	int16_t count = (int16_t)(stop - start);
	int32_t set_flag = 0;

	while(count >= 0) {
		int8_t val = dst[buf];
		if(val != src[buf]) {
			set_flag = 1;
			if(val < src[buf]) {
				val++;
			} else {
				val--;
			}
			dst[buf] = val;
		}
		buf++;
		count--;
	}

	if(!set_flag) {
		v->flag |= 0x02;
	}
}

// [=]===^=[ sa_syn_metamorph ]===================================================================[=]
static void sa_syn_metamorph(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	if((v->flag & 0x01) != 0) {
		return;
	}
	if(ins->effect_arg1 >= (uint16_t)s->num_waveforms) {
		return;
	}
	int8_t *waveform = s->waveform_data[ins->effect_arg1];
	sa_metamorph_oszilator_helper(v, ins, waveform);
}

// [=]===^=[ sa_syn_laser ]=======================================================================[=]
static void sa_syn_laser(struct sa_voice *v, struct sa_instrument *ins) {
	int8_t detune = (int8_t)ins->effect_arg2;
	uint16_t repeats = ins->effect_arg3;

	if(v->synth_effect_wave_position < repeats) {
		v->slide_value = (int16_t)(v->slide_value + detune);
		v->synth_effect_wave_position++;
	}

	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_syn_wave_alias ]==================================================================[=]
static void sa_syn_wave_alias(struct sa_voice *v, struct sa_instrument *ins) {
	int8_t *dst = v->waveform_buffer;
	int8_t delta_value = (int8_t)ins->effect_arg1;
	uint16_t start = ins->effect_arg2;
	uint16_t stop = ins->effect_arg3;
	if(start >= 128 || stop >= 128 || stop < start) {
		sa_increment_synth_effect_position(v, ins);
		return;
	}

	int16_t count = (int16_t)(stop - start);
	int32_t buf = start;

	while(count >= 0) {
		int8_t val = dst[buf];
		if(((buf + 1) >= 128) || (val > dst[buf + 1])) {
			val = (int8_t)(val - delta_value);
		} else {
			val = (int8_t)(val + delta_value);
		}
		dst[buf++] = val;
		count--;
	}

	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_syn_noise_generator_1 ]===========================================================[=]
static void sa_syn_noise_generator_1(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	int8_t *dst = v->waveform_buffer;
	uint32_t pos = v->synth_effect_position & 0x7f;
	dst[pos] = (int8_t)((uint8_t)dst[pos] ^ (uint8_t)sa_random(s));
	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_syn_low_pass_1 ]==================================================================[=]
static void sa_syn_low_pass_1(struct sa_voice *v, struct sa_instrument *ins) {
	int8_t *dst = v->waveform_buffer;
	uint16_t delta_value = ins->effect_arg1;
	uint16_t start = ins->effect_arg2;
	uint16_t stop = ins->effect_arg3;
	if(start >= 128 || stop >= 128 || stop < start) {
		sa_increment_synth_effect_position(v, ins);
		return;
	}

	for(int32_t i = (int32_t)start; i <= (int32_t)stop; ++i) {
		int32_t flag = 0;
		int8_t val1 = dst[i];
		int8_t val2 = (i == (int32_t)stop) ? dst[start] : dst[i + 1];
		if(val1 <= val2) {
			flag = 1;
		}
		val1 = (int8_t)(val1 - val2);
		if(val1 < 0) {
			val1 = (int8_t)-val1;
		}
		if((uint16_t)(uint8_t)val1 > delta_value) {
			if(flag) {
				dst[i] = (int8_t)(dst[i] + 2);
			} else {
				dst[i] = (int8_t)(dst[i] - 2);
			}
		}
	}

	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_syn_low_pass_2 ]==================================================================[=]
static void sa_syn_low_pass_2(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	if(ins->effect_arg1 >= (uint16_t)s->num_waveforms) {
		sa_increment_synth_effect_position(v, ins);
		return;
	}
	uint16_t start = ins->effect_arg2;
	uint16_t stop = ins->effect_arg3;
	if(start >= 128 || stop >= 128 || stop < start) {
		sa_increment_synth_effect_position(v, ins);
		return;
	}

	int8_t *waveform = s->waveform_data[ins->effect_arg1];
	int8_t *dst = v->waveform_buffer;
	int32_t buffer_index = stop;

	for(int32_t i = (int32_t)start; i <= (int32_t)stop; ++i) {
		int32_t flag = 0;
		int8_t val1 = dst[i];
		int8_t val2 = (i == (int32_t)stop) ? dst[start] : dst[i + 1];
		if(val1 <= val2) {
			flag = 1;
		}
		val1 = (int8_t)(val1 - val2);
		if(val1 < 0) {
			val1 = (int8_t)-val1;
		}
		uint8_t delta_value = (uint8_t)(waveform[buffer_index & 0x7f] & 0x7f);
		buffer_index = (buffer_index + 1) & 0x7f;
		if((uint8_t)val1 > delta_value) {
			if(flag) {
				dst[i] = (int8_t)(dst[i] + 2);
			} else {
				dst[i] = (int8_t)(dst[i] - 2);
			}
		}
	}

	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_syn_oszilator ]===================================================================[=]
static void sa_syn_oszilator(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	if((v->flag & 0x02) != 0) {
		v->flag ^= 0x08;
		v->flag &= (uint8_t)~0x02;
	}

	int8_t *src;
	if((v->flag & 0x08) != 0) {
		if(ins->waveform_number >= (uint16_t)s->num_waveforms) {
			return;
		}
		src = s->waveform_data[ins->waveform_number];
	} else {
		if(ins->effect_arg1 >= (uint16_t)s->num_waveforms) {
			return;
		}
		src = s->waveform_data[ins->effect_arg1];
	}

	sa_metamorph_oszilator_helper(v, ins, src);
}

// [=]===^=[ sa_syn_noise_generator_2 ]===========================================================[=]
static void sa_syn_noise_generator_2(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	int8_t *dst = v->waveform_buffer;
	uint16_t start = ins->effect_arg2;
	uint16_t stop = ins->effect_arg3;
	if(start >= 128 || stop >= 128 || stop < start) {
		return;
	}

	int16_t count = (int16_t)(stop - start);
	int32_t buf = start;

	do {
		int32_t idx = buf + count;
		if(idx >= 0 && idx < 128) {
			int8_t val = dst[idx];
			val = (int8_t)((uint8_t)val ^ 0x05);
			val = (int8_t)(((uint8_t)val << 2) | ((uint8_t)val >> 6));
			val = (int8_t)(val + (int8_t)sa_random(s));
			dst[idx] = val;
		}
		count--;
	} while(count >= 0);
}

// [=]===^=[ sa_syn_fm_drum ]=====================================================================[=]
static void sa_syn_fm_drum(struct sa_voice *v, struct sa_instrument *ins) {
	uint8_t level = (uint8_t)ins->effect_arg1;
	uint16_t factor = ins->effect_arg2;
	uint16_t repeats = ins->effect_arg3;

	if(v->synth_effect_wave_position >= repeats) {
		v->slide_value = ins->fine_tuning;
		v->synth_effect_wave_position = 0;
	}

	uint16_t decrement = (uint16_t)((factor << 8) | level);
	v->slide_value = (int16_t)(v->slide_value - decrement);

	v->synth_effect_wave_position++;
	sa_increment_synth_effect_position(v, ins);
}

// [=]===^=[ sa_do_synth_effects ]================================================================[=]
static void sa_do_synth_effects(struct sonicarranger_state *s, struct sa_voice *v, struct sa_instrument *ins) {
	v->effect_delay_counter--;
	if(v->effect_delay_counter != 0) {
		return;
	}
	v->effect_delay_counter = ins->effect_delay;

	switch(ins->effect) {
		case SA_SYN_WAVE_NEGATOR: {
			sa_syn_wave_negator(v, ins);
			break;
		}

		case SA_SYN_FREE_NEGATOR: {
			sa_syn_free_negator(s, v, ins);
			break;
		}

		case SA_SYN_ROTATE_VERTICAL: {
			sa_syn_rotate_vertical(v, ins);
			break;
		}

		case SA_SYN_ROTATE_HORIZONTAL: {
			sa_syn_rotate_horizontal(v, ins);
			break;
		}

		case SA_SYN_ALIEN_VOICE: {
			sa_syn_alien_voice(s, v, ins);
			break;
		}

		case SA_SYN_POLY_NEGATOR: {
			sa_syn_poly_negator(s, v, ins);
			break;
		}

		case SA_SYN_SHACK_WAVE_1: {
			sa_syn_shack_wave_1(s, v, ins);
			break;
		}

		case SA_SYN_SHACK_WAVE_2: {
			sa_syn_shack_wave_2(s, v, ins);
			break;
		}

		case SA_SYN_METAMORPH: {
			sa_syn_metamorph(s, v, ins);
			break;
		}

		case SA_SYN_LASER: {
			sa_syn_laser(v, ins);
			break;
		}

		case SA_SYN_WAVE_ALIAS: {
			sa_syn_wave_alias(v, ins);
			break;
		}

		case SA_SYN_NOISE_GEN_1: {
			sa_syn_noise_generator_1(s, v, ins);
			break;
		}

		case SA_SYN_LOW_PASS_1: {
			sa_syn_low_pass_1(v, ins);
			break;
		}

		case SA_SYN_LOW_PASS_2: {
			sa_syn_low_pass_2(s, v, ins);
			break;
		}

		case SA_SYN_OSZILATOR: {
			sa_syn_oszilator(s, v, ins);
			break;
		}

		case SA_SYN_NOISE_GEN_2: {
			sa_syn_noise_generator_2(s, v, ins);
			break;
		}

		case SA_SYN_FM_DRUM: {
			sa_syn_fm_drum(v, ins);
			break;
		}
	}
}

// [=]===^=[ sa_update_voice_effect ]=============================================================[=]
static void sa_update_voice_effect(struct sonicarranger_state *s, struct sa_voice *v, int32_t chan) {
	if(((v->flag & 0x01) != 0) || (v->instrument_info == 0)) {
		paula_mute(&s->paula, chan);
		return;
	}

	struct sa_instrument *ins = v->instrument_info;

	uint16_t period = 0;
	uint16_t prev_period = 0;
	sa_do_arpeggio(s, v, ins, &period, &prev_period);
	sa_do_portamento(v, &period, prev_period);
	sa_do_vibrato(v, &period, ins);
	sa_do_amf(s, v, &period, ins);
	sa_do_slide(s, v, &period);

	paula_set_period(&s->paula, chan, period);

	if(ins->type == SA_INSTR_SYNTH) {
		sa_do_synth_effects(s, v, ins);
	}

	sa_do_adsr(s, v, chan, ins);
	sa_do_volume_slide(v);
}

// [=]===^=[ sa_update_effects ]==================================================================[=]
static void sa_update_effects(struct sonicarranger_state *s) {
	for(int32_t i = 0; i < SA_NUM_CHANNELS; ++i) {
		sa_update_voice_effect(s, &s->voices[i], i);
	}
}

// [=]===^=[ sa_tick ]============================================================================[=]
static void sa_tick(struct sonicarranger_state *s) {
	s->speed_counter++;
	if(s->speed_counter >= s->current_speed) {
		s->speed_counter = 0;
		sa_get_next_row(s);
	}
	sa_update_effects(s);
}

// [=]===^=[ sonicarranger_init ]=================================================================[=]
static struct sonicarranger_state *sonicarranger_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 16 || sample_rate < 8000) {
		return 0;
	}

	uint8_t *eff_data = (uint8_t *)data;
	uint32_t eff_len = len;
	uint8_t *converted = 0;

	if(!sa_identify(eff_data, eff_len)) {
		// Try the m68k-embedded "Sonic Arranger Final" format and convert
		// it on the fly to SOARV1.0.
		uint32_t mod_off = sa_test_final(eff_data, eff_len);
		if(mod_off == 0) {
			return 0;
		}
		uint32_t conv_len = 0;
		converted = sa_convert_final(eff_data, eff_len, mod_off, &conv_len);
		if(!converted) {
			return 0;
		}
		eff_data = converted;
		eff_len = conv_len;
	}

	struct sonicarranger_state *s = (struct sonicarranger_state *)calloc(1, sizeof(struct sonicarranger_state));
	if(!s) {
		if(converted) {
			free(converted);
		}
		return 0;
	}
	s->random_state = 0x1234abcd;
	s->owned_data = converted;        // freed by sa_cleanup; null for SOARV1.0 input

	if(!sa_load(s, eff_data, eff_len)) {
		sa_cleanup(s);
		free(s);
		return 0;
	}

	if(s->num_sub_songs == 0 || s->num_positions == 0) {
		sa_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, SA_TICK_HZ);
	sa_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ sonicarranger_free ]=================================================================[=]
static void sonicarranger_free(struct sonicarranger_state *s) {
	if(!s) {
		return;
	}
	sa_cleanup(s);
	free(s);
}

// [=]===^=[ sonicarranger_get_audio ]============================================================[=]
static void sonicarranger_get_audio(struct sonicarranger_state *s, int16_t *output, int32_t frames) {
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
			sa_tick(s);
		}
	}
}

// [=]===^=[ sonicarranger_api_init ]=============================================================[=]
static void *sonicarranger_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return sonicarranger_init(data, len, sample_rate);
}

// [=]===^=[ sonicarranger_api_free ]=============================================================[=]
static void sonicarranger_api_free(void *state) {
	sonicarranger_free((struct sonicarranger_state *)state);
}

// [=]===^=[ sonicarranger_api_get_audio ]========================================================[=]
static void sonicarranger_api_get_audio(void *state, int16_t *output, int32_t frames) {
	sonicarranger_get_audio((struct sonicarranger_state *)state, output, frames);
}

static const char *sonicarranger_extensions[] = { "sa", "sonic", 0 };

static struct player_api sonicarranger_api = {
	"Sonic Arranger",
	sonicarranger_extensions,
	sonicarranger_api_init,
	sonicarranger_api_free,
	sonicarranger_api_get_audio,
	0,
};
