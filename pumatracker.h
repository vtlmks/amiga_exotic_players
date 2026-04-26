// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// PumaTracker replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct pumatracker_state *pumatracker_init(void *data, uint32_t len, int32_t sample_rate);
//   void pumatracker_free(struct pumatracker_state *s);
//   void pumatracker_get_audio(struct pumatracker_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define PUMATRACKER_TICK_HZ      50
#define PUMATRACKER_NUM_SAMPLES  10
#define PUMATRACKER_NUM_WAVEFORMS 44
#define PUMATRACKER_TOTAL_SAMPLES (PUMATRACKER_NUM_SAMPLES + PUMATRACKER_NUM_WAVEFORMS)

enum {
	PUMATRACKER_VF_SET_VOLUME_SLIDE = 0x01,
	PUMATRACKER_VF_SET_FREQUENCY    = 0x02,
	PUMATRACKER_VF_VOICE_RUNNING    = 0x04,
	PUMATRACKER_VF_NO_LOOP          = 0x08,
};

enum {
	PUMATRACKER_FX_NONE            = 0,
	PUMATRACKER_FX_SET_VOLUME      = 1,
	PUMATRACKER_FX_PORTAMENTO_DOWN = 2,
	PUMATRACKER_FX_PORTAMENTO_UP   = 3,
};

struct pumatracker_track_row {
	uint8_t note;
	uint8_t instrument;
	uint8_t effect;
	uint8_t effect_argument;
	uint8_t rows_to_wait;
};

struct pumatracker_track {
	struct pumatracker_track_row *rows;
	uint32_t num_rows;
};

struct pumatracker_instrument_command {
	uint8_t command;
	uint8_t argument1;
	uint8_t argument2;
	uint8_t argument3;
};

struct pumatracker_instrument {
	struct pumatracker_instrument_command *volume_commands;
	uint32_t num_volume_commands;
	struct pumatracker_instrument_command *frequency_commands;
	uint32_t num_frequency_commands;
};

struct pumatracker_voice_position {
	uint8_t track_number;
	int8_t instrument_transpose;
	int8_t note_transpose;
};

struct pumatracker_position {
	struct pumatracker_voice_position voice_position[4];
	uint8_t speed;
};

struct pumatracker_voice_info {
	struct pumatracker_track *track;
	int32_t track_position;

	uint8_t row_counter;

	struct pumatracker_instrument_command *volume_commands;
	uint32_t num_volume_commands;
	uint8_t volume_command_position;

	struct pumatracker_instrument_command *frequency_commands;
	uint32_t num_frequency_commands;
	uint8_t frequency_command_position;

	int8_t instrument_transpose;
	int8_t note_transpose;
	uint8_t transposed_note;

	int8_t *sample_data;
	uint16_t sample_length;
	uint8_t sample_number;

	uint16_t period;
	uint8_t volume;

	int16_t portamento_add_value;

	uint8_t voice_flag;

	uint8_t volume_slide_counter;
	uint8_t volume_slide_remaining_time;
	int8_t volume_slide_delta;
	int8_t volume_slide_direction;
	int16_t volume_slide_value;

	uint8_t frequency_counter;
	int32_t frequency_vary_add_value;
	int32_t frequency_vary_value;

	uint8_t waveform_change_counter;
	int8_t waveform_add_value;
	int8_t waveform_value;
};

struct pumatracker_state {
	struct paula paula;

	struct pumatracker_position *positions;
	uint32_t num_positions;

	struct pumatracker_track *tracks;
	uint32_t num_tracks;

	struct pumatracker_instrument *instruments;
	uint32_t num_instruments;

	int8_t *samples[PUMATRACKER_NUM_SAMPLES];
	uint32_t sample_lengths[PUMATRACKER_NUM_SAMPLES];

	// Index 0..9 = module samples (raw pointers into module buffer or 0).
	// Index 10..10+43 = built-in waveforms (point into pumatracker_waveforms).
	int8_t *all_samples[PUMATRACKER_TOTAL_SAMPLES];
	uint32_t all_sample_lengths[PUMATRACKER_TOTAL_SAMPLES];

	struct pumatracker_voice_info voices[4];

	uint8_t current_speed;
	uint8_t speed_counter;
	int32_t current_position;
	uint8_t current_row_number;

	// An empty track used when a position references an out-of-range track number.
	struct pumatracker_track_row empty_track_row;
	struct pumatracker_track empty_track;

	// An empty instrument (single 0xe0 end) used when track instrument index is 0.
	struct pumatracker_instrument_command empty_volume_cmd;
	struct pumatracker_instrument_command empty_frequency_cmd;
	struct pumatracker_instrument empty_instrument;
};

// [=]===^=[ pumatracker_periods ]================================================================[=]
static uint16_t pumatracker_periods[] = {
	   0,
	6848, 6464, 6096, 5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3624,
	3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1812,
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  906,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113
};

// [=]===^=[ pumatracker_waveforms ]==============================================================[=]
// 44 built-in synthesis waveforms, 32 bytes each (unsigned in C# source so kept as int8 with the
// same bit pattern; Paula reads them as signed 8-bit either way).
static int8_t pumatracker_waveforms[PUMATRACKER_NUM_WAVEFORMS][32] = {
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0x3f, (int8_t)0x37, (int8_t)0x2f, (int8_t)0x27, (int8_t)0x1f, (int8_t)0x17, (int8_t)0x0f, (int8_t)0x07, (int8_t)0xff, (int8_t)0x07, (int8_t)0x0f, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0x37, (int8_t)0x2f, (int8_t)0x27, (int8_t)0x1f, (int8_t)0x17, (int8_t)0x0f, (int8_t)0x07, (int8_t)0xff, (int8_t)0x07, (int8_t)0x0f, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0x2f, (int8_t)0x27, (int8_t)0x1f, (int8_t)0x17, (int8_t)0x0f, (int8_t)0x07, (int8_t)0xff, (int8_t)0x07, (int8_t)0x0f, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0x27, (int8_t)0x1f, (int8_t)0x17, (int8_t)0x0f, (int8_t)0x07, (int8_t)0xff, (int8_t)0x07, (int8_t)0x0f, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0x1f, (int8_t)0x17, (int8_t)0x0f, (int8_t)0x07, (int8_t)0xff, (int8_t)0x07, (int8_t)0x0f, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa0, (int8_t)0x17, (int8_t)0x0f, (int8_t)0x07, (int8_t)0xff, (int8_t)0x07, (int8_t)0x0f, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa0, (int8_t)0x98, (int8_t)0x0f, (int8_t)0x07, (int8_t)0xff, (int8_t)0x07, (int8_t)0x0f, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa0, (int8_t)0x98, (int8_t)0x90, (int8_t)0x07, (int8_t)0xff, (int8_t)0x07, (int8_t)0x0f, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa0, (int8_t)0x98, (int8_t)0x90, (int8_t)0x88, (int8_t)0xff, (int8_t)0x07, (int8_t)0x0f, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa0, (int8_t)0x98, (int8_t)0x90, (int8_t)0x88, (int8_t)0x80, (int8_t)0x07, (int8_t)0x0f, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa0, (int8_t)0x98, (int8_t)0x90, (int8_t)0x88, (int8_t)0x80, (int8_t)0x88, (int8_t)0x0f, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa0, (int8_t)0x98, (int8_t)0x90, (int8_t)0x88, (int8_t)0x80, (int8_t)0x88, (int8_t)0x90, (int8_t)0x17, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa0, (int8_t)0x98, (int8_t)0x90, (int8_t)0x88, (int8_t)0x80, (int8_t)0x88, (int8_t)0x90, (int8_t)0x98, (int8_t)0x1f, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa0, (int8_t)0x98, (int8_t)0x90, (int8_t)0x88, (int8_t)0x80, (int8_t)0x88, (int8_t)0x90, (int8_t)0x98, (int8_t)0xa0, (int8_t)0x27, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa0, (int8_t)0x98, (int8_t)0x90, (int8_t)0x88, (int8_t)0x80, (int8_t)0x88, (int8_t)0x90, (int8_t)0x98, (int8_t)0xa0, (int8_t)0xa8, (int8_t)0x2f, (int8_t)0x37 },
	{ (int8_t)0xc0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0xf8, (int8_t)0xf0, (int8_t)0xe8, (int8_t)0xe0, (int8_t)0xd8, (int8_t)0xd0, (int8_t)0xc8, (int8_t)0xc0, (int8_t)0xb8, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa0, (int8_t)0x98, (int8_t)0x90, (int8_t)0x88, (int8_t)0x80, (int8_t)0x88, (int8_t)0x90, (int8_t)0x98, (int8_t)0xa0, (int8_t)0xa8, (int8_t)0xb0, (int8_t)0x37 },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x81, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x7f },
	{ (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x80, (int8_t)0x80, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x80, (int8_t)0x80, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f, (int8_t)0x7f },
	{ (int8_t)0x80, (int8_t)0x80, (int8_t)0x90, (int8_t)0x98, (int8_t)0xa0, (int8_t)0xa8, (int8_t)0xb0, (int8_t)0xb8, (int8_t)0xc0, (int8_t)0xc8, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8, (int8_t)0x00, (int8_t)0x08, (int8_t)0x10, (int8_t)0x18, (int8_t)0x20, (int8_t)0x28, (int8_t)0x30, (int8_t)0x38, (int8_t)0x40, (int8_t)0x48, (int8_t)0x50, (int8_t)0x58, (int8_t)0x60, (int8_t)0x68, (int8_t)0x70, (int8_t)0x7f },
	{ (int8_t)0x80, (int8_t)0x80, (int8_t)0xa0, (int8_t)0xb0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xe0, (int8_t)0xf0, (int8_t)0x00, (int8_t)0x10, (int8_t)0x20, (int8_t)0x30, (int8_t)0x40, (int8_t)0x50, (int8_t)0x60, (int8_t)0x70, (int8_t)0x45, (int8_t)0x45, (int8_t)0x79, (int8_t)0x7d, (int8_t)0x7a, (int8_t)0x77, (int8_t)0x70, (int8_t)0x66, (int8_t)0x61, (int8_t)0x58, (int8_t)0x53, (int8_t)0x4d, (int8_t)0x2c, (int8_t)0x20, (int8_t)0x18, (int8_t)0x12 },
	{ (int8_t)0x04, (int8_t)0xdb, (int8_t)0xd3, (int8_t)0xcd, (int8_t)0xc6, (int8_t)0xbc, (int8_t)0xb5, (int8_t)0xae, (int8_t)0xa8, (int8_t)0xa3, (int8_t)0x9d, (int8_t)0x99, (int8_t)0x93, (int8_t)0x8e, (int8_t)0x8b, (int8_t)0x8a, (int8_t)0x45, (int8_t)0x45, (int8_t)0x79, (int8_t)0x7d, (int8_t)0x7a, (int8_t)0x77, (int8_t)0x70, (int8_t)0x66, (int8_t)0x5b, (int8_t)0x4b, (int8_t)0x43, (int8_t)0x37, (int8_t)0x2c, (int8_t)0x20, (int8_t)0x18, (int8_t)0x12 },
	{ (int8_t)0x04, (int8_t)0xf8, (int8_t)0xe8, (int8_t)0xdb, (int8_t)0xcf, (int8_t)0xc6, (int8_t)0xbe, (int8_t)0xb0, (int8_t)0xa8, (int8_t)0xa4, (int8_t)0x9e, (int8_t)0x9a, (int8_t)0x95, (int8_t)0x94, (int8_t)0x8d, (int8_t)0x83, (int8_t)0x00, (int8_t)0x00, (int8_t)0x40, (int8_t)0x60, (int8_t)0x7f, (int8_t)0x60, (int8_t)0x40, (int8_t)0x20, (int8_t)0x00, (int8_t)0xe0, (int8_t)0xc0, (int8_t)0xa0, (int8_t)0x80, (int8_t)0xa0, (int8_t)0xc0, (int8_t)0xe0 },
	{ (int8_t)0x00, (int8_t)0x00, (int8_t)0x40, (int8_t)0x60, (int8_t)0x7f, (int8_t)0x60, (int8_t)0x40, (int8_t)0x20, (int8_t)0x00, (int8_t)0xe0, (int8_t)0xc0, (int8_t)0xa0, (int8_t)0x80, (int8_t)0xa0, (int8_t)0xc0, (int8_t)0xe0, (int8_t)0x80, (int8_t)0x80, (int8_t)0x90, (int8_t)0x98, (int8_t)0xa0, (int8_t)0xa8, (int8_t)0xb0, (int8_t)0xb8, (int8_t)0xc0, (int8_t)0xc8, (int8_t)0xd0, (int8_t)0xd8, (int8_t)0xe0, (int8_t)0xe8, (int8_t)0xf0, (int8_t)0xf8 },
	{ (int8_t)0x00, (int8_t)0x08, (int8_t)0x10, (int8_t)0x18, (int8_t)0x20, (int8_t)0x28, (int8_t)0x30, (int8_t)0x38, (int8_t)0x40, (int8_t)0x48, (int8_t)0x50, (int8_t)0x58, (int8_t)0x60, (int8_t)0x68, (int8_t)0x70, (int8_t)0x7f, (int8_t)0x80, (int8_t)0x80, (int8_t)0xa0, (int8_t)0xb0, (int8_t)0xc0, (int8_t)0xd0, (int8_t)0xe0, (int8_t)0xf0, (int8_t)0x00, (int8_t)0x10, (int8_t)0x20, (int8_t)0x30, (int8_t)0x40, (int8_t)0x50, (int8_t)0x60, (int8_t)0x70 }
};

// [=]===^=[ pumatracker_read_u16_be ]============================================================[=]
static uint16_t pumatracker_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ pumatracker_read_u32_be ]============================================================[=]
static uint32_t pumatracker_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ pumatracker_check_mark ]=============================================================[=]
static int32_t pumatracker_check_mark(uint8_t *p, char *mark) {
	return (p[0] == (uint8_t)mark[0]) && (p[1] == (uint8_t)mark[1]) && (p[2] == (uint8_t)mark[2]) && (p[3] == (uint8_t)mark[3]);
}

// [=]===^=[ pumatracker_load_position_list ]=====================================================[=]
static int32_t pumatracker_load_position_list(struct pumatracker_state *s, uint8_t *data, uint32_t data_len, uint32_t *offset, uint32_t num_positions) {
	uint32_t need = num_positions * 14;
	if((*offset + need) > data_len) {
		return 0;
	}

	s->positions = (struct pumatracker_position *)calloc(num_positions, sizeof(struct pumatracker_position));
	if(!s->positions) {
		return 0;
	}
	s->num_positions = num_positions;

	for(uint32_t i = 0; i < num_positions; ++i) {
		uint8_t *p = data + *offset;
		struct pumatracker_position *pos = &s->positions[i];

		for(int32_t j = 0; j < 4; ++j) {
			pos->voice_position[j].track_number         = p[j * 3 + 0];
			pos->voice_position[j].instrument_transpose = (int8_t)p[j * 3 + 1];
			pos->voice_position[j].note_transpose       = (int8_t)p[j * 3 + 2];
		}
		pos->speed = p[12];
		// p[13] skipped

		*offset += 14;
	}

	return 1;
}

// [=]===^=[ pumatracker_load_single_track ]======================================================[=]
static int32_t pumatracker_load_single_track(uint8_t *data, uint32_t data_len, uint32_t *offset, struct pumatracker_track *out) {
	uint32_t capacity = 16;
	uint32_t count = 0;
	struct pumatracker_track_row *rows = (struct pumatracker_track_row *)malloc(capacity * sizeof(struct pumatracker_track_row));
	if(!rows) {
		return 0;
	}

	int32_t row = 0;
	while(row < 32) {
		if((*offset + 4) > data_len) {
			free(rows);
			return 0;
		}
		if(count == capacity) {
			capacity *= 2;
			struct pumatracker_track_row *nrows = (struct pumatracker_track_row *)realloc(rows, capacity * sizeof(struct pumatracker_track_row));
			if(!nrows) {
				free(rows);
				return 0;
			}
			rows = nrows;
		}

		uint8_t b1 = data[*offset + 0];
		uint8_t b2 = data[*offset + 1];
		uint8_t b3 = data[*offset + 2];
		uint8_t b4 = data[*offset + 3];
		*offset += 4;

		struct pumatracker_track_row *r = &rows[count++];
		r->note            = b1;
		r->instrument      = (uint8_t)(b2 & 0x1f);
		r->effect          = (uint8_t)((b2 & 0xe0) >> 5);
		r->effect_argument = b3;
		r->rows_to_wait    = b4;

		row += b4;
	}

	out->rows = rows;
	out->num_rows = count;
	return 1;
}

// [=]===^=[ pumatracker_load_tracks ]============================================================[=]
static int32_t pumatracker_load_tracks(struct pumatracker_state *s, uint8_t *data, uint32_t data_len, uint32_t *offset, uint32_t num_tracks) {
	s->tracks = (struct pumatracker_track *)calloc(num_tracks, sizeof(struct pumatracker_track));
	if(!s->tracks) {
		return 0;
	}
	s->num_tracks = num_tracks;

	for(uint32_t i = 0; i < num_tracks; ++i) {
		if((*offset + 4) > data_len) {
			return 0;
		}
		if(!pumatracker_check_mark(data + *offset, "patt")) {
			return 0;
		}
		*offset += 4;
		if(!pumatracker_load_single_track(data, data_len, offset, &s->tracks[i])) {
			return 0;
		}
	}

	// Trailing "patt" mark
	if((*offset + 4) > data_len) {
		return 0;
	}
	if(!pumatracker_check_mark(data + *offset, "patt")) {
		return 0;
	}
	*offset += 4;
	return 1;
}

// [=]===^=[ pumatracker_load_commands ]==========================================================[=]
static struct pumatracker_instrument_command *pumatracker_load_commands(uint8_t *data, uint32_t data_len, uint32_t *offset, uint32_t *out_count) {
	uint32_t capacity = 16;
	uint32_t count = 0;
	struct pumatracker_instrument_command *cmds = (struct pumatracker_instrument_command *)malloc(capacity * sizeof(struct pumatracker_instrument_command));
	if(!cmds) {
		return 0;
	}

	for(;;) {
		if((*offset + 4) > data_len) {
			free(cmds);
			return 0;
		}

		uint8_t c0 = data[*offset + 0];
		uint8_t c1 = data[*offset + 1];
		uint8_t c2 = data[*offset + 2];
		uint8_t c3 = data[*offset + 3];

		// Vimto fix: missing end mark, "inst" peeked here means the next instrument starts.
		if((c0 == 'i') && (c1 == 'n') && (c2 == 's') && (c3 == 't')) {
			c0 = 0xe0;
			c1 = 0;
			c2 = 0;
			c3 = 0;
		} else {
			*offset += 4;
		}

		if(count == capacity) {
			capacity *= 2;
			struct pumatracker_instrument_command *ncmds = (struct pumatracker_instrument_command *)realloc(cmds, capacity * sizeof(struct pumatracker_instrument_command));
			if(!ncmds) {
				free(cmds);
				return 0;
			}
			cmds = ncmds;
		}

		struct pumatracker_instrument_command *cmd = &cmds[count++];
		cmd->command = c0;
		cmd->argument1 = c1;
		cmd->argument2 = c2;
		cmd->argument3 = c3;

		if((c0 == 0xe0) || (c0 == 0xb0)) {
			break;
		}
	}

	*out_count = count;
	return cmds;
}

// [=]===^=[ pumatracker_load_instruments ]=======================================================[=]
static int32_t pumatracker_load_instruments(struct pumatracker_state *s, uint8_t *data, uint32_t data_len, uint32_t *offset, uint32_t num_instruments) {
	s->instruments = (struct pumatracker_instrument *)calloc(num_instruments, sizeof(struct pumatracker_instrument));
	if(!s->instruments) {
		return 0;
	}
	s->num_instruments = num_instruments;

	for(uint32_t i = 0; i < num_instruments; ++i) {
		if((*offset + 4) > data_len) {
			return 0;
		}
		if(!pumatracker_check_mark(data + *offset, "inst")) {
			return 0;
		}
		*offset += 4;

		s->instruments[i].volume_commands = pumatracker_load_commands(data, data_len, offset, &s->instruments[i].num_volume_commands);
		if(!s->instruments[i].volume_commands) {
			return 0;
		}

		if((*offset + 4) > data_len) {
			return 0;
		}
		if(!pumatracker_check_mark(data + *offset, "insf")) {
			return 0;
		}
		*offset += 4;

		s->instruments[i].frequency_commands = pumatracker_load_commands(data, data_len, offset, &s->instruments[i].num_frequency_commands);
		if(!s->instruments[i].frequency_commands) {
			return 0;
		}
	}

	// Trailing "inst" mark only if there are samples following.
	if(*offset < data_len) {
		if((*offset + 4) > data_len) {
			return 0;
		}
		if(!pumatracker_check_mark(data + *offset, "inst")) {
			return 0;
		}
		// Do not advance past it; the sample data offsets in the header are absolute.
	}

	return 1;
}

// [=]===^=[ pumatracker_load_sample_data ]=======================================================[=]
static int32_t pumatracker_load_sample_data(struct pumatracker_state *s, uint8_t *data, uint32_t data_len, uint32_t *sample_data_offsets, uint16_t *sample_lengths) {
	for(int32_t i = 0; i < PUMATRACKER_NUM_SAMPLES; ++i) {
		s->samples[i] = 0;
		s->sample_lengths[i] = 0;
		if(sample_lengths[i] != 0) {
			// "Because of a bug, the sample length is one word too many" (NostalgicPlayer comment).
			int32_t length = ((int32_t)sample_lengths[i] - 1) * 2;
			if(length <= 0) {
				continue;
			}
			uint32_t off = sample_data_offsets[i];
			if(off >= data_len) {
				continue;
			}
			uint32_t avail = data_len - off;
			// Tolerate up to 48 bytes shortage (matches loader's readBytes < length-48 check).
			if((uint32_t)length > avail) {
				if(((uint32_t)length - avail) > 48) {
					return 0;
				}
				length = (int32_t)avail;
			}
			s->samples[i] = (int8_t *)(data + off);
			s->sample_lengths[i] = (uint32_t)length;
		}
	}
	return 1;
}

// [=]===^=[ pumatracker_build_sample_list ]======================================================[=]
static void pumatracker_build_sample_list(struct pumatracker_state *s) {
	for(int32_t i = 0; i < PUMATRACKER_NUM_SAMPLES; ++i) {
		s->all_samples[i] = s->samples[i];
		s->all_sample_lengths[i] = s->sample_lengths[i];
	}
	for(int32_t i = 0; i < PUMATRACKER_NUM_WAVEFORMS; ++i) {
		s->all_samples[PUMATRACKER_NUM_SAMPLES + i] = pumatracker_waveforms[i];
		s->all_sample_lengths[PUMATRACKER_NUM_SAMPLES + i] = 32;
	}
}

// [=]===^=[ pumatracker_initialize_sound ]=======================================================[=]
static void pumatracker_initialize_sound(struct pumatracker_state *s) {
	s->current_speed = 3;
	s->speed_counter = 3;
	s->current_position = -1;
	s->current_row_number = 32;

	for(int32_t i = 0; i < 4; ++i) {
		memset(&s->voices[i], 0, sizeof(s->voices[i]));
		s->voices[i].row_counter = 1;
	}
}

// [=]===^=[ pumatracker_setup_empty ]============================================================[=]
static void pumatracker_setup_empty(struct pumatracker_state *s) {
	s->empty_track_row.note = 0;
	s->empty_track_row.instrument = 0;
	s->empty_track_row.effect = PUMATRACKER_FX_NONE;
	s->empty_track_row.effect_argument = 0;
	s->empty_track_row.rows_to_wait = 32;
	s->empty_track.rows = &s->empty_track_row;
	s->empty_track.num_rows = 1;

	s->empty_volume_cmd.command = 0xe0;
	s->empty_volume_cmd.argument1 = 0;
	s->empty_volume_cmd.argument2 = 0;
	s->empty_volume_cmd.argument3 = 0;
	s->empty_frequency_cmd.command = 0xe0;
	s->empty_frequency_cmd.argument1 = 0;
	s->empty_frequency_cmd.argument2 = 0;
	s->empty_frequency_cmd.argument3 = 0;
	s->empty_instrument.volume_commands = &s->empty_volume_cmd;
	s->empty_instrument.num_volume_commands = 1;
	s->empty_instrument.frequency_commands = &s->empty_frequency_cmd;
	s->empty_instrument.num_frequency_commands = 1;
}

// [=]===^=[ pumatracker_get_next_position ]======================================================[=]
static void pumatracker_get_next_position(struct pumatracker_state *s) {
	s->current_row_number = 0;
	struct pumatracker_position *pos = &s->positions[s->current_position];

	for(int32_t i = 0; i < 4; ++i) {
		struct pumatracker_voice_info *v = &s->voices[i];
		struct pumatracker_voice_position *vp = &pos->voice_position[i];
		uint8_t track_number = vp->track_number;

		if(track_number >= s->num_tracks) {
			v->track = &s->empty_track;
		} else {
			v->track = &s->tracks[track_number];
		}
		v->track_position = -1;
		v->instrument_transpose = vp->instrument_transpose;
		v->note_transpose = vp->note_transpose;
		v->row_counter = 1;
	}

	if(pos->speed != 0) {
		s->current_speed = pos->speed;
	}
}

// [=]===^=[ pumatracker_get_next_track_info ]====================================================[=]
static void pumatracker_get_next_track_info(struct pumatracker_state *s, int32_t voice_idx) {
	struct pumatracker_voice_info *v = &s->voices[voice_idx];
	if(v->track_position < 0 || (uint32_t)v->track_position >= v->track->num_rows) {
		// Defensive: empty/short track. Hold and reset the row counter to track's full length.
		v->row_counter = 32;
		return;
	}
	struct pumatracker_track_row *row = &v->track->rows[v->track_position];

	uint8_t note = row->note;
	if(note != 0) {
		paula_mute(&s->paula, voice_idx);

		v->transposed_note = (uint8_t)(note + v->note_transpose);

		int32_t instr_number = (int32_t)row->instrument + (int32_t)v->instrument_transpose;
		struct pumatracker_instrument *instr;
		if(instr_number == 0) {
			instr = &s->empty_instrument;
		} else {
			int32_t idx = (int32_t)row->instrument + (int32_t)v->instrument_transpose - 1;
			if(idx < 0 || (uint32_t)idx >= s->num_instruments) {
				instr = &s->empty_instrument;
			} else {
				instr = &s->instruments[idx];
			}
		}

		v->volume_commands = instr->volume_commands;
		v->num_volume_commands = instr->num_volume_commands;
		v->frequency_commands = instr->frequency_commands;
		v->num_frequency_commands = instr->num_frequency_commands;

		v->voice_flag = PUMATRACKER_VF_SET_VOLUME_SLIDE | PUMATRACKER_VF_SET_FREQUENCY | PUMATRACKER_VF_VOICE_RUNNING;
		v->volume_command_position = 0;
		v->frequency_command_position = 0;
	}

	v->row_counter = row->rows_to_wait;

	switch(row->effect) {
		case PUMATRACKER_FX_NONE: {
			v->volume = 64;
			v->portamento_add_value = 0;
			break;
		}
		case PUMATRACKER_FX_SET_VOLUME: {
			v->volume = row->effect_argument;
			v->portamento_add_value = 0;
			break;
		}
		case PUMATRACKER_FX_PORTAMENTO_DOWN: {
			v->volume = 64;
			v->portamento_add_value = (int16_t)row->effect_argument;
			break;
		}
		case PUMATRACKER_FX_PORTAMENTO_UP: {
			v->volume = 64;
			v->portamento_add_value = (int16_t)(-(int32_t)row->effect_argument);
			break;
		}
	}
}

// [=]===^=[ pumatracker_do_volume_slide ]========================================================[=]
static int32_t pumatracker_do_volume_slide(struct pumatracker_voice_info *v, struct pumatracker_instrument_command *cmd) {
	int32_t has_flag = (v->voice_flag & PUMATRACKER_VF_SET_VOLUME_SLIDE) != 0;
	v->voice_flag = (uint8_t)(v->voice_flag & ~PUMATRACKER_VF_SET_VOLUME_SLIDE);

	if(has_flag) {
		uint8_t volume_begin = cmd->argument1;
		uint8_t volume_end = cmd->argument2;

		v->volume_slide_counter = (uint8_t)(cmd->argument3 + 1);
		v->volume_slide_value = (int16_t)volume_begin;
		v->volume_slide_remaining_time = 0;
		v->volume_slide_direction = 1;

		int32_t delta = (int32_t)volume_end - (int32_t)volume_begin;
		if(delta < 0) {
			delta = -delta;
			v->volume_slide_direction = -1;
		}
		v->volume_slide_delta = (int8_t)delta;
	} else {
		v->volume_slide_counter--;
		if(v->volume_slide_counter == 0) {
			v->volume_command_position++;
			v->voice_flag = (uint8_t)(v->voice_flag | PUMATRACKER_VF_SET_VOLUME_SLIDE);
			return 1;
		}

		uint8_t time = cmd->argument3;
		int32_t time_count = ((int32_t)v->volume_slide_remaining_time + (int32_t)v->volume_slide_delta) - (int32_t)time;

		if(time_count >= 0) {
			int16_t value_to_add = 0;
			do {
				value_to_add = (int16_t)(value_to_add + v->volume_slide_direction);
				time_count -= (int32_t)time;
			} while(time_count >= 0);
			v->volume_slide_value = (int16_t)(v->volume_slide_value + value_to_add);
		}

		time_count += (int32_t)time;
		v->volume_slide_remaining_time = (uint8_t)time_count;
	}

	return 0;
}

// [=]===^=[ pumatracker_do_set_sample ]==========================================================[=]
static void pumatracker_do_set_sample(struct pumatracker_state *s, struct pumatracker_voice_info *v, struct pumatracker_instrument_command *cmd) {
	uint8_t sample_number = cmd->argument1;

	v->waveform_add_value = (int8_t)cmd->argument2;
	v->waveform_value = (int8_t)cmd->argument2;
	v->waveform_change_counter = cmd->argument3;

	uint32_t sample_length = 0;
	int8_t *sample_data = 0;
	if(sample_number < PUMATRACKER_TOTAL_SAMPLES) {
		sample_data = s->all_samples[sample_number];
		sample_length = s->all_sample_lengths[sample_number];
	}

	if(sample_length > 0xffff) {
		sample_length = 0xffff;
	}
	v->sample_length = (uint16_t)sample_length;

	if(sample_length >= 80 * 2) {
		v->voice_flag = (uint8_t)(v->voice_flag | PUMATRACKER_VF_NO_LOOP);
	}

	v->sample_data = sample_data;
	v->sample_number = sample_number;

	v->volume_command_position++;
	v->voice_flag = (uint8_t)(v->voice_flag | PUMATRACKER_VF_SET_VOLUME_SLIDE);
}

// [=]===^=[ pumatracker_do_volume_commands ]=====================================================[=]
static void pumatracker_do_volume_commands(struct pumatracker_state *s, struct pumatracker_voice_info *v) {
	int32_t one_more;
	do {
		one_more = 0;
		if(v->num_volume_commands == 0 || v->volume_command_position >= v->num_volume_commands) {
			v->volume_slide_value = 0;
			v->voice_flag = (uint8_t)(v->voice_flag & ~PUMATRACKER_VF_VOICE_RUNNING);
			break;
		}
		struct pumatracker_instrument_command *cmd = &v->volume_commands[v->volume_command_position];

		switch(cmd->command) {
			case 0xa0: {
				one_more = pumatracker_do_volume_slide(v, cmd);
				break;
			}
			case 0xc0: {
				pumatracker_do_set_sample(s, v, cmd);
				one_more = 1;
				break;
			}
			case 0xb0: {
				v->volume_command_position = (uint8_t)(cmd->argument1 / 4);
				one_more = 1;
				break;
			}
			default: {
				v->volume_slide_value = 0;
				v->voice_flag = (uint8_t)(v->voice_flag & ~PUMATRACKER_VF_VOICE_RUNNING);
				break;
			}
		}
	} while(one_more);
}

// [=]===^=[ pumatracker_do_vary_frequency ]======================================================[=]
static int32_t pumatracker_do_vary_frequency(struct pumatracker_voice_info *v, struct pumatracker_instrument_command *cmd) {
	int32_t has_flag = (v->voice_flag & PUMATRACKER_VF_SET_FREQUENCY) != 0;
	v->voice_flag = (uint8_t)(v->voice_flag & ~PUMATRACKER_VF_SET_FREQUENCY);

	if(has_flag) {
		int16_t begin_add = (int16_t)(int8_t)cmd->argument1;
		int16_t end_add = (int16_t)(int8_t)cmd->argument2;

		v->frequency_counter = cmd->argument3;
		uint32_t period_idx = (uint32_t)v->transposed_note / 2;
		uint16_t period = (period_idx < (sizeof(pumatracker_periods) / sizeof(pumatracker_periods[0]))) ? pumatracker_periods[period_idx] : 0;

		begin_add = (int16_t)(begin_add + (int16_t)period);
		end_add = (int16_t)(end_add + (int16_t)period);

		v->period = (uint16_t)begin_add;
		v->frequency_vary_value = (int32_t)begin_add << 16;

		int16_t delta = (int16_t)(end_add - begin_add);
		if(v->frequency_counter == 0) {
			v->frequency_vary_add_value = 0;
		} else {
			int32_t add_value = ((int32_t)delta * 256) / (int32_t)v->frequency_counter;
			if(add_value < 65536 && add_value > -65536) {
				v->frequency_vary_add_value = add_value << 8;
			} else {
				add_value = (int32_t)delta / (int32_t)v->frequency_counter;
				v->frequency_vary_add_value = add_value << 16;
			}
		}
	} else {
		v->frequency_counter--;
		if(v->frequency_counter == 0) {
			v->frequency_command_position++;
			v->voice_flag = (uint8_t)(v->voice_flag | PUMATRACKER_VF_SET_FREQUENCY);
			return 1;
		}
		v->frequency_vary_value += v->frequency_vary_add_value;
		v->period = (uint16_t)(v->frequency_vary_value >> 16);
	}

	return 0;
}

// [=]===^=[ pumatracker_do_constant_frequency ]==================================================[=]
static int32_t pumatracker_do_constant_frequency(struct pumatracker_voice_info *v, struct pumatracker_instrument_command *cmd) {
	int32_t has_flag = (v->voice_flag & PUMATRACKER_VF_SET_FREQUENCY) != 0;
	v->voice_flag = (uint8_t)(v->voice_flag & ~PUMATRACKER_VF_SET_FREQUENCY);

	if(has_flag) {
		v->frequency_counter = cmd->argument3;
		int32_t index = ((int32_t)(int8_t)cmd->argument1 + (int32_t)v->transposed_note) / 2;
		int32_t max_idx = (int32_t)(sizeof(pumatracker_periods) / sizeof(pumatracker_periods[0])) - 1;
		if(index < 0) {
			index = 0;
		}
		if(index > max_idx) {
			index = max_idx;
		}
		v->period = pumatracker_periods[index];
	} else {
		v->frequency_counter--;
		if(v->frequency_counter == 0) {
			v->voice_flag = (uint8_t)(v->voice_flag | PUMATRACKER_VF_SET_FREQUENCY);
			v->frequency_command_position++;
			return 1;
		}
	}

	return 0;
}

// [=]===^=[ pumatracker_do_frequency_commands ]==================================================[=]
static void pumatracker_do_frequency_commands(struct pumatracker_voice_info *v) {
	int32_t one_more;
	do {
		one_more = 0;
		if(v->num_frequency_commands == 0 || v->frequency_command_position >= v->num_frequency_commands) {
			break;
		}
		struct pumatracker_instrument_command *cmd = &v->frequency_commands[v->frequency_command_position];

		switch(cmd->command) {
			case 0xa0: {
				one_more = pumatracker_do_vary_frequency(v, cmd);
				break;
			}
			case 0xd0: {
				one_more = pumatracker_do_constant_frequency(v, cmd);
				break;
			}
			case 0xb0: {
				v->frequency_command_position = (uint8_t)(cmd->argument1 / 4);
				one_more = 1;
				break;
			}
			default: break;
		}
	} while(one_more);
}

// [=]===^=[ pumatracker_setup_hardware ]=========================================================[=]
static void pumatracker_setup_hardware(struct pumatracker_state *s, int32_t voice_idx) {
	struct pumatracker_voice_info *v = &s->voices[voice_idx];

	v->period = (uint16_t)((int32_t)v->period + (int32_t)v->portamento_add_value);

	int8_t *sample_data = v->sample_data;
	uint32_t sample_length_full = v->sample_length;

	if(v->waveform_add_value != 0) {
		if((v->waveform_value <= 0) || (v->waveform_value >= (int8_t)v->waveform_change_counter)) {
			v->waveform_add_value = (int8_t)(-(int32_t)v->waveform_add_value);
		}
		v->waveform_value = (int8_t)((int32_t)v->waveform_value + (int32_t)v->waveform_add_value);

		int32_t idx = (int32_t)v->sample_number + (int32_t)v->waveform_value;
		if(idx >= 0 && idx < PUMATRACKER_TOTAL_SAMPLES) {
			sample_data = s->all_samples[idx];
			sample_length_full = s->all_sample_lengths[idx];
		} else {
			sample_data = 0;
			sample_length_full = 0;
		}
	}

	if(sample_data != 0) {
		if(v->sample_length > 2) {
			uint32_t play_len = v->sample_length;
			if(play_len > sample_length_full) {
				play_len = sample_length_full;
			}

			paula_queue_sample(&s->paula, voice_idx, sample_data, 0, play_len);
			paula_set_loop(&s->paula, voice_idx, 0, play_len);

			int32_t has_flag = (v->voice_flag & PUMATRACKER_VF_NO_LOOP) != 0;
			v->voice_flag = (uint8_t)(v->voice_flag & ~PUMATRACKER_VF_NO_LOOP);

			if(has_flag) {
				// One-shot: clear the loop so playback ends after the buffer.
				paula_set_loop(&s->paula, voice_idx, 0, 0);
				v->sample_length = 2;
			}
		}

		paula_set_period(&s->paula, voice_idx, v->period);

		int32_t volume = (int32_t)v->volume_slide_value + (int32_t)v->volume - 64;
		if(volume < 0) {
			volume = 0;
		}
		paula_set_volume(&s->paula, voice_idx, (uint16_t)volume);
	} else {
		paula_mute(&s->paula, voice_idx);
	}
}

// [=]===^=[ pumatracker_effect_handler ]=========================================================[=]
static void pumatracker_effect_handler(struct pumatracker_state *s, int32_t voice_idx) {
	struct pumatracker_voice_info *v = &s->voices[voice_idx];
	pumatracker_do_volume_commands(s, v);
	pumatracker_do_frequency_commands(v);
	pumatracker_setup_hardware(s, voice_idx);
}

// [=]===^=[ pumatracker_tick ]===================================================================[=]
static void pumatracker_tick(struct pumatracker_state *s) {
	if(s->current_row_number == 32) {
		s->current_position++;
		if((uint32_t)s->current_position == s->num_positions) {
			s->current_position = 0;
		}
		pumatracker_get_next_position(s);
	}

	s->speed_counter--;
	if(s->speed_counter == 0) {
		s->current_row_number++;
		s->speed_counter = s->current_speed;

		for(int32_t i = 0; i < 4; ++i) {
			struct pumatracker_voice_info *v = &s->voices[i];
			v->row_counter--;
			if(v->row_counter == 0) {
				v->track_position++;
				pumatracker_get_next_track_info(s, i);
			}
		}
	}

	for(int32_t i = 0; i < 4; ++i) {
		if((s->voices[i].voice_flag & PUMATRACKER_VF_VOICE_RUNNING) != 0) {
			pumatracker_effect_handler(s, i);
		}
	}
}

// [=]===^=[ pumatracker_cleanup ]================================================================[=]
static void pumatracker_cleanup(struct pumatracker_state *s) {
	if(!s) {
		return;
	}
	if(s->positions) {
		free(s->positions);
		s->positions = 0;
	}
	if(s->tracks) {
		for(uint32_t i = 0; i < s->num_tracks; ++i) {
			free(s->tracks[i].rows);
		}
		free(s->tracks);
		s->tracks = 0;
	}
	if(s->instruments) {
		for(uint32_t i = 0; i < s->num_instruments; ++i) {
			free(s->instruments[i].volume_commands);
			free(s->instruments[i].frequency_commands);
		}
		free(s->instruments);
		s->instruments = 0;
	}
}

// [=]===^=[ pumatracker_init ]===================================================================[=]
static struct pumatracker_state *pumatracker_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 80 || sample_rate < 8000) {
		return 0;
	}

	uint8_t *buf = (uint8_t *)data;

	// Validate first track mark (12-byte name, then header words, then sample table).
	uint16_t check_num_tracks = pumatracker_read_u16_be(buf + 12);
	uint32_t first_track_off = ((uint32_t)check_num_tracks + 1) * 14 + 80;
	if((first_track_off + 4) > len) {
		return 0;
	}
	if(!pumatracker_check_mark(buf + first_track_off, "patt")) {
		return 0;
	}

	struct pumatracker_state *s = (struct pumatracker_state *)calloc(1, sizeof(struct pumatracker_state));
	if(!s) {
		return 0;
	}

	pumatracker_setup_empty(s);

	uint32_t offset = 0;
	// 12-byte song name (skipped; not needed for playback).
	offset += 12;

	uint32_t num_positions = (uint32_t)pumatracker_read_u16_be(buf + offset) + 1;
	offset += 2;
	uint32_t num_tracks = (uint32_t)pumatracker_read_u16_be(buf + offset);
	offset += 2;
	uint32_t num_instruments = (uint32_t)pumatracker_read_u16_be(buf + offset);
	offset += 2;
	offset += 2;  // skip 2 bytes

	uint32_t sample_data_offsets[PUMATRACKER_NUM_SAMPLES];
	uint16_t sample_lengths[PUMATRACKER_NUM_SAMPLES];

	if((offset + 10 * 4 + 10 * 2) > len) {
		free(s);
		return 0;
	}
	for(int32_t i = 0; i < PUMATRACKER_NUM_SAMPLES; ++i) {
		sample_data_offsets[i] = pumatracker_read_u32_be(buf + offset);
		offset += 4;
	}
	for(int32_t i = 0; i < PUMATRACKER_NUM_SAMPLES; ++i) {
		sample_lengths[i] = pumatracker_read_u16_be(buf + offset);
		offset += 2;
	}

	if(!pumatracker_load_position_list(s, buf, len, &offset, num_positions)) {
		goto fail;
	}
	if(!pumatracker_load_tracks(s, buf, len, &offset, num_tracks)) {
		goto fail;
	}
	if(!pumatracker_load_instruments(s, buf, len, &offset, num_instruments)) {
		goto fail;
	}
	if(!pumatracker_load_sample_data(s, buf, len, sample_data_offsets, sample_lengths)) {
		goto fail;
	}

	pumatracker_build_sample_list(s);
	paula_init(&s->paula, sample_rate, PUMATRACKER_TICK_HZ);
	pumatracker_initialize_sound(s);
	return s;

fail:
	pumatracker_cleanup(s);
	free(s);
	return 0;
}

// [=]===^=[ pumatracker_free ]===================================================================[=]
static void pumatracker_free(struct pumatracker_state *s) {
	if(!s) {
		return;
	}
	pumatracker_cleanup(s);
	free(s);
}

// [=]===^=[ pumatracker_get_audio ]==============================================================[=]
static void pumatracker_get_audio(struct pumatracker_state *s, int16_t *output, int32_t frames) {
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
			pumatracker_tick(s);
		}
	}
}

// [=]===^=[ pumatracker_api_init ]===============================================================[=]
static void *pumatracker_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return pumatracker_init(data, len, sample_rate);
}

// [=]===^=[ pumatracker_api_free ]===============================================================[=]
static void pumatracker_api_free(void *state) {
	pumatracker_free((struct pumatracker_state *)state);
}

// [=]===^=[ pumatracker_api_get_audio ]==========================================================[=]
static void pumatracker_api_get_audio(void *state, int16_t *output, int32_t frames) {
	pumatracker_get_audio((struct pumatracker_state *)state, output, frames);
}

static const char *pumatracker_extensions[] = { "puma", 0 };

static struct player_api pumatracker_api = {
	"PumaTracker",
	pumatracker_extensions,
	pumatracker_api_init,
	pumatracker_api_free,
	pumatracker_api_get_audio,
	0,
};
