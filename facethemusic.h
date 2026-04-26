// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Face The Music replayer, ported from NostalgicPlayer's C# implementation.
// Drives an 8-channel Amiga Paula (see paula.h). Variable CIA-based tick rate.
//
// Public API:
//   struct facethemusic_state *facethemusic_init(void *data, uint32_t len, int32_t sample_rate);
//   void facethemusic_free(struct facethemusic_state *s);
//   void facethemusic_get_audio(struct facethemusic_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "paula.h"
#include "player_api.h"

#define FTM_MAX_SAMPLES        64
#define FTM_MAX_SCRIPTS        64
#define FTM_MAX_LOOP_STACK     16
#define FTM_NUM_TRACKS         8
#define FTM_NUM_LFO            4
#define FTM_PAL_CLOCK          3546895U
#define FTM_CIA_BASE           709379U

// Track effect opcodes
#define FTM_TE_NONE            0
#define FTM_TE_VOLUME0         1
#define FTM_TE_VOLUME1         2
#define FTM_TE_VOLUME2         3
#define FTM_TE_VOLUME3         4
#define FTM_TE_VOLUME4         5
#define FTM_TE_VOLUME5         6
#define FTM_TE_VOLUME6         7
#define FTM_TE_VOLUME7         8
#define FTM_TE_VOLUME8         9
#define FTM_TE_VOLUME9         10
#define FTM_TE_SEL             11
#define FTM_TE_PORTAMENTO      12
#define FTM_TE_VOLUME_DOWN     13
#define FTM_TE_PATTERN_LOOP    14
#define FTM_TE_SKIP_EMPTY_ROWS 15

// Sound effect (SEL) opcodes
#define FTM_SE_NOTHING                  0
#define FTM_SE_WAIT                     1
#define FTM_SE_GOTO                     2
#define FTM_SE_LOOP                     3
#define FTM_SE_GOTO_SCRIPT              4
#define FTM_SE_END                      5
#define FTM_SE_IF_PITCH_EQ              6
#define FTM_SE_IF_PITCH_LT              7
#define FTM_SE_IF_PITCH_GT              8
#define FTM_SE_IF_VOLUME_EQ             9
#define FTM_SE_IF_VOLUME_LT             10
#define FTM_SE_IF_VOLUME_GT             11
#define FTM_SE_ON_NEW_PITCH             12
#define FTM_SE_ON_NEW_VOLUME            13
#define FTM_SE_ON_NEW_SAMPLE            14
#define FTM_SE_ON_RELEASE               15
#define FTM_SE_ON_PORTAMENTO            16
#define FTM_SE_ON_VOLUME_DOWN           17
#define FTM_SE_PLAY_CURRENT_SAMPLE      18
#define FTM_SE_PLAY_QUIET_SAMPLE        19
#define FTM_SE_PLAY_POSITION            20
#define FTM_SE_PLAY_POSITION_ADD        21
#define FTM_SE_PLAY_POSITION_SUB        22
#define FTM_SE_PITCH                    23
#define FTM_SE_DETUNE                   24
#define FTM_SE_DETUNE_PITCH_ADD         25
#define FTM_SE_DETUNE_PITCH_SUB         26
#define FTM_SE_VOLUME                   27
#define FTM_SE_VOLUME_ADD               28
#define FTM_SE_VOLUME_SUB               29
#define FTM_SE_CURRENT_SAMPLE           30
#define FTM_SE_SAMPLE_START             31
#define FTM_SE_SAMPLE_START_ADD         32
#define FTM_SE_SAMPLE_START_SUB         33
#define FTM_SE_ONESHOT_LENGTH           34
#define FTM_SE_ONESHOT_LENGTH_ADD       35
#define FTM_SE_ONESHOT_LENGTH_SUB       36
#define FTM_SE_REPEAT_LENGTH            37
#define FTM_SE_REPEAT_LENGTH_ADD        38
#define FTM_SE_REPEAT_LENGTH_SUB        39
#define FTM_SE_GET_PITCH_OF_TRACK       40
#define FTM_SE_GET_VOLUME_OF_TRACK      41
#define FTM_SE_GET_SAMPLE_OF_TRACK      42
#define FTM_SE_CLONE_TRACK              43
#define FTM_SE_FIRST_LFO_START          44
#define FTM_SE_FIRST_LFO_SD_ADD         45
#define FTM_SE_FIRST_LFO_SD_SUB         46
#define FTM_SE_SECOND_LFO_START         47
#define FTM_SE_SECOND_LFO_SD_ADD        48
#define FTM_SE_SECOND_LFO_SD_SUB        49
#define FTM_SE_THIRD_LFO_START          50
#define FTM_SE_THIRD_LFO_SD_ADD         51
#define FTM_SE_THIRD_LFO_SD_SUB         52
#define FTM_SE_FOURTH_LFO_START         53
#define FTM_SE_FOURTH_LFO_SD_ADD        54
#define FTM_SE_FOURTH_LFO_SD_SUB        55
#define FTM_SE_WORK_ON_TRACK            56
#define FTM_SE_WORK_TRACK_ADD           57
#define FTM_SE_GLOBAL_VOLUME            58
#define FTM_SE_GLOBAL_SPEED             59
#define FTM_SE_TICKS_PER_LINE           60
#define FTM_SE_JUMP_TO_SONG_LINE        61

// LFO targets
#define FTM_LFO_NOTHING         0
#define FTM_LFO_LFO1_SPEED      1
#define FTM_LFO_LFO2_SPEED      2
#define FTM_LFO_LFO3_SPEED      3
#define FTM_LFO_LFO4_SPEED      4
#define FTM_LFO_LFO1_DEPTH      5
#define FTM_LFO_LFO2_DEPTH      6
#define FTM_LFO_LFO3_DEPTH      7
#define FTM_LFO_LFO4_DEPTH      8
#define FTM_LFO_TRACK_AMPLITUDE 10
#define FTM_LFO_TRACK_FREQUENCY 15

struct ftm_sample {
	uint8_t in_use;
	int8_t *sample_data;
	uint16_t oneshot_length;	// in words
	uint32_t loop_start;		// in bytes
	uint16_t loop_length;		// in words
	uint32_t total_length;		// in bytes
};

struct ftm_track_line {
	uint8_t effect;
	uint8_t note;
	uint16_t effect_argument;
};

struct ftm_track {
	uint16_t default_spacing;
	struct ftm_track_line *lines;
	uint32_t num_lines;
};

struct ftm_sound_effect_line {
	uint8_t effect;
	uint8_t argument1;
	uint16_t argument2;
};

struct ftm_sound_effect_script {
	uint8_t in_use;
	struct ftm_sound_effect_line *lines;
	uint32_t num_lines;
};

struct ftm_pattern_loop_info {
	uint32_t track_position;
	uint16_t loop_start_position;
	int16_t loop_count;
	int16_t original_loop_count;
};

struct ftm_lfo_state {
	uint8_t target;
	uint8_t loop_modulation;
	int8_t *shape_table;
	int32_t shape_table_position;
	uint16_t modulation_speed;
	uint16_t modulation_depth;
	int16_t modulation_value;
};

struct ftm_sound_effect_state {
	int32_t script_index;
	int32_t script_position;
	uint16_t wait_counter;
	uint16_t loop_counter;
	int32_t voice_index;
	uint16_t new_pitch_goto;
	uint16_t new_volume_goto;
	uint16_t new_sample_goto;
	uint16_t release_goto;
	uint16_t portamento_goto;
	uint16_t volume_down_goto;
	uint16_t interrupt_line_number;
};

struct ftm_voice {
	int32_t channel_number;

	int32_t sample_number;	// -1 if none
	int8_t *sample_data;
	uint32_t sample_start_offset;
	uint32_t sample_calculate_offset;
	uint16_t sample_oneshot_length;
	uint32_t sample_loop_start;
	uint16_t sample_loop_length;
	uint32_t sample_total_length;
	int32_t current_sample_index;	// index into samples[] used by SEL clone-of-track etc

	uint16_t volume;
	uint16_t note_index;
	uint8_t retrig_sample;

	int32_t detune_index;

	uint32_t track_position;
	int16_t rows_left_to_skip;

	int16_t portamento_ticks;
	uint32_t portamento_note;
	uint16_t portamento_end_note;

	uint32_t volume_down_volume;
	uint16_t volume_down_speed;

	struct ftm_sound_effect_state sel_state;
	struct ftm_lfo_state lfo_states[FTM_NUM_LFO];
};

struct facethemusic_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint16_t number_of_measures;
	uint8_t rows_per_measure;
	uint16_t start_cia_timer_value;
	uint8_t start_speed;
	uint8_t channel_mute_status;
	uint8_t global_volume_init;
	uint8_t flag;
	uint8_t number_of_samples;

	struct ftm_sample samples[FTM_MAX_SAMPLES];
	int8_t quiet_sample[4];

	struct ftm_sound_effect_script scripts[FTM_MAX_SCRIPTS];

	struct ftm_track tracks[FTM_NUM_TRACKS];

	int32_t channel_mapping[FTM_NUM_TRACKS];

	// Global playing info
	uint16_t start_row;
	uint16_t end_row;
	uint8_t global_volume;
	uint16_t speed;
	uint16_t speed_counter;
	uint16_t current_row;
	uint16_t pattern_loop_stop_row;
	uint16_t pattern_loop_start_row;
	uint8_t do_pattern_loop;
	struct ftm_pattern_loop_info pattern_loop_stack[FTM_MAX_LOOP_STACK];
	int32_t pattern_loop_stack_count;
	int16_t detune_values[4];
	uint16_t cia_timer_value;

	struct ftm_voice voices[FTM_NUM_TRACKS];

	uint8_t end_reached;
};

// [=]===^=[ ftm_pan_pos ]========================================================================[=]
static uint8_t ftm_pan_pos[FTM_NUM_TRACKS] = { 0, 0, 127, 127, 127, 127, 0, 0 };

// [=]===^=[ ftm_effect_volume ]==================================================================[=]
static uint8_t ftm_effect_volume[10] = { 0, 7, 14, 21, 28, 36, 43, 50, 57, 64 };

// [=]===^=[ ftm_periods ]========================================================================[=]
static uint16_t ftm_periods[34 * 8] = {
	855, 849, 843, 837, 831, 825, 819, 813,
	807, 801, 796, 790, 784, 779, 773, 767,
	762, 756, 751, 746, 740, 735, 730, 724,
	719, 714, 709, 704, 699, 694, 689, 684,
	679, 674, 669, 664, 659, 655, 650, 645,
	641, 636, 631, 627, 622, 618, 613, 609,
	605, 600, 596, 592, 587, 583, 579, 575,
	571, 567, 563, 558, 554, 550, 547, 543,
	539, 535, 531, 527, 523, 520, 516, 512,
	508, 505, 501, 498, 494, 490, 487, 483,
	480, 476, 473, 470, 466, 463, 460, 456,
	453, 450, 446, 443, 440, 437, 434, 431,

	428, 424, 421, 418, 415, 412, 409, 406,
	404, 401, 398, 395, 392, 389, 386, 384,
	381, 378, 375, 373, 370, 367, 365, 362,
	360, 357, 354, 352, 349, 347, 344, 342,
	339, 337, 334, 332, 330, 327, 325, 323,
	320, 318, 316, 313, 311, 309, 307, 305,
	302, 300, 298, 296, 294, 292, 290, 287,
	285, 283, 281, 279, 277, 275, 273, 271,
	269, 267, 265, 264, 262, 260, 258, 256,
	254, 252, 251, 249, 247, 245, 243, 242,
	240, 238, 237, 235, 233, 231, 230, 228,
	226, 225, 223, 222, 220, 218, 217, 215,

	214, 212, 211, 209, 208, 206, 205, 203,
	202, 200, 199, 197, 196, 195, 193, 192,
	190, 189, 188, 186, 185, 184, 182, 181,
	180, 178, 177, 176, 175, 173, 172, 171,
	170, 168, 167, 166, 165, 164, 162, 161,
	160, 159, 158, 157, 156, 154, 153, 152,
	151, 150, 149, 148, 147, 146, 145, 144,
	143, 142, 141, 140, 139, 138, 137, 136,
	135, 134, 133, 132, 131, 130, 129, 128,
	127, 126, 125, 124, 123, 123, 122, 121
};

// [=]===^=[ ftm_lfo_sine ]=======================================================================[=]
static int8_t ftm_lfo_sine[192] = {
	   0,    4,    8,   12,   16,   20,   24,   28,   33,   37,   41,   44,   48,   52,   56,   60,
	  63,   67,   70,   74,   77,   80,   84,   87,   90,   93,   95,   98,  101,  103,  105,  108,
	 110,  112,  114,  115,  117,  119,  120,  121,  122,  123,  124,  125,  126,  126,  126,  126,
	 126,  126,  126,  126,  125,  125,  124,  123,  122,  121,  119,  118,  116,  115,  113,  111,
	 109,  107,  104,  102,   99,   97,   94,   91,   88,   85,   82,   79,   75,   72,   69,   65,
	  61,   58,   54,   50,   46,   43,   39,   35,   31,   26,   22,   18,   14,   10,    6,    2,
	  -2,   -6,  -10,  -14,  -18,  -22,  -26,  -31,  -35,  -39,  -43,  -46,  -50,  -54,  -58,  -61,
	 -65,  -69,  -72,  -75,  -79,  -82,  -85,  -88,  -91,  -94,  -97,  -99, -102, -104, -107, -109,
	-111, -113, -115, -116, -118, -119, -121, -122, -123, -124, -125, -125, -126, -126, -126, -126,
	-126, -126, -126, -126, -125, -124, -123, -122, -121, -120, -119, -117, -115, -114, -112, -110,
	-108, -105, -103, -101,  -98,  -95,  -93,  -90,  -87,  -84,  -80,  -77,  -74,  -70,  -67,  -63,
	 -60,  -56,  -52,  -48,  -44,  -41,  -37,  -33,  -28,  -24,  -20,  -16,  -12,   -8,   -4,    0
};

// [=]===^=[ ftm_lfo_square ]=====================================================================[=]
static int8_t ftm_lfo_square[192] = {
	 127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,
	 127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,
	 127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,
	 127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,
	 127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,
	 127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,
	-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
	-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
	-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
	-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
	-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
	-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128
};

// [=]===^=[ ftm_lfo_triangle ]===================================================================[=]
static int8_t ftm_lfo_triangle[192] = {
	   2,    5,    8,   10,   13,   16,   18,   21,   23,   26,   29,   31,   34,   37,   39,   42,
	  45,   48,   50,   53,   56,   58,   61,   64,   66,   69,   72,   74,   77,   79,   82,   85,
	  87,   90,   93,   95,   98,  101,  103,  106,  109,  111,  114,  117,  119,  122,  125,  127,
	 127,  125,  122,  119,  117,  114,  111,  109,  106,  103,  101,   98,   95,   93,   90,   87,
	  85,   82,   79,   77,   74,   71,   69,   66,   64,   61,   58,   56,   53,   50,   47,   45,
	  42,   39,   37,   34,   31,   29,   26,   23,   21,   18,   15,   13,   10,    7,    5,    2,
	   0,   -2,   -5,   -8,  -10,  -13,  -16,  -18,  -21,  -24,  -26,  -29,  -32,  -34,  -37,  -40,
	 -42,  -45,  -48,  -50,  -53,  -56,  -58,  -61,  -64,  -66,  -69,  -72,  -74,  -77,  -80,  -82,
	 -85,  -87,  -90,  -93,  -95,  -98, -101, -103, -106, -109, -111, -114, -117, -119, -122, -125,
	-127, -127, -125, -122, -119, -117, -114, -111, -109, -106, -103, -101,  -98,  -95,  -93,  -90,
	 -87,  -85,  -82,  -79,  -77,  -74,  -71,  -69,  -66,  -64,  -61,  -58,  -56,  -53,  -50,  -47,
	 -45,  -42,  -39,  -37,  -34,  -31,  -29,  -26,  -23,  -21,  -18,  -15,  -13,  -10,   -7,   -5
};

// [=]===^=[ ftm_lfo_sawtooth_up ]================================================================[=]
static int8_t ftm_lfo_sawtooth_up[192] = {
	 127,  125,  124,  122,  121,  120,  118,  117,  116,  114,  113,  112,  110,  109,  108,  106,
	 105,  104,  102,  101,  100,   98,   97,   96,   94,   93,   92,   90,   89,   88,   86,   85,
	  84,   82,   81,   80,   78,   77,   76,   74,   73,   72,   70,   69,   68,   66,   65,   64,
	  62,   61,   60,   58,   57,   56,   54,   53,   52,   50,   49,   48,   46,   45,   44,   42,
	  41,   40,   38,   37,   36,   34,   33,   32,   30,   29,   28,   26,   25,   24,   22,   21,
	  20,   18,   17,   16,   14,   13,   12,   10,    9,    8,    6,    5,    4,    2,    1,    0,
	  -1,   -2,   -3,   -5,   -6,   -7,   -9,  -10,  -11,  -13,  -14,  -15,  -17,  -18,  -19,  -21,
	 -22,  -23,  -25,  -26,  -27,  -29,  -30,  -31,  -33,  -34,  -35,  -37,  -38,  -39,  -41,  -42,
	 -43,  -45,  -46,  -47,  -49,  -50,  -51,  -53,  -54,  -55,  -57,  -58,  -59,  -61,  -62,  -63,
	 -65,  -66,  -67,  -69,  -70,  -71,  -73,  -74,  -75,  -77,  -78,  -79,  -81,  -82,  -83,  -85,
	 -86,  -87,  -89,  -90,  -91,  -93,  -94,  -95,  -97,  -98,  -99, -101, -102, -103, -105, -106,
	-107, -109, -110, -111, -113, -114, -115, -117, -118, -119, -121, -122, -123, -125, -126, -128
};

// [=]===^=[ ftm_lfo_sawtooth_down ]==============================================================[=]
static int8_t ftm_lfo_sawtooth_down[192] = {
	-128, -126, -125, -123, -122, -121, -119, -118, -117, -115, -114, -113, -111, -110, -109, -107,
	-106, -105, -103, -102, -101,  -99,  -98,  -97,  -95,  -94,  -93,  -91,  -90,  -89,  -87,  -86,
	 -85,  -83,  -82,  -81,  -79,  -78,  -77,  -75,  -74,  -73,  -71,  -70,  -69,  -67,  -66,  -65,
	 -63,  -62,  -61,  -59,  -58,  -57,  -55,  -54,  -53,  -51,  -50,  -49,  -47,  -46,  -45,  -43,
	 -42,  -41,  -39,  -38,  -37,  -35,  -34,  -33,  -31,  -30,  -29,  -27,  -26,  -25,  -23,  -22,
	 -21,  -19,  -18,  -17,  -15,  -14,  -13,  -11,  -10,   -9,   -7,   -6,   -5,   -3,   -2,   -1,
	   0,    1,    2,    4,    5,    6,    8,    9,   10,   12,   13,   14,   16,   17,   18,   20,
	  21,   22,   24,   25,   26,   28,   29,   30,   32,   33,   34,   36,   37,   38,   40,   41,
	  42,   44,   45,   46,   48,   49,   50,   52,   53,   54,   56,   57,   58,   60,   61,   62,
	  64,   65,   66,   68,   69,   70,   72,   73,   74,   76,   77,   78,   80,   81,   82,   84,
	  85,   86,   88,   89,   90,   92,   93,   94,   96,   97,   98,  100,  101,  102,  104,  105,
	 106,  108,  109,  110,  112,  113,  114,  116,  117,  118,  120,  121,  122,  124,  125,  127
};

// [=]===^=[ ftm_lfo_shapes ]=====================================================================[=]
static int8_t *ftm_lfo_shapes[8] = {
	ftm_lfo_sine,
	ftm_lfo_square,
	ftm_lfo_triangle,
	ftm_lfo_sawtooth_up,
	ftm_lfo_sawtooth_down,
	ftm_lfo_sine,
	ftm_lfo_sine,
	ftm_lfo_sine
};

// [=]===^=[ ftm_read_u16_be ]====================================================================[=]
static uint16_t ftm_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ ftm_read_u32_be ]====================================================================[=]
static uint32_t ftm_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ ftm_check_mark ]=====================================================================[=]
static int32_t ftm_check_mark(uint8_t *data, uint32_t pos, uint32_t len, const char *mark) {
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

// [=]===^=[ ftm_clamp_i32 ]======================================================================[=]
static int32_t ftm_clamp_i32(int32_t v, int32_t lo, int32_t hi) {
	if(v < lo) {
		return lo;
	}
	if(v > hi) {
		return hi;
	}
	return v;
}

// [=]===^=[ ftm_load_header ]====================================================================[=]
static int32_t ftm_load_header(struct facethemusic_state *s, uint32_t *pos_io, uint8_t *out_num_sels) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = 5;	// skip "FTM\0" + version

	if(pos + 11 + 32 + 32 + 1 + 1 > len) {
		return 0;
	}

	s->number_of_samples = data[pos++];
	s->number_of_measures = ftm_read_u16_be(data + pos); pos += 2;
	s->start_cia_timer_value = ftm_read_u16_be(data + pos); pos += 2;
	pos += 1;	// tonality
	s->channel_mute_status = data[pos++];
	s->global_volume_init = data[pos++];
	s->flag = data[pos++];
	s->start_speed = data[pos++];
	s->rows_per_measure = data[pos++];

	if(s->start_speed == 0) {
		return 0;
	}
	if(s->rows_per_measure != (96 / s->start_speed)) {
		return 0;
	}

	pos += 32;	// song title
	pos += 32;	// artist

	*out_num_sels = data[pos++];
	pos += 1;	// padding

	*pos_io = pos;
	return 1;
}

// [=]===^=[ ftm_load_sample_names ]==============================================================[=]
static int32_t ftm_load_sample_names(struct facethemusic_state *s, uint32_t *pos_io) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = *pos_io;

	for(uint32_t i = 0; i < s->number_of_samples; ++i) {
		if(pos + 29 + 3 > len) {
			return 0;
		}

		uint8_t has_name = 0;
		for(uint32_t k = 0; k < 29; ++k) {
			uint8_t c = data[pos + k];
			if(c != 0 && c != ' ') {
				has_name = 1;
				break;
			}
		}
		s->samples[i].in_use = has_name;
		if(!has_name) {
			s->samples[i].sample_data = s->quiet_sample;
			s->samples[i].oneshot_length = 2;
			s->samples[i].loop_start = 0;
			s->samples[i].loop_length = 0;
			s->samples[i].total_length = 4;
		}

		pos += 29;
		pos += 3;	// octave + padding
	}

	for(uint32_t i = s->number_of_samples; i < FTM_MAX_SAMPLES; ++i) {
		s->samples[i].in_use = 0;
		s->samples[i].sample_data = s->quiet_sample;
		s->samples[i].oneshot_length = 2;
		s->samples[i].loop_start = 0;
		s->samples[i].loop_length = 0;
		s->samples[i].total_length = 4;
	}

	*pos_io = pos;
	return 1;
}

// [=]===^=[ ftm_load_scripts ]===================================================================[=]
static int32_t ftm_load_scripts(struct facethemusic_state *s, uint32_t *pos_io, uint8_t num_sels) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = *pos_io;

	for(uint32_t i = 0; i < num_sels; ++i) {
		if(pos + 4 > len) {
			return 0;
		}
		uint16_t num_lines = ftm_read_u16_be(data + pos); pos += 2;
		uint16_t script_index = ftm_read_u16_be(data + pos); pos += 2;

		if(script_index >= FTM_MAX_SCRIPTS) {
			return 0;
		}

		if(pos + (uint32_t)num_lines * 4 > len) {
			return 0;
		}

		struct ftm_sound_effect_line *lines = (struct ftm_sound_effect_line *)calloc(num_lines ? num_lines : 1, sizeof(struct ftm_sound_effect_line));
		if(!lines) {
			return 0;
		}

		for(uint32_t j = 0; j < num_lines; ++j) {
			lines[j].effect = data[pos++];
			lines[j].argument1 = data[pos++];
			lines[j].argument2 = ftm_read_u16_be(data + pos); pos += 2;
		}

		s->scripts[script_index].in_use = 1;
		s->scripts[script_index].lines = lines;
		s->scripts[script_index].num_lines = num_lines;
	}

	*pos_io = pos;
	return 1;
}

// [=]===^=[ ftm_load_tracks ]====================================================================[=]
static int32_t ftm_load_tracks(struct facethemusic_state *s, uint32_t *pos_io) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = *pos_io;

	for(uint32_t i = 0; i < FTM_NUM_TRACKS; ++i) {
		if(pos + 6 > len) {
			return 0;
		}
		s->tracks[i].default_spacing = ftm_read_u16_be(data + pos); pos += 2;
		uint32_t track_byte_len = ftm_read_u32_be(data + pos); pos += 4;
		uint32_t num_lines = track_byte_len / 2;

		if(pos + num_lines * 2 > len) {
			return 0;
		}

		struct ftm_track_line *lines = (struct ftm_track_line *)calloc(num_lines ? num_lines : 1, sizeof(struct ftm_track_line));
		if(!lines) {
			return 0;
		}

		for(uint32_t j = 0; j < num_lines; ++j) {
			uint8_t b1 = data[pos++];
			uint8_t b2 = data[pos++];
			uint8_t effect = (uint8_t)((b1 & 0xf0) >> 4);
			uint16_t arg;
			uint8_t note;

			if(effect == FTM_TE_SKIP_EMPTY_ROWS) {
				arg = (uint16_t)(((uint32_t)(b1 & 0x0f) << 8) | b2);
				note = 0;
			} else {
				arg = (uint16_t)(((uint32_t)(b1 & 0x0f) << 2) | ((b2 & 0xc0) >> 6));
				note = (uint8_t)(b2 & 0x3f);
			}

			lines[j].effect = effect;
			lines[j].effect_argument = arg;
			lines[j].note = note;
		}

		s->tracks[i].lines = lines;
		s->tracks[i].num_lines = num_lines;
	}

	*pos_io = pos;
	return 1;
}

// [=]===^=[ ftm_load_sample_data ]===============================================================[=]
static int32_t ftm_load_sample_data(struct facethemusic_state *s, uint32_t *pos_io) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = *pos_io;

	if((s->flag & 0x01) == 0) {
		// External samples not supported in this port; treat as no data
		for(uint32_t i = 0; i < FTM_MAX_SAMPLES; ++i) {
			if(s->samples[i].in_use) {
				s->samples[i].in_use = 0;
				s->samples[i].sample_data = s->quiet_sample;
				s->samples[i].oneshot_length = 2;
				s->samples[i].loop_start = 0;
				s->samples[i].loop_length = 0;
				s->samples[i].total_length = 4;
			}
		}
		*pos_io = pos;
		return 1;
	}

	for(uint32_t i = 0; i < FTM_MAX_SAMPLES; ++i) {
		if(!s->samples[i].in_use) {
			continue;
		}
		if(pos + 4 > len) {
			return 0;
		}
		uint16_t oneshot_len = ftm_read_u16_be(data + pos); pos += 2;
		uint16_t loop_len = ftm_read_u16_be(data + pos); pos += 2;

		uint32_t total_len = ((uint32_t)oneshot_len + (uint32_t)loop_len) * 2U;

		if(pos + total_len > len) {
			return 0;
		}

		s->samples[i].oneshot_length = oneshot_len;
		s->samples[i].loop_length = loop_len;
		s->samples[i].loop_start = (uint32_t)oneshot_len * 2U;
		s->samples[i].total_length = total_len;
		s->samples[i].sample_data = (int8_t *)(data + pos);
		pos += total_len;
	}

	*pos_io = pos;
	return 1;
}

// [=]===^=[ ftm_set_cia_timer ]==================================================================[=]
static void ftm_set_cia_timer(struct facethemusic_state *s, uint16_t cia_timer_value) {
	s->cia_timer_value = cia_timer_value;
	if(cia_timer_value == 0) {
		s->paula.samples_per_tick = s->paula.sample_rate / 50;
		return;
	}
	double freq_hz = (double)FTM_CIA_BASE / (double)cia_timer_value;
	int32_t spt = (int32_t)((double)s->paula.sample_rate / freq_hz);
	if(spt < 1) {
		spt = 1;
	}
	s->paula.samples_per_tick = spt;
}

// [=]===^=[ ftm_adjust_track_indexes ]===========================================================[=]
static void ftm_adjust_track_indexes(struct facethemusic_state *s, struct ftm_voice *v) {
	struct ftm_track *track = &s->tracks[v->channel_number];
	uint16_t spacing = track->default_spacing;
	uint32_t track_position = 0;
	int32_t row = 0;
	int32_t target = (int32_t)s->current_row;

	while(row < target) {
		if(track_position >= track->num_lines) {
			break;
		}
		struct ftm_track_line *line = &track->lines[track_position];

		if(line->effect == FTM_TE_SKIP_EMPTY_ROWS) {
			row += line->effect_argument;
			track_position++;
		} else {
			do {
				row++;
				track_position++;

				if(track_position >= track->num_lines) {
					break;
				}

				line = &track->lines[track_position];
				if(line->effect == FTM_TE_SKIP_EMPTY_ROWS) {
					break;
				}

				row += spacing;
			} while(row < target);
		}
	}

	v->rows_left_to_skip = (int16_t)(row - target);
	v->track_position = track_position;
}

// [=]===^=[ ftm_adjust_all_track_indexes ]=======================================================[=]
static void ftm_adjust_all_track_indexes(struct facethemusic_state *s) {
	for(uint32_t i = 0; i < FTM_NUM_TRACKS; ++i) {
		ftm_adjust_track_indexes(s, &s->voices[i]);
	}
}

// [=]===^=[ ftm_parse_track_pattern_loop ]=======================================================[=]
static void ftm_parse_track_pattern_loop(struct facethemusic_state *s, struct ftm_track_line *track_line, uint32_t track_position) {
	if(track_line->effect_argument == 0) {
		if(s->pattern_loop_stack_count > 0 && s->pattern_loop_stop_row == 0) {
			struct ftm_pattern_loop_info *pli = &s->pattern_loop_stack[s->pattern_loop_stack_count - 1];

			if(pli->original_loop_count == 63) {
				s->end_reached = 1;
			}

			pli->loop_count--;

			if(pli->loop_count < 0) {
				s->pattern_loop_stack_count--;
				s->pattern_loop_stop_row = 0;
			} else {
				s->pattern_loop_stop_row = (uint16_t)((((uint32_t)s->current_row / s->rows_per_measure) + 1) * s->rows_per_measure);
				s->pattern_loop_start_row = pli->loop_start_position;
			}
		}
	} else {
		if(s->pattern_loop_stop_row == 0) {
			if(s->pattern_loop_stack_count < FTM_MAX_LOOP_STACK) {
				uint16_t loop_start = (uint16_t)(((uint32_t)s->current_row / s->rows_per_measure) * s->rows_per_measure);
				struct ftm_pattern_loop_info *pli = &s->pattern_loop_stack[s->pattern_loop_stack_count++];
				pli->track_position = track_position;
				pli->loop_start_position = loop_start;
				pli->loop_count = (int16_t)track_line->effect_argument;
				pli->original_loop_count = (int16_t)track_line->effect_argument;
			}
		} else {
			struct ftm_pattern_loop_info *pli = &s->pattern_loop_stack[s->pattern_loop_stack_count - 1];
			if(track_position == pli->track_position) {
				s->pattern_loop_stop_row = 0;
			}
		}
	}
}

// [=]===^=[ ftm_parse_track_volume_down ]========================================================[=]
static void ftm_parse_track_volume_down(struct facethemusic_state *s, struct ftm_voice *v, struct ftm_track_line *track_line) {
	if(track_line->effect_argument == 0) {
		v->volume = 0;
	} else {
		uint32_t ticks = (uint32_t)track_line->effect_argument * (uint32_t)s->speed;
		v->volume_down_volume = (uint32_t)v->volume * 256U;
		v->volume_down_speed = (ticks != 0) ? (uint16_t)(v->volume_down_volume / ticks) : 0;

		if(v->sel_state.volume_down_goto != 0) {
			v->sel_state.interrupt_line_number = v->sel_state.volume_down_goto;
		}
	}
}

// [=]===^=[ ftm_parse_track_portamento ]=========================================================[=]
static void ftm_parse_track_portamento(struct facethemusic_state *s, struct ftm_voice *v, struct ftm_track_line *track_line) {
	if(track_line->note != 0) {
		v->portamento_end_note = (uint16_t)((track_line->note - 1) * 16);
		v->portamento_note = (uint32_t)v->note_index * 256U;

		int32_t ticks_to_porta = (int32_t)((uint32_t)v->portamento_end_note * 256U) - (int32_t)v->portamento_note;

		if(ticks_to_porta != 0) {
			if(track_line->effect_argument == 0) {
				v->note_index = v->portamento_end_note;
				v->portamento_ticks = 0;
			} else {
				ticks_to_porta /= (int32_t)((uint32_t)track_line->effect_argument * s->speed);
				if(v->sel_state.portamento_goto != 0) {
					v->sel_state.interrupt_line_number = v->sel_state.portamento_goto;
				}
			}
		}
		v->portamento_ticks = (int16_t)ticks_to_porta;
	}
}

// [=]===^=[ ftm_parse_track_sel ]================================================================[=]
static void ftm_parse_track_sel(struct facethemusic_state *s, struct ftm_voice *v, struct ftm_track_line *track_line) {
	int32_t script_index = track_line->effect_argument;
	if(script_index < 0 || script_index >= FTM_MAX_SCRIPTS || !s->scripts[script_index].in_use) {
		v->sel_state.script_index = -1;
		v->sel_state.script_position = -1;
	} else {
		v->sel_state.script_index = script_index;
		v->sel_state.script_position = 0;
	}

	v->sel_state.wait_counter = 0;
	v->sel_state.loop_counter = 0;

	v->sel_state.new_pitch_goto = 0;
	v->sel_state.new_volume_goto = 0;
	v->sel_state.new_sample_goto = 0;
	v->sel_state.release_goto = 0;
	v->sel_state.portamento_goto = 0;
	v->sel_state.volume_down_goto = 0;

	v->sel_state.interrupt_line_number = 0;
	v->sel_state.voice_index = v->channel_number;
}

// [=]===^=[ ftm_parse_track_set_volume ]=========================================================[=]
static void ftm_parse_track_set_volume(struct facethemusic_state *s, struct ftm_voice *v, struct ftm_track_line *track_line) {
	(void)s;
	if(track_line->effect_argument != 0) {
		v->sample_number = (int32_t)track_line->effect_argument - 1;
		v->current_sample_index = v->sample_number;

		if(v->sel_state.new_sample_goto != 0) {
			v->sel_state.interrupt_line_number = v->sel_state.new_sample_goto;
		}
	}

	if(track_line->effect != FTM_TE_NONE) {
		v->volume = ftm_effect_volume[track_line->effect - 1];
		v->volume_down_speed = 0;

		if(v->sel_state.new_volume_goto != 0) {
			v->sel_state.interrupt_line_number = v->sel_state.new_volume_goto;
		}
	}
}

// [=]===^=[ ftm_setup_note_and_sample ]==========================================================[=]
static void ftm_setup_note_and_sample(struct facethemusic_state *s, struct ftm_voice *v, struct ftm_track_line *track_line, uint8_t new_sample) {
	uint8_t note = track_line->note;

	if(note == 35) {
		if(v->sel_state.release_goto != 0) {
			v->sel_state.interrupt_line_number = v->sel_state.release_goto;
		}
	} else if(note > 0 && note < 35) {
		v->note_index = (uint16_t)((note - 1) * 16);

		if(v->sel_state.new_pitch_goto != 0) {
			v->sel_state.interrupt_line_number = v->sel_state.new_pitch_goto;
		}

		v->portamento_ticks = 0;

		struct ftm_sample *samp = (v->current_sample_index >= 0 && v->current_sample_index < FTM_MAX_SAMPLES) ? &s->samples[v->current_sample_index] : 0;

		uint8_t want_retrig = new_sample || (v->sample_data == s->quiet_sample);
		if(samp != 0 && samp->loop_length == 0) {
			want_retrig = 1;
		}

		if(want_retrig) {
			v->retrig_sample = 1;
			if(samp && samp->in_use) {
				v->sample_data = samp->sample_data;
				v->sample_oneshot_length = samp->oneshot_length;
				v->sample_loop_start = samp->loop_start;
				v->sample_loop_length = samp->loop_length;
				v->sample_total_length = samp->total_length;
			} else {
				v->sample_data = s->quiet_sample;
				v->sample_oneshot_length = 2;
				v->sample_loop_start = 0;
				v->sample_loop_length = 0;
				v->sample_total_length = 4;
			}
			v->sample_start_offset = 0;
			v->sample_calculate_offset = 0;
		}
	}
}

// [=]===^=[ ftm_parse_track_effect ]=============================================================[=]
static void ftm_parse_track_effect(struct facethemusic_state *s, struct ftm_voice *v, uint32_t track_position) {
	struct ftm_track *track = &s->tracks[v->channel_number];
	if(track_position >= track->num_lines) {
		return;
	}
	struct ftm_track_line *track_line = &track->lines[track_position];

	if(track_line->effect == FTM_TE_NONE && track_line->effect_argument == 0 && track_line->note == 0) {
		return;
	}

	uint8_t new_sample = 0;

	switch(track_line->effect) {
		case FTM_TE_PATTERN_LOOP: {
			ftm_parse_track_pattern_loop(s, track_line, track_position);
			break;
		}

		case FTM_TE_VOLUME_DOWN: {
			ftm_parse_track_volume_down(s, v, track_line);
			break;
		}

		case FTM_TE_PORTAMENTO: {
			ftm_parse_track_portamento(s, v, track_line);
			return;
		}

		case FTM_TE_SEL: {
			ftm_parse_track_sel(s, v, track_line);
			break;
		}

		default: {
			new_sample = (track_line->effect_argument != 0) ? 1 : 0;
			ftm_parse_track_set_volume(s, v, track_line);
			break;
		}
	}

	ftm_setup_note_and_sample(s, v, track_line, new_sample);
}

// [=]===^=[ ftm_initialize_voice_with_latest ]===================================================[=]
// Walks back through the track to find the last instrument and last volume effect
// before track_position, then uses them to seed the voice.
static void ftm_initialize_voice_with_latest(struct facethemusic_state *s, struct ftm_voice *v) {
	struct ftm_track *track = &s->tracks[v->channel_number];
	uint32_t old_track_position = v->track_position;
	if(old_track_position >= track->num_lines) {
		return;
	}
	struct ftm_track_line saved = track->lines[old_track_position];

	int32_t track_position = (int32_t)old_track_position - 1;

	uint8_t previous_volume_effect = FTM_TE_NONE;
	uint16_t previous_instrument = 0;

	while(track_position >= 0) {
		struct ftm_track_line *line = &track->lines[track_position];
		if(line->effect < FTM_TE_SEL) {
			previous_volume_effect = line->effect;
			if(previous_volume_effect != FTM_TE_NONE) {
				break;
			}
		}
		track_position--;
	}

	track_position = (int32_t)old_track_position - 1;

	while(track_position >= 0) {
		struct ftm_track_line *line = &track->lines[track_position];
		if(line->effect < FTM_TE_SEL) {
			previous_instrument = line->effect_argument;
			if(previous_instrument != 0) {
				break;
			}
		}
		track_position--;
	}

	track->lines[old_track_position].effect = previous_volume_effect;
	track->lines[old_track_position].effect_argument = previous_instrument;
	track->lines[old_track_position].note = 0;

	ftm_parse_track_effect(s, v, v->track_position);

	track->lines[old_track_position] = saved;
}

// [=]===^=[ ftm_handle_row ]=====================================================================[=]
static void ftm_handle_row(struct facethemusic_state *s, struct ftm_voice *v) {
	struct ftm_track *track = &s->tracks[v->channel_number];

	v->rows_left_to_skip--;

	if(v->rows_left_to_skip < 0) {
		uint32_t track_position = v->track_position;
		if(track_position >= track->num_lines) {
			return;
		}
		struct ftm_track_line *track_line = &track->lines[track_position];

		if(track_line->effect == FTM_TE_SKIP_EMPTY_ROWS) {
			if(track_line->effect_argument == 0 && track_line->note == 0) {
				track_position++;
			} else {
				v->rows_left_to_skip = (int16_t)(track_line->effect_argument - 1);
				v->track_position++;
				return;
			}
		}

		ftm_parse_track_effect(s, v, track_position);

		track_position++;

		if(track_position < track->num_lines) {
			track_line = &track->lines[track_position];

			if(track_line->effect == FTM_TE_SKIP_EMPTY_ROWS) {
				v->rows_left_to_skip = (int16_t)track_line->effect_argument;
				v->track_position = track_position + 1;
			} else {
				v->rows_left_to_skip = (int16_t)track->default_spacing;
				v->track_position = track_position;
			}
		} else {
			v->rows_left_to_skip = (int16_t)track->default_spacing;
			v->track_position = track_position;
		}
	}
}

// [=]===^=[ ftm_take_next_row ]==================================================================[=]
static void ftm_take_next_row(struct facethemusic_state *s) {
	if(s->do_pattern_loop) {
		s->do_pattern_loop = 0;
		s->current_row = s->pattern_loop_start_row;
	} else {
		for(uint32_t i = 0; i < FTM_NUM_TRACKS; ++i) {
			if(s->channel_mapping[i] != -1) {
				ftm_handle_row(s, &s->voices[i]);
			}
		}

		s->current_row++;

		if(s->current_row == s->pattern_loop_stop_row) {
			s->current_row = s->pattern_loop_start_row;
		} else if(s->current_row == s->end_row) {
			s->current_row = s->start_row;
			s->end_reached = 1;
		}
	}

	ftm_adjust_all_track_indexes(s);
}

// SEL helpers / forward declarations omitted; below is the SEL runtime.

// [=]===^=[ ftm_run_volume_down ]================================================================[=]
static void ftm_run_volume_down(struct ftm_voice *v) {
	if(v->volume_down_speed != 0) {
		int32_t new_value = (int32_t)v->volume_down_volume - (int32_t)v->volume_down_speed;
		if(new_value < 0) {
			v->volume_down_speed = 0;
			new_value = 0;
		}
		v->volume_down_volume = (uint32_t)new_value;
		v->volume = (uint16_t)(new_value / 256);
	}
}

// [=]===^=[ ftm_run_portamento ]=================================================================[=]
static void ftm_run_portamento(struct ftm_voice *v) {
	if(v->portamento_ticks != 0) {
		v->portamento_note = (uint32_t)((int64_t)v->portamento_note + v->portamento_ticks);
		v->note_index = (uint16_t)((v->portamento_note / 256) & 0xfffe);

		if(v->portamento_ticks < 0) {
			if(v->note_index <= v->portamento_end_note) {
				v->note_index = v->portamento_end_note;
				v->portamento_ticks = 0;
			}
		} else {
			if(v->note_index >= v->portamento_end_note) {
				v->note_index = v->portamento_end_note;
				v->portamento_ticks = 0;
			}
		}
	}
}

// [=]===^=[ ftm_sel_goto_line_n ]================================================================[=]
static int32_t ftm_sel_goto_line_n(struct facethemusic_state *s, struct ftm_sound_effect_state *st, uint16_t line_number) {
	if(st->script_index < 0) {
		st->script_position = -1;
		return 0;
	}
	struct ftm_sound_effect_script *sc = &s->scripts[st->script_index];
	if(line_number < sc->num_lines) {
		st->script_position = (int32_t)line_number - 1;
		return 1;
	}
	st->script_position = -1;
	return 0;
}

// [=]===^=[ ftm_sel_goto_line ]==================================================================[=]
static int32_t ftm_sel_goto_line(struct facethemusic_state *s, struct ftm_sound_effect_state *st, struct ftm_sound_effect_line *line) {
	return ftm_sel_goto_line_n(s, st, (uint16_t)(line->argument2 & 0x0fff));
}

// [=]===^=[ ftm_sel_play_sample_quiet ]==========================================================[=]
static void ftm_sel_play_sample_quiet(struct facethemusic_state *s, struct ftm_voice *sel_v) {
	sel_v->sample_data = s->quiet_sample;
	sel_v->sample_start_offset = 0;
	sel_v->sample_calculate_offset = 0;
	sel_v->sample_oneshot_length = 2;
	sel_v->sample_loop_start = 0;
	sel_v->sample_loop_length = 0;
	sel_v->sample_total_length = 4;
	sel_v->retrig_sample = 1;
}

// [=]===^=[ ftm_sel_play_sample_current ]========================================================[=]
static void ftm_sel_play_sample_current(struct facethemusic_state *s, struct ftm_voice *sel_v) {
	int32_t idx = sel_v->current_sample_index;
	if(idx < 0 || idx >= FTM_MAX_SAMPLES || !s->samples[idx].in_use) {
		ftm_sel_play_sample_quiet(s, sel_v);
		return;
	}
	struct ftm_sample *samp = &s->samples[idx];
	sel_v->sample_data = samp->sample_data;
	sel_v->sample_start_offset = 0;
	sel_v->sample_calculate_offset = 0;
	sel_v->sample_oneshot_length = samp->oneshot_length;
	sel_v->sample_loop_start = samp->loop_start;
	sel_v->sample_loop_length = samp->loop_length;
	sel_v->sample_total_length = samp->total_length;
	sel_v->retrig_sample = 1;
}

// [=]===^=[ ftm_sel_oneshot_helper ]=============================================================[=]
static void ftm_sel_oneshot_helper(struct ftm_voice *sel_v, uint16_t new_length) {
	sel_v->sample_oneshot_length = new_length;
	sel_v->sample_loop_start = sel_v->sample_calculate_offset + (uint32_t)new_length * 2U;
	sel_v->sample_total_length = (uint32_t)sel_v->sample_loop_length * 2U + sel_v->sample_loop_start;
}

// [=]===^=[ ftm_sel_repeat_helper ]==============================================================[=]
static void ftm_sel_repeat_helper(struct ftm_voice *sel_v, uint16_t new_length) {
	sel_v->sample_loop_length = new_length;
	sel_v->sample_total_length = sel_v->sample_calculate_offset + ((uint32_t)sel_v->sample_oneshot_length + (uint32_t)new_length) * 2U;
}

// [=]===^=[ ftm_sel_lfo_start ]==================================================================[=]
static void ftm_sel_lfo_start(struct ftm_lfo_state *lfo, struct ftm_sound_effect_line *line) {
	uint32_t target = (uint32_t)((line->argument1 & 0xf0) >> 4);
	uint32_t shape = (uint32_t)(line->argument1 & 0x0f);
	uint32_t speed = (uint32_t)((line->argument2 & 0xff00) >> 8);
	uint32_t depth = (uint32_t)(line->argument2 & 0x00ff);

	lfo->target = (uint8_t)target;
	lfo->loop_modulation = ((shape & 0x08) == 0) ? 1 : 0;
	lfo->shape_table = ftm_lfo_shapes[shape & 0x07];
	lfo->shape_table_position = 0;

	if(speed > 192) {
		speed = 191;
	}
	lfo->modulation_speed = (uint16_t)speed;
	lfo->modulation_depth = (uint16_t)(depth & 0x7f);
	lfo->modulation_value = 0;
}

// [=]===^=[ ftm_sel_lfo_sd_add ]=================================================================[=]
static void ftm_sel_lfo_sd_add(struct ftm_lfo_state *lfo, struct ftm_sound_effect_line *line) {
	uint32_t speed = (uint32_t)lfo->modulation_speed + ((line->argument2 & 0xff00) >> 8);
	if(speed > 192) {
		speed = 191;
	}
	lfo->modulation_speed = (uint16_t)speed;

	uint32_t depth = (uint32_t)lfo->modulation_depth + (line->argument2 & 0x00ff);
	if(depth > 127) {
		depth = 127;
	}
	lfo->modulation_depth = (uint16_t)depth;
}

// [=]===^=[ ftm_sel_lfo_sd_sub ]=================================================================[=]
static void ftm_sel_lfo_sd_sub(struct ftm_lfo_state *lfo, struct ftm_sound_effect_line *line) {
	int32_t speed = (int32_t)lfo->modulation_speed - (int32_t)((line->argument2 & 0xff00) >> 8);
	if(speed < 0) {
		speed = 0;
	}
	lfo->modulation_speed = (uint16_t)speed;

	int32_t depth = (int32_t)lfo->modulation_depth - (int32_t)(line->argument2 & 0x00ff);
	if(depth < 0) {
		depth = 0;
	}
	lfo->modulation_depth = (uint16_t)depth;
}

// [=]===^=[ ftm_run_specific_sel_effect ]========================================================[=]
// Returns 1 if the SEL engine should advance to the next line, 0 if it should stop now.
static int32_t ftm_run_specific_sel_effect(struct facethemusic_state *s, struct ftm_voice *v, struct ftm_sound_effect_state *st, struct ftm_sound_effect_line *line) {
	int32_t sel_idx = (st->voice_index >= 0 && st->voice_index < FTM_NUM_TRACKS) ? st->voice_index : v->channel_number;
	struct ftm_voice *sel_v = &s->voices[sel_idx];

	switch(line->effect) {
		case FTM_SE_WAIT: {
			st->wait_counter = (uint16_t)(line->argument2 - 1);
			if(st->wait_counter != 0xffff) {
				st->script_position++;
				if(st->script_index >= 0) {
					struct ftm_sound_effect_script *sc = &s->scripts[st->script_index];
					if((uint32_t)st->script_position == sc->num_lines) {
						st->script_position = -1;
					}
				}
			}
			return 0;
		}

		case FTM_SE_GOTO: {
			return ftm_sel_goto_line(s, st, line);
		}

		case FTM_SE_LOOP: {
			if(st->loop_counter == 0) {
				st->loop_counter = (uint16_t)(((uint32_t)line->argument1 << 4) | ((line->argument2 & 0xf000) >> 12));
			} else {
				st->loop_counter--;
			}
			if(st->loop_counter == 0) {
				return 1;
			}
			return ftm_sel_goto_line(s, st, line);
		}

		case FTM_SE_GOTO_SCRIPT: {
			int32_t script_number = (int32_t)(((uint32_t)line->argument1 << 4) | ((line->argument2 & 0xf000) >> 12));
			if(script_number < 0 || script_number >= FTM_MAX_SCRIPTS || !s->scripts[script_number].in_use) {
				st->script_position = -1;
				return 0;
			}
			st->script_index = script_number;
			return ftm_sel_goto_line(s, st, line);
		}

		case FTM_SE_END: {
			st->script_position = -1;
			return 0;
		}

		case FTM_SE_IF_PITCH_EQ: {
			int32_t pitch = (int32_t)(((uint32_t)line->argument1 << 4) | ((line->argument2 & 0xf000) >> 12));
			if((int32_t)sel_v->note_index == pitch) {
				return ftm_sel_goto_line(s, st, line);
			}
			return 1;
		}

		case FTM_SE_IF_PITCH_LT: {
			int32_t pitch = (int32_t)(((uint32_t)line->argument1 << 4) | ((line->argument2 & 0xf000) >> 12));
			if((int32_t)sel_v->note_index < pitch) {
				return ftm_sel_goto_line(s, st, line);
			}
			return 1;
		}

		case FTM_SE_IF_PITCH_GT: {
			int32_t pitch = (int32_t)(((uint32_t)line->argument1 << 4) | ((line->argument2 & 0xf000) >> 12));
			if((int32_t)sel_v->note_index > pitch) {
				return ftm_sel_goto_line(s, st, line);
			}
			return 1;
		}

		case FTM_SE_IF_VOLUME_EQ: {
			int32_t vol = (int32_t)(((uint32_t)line->argument1 << 4) | ((line->argument2 & 0xf000) >> 12));
			if((int32_t)sel_v->volume == vol) {
				return ftm_sel_goto_line(s, st, line);
			}
			return 1;
		}

		case FTM_SE_IF_VOLUME_LT: {
			int32_t vol = (int32_t)(((uint32_t)line->argument1 << 4) | ((line->argument2 & 0xf000) >> 12));
			if((int32_t)sel_v->volume < vol) {
				return ftm_sel_goto_line(s, st, line);
			}
			return 1;
		}

		case FTM_SE_IF_VOLUME_GT: {
			int32_t vol = (int32_t)(((uint32_t)line->argument1 << 4) | ((line->argument2 & 0xf000) >> 12));
			if((int32_t)sel_v->volume > vol) {
				return ftm_sel_goto_line(s, st, line);
			}
			return 1;
		}

		case FTM_SE_ON_NEW_PITCH: {
			st->new_pitch_goto = (uint16_t)((line->argument2 & 0x0fff) + 1);
			break;
		}

		case FTM_SE_ON_NEW_VOLUME: {
			st->new_volume_goto = (uint16_t)((line->argument2 & 0x0fff) + 1);
			break;
		}

		case FTM_SE_ON_NEW_SAMPLE: {
			st->new_sample_goto = (uint16_t)((line->argument2 & 0x0fff) + 1);
			break;
		}

		case FTM_SE_ON_RELEASE: {
			st->release_goto = (uint16_t)((line->argument2 & 0x0fff) + 1);
			break;
		}

		case FTM_SE_ON_PORTAMENTO: {
			st->portamento_goto = (uint16_t)((line->argument2 & 0x0fff) + 1);
			break;
		}

		case FTM_SE_ON_VOLUME_DOWN: {
			st->volume_down_goto = (uint16_t)((line->argument2 & 0x0fff) + 1);
			break;
		}

		case FTM_SE_PLAY_CURRENT_SAMPLE: {
			ftm_sel_play_sample_current(s, sel_v);
			break;
		}

		case FTM_SE_PLAY_QUIET_SAMPLE: {
			ftm_sel_play_sample_quiet(s, sel_v);
			break;
		}

		case FTM_SE_PLAY_POSITION: {
			uint32_t offset = (uint32_t)(((line->argument1 & 0x0f) << 16) + line->argument2);
			sel_v->sample_start_offset = sel_v->sample_calculate_offset + offset * 2;
			sel_v->retrig_sample = 1;
			break;
		}

		case FTM_SE_PLAY_POSITION_ADD: {
			uint32_t offset = (uint32_t)(((line->argument1 & 0x0f) << 16) + line->argument2);
			sel_v->sample_start_offset += offset;
			sel_v->retrig_sample = 1;
			break;
		}

		case FTM_SE_PLAY_POSITION_SUB: {
			uint32_t offset = (uint32_t)(((line->argument1 & 0x0f) << 16) + line->argument2);
			sel_v->sample_start_offset -= offset;
			sel_v->retrig_sample = 1;
			break;
		}

		case FTM_SE_PITCH: {
			uint32_t note_index = (uint32_t)((line->argument2 & 0x0fff) * 2);
			if(note_index >= 542) {
				note_index = 542;
			}
			sel_v->note_index = (uint16_t)note_index;
			break;
		}

		case FTM_SE_DETUNE: {
			int16_t detune = (int16_t)(line->argument2 & 0x0fff);
			if(sel_v->detune_index >= 0 && sel_v->detune_index < 4) {
				s->detune_values[sel_v->detune_index] = detune;
			}
			break;
		}

		case FTM_SE_DETUNE_PITCH_ADD: {
			int16_t detune = (int16_t)(((uint32_t)line->argument1 << 4) | ((line->argument2 & 0xf000) >> 12));
			if(sel_v->detune_index >= 0 && sel_v->detune_index < 4) {
				s->detune_values[sel_v->detune_index] = (int16_t)(s->detune_values[sel_v->detune_index] + detune);
			}
			uint32_t note_index = (uint32_t)((line->argument2 & 0x0fff) * 2) + sel_v->note_index;
			if(note_index > 542) {
				note_index = 542;
			}
			sel_v->note_index = (uint16_t)note_index;
			break;
		}

		case FTM_SE_DETUNE_PITCH_SUB: {
			int16_t detune = (int16_t)(((uint32_t)line->argument1 << 4) | ((line->argument2 & 0xf000) >> 12));
			if(sel_v->detune_index >= 0 && sel_v->detune_index < 4) {
				s->detune_values[sel_v->detune_index] = (int16_t)(s->detune_values[sel_v->detune_index] - detune);
			}
			int32_t note_index = (int32_t)sel_v->note_index - (int32_t)((line->argument2 & 0x0fff) * 2);
			if(note_index < 0) {
				note_index = 0;
			}
			sel_v->note_index = (uint16_t)note_index;
			break;
		}

		case FTM_SE_VOLUME: {
			int32_t vol = (int32_t)(line->argument2 & 0x00ff);
			if(vol > 64) {
				vol = 64;
			}
			sel_v->volume = (uint16_t)vol;
			break;
		}

		case FTM_SE_VOLUME_ADD: {
			int32_t vol = (int32_t)sel_v->volume + (int32_t)(line->argument2 & 0x00ff);
			if(vol > 64) {
				vol = 64;
			}
			sel_v->volume = (uint16_t)vol;
			break;
		}

		case FTM_SE_VOLUME_SUB: {
			int32_t vol = (int32_t)sel_v->volume - (int32_t)(line->argument2 & 0x00ff);
			if(vol < 0) {
				vol = 0;
			}
			sel_v->volume = (uint16_t)vol;
			break;
		}

		case FTM_SE_CURRENT_SAMPLE: {
			sel_v->sample_number = (int32_t)(line->argument2 & 0x00ff);
			sel_v->current_sample_index = sel_v->sample_number;
			break;
		}

		case FTM_SE_SAMPLE_START: {
			uint32_t offset = (uint32_t)(((line->argument1 & 0x0f) << 16) + line->argument2);
			if(offset > sel_v->sample_total_length) {
				offset = sel_v->sample_total_length;
			}
			sel_v->sample_calculate_offset = offset;
			break;
		}

		case FTM_SE_SAMPLE_START_ADD: {
			uint32_t offset = (uint32_t)(((line->argument1 & 0x0f) << 16) + line->argument2);
			offset += sel_v->sample_calculate_offset;
			if(offset > sel_v->sample_total_length) {
				offset = sel_v->sample_total_length;
			}
			sel_v->sample_calculate_offset = offset;
			break;
		}

		case FTM_SE_SAMPLE_START_SUB: {
			int32_t offset = (int32_t)(((line->argument1 & 0x0f) << 16) + line->argument2);
			offset = (int32_t)sel_v->sample_calculate_offset - offset;
			if(offset < 0) {
				offset = 0;
			}
			sel_v->sample_calculate_offset = (uint32_t)offset;
			break;
		}

		case FTM_SE_ONESHOT_LENGTH: {
			ftm_sel_oneshot_helper(sel_v, line->argument2);
			break;
		}

		case FTM_SE_ONESHOT_LENGTH_ADD: {
			int32_t new_length = (int32_t)sel_v->sample_oneshot_length + line->argument2;
			if(new_length > 0xffff) {
				new_length = 0xffff;
			}
			ftm_sel_oneshot_helper(sel_v, (uint16_t)new_length);
			break;
		}

		case FTM_SE_ONESHOT_LENGTH_SUB: {
			int32_t new_length = (int32_t)sel_v->sample_oneshot_length - line->argument2;
			if(new_length < 0) {
				new_length = 0;
			}
			ftm_sel_oneshot_helper(sel_v, (uint16_t)new_length);
			break;
		}

		case FTM_SE_REPEAT_LENGTH: {
			ftm_sel_repeat_helper(sel_v, line->argument2);
			break;
		}

		case FTM_SE_REPEAT_LENGTH_ADD: {
			int32_t new_length = (int32_t)sel_v->sample_loop_length + line->argument2;
			if(new_length > 0xffff) {
				new_length = 0xffff;
			}
			ftm_sel_repeat_helper(sel_v, (uint16_t)new_length);
			break;
		}

		case FTM_SE_REPEAT_LENGTH_SUB: {
			int32_t new_length = (int32_t)sel_v->sample_loop_length - line->argument2;
			if(new_length < 0) {
				new_length = 0;
			}
			ftm_sel_repeat_helper(sel_v, (uint16_t)new_length);
			break;
		}

		case FTM_SE_GET_PITCH_OF_TRACK: {
			int32_t track = line->argument2 & 0x0007;
			sel_v->note_index = s->voices[track].note_index;
			break;
		}

		case FTM_SE_GET_VOLUME_OF_TRACK: {
			int32_t track = line->argument2 & 0x0007;
			sel_v->volume = s->voices[track].volume;
			break;
		}

		case FTM_SE_GET_SAMPLE_OF_TRACK: {
			int32_t track = line->argument2 & 0x0007;
			sel_v->current_sample_index = s->voices[track].current_sample_index;
			break;
		}

		case FTM_SE_CLONE_TRACK: {
			int32_t track = line->argument2 & 0x0007;
			struct ftm_voice *src = &s->voices[track];
			sel_v->current_sample_index = src->current_sample_index;
			sel_v->sample_number = src->sample_number;
			sel_v->sample_data = src->sample_data;
			sel_v->sample_start_offset = src->sample_start_offset;
			sel_v->sample_calculate_offset = src->sample_calculate_offset;
			sel_v->sample_oneshot_length = src->sample_oneshot_length;
			sel_v->sample_loop_start = src->sample_loop_start;
			sel_v->sample_loop_length = src->sample_loop_length;
			sel_v->sample_total_length = src->sample_total_length;
			sel_v->volume = src->volume;
			sel_v->note_index = src->note_index;
			sel_v->retrig_sample = src->retrig_sample;
			sel_v->detune_index = src->detune_index;
			sel_v->portamento_ticks = src->portamento_ticks;
			sel_v->portamento_note = src->portamento_note;
			sel_v->portamento_end_note = src->portamento_end_note;
			sel_v->volume_down_volume = src->volume_down_volume;
			sel_v->volume_down_speed = src->volume_down_speed;
			sel_v->sel_state = src->sel_state;
			memcpy(sel_v->lfo_states, src->lfo_states, sizeof(sel_v->lfo_states));
			break;
		}

		case FTM_SE_FIRST_LFO_START: {
			ftm_sel_lfo_start(&v->lfo_states[0], line);
			break;
		}

		case FTM_SE_FIRST_LFO_SD_ADD: {
			ftm_sel_lfo_sd_add(&v->lfo_states[0], line);
			break;
		}

		case FTM_SE_FIRST_LFO_SD_SUB: {
			ftm_sel_lfo_sd_sub(&v->lfo_states[0], line);
			break;
		}

		case FTM_SE_SECOND_LFO_START: {
			ftm_sel_lfo_start(&v->lfo_states[1], line);
			break;
		}

		case FTM_SE_SECOND_LFO_SD_ADD: {
			ftm_sel_lfo_sd_add(&v->lfo_states[1], line);
			break;
		}

		case FTM_SE_SECOND_LFO_SD_SUB: {
			ftm_sel_lfo_sd_sub(&v->lfo_states[1], line);
			break;
		}

		case FTM_SE_THIRD_LFO_START: {
			ftm_sel_lfo_start(&v->lfo_states[2], line);
			break;
		}

		case FTM_SE_THIRD_LFO_SD_ADD: {
			ftm_sel_lfo_sd_add(&v->lfo_states[2], line);
			break;
		}

		case FTM_SE_THIRD_LFO_SD_SUB: {
			ftm_sel_lfo_sd_sub(&v->lfo_states[2], line);
			break;
		}

		case FTM_SE_FOURTH_LFO_START: {
			ftm_sel_lfo_start(&v->lfo_states[3], line);
			break;
		}

		case FTM_SE_FOURTH_LFO_SD_ADD: {
			ftm_sel_lfo_sd_add(&v->lfo_states[3], line);
			break;
		}

		case FTM_SE_FOURTH_LFO_SD_SUB: {
			ftm_sel_lfo_sd_sub(&v->lfo_states[3], line);
			break;
		}

		case FTM_SE_WORK_ON_TRACK: {
			int32_t track = line->argument2 & 0x0007;
			v->sel_state.voice_index = track;
			break;
		}

		case FTM_SE_WORK_TRACK_ADD: {
			int32_t track = line->argument2 & 0x0007;
			int32_t base_idx = (v->sel_state.voice_index >= 0 && v->sel_state.voice_index < FTM_NUM_TRACKS) ? v->sel_state.voice_index : v->channel_number;
			track += base_idx;
			if(track >= FTM_NUM_TRACKS) {
				track -= FTM_NUM_TRACKS;
			}
			v->sel_state.voice_index = track;
			break;
		}

		case FTM_SE_GLOBAL_VOLUME: {
			int32_t vol = line->argument2 & 0x00ff;
			if(vol > 64) {
				vol = 64;
			}
			s->global_volume = (uint8_t)vol;
			break;
		}

		case FTM_SE_GLOBAL_SPEED: {
			uint16_t cia = line->argument2;
			if(cia < 0x1000) {
				cia = 0x1000;
			}
			ftm_set_cia_timer(s, cia);
			break;
		}

		case FTM_SE_TICKS_PER_LINE: {
			s->speed = (uint16_t)(line->argument2 & 0x00ff);
			break;
		}

		case FTM_SE_JUMP_TO_SONG_LINE: {
			s->pattern_loop_start_row = line->argument2;
			s->do_pattern_loop = 1;
			break;
		}
	}

	return 1;
}

// [=]===^=[ ftm_run_track_sel ]==================================================================[=]
static void ftm_run_track_sel(struct facethemusic_state *s, struct ftm_voice *v) {
	struct ftm_sound_effect_state *st = &v->sel_state;

	if(st->script_position < 0) {
		return;
	}

	if(st->interrupt_line_number != 0) {
		uint16_t goto_line = st->interrupt_line_number;
		st->interrupt_line_number = 0;
		st->wait_counter = 0;
		st->loop_counter = 0;
		if(!ftm_sel_goto_line_n(s, st, goto_line)) {
			return;
		}
	} else {
		if(st->wait_counter != 0) {
			if(st->wait_counter != 0xffff) {
				st->wait_counter--;
			}
			return;
		}
	}

	for(;;) {
		if(st->script_index < 0 || st->script_position < 0) {
			break;
		}
		struct ftm_sound_effect_script *sc = &s->scripts[st->script_index];
		if((uint32_t)st->script_position >= sc->num_lines) {
			st->script_position = -1;
			break;
		}
		struct ftm_sound_effect_line *line = &sc->lines[st->script_position];

		if(line->effect != FTM_SE_NOTHING) {
			if(!ftm_run_specific_sel_effect(s, v, st, line)) {
				break;
			}
		}

		st->script_position++;
		if(st->script_index >= 0) {
			sc = &s->scripts[st->script_index];
			if((uint32_t)st->script_position >= sc->num_lines) {
				st->script_position = -1;
				break;
			}
		}
	}
}

// [=]===^=[ ftm_run_lfo ]========================================================================[=]
static void ftm_run_lfo(struct facethemusic_state *s, struct ftm_voice *v) {
	for(uint32_t i = 0; i < FTM_NUM_LFO; ++i) {
		struct ftm_lfo_state *lfo = &v->lfo_states[i];

		if(lfo->modulation_speed == 0 || lfo->target == FTM_LFO_NOTHING) {
			continue;
		}
		if(lfo->shape_table == 0) {
			continue;
		}

		int32_t sel_idx = (v->sel_state.voice_index >= 0 && v->sel_state.voice_index < FTM_NUM_TRACKS) ? v->sel_state.voice_index : v->channel_number;
		struct ftm_voice *sel_v = &s->voices[sel_idx];

		int32_t shape_pos = lfo->shape_table_position;
		int32_t value = (int32_t)((int8_t)lfo->shape_table[shape_pos]) * (int32_t)lfo->modulation_depth / 128;
		int32_t add_value = value - (int32_t)lfo->modulation_value;

		switch(lfo->target) {
			case FTM_LFO_LFO1_SPEED: {
				int32_t nv = (int32_t)sel_v->lfo_states[0].modulation_speed + add_value;
				sel_v->lfo_states[0].modulation_speed = (uint16_t)ftm_clamp_i32(nv, 0, 95);
				break;
			}

			case FTM_LFO_LFO2_SPEED: {
				int32_t nv = (int32_t)sel_v->lfo_states[1].modulation_speed + add_value;
				sel_v->lfo_states[1].modulation_speed = (uint16_t)ftm_clamp_i32(nv, 0, 95);
				break;
			}

			case FTM_LFO_LFO3_SPEED: {
				int32_t nv = (int32_t)sel_v->lfo_states[2].modulation_speed + add_value;
				sel_v->lfo_states[2].modulation_speed = (uint16_t)ftm_clamp_i32(nv, 0, 95);
				break;
			}

			case FTM_LFO_LFO4_SPEED: {
				int32_t nv = (int32_t)sel_v->lfo_states[3].modulation_speed + add_value;
				sel_v->lfo_states[3].modulation_speed = (uint16_t)ftm_clamp_i32(nv, 0, 95);
				break;
			}

			case FTM_LFO_LFO1_DEPTH: {
				int32_t nv = (int32_t)sel_v->lfo_states[0].modulation_depth + add_value;
				sel_v->lfo_states[0].modulation_depth = (uint16_t)ftm_clamp_i32(nv, 0, 127);
				break;
			}

			case FTM_LFO_LFO2_DEPTH: {
				int32_t nv = (int32_t)sel_v->lfo_states[1].modulation_depth + add_value;
				sel_v->lfo_states[1].modulation_depth = (uint16_t)ftm_clamp_i32(nv, 0, 127);
				break;
			}

			case FTM_LFO_LFO3_DEPTH: {
				int32_t nv = (int32_t)sel_v->lfo_states[2].modulation_depth + add_value;
				sel_v->lfo_states[2].modulation_depth = (uint16_t)ftm_clamp_i32(nv, 0, 127);
				break;
			}

			case FTM_LFO_LFO4_DEPTH: {
				int32_t nv = (int32_t)sel_v->lfo_states[3].modulation_depth + add_value;
				sel_v->lfo_states[3].modulation_depth = (uint16_t)ftm_clamp_i32(nv, 0, 127);
				break;
			}

			case FTM_LFO_TRACK_AMPLITUDE: {
				int32_t nv = (int32_t)sel_v->volume + add_value;
				sel_v->volume = (uint16_t)ftm_clamp_i32(nv, 0, 64);
				break;
			}

			case FTM_LFO_TRACK_FREQUENCY: {
				int32_t nv = (int32_t)sel_v->note_index + (add_value * 2);
				sel_v->note_index = (uint16_t)ftm_clamp_i32(nv, 0, 542);
				break;
			}
		}

		lfo->modulation_value = (int16_t)value;

		shape_pos += (int32_t)lfo->modulation_speed;
		if(shape_pos >= 192) {
			shape_pos -= 192;
			if(!lfo->loop_modulation) {
				lfo->modulation_speed = 0;
			}
		}
		lfo->shape_table_position = shape_pos;
	}
}

// [=]===^=[ ftm_run_effects ]====================================================================[=]
static void ftm_run_effects(struct facethemusic_state *s, struct ftm_voice *v) {
	ftm_run_volume_down(v);
	ftm_run_portamento(v);
	ftm_run_track_sel(s, v);
	ftm_run_lfo(s, v);
}

// [=]===^=[ ftm_setup_hardware ]=================================================================[=]
static void ftm_setup_hardware(struct facethemusic_state *s) {
	for(uint32_t i = 0; i < FTM_NUM_TRACKS; ++i) {
		int32_t chn = s->channel_mapping[i];
		if(chn == -1) {
			paula_mute(&s->paula, (int32_t)i);
			continue;
		}
		struct ftm_voice *v = &s->voices[i];
		int32_t paula_ch = (int32_t)i;

		if(v->retrig_sample) {
			v->retrig_sample = 0;
			uint32_t play_len = (v->sample_total_length > v->sample_start_offset) ? (v->sample_total_length - v->sample_start_offset) : 0;
			paula_play_sample(&s->paula, paula_ch, v->sample_data, v->sample_start_offset + play_len);
			s->paula.ch[paula_ch].pos_fp = v->sample_start_offset << PAULA_FP_SHIFT;
			if(v->sample_loop_length != 0) {
				paula_set_loop(&s->paula, paula_ch, v->sample_loop_start, (uint32_t)v->sample_loop_length * 2U);
			} else {
				paula_set_loop(&s->paula, paula_ch, 0, 0);
			}
		}

		uint32_t period_index = (uint32_t)v->note_index / 2;
		if(period_index >= sizeof(ftm_periods) / sizeof(ftm_periods[0])) {
			period_index = sizeof(ftm_periods) / sizeof(ftm_periods[0]) - 1;
		}
		uint16_t period = ftm_periods[period_index];

		if(period != 0) {
			// C# FaceTheMusic uses SetFrequency, not SetAmigaPeriod, so we
			// stay in Hz the whole way and feed paula_set_freq_hz; otherwise
			// the period round-trip would clamp at PAULA_MIN_PERIOD on the
			// highest notes.
			uint32_t freq = FTM_PAL_CLOCK / period;
			int32_t didx = v->detune_index;
			int16_t detune_raw = (didx >= 0 && didx < 4) ? s->detune_values[didx] : 0;
			int32_t detune = (int32_t)detune_raw * -8;
			if(detune != 0) {
				double mult = pow(2.0, (double)detune / (12.0 * 256.0 * 128.0));
				double new_freq = (double)freq * mult;
				if(new_freq > 0.0 && new_freq < 4.0e9) {
					freq = (uint32_t)new_freq;
				}
			}
			paula_set_freq_hz(&s->paula, paula_ch, freq);
		}

		uint32_t vol = ((uint32_t)v->volume * (uint32_t)s->global_volume) / 64U;
		paula_set_volume(&s->paula, paula_ch, (uint16_t)vol);

		s->paula.ch[paula_ch].pan = ftm_pan_pos[i];
	}
}

// [=]===^=[ ftm_initialize_sound ]===============================================================[=]
static void ftm_initialize_sound(struct facethemusic_state *s, uint16_t start_row, uint16_t end_row) {
	s->start_row = start_row;
	s->end_row = end_row;
	s->global_volume = s->global_volume_init;
	s->speed = s->start_speed;
	s->pattern_loop_stop_row = 0;
	s->pattern_loop_start_row = 0;
	s->do_pattern_loop = 0;
	s->speed_counter = 1;
	s->current_row = start_row;
	s->pattern_loop_stack_count = 0;
	for(uint32_t i = 0; i < 4; ++i) {
		s->detune_values[i] = 0;
	}

	for(uint32_t i = 0; i < FTM_NUM_TRACKS; ++i) {
		struct ftm_voice *v = &s->voices[i];
		memset(v, 0, sizeof(*v));
		v->channel_number = (int32_t)i;
		v->sample_number = -1;
		v->current_sample_index = -1;
		v->sample_data = s->quiet_sample;
		v->sample_oneshot_length = 2;
		v->sample_loop_start = 0;
		v->sample_loop_length = 0;
		v->sample_total_length = 4;
		v->volume = 64;
		v->detune_index = (int32_t)(i / 2);
		v->sel_state.script_index = -1;
		v->sel_state.script_position = -1;
		v->sel_state.voice_index = (int32_t)i;
	}

	for(uint32_t i = 0; i < FTM_NUM_TRACKS; ++i) {
		ftm_adjust_track_indexes(s, &s->voices[i]);
		ftm_initialize_voice_with_latest(s, &s->voices[i]);
	}

	ftm_set_cia_timer(s, s->start_cia_timer_value);
	s->end_reached = 0;
}

// [=]===^=[ ftm_play_tick ]======================================================================[=]
static void ftm_play_tick(struct facethemusic_state *s) {
	s->speed_counter--;
	if(s->speed_counter == 0) {
		s->speed_counter = s->speed;
		ftm_take_next_row(s);
	}

	for(uint32_t i = 0; i < FTM_NUM_TRACKS; ++i) {
		ftm_run_effects(s, &s->voices[i]);
	}

	ftm_setup_hardware(s);
}

// [=]===^=[ ftm_cleanup ]========================================================================[=]
static void ftm_cleanup(struct facethemusic_state *s) {
	if(!s) {
		return;
	}
	for(uint32_t i = 0; i < FTM_MAX_SCRIPTS; ++i) {
		if(s->scripts[i].lines) {
			free(s->scripts[i].lines);
			s->scripts[i].lines = 0;
		}
	}
	for(uint32_t i = 0; i < FTM_NUM_TRACKS; ++i) {
		if(s->tracks[i].lines) {
			free(s->tracks[i].lines);
			s->tracks[i].lines = 0;
		}
	}
}

// [=]===^=[ facethemusic_init ]==================================================================[=]
static struct facethemusic_state *facethemusic_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 82 || sample_rate < 8000) {
		return 0;
	}

	uint8_t *bytes = (uint8_t *)data;
	if(!ftm_check_mark(bytes, 0, len, "FTM")) {
		return 0;
	}
	if(bytes[4] != 3) {
		return 0;
	}

	struct facethemusic_state *s = (struct facethemusic_state *)calloc(1, sizeof(struct facethemusic_state));
	if(!s) {
		return 0;
	}

	s->module_data = bytes;
	s->module_len = len;

	memset(s->quiet_sample, 0, sizeof(s->quiet_sample));

	uint8_t num_sels = 0;
	uint32_t pos = 0;
	if(!ftm_load_header(s, &pos, &num_sels)) {
		ftm_cleanup(s);
		free(s);
		return 0;
	}
	if(!ftm_load_sample_names(s, &pos)) {
		ftm_cleanup(s);
		free(s);
		return 0;
	}
	if(!ftm_load_scripts(s, &pos, num_sels)) {
		ftm_cleanup(s);
		free(s);
		return 0;
	}
	if(!ftm_load_tracks(s, &pos)) {
		ftm_cleanup(s);
		free(s);
		return 0;
	}
	if(!ftm_load_sample_data(s, &pos)) {
		ftm_cleanup(s);
		free(s);
		return 0;
	}

	for(int32_t i = 0, chn = 0; i < FTM_NUM_TRACKS; ++i) {
		if((s->channel_mute_status & (1 << i)) != 0) {
			s->channel_mapping[i] = chn++;
		} else {
			s->channel_mapping[i] = -1;
		}
	}

	paula_init(&s->paula, sample_rate, 50);
	ftm_initialize_sound(s, 0, (uint16_t)(s->number_of_measures * s->rows_per_measure));

	return s;
}

// [=]===^=[ facethemusic_free ]==================================================================[=]
static void facethemusic_free(struct facethemusic_state *s) {
	if(!s) {
		return;
	}
	ftm_cleanup(s);
	free(s);
}

// [=]===^=[ facethemusic_get_audio ]=============================================================[=]
static void facethemusic_get_audio(struct facethemusic_state *s, int16_t *output, int32_t frames) {
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
			ftm_play_tick(s);
		}
	}
}

// [=]===^=[ facethemusic_api_init ]==============================================================[=]
static void *facethemusic_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return facethemusic_init(data, len, sample_rate);
}

// [=]===^=[ facethemusic_api_free ]==============================================================[=]
static void facethemusic_api_free(void *state) {
	facethemusic_free((struct facethemusic_state *)state);
}

// [=]===^=[ facethemusic_api_get_audio ]=========================================================[=]
static void facethemusic_api_get_audio(void *state, int16_t *output, int32_t frames) {
	facethemusic_get_audio((struct facethemusic_state *)state, output, frames);
}

static const char *facethemusic_extensions[] = { "ftm", 0 };

static struct player_api facethemusic_api = {
	"Face The Music",
	facethemusic_extensions,
	facethemusic_api_init,
	facethemusic_api_free,
	facethemusic_api_get_audio,
	0,
};
