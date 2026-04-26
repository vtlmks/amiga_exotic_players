// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// SidMon 1.0 replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct sidmon10_state *sidmon10_init(void *data, uint32_t len, int32_t sample_rate);
//   void sidmon10_free(struct sidmon10_state *s);
//   void sidmon10_get_audio(struct sidmon10_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define SIDMON10_TICK_HZ       50
#define SIDMON10_MAX_SPECIAL   32
#define SIDMON10_MAX_INSTR     63

enum {
	SIDMON10_ENV_ATTACK  = 0,
	SIDMON10_ENV_DECAY   = 1,
	SIDMON10_ENV_SUSTAIN = 2,
	SIDMON10_ENV_RELEASE = 3,
	SIDMON10_ENV_DONE    = 4,
};

struct sidmon10_sequence {
	uint32_t track_number;
	int8_t transpose;
};

struct sidmon10_track_row {
	int8_t note;
	uint8_t instrument;
	uint8_t effect;
	uint8_t effect_param;
	uint8_t duration;
};

struct sidmon10_track {
	struct sidmon10_track_row *rows;
	uint32_t num_rows;
};

struct sidmon10_instrument {
	uint32_t waveform_number;
	uint8_t arpeggio[16];
	uint8_t attack_speed;
	uint8_t attack_max;
	uint8_t decay_speed;
	uint8_t decay_min;
	uint8_t sustain_time;
	uint8_t release_speed;
	uint8_t release_min;
	uint8_t phase_shift;
	uint8_t phase_speed;
	uint8_t fine_tune;
	int8_t pitch_fall;
	uint16_t volume;
};

struct sidmon10_sample {
	int8_t *data;
	uint32_t length;
	int32_t loop_start;
};

struct sidmon10_mix_info {
	uint32_t source1;
	uint32_t source2;
	uint32_t destination;
	uint32_t speed;
};

struct sidmon10_special_sample {
	uint32_t instrument_number;
	int32_t sample_offset;
	int32_t length;
};

struct sidmon10_voice {
	int32_t sequence_index;
	struct sidmon10_sequence *current_sequence;
	int32_t row_index;
	struct sidmon10_track *current_track;
	struct sidmon10_instrument *current_instrument;
	int32_t instrument_number;
	uint16_t note_period;
	uint8_t bend_to;
	int8_t bend_speed;
	int8_t note_offset;
	uint8_t arpeggio_index;
	uint8_t envelope_state;
	uint8_t row_count;
	uint8_t volume;
	uint8_t sustain_control;
	uint16_t pitch_control;
	uint8_t phase_index;
	uint8_t phase_speed;
	uint16_t pitch_fall_control;
	uint8_t wave_index;
	uint8_t waveform_number;
	uint8_t wave_speed;
	uint8_t loop_control;
};

struct sidmon10_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	int32_t base_offset;
	int32_t sequence_offsets[4];
	int32_t track_data_offset;
	int32_t track_offset_table_offset;
	int32_t instrument_offset;
	int32_t waveform_list_offset;
	int32_t waveform_offset;
	int32_t sample_offset;
	int32_t song_data_offset;

	uint32_t no_loop_value;
	uint8_t enable_filter;
	uint8_t enable_mixing;

	struct sidmon10_special_sample special_samples[SIDMON10_MAX_SPECIAL];
	uint32_t num_special_samples;

	struct sidmon10_sequence *sequences[4];
	uint32_t num_positions;

	struct sidmon10_track *tracks;
	uint32_t num_tracks;

	struct sidmon10_instrument *instruments;
	uint32_t num_instruments;

	uint8_t (*waveform_info)[16];
	uint32_t num_waveform_info;

	int8_t (*waveforms)[32];
	uint32_t num_waveforms;

	struct sidmon10_sample *samples;
	uint32_t num_samples;

	uint16_t *periods;
	uint32_t num_periods;
	int32_t fine_tune_multiply;

	struct sidmon10_mix_info mix1;
	struct sidmon10_mix_info mix2;

	uint32_t number_of_rows;
	uint32_t speed;
	uint32_t speed_counter;
	uint8_t new_track;
	uint8_t loop_song;
	int32_t current_row;
	uint32_t current_position;

	uint32_t mix1_counter;
	uint32_t mix1_position;
	uint32_t mix2_counter;
	uint32_t mix2_position;

	struct sidmon10_voice voices[4];

	uint32_t start_number_of_rows;
	uint32_t start_speed;
};

// [=]===^=[ sidmon10_periods_v1 ]================================================================[=]
static uint16_t sidmon10_periods_v1[] = {
	   0,
	                  5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3616,
	3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1808,
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  904,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  452,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,
	   0,    0,    0,    0,    0,    0,    0,
	                                                            4028, 3806,
	3584, 3394, 3204, 3013, 2855, 2696, 2538, 2395, 2268, 2141, 2014, 1903,
	1792, 1697, 1602, 1507, 1428, 1348, 1269, 1198, 1134, 1071, 1007,  952,
	 896,  849,  801,  754,  714,  674,  635,  599,  567,  536,  504,  476,
	 448,  425,  401,  377,  357,  337,  310,  300,  284,  268,  252,  238,
	 224,  213,  201,  189,  179,  169,  159,  150,  142,  134,
	   0,    0,    0,    0,    0,    0,    0,
	                                                            3993, 3773,
	3552, 3364, 3175, 2987, 2830, 2672, 2515, 2374, 2248, 2122, 1997, 1887,
	1776, 1682, 1588, 1494, 1415, 1336, 1258, 1187, 1124, 1061,  999,  944,
	 888,  841,  794,  747,  708,  668,  629,  594,  562,  531,  500,  472,
	 444,  421,  397,  374,  354,  334,  315,  297,  281,  266,  250,  236,
	 222,  211,  199,  187,  177,  167,  158,  149,  141,  133,
	   0,    0,    0,    0,    0,    0,    0,
	                                                            3957, 3739,
	3521, 3334, 3147, 2960, 2804, 2648, 2493, 2353, 2228, 2103, 1979, 1870,
	1761, 1667, 1574, 1480, 1402, 1324, 1247, 1177, 1114, 1052,  990,  935,
	 881,  834,  787,  740,  701,  662,  624,  589,  557,  526,  495,  468,
	 441,  417,  394,  370,  351,  331,  312,  295,  279,  263,  248,  234,
	 221,  209,  197,  185,  176,  166,  156,  148,  140,  132,
	   0,    0,    0,    0,    0,    0,    0,
	                                                            3921, 3705,
	3489, 3304, 3119, 2933, 2779, 2625, 2470, 2331, 2208, 2084, 1961, 1853,
	1745, 1652, 1560, 1467, 1390, 1313, 1235, 1166, 1104, 1042,  981,  927,
	 873,  826,  780,  734,  695,  657,  618,  583,  552,  521,  491,  464,
	 437,  413,  390,  367,  348,  329,  309,  292,  276,  261,  246,  232,
	 219,  207,  195,  184,  174,  165,  155,  146,  138,  131,
	   0,    0,    0,    0,    0,    0,    0,
	                                                            3886, 3671,
	3457, 3274, 3090, 2907, 2754, 2601, 2448, 2310, 2188, 2065, 1943, 1836,
	1729, 1637, 1545, 1454, 1377, 1301, 1224, 1155, 1094, 1033,  972,  918,
	 865,  819,  773,  727,  689,  651,  612,  578,  547,  517,  486,  459,
	 433,  410,  387,  364,  345,  326,  306,  289,  274,  259,  243,  230,
	 217,  205,  194,  182,  173,  163,  153,  145,  137,  130,
	   0,    0,    0,    0,    0,    0,    0,
	                                                            3851, 3638,
	3426, 3244, 3062, 2880, 2729, 2577, 2426, 2289, 2168, 2047, 1926, 1819,
	1713, 1622, 1531, 1440, 1365, 1289, 1213, 1145, 1084, 1024,  963,  910,
	 857,  811,  766,  720,  683,  645,  607,  573,  542,  512,  482,  455,
	 429,  406,  383,  360,  342,  323,  304,  287,  271,  256,  241,  228,
	 215,  203,  192,  180,  171,  162,  152,  144,  136,  128
};

// [=]===^=[ sidmon10_periods_v2 ]================================================================[=]
static uint16_t sidmon10_periods_v2[] = {
	   0,
	                  5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3616,
	3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1808,
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  904,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  452,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,
	   0,    0,    0,    0,    0,    0,    0,    0,
	                                                            4028, 3806,
	3584, 3394, 3204, 3013, 2855, 2696, 2538, 2395, 2268, 2141, 2014, 1903,
	1792, 1697, 1602, 1507, 1428, 1348, 1269, 1198, 1134, 1071, 1007,  952,
	 896,  849,  801,  754,  714,  674,  635,  599,  567,  536,  504,  476,
	 448,  425,  401,  377,  357,  337,  310,  300,  284,  268,  252,  238,
	 224,  213,  201,  189,  179,  169,  159,  150,  142,  134,
	   0,    0,    0,    0,    0,    0,    0,    0,
	                                                            3993, 3773,
	3552, 3364, 3175, 2987, 2830, 2672, 2515, 2374, 2248, 2122, 1997, 1887,
	1776, 1682, 1588, 1494, 1415, 1336, 1258, 1187, 1124, 1061,  999,  944,
	 888,  841,  794,  747,  708,  668,  629,  594,  562,  531,  500,  472,
	 444,  421,  397,  374,  354,  334,  315,  297,  281,  266,  250,  236,
	 222,  211,  199,  187,  177,  167,  158,  149,  141,  133,
	   0,    0,    0,    0,    0,    0,    0,    0,
	                                                            3957, 3739,
	3521, 3334, 3147, 2960, 2804, 2648, 2493, 2353, 2228, 2103, 1979, 1870,
	1761, 1667, 1574, 1480, 1402, 1324, 1247, 1177, 1114, 1052,  990,  935,
	 881,  834,  787,  740,  701,  662,  624,  589,  557,  526,  495,  468,
	 441,  417,  394,  370,  351,  331,  312,  295,  279,  263,  248,  234,
	 221,  209,  197,  185,  176,  166,  156,  148,  140,  132,
	   0,    0,    0,    0,    0,    0,    0,    0,
	                                                            3921, 3705,
	3489, 3304, 3119, 2933, 2779, 2625, 2470, 2331, 2208, 2084, 1961, 1853,
	1745, 1652, 1560, 1467, 1390, 1313, 1235, 1166, 1104, 1042,  981,  927,
	 873,  826,  780,  734,  695,  657,  618,  583,  552,  521,  491,  464,
	 437,  413,  390,  367,  348,  329,  309,  292,  276,  261,  246,  232,
	 219,  207,  195,  184,  174,  165,  155,  146,  138,  131,
	   0,    0,    0,    0,    0,    0,    0,    0,
	                                                            3886, 3671,
	3457, 3274, 3090, 2907, 2754, 2601, 2448, 2310, 2188, 2065, 1943, 1836,
	1729, 1637, 1545, 1454, 1377, 1301, 1224, 1155, 1094, 1033,  972,  918,
	 865,  819,  773,  727,  689,  651,  612,  578,  547,  517,  486,  459,
	 433,  410,  387,  364,  345,  326,  306,  289,  274,  259,  243,  230,
	 217,  205,  194,  182,  173,  163,  153,  145,  137,  130,
	   0,    0,    0,    0,    0,    0,    0,    0,
	                                                            3851, 3638,
	3426, 3244, 3062, 2880, 2729, 2577, 2426, 2289, 2168, 2047, 1926, 1819,
	1713, 1622, 1531, 1440, 1365, 1289, 1213, 1145, 1084, 1024,  963,  910,
	 857,  811,  766,  720,  683,  645,  607,  573,  542,  512,  482,  455,
	 429,  406,  383,  360,  342,  323,  304,  287,  271,  256,  241,  228,
	 215,  203,  192,  180,  171,  162,  152,  144,  136,  128,
	   0,    0,    0,    0,    0
};

// [=]===^=[ sidmon10_periods_v3 ]================================================================[=]
static uint16_t sidmon10_periods_v3[] = {
	   0,
	6848, 6464, 6096, 5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3616,
	3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1808,
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  904,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  452,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,
	   0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0
};

// [=]===^=[ sidmon10_read_u32_be ]===============================================================[=]
static uint32_t sidmon10_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ sidmon10_read_i32_be ]===============================================================[=]
static int32_t sidmon10_read_i32_be(uint8_t *p) {
	return (int32_t)sidmon10_read_u32_be(p);
}

// [=]===^=[ sidmon10_read_s16_be ]===============================================================[=]
static int32_t sidmon10_read_s16_be(uint8_t *buf, int32_t i) {
	return (int32_t)(int16_t)((uint16_t)((buf[i] << 8) | buf[i + 1]));
}

// [=]===^=[ sidmon10_extract_init ]==============================================================[=]
static int32_t sidmon10_extract_init(struct sidmon10_state *s, uint8_t *buf, int32_t len) {
	int32_t index;
	int32_t channel;

	for(index = 0; index < len - 2; index += 2) {
		if((buf[index] == 0x41) && (buf[index + 1] == 0xfa)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	s->base_offset = sidmon10_read_s16_be(buf, index + 2) + index + 2;

	index += 4;
	for(channel = 0; (index < len - 2) && (channel < 4); index += 2) {
		if((buf[index] == 0xd1) && (buf[index + 1] == 0xe8)) {
			s->sequence_offsets[channel++] = sidmon10_read_s16_be(buf, index + 2) + s->base_offset;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	for(; index < len - 2; index += 2) {
		if(((buf[index] == 0x70) && (buf[index + 1] == 0x03)) ||
		   ((buf[index] == 0x20) && (buf[index + 1] == 0x3c) && (buf[index + 5] == 0x03))) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	if(buf[index] == 0x20) {
		index += 4;
	}

	if((buf[index + 2] != 0x41) || (buf[index + 3] != 0xfa) ||
	   (buf[index + 6] != 0xd1) || (buf[index + 7] != 0xe8)) {
		return 0;
	}

	s->track_data_offset = sidmon10_read_s16_be(buf, index + 8) + s->base_offset;
	index += 10;

	for(; index < len - 2; index += 2) {
		if((buf[index] == 0xd9) && (buf[index + 1] == 0xec)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	s->track_offset_table_offset = sidmon10_read_s16_be(buf, index + 2) + s->base_offset;
	index += 4;

	for(; index < len - 2; index += 2) {
		if((buf[index] == 0xd9) && (buf[index + 1] == 0xec)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	s->instrument_offset = sidmon10_read_s16_be(buf, index + 2) + s->base_offset;

	return 1;
}

// [=]===^=[ sidmon10_extract_play ]==============================================================[=]
static int32_t sidmon10_extract_play(struct sidmon10_state *s, uint8_t *buf, int32_t len) {
	int32_t index;
	int32_t index1;

	s->enable_filter = 0;
	s->enable_mixing = 0;
	s->num_special_samples = 0;
	s->waveform_list_offset = 0;
	s->sample_offset = 0;
	s->no_loop_value = 0;

	for(index = 0; index < len - 4; index += 2) {
		if((buf[index] == 0x48) && (buf[index + 1] == 0xe7) &&
		   (buf[index + 2] == 0xff) && (buf[index + 3] == 0xfe)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	for(; index < len - 4; index += 2) {
		if((buf[index] == 0x61) && (buf[index + 1] == 0x00)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	int32_t play_note_offset = sidmon10_read_s16_be(buf, index + 2) + index + 2;

	for(; index < len - 4; index += 2) {
		if((buf[index] == 0xd3) && (buf[index + 1] == 0xe9)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	s->song_data_offset = sidmon10_read_s16_be(buf, index + 2) + s->base_offset;

	for(; index < len - 4; index += 2) {
		if((buf[index] == 0x4c) && (buf[index + 1] == 0xdf)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	index -= 4;

	if(((buf[index] == 0x00) && (buf[index + 1] == 0xdf)) ||
	   ((buf[index] == 0x4e) && (buf[index + 1] == 0x71))) {
		index -= 6;
	}

	for(int32_t i = 0; i < 3; ++i) {
		if((buf[index] != 0x61) || (buf[index + 1] != 0x00)) {
			break;
		}

		int32_t check_offset = sidmon10_read_s16_be(buf, index + 2) + index + 2;

		if((check_offset + 15) >= len) {
			break;
		}

		if((buf[check_offset + 14] == 0x44) && (buf[check_offset + 15] == 0x10)) {
			s->enable_filter = 1;
		} else if(((buf[check_offset + 12] == 0x20) && (buf[check_offset + 13] == 0x10)) ||
		          ((buf[check_offset + 14] == 0x20) && (buf[check_offset + 15] == 0x10))) {
			s->enable_mixing = 1;
		}
		index -= 4;
	}

	for(index = play_note_offset; index < len; index += 2) {
		if((buf[index] == 0xd9) && (buf[index + 1] == 0xec)) {
			break;
		}
	}
	if(index < (len - 4)) {
		s->waveform_list_offset = sidmon10_read_s16_be(buf, index + 2) + s->base_offset;
	} else {
		index = play_note_offset;
	}

	for(; index < len; index += 2) {
		if((buf[index] == 0xeb) && ((buf[index + 1] == 0x81) || (buf[index + 1] == 0x49))) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	for(; index < len; index += 2) {
		if((buf[index] == 0xdd) && (buf[index + 1] == 0xee)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	s->waveform_offset = sidmon10_read_s16_be(buf, index + 2) + s->base_offset;

	for(index1 = index; index1 < len; index1 += 2) {
		if((buf[index1] == 0x16) && (buf[index1 + 1] == 0x2c)) {
			break;
		}
	}

	if(index1 >= (len - 4)) {
		s->periods = sidmon10_periods_v3;
		s->num_periods = sizeof(sidmon10_periods_v3) / sizeof(sidmon10_periods_v3[0]);
		s->fine_tune_multiply = 0;
	} else if((buf[index1 + 4] == 0xc6) && (buf[index1 + 5] == 0xfc)) {
		s->periods = sidmon10_periods_v1;
		s->num_periods = sizeof(sidmon10_periods_v1) / sizeof(sidmon10_periods_v1[0]);
		s->fine_tune_multiply = 67;
	} else {
		s->periods = sidmon10_periods_v2;
		s->num_periods = sizeof(sidmon10_periods_v2) / sizeof(sidmon10_periods_v2[0]);
		s->fine_tune_multiply = 68;
	}

	for(; index >= 0; index -= 2) {
		if((buf[index] == 0x64) && (buf[index + 1] == 0x00)) {
			break;
		}
	}
	if(index < 4) {
		return 0;
	}

	if((buf[index - 2] == 0x00) && (buf[index - 1] == 0x3c)) {
		index = sidmon10_read_s16_be(buf, index + 2) + index + 2;

		s->num_special_samples = 0;
		for(; index < len; index += 2) {
			if((buf[index] == 0x4e) && (buf[index + 1] == 0x75)) {
				break;
			}
			if((buf[index] == 0x0c) && (buf[index + 1] == 0x00)) {
				if(s->num_special_samples >= SIDMON10_MAX_SPECIAL) {
					return 0;
				}
				uint32_t instr_num = ((uint32_t)buf[index + 2] << 8) | (uint32_t)buf[index + 3];
				index += 8;

				if((buf[index] != 0x4d) || (buf[index + 1] != 0xfa)) {
					return 0;
				}

				int32_t start_offset = sidmon10_read_s16_be(buf, index + 2) + index + 2;

				if((buf[index + 4] == 0xdd) && (buf[index + 5] == 0xfc)) {
					start_offset += (int32_t)sidmon10_read_u32_be(buf + index + 6);
					index += 6;
				}

				index += 10;

				if((buf[index] != 0x33) || (buf[index + 1] != 0xfc)) {
					return 0;
				}

				int32_t length = (int32_t)(((uint32_t)buf[index + 2] << 8) | buf[index + 3]) * 2;

				struct sidmon10_special_sample *ss = &s->special_samples[s->num_special_samples++];
				ss->instrument_number = instr_num;
				ss->sample_offset = start_offset;
				ss->length = length;
			}
		}
		if(index >= (len - 4)) {
			return 0;
		}
	} else {
		index = sidmon10_read_s16_be(buf, index + 2) + index + 2;

		for(; index < len; index += 2) {
			if((buf[index] == 0xd9) && (buf[index + 1] == 0xec)) {
				break;
			}
		}
		if(index >= (len - 4)) {
			return 0;
		}

		s->sample_offset = sidmon10_read_s16_be(buf, index + 2) + s->base_offset;

		for(; index < len; index += 2) {
			if((buf[index] == 0x0c) && (buf[index + 1] == 0xab)) {
				break;
			}
		}
		if(index >= (len - 4)) {
			return 0;
		}

		s->no_loop_value = sidmon10_read_u32_be(buf + index + 2);
	}

	return 1;
}

// [=]===^=[ sidmon10_test_module ]===============================================================[=]
static int32_t sidmon10_test_module(struct sidmon10_state *s, uint8_t *buf, int32_t len) {
	if(len < 1024) {
		return 0;
	}
	if((buf[0] == 0x53) && (buf[1] == 0x43) && (buf[2] == 0x36) && (buf[3] == 0x38)) {
		return 0;
	}
	if(!sidmon10_extract_init(s, buf, len)) {
		return 0;
	}
	if(!sidmon10_extract_play(s, buf, len)) {
		return 0;
	}
	return 1;
}

// [=]===^=[ sidmon10_sort_i32 ]==================================================================[=]
static int sidmon10_cmp_i32(const void *a, const void *b) {
	int32_t av = *(const int32_t *)a;
	int32_t bv = *(const int32_t *)b;
	return (av > bv) - (av < bv);
}

// [=]===^=[ sidmon10_find_offset_index ]=========================================================[=]
static int32_t sidmon10_find_offset_index(int32_t *sorted, int32_t n, int32_t value) {
	for(int32_t i = 0; i < n; ++i) {
		if(sorted[i] == value) {
			return i;
		}
	}
	return -1;
}

// [=]===^=[ sidmon10_find_counts ]===============================================================[=]
static int32_t sidmon10_find_counts(struct sidmon10_state *s, int32_t *num_tracks, int32_t *num_instruments, int32_t *num_waveform_list, int32_t *num_waveforms) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	for(int32_t i = 0; i < 4; ++i) {
		if((uint32_t)(s->sequence_offsets[i] + 4) > data_len) {
			return 0;
		}
		s->sequence_offsets[i] = sidmon10_read_i32_be(data + s->sequence_offsets[i]) + s->base_offset;
	}

	if((uint32_t)(s->track_data_offset + 4) > data_len) {
		return 0;
	}
	s->track_data_offset = sidmon10_read_i32_be(data + s->track_data_offset) + s->base_offset;

	if((uint32_t)(s->track_offset_table_offset + 4) > data_len) {
		return 0;
	}
	s->track_offset_table_offset = sidmon10_read_i32_be(data + s->track_offset_table_offset) + s->base_offset;

	if((uint32_t)(s->instrument_offset + 4) > data_len) {
		return 0;
	}
	s->instrument_offset = sidmon10_read_i32_be(data + s->instrument_offset) + s->base_offset;

	if(s->waveform_list_offset != 0) {
		if((uint32_t)(s->waveform_list_offset + 4) > data_len) {
			return 0;
		}
		s->waveform_list_offset = sidmon10_read_i32_be(data + s->waveform_list_offset) + s->base_offset;
	}

	if((uint32_t)(s->waveform_offset + 4) > data_len) {
		return 0;
	}
	s->waveform_offset = sidmon10_read_i32_be(data + s->waveform_offset) + s->base_offset;

	if(s->sample_offset != 0) {
		if((uint32_t)(s->sample_offset + 4) > data_len) {
			return 0;
		}
		s->sample_offset = sidmon10_read_i32_be(data + s->sample_offset) + s->base_offset;
	}

	if((uint32_t)(s->song_data_offset + 4) > data_len) {
		return 0;
	}
	s->song_data_offset = sidmon10_read_i32_be(data + s->song_data_offset) + s->base_offset;

	int32_t sorted[11];
	int32_t n = 0;
	sorted[n++] = s->sequence_offsets[0];
	sorted[n++] = s->sequence_offsets[1];
	sorted[n++] = s->sequence_offsets[2];
	sorted[n++] = s->sequence_offsets[3];
	sorted[n++] = s->track_data_offset;
	sorted[n++] = s->track_offset_table_offset;
	sorted[n++] = s->instrument_offset;
	sorted[n++] = s->waveform_list_offset;
	sorted[n++] = s->waveform_offset;
	sorted[n++] = s->sample_offset;
	sorted[n++] = s->song_data_offset;
	qsort(sorted, n, sizeof(int32_t), sidmon10_cmp_i32);

	int32_t idx = sidmon10_find_offset_index(sorted, n, s->track_offset_table_offset);
	if(idx == (n - 1)) {
		int32_t count = 1;
		int32_t pos = s->track_offset_table_offset + 4;
		while((uint32_t)(pos + 4) <= data_len) {
			uint32_t offset = sidmon10_read_u32_be(data + pos);
			if((offset == 0) || (offset > data_len)) {
				break;
			}
			count++;
			pos += 4;
		}
		*num_tracks = count;
	} else {
		*num_tracks = (sorted[idx + 1] - sorted[idx]) / 4;
	}

	idx = sidmon10_find_offset_index(sorted, n, s->instrument_offset);
	*num_instruments = (sorted[idx + 1] - sorted[idx]) / 32;
	if(*num_instruments > SIDMON10_MAX_INSTR) {
		*num_instruments = SIDMON10_MAX_INSTR;
	}

	if(s->waveform_list_offset != 0) {
		idx = sidmon10_find_offset_index(sorted, n, s->waveform_list_offset);
		*num_waveform_list = (sorted[idx + 1] - sorted[idx]) / 16;
	} else {
		*num_waveform_list = 0;
	}

	idx = sidmon10_find_offset_index(sorted, n, s->waveform_offset);
	*num_waveforms = (sorted[idx + 1] - sorted[idx]) / 32;

	return 1;
}

// [=]===^=[ sidmon10_load_song_data ]============================================================[=]
static int32_t sidmon10_load_song_data(struct sidmon10_state *s, uint32_t *num_positions) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	if((uint32_t)(s->song_data_offset + 11 * 4) > data_len) {
		return 0;
	}

	uint8_t *p = data + s->song_data_offset;
	s->mix1.source1     = sidmon10_read_u32_be(p +  0);
	s->mix2.source1     = sidmon10_read_u32_be(p +  4);
	s->mix1.source2     = sidmon10_read_u32_be(p +  8);
	s->mix2.source2     = sidmon10_read_u32_be(p + 12);
	s->mix1.destination = sidmon10_read_u32_be(p + 16);
	s->mix2.destination = sidmon10_read_u32_be(p + 20);

	s->start_number_of_rows = sidmon10_read_u32_be(p + 24);
	uint32_t pos_val = sidmon10_read_u32_be(p + 28);
	*num_positions = pos_val - 1;

	s->start_speed  = sidmon10_read_u32_be(p + 32);
	s->mix1.speed   = sidmon10_read_u32_be(p + 36);
	s->mix2.speed   = sidmon10_read_u32_be(p + 40);

	return 1;
}

// [=]===^=[ sidmon10_load_instruments ]==========================================================[=]
static int32_t sidmon10_load_instruments(struct sidmon10_state *s, int32_t num_instruments) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	int32_t extra = (int32_t)s->num_special_samples;
	int32_t total = num_instruments + extra;

	s->instruments = (struct sidmon10_instrument *)calloc((size_t)total, sizeof(struct sidmon10_instrument));
	if(!s->instruments) {
		return 0;
	}
	s->num_instruments = (uint32_t)total;

	if((uint32_t)(s->instrument_offset + num_instruments * 32) > data_len) {
		return 0;
	}

	for(int32_t i = 0; i < num_instruments; ++i) {
		uint8_t *p = data + s->instrument_offset + i * 32;
		struct sidmon10_instrument *ins = &s->instruments[i];

		ins->waveform_number = sidmon10_read_u32_be(p);
		memcpy(ins->arpeggio, p + 4, 16);
		ins->attack_speed = p[20];
		ins->attack_max   = p[21];
		ins->decay_speed  = p[22];
		ins->decay_min    = p[23];
		ins->sustain_time = p[24];
		// p[25] skipped
		ins->release_speed = p[26];
		ins->release_min   = p[27];
		ins->phase_shift   = p[28];
		ins->phase_speed   = p[29];
		ins->fine_tune     = p[30];
		ins->pitch_fall    = (int8_t)p[31];
		ins->volume        = 0;

		if(extra != 0) {
			ins->pitch_fall = (int8_t)ins->fine_tune;
			ins->fine_tune = 0;
		} else if(ins->fine_tune > 6) {
			ins->fine_tune = 0;
		}
	}

	for(int32_t i = 0; i < extra; ++i) {
		struct sidmon10_special_sample *ss = &s->special_samples[i];
		struct sidmon10_instrument *ins = &s->instruments[num_instruments + i];
		ins->waveform_number = ss->instrument_number - 60 + 16;
		ins->volume = 64;
	}

	return 1;
}

// [=]===^=[ sidmon10_load_single_track ]=========================================================[=]
static int32_t sidmon10_load_single_track(struct sidmon10_state *s, int32_t track_offset, struct sidmon10_track *out_track) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	uint32_t pos = (uint32_t)s->track_data_offset + (uint32_t)track_offset;
	uint32_t row_line = 0;
	uint32_t num_rows = 0;

	uint32_t capacity = 16;
	struct sidmon10_track_row *rows = (struct sidmon10_track_row *)malloc(capacity * sizeof(struct sidmon10_track_row));
	if(!rows) {
		return 0;
	}

	while(row_line < 64) {
		if(pos + 5 > data_len) {
			free(rows);
			return 0;
		}
		if(num_rows == capacity) {
			capacity *= 2;
			struct sidmon10_track_row *nrows = (struct sidmon10_track_row *)realloc(rows, capacity * sizeof(struct sidmon10_track_row));
			if(!nrows) {
				free(rows);
				return 0;
			}
			rows = nrows;
		}
		struct sidmon10_track_row *r = &rows[num_rows++];
		r->note         = (int8_t)data[pos + 0];
		r->instrument   = data[pos + 1];
		r->effect       = data[pos + 2];
		r->effect_param = data[pos + 3];
		r->duration     = data[pos + 4];
		pos += 5;

		if((s->num_special_samples > 0) && (r->instrument >= 60)) {
			r->instrument = (uint8_t)(s->num_instruments - s->num_special_samples + (r->instrument - 60));
		}

		row_line += r->duration + 1;
	}

	out_track->rows = rows;
	out_track->num_rows = num_rows;
	return 1;
}

// [=]===^=[ sidmon10_load_tracks ]===============================================================[=]
static int32_t sidmon10_load_tracks(struct sidmon10_state *s, int32_t num_tracks) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	if((uint32_t)(s->track_offset_table_offset + num_tracks * 4) > data_len) {
		return 0;
	}

	s->tracks = (struct sidmon10_track *)calloc((size_t)num_tracks, sizeof(struct sidmon10_track));
	if(!s->tracks) {
		return 0;
	}
	s->num_tracks = (uint32_t)num_tracks;

	for(int32_t i = 0; i < num_tracks; ++i) {
		int32_t track_offset = sidmon10_read_i32_be(data + s->track_offset_table_offset + i * 4);
		if(!sidmon10_load_single_track(s, track_offset, &s->tracks[i])) {
			return 0;
		}
	}

	return 1;
}

// [=]===^=[ sidmon10_load_sequences ]============================================================[=]
static int32_t sidmon10_load_sequences(struct sidmon10_state *s, uint32_t num_positions) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	s->num_positions = num_positions;

	for(int32_t i = 0; i < 4; ++i) {
		if((uint32_t)(s->sequence_offsets[i] + num_positions * 6) > data_len) {
			return 0;
		}
		s->sequences[i] = (struct sidmon10_sequence *)calloc(num_positions, sizeof(struct sidmon10_sequence));
		if(!s->sequences[i]) {
			return 0;
		}
		for(uint32_t j = 0; j < num_positions; ++j) {
			uint8_t *p = data + s->sequence_offsets[i] + j * 6;
			s->sequences[i][j].track_number = sidmon10_read_u32_be(p);
			s->sequences[i][j].transpose = (int8_t)p[5];
		}
	}

	return 1;
}

// [=]===^=[ sidmon10_load_waveforms ]============================================================[=]
static int32_t sidmon10_load_waveforms(struct sidmon10_state *s, int32_t num_waveform_list, int32_t num_waveforms) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	if(s->waveform_list_offset == 0) {
		s->num_waveform_info = s->num_instruments;
		s->waveform_info = (uint8_t (*)[16])calloc(s->num_instruments, 16);
		if(!s->waveform_info) {
			return 0;
		}
		for(uint32_t i = 0; i < s->num_instruments; ++i) {
			s->waveform_info[i][0]  = (uint8_t)(i + 1);
			s->waveform_info[i][1]  = 0x01;
			s->waveform_info[i][2]  = 0xff;
			s->waveform_info[i][3]  = 0x10;
		}
	} else {
		if((uint32_t)(s->waveform_list_offset + num_waveform_list * 16) > data_len) {
			return 0;
		}
		s->num_waveform_info = (uint32_t)num_waveform_list;
		s->waveform_info = (uint8_t (*)[16])calloc((size_t)num_waveform_list, 16);
		if(!s->waveform_info) {
			return 0;
		}
		memcpy(s->waveform_info, data + s->waveform_list_offset, (size_t)num_waveform_list * 16);
	}

	if((uint32_t)(s->waveform_offset + num_waveforms * 32) > data_len) {
		return 0;
	}

	s->num_waveforms = (uint32_t)num_waveforms;
	s->waveforms = (int8_t (*)[32])calloc((size_t)num_waveforms, 32);
	if(!s->waveforms) {
		return 0;
	}
	memcpy(s->waveforms, data + s->waveform_offset, (size_t)num_waveforms * 32);

	return 1;
}

// [=]===^=[ sidmon10_load_samples ]==============================================================[=]
static int32_t sidmon10_load_samples(struct sidmon10_state *s) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	uint32_t max_wf = 0;
	uint32_t ordered_count = 0;
	uint32_t ordered[SIDMON10_MAX_INSTR + SIDMON10_MAX_SPECIAL];

	for(uint32_t i = 0; i < s->num_instruments; ++i) {
		if(s->instruments[i].waveform_number >= 16) {
			uint32_t wf = s->instruments[i].waveform_number - 16;
			uint32_t j;
			for(j = 0; j < ordered_count; ++j) {
				if(ordered[j] == wf) {
					break;
				}
			}
			if(j == ordered_count) {
				ordered[ordered_count++] = wf;
				if(wf > max_wf) {
					max_wf = wf;
				}
			}
		}
	}

	// insertion sort ordered[]
	for(uint32_t i = 1; i < ordered_count; ++i) {
		uint32_t key = ordered[i];
		int32_t j = (int32_t)i - 1;
		while(j >= 0 && ordered[j] > key) {
			ordered[j + 1] = ordered[j];
			--j;
		}
		ordered[j + 1] = key;
	}

	if(s->sample_offset == 0) {
		s->num_samples = s->num_special_samples;
		s->samples = (struct sidmon10_sample *)calloc(s->num_samples, sizeof(struct sidmon10_sample));
		if(!s->samples) {
			return 0;
		}
		for(uint32_t i = 0; i < s->num_special_samples; ++i) {
			struct sidmon10_special_sample *ss = &s->special_samples[i];
			if((uint32_t)ss->sample_offset + (uint32_t)ss->length > data_len) {
				return 0;
			}
			s->samples[i].data = (int8_t *)(data + ss->sample_offset);
			s->samples[i].length = (uint32_t)ss->length;
			s->samples[i].loop_start = -1;
		}
		return 1;
	}

	// Allocate sample array sized to max_wf + 1 so index by waveform_number - 16 works.
	uint32_t num = max_wf + 1;
	s->num_samples = num;
	s->samples = (struct sidmon10_sample *)calloc(num, sizeof(struct sidmon10_sample));
	if(!s->samples) {
		return 0;
	}

	if((uint32_t)(s->sample_offset + 4) > data_len) {
		return 0;
	}
	uint32_t sample_base_offset = sidmon10_read_u32_be(data + s->sample_offset);
	int32_t sample_info_offset = s->sample_offset + 4;

	uint32_t first_sample_position = (sample_base_offset != 0) ? sample_base_offset : 0xffffffffu;

	for(uint32_t oi = 0; oi < ordered_count; ++oi) {
		uint32_t sample_number = ordered[oi];
		uint32_t info_pos = (uint32_t)sample_info_offset + sample_number * 32;
		if((uint32_t)(info_pos + 28) > data_len) {
			return 0;
		}
		if(sample_number * 32 >= first_sample_position) {
			// skip: would overlap actual sample data
			continue;
		}

		uint32_t start_offset = sidmon10_read_u32_be(data + info_pos + 0);
		int32_t loop_start    = sidmon10_read_i32_be(data + info_pos + 4);
		uint32_t end_offset   = sidmon10_read_u32_be(data + info_pos + 8);

		if((uint32_t)loop_start == s->no_loop_value || loop_start >= (int32_t)end_offset || start_offset > (uint32_t)loop_start) {
			loop_start = -1;
		} else {
			loop_start -= (int32_t)start_offset;
		}

		int32_t length = (int32_t)(end_offset - start_offset);
		if(length <= 0) {
			continue;
		}

		uint32_t sample_position = (uint32_t)sample_info_offset + sample_base_offset + start_offset;
		if(sample_position < first_sample_position) {
			first_sample_position = sample_position;
		}

		if((uint32_t)(sample_position + length) > data_len) {
			return 0;
		}

		s->samples[sample_number].data = (int8_t *)(data + sample_position);
		s->samples[sample_number].length = (uint32_t)length;
		s->samples[sample_number].loop_start = loop_start;
	}

	return 1;
}

// [=]===^=[ sidmon10_cleanup ]===================================================================[=]
static void sidmon10_cleanup(struct sidmon10_state *s) {
	if(!s) {
		return;
	}
	for(int32_t i = 0; i < 4; ++i) {
		free(s->sequences[i]);
		s->sequences[i] = 0;
	}
	if(s->tracks) {
		for(uint32_t i = 0; i < s->num_tracks; ++i) {
			free(s->tracks[i].rows);
		}
		free(s->tracks);
		s->tracks = 0;
	}
	free(s->instruments); s->instruments = 0;
	free(s->waveform_info); s->waveform_info = 0;
	free(s->waveforms); s->waveforms = 0;
	free(s->samples); s->samples = 0;
}

// [=]===^=[ sidmon10_initialize_sound ]==========================================================[=]
static void sidmon10_initialize_sound(struct sidmon10_state *s) {
	s->number_of_rows = s->start_number_of_rows;
	s->speed         = s->start_speed;
	s->speed_counter = s->start_speed;
	s->new_track     = 0;
	s->loop_song     = 0;
	s->current_row   = -1;
	s->current_position = 1;
	s->mix1_counter = 0;
	s->mix1_position = 0;
	s->mix2_counter = 0;
	s->mix2_position = 0;

	for(int32_t i = 0; i < 4; ++i) {
		struct sidmon10_voice *v = &s->voices[i];
		memset(v, 0, sizeof(*v));
		v->note_period = 0x9999;
		v->envelope_state = SIDMON10_ENV_DONE;
		v->current_sequence = &s->sequences[i][0];
		v->current_track = &s->tracks[v->current_sequence->track_number];
		int32_t instr_number = (int32_t)v->current_track->rows[0].instrument - 1;
		if((instr_number >= 0) && ((uint32_t)instr_number < s->num_instruments)) {
			v->current_instrument = &s->instruments[instr_number];
			v->instrument_number = instr_number;
		}
	}
}

// [=]===^=[ sidmon10_process_envelope ]==========================================================[=]
static int32_t sidmon10_process_envelope(struct sidmon10_voice *v) {
	struct sidmon10_instrument *instr = v->current_instrument;
	if(instr->volume != 0) {
		return (int32_t)instr->volume * 4;
	}

	int32_t volume = 0;
	switch(v->envelope_state) {
		case SIDMON10_ENV_ATTACK: {
			volume = (int32_t)v->volume + (int32_t)instr->attack_speed;
			if(volume > instr->attack_max) {
				volume = instr->attack_max;
				v->envelope_state = SIDMON10_ENV_DECAY;
			}
			v->volume = (uint8_t)volume;
			break;
		}
		case SIDMON10_ENV_DECAY: {
			volume = (int32_t)v->volume - (int32_t)instr->decay_speed;
			if(volume <= instr->decay_min) {
				volume = instr->decay_min;
				v->sustain_control = instr->sustain_time;
				v->envelope_state = SIDMON10_ENV_SUSTAIN;
			}
			v->volume = (uint8_t)volume;
			break;
		}
		case SIDMON10_ENV_SUSTAIN: {
			volume = v->volume;
			v->sustain_control--;
			if(v->sustain_control == 0) {
				v->envelope_state = SIDMON10_ENV_RELEASE;
			}
			break;
		}
		case SIDMON10_ENV_RELEASE: {
			volume = (int32_t)v->volume - (int32_t)instr->release_speed;
			if(volume <= instr->release_min) {
				volume = instr->release_min;
				v->envelope_state = SIDMON10_ENV_DONE;
			}
			v->volume = (uint8_t)volume;
			break;
		}
		default: break;
	}
	return volume;
}

// [=]===^=[ sidmon10_do_arpeggio ]===============================================================[=]
static uint32_t sidmon10_do_arpeggio(struct sidmon10_state *s, struct sidmon10_voice *v) {
	struct sidmon10_instrument *instr = v->current_instrument;
	v->arpeggio_index++;
	v->arpeggio_index &= 0x0f;

	uint8_t arp = instr->arpeggio[v->arpeggio_index];
	int32_t index = (int32_t)v->note_offset + (int32_t)arp + (int32_t)instr->fine_tune * s->fine_tune_multiply;
	uint16_t period = (index < 0 || index >= (int32_t)s->num_periods) ? 0 : s->periods[index];
	v->note_period = period;
	return period;
}

// [=]===^=[ sidmon10_do_bending ]================================================================[=]
static void sidmon10_do_bending(struct sidmon10_state *s, struct sidmon10_voice *v) {
	if(v->bend_speed == 0) {
		return;
	}
	int16_t bend_speed = (int16_t)(-v->bend_speed);
	int32_t index = (int32_t)v->bend_to + (int32_t)v->current_instrument->fine_tune * s->fine_tune_multiply;
	if(index < 0 || index >= (int32_t)s->num_periods) {
		v->bend_speed = 0;
		return;
	}
	uint16_t bend_to = s->periods[index];

	if(bend_speed < 0) {
		v->pitch_control = (uint16_t)(v->pitch_control + bend_speed);
		v->note_period = (uint16_t)(v->note_period + v->pitch_control);
		if(v->note_period < bend_to) {
			v->note_offset = (int8_t)v->bend_to;
			v->note_period = bend_to;
			v->pitch_control = 0;
			v->bend_speed = 0;
		}
	} else {
		v->pitch_control = (uint16_t)(v->pitch_control + bend_speed);
		v->note_period = (uint16_t)(v->note_period + v->pitch_control);
		if(v->note_period > bend_to) {
			v->note_offset = (int8_t)v->bend_to;
			v->note_period = bend_to;
			v->pitch_control = 0;
			v->bend_speed = 0;
		}
	}
}

// [=]===^=[ sidmon10_do_phase_shift ]============================================================[=]
static void sidmon10_do_phase_shift(struct sidmon10_state *s, struct sidmon10_voice *v) {
	struct sidmon10_instrument *instr = v->current_instrument;
	if(instr->phase_shift == 0) {
		return;
	}
	if(v->phase_speed == 0) {
		uint32_t wf_idx = (uint32_t)instr->phase_shift - 1;
		if(wf_idx < s->num_waveforms) {
			int8_t *wf = s->waveforms[wf_idx];
			v->phase_index++;
			v->phase_index &= 0x1f;
			v->note_period = (uint16_t)(v->note_period + (wf[v->phase_index] / 4));
		}
	} else {
		v->phase_speed--;
	}
}

// [=]===^=[ sidmon10_do_waveform_mixing ]========================================================[=]
static void sidmon10_do_waveform_mixing(struct sidmon10_state *s, struct sidmon10_mix_info *mi, uint32_t *counter, uint32_t *position) {
	if(!s->enable_mixing) {
		return;
	}
	if(mi->speed == 0) {
		return;
	}
	if(*counter == 0) {
		*counter = mi->speed;
		*position = (*position + 1) & 0x1f;
		if((mi->source1 <= s->num_waveforms) && (mi->source2 <= s->num_waveforms) && (mi->destination <= s->num_waveforms)) {
			int8_t *src1 = s->waveforms[mi->source1 - 1];
			int8_t *src2 = s->waveforms[mi->source2 - 1];
			int8_t *dst  = s->waveforms[mi->destination - 1];
			uint32_t pos = *position;
			for(int32_t i = 31; i >= 0; --i) {
				dst[i] = (int8_t)((src1[i] + src2[pos]) / 2);
				pos = (pos - 1) & 0x1f;
			}
		}
	}
	(*counter)--;
}

// [=]===^=[ sidmon10_do_filter_effect ]==========================================================[=]
static void sidmon10_do_filter_effect(struct sidmon10_state *s) {
	if(s->enable_filter && s->num_waveforms > 0) {
		uint32_t pos = s->mix1_position & 0x1f;
		s->waveforms[0][pos] = (int8_t)(-s->waveforms[0][pos]);
	}
}

// [=]===^=[ sidmon10_play_note ]=================================================================[=]
static void sidmon10_play_note(struct sidmon10_state *s, int32_t voice_idx) {
	struct sidmon10_voice *v = &s->voices[voice_idx];
	struct sidmon10_sequence *seq = s->sequences[voice_idx];

	int8_t *sample_address = 0;
	uint32_t sample_length = 0;
	int32_t loop_start = -1;
	int32_t sample_address_set = 0;
	uint32_t period = 0;
	struct sidmon10_instrument *instr = 0;

	if(s->speed_counter == 0) {
		if(s->new_track) {
			v->row_count = 0;
			v->sequence_index++;
			if(s->loop_song) {
				v->sequence_index = 0;
			}
			v->current_sequence = &seq[v->sequence_index];
			v->current_track = &s->tracks[v->current_sequence->track_number];
			v->row_index = 0;
		}

		if(v->row_count == 0) {
			struct sidmon10_track_row *row = &v->current_track->rows[v->row_index];

			if(row->instrument != 0) {
				instr = &s->instruments[row->instrument - 1];

				if(v->loop_control) {
					paula_mute(&s->paula, voice_idx);
					v->loop_control = 0;
				}

				uint32_t wf_num = instr->waveform_number;
				if(wf_num >= 16) {
					uint32_t sample_num = wf_num - 16;
					if(sample_num < s->num_samples && s->samples[sample_num].data) {
						if(v->loop_control) {
							paula_mute(&s->paula, voice_idx);
						}
						sample_address = s->samples[sample_num].data;
						sample_length = s->samples[sample_num].length;
						loop_start = s->samples[sample_num].loop_start;
						sample_address_set = 1;
						v->loop_control = 1;
					}
				}

				v->wave_index = 0;
				v->waveform_number = (uint8_t)wf_num;

				if(!sample_address_set && (wf_num != 0)) {
					uint32_t wi_idx = (uint32_t)v->waveform_number - 1;
					if(wi_idx < s->num_waveform_info) {
						uint8_t *wi = s->waveform_info[wi_idx];
						v->wave_speed = wi[1];
						uint32_t wf_idx = (uint32_t)wi[0] - 1;
						if(wf_idx < s->num_waveforms) {
							sample_address = s->waveforms[wf_idx];
							sample_length = 32;
							loop_start = 0;
						}
					}
				}

				v->current_instrument = instr;
				v->instrument_number = row->instrument - 1;

				v->envelope_state = SIDMON10_ENV_ATTACK;
				v->pitch_fall_control = 0;
				v->pitch_control = 0;
				v->row_count = row->duration;
			} else {
				if(row->note != 0) {
					v->row_count = row->duration;
					if(v->loop_control && (row->note != -1)) {
						uint32_t sample_num = (uint32_t)v->waveform_number - 16;
						if(sample_num < s->num_samples && s->samples[sample_num].data) {
							paula_mute(&s->paula, voice_idx);
							sample_address = s->samples[sample_num].data;
							sample_length = s->samples[sample_num].length;
							loop_start = s->samples[sample_num].loop_start;
							sample_address_set = 1;
							v->loop_control = 1;
						}
					}
				}
			}

			int8_t note = v->current_track->rows[v->row_index].note;
			if(note != 0) {
				v->row_count = v->current_track->rows[v->row_index].duration;
				if(note != -1) {
					struct sidmon10_instrument *cin = v->current_instrument;
					if(cin) {
						int8_t transpose = v->current_sequence->transpose;
						int32_t note_tr = (int32_t)note + (int32_t)transpose;
						int32_t index = note_tr + (int32_t)cin->fine_tune * s->fine_tune_multiply + 1;
						period = (index < 0 || index >= (int32_t)s->num_periods) ? 0 : s->periods[index];
						v->note_period = (uint16_t)period;
						v->note_offset = (int8_t)note_tr;

						v->envelope_state = SIDMON10_ENV_ATTACK;
						v->volume = 0;
						v->sustain_control = 0;
						v->pitch_control = 0;
						v->pitch_fall_control = 0;
						v->bend_speed = 0;
						v->phase_speed = cin->phase_speed;

						uint8_t eff = v->current_track->rows[v->row_index].effect;
						uint8_t par = v->current_track->rows[v->row_index].effect_param;
						switch(eff) {
							case 0: {
								if(par != 0) {
									cin->attack_max   = par;
									cin->attack_speed = par;
									v->wave_speed = 0;
								}
								break;
							}
							case 2: {
								s->speed = par;
								v->wave_speed = 0;
								break;
							}
							case 3: {
								s->number_of_rows = par;
								v->wave_speed = 0;
								break;
							}
							default: {
								v->bend_to = (uint8_t)(eff + transpose);
								v->bend_speed = (int8_t)par;
								break;
							}
						}
					}
				}
			}

			v->row_index++;
		} else {
			v->row_count--;
		}
	}

	instr = v->current_instrument;
	if(instr && (v->waveform_number > 0)) {
		int32_t volume = sidmon10_process_envelope(v);
		period = sidmon10_do_arpeggio(s, v);
		sidmon10_do_bending(s, v);
		sidmon10_do_phase_shift(s, v);

		v->pitch_fall_control = (uint16_t)(v->pitch_fall_control - instr->pitch_fall);
		v->note_period = (uint16_t)(v->note_period + v->pitch_fall_control);

		if(!v->loop_control) {
			if(v->wave_speed == 0) {
				if(v->wave_index != 16) {
					uint32_t wi_idx = (uint32_t)v->waveform_number - 1;
					if(wi_idx < s->num_waveform_info) {
						uint8_t *wi = s->waveform_info[wi_idx];
						if(wi[v->wave_index] == 0xff) {
							v->wave_index = (uint8_t)(wi[v->wave_index + 1] & 0xfe);
						} else {
							uint32_t wf_idx = (uint32_t)wi[v->wave_index] - 1;
							if(wf_idx < s->num_waveforms) {
								sample_address = s->waveforms[wf_idx];
								sample_length = 32;
								loop_start = 0;
							}
							v->wave_speed = wi[v->wave_index + 1];
							v->wave_index += 2;
						}
					}
				}
			} else {
				v->wave_speed--;
			}
		}

		if(period != 0) {
			paula_set_period(&s->paula, voice_idx, v->note_period);
		}
		if(volume != 0) {
			paula_set_volume_256(&s->paula, voice_idx, (uint16_t)volume);
		}
		if(sample_address != 0) {
			paula_play_sample(&s->paula, voice_idx, sample_address, sample_length);
			if(loop_start != -1) {
				paula_set_loop(&s->paula, voice_idx, (uint32_t)loop_start, sample_length - (uint32_t)loop_start);
			}
		}
	}
}

// [=]===^=[ sidmon10_tick ]======================================================================[=]
static void sidmon10_tick(struct sidmon10_state *s) {
	for(int32_t i = 0; i < 4; ++i) {
		sidmon10_play_note(s, i);
	}

	s->speed_counter++;
	if(s->speed_counter > s->speed) {
		s->speed_counter = 0;
	}

	s->new_track = 0;
	s->loop_song = 0;

	if(s->speed_counter == 0) {
		s->current_row++;
		if((uint32_t)s->current_row == s->number_of_rows) {
			s->current_row = 0;
			s->new_track = 1;
			s->current_position++;
			if(s->current_position > s->num_positions) {
				s->current_position = 1;
				s->loop_song = 1;
			}
		}
	}

	sidmon10_do_waveform_mixing(s, &s->mix1, &s->mix1_counter, &s->mix1_position);
	sidmon10_do_waveform_mixing(s, &s->mix2, &s->mix2_counter, &s->mix2_position);
	sidmon10_do_filter_effect(s);
}

// [=]===^=[ sidmon10_init ]======================================================================[=]
static struct sidmon10_state *sidmon10_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 1024 || sample_rate < 8000) {
		return 0;
	}

	struct sidmon10_state *s = (struct sidmon10_state *)calloc(1, sizeof(struct sidmon10_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;

	if(!sidmon10_test_module(s, s->module_data, (int32_t)len)) {
		free(s);
		return 0;
	}

	int32_t num_tracks = 0;
	int32_t num_instruments = 0;
	int32_t num_waveform_list = 0;
	int32_t num_waveforms = 0;
	if(!sidmon10_find_counts(s, &num_tracks, &num_instruments, &num_waveform_list, &num_waveforms)) {
		free(s);
		return 0;
	}

	uint32_t num_positions = 0;
	if(!sidmon10_load_song_data(s, &num_positions)) {
		goto fail;
	}
	if(!sidmon10_load_instruments(s, num_instruments)) {
		goto fail;
	}
	if(!sidmon10_load_tracks(s, num_tracks)) {
		goto fail;
	}
	if(!sidmon10_load_sequences(s, num_positions)) {
		goto fail;
	}
	if(!sidmon10_load_waveforms(s, num_waveform_list, num_waveforms)) {
		goto fail;
	}
	if(!sidmon10_load_samples(s)) {
		goto fail;
	}

	paula_init(&s->paula, sample_rate, SIDMON10_TICK_HZ);
	sidmon10_initialize_sound(s);
	return s;

fail:
	sidmon10_cleanup(s);
	free(s);
	return 0;
}

// [=]===^=[ sidmon10_free ]======================================================================[=]
static void sidmon10_free(struct sidmon10_state *s) {
	if(!s) {
		return;
	}
	sidmon10_cleanup(s);
	free(s);
}

// [=]===^=[ sidmon10_get_audio ]=================================================================[=]
static void sidmon10_get_audio(struct sidmon10_state *s, int16_t *output, int32_t frames) {
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
			sidmon10_tick(s);
		}
	}
}

// [=]===^=[ sidmon10_api_init ]==================================================================[=]
static void *sidmon10_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return sidmon10_init(data, len, sample_rate);
}

// [=]===^=[ sidmon10_api_free ]==================================================================[=]
static void sidmon10_api_free(void *state) {
	sidmon10_free((struct sidmon10_state *)state);
}

// [=]===^=[ sidmon10_api_get_audio ]==============================================================[=]
static void sidmon10_api_get_audio(void *state, int16_t *output, int32_t frames) {
	sidmon10_get_audio((struct sidmon10_state *)state, output, frames);
}

static const char *sidmon10_extensions[] = { "sd1", "sid1", "sid", 0 };

static struct player_api sidmon10_api = {
	"SidMon 1.0",
	sidmon10_extensions,
	sidmon10_api_init,
	sidmon10_api_free,
	sidmon10_api_get_audio,
	0,
};
