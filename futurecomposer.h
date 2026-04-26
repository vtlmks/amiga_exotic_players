// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Future Composer 1.4 replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct futurecomposer_state *futurecomposer_init(void *data, uint32_t len, int32_t sample_rate);
//   void futurecomposer_free(struct futurecomposer_state *s);
//   void futurecomposer_get_audio(struct futurecomposer_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define FC_TICK_HZ     50
#define FC_NUM_SAMPLES 10
#define FC_NUM_WAVES   80
#define FC_TOTAL_SLOTS (FC_NUM_SAMPLES + FC_NUM_WAVES)
#define FC_MULTI_COUNT 20

struct fc_sample {
	int8_t *data;            // points into module buffer
	uint16_t length;         // in samples (bytes)
	uint16_t loop_start;
	uint16_t loop_length;
	int16_t sample_number;
};

struct fc_multi {
	struct fc_sample samples[FC_MULTI_COUNT];
};

struct fc_slot {
	struct fc_sample base;
	struct fc_multi *multi;  // non-null => multi sample
};

struct fc_sequence {
	uint8_t pattern[4];
	int8_t transpose[4];
	int8_t sound_transpose[4];
	uint8_t speed;
};

struct fc_pattern_row {
	uint8_t note;
	uint8_t info;
};

struct fc_pattern {
	struct fc_pattern_row rows[32];
};

struct fc_volseq {
	uint8_t speed;
	uint8_t frq_number;
	int8_t vib_speed;
	int8_t vib_depth;
	uint8_t vib_delay;
	uint8_t values[59];
};

struct fc_voice {
	int8_t pitch_bend_speed;
	uint8_t pitch_bend_time;
	uint16_t song_pos;
	int8_t cur_note;
	uint8_t *volume_seq;         // points at fc_volseq.values (length 59)
	uint32_t volume_seq_len;
	uint8_t volume_bend_speed;
	uint8_t volume_bend_time;
	uint16_t volume_seq_pos;
	int8_t sound_transpose;
	uint8_t volume_counter;
	uint8_t volume_speed;
	uint8_t vol_sus_counter;
	uint8_t sus_counter;
	int8_t vib_speed;
	int8_t vib_depth;
	int8_t vib_value;
	int8_t vib_delay;
	struct fc_pattern *cur_pattern;
	uint8_t vol_bend_flag;
	uint8_t port_flag;
	uint16_t pattern_pos;
	uint8_t pitch_bend_flag;
	int8_t patt_transpose;
	int8_t transpose;
	int8_t volume;
	uint8_t vib_flag;
	uint8_t portamento;
	uint16_t frequency_seq_start_offset;
	uint16_t frequency_seq_pos;
	uint16_t pitch;
	uint8_t audio_on;           // mirrors GlobalPlayingInfo.AudTemp[chan]
};

struct futurecomposer_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;
	uint8_t *owned_data;                    // non-null when init converted an
	                                        // SMOD (FC 1.0..1.3) input to FC14
	                                        // in memory; freed by fc_cleanup.

	struct fc_slot slots[FC_TOTAL_SLOTS];   // 0..9 sample, 10..89 wave
	int32_t num_samples;                    // for info
	int32_t num_waves;

	struct fc_sequence *sequences;
	int32_t num_sequences;

	struct fc_pattern *patterns;
	int32_t num_patterns;

	uint8_t *freq_sequences;                // concatenated, prefixed by Silent block
	uint32_t freq_sequences_len;

	struct fc_volseq *vol_sequences;        // index 0 = silent
	int32_t num_vol_sequences;              // includes silent

	struct fc_voice voices[4];
	uint16_t re_sp_cnt;
	uint16_t rep_spd;
	uint16_t spd_temp;
};

// [=]===^=[ fc_periods ]=========================================================================[=]
static uint16_t fc_periods[] = {
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  906,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
	 113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,
	3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1812,
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  906,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
	 113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113
};

static uint8_t fc_silent[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe1 };

// [=]===^=[ fc_read_u32_be ]=====================================================================[=]
static uint32_t fc_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ fc_read_u16_be ]=====================================================================[=]
static uint16_t fc_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ fc_cleanup ]=========================================================================[=]
static void fc_cleanup(struct futurecomposer_state *s) {
	if(!s) {
		return;
	}
	for(int32_t i = 0; i < FC_TOTAL_SLOTS; ++i) {
		if(s->slots[i].multi) {
			free(s->slots[i].multi);
			s->slots[i].multi = 0;
		}
	}
	free(s->sequences); s->sequences = 0;
	free(s->patterns); s->patterns = 0;
	free(s->freq_sequences); s->freq_sequences = 0;
	free(s->vol_sequences); s->vol_sequences = 0;
	free(s->owned_data); s->owned_data = 0;
}

// [=]===^=[ fc_smod_wave_length ]================================================================[=]
// FC 1.0..1.3 stores no per-wavetable lengths in the module file; the original
// player ROM carries fixed lengths for each of 80 wavetable slots (entries
// past the first 47 are zero, meaning "unused"). Mirrors the table from
// NostalgicPlayer's FutureComposer13Format.cs converter.
static uint8_t fc_smod_wave_length[80] = {
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x10, 0x08, 0x10, 0x10, 0x08, 0x08, 0x18, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// [=]===^=[ fc_smod_wave_tables ]================================================================[=]
// FC 1.0..1.3 wavetable contents (1344 bytes). NostalgicPlayer's
// FutureComposer13Format.cs ships incorrect data for the second half of this
// table (the waveforms past offset 1024 are wrong); the values below come
// from a working FC1.3 player and produce audibly correct synth output on
// modules that lean on those slots (e.g. inner space 1.smod).
static uint8_t fc_smod_wave_tables[1344] = {
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0x3f, 0x37, 0x2f, 0x27, 0x1f, 0x17, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0x37, 0x2f, 0x27, 0x1f, 0x17, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0x2f, 0x27, 0x1f, 0x17, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0x27, 0x1f, 0x17, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0x1f, 0x17, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x17, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x88, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x88, 0x90, 0x17, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x88, 0x90, 0x98, 0x1f, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x88, 0x90, 0x98, 0xa0, 0x27, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x88, 0x90, 0x98, 0xa0, 0xa8, 0x2f, 0x37,
	0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
	0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x88, 0x90, 0x98, 0xa0, 0xa8, 0xb0, 0x37,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x80, 0x80, 0x90, 0x98, 0xa0, 0xa8, 0xb0, 0xb8, 0xc0, 0xc8, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8,
	0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x7f,
	0x80, 0x80, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
	0x45, 0x45, 0x79, 0x7d, 0x7a, 0x77, 0x70, 0x66, 0x61, 0x58, 0x53, 0x4d, 0x2c, 0x20, 0x18, 0x12,
	0x04, 0xdb, 0xd3, 0xcd, 0xc6, 0xbc, 0xb5, 0xae, 0xa8, 0xa3, 0x9d, 0x99, 0x93, 0x8e, 0x8b, 0x8a,
	0x45, 0x45, 0x79, 0x7d, 0x7a, 0x77, 0x70, 0x66, 0x5b, 0x4b, 0x43, 0x37, 0x2c, 0x20, 0x18, 0x12,
	0x04, 0xf8, 0xe8, 0xdb, 0xcf, 0xc6, 0xbe, 0xb0, 0xa8, 0xa4, 0x9e, 0x9a, 0x95, 0x94, 0x8d, 0x83,
	0x00, 0x00, 0x40, 0x60, 0x7f, 0x60, 0x40, 0x20, 0x00, 0xe0, 0xc0, 0xa0, 0x80, 0xa0, 0xc0, 0xe0,
	0x00, 0x00, 0x40, 0x60, 0x7f, 0x60, 0x40, 0x20, 0x00, 0xe0, 0xc0, 0xa0, 0x80, 0xa0, 0xc0, 0xe0,
	0x80, 0x80, 0x90, 0x98, 0xa0, 0xa8, 0xb0, 0xb8, 0xc0, 0xc8, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8,
	0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x7f,
	0x80, 0x80, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
};

// [=]===^=[ fc_be_w_u32 ]========================================================================[=]
static void fc_be_w_u32(uint8_t *p, uint32_t v) {
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

// [=]===^=[ fc_test_smod ]=======================================================================[=]
// Detect Future Composer 1.0..1.3 (SMOD) input. Mirrors C# FutureComposer13Format.Identify.
static int32_t fc_test_smod(uint8_t *data, uint32_t len) {
	if(len < 100) {
		return 0;
	}
	if(data[0] != 'S' || data[1] != 'M' || data[2] != 'O' || data[3] != 'D') {
		return 0;
	}
	for(int32_t i = 0; i < 8; ++i) {
		if(fc_read_u32_be(data + 8 + i * 4) > len) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ fc_convert_smod ]====================================================================[=]
// Convert an SMOD (FC 1.0..1.3) module to an in-memory FC14 buffer. Mirrors
// the C# FutureComposer13Format.Convert: rewrites the header, copies sample
// metadata, embeds the player ROM's wave-length and wave-table data, applies
// the portamento doubling fix to pattern bytes, and writes the new section
// offsets back at the FC14 layout positions. Caller frees the returned buffer.
static uint8_t *fc_convert_smod(uint8_t *src, uint32_t src_len, uint32_t *out_len) {
	if(src_len < 100) {
		return 0;
	}

	uint32_t orig_off[8];
	for(uint32_t i = 0; i < 8; ++i) {
		orig_off[i] = fc_read_u32_be(src + 8 + i * 4);
	}

	uint32_t seq_length = fc_read_u32_be(src + 4);
	if(seq_length == 0) {
		// cult.fc fix: when the source omits seqLength, derive it from the
		// distance between the SMOD header end and the first section offset.
		if(orig_off[0] < 0x64) {
			return 0;
		}
		seq_length = orig_off[0] - 0x64;
	}
	uint32_t out_seq_length = (seq_length & 1u) ? (seq_length + 1u) : seq_length;

	if(100u + seq_length > src_len) {
		return 0;
	}
	if(orig_off[0] + orig_off[1] > src_len) {
		return 0;
	}
	if(orig_off[2] + orig_off[3] > src_len) {
		return 0;
	}
	if(orig_off[4] + orig_off[5] > src_len) {
		return 0;
	}

	uint16_t sample_lengths_w[10];
	uint16_t sample_loop_starts[10];
	uint16_t sample_loop_lens[10];
	uint64_t total_sample_bytes = 0;
	for(int32_t i = 0; i < 10; ++i) {
		uint8_t *hp = src + 40 + i * 6;
		sample_lengths_w[i]   = fc_read_u16_be(hp + 0);
		sample_loop_starts[i] = fc_read_u16_be(hp + 2);
		sample_loop_lens[i]   = fc_read_u16_be(hp + 4);
		total_sample_bytes += (uint64_t)sample_lengths_w[i] * 2u;
	}
	if((uint64_t)orig_off[6] + total_sample_bytes > src_len) {
		return 0;
	}

	// Output layout:
	//   [0..3]      "FC14"
	//   [4..7]      out_seq_length
	//   [8..39]     8 section offsets/lengths (filled at end)
	//   [40..99]    10 sample headers (60 bytes)
	//   [100..179]  fc_smod_wave_length (80 bytes)
	//   [180..]     sequences (out_seq_length bytes)
	//   [..]        optional 1-byte alignment pad
	//   [..]        patterns (orig_off[1] bytes)
	//   [..]        freq sequences (orig_off[3] bytes)
	//   [..]        vol sequences (orig_off[5] bytes)
	//   [..]        sample data + 2-byte pad per slot (10 slots)
	//   [..]        fc_smod_wave_tables (1344 bytes)
	uint64_t out_size = 4ull + 4ull + 32ull + 60ull + 80ull
	                  + (uint64_t)out_seq_length + 1ull
	                  + (uint64_t)orig_off[1]
	                  + (uint64_t)orig_off[3]
	                  + (uint64_t)orig_off[5]
	                  + total_sample_bytes + 20ull
	                  + 1344ull;
	if(out_size > 0x40000000ull) {
		return 0;
	}

	uint8_t *out = (uint8_t *)calloc(1, (size_t)out_size);
	if(!out) {
		return 0;
	}
	uint32_t op = 0;

	out[op++] = 'F'; out[op++] = 'C'; out[op++] = '1'; out[op++] = '4';
	fc_be_w_u32(out + op, out_seq_length); op += 4;

	uint32_t off_placeholder = op;
	for(int32_t i = 0; i < 32; ++i) {
		out[op++] = 0;
	}

	for(int32_t i = 0; i < 10; ++i) {
		out[op++] = (uint8_t)(sample_lengths_w[i] >> 8);
		out[op++] = (uint8_t)sample_lengths_w[i];
		out[op++] = (uint8_t)(sample_loop_starts[i] >> 8);
		out[op++] = (uint8_t)sample_loop_starts[i];
		out[op++] = (uint8_t)(sample_loop_lens[i] >> 8);
		out[op++] = (uint8_t)sample_loop_lens[i];
	}

	memcpy(out + op, fc_smod_wave_length, 80); op += 80;

	memcpy(out + op, src + 100, seq_length); op += seq_length;
	if(out_seq_length > seq_length) {
		out[op++] = 0;
	}

	uint32_t newoffsets[8];

	newoffsets[0] = op;
	if(newoffsets[0] & 1u) {
		out[op++] = 0;
		newoffsets[0]++;
	}
	newoffsets[1] = orig_off[1];

	if(orig_off[1] > 0) {
		memcpy(out + op, src + orig_off[0], orig_off[1]);
		// Portamento doubling fix from the C# converter: when the high bit of
		// a portamento command flag byte is set, double the data byte two
		// slots later (low 5 bits scaled, high bit preserved).
		if(orig_off[1] >= 3) {
			for(uint32_t i = 1; i + 2 < orig_off[1]; i += 2) {
				if(out[op + i] & 0x80u) {
					uint8_t b = out[op + i + 2];
					out[op + i + 2] = (uint8_t)((((b & 0x1fu) * 2u) & 0x1fu) | (b & 0x20u));
				}
			}
		}
		op += orig_off[1];
	}

	newoffsets[2] = op;
	newoffsets[3] = orig_off[3];
	memcpy(out + op, src + orig_off[2], orig_off[3]); op += orig_off[3];

	newoffsets[4] = op;
	newoffsets[5] = orig_off[5];
	memcpy(out + op, src + orig_off[4], orig_off[5]); op += orig_off[5];

	newoffsets[6] = op;
	{
		uint32_t sp = orig_off[6];
		for(int32_t i = 0; i < 10; ++i) {
			uint32_t slen = (uint32_t)sample_lengths_w[i] * 2u;
			if(slen > 0) {
				memcpy(out + op, src + sp, slen);
				op += slen;
				sp += slen;
			}
			out[op++] = 0;
			out[op++] = 0;
		}
	}

	newoffsets[7] = op;
	memcpy(out + op, fc_smod_wave_tables, 1344); op += 1344;

	for(int32_t i = 0; i < 8; ++i) {
		fc_be_w_u32(out + off_placeholder + i * 4, newoffsets[i]);
	}

	*out_len = op;
	return out;
}

// [=]===^=[ fc_identify ]========================================================================[=]
static int32_t fc_identify(uint8_t *data, uint32_t len) {
	if(len < 180) {
		return 0;
	}
	if(data[0] != 'F' || data[1] != 'C' || data[2] != '1' || data[3] != '4') {
		return 0;
	}
	for(int32_t i = 0; i < 8; ++i) {
		if(fc_read_u32_be(data + 8 + i * 4) > len) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ fc_load ]============================================================================[=]
static int32_t fc_load(struct futurecomposer_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	if(len < 180) {
		return 0;
	}

	uint32_t pos = 4;
	uint32_t seq_length  = fc_read_u32_be(data + pos); pos += 4;
	uint32_t pat_offset  = fc_read_u32_be(data + pos); pos += 4;
	uint32_t pat_length  = fc_read_u32_be(data + pos); pos += 4;
	uint32_t frq_offset  = fc_read_u32_be(data + pos); pos += 4;
	uint32_t frq_length  = fc_read_u32_be(data + pos); pos += 4;
	uint32_t vol_offset  = fc_read_u32_be(data + pos); pos += 4;
	uint32_t vol_length  = fc_read_u32_be(data + pos); pos += 4;
	uint32_t smp_offset  = fc_read_u32_be(data + pos); pos += 4;
	uint32_t wav_offset  = fc_read_u32_be(data + pos); pos += 4;

	if(pat_offset > len || frq_offset > len || vol_offset > len || smp_offset > len || wav_offset > len) {
		return 0;
	}

	// Sample info: 10 * 6 bytes
	if(pos + 10 * 6 > len) {
		return 0;
	}
	s->num_samples = 10;
	for(int32_t i = 0; i < 10; ++i) {
		struct fc_sample *samp = &s->slots[i].base;
		uint16_t slen = fc_read_u16_be(data + pos) * 2;      pos += 2;
		uint16_t lstart = fc_read_u16_be(data + pos);        pos += 2;
		uint16_t llen = fc_read_u16_be(data + pos) * 2;      pos += 2;
		samp->length = slen;
		samp->loop_start = lstart;
		samp->loop_length = llen;
		samp->data = 0;
		if(samp->loop_start >= samp->length) {
			samp->loop_start = 0;
			samp->loop_length = 2;
		}
	}

	// Wave table lengths: 80 * 1 byte
	if(pos + FC_NUM_WAVES > len) {
		return 0;
	}
	for(int32_t i = 0; i < FC_NUM_WAVES; ++i) {
		struct fc_sample *samp = &s->slots[10 + i].base;
		samp->length = (uint16_t)(data[pos++] * 2);
		samp->loop_start = 0;
		samp->loop_length = samp->length;
		samp->data = 0;
	}

	// Count used wave tables
	s->num_waves = 0;
	for(int32_t i = FC_NUM_WAVES - 1; i >= 0; --i) {
		if(s->slots[10 + i].base.length != 0) {
			s->num_waves = i + 1;
			break;
		}
	}

	// Sequences: seq_length / 13 entries, 13 bytes each
	s->num_sequences = (int32_t)(seq_length / 13);
	if(s->num_sequences == 0) {
		s->num_sequences = 1;
		s->sequences = (struct fc_sequence *)calloc(1, sizeof(struct fc_sequence));
		if(!s->sequences) {
			return 0;
		}
	} else {
		if(pos + s->num_sequences * 13 > len) {
			return 0;
		}
		s->sequences = (struct fc_sequence *)calloc((size_t)s->num_sequences, sizeof(struct fc_sequence));
		if(!s->sequences) {
			return 0;
		}
		for(int32_t i = 0; i < s->num_sequences; ++i) {
			struct fc_sequence *seq = &s->sequences[i];
			for(int32_t c = 0; c < 4; ++c) {
				seq->pattern[c]         = data[pos++];
				seq->transpose[c]       = (int8_t)data[pos++];
				seq->sound_transpose[c] = (int8_t)data[pos++];
			}
			seq->speed = data[pos++];
		}
	}

	// Patterns
	s->num_patterns = (int32_t)(pat_length / 64);
	if(pat_offset + s->num_patterns * 64 > len) {
		return 0;
	}
	s->patterns = (struct fc_pattern *)calloc((size_t)s->num_patterns, sizeof(struct fc_pattern));
	if(!s->patterns) {
		return 0;
	}
	for(int32_t i = 0; i < s->num_patterns; ++i) {
		uint8_t *pp = data + pat_offset + i * 64;
		for(int32_t j = 0; j < 32; ++j) {
			s->patterns[i].rows[j].note = pp[j * 2 + 0];
			s->patterns[i].rows[j].info = pp[j * 2 + 1];
		}
	}

	// Frequency sequences: silent block + raw data + terminator
	if(frq_offset + frq_length > len) {
		return 0;
	}
	uint32_t silent_len = sizeof(fc_silent);
	s->freq_sequences_len = silent_len + frq_length + 1;
	s->freq_sequences = (uint8_t *)calloc(s->freq_sequences_len, 1);
	if(!s->freq_sequences) {
		return 0;
	}
	memcpy(s->freq_sequences, fc_silent, silent_len);
	memcpy(s->freq_sequences + silent_len, data + frq_offset, frq_length);
	s->freq_sequences[s->freq_sequences_len - 1] = 0xe1;

	// Volume sequences: 1 silent + vol_length/64 entries of 64 bytes each
	int32_t vol_num = (int32_t)(vol_length / 64);
	s->num_vol_sequences = 1 + vol_num;
	s->vol_sequences = (struct fc_volseq *)calloc((size_t)s->num_vol_sequences, sizeof(struct fc_volseq));
	if(!s->vol_sequences) {
		return 0;
	}
	// Silent vol seq (index 0)
	s->vol_sequences[0].speed      = fc_silent[0];
	s->vol_sequences[0].frq_number = fc_silent[1];
	s->vol_sequences[0].vib_speed  = (int8_t)fc_silent[2];
	s->vol_sequences[0].vib_depth  = (int8_t)fc_silent[3];
	s->vol_sequences[0].vib_delay  = fc_silent[4];
	memcpy(s->vol_sequences[0].values, fc_silent + 4, silent_len - 4);

	if(vol_offset + vol_num * 64 > len) {
		return 0;
	}
	for(int32_t i = 0; i < vol_num; ++i) {
		uint8_t *vp = data + vol_offset + i * 64;
		struct fc_volseq *vs = &s->vol_sequences[1 + i];
		vs->speed      = vp[0];
		vs->frq_number = vp[1];
		vs->vib_speed  = (int8_t)vp[2];
		vs->vib_depth  = (int8_t)vp[3];
		vs->vib_delay  = vp[4];
		memcpy(vs->values, vp + 5, 59);
	}

	// Load samples
	uint32_t sp = smp_offset;
	int16_t real_sample_number = 0;
	for(int32_t i = 0; i < 10; ++i) {
		s->slots[i].base.sample_number = real_sample_number++;

		if(s->slots[i].base.length != 0) {
			if(sp + 4 > len) {
				return 0;
			}
			if(data[sp] == 'S' && data[sp + 1] == 'S' && data[sp + 2] == 'M' && data[sp + 3] == 'P') {
				sp += 4;
				struct fc_multi *multi = (struct fc_multi *)calloc(1, sizeof(struct fc_multi));
				if(!multi) {
					return 0;
				}
				uint32_t offsets[FC_MULTI_COUNT];
				if(sp + FC_MULTI_COUNT * 16 > len) {
					free(multi);
					return 0;
				}
				for(int32_t j = 0; j < FC_MULTI_COUNT; ++j) {
					offsets[j]                 = fc_read_u32_be(data + sp); sp += 4;
					multi->samples[j].length   = fc_read_u16_be(data + sp) * 2; sp += 2;
					multi->samples[j].loop_start  = fc_read_u16_be(data + sp); sp += 2;
					multi->samples[j].loop_length = fc_read_u16_be(data + sp) * 2; sp += 2;
					sp += 6; // pad
				}
				// Multi sample is not counted as a single slot
				s->num_samples--;
				real_sample_number--;

				uint32_t samp_start = sp;
				for(int32_t j = 0; j < FC_MULTI_COUNT; ++j) {
					if(multi->samples[j].length != 0) {
						uint32_t abs_off = samp_start + offsets[j];
						if(abs_off + multi->samples[j].length > len) {
							free(multi);
							return 0;
						}
						multi->samples[j].data = (int8_t *)(data + abs_off);
						multi->samples[j].sample_number = real_sample_number++;
						s->num_samples++;
					}
				}
				s->slots[i].multi = multi;

				// Advance sp past the multi sample data block (use max of offset + length)
				uint32_t max_end = samp_start;
				for(int32_t j = 0; j < FC_MULTI_COUNT; ++j) {
					if(multi->samples[j].length != 0) {
						uint32_t e = samp_start + offsets[j] + multi->samples[j].length + 2; // +2 pad
						if(e > max_end) {
							max_end = e;
						}
					}
				}
				sp = max_end;
			} else {
				// Normal sample
				if(sp + s->slots[i].base.length > len) {
					return 0;
				}
				s->slots[i].base.data = (int8_t *)(data + sp);
				sp += s->slots[i].base.length;
				sp += 2; // pad
			}
		} else {
			sp += 2; // pad
		}
	}

	// Load wave tables
	uint32_t wp = wav_offset;
	for(int32_t i = 10; i < FC_TOTAL_SLOTS; ++i) {
		if(s->slots[i].base.length != 0) {
			if(wp + s->slots[i].base.length > len) {
				return 0;
			}
			s->slots[i].base.data = (int8_t *)(data + wp);
			s->slots[i].base.sample_number = real_sample_number++;
			wp += s->slots[i].base.length;
		}
	}

	return 1;
}

// [=]===^=[ fc_initialize_sound ]================================================================[=]
static void fc_initialize_sound(struct futurecomposer_state *s) {
	uint16_t spd = s->sequences[0].speed;
	if(spd == 0) {
		spd = 3;
	}
	s->re_sp_cnt = spd;
	s->rep_spd = spd;
	s->spd_temp = 1;

	for(int32_t i = 0; i < 4; ++i) {
		struct fc_voice *v = &s->voices[i];
		memset(v, 0, sizeof(*v));
		v->volume_seq = s->vol_sequences[0].values;
		v->volume_seq_len = sizeof(s->vol_sequences[0].values);
		v->volume_counter = 1;
		v->volume_speed = 1;
		v->cur_pattern = &s->patterns[s->sequences[0].pattern[i]];
		v->transpose = s->sequences[0].transpose[i];
		v->sound_transpose = s->sequences[0].sound_transpose[i];
	}
}

// [=]===^=[ fc_new_note ]========================================================================[=]
static void fc_new_note(struct futurecomposer_state *s, int32_t chan) {
	struct fc_voice *v = &s->voices[chan];

	// End of pattern or explicit "end" marker
	if((v->pattern_pos == 32) || (v->cur_pattern->rows[v->pattern_pos].note == 0x49)) {
		v->song_pos++;
		v->pattern_pos = 0;

		if(v->song_pos >= s->num_sequences) {
			v->song_pos = 0;
		}

		s->spd_temp++;
		if(s->spd_temp == 5) {
			s->spd_temp = 1;
			if(s->sequences[v->song_pos].speed != 0) {
				s->re_sp_cnt = s->sequences[v->song_pos].speed;
				s->rep_spd = s->re_sp_cnt;
			}
		}

		v->transpose = s->sequences[v->song_pos].transpose[chan];
		v->sound_transpose = s->sequences[v->song_pos].sound_transpose[chan];

		uint8_t patt_num = s->sequences[v->song_pos].pattern[chan];
		if(patt_num >= s->num_patterns) {
			patt_num = 0;
		}
		v->cur_pattern = &s->patterns[patt_num];
	}

	uint8_t note = v->cur_pattern->rows[v->pattern_pos].note;
	uint8_t info = v->cur_pattern->rows[v->pattern_pos].info;

	if((note != 0) || ((info & 0xc0) != 0)) {
		if(note != 0) {
			v->pitch = 0;
		}
		if((info & 0x80) != 0) {
			v->portamento = (v->pattern_pos < 31) ? v->cur_pattern->rows[v->pattern_pos + 1].info : 0;
		} else {
			v->portamento = 0;
		}
	}

	note &= 0x7f;
	if(note != 0) {
		v->cur_note = (int8_t)note;

		// Mute the channel
		v->audio_on = 0;
		paula_mute(&s->paula, chan);

		int32_t inst = (int32_t)(info & 0x3f) + (int32_t)v->sound_transpose;
		if(inst < 0 || inst >= (s->num_vol_sequences - 1)) {
			inst = 0;
		} else {
			inst++;
		}

		struct fc_volseq *vs = &s->vol_sequences[inst];
		v->volume_seq_pos = 0;
		v->volume_counter = vs->speed;
		v->volume_speed = vs->speed;
		v->vol_sus_counter = 0;

		v->vib_speed = vs->vib_speed;
		v->vib_flag = 0x40;
		v->vib_depth = vs->vib_depth;
		v->vib_value = vs->vib_depth;
		v->vib_delay = (int8_t)vs->vib_delay;
		v->volume_seq = vs->values;
		v->volume_seq_len = sizeof(vs->values);

		v->frequency_seq_start_offset = (uint16_t)(sizeof(fc_silent) + vs->frq_number * 64);
		v->frequency_seq_pos = 0;
		v->sus_counter = 0;
	}

	v->pattern_pos++;
}

// [=]===^=[ fc_do_vol_bend ]=====================================================================[=]
static void fc_do_vol_bend(struct fc_voice *v) {
	v->vol_bend_flag ^= 1;
	if(v->vol_bend_flag) {
		v->volume_bend_time--;
		v->volume = (int8_t)(v->volume + (int8_t)v->volume_bend_speed);
		if(v->volume > 64) {
			v->volume = 64;
			v->volume_bend_time = 0;
		} else if(v->volume < 0) {
			v->volume = 0;
			v->volume_bend_time = 0;
		}
	}
}

// [=]===^=[ fc_apply_waveform ]==================================================================[=]
// Helper for the 0xe2 "set wave form" opcode: start a new sample on the channel.
static void fc_apply_waveform(struct futurecomposer_state *s, int32_t chan, uint8_t dat) {
	if(dat >= FC_TOTAL_SLOTS) {
		return;
	}
	struct fc_sample *smp = &s->slots[dat].base;
	if(!smp->data) {
		return;
	}
	paula_play_sample(&s->paula, chan, smp->data, smp->length);
	if(smp->loop_length > 2) {
		uint32_t ls = smp->loop_start;
		uint32_t ll = smp->loop_length;
		if(ls + ll > smp->length) {
			ll = (uint32_t)smp->length - ls;
		}
		paula_set_loop(&s->paula, chan, ls, ll);
	}
}

// [=]===^=[ fc_apply_loop_switch ]===============================================================[=]
// Helper for the 0xe4 "set loop" opcode: deferred sample switch plus new loop.
static void fc_apply_loop_switch(struct futurecomposer_state *s, int32_t chan, uint8_t dat) {
	if(dat >= FC_TOTAL_SLOTS) {
		return;
	}
	struct fc_sample *smp = &s->slots[dat].base;
	if(!smp->data) {
		return;
	}
	paula_queue_sample(&s->paula, chan, smp->data, smp->loop_start, smp->loop_length);
	paula_set_loop(&s->paula, chan, smp->loop_start, smp->loop_length);
}

// [=]===^=[ fc_apply_multi_sample ]==============================================================[=]
// Helper for the 0xe9 "set sample" opcode.
static void fc_apply_multi_sample(struct futurecomposer_state *s, int32_t chan, uint8_t slot_idx, uint8_t sub_idx) {
	if(slot_idx >= FC_TOTAL_SLOTS || !s->slots[slot_idx].multi) {
		return;
	}
	if(sub_idx >= FC_MULTI_COUNT) {
		return;
	}
	struct fc_sample *smp = &s->slots[slot_idx].multi->samples[sub_idx];
	if(!smp->data) {
		return;
	}
	paula_play_sample(&s->paula, chan, smp->data, smp->length);
	if(smp->loop_length > 2) {
		uint32_t ls = smp->loop_start;
		uint32_t ll = smp->loop_length;
		if(ls + ll > smp->length) {
			ll = (uint32_t)smp->length - ls;
		}
		paula_set_loop(&s->paula, chan, ls, ll);
	}
}

// [=]===^=[ fc_effect ]==========================================================================[=]
static void fc_effect(struct futurecomposer_state *s, int32_t chan) {
	struct fc_voice *v = &s->voices[chan];

	// --- frequency sequence commands ---
	int32_t one_more = 1;
	while(one_more) {
		one_more = 0;

		if(v->sus_counter != 0) {
			v->sus_counter--;
			break;
		}

		uint32_t seq_poi = (uint32_t)v->frequency_seq_start_offset + (uint32_t)v->frequency_seq_pos;
		if(seq_poi >= s->freq_sequences_len) {
			break;
		}

		int32_t parse_effect = 1;
		while(parse_effect) {
			parse_effect = 0;

			uint8_t cmd = s->freq_sequences[seq_poi++];
			if(cmd == 0xe1) {
				break;
			}

			// 0xe0: loop to another position in the sequence
			if(cmd == 0xe0) {
				if(seq_poi >= s->freq_sequences_len) {
					break;
				}
				uint8_t dat = (uint8_t)(s->freq_sequences[seq_poi] & 0x3f);
				v->frequency_seq_pos = dat;
				seq_poi = (uint32_t)v->frequency_seq_start_offset + dat;
				if(seq_poi >= s->freq_sequences_len) {
					break;
				}
				cmd = s->freq_sequences[seq_poi++];
			}

			uint8_t dat;
			switch(cmd) {
				case 0xe2: {
					if(seq_poi >= s->freq_sequences_len) {
						break;
					}
					dat = s->freq_sequences[seq_poi++];
					if(dat < FC_TOTAL_SLOTS) {
						fc_apply_waveform(s, chan, dat);
					}
					v->volume_seq_pos = 0;
					v->volume_counter = 1;
					v->frequency_seq_pos += 2;
					v->audio_on = 1;
					break;
				}
				case 0xe4: {
					if(v->audio_on) {
						if(seq_poi >= s->freq_sequences_len) {
							break;
						}
						dat = s->freq_sequences[seq_poi++];
						if(dat < FC_TOTAL_SLOTS) {
							fc_apply_loop_switch(s, chan, dat);
						}
						v->frequency_seq_pos += 2;
					}
					break;
				}
				case 0xe9: {
					v->audio_on = 1;
					if(seq_poi + 1 >= s->freq_sequences_len) {
						break;
					}
					uint8_t slot_idx = s->freq_sequences[seq_poi++];
					uint8_t sub_idx  = s->freq_sequences[seq_poi++];
					fc_apply_multi_sample(s, chan, slot_idx, sub_idx);
					v->volume_seq_pos = 0;
					v->volume_counter = 1;
					v->frequency_seq_pos += 3;
					break;
				}
				case 0xe7: {
					parse_effect = 1;
					if(seq_poi >= s->freq_sequences_len) {
						break;
					}
					dat = s->freq_sequences[seq_poi];
					uint32_t new_poi = sizeof(fc_silent) + (uint32_t)dat * 64;
					if(new_poi >= s->freq_sequences_len) {
						new_poi = 0;
					}
					v->frequency_seq_start_offset = (uint16_t)new_poi;
					v->frequency_seq_pos = 0;
					seq_poi = new_poi;
					break;
				}
				case 0xea: {
					if(seq_poi + 1 >= s->freq_sequences_len) {
						break;
					}
					v->pitch_bend_speed = (int8_t)s->freq_sequences[seq_poi++];
					v->pitch_bend_time  = s->freq_sequences[seq_poi++];
					v->frequency_seq_pos += 3;
					break;
				}
				case 0xe8: {
					if(seq_poi >= s->freq_sequences_len) {
						break;
					}
					v->sus_counter = s->freq_sequences[seq_poi++];
					v->frequency_seq_pos += 2;
					one_more = 1;
					break;
				}
				case 0xe3: {
					if(seq_poi + 1 >= s->freq_sequences_len) {
						break;
					}
					v->vib_speed = (int8_t)s->freq_sequences[seq_poi++];
					v->vib_depth = (int8_t)s->freq_sequences[seq_poi++];
					v->frequency_seq_pos += 3;
					break;
				}
				default: break;
			}

			if(!parse_effect && !one_more) {
				uint32_t trans_poi = (uint32_t)v->frequency_seq_start_offset + (uint32_t)v->frequency_seq_pos;
				if(trans_poi < s->freq_sequences_len) {
					v->patt_transpose = (int8_t)s->freq_sequences[trans_poi];
				} else {
					v->patt_transpose = 0;
				}
				v->frequency_seq_pos++;
			}
		}
	}

	// --- volume sequence commands ---
	if(v->vol_sus_counter != 0) {
		v->vol_sus_counter--;
	} else if(v->volume_bend_time != 0) {
		fc_do_vol_bend(v);
	} else {
		v->volume_counter--;
		if(v->volume_counter == 0) {
			v->volume_counter = v->volume_speed;
			int32_t parse_effect = 1;
			while(parse_effect) {
				parse_effect = 0;

				if(v->volume_seq_pos >= v->volume_seq_len) {
					break;
				}
				uint8_t cmd = v->volume_seq[v->volume_seq_pos];
				if(cmd == 0xe1) {
					break;
				}

				switch(cmd) {
					case 0xea: {
						if((uint32_t)(v->volume_seq_pos + 2) >= v->volume_seq_len) {
							break;
						}
						v->volume_bend_speed = v->volume_seq[v->volume_seq_pos + 1];
						v->volume_bend_time  = v->volume_seq[v->volume_seq_pos + 2];
						v->volume_seq_pos += 3;
						fc_do_vol_bend(v);
						break;
					}
					case 0xe8: {
						if((uint32_t)(v->volume_seq_pos + 1) >= v->volume_seq_len) {
							break;
						}
						v->vol_sus_counter = v->volume_seq[v->volume_seq_pos + 1];
						v->volume_seq_pos += 2;
						break;
					}
					case 0xe0: {
						if((uint32_t)(v->volume_seq_pos + 1) >= v->volume_seq_len) {
							break;
						}
						int32_t new_pos = (int32_t)(v->volume_seq[v->volume_seq_pos + 1] & 0x3f) - 5;
						if(new_pos < 0) {
							new_pos = 0;
						}
						v->volume_seq_pos = (uint16_t)new_pos;
						parse_effect = 1;
						break;
					}
					default: {
						v->volume = (int8_t)(cmd & 0x7f);
						v->volume_seq_pos++;
						break;
					}
				}
			}
		}
	}

	// --- period calculation ---
	int8_t note = v->patt_transpose;
	if(note >= 0) {
		note = (int8_t)(note + v->cur_note + v->transpose);
	}
	note = (int8_t)(note & 0x7f);

	uint16_t period = ((uint32_t)note < sizeof(fc_periods) / sizeof(fc_periods[0])) ? fc_periods[note] : 0;
	uint8_t vib_flag = v->vib_flag;

	if(v->vib_delay != 0) {
		v->vib_delay--;
	} else {
		uint16_t vib_base = (uint16_t)((uint16_t)note * 2);
		int8_t vib_dep = (int8_t)(v->vib_depth * 2);
		int8_t vib_val = v->vib_value;

		if(((vib_flag & 0x80) == 0) || ((vib_flag & 0x01) == 0)) {
			if((vib_flag & 0x20) != 0) {
				vib_val = (int8_t)(vib_val + v->vib_speed);
				if(vib_val >= vib_dep) {
					vib_flag &= (uint8_t)~0x20u;
					vib_val = vib_dep;
				}
			} else {
				vib_val = (int8_t)(vib_val - v->vib_speed);
				if(vib_val < 0) {
					vib_flag |= 0x20;
					vib_val = 0;
				}
			}
			v->vib_value = vib_val;
		}

		vib_dep = (int8_t)(vib_dep / 2);
		vib_val = (int8_t)(vib_val - vib_dep);
		vib_base = (uint16_t)(vib_base + 160);

		while(vib_base < 256) {
			vib_val = (int8_t)(vib_val * 2);
			vib_base = (uint16_t)(vib_base + 24);
		}

		period = (uint16_t)(period + (uint16_t)(int16_t)vib_val);
	}

	v->vib_flag = (uint8_t)(vib_flag ^ 0x01);

	// Portamento
	v->port_flag ^= 1;
	if(v->port_flag && v->portamento != 0) {
		if(v->portamento <= 31) {
			v->pitch = (uint16_t)(v->pitch - v->portamento);
		} else {
			v->pitch = (uint16_t)(v->pitch + (uint16_t)(v->portamento & 0x1f));
		}
	}

	// Pitch bend
	v->pitch_bend_flag ^= 1;
	if(v->pitch_bend_flag && v->pitch_bend_time != 0) {
		v->pitch_bend_time--;
		v->pitch = (uint16_t)(v->pitch - (uint16_t)(int16_t)v->pitch_bend_speed);
	}

	period = (uint16_t)(period + v->pitch);

	if(period < 113) {
		period = 113;
	} else if(period > 3424) {
		period = 3424;
	}

	if(v->volume < 0) {
		v->volume = 0;
	} else if(v->volume > 64) {
		v->volume = 64;
	}

	paula_set_period(&s->paula, chan, period);
	paula_set_volume(&s->paula, chan, (uint16_t)v->volume);
}

// [=]===^=[ fc_tick ]============================================================================[=]
static void fc_tick(struct futurecomposer_state *s) {
	s->re_sp_cnt--;
	if(s->re_sp_cnt == 0) {
		s->re_sp_cnt = s->rep_spd;
		for(int32_t i = 0; i < 4; ++i) {
			fc_new_note(s, i);
		}
	}

	for(int32_t i = 0; i < 4; ++i) {
		fc_effect(s, i);
	}
}

// [=]===^=[ futurecomposer_init ]================================================================[=]
static struct futurecomposer_state *futurecomposer_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 100 || sample_rate < 8000) {
		return 0;
	}

	uint8_t *eff_data = (uint8_t *)data;
	uint32_t eff_len = len;
	uint8_t *converted = 0;

	if(!fc_identify(eff_data, eff_len)) {
		// Try the FC 1.0..1.3 (SMOD) format and convert to FC14 in memory.
		if(!fc_test_smod(eff_data, eff_len)) {
			return 0;
		}
		uint32_t conv_len = 0;
		converted = fc_convert_smod(eff_data, eff_len, &conv_len);
		if(!converted || !fc_identify(converted, conv_len)) {
			free(converted);
			return 0;
		}
		eff_data = converted;
		eff_len = conv_len;
	}

	struct futurecomposer_state *s = (struct futurecomposer_state *)calloc(1, sizeof(struct futurecomposer_state));
	if(!s) {
		free(converted);
		return 0;
	}
	s->module_data = eff_data;
	s->module_len = eff_len;
	s->owned_data = converted;

	if(!fc_load(s)) {
		fc_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, FC_TICK_HZ);
	fc_initialize_sound(s);
	return s;
}

// [=]===^=[ futurecomposer_free ]================================================================[=]
static void futurecomposer_free(struct futurecomposer_state *s) {
	if(!s) {
		return;
	}
	fc_cleanup(s);
	free(s);
}

// [=]===^=[ futurecomposer_get_audio ]============================================================[=]
static void futurecomposer_get_audio(struct futurecomposer_state *s, int16_t *output, int32_t frames) {
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
			fc_tick(s);
		}
	}
}

// [=]===^=[ futurecomposer_api_init ]============================================================[=]
static void *futurecomposer_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return futurecomposer_init(data, len, sample_rate);
}

// [=]===^=[ futurecomposer_api_free ]============================================================[=]
static void futurecomposer_api_free(void *state) {
	futurecomposer_free((struct futurecomposer_state *)state);
}

// [=]===^=[ futurecomposer_api_get_audio ]========================================================[=]
static void futurecomposer_api_get_audio(void *state, int16_t *output, int32_t frames) {
	futurecomposer_get_audio((struct futurecomposer_state *)state, output, frames);
}

static const char *futurecomposer_extensions[] = { "fc", "fc14", "smod", 0 };

static struct player_api futurecomposer_api = {
	"Future Composer 1.0-1.4",
	futurecomposer_extensions,
	futurecomposer_api_init,
	futurecomposer_api_free,
	futurecomposer_api_get_audio,
	0,
};
