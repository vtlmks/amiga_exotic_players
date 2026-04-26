// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// InStereo 2.0 replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate (PAL).
//
// Public API:
//   struct instereo20_state *instereo20_init(void *data, uint32_t len, int32_t sample_rate);
//   void instereo20_free(struct instereo20_state *s);
//   void instereo20_get_audio(struct instereo20_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define IS20_TICK_HZ        50
#define IS20_NUM_CHANNELS   4

// Effect codes (matching NostalgicPlayer Effect enum)
#define IS20_EFF_ARPEGGIO            0x0
#define IS20_EFF_SET_SLIDE_SPEED     0x1
#define IS20_EFF_RESTART_ADSR        0x2
#define IS20_EFF_SET_VIBRATO         0x4
#define IS20_EFF_SET_PORTAMENTO      0x7
#define IS20_EFF_SKIP_PORTAMENTO     0x8
#define IS20_EFF_SET_TRACK_LEN       0x9
#define IS20_EFF_SET_VOLUME_INCR     0xa
#define IS20_EFF_POSITION_JUMP       0xb
#define IS20_EFF_SET_VOLUME          0xc
#define IS20_EFF_TRACK_BREAK         0xd
#define IS20_EFF_SET_FILTER          0xe
#define IS20_EFF_SET_SPEED           0xf

// Envelope generator modes
#define IS20_EG_DISABLED 0
#define IS20_EG_CALC     1
#define IS20_EG_FREE     2

// Voice playing modes
#define IS20_PM_SAMPLE 0
#define IS20_PM_SYNTH  1

struct is20_song_info {
	uint8_t start_speed;
	uint8_t rows_per_track;
	uint16_t first_position;
	uint16_t last_position;
	uint16_t restart_position;
	uint16_t tempo;
};

struct is20_single_position_info {
	uint16_t start_track_row;
	int8_t sound_transpose;
	int8_t note_transpose;
};

struct is20_track_line {
	uint8_t note;
	uint8_t instrument;
	uint8_t disable_sound_transpose;
	uint8_t disable_note_transpose;
	uint8_t arpeggio;
	uint8_t effect;
	uint8_t effect_arg;
};

struct is20_sample {
	uint16_t one_shot_length;       // in words
	uint16_t repeat_length;         // in words
	int8_t sample_number;
	uint8_t volume;
	uint8_t vibrato_delay;
	uint8_t vibrato_speed;
	uint8_t vibrato_level;
	uint8_t portamento_speed;
};

struct is20_arpeggio {
	uint8_t length;
	uint8_t repeat;
	int8_t values[14];
};

struct is20_instrument {
	uint16_t waveform_length;
	uint8_t volume;
	uint8_t vibrato_delay;
	uint8_t vibrato_speed;
	uint8_t vibrato_level;
	uint8_t portamento_speed;
	uint8_t adsr_length;
	uint8_t adsr_repeat;
	uint8_t sustain_point;
	uint8_t sustain_speed;
	uint8_t amf_length;
	uint8_t amf_repeat;
	uint8_t envelope_generator_mode;
	uint8_t start_len;
	uint8_t stop_rep;
	uint8_t speed_up;
	uint8_t speed_down;
	uint8_t adsr_table[128];
	int8_t lfo_table[128];
	struct is20_arpeggio arpeggios[3];
	uint8_t envelope_generator_table[128];
	int8_t waveform1[256];
	int8_t waveform2[256];
};

struct is20_voice {
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

	uint8_t transposed_note;
	uint8_t previous_transposed_note;

	uint8_t transposed_instrument;
	uint8_t playing_mode;

	uint8_t current_volume;

	uint16_t arpeggio_position;
	uint8_t arpeggio_effect_nibble;

	int8_t slide_speed;
	int16_t slide_value;

	uint16_t portamento_speed_counter;
	uint16_t portamento_speed;

	uint8_t vibrato_delay;
	uint8_t vibrato_speed;
	uint8_t vibrato_level;
	uint16_t vibrato_position;

	uint16_t adsr_position;
	uint16_t sustain_counter;

	int8_t envelope_generator_duration;
	uint16_t envelope_generator_position;

	uint16_t lfo_position;
};

struct is20_global_info {
	uint8_t speed_counter;
	uint8_t current_speed;
	int16_t song_position;
	uint8_t row_position;
	uint8_t rows_per_track;
};

struct instereo20_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	struct is20_song_info *sub_songs;
	uint32_t num_sub_songs;

	struct is20_single_position_info *positions; // num_positions * 4
	uint32_t num_positions;

	struct is20_track_line *track_lines;
	uint32_t num_track_lines;

	struct is20_sample *samples;
	uint32_t num_samples;

	int8_t **sample_data;
	uint32_t *sample_lengths;

	struct is20_instrument *instruments;
	uint32_t num_instruments;

	struct is20_song_info *current_song_info;
	struct is20_global_info playing_info;
	struct is20_voice voices[IS20_NUM_CHANNELS];
};

// [=]===^=[ is20_periods ]=======================================================================[=]
static uint16_t is20_periods[] = {
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

#define IS20_PERIODS_COUNT (sizeof(is20_periods) / sizeof(is20_periods[0]))

// [=]===^=[ is20_vibrato ]=======================================================================[=]
static int8_t is20_vibrato[256] = {
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

// [=]===^=[ is20_read_u16_be ]===================================================================[=]
static uint16_t is20_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ is20_read_u32_be ]===================================================================[=]
static uint32_t is20_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ is20_check_mark ]====================================================================[=]
static int32_t is20_check_mark(uint8_t *p, uint32_t len, uint32_t off, const char *mark) {
	if(off + 4 > len) {
		return 0;
	}
	for(int32_t i = 0; i < 4; ++i) {
		if(p[off + i] != (uint8_t)mark[i]) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ is20_identify ]======================================================================[=]
static int32_t is20_identify(uint8_t *data, uint32_t len) {
	if(len < 16) {
		return 0;
	}
	uint8_t want[8] = { 'I','S','2','0','D','F','1','0' };
	for(int32_t i = 0; i < 8; ++i) {
		if(data[i] != want[i]) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ is20_cleanup ]=======================================================================[=]
static void is20_cleanup(struct instereo20_state *s) {
	if(!s) {
		return;
	}
	free(s->sub_songs); s->sub_songs = 0;
	free(s->positions); s->positions = 0;
	free(s->track_lines); s->track_lines = 0;
	free(s->samples); s->samples = 0;
	if(s->sample_data) {
		free(s->sample_data);
		s->sample_data = 0;
	}
	free(s->sample_lengths); s->sample_lengths = 0;
	free(s->instruments); s->instruments = 0;
}

// [=]===^=[ is20_load ]==========================================================================[=]
static int32_t is20_load(struct instereo20_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = 8;

	// --- STBL: sub-songs ---
	if(!is20_check_mark(data, len, pos, "STBL")) {
		return 0;
	}
	pos += 4;
	if(pos + 4 > len) {
		return 0;
	}
	s->num_sub_songs = is20_read_u32_be(data + pos);
	pos += 4;
	if(s->num_sub_songs == 0 || s->num_sub_songs > 0x10000) {
		return 0;
	}
	s->sub_songs = (struct is20_song_info *)calloc(s->num_sub_songs, sizeof(struct is20_song_info));
	if(!s->sub_songs) {
		return 0;
	}
	for(uint32_t i = 0; i < s->num_sub_songs; ++i) {
		if(pos + 10 > len) {
			return 0;
		}
		struct is20_song_info *si = &s->sub_songs[i];
		si->start_speed       = data[pos++];
		si->rows_per_track    = data[pos++];
		si->first_position    = is20_read_u16_be(data + pos); pos += 2;
		si->last_position     = is20_read_u16_be(data + pos); pos += 2;
		si->restart_position  = is20_read_u16_be(data + pos); pos += 2;
		si->tempo             = is20_read_u16_be(data + pos); pos += 2;
	}

	// --- OVTB: position information ---
	if(!is20_check_mark(data, len, pos, "OVTB")) {
		return 0;
	}
	pos += 4;
	if(pos + 4 > len) {
		return 0;
	}
	s->num_positions = is20_read_u32_be(data + pos);
	pos += 4;
	if(s->num_positions == 0 || s->num_positions > 0x100000) {
		return 0;
	}
	s->positions = (struct is20_single_position_info *)calloc(s->num_positions * 4, sizeof(struct is20_single_position_info));
	if(!s->positions) {
		return 0;
	}
	for(uint32_t i = 0; i < s->num_positions; ++i) {
		for(int32_t j = 0; j < 4; ++j) {
			if(pos + 4 > len) {
				return 0;
			}
			struct is20_single_position_info *sp = &s->positions[i * 4 + j];
			sp->start_track_row = is20_read_u16_be(data + pos); pos += 2;
			sp->sound_transpose = (int8_t)data[pos++];
			sp->note_transpose  = (int8_t)data[pos++];
		}
	}

	// --- NTBL: track rows ---
	if(!is20_check_mark(data, len, pos, "NTBL")) {
		return 0;
	}
	pos += 4;
	if(pos + 4 > len) {
		return 0;
	}
	s->num_track_lines = is20_read_u32_be(data + pos);
	pos += 4;
	if(s->num_track_lines > 0x1000000) {
		return 0;
	}
	if(s->num_track_lines > 0) {
		s->track_lines = (struct is20_track_line *)calloc(s->num_track_lines, sizeof(struct is20_track_line));
		if(!s->track_lines) {
			return 0;
		}
		for(uint32_t i = 0; i < s->num_track_lines; ++i) {
			if(pos + 4 > len) {
				return 0;
			}
			struct is20_track_line *tl = &s->track_lines[i];
			uint8_t b1 = data[pos++];
			uint8_t b2 = data[pos++];
			uint8_t b3 = data[pos++];
			uint8_t b4 = data[pos++];
			tl->note = b1;
			tl->instrument = b2;
			tl->disable_sound_transpose = (b3 & 0x80) != 0 ? 1 : 0;
			tl->disable_note_transpose  = (b3 & 0x40) != 0 ? 1 : 0;
			tl->arpeggio = (uint8_t)((b3 & 0x30) >> 4);
			tl->effect = (uint8_t)(b3 & 0x0f);
			tl->effect_arg = b4;
		}
	}

	// --- SAMP: sample information ---
	if(!is20_check_mark(data, len, pos, "SAMP")) {
		return 0;
	}
	pos += 4;
	if(pos + 4 > len) {
		return 0;
	}
	s->num_samples = is20_read_u32_be(data + pos);
	pos += 4;
	if(s->num_samples > 0x10000) {
		return 0;
	}
	if(s->num_samples > 0) {
		s->samples = (struct is20_sample *)calloc(s->num_samples, sizeof(struct is20_sample));
		if(!s->samples) {
			return 0;
		}
		for(uint32_t i = 0; i < s->num_samples; ++i) {
			if(pos + 16 > len) {
				return 0;
			}
			struct is20_sample *smp = &s->samples[i];
			smp->one_shot_length   = is20_read_u16_be(data + pos); pos += 2;
			smp->repeat_length     = is20_read_u16_be(data + pos); pos += 2;
			smp->sample_number     = (int8_t)data[pos++];
			smp->volume            = data[pos++];
			smp->vibrato_delay     = data[pos++];
			smp->vibrato_speed     = data[pos++];
			smp->vibrato_level     = data[pos++];
			smp->portamento_speed  = data[pos++];
			pos += 6; // reserved
		}
		// Skip names: 20 bytes each
		if(pos + s->num_samples * 20 > len) {
			return 0;
		}
		pos += s->num_samples * 20;
		// Skip extra length tables: 4 words per sample
		if(pos + s->num_samples * 4 * 2 > len) {
			return 0;
		}
		pos += s->num_samples * 4 * 2;

		// --- sample data: lengths array then reverse-order data ---
		s->sample_lengths = (uint32_t *)calloc(s->num_samples, sizeof(uint32_t));
		s->sample_data    = (int8_t **)calloc(s->num_samples, sizeof(int8_t *));
		if(!s->sample_lengths || !s->sample_data) {
			return 0;
		}
		if(pos + s->num_samples * 4 > len) {
			return 0;
		}
		for(uint32_t i = 0; i < s->num_samples; ++i) {
			s->sample_lengths[i] = is20_read_u32_be(data + pos); pos += 4;
		}
		// Sample data follows in reverse order: index num-1 first, ..., index 0 last.
		uint32_t total = 0;
		for(uint32_t i = 0; i < s->num_samples; ++i) {
			total += s->sample_lengths[i];
		}
		if(pos + total > len) {
			return 0;
		}
		uint32_t cur = pos;
		for(int32_t i = (int32_t)s->num_samples - 1; i >= 0; --i) {
			uint32_t slen = s->sample_lengths[i];
			if(slen > 0) {
				s->sample_data[i] = (int8_t *)(data + cur);
				cur += slen;
			} else {
				s->sample_data[i] = 0;
			}
		}
		pos = cur;
	}

	// --- SYNT: synth instruments ---
	if(!is20_check_mark(data, len, pos, "SYNT")) {
		return 0;
	}
	pos += 4;
	if(pos + 4 > len) {
		return 0;
	}
	s->num_instruments = is20_read_u32_be(data + pos);
	pos += 4;
	if(s->num_instruments > 0x10000) {
		return 0;
	}
	if(s->num_instruments > 0) {
		s->instruments = (struct is20_instrument *)calloc(s->num_instruments, sizeof(struct is20_instrument));
		if(!s->instruments) {
			return 0;
		}
		for(uint32_t i = 0; i < s->num_instruments; ++i) {
			if(!is20_check_mark(data, len, pos, "IS20")) {
				return 0;
			}
			pos += 4;
			if(pos + 20 > len) {
				return 0;
			}
			pos += 20; // name
			struct is20_instrument *instr = &s->instruments[i];
			if(pos + 9 > len) {
				return 0;
			}
			instr->waveform_length    = is20_read_u16_be(data + pos); pos += 2;
			instr->volume             = data[pos++];
			instr->vibrato_delay      = data[pos++];
			instr->vibrato_speed      = data[pos++];
			instr->vibrato_level      = data[pos++];
			instr->portamento_speed   = data[pos++];
			instr->adsr_length        = data[pos++];
			instr->adsr_repeat        = data[pos++];
			if(pos + 4 > len) {
				return 0;
			}
			pos += 4;
			if(pos + 6 > len) {
				return 0;
			}
			instr->sustain_point = data[pos++];
			instr->sustain_speed = data[pos++];
			instr->amf_length    = data[pos++];
			instr->amf_repeat    = data[pos++];
			uint8_t eg_mode    = data[pos++];
			uint8_t eg_enabled = data[pos++];
			instr->envelope_generator_mode = (eg_enabled == 0) ? IS20_EG_DISABLED : (eg_mode == 0 ? IS20_EG_CALC : IS20_EG_FREE);

			if(pos + 4 > len) {
				return 0;
			}
			instr->start_len  = data[pos++];
			instr->stop_rep   = data[pos++];
			instr->speed_up   = data[pos++];
			instr->speed_down = data[pos++];

			if(pos + 19 > len) {
				return 0;
			}
			pos += 19;

			if(pos + 128 > len) {
				return 0;
			}
			memcpy(instr->adsr_table, data + pos, 128); pos += 128;

			if(pos + 128 > len) {
				return 0;
			}
			memcpy(instr->lfo_table, data + pos, 128); pos += 128;

			for(int32_t j = 0; j < 3; ++j) {
				if(pos + 2 + 14 > len) {
					return 0;
				}
				instr->arpeggios[j].length = data[pos++];
				instr->arpeggios[j].repeat = data[pos++];
				memcpy(instr->arpeggios[j].values, data + pos, 14); pos += 14;
			}

			if(pos + 128 > len) {
				return 0;
			}
			memcpy(instr->envelope_generator_table, data + pos, 128); pos += 128;

			if(pos + 256 > len) {
				return 0;
			}
			memcpy(instr->waveform1, data + pos, 256); pos += 256;

			if(pos + 256 > len) {
				return 0;
			}
			memcpy(instr->waveform2, data + pos, 256); pos += 256;
		}
	}

	return 1;
}

// [=]===^=[ is20_initialize_sound ]==============================================================[=]
static void is20_initialize_sound(struct instereo20_state *s, uint32_t sub_song) {
	if(sub_song >= s->num_sub_songs) {
		sub_song = 0;
	}
	s->current_song_info = &s->sub_songs[sub_song];

	memset(&s->playing_info, 0, sizeof(s->playing_info));
	s->playing_info.speed_counter  = s->current_song_info->start_speed;
	s->playing_info.current_speed  = s->current_song_info->start_speed;
	s->playing_info.song_position  = (int16_t)((int32_t)s->current_song_info->first_position - 1);
	s->playing_info.row_position   = s->current_song_info->rows_per_track;
	s->playing_info.rows_per_track = s->current_song_info->rows_per_track;

	for(int32_t i = 0; i < IS20_NUM_CHANNELS; ++i) {
		memset(&s->voices[i], 0, sizeof(s->voices[i]));
		s->voices[i].playing_mode = IS20_PM_SAMPLE;
		s->voices[i].effect = IS20_EFF_ARPEGGIO;
	}
}

// [=]===^=[ is20_force_quiet ]===================================================================[=]
static void is20_force_quiet(struct instereo20_state *s, struct is20_voice *v, int32_t ch) {
	paula_mute(&s->paula, ch);
	v->current_volume = 0;
	v->transposed_instrument = 0;
	v->playing_mode = IS20_PM_SAMPLE;
}

// [=]===^=[ is20_set_transpose ]=================================================================[=]
static void is20_set_transpose(struct is20_voice *v, uint8_t *note, uint8_t *instr_num) {
	if(!v->disable_note_transpose || v->disable_sound_transpose) {
		*note = (uint8_t)(*note + (uint8_t)v->note_transpose);
	}
	if(!v->disable_sound_transpose || v->disable_note_transpose) {
		*instr_num = (uint8_t)(*instr_num + (uint8_t)v->sound_transpose);
	}
}

// [=]===^=[ is20_reset_voice ]===================================================================[=]
static void is20_reset_voice(struct is20_voice *v) {
	v->transposed_instrument = 0;
	v->arpeggio_position = 0;
	v->slide_speed = 0;
	v->slide_value = 0;
	v->vibrato_position = 0;
	v->adsr_position = 0;
	v->sustain_counter = 0;
	v->envelope_generator_duration = 0;
	v->envelope_generator_position = 0;
	v->lfo_position = 0;

	if(v->effect != IS20_EFF_SET_PORTAMENTO) {
		v->portamento_speed_counter = 0;
		v->portamento_speed = 0;
	}
	if(v->effect != IS20_EFF_SET_VIBRATO) {
		v->vibrato_delay = 0;
		v->vibrato_speed = 0;
		v->vibrato_level = 0;
	}
}

// [=]===^=[ is20_restore_voice ]=================================================================[=]
static void is20_restore_voice(struct instereo20_state *s, struct is20_voice *v, uint8_t instr_num) {
	instr_num &= 0x3f;
	v->transposed_instrument = instr_num;
	if(instr_num == 0) {
		return;
	}

	if(v->playing_mode == IS20_PM_SAMPLE) {
		if((uint32_t)(instr_num - 1) >= s->num_samples) {
			return;
		}
		struct is20_sample *smp = &s->samples[instr_num - 1];
		if(v->effect != IS20_EFF_SET_VIBRATO) {
			v->vibrato_delay = smp->vibrato_delay;
			v->vibrato_speed = smp->vibrato_speed;
			v->vibrato_level = smp->vibrato_level;
		}
		if((v->effect != IS20_EFF_SKIP_PORTAMENTO) && (v->effect != IS20_EFF_SET_PORTAMENTO)) {
			v->portamento_speed_counter = smp->portamento_speed;
			v->portamento_speed = smp->portamento_speed;
		}
		if((v->effect != IS20_EFF_SET_VOLUME) && (v->effect != IS20_EFF_SET_VOLUME_INCR)) {
			v->current_volume = smp->volume;
		}
	} else {
		if((uint32_t)(instr_num - 1) >= s->num_instruments) {
			return;
		}
		struct is20_instrument *instr = &s->instruments[instr_num - 1];
		if(v->effect != IS20_EFF_SET_VIBRATO) {
			v->vibrato_delay = instr->vibrato_delay;
			v->vibrato_speed = instr->vibrato_speed;
			v->vibrato_level = instr->vibrato_level;
		}
		if((v->effect != IS20_EFF_SKIP_PORTAMENTO) && (v->effect != IS20_EFF_SET_PORTAMENTO)) {
			v->portamento_speed_counter = instr->portamento_speed;
			v->portamento_speed = instr->portamento_speed;
		}
		if((v->effect != IS20_EFF_SET_VOLUME) && (v->effect != IS20_EFF_SET_VOLUME_INCR)) {
			v->current_volume = instr->volume;
		}
	}
}

// [=]===^=[ is20_set_sample_instrument ]=========================================================[=]
static void is20_set_sample_instrument(struct instereo20_state *s, struct is20_voice *v, int32_t ch, uint8_t instr_num) {
	v->playing_mode = IS20_PM_SAMPLE;
	instr_num &= 0x3f;
	v->transposed_instrument = instr_num;

	if(instr_num == 0 || (uint32_t)(instr_num - 1) >= s->num_samples) {
		is20_force_quiet(s, v, ch);
		return;
	}

	struct is20_sample *smp = &s->samples[instr_num - 1];

	if(v->effect != IS20_EFF_SET_VIBRATO) {
		v->vibrato_delay = smp->vibrato_delay;
		v->vibrato_speed = smp->vibrato_speed;
		v->vibrato_level = smp->vibrato_level;
	}
	if((v->effect != IS20_EFF_SKIP_PORTAMENTO) && (v->effect != IS20_EFF_SET_PORTAMENTO)) {
		v->portamento_speed_counter = smp->portamento_speed;
		v->portamento_speed = smp->portamento_speed;
	}

	if(smp->sample_number < 0 || (uint32_t)smp->sample_number >= s->num_samples || s->sample_data[smp->sample_number] == 0) {
		is20_force_quiet(s, v, ch);
		return;
	}

	int8_t *sdat = s->sample_data[smp->sample_number];
	uint32_t play_length = smp->one_shot_length;
	uint32_t loop_start = 0;
	uint32_t loop_length = 0;

	if(smp->repeat_length == 0) {
		loop_length = smp->one_shot_length;
	} else if(smp->repeat_length != 1) {
		play_length += smp->repeat_length;
		loop_start = smp->one_shot_length;
		loop_length = smp->repeat_length;
	}

	paula_play_sample(&s->paula, ch, sdat, play_length * 2u);
	if(loop_length != 0) {
		paula_set_loop(&s->paula, ch, loop_start * 2u, loop_length * 2u);
	}

	if((v->effect != IS20_EFF_SET_VOLUME) && (v->effect != IS20_EFF_SET_VOLUME_INCR)) {
		v->current_volume = smp->volume;
		paula_set_volume(&s->paula, ch, smp->volume);
	}
}

// [=]===^=[ is20_set_synth_instrument ]==========================================================[=]
static void is20_set_synth_instrument(struct instereo20_state *s, struct is20_voice *v, int32_t ch, uint8_t instr_num) {
	v->playing_mode = IS20_PM_SYNTH;
	instr_num &= 0x3f;
	v->transposed_instrument = instr_num;

	if(instr_num == 0 || (uint32_t)(instr_num - 1) >= s->num_instruments) {
		is20_force_quiet(s, v, ch);
		return;
	}

	struct is20_instrument *instr = &s->instruments[instr_num - 1];

	if(v->effect != IS20_EFF_SET_VIBRATO) {
		v->vibrato_delay = instr->vibrato_delay;
		v->vibrato_speed = instr->vibrato_speed;
		v->vibrato_level = instr->vibrato_level;
	}
	if((v->effect != IS20_EFF_SKIP_PORTAMENTO) && (v->effect != IS20_EFF_SET_PORTAMENTO)) {
		v->portamento_speed_counter = instr->portamento_speed;
		v->portamento_speed = instr->portamento_speed;
	}

	uint8_t eg_val = 0;
	uint8_t eg_mode = instr->envelope_generator_mode;

	if(eg_mode == IS20_EG_FREE) {
		v->envelope_generator_position = 0;
		if((uint8_t)(instr->start_len + instr->stop_rep) == 0) {
			eg_mode = IS20_EG_DISABLED;
		} else {
			eg_val = instr->envelope_generator_table[0];
		}
	}
	if(eg_mode == IS20_EG_CALC) {
		v->envelope_generator_position = (uint16_t)((uint16_t)instr->start_len << 8);
		v->envelope_generator_duration = 1;
		eg_val = instr->start_len;
	}
	if(eg_mode == IS20_EG_DISABLED) {
		v->envelope_generator_duration = 0;
		eg_val = 0;
	}

	int8_t *waveform = ((eg_val & 1) != 0) ? instr->waveform2 : instr->waveform1;
	uint32_t length = instr->waveform_length;
	uint32_t start_offset = (uint32_t)eg_val & 0xfe;

	paula_queue_sample(&s->paula, ch, waveform, start_offset, length);
	paula_set_loop(&s->paula, ch, start_offset, length);

	if((v->effect != IS20_EFF_SET_VOLUME) && (v->effect != IS20_EFF_SET_VOLUME_INCR)) {
		v->current_volume = instr->volume;
	}
}

// [=]===^=[ is20_set_effects ]===================================================================[=]
static void is20_set_effects(struct instereo20_state *s, struct is20_voice *v) {
	switch(v->effect) {
		case IS20_EFF_SET_SLIDE_SPEED: {
			v->slide_speed = (int8_t)v->effect_arg;
			break;
		}
		case IS20_EFF_RESTART_ADSR: {
			v->adsr_position = v->effect_arg;
			break;
		}
		case IS20_EFF_SET_PORTAMENTO: {
			v->portamento_speed_counter = v->effect_arg;
			v->portamento_speed = v->effect_arg;
			break;
		}
		case IS20_EFF_SET_VOLUME_INCR: {
			int32_t new_vol = (int32_t)v->current_volume + (int32_t)(int8_t)v->effect_arg;
			if(new_vol < 0) {
				new_vol = 0;
			} else {
				if(v->playing_mode == IS20_PM_SAMPLE) {
					if(new_vol > 64) {
						new_vol = 64;
					}
				} else {
					if(new_vol > 255) {
						new_vol = 255;
					}
				}
			}
			v->current_volume = (uint8_t)new_vol;
			break;
		}
		case IS20_EFF_POSITION_JUMP: {
			s->playing_info.song_position = v->effect_arg;
			s->playing_info.row_position  = s->playing_info.rows_per_track;
			break;
		}
		case IS20_EFF_TRACK_BREAK: {
			s->playing_info.row_position = s->playing_info.rows_per_track;
			break;
		}
		case IS20_EFF_SET_VOLUME: {
			uint8_t new_vol = v->effect_arg;
			if((v->playing_mode == IS20_PM_SAMPLE) && (new_vol > 64)) {
				new_vol = 64;
			}
			v->current_volume = new_vol;
			break;
		}
		case IS20_EFF_SET_TRACK_LEN: {
			if(v->effect_arg <= 64) {
				s->playing_info.rows_per_track = v->effect_arg;
			}
			break;
		}
		case IS20_EFF_SET_FILTER: {
			break; // Amiga LED filter, not modeled in paula.h
		}
		case IS20_EFF_SET_SPEED: {
			if((v->effect_arg > 0) && (v->effect_arg <= 16)) {
				s->playing_info.current_speed = v->effect_arg;
			}
			break;
		}
		case IS20_EFF_SET_VIBRATO: {
			v->vibrato_delay = 0;
			v->vibrato_speed = (uint8_t)(((v->effect_arg >> 4) & 0x0f) * 2);
			v->vibrato_level = (uint8_t)((-((v->effect_arg & 0x0f) << 4)) + 160);
			break;
		}
		default: break;
	}
}

// [=]===^=[ is20_play_voice ]====================================================================[=]
static void is20_play_voice(struct instereo20_state *s, struct is20_voice *v, int32_t ch) {
	is20_set_effects(s, v);

	uint8_t note = v->note;
	uint8_t instr_num = v->instrument;

	if(note == 0) {
		if(instr_num != 0) {
			is20_restore_voice(s, v, instr_num);
		}
		return;
	}

	if(note == 0x80) {
		return;
	}

	if(note == 0x7f) {
		is20_force_quiet(s, v, ch);
		return;
	}

	is20_set_transpose(v, &note, &instr_num);

	v->previous_transposed_note = v->transposed_note;
	v->transposed_note = note;

	if(instr_num <= 127) {
		is20_reset_voice(v);
		if(instr_num >= 64) {
			is20_set_sample_instrument(s, v, ch, instr_num);
		} else {
			is20_set_synth_instrument(s, v, ch, instr_num);
		}
	}
}

// [=]===^=[ is20_get_next_position ]=============================================================[=]
static void is20_get_next_position(struct instereo20_state *s) {
	s->playing_info.song_position++;
	if(s->playing_info.song_position > (int16_t)s->current_song_info->last_position) {
		s->playing_info.song_position = (int16_t)s->current_song_info->restart_position;
	}
	if((uint32_t)s->playing_info.song_position >= s->num_positions) {
		s->playing_info.song_position = 0;
	}

	struct is20_single_position_info *row = &s->positions[(uint32_t)s->playing_info.song_position * 4];
	for(int32_t i = 0; i < IS20_NUM_CHANNELS; ++i) {
		s->voices[i].start_track_row = row[i].start_track_row;
		s->voices[i].sound_transpose = row[i].sound_transpose;
		s->voices[i].note_transpose  = row[i].note_transpose;
	}
}

// [=]===^=[ is20_get_next_row ]==================================================================[=]
static void is20_get_next_row(struct instereo20_state *s) {
	s->playing_info.row_position++;
	if(s->playing_info.row_position >= s->playing_info.rows_per_track) {
		s->playing_info.row_position = 0;
		is20_get_next_position(s);
	}

	// Read new notes
	for(int32_t i = 0; i < IS20_NUM_CHANNELS; ++i) {
		struct is20_voice *v = &s->voices[i];
		uint32_t position = (uint32_t)v->start_track_row + (uint32_t)s->playing_info.row_position;
		struct is20_track_line tl;
		if(position < s->num_track_lines) {
			tl = s->track_lines[position];
		} else {
			memset(&tl, 0, sizeof(tl));
		}
		v->note = tl.note;
		v->instrument = tl.instrument;
		v->disable_sound_transpose = tl.disable_sound_transpose;
		v->disable_note_transpose  = tl.disable_note_transpose;
		v->arpeggio = tl.arpeggio;
		v->effect = tl.effect;
		v->effect_arg = tl.effect_arg;
	}

	// Check for new instruments
	for(int32_t i = 0; i < IS20_NUM_CHANNELS; ++i) {
		struct is20_voice *v = &s->voices[i];
		if((v->note != 0) && (v->instrument != 0)) {
			v->transposed_instrument = 0;
		}
	}

	// Update voices
	for(int32_t i = 0; i < IS20_NUM_CHANNELS; ++i) {
		is20_play_voice(s, &s->voices[i], i);
	}
}

// [=]===^=[ is20_do_sample_arpeggio ]============================================================[=]
static void is20_do_sample_arpeggio(struct is20_voice *v, uint16_t *period, uint16_t *previous_period) {
	uint8_t note = v->transposed_note;
	uint8_t prev_note = v->previous_transposed_note;

	if(v->effect == IS20_EFF_ARPEGGIO) {
		uint8_t arp_val = v->arpeggio_effect_nibble ? (uint8_t)(v->effect_arg & 0x0f) : (uint8_t)(v->effect_arg >> 4);
		v->arpeggio_effect_nibble = !v->arpeggio_effect_nibble;
		note = (uint8_t)(note + arp_val);
		prev_note = (uint8_t)(prev_note + arp_val);
	}

	*period          = (note < IS20_PERIODS_COUNT) ? is20_periods[note] : 0;
	*previous_period = (prev_note < IS20_PERIODS_COUNT) ? is20_periods[prev_note] : 0;
}

// [=]===^=[ is20_do_synth_arpeggio ]=============================================================[=]
static void is20_do_synth_arpeggio(struct is20_voice *v, struct is20_instrument *instr, uint16_t *period, uint16_t *previous_period) {
	uint8_t note = v->transposed_note;
	uint8_t prev_note = v->previous_transposed_note;

	if(v->arpeggio != 0) {
		struct is20_arpeggio *arp = &instr->arpeggios[v->arpeggio - 1];
		int8_t arp_val = arp->values[v->arpeggio_position % 14];
		note = (uint8_t)(note + (uint8_t)arp_val);
		prev_note = (uint8_t)(prev_note + (uint8_t)arp_val);

		if(v->arpeggio_position == (uint16_t)(arp->length + arp->repeat)) {
			v->arpeggio_position = arp->length;
		} else {
			v->arpeggio_position++;
		}
	}

	*period          = (note < IS20_PERIODS_COUNT) ? is20_periods[note] : 0;
	*previous_period = (prev_note < IS20_PERIODS_COUNT) ? is20_periods[prev_note] : 0;
}

// [=]===^=[ is20_do_portamento ]=================================================================[=]
static void is20_do_portamento(uint16_t *period, uint16_t *previous_period, struct is20_voice *v) {
	if((v->portamento_speed != 0) && (v->portamento_speed_counter != 0) && (*period != *previous_period)) {
		v->portamento_speed_counter--;

		uint16_t a = *period;
		uint16_t b = *previous_period;
		// Swap so that period <-> previous_period after this point
		*period = b;
		*previous_period = a;

		int32_t new_period = ((int32_t)*period - (int32_t)*previous_period) * (int32_t)v->portamento_speed_counter;
		if(v->portamento_speed != 0) {
			new_period /= (int32_t)v->portamento_speed;
		}
		new_period += (int32_t)*previous_period;
		*period = (uint16_t)new_period;
	}
}

// [=]===^=[ is20_do_vibrato ]====================================================================[=]
static void is20_do_vibrato(uint16_t *period, struct is20_voice *v) {
	if(v->vibrato_delay != 255) {
		if(v->vibrato_delay == 0) {
			int8_t vib_val = is20_vibrato[v->vibrato_position & 0xff];
			uint8_t vib_level = v->vibrato_level;

			if(vib_val < 0) {
				if(vib_level != 0) {
					*period = (uint16_t)(*period - (uint16_t)(((-(int32_t)vib_val) * 4) / (int32_t)vib_level));
				}
			} else {
				if(vib_level != 0) {
					*period = (uint16_t)(*period + (uint16_t)(((int32_t)vib_val * 4) / (int32_t)vib_level));
				}
			}

			v->vibrato_position = (uint16_t)((v->vibrato_position + v->vibrato_speed) & 0xff);
		} else {
			v->vibrato_delay--;
		}
	}
	*period = (uint16_t)((int32_t)*period + (int32_t)v->slide_value);
}

// [=]===^=[ is20_do_lfo ]========================================================================[=]
static void is20_do_lfo(uint16_t *period, struct is20_voice *v, struct is20_instrument *instr) {
	if((instr->amf_length + instr->amf_repeat) != 0) {
		int8_t lfo_val = instr->lfo_table[v->lfo_position & 0x7f];
		*period = (uint16_t)((int32_t)*period - (int32_t)lfo_val);

		if(v->lfo_position == (uint16_t)(instr->amf_length + instr->amf_repeat)) {
			v->lfo_position = instr->amf_length;
		} else {
			v->lfo_position++;
		}
	}
}

// [=]===^=[ is20_do_adsr ]=======================================================================[=]
static void is20_do_adsr(struct instereo20_state *s, struct is20_voice *v, int32_t ch, struct is20_instrument *instr) {
	if((instr->adsr_length + instr->adsr_repeat) == 0) {
		paula_set_volume_256(&s->paula, ch, v->current_volume);
		return;
	}

	uint8_t adsr_val = instr->adsr_table[v->adsr_position & 0x7f];
	uint16_t vol = (uint16_t)(((uint32_t)v->current_volume * (uint32_t)adsr_val) / 256u);
	paula_set_volume_256(&s->paula, ch, vol);

	if(v->adsr_position >= (uint16_t)(instr->adsr_length + instr->adsr_repeat)) {
		v->adsr_position = instr->adsr_length;
	} else {
		if((v->note != 0x80) || (instr->sustain_speed == 1) || (v->adsr_position < instr->sustain_point)) {
			v->adsr_position++;
		} else {
			if(instr->sustain_speed != 0) {
				if(v->sustain_counter == 0) {
					v->sustain_counter = instr->sustain_speed;
					v->adsr_position++;
				} else {
					v->sustain_counter--;
				}
			}
		}
	}
}

// [=]===^=[ is20_do_envelope_generator ]=========================================================[=]
static void is20_do_envelope_generator(struct instereo20_state *s, struct is20_voice *v, int32_t ch, struct is20_instrument *instr) {
	if(instr->envelope_generator_mode == IS20_EG_DISABLED) {
		return;
	}

	uint8_t eg_val;

	if(instr->envelope_generator_mode == IS20_EG_FREE) {
		int32_t total_len = (int32_t)instr->start_len + (int32_t)instr->stop_rep;
		if(total_len == 0) {
			return;
		}
		if(v->envelope_generator_position >= (uint16_t)total_len) {
			v->envelope_generator_position = instr->start_len;
		} else {
			v->envelope_generator_position++;
		}
		eg_val = instr->envelope_generator_table[v->envelope_generator_position & 0x7f];
	} else {
		// Calc mode
		if(v->envelope_generator_duration == 0) {
			return;
		}
		uint16_t position = v->envelope_generator_position;
		if(v->envelope_generator_duration > 0) {
			position = (uint16_t)(position + (uint16_t)((uint32_t)instr->speed_up * 32u));
			if((position >> 8) >= instr->stop_rep) {
				v->envelope_generator_position = (uint16_t)((uint16_t)instr->stop_rep << 8);
				v->envelope_generator_duration = -1;
			} else {
				v->envelope_generator_position = position;
			}
		} else {
			position = (uint16_t)(position - (uint16_t)((uint32_t)instr->speed_down * 32u));
			if((position >> 8) >= instr->start_len) {
				v->envelope_generator_position = (uint16_t)((uint16_t)instr->start_len << 8);
				v->envelope_generator_duration = 1;
			} else {
				v->envelope_generator_position = position;
			}
		}
		eg_val = (uint8_t)(v->envelope_generator_position >> 8);
	}

	int8_t *waveform = ((eg_val & 1) != 0) ? instr->waveform2 : instr->waveform1;
	uint32_t length = instr->waveform_length;
	uint32_t start_offset = (uint32_t)eg_val & 0xfe;

	paula_queue_sample(&s->paula, ch, waveform, start_offset, length);
	paula_set_loop(&s->paula, ch, start_offset, length);
}

// [=]===^=[ is20_update_voice_effect ]===========================================================[=]
static void is20_update_voice_effect(struct instereo20_state *s, struct is20_voice *v, int32_t ch) {
	if(v->transposed_instrument == 0) {
		paula_mute(&s->paula, ch);
		return;
	}

	uint16_t period = 0;
	uint16_t previous_period = 0;

	if(v->playing_mode == IS20_PM_SAMPLE) {
		is20_do_sample_arpeggio(v, &period, &previous_period);
		is20_do_portamento(&period, &previous_period, v);
		is20_do_vibrato(&period, v);

		if(period != 0) {
			paula_set_period(&s->paula, ch, period);
		}
		paula_set_volume(&s->paula, ch, v->current_volume);
	} else {
		if((uint32_t)(v->transposed_instrument - 1) >= s->num_instruments) {
			paula_mute(&s->paula, ch);
			return;
		}
		struct is20_instrument *instr = &s->instruments[v->transposed_instrument - 1];

		is20_do_synth_arpeggio(v, instr, &period, &previous_period);
		is20_do_portamento(&period, &previous_period, v);
		is20_do_vibrato(&period, v);
		is20_do_lfo(&period, v, instr);

		if(period != 0) {
			paula_set_period(&s->paula, ch, period);
		}

		is20_do_adsr(s, v, ch, instr);
		is20_do_envelope_generator(s, v, ch, instr);
	}

	v->slide_value = (int16_t)(v->slide_value - v->slide_speed);
}

// [=]===^=[ is20_tick ]==========================================================================[=]
static void is20_tick(struct instereo20_state *s) {
	s->playing_info.speed_counter++;
	if(s->playing_info.speed_counter >= s->playing_info.current_speed) {
		s->playing_info.speed_counter = 0;
		is20_get_next_row(s);
	}

	for(int32_t i = 0; i < IS20_NUM_CHANNELS; ++i) {
		is20_update_voice_effect(s, &s->voices[i], i);
	}
}

// [=]===^=[ instereo20_init ]====================================================================[=]
static struct instereo20_state *instereo20_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 16 || sample_rate < 8000) {
		return 0;
	}
	if(!is20_identify((uint8_t *)data, len)) {
		return 0;
	}

	struct instereo20_state *s = (struct instereo20_state *)calloc(1, sizeof(struct instereo20_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;

	if(!is20_load(s)) {
		is20_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, IS20_TICK_HZ);
	is20_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ instereo20_free ]====================================================================[=]
static void instereo20_free(struct instereo20_state *s) {
	if(!s) {
		return;
	}
	is20_cleanup(s);
	free(s);
}

// [=]===^=[ instereo20_get_audio ]===============================================================[=]
static void instereo20_get_audio(struct instereo20_state *s, int16_t *output, int32_t frames) {
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
			is20_tick(s);
		}
	}
}

// [=]===^=[ instereo20_api_init ]================================================================[=]
static void *instereo20_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return instereo20_init(data, len, sample_rate);
}

// [=]===^=[ instereo20_api_free ]================================================================[=]
static void instereo20_api_free(void *state) {
	instereo20_free((struct instereo20_state *)state);
}

// [=]===^=[ instereo20_api_get_audio ]===========================================================[=]
static void instereo20_api_get_audio(void *state, int16_t *output, int32_t frames) {
	instereo20_get_audio((struct instereo20_state *)state, output, frames);
}

static const char *instereo20_extensions[] = { "is", "is20", 0 };

static struct player_api instereo20_api = {
	"InStereo 2.0",
	instereo20_extensions,
	instereo20_api_init,
	instereo20_api_free,
	instereo20_api_get_audio,
	0,
};
