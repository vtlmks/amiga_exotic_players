// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Art Of Noise (AON4 / AON8) replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4- or 8-channel Amiga Paula (see paula.h). 50Hz default tick rate,
// adjusted by Fxx tempo command.
//
// Public API:
//   struct artofnoise_state *artofnoise_init(void *data, uint32_t len, int32_t sample_rate);
//   void artofnoise_free(struct artofnoise_state *s);
//   void artofnoise_get_audio(struct artofnoise_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define AON_TICK_HZ              50
#define AON_DEFAULT_TEMPO        125
#define AON_DEFAULT_SPEED        6
#define AON_MAX_CHANNELS         8
#define AON_NUM_ARPEGGIOS        16
#define AON_PATTERN_ROWS         64

// Effects
#define AON_FX_ARPEGGIO          0x00
#define AON_FX_SLIDE_UP          0x01
#define AON_FX_SLIDE_DOWN        0x02
#define AON_FX_TONE_PORTA        0x03
#define AON_FX_VIBRATO           0x04
#define AON_FX_TONE_VOL          0x05
#define AON_FX_VIB_VOL           0x06
#define AON_FX_SET_OFFSET        0x09
#define AON_FX_VOL_SLIDE         0x0a
#define AON_FX_POS_JUMP          0x0b
#define AON_FX_SET_VOLUME        0x0c
#define AON_FX_PATTERN_BREAK     0x0d
#define AON_FX_EXTRA             0x0e
#define AON_FX_SET_SPEED         0x0f
#define AON_FX_NEW_VOLUME        0x10
#define AON_FX_SYNTH_CONTROL     0x11
#define AON_FX_WAVE_SPEED        0x12
#define AON_FX_SET_ARP_SPEED     0x13
#define AON_FX_VOL_VIB           0x14
#define AON_FX_FINESLIDE_PORTUP  0x15
#define AON_FX_FINESLIDE_PORTDN  0x16
#define AON_FX_AVOID_NOISE       0x17
#define AON_FX_OVERSIZE          0x18
#define AON_FX_FINEVOL_VIB       0x19
#define AON_FX_VOL_PORTDN        0x1a
#define AON_FX_VOL_TONE          0x1b
#define AON_FX_FINEVOL_TONE      0x1c
#define AON_FX_TRACK_VOLUME      0x1d
#define AON_FX_WAVE_MODE         0x1e
#define AON_FX_EXTERNAL          0x21

// Extra effects (Exy, upper nibble)
#define AON_EX_SET_FILTER        0x00
#define AON_EX_FINE_SLIDE_UP     0x10
#define AON_EX_FINE_SLIDE_DN     0x20
#define AON_EX_VIB_WAVE          0x40
#define AON_EX_SET_LOOP          0x50
#define AON_EX_PATTERN_LOOP      0x60
#define AON_EX_RETRIG            0x90
#define AON_EX_FINE_VOL_UP       0xa0
#define AON_EX_FINE_VOL_DN       0xb0
#define AON_EX_NOTE_CUT          0xc0
#define AON_EX_NOTE_DELAY        0xd0
#define AON_EX_PATTERN_DELAY     0xe0

// Envelope states
#define AON_ENV_DONE             0
#define AON_ENV_ADD              1
#define AON_ENV_SUB              2

// Instrument types
#define AON_INSTR_SAMPLE         0
#define AON_INSTR_SYNTH          1

struct aon_track_line {
	uint8_t instrument;
	uint8_t note;
	uint8_t arpeggio;
	uint8_t effect;
	uint8_t effect_arg;
};

struct aon_pattern {
	struct aon_track_line *tracks;       // [AON_PATTERN_ROWS * channels]
};

struct aon_instrument {
	uint8_t type;
	uint8_t volume;
	uint8_t fine_tune;
	uint8_t wave_form;
	uint8_t envelope_start;
	uint8_t envelope_add;
	uint8_t envelope_end;
	uint8_t envelope_sub;
	// Sample
	uint32_t start_offset;
	uint32_t length;
	uint32_t loop_start;
	uint32_t loop_length;
	// Synth
	uint8_t synth_length;
	uint8_t vib_param;
	uint8_t vib_delay;
	uint8_t vib_wave;
	uint8_t wave_speed;
	uint8_t wave_length;
	uint8_t wave_loop_start;
	uint8_t wave_loop_length;
	uint8_t wave_loop_control;
};

struct aon_voice {
	uint8_t ch_flag;                     // 0, 1, 3 (new wave); bit 1 = play
	uint8_t last_note;

	int8_t *wave_form;                   // pointer into module waveform buffer
	uint32_t wave_form_offset;
	uint16_t wave_len;                   // /2
	uint16_t old_wave_len;
	int8_t *repeat_start;
	uint32_t repeat_offset;
	uint16_t repeat_length;              // /2
	int32_t repeat_buffer_len;           // length of repeat_start buffer in bytes (clamping)

	int8_t *wave_form_buffer_end;        // unused, kept for clarity
	int32_t wave_form_buffer_len;        // length of wave_form buffer (for clamping)

	int32_t instrument;                  // -1 = none
	int16_t instrument_number;
	uint8_t volume;

	uint8_t step_fx_cnt;
	uint8_t ch_mode;                     // 0 = Sample, 1 = Synth

	uint16_t period;
	int16_t per_slide;

	uint16_t arpeggio_off;
	uint8_t arpeggio_fine_tune;
	int16_t arpeggio_tab[8];
	uint8_t arpeggio_spd;
	uint8_t arpeggio_cnt;

	int8_t *synth_wave_act;
	uint32_t synth_wave_act_offset;
	uint32_t synth_wave_end_offset;
	int8_t *synth_wave_rep;
	uint32_t synth_wave_rep_offset;
	uint32_t synth_wave_rep_end_offset;
	int32_t synth_wave_add_bytes;
	uint8_t synth_wave_cnt;
	uint8_t synth_wave_spd;
	uint8_t synth_wave_rep_ctrl;
	uint8_t synth_wave_cont;
	uint8_t synth_wave_stop;
	uint8_t synth_add;
	uint8_t synth_sub;
	uint8_t synth_end;
	uint8_t synth_env;
	uint8_t synth_vol;

	uint8_t vib_on;
	uint8_t vib_done;
	uint8_t vib_cont;
	uint8_t vibrato_spd;
	uint8_t vibrato_ampl;
	uint8_t vibrato_pos;
	int16_t vibrato_trig_delay;

	uint8_t fx_com;
	uint8_t fx_dat;

	uint8_t slide_flag;

	uint32_t old_sample_offset;
	uint8_t gliss_spd;

	uint8_t track_volume;
};

struct artofnoise_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	int32_t module_type;                 // 4 or 8
	int32_t num_channels;

	uint8_t number_of_positions;
	uint8_t restart_position;
	uint8_t *position_list;
	uint32_t position_list_len;

	struct aon_pattern *patterns;
	uint32_t num_patterns;

	struct aon_instrument *instruments;
	uint32_t num_instruments;

	uint8_t arpeggios[AON_NUM_ARPEGGIOS][4];

	int8_t **wave_forms;                 // each wave_forms[i] points into module_data
	uint32_t *wave_form_lengths;
	uint32_t num_wave_forms;

	// Global playing info
	uint8_t tempo;
	uint8_t speed;
	uint8_t frame_cnt;

	uint8_t pattern_break;
	uint8_t pat_cnt;
	int8_t pat_delay_cnt;

	uint8_t loop_flag;
	uint8_t loop_point;
	uint8_t loop_cnt;

	uint8_t position;
	uint8_t new_position;
	uint8_t current_pattern;

	uint8_t noise_avoid;
	uint8_t oversize;

	uint8_t event[3];

	uint8_t end_reached;
	uint8_t restart_song;

	struct aon_voice voices[AON_MAX_CHANNELS];
};

// [=]===^=[ aon_periods ]========================================================================[=]
// 16 fine tunes (0..7, then -8..-1), 5 octaves x 12 notes = 60 entries each
static uint16_t aon_periods[16][60] = {
	{
		3434,3232,3048,2880,2712,2560,2416,2280,2152,2032,1920,1812,
		1712,1616,1524,1440,1356,1280,1208,1140,1076,1016, 960, 906,
		 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
		 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
		 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113
	},
	{
		3400,3208,3028,2860,2696,2548,2404,2268,2140,2020,1908,1800,
		1700,1604,1514,1430,1348,1274,1202,1134,1070,1010, 954, 900,
		 850, 802, 757, 715, 674, 637, 601, 567, 535, 505, 477, 450,
		 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 239, 225,
		 213, 201, 189, 179, 169, 159, 150, 142, 134, 126, 119, 113
	},
	{
		3376,3184,3008,2836,2680,2528,2388,2252,2128,2008,1896,1788,
		1688,1592,1504,1418,1340,1264,1194,1126,1064,1004, 948, 894,
		 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447,
		 422, 398, 376, 355, 335, 316, 298, 282, 266, 251, 237, 224,
		 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118, 112
	},
	{
		3352,3164,2984,2816,2660,2512,2368,2236,2112,1992,1880,1776,
		1676,1582,1492,1408,1330,1256,1184,1118,1056, 996, 940, 888,
		 838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470, 444,
		 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235, 222,
		 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118, 111
	},
	{
		3328,3140,2964,2796,2640,2492,2352,2220,2096,1980,1868,1764,
		1664,1570,1482,1398,1320,1246,1176,1110,1048, 990, 934, 882,
		 832, 785, 741, 699, 660, 623, 588, 555, 524, 495, 467, 441,
		 416, 392, 370, 350, 330, 312, 294, 278, 262, 247, 233, 220,
		 208, 196, 185, 175, 165, 156, 147, 139, 131, 124, 117, 110
	},
	{
		3304,3116,2944,2776,2620,2476,2336,2204,2080,1964,1852,1748,
		1652,1558,1472,1388,1310,1238,1168,1102,1040, 982, 926, 874,
		 826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437,
		 413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232, 219,
		 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116, 109
	},
	{
		3280,3096,2920,2756,2604,2456,2320,2188,2064,1948,1840,1736,
		1640,1548,1460,1378,1302,1228,1160,1094,1032, 974, 920, 868,
		 820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434,
		 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230, 217,
		 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115, 109
	},
	{
		3256,3072,2900,2736,2584,2440,2300,2172,2052,1936,1828,1724,
		1628,1536,1450,1368,1292,1220,1150,1086,1026, 968, 914, 862,
		 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431,
		 407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228, 216,
		 204, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114, 108
	},
	{
		3628,3424,3232,3048,2880,2712,2560,2416,2280,2152,2032,1920,
		1814,1712,1616,1524,1440,1356,1280,1208,1140,1076,1016, 960,
		 907, 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480,
		 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240,
		 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120
	},
	{
		3600,3400,3208,3028,2860,2700,2544,2404,2268,2140,2020,1908,
		1800,1700,1604,1514,1430,1350,1272,1202,1134,1070,1010, 954,
		 900, 850, 802, 757, 715, 675, 636, 601, 567, 535, 505, 477,
		 450, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 238,
		 225, 212, 200, 189, 179, 169, 159, 150, 142, 134, 126, 119
	},
	{
		3576,3376,3184,3008,2836,2680,2528,2388,2252,2128,2008,1896,
		1788,1688,1592,1504,1418,1340,1264,1194,1126,1064,1004, 948,
		 894, 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474,
		 447, 422, 398, 376, 355, 335, 316, 298, 282, 266, 251, 237,
		 223, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118
	},
	{
		3548,3352,3164,2984,2816,2660,2512,2368,2236,2112,1992,1880,
		1774,1676,1582,1492,1408,1330,1256,1184,1118,1056, 996, 940,
		 887, 838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470,
		 444, 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235,
		 222, 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118
	},
	{
		3524,3328,3140,2964,2796,2640,2492,2352,2220,2096,1976,1868,
		1762,1664,1570,1482,1398,1320,1246,1176,1110,1048, 988, 934,
		 881, 832, 785, 741, 699, 660, 623, 588, 555, 524, 494, 467,
		 441, 416, 392, 370, 350, 330, 312, 294, 278, 262, 247, 233,
		 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 123, 117
	},
	{
		3500,3304,3116,2944,2776,2620,2476,2336,2204,2080,1964,1852,
		1750,1652,1558,1472,1388,1310,1238,1168,1102,1040, 982, 926,
		 875, 826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463,
		 437, 413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232,
		 219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116
	},
	{
		3472,3280,3096,2920,2756,2604,2456,2320,2188,2064,1948,1840,
		1736,1640,1548,1460,1378,1302,1228,1160,1094,1032, 974, 920,
		 868, 820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460,
		 434, 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230,
		 217, 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115
	},
	{
		3448,3256,3072,2900,2736,2584,2440,2300,2172,2052,1936,1828,
		1724,1628,1536,1450,1368,1292,1220,1150,1086,1026, 968, 914,
		 862, 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457,
		 431, 407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228,
		 216, 203, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114
	}
};

// [=]===^=[ aon_vibrato_sine ]===================================================================[=]
static uint8_t aon_vibrato_sine[32] = {
	  0, 24, 49, 74, 97,120,141,161,
	180,197,212,224,235,244,250,253,
	255,253,250,244,235,224,212,197,
	180,161,141,120, 97, 74, 49, 24
};

// [=]===^=[ aon_vibrato_ramp_down ]==============================================================[=]
static uint8_t aon_vibrato_ramp_down[32] = {
	255,248,240,232,224,216,208,200,192,184,176,168,160,152,144,
	136,128,120,112,104, 96, 88, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8
};

// [=]===^=[ aon_vibrato_square ]=================================================================[=]
static uint8_t aon_vibrato_square[32] = {
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

// [=]===^=[ aon_nibble_tab ]=====================================================================[=]
static int8_t aon_nibble_tab[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1
};

// [=]===^=[ aon_pan4 ]===========================================================================[=]
// Amiga LRRL: 0=L, 127=R
static uint8_t aon_pan4[4] = { 0, 127, 127, 0 };

// [=]===^=[ aon_pan8 ]===========================================================================[=]
static uint8_t aon_pan8[8] = { 0, 0, 127, 127, 127, 127, 0, 0 };

// [=]===^=[ aon_read_u32_be ]====================================================================[=]
static uint32_t aon_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ aon_check_id ]=======================================================================[=]
static int32_t aon_check_id(uint8_t *data, uint32_t pos, uint32_t len, const char *mark) {
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

// [=]===^=[ aon_identify ]=======================================================================[=]
static int32_t aon_identify(uint8_t *data, uint32_t len) {
	if(len < 54) {
		return 0;
	}
	if(aon_check_id(data, 0, len, "AON4")) {
		return 4;
	}
	if(aon_check_id(data, 0, len, "AON8")) {
		return 8;
	}
	return 0;
}

// [=]===^=[ aon_set_bpm_tempo ]==================================================================[=]
// NostalgicPlayer SetBpmTempo: paula tick interval = sample_rate * 2.5 / bpm
static void aon_set_bpm_tempo(struct artofnoise_state *s, uint8_t bpm) {
	if(bpm < 32) {
		bpm = 32;
	}
	s->tempo = bpm;
	int32_t spt = (int32_t)(((int64_t)s->paula.sample_rate * 5) / ((int64_t)bpm * 2));
	if(spt < 1) {
		spt = 1;
	}
	s->paula.samples_per_tick = spt;
}

// [=]===^=[ aon_cleanup ]========================================================================[=]
static void aon_cleanup(struct artofnoise_state *s) {
	if(!s) {
		return;
	}
	if(s->patterns) {
		for(uint32_t i = 0; i < s->num_patterns; ++i) {
			free(s->patterns[i].tracks);
		}
		free(s->patterns);
		s->patterns = 0;
	}
	free(s->position_list); s->position_list = 0;
	free(s->instruments); s->instruments = 0;
	free(s->wave_forms); s->wave_forms = 0;
	free(s->wave_form_lengths); s->wave_form_lengths = 0;
}

// [=]===^=[ aon_load ]===========================================================================[=]
static int32_t aon_load(struct artofnoise_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = 46;

	while(pos + 8 <= len) {
		uint8_t name[5];
		name[0] = data[pos + 0];
		name[1] = data[pos + 1];
		name[2] = data[pos + 2];
		name[3] = data[pos + 3];
		name[4] = 0;
		uint32_t chunk_size = aon_read_u32_be(&data[pos + 4]);
		pos += 8;

		if(chunk_size > len - pos) {
			return 0;
		}

		if(name[0]=='I' && name[1]=='N' && name[2]=='F' && name[3]=='O') {
			if(chunk_size < 3) {
				return 0;
			}
			s->number_of_positions = data[pos + 1];
			s->restart_position = data[pos + 2];
			if(s->restart_position >= s->number_of_positions) {
				s->restart_position = 0;
			}
		} else if(name[0]=='A' && name[1]=='R' && name[2]=='P' && name[3]=='G') {
			for(int32_t i = 0; i < AON_NUM_ARPEGGIOS; ++i) {
				if(pos + i * 4 + 4 > len) {
					return 0;
				}
				s->arpeggios[i][0] = data[pos + i * 4 + 0];
				s->arpeggios[i][1] = data[pos + i * 4 + 1];
				s->arpeggios[i][2] = data[pos + i * 4 + 2];
				s->arpeggios[i][3] = data[pos + i * 4 + 3];
			}
		} else if(name[0]=='P' && name[1]=='L' && name[2]=='S' && name[3]=='T') {
			s->position_list = (uint8_t *)malloc(chunk_size);
			if(!s->position_list) {
				return 0;
			}
			memcpy(s->position_list, &data[pos], chunk_size);
			s->position_list_len = chunk_size;
		} else if(name[0]=='P' && name[1]=='A' && name[2]=='T' && name[3]=='T') {
			uint32_t bytes_per_pattern = 4 * (uint32_t)s->num_channels * AON_PATTERN_ROWS;
			if(bytes_per_pattern == 0) {
				return 0;
			}
			uint32_t n = chunk_size / bytes_per_pattern;
			s->num_patterns = n;
			s->patterns = (struct aon_pattern *)calloc(n, sizeof(struct aon_pattern));
			if(!s->patterns && n > 0) {
				return 0;
			}
			uint32_t p = pos;
			for(uint32_t i = 0; i < n; ++i) {
				uint32_t cells = AON_PATTERN_ROWS * (uint32_t)s->num_channels;
				s->patterns[i].tracks = (struct aon_track_line *)calloc(cells, sizeof(struct aon_track_line));
				if(!s->patterns[i].tracks) {
					return 0;
				}
				for(uint32_t r = 0; r < AON_PATTERN_ROWS; ++r) {
					for(int32_t c = 0; c < s->num_channels; ++c) {
						uint8_t b1 = data[p++];
						uint8_t b2 = data[p++];
						uint8_t b3 = data[p++];
						uint8_t b4 = data[p++];
						struct aon_track_line *t = &s->patterns[i].tracks[r * (uint32_t)s->num_channels + (uint32_t)c];
						t->instrument = (uint8_t)(b2 & 0x3f);
						t->note = (uint8_t)(b1 & 0x3f);
						t->arpeggio = (uint8_t)(((b3 & 0xc0) >> 4) | ((b2 & 0xc0) >> 6));
						t->effect = (uint8_t)(b3 & 0x3f);
						t->effect_arg = b4;
					}
				}
			}
		} else if(name[0]=='I' && name[1]=='N' && name[2]=='S' && name[3]=='T') {
			uint32_t n = chunk_size / 32;
			s->num_instruments = n;
			s->instruments = (struct aon_instrument *)calloc(n, sizeof(struct aon_instrument));
			if(!s->instruments && n > 0) {
				return 0;
			}
			uint32_t p = pos;
			for(uint32_t i = 0; i < n; ++i) {
				struct aon_instrument *ins = &s->instruments[i];
				ins->type = data[p + 0];
				ins->volume = data[p + 1];
				ins->fine_tune = data[p + 2];
				ins->wave_form = data[p + 3];
				if(ins->type == AON_INSTR_SAMPLE) {
					ins->start_offset = aon_read_u32_be(&data[p + 4]);
					ins->length = aon_read_u32_be(&data[p + 8]);
					ins->loop_start = aon_read_u32_be(&data[p + 12]);
					ins->loop_length = aon_read_u32_be(&data[p + 16]);
				} else if(ins->type == AON_INSTR_SYNTH) {
					ins->synth_length = data[p + 4];
					ins->vib_param = data[p + 10];
					ins->vib_delay = data[p + 11];
					ins->vib_wave = data[p + 12];
					ins->wave_speed = data[p + 13];
					ins->wave_length = data[p + 14];
					ins->wave_loop_start = data[p + 15];
					ins->wave_loop_length = data[p + 16];
					ins->wave_loop_control = data[p + 17];
				} else {
					return 0;
				}
				ins->envelope_start = data[p + 28];
				ins->envelope_add = data[p + 29];
				ins->envelope_end = data[p + 30];
				ins->envelope_sub = data[p + 31];
				p += 32;
			}
		} else if(name[0]=='W' && name[1]=='L' && name[2]=='E' && name[3]=='N') {
			uint32_t n = chunk_size / 4;
			s->num_wave_forms = n;
			s->wave_form_lengths = (uint32_t *)calloc(n, sizeof(uint32_t));
			s->wave_forms = (int8_t **)calloc(n, sizeof(int8_t *));
			if((!s->wave_form_lengths || !s->wave_forms) && n > 0) {
				return 0;
			}
			for(uint32_t i = 0; i < n; ++i) {
				s->wave_form_lengths[i] = aon_read_u32_be(&data[pos + i * 4]);
			}
		} else if(name[0]=='W' && name[1]=='A' && name[2]=='V' && name[3]=='E') {
			if(!s->wave_forms || !s->wave_form_lengths) {
				return 0;
			}
			uint32_t p = pos;
			for(uint32_t i = 0; i < s->num_wave_forms; ++i) {
				uint32_t wlen = s->wave_form_lengths[i];
				if(wlen == 0) {
					s->wave_forms[i] = 0;
					continue;
				}
				if(p + wlen > len) {
					return 0;
				}
				s->wave_forms[i] = (int8_t *)&data[p];
				p += wlen;
			}
		}

		pos += chunk_size;
	}

	if(s->number_of_positions == 0 || !s->position_list || !s->patterns || !s->instruments || !s->wave_forms) {
		return 0;
	}

	return 1;
}

// [=]===^=[ aon_initialize_sound ]===============================================================[=]
static void aon_initialize_sound(struct artofnoise_state *s, int32_t start_position) {
	s->tempo = AON_DEFAULT_TEMPO;
	s->speed = AON_DEFAULT_SPEED;
	s->frame_cnt = 0;

	s->pattern_break = 0;
	s->pat_cnt = 0;
	s->pat_delay_cnt = 0;

	s->loop_flag = 0;
	s->loop_point = 0;
	s->loop_cnt = 0;

	s->position = (uint8_t)start_position;
	s->new_position = (uint8_t)start_position;
	s->current_pattern = s->position_list[start_position];

	s->noise_avoid = 0;
	s->oversize = 0;

	s->event[0] = 0;
	s->event[1] = 0;
	s->event[2] = 0;

	s->end_reached = 0;
	s->restart_song = 0;

	memset(s->voices, 0, sizeof(s->voices));
	for(int32_t i = 0; i < s->num_channels; ++i) {
		struct aon_voice *v = &s->voices[i];
		v->instrument = -1;
		v->fx_com = AON_FX_ARPEGGIO;
		v->track_volume = 64;
	}

	aon_set_bpm_tempo(s, AON_DEFAULT_TEMPO);
}

// [=]===^=[ aon_do_fx_vibrato ]==================================================================[=]
static void aon_do_fx_vibrato(struct aon_voice *v, uint8_t arg) {
	if(arg != 0) {
		uint8_t spd = (uint8_t)((arg & 0xf0) >> 4);
		if(spd != 0) {
			v->vibrato_spd = spd;
		}
		uint8_t ampl = (uint8_t)(arg & 0x0f);
		if(ampl != 0) {
			v->vibrato_ampl &= 0xf0;
			v->vibrato_ampl |= ampl;
		}
	}
}

// [=]===^=[ aon_start_repeat ]===================================================================[=]
static void aon_start_repeat(struct artofnoise_state *s, uint8_t track_note, struct aon_voice *v, struct aon_instrument *ins) {
	if(track_note != 0) {
		v->per_slide = 0;
	}

	int8_t *wave_form = s->wave_forms[ins->wave_form];
	uint32_t wave_buffer_len = (ins->wave_form < s->num_wave_forms) ? s->wave_form_lengths[ins->wave_form] : 0;

	v->old_wave_len = v->wave_len;
	v->wave_len = (uint16_t)ins->length;

	if(ins->loop_length != 0) {
		if(s->oversize || ins->loop_start != 0) {
			v->repeat_start = wave_form;
			v->repeat_offset = ins->loop_start * 2;
			v->repeat_length = (uint16_t)ins->loop_length;
			v->repeat_buffer_len = (int32_t)wave_buffer_len;
			if(!s->oversize) {
				v->wave_len = (uint16_t)(ins->loop_length + ins->loop_start);
			}
		} else {
			v->repeat_start = wave_form;
			v->repeat_offset = 0;
			v->repeat_length = (uint16_t)ins->loop_length;
			v->repeat_buffer_len = (int32_t)wave_buffer_len;
		}
	} else {
		v->repeat_start = 0;
		v->repeat_offset = 0;
		v->repeat_length = 0;
		v->repeat_buffer_len = 0;
	}

	uint32_t offset = ins->start_offset * 2;
	v->wave_len = (uint16_t)(v->wave_len - (uint16_t)(v->old_sample_offset / 2));

	if(v->fx_com == AON_FX_SET_OFFSET) {
		uint32_t new_offset = (uint32_t)v->fx_dat * 256u;
		int32_t new_wave_len = (int32_t)v->wave_len - (int32_t)(new_offset / 2);
		if(new_wave_len < 0) {
			v->wave_form = v->repeat_start;
			v->wave_form_offset = v->repeat_offset;
			v->wave_form_buffer_len = v->repeat_buffer_len;
			v->wave_len = (uint16_t)(v->repeat_offset + v->repeat_length);
		} else {
			v->wave_len = (uint16_t)new_wave_len;
			v->old_sample_offset += new_offset;
			v->wave_form = wave_form;
			v->wave_form_offset = offset + v->old_sample_offset;
			v->wave_form_buffer_len = (int32_t)wave_buffer_len;
		}
	} else {
		v->wave_form = wave_form;
		v->wave_form_offset = offset + v->old_sample_offset;
		v->wave_form_buffer_len = (int32_t)wave_buffer_len;
	}
}

// [=]===^=[ aon_start_sample ]===================================================================[=]
static void aon_start_sample(struct artofnoise_state *s, uint8_t track_note, struct aon_voice *v, struct aon_instrument *ins) {
	v->vib_on = 0x21;
	v->ch_mode = 0;

	if(v->fx_com == AON_FX_EXTRA) {
		uint8_t extra = (uint8_t)(v->fx_dat & 0xf0);
		uint8_t arg_low = (uint8_t)(v->fx_dat & 0x0f);
		if(extra == AON_EX_NOTE_DELAY) {
			v->step_fx_cnt = arg_low;
		} else {
			if(extra == AON_EX_RETRIG) {
				v->step_fx_cnt = arg_low;
			}
			v->ch_flag = 3;
		}
	} else {
		v->ch_flag = 3;
	}

	aon_start_repeat(s, track_note, v, ins);
}

// [=]===^=[ aon_init_adsr ]======================================================================[=]
static void aon_init_adsr(struct aon_voice *v, struct aon_instrument *ins) {
	if(v->fx_com != AON_FX_SYNTH_CONTROL || (v->fx_dat & 0x01) == 0) {
		v->synth_vol = ins->envelope_start;
		if(ins->envelope_add != 0) {
			v->synth_add = ins->envelope_add;
			v->synth_sub = ins->envelope_sub;
			v->synth_end = ins->envelope_end;
			v->synth_env = AON_ENV_ADD;
		} else {
			v->synth_vol = 127;
			v->synth_env = AON_ENV_DONE;
		}
	}
}

// [=]===^=[ aon_init_synth ]=====================================================================[=]
static void aon_init_synth(struct artofnoise_state *s, uint8_t track_note, struct aon_voice *v, struct aon_instrument *ins) {
	v->ch_mode = 1;

	uint8_t effect = v->fx_com;
	uint8_t extra = (uint8_t)(v->fx_dat & 0xf0);

	if(effect == AON_FX_EXTRA && extra == AON_EX_RETRIG) {
		v->step_fx_cnt = (uint8_t)(v->fx_dat & 0x0f);
	}

	if(track_note != 0) {
		v->per_slide = 0;

		uint8_t arg = (effect == AON_FX_SYNTH_CONTROL) ? v->fx_dat : (uint8_t)0;

		if((arg & 0x10) == 0) {
			if(effect == AON_FX_EXTRA && extra == AON_EX_NOTE_DELAY) {
				v->step_fx_cnt = (uint8_t)(v->fx_dat & 0x0f);
			} else {
				v->ch_flag = 3;
			}

			int8_t *wave_form = s->wave_forms[ins->wave_form];
			uint32_t wave_buffer_len = (ins->wave_form < s->num_wave_forms) ? s->wave_form_lengths[ins->wave_form] : 0;

			uint8_t reset = 1;
			if(v->wave_form == wave_form && v->wave_form_offset == 0) {
				v->ch_flag = 0;
				reset = (v->synth_wave_cont == 0);
			}

			if(reset) {
				v->wave_form = wave_form;
				v->wave_form_offset = 0;
				v->wave_form_buffer_len = (int32_t)wave_buffer_len;

				uint16_t wave_length = ins->synth_length;
				uint32_t offset = 0;

				if(effect == AON_FX_SET_OFFSET) {
					offset = (uint32_t)v->fx_dat * wave_length;
					if(v->synth_wave_stop != 0) {
						v->synth_wave_act = wave_form;
						v->synth_wave_act_offset = offset;
						v->repeat_start = wave_form;
						v->repeat_offset = offset;
						v->repeat_buffer_len = (int32_t)wave_buffer_len;
					}
				}

				if(v->synth_wave_stop == 0) {
					v->synth_wave_act = wave_form;
					v->synth_wave_act_offset = offset;
					v->old_wave_len = v->wave_len;
					v->wave_len = wave_length;
					v->repeat_length = wave_length;

					uint16_t wave_length2 = (uint16_t)(wave_length * 2);
					v->synth_wave_add_bytes = wave_length2;

					v->repeat_start = wave_form;
					v->repeat_offset = (uint32_t)ins->wave_loop_start * wave_length2 + offset;
					v->repeat_buffer_len = (int32_t)wave_buffer_len;

					v->synth_wave_end_offset = (uint32_t)ins->wave_length * wave_length2 + offset;

					v->synth_wave_rep = wave_form;
					v->synth_wave_rep_offset = (uint32_t)ins->wave_loop_start * wave_length2 + offset;

					v->synth_wave_rep_end_offset = (uint32_t)(ins->wave_loop_length + ins->wave_loop_start) * wave_length2 + offset;

					v->synth_wave_cnt = ins->wave_speed;
					v->synth_wave_spd = ins->wave_speed;
					v->synth_wave_rep_ctrl = ins->wave_loop_control;
				}
			}
		}

		v->vib_on = 0;

		if(ins->vib_wave != 3) {
			v->vibrato_trig_delay = (int16_t)(int8_t)ins->vib_delay;
			if(ins->vib_param != 0) {
				aon_do_fx_vibrato(v, ins->vib_param);
				v->vibrato_ampl &= 0x9f;
				v->vibrato_ampl |= (uint8_t)((uint8_t)(ins->vib_wave >> 3) | (uint8_t)(ins->vib_wave << 5));
				v->vib_cont = 1;
			} else {
				v->vibrato_trig_delay = -2;
			}
		} else {
			v->vib_on = 0x21;
		}
	}
}

// [=]===^=[ aon_use_old_instrument ]=============================================================[=]
static void aon_use_old_instrument(struct artofnoise_state *s, struct aon_track_line *tl, struct aon_voice *v, struct aon_instrument *ins) {
	v->vib_cont = 0;
	aon_init_adsr(v, ins);
	if(ins->type == AON_INSTR_SAMPLE) {
		aon_start_sample(s, tl->note, v, ins);
	} else {
		aon_init_synth(s, tl->note, v, ins);
	}
}

// [=]===^=[ aon_get_da_channel ]=================================================================[=]
static void aon_get_da_channel(struct artofnoise_state *s, struct aon_track_line *tl, struct aon_voice *v) {
	v->fx_com = tl->effect;
	v->fx_dat = tl->effect_arg;

	if(v->fx_com == AON_FX_PATTERN_BREAK) {
		if(s->pat_cnt == 63) {
			s->pat_cnt = 62;
		}
	}

	uint8_t arg_hi = (uint8_t)(v->fx_dat & 0xf0);
	uint8_t arg_low = (uint8_t)(v->fx_dat & 0x0f);

	if(v->fx_com == AON_FX_NEW_VOLUME) {
		v->step_fx_cnt = arg_low;
	} else if(v->fx_com == AON_FX_EXTRA) {
		uint8_t extra = arg_hi;
		if(extra == AON_EX_NOTE_CUT) {
			v->step_fx_cnt = arg_low;
		} else if(extra == AON_EX_PATTERN_DELAY) {
			if(s->pat_delay_cnt < 0) {
				s->pat_delay_cnt = (int8_t)arg_low;
			}
		} else if(extra == AON_EX_PATTERN_LOOP) {
			if(s->loop_cnt != 0xf0 && arg_low != 0) {
				if(s->loop_cnt == 0) {
					s->loop_cnt = arg_low;
				}
				s->loop_cnt--;
				if(s->loop_cnt == 0) {
					s->loop_cnt = 0xf0;
				}
				s->loop_flag = 1;
			}
		}
	}

	struct aon_instrument *ins;
	int32_t instr_num = (int32_t)tl->instrument - 1;

	if(instr_num < 0) {
		// Old instrument
		if(v->instrument < 0) {
			return;
		}
		ins = &s->instruments[v->instrument];
		if(tl->note != 0) {
			if(v->fx_com != AON_FX_TONE_PORTA && v->fx_com != AON_FX_TONE_VOL && v->fx_com != AON_FX_VOL_TONE && v->fx_com != AON_FX_FINEVOL_TONE) {
				aon_use_old_instrument(s, tl, v, ins);
			}
		}
	} else {
		if((uint32_t)instr_num >= s->num_instruments) {
			return;
		}
		if(tl->note == 0) {
			ins = &s->instruments[instr_num];
			if(ins->type == AON_INSTR_SAMPLE) {
				if(v->instrument != instr_num) {
					v->instrument = instr_num;
					v->instrument_number = (int16_t)instr_num;
					v->ch_flag = 1;
					aon_start_repeat(s, tl->note, v, ins);
				}
			}
			v->volume = ins->volume;
		} else {
			v->old_sample_offset = 0;
			ins = &s->instruments[instr_num];
			if(v->instrument != instr_num || (v->fx_com != AON_FX_TONE_PORTA && v->fx_com != AON_FX_TONE_VOL && v->fx_com != AON_FX_VOL_TONE && v->fx_com != AON_FX_FINEVOL_TONE)) {
				v->instrument = instr_num;
				v->instrument_number = (int16_t)instr_num;
				aon_use_old_instrument(s, tl, v, ins);
			}
			v->volume = ins->volume;
		}
	}

	uint8_t note = tl->note;
	if(note == 0) {
		note = v->last_note;
		if(note == 0 || note > 60) {
			return;
		}
	} else {
		v->slide_flag = 0;
		v->last_note = note;
		if(note > 60) {
			return;
		}
	}

	v->arpeggio_fine_tune = ins->fine_tune;
	note--;

	if(v->fx_com == AON_FX_VOL_TONE || v->fx_com == AON_FX_FINEVOL_TONE || v->fx_com == AON_FX_TONE_VOL || v->fx_com == AON_FX_TONE_PORTA) {
		if(tl->note != 0) {
			v->slide_flag = 1;
			uint16_t period = aon_periods[v->arpeggio_fine_tune & 0x0f][note];
			v->per_slide = (int16_t)((int32_t)v->period + (int32_t)v->per_slide - (int32_t)period);
		}
	}

	if(v->arpeggio_tab[1] == -1) {
		v->arpeggio_off = 0;
		v->arpeggio_cnt = 0;
	}

	if(v->fx_com == AON_FX_ARPEGGIO && v->fx_dat != 0) {
		uint8_t arp1 = (uint8_t)((v->fx_dat & 0xf0) >> 4);
		uint8_t arp2 = (uint8_t)(v->fx_dat & 0x0f);
		v->arpeggio_tab[0] = (int16_t)note;
		v->arpeggio_tab[1] = (int16_t)((int32_t)note + arp1);
		v->arpeggio_tab[2] = (int16_t)((int32_t)note + arp2);
		v->arpeggio_tab[3] = -1;
	} else {
		uint8_t *arp = s->arpeggios[tl->arpeggio];
		uint32_t read_offset = 0;
		uint32_t write_offset = 0;
		uint8_t arp_byte = arp[read_offset++];
		uint8_t count = (uint8_t)(arp_byte >> 4);
		if(count != 0) {
			v->arpeggio_tab[write_offset++] = (int16_t)((arp_byte & 0x0f) + note);
			count--;
			while(count > 0) {
				arp_byte = arp[read_offset];
				v->arpeggio_tab[write_offset++] = (int16_t)(((arp_byte & 0xf0) >> 4) + note);
				count--;
				if(count == 0) {
					break;
				}
				v->arpeggio_tab[write_offset++] = (int16_t)((arp_byte & 0x0f) + note);
				count--;
			}
		} else {
			v->arpeggio_off = 0;
			v->arpeggio_cnt = (uint8_t)(v->arpeggio_spd - 1);
			v->arpeggio_tab[write_offset++] = (int16_t)note;
		}
		v->arpeggio_tab[write_offset] = -1;
	}
}

// [=]===^=[ aon_play_new_step ]==================================================================[=]
static void aon_play_new_step(struct artofnoise_state *s) {
	uint8_t one_more_time;
	do {
		one_more_time = 0;

		if(!s->pattern_break) {
			if(s->pat_delay_cnt > 0) {
				s->pat_delay_cnt--;
				return;
			}
			s->pat_delay_cnt = -1;

			struct aon_pattern *pat = &s->patterns[s->current_pattern];
			for(int32_t i = 0; i < s->num_channels; ++i) {
				struct aon_track_line *tl = &pat->tracks[s->pat_cnt * (uint32_t)s->num_channels + (uint32_t)i];
				aon_get_da_channel(s, tl, &s->voices[i]);
			}

			if(s->loop_flag) {
				s->loop_flag = 0;
				s->pat_cnt = s->loop_point;
				return;
			}

			s->pat_cnt++;
			if(s->pat_cnt < AON_PATTERN_ROWS) {
				return;
			}
		} else {
			s->position = s->new_position;
		}

		s->pat_cnt = 0;
		s->pat_delay_cnt = 0;
		s->loop_point = 0;
		s->loop_cnt = 0;

		s->position++;
		if(s->position >= s->number_of_positions) {
			s->position = s->restart_position;
			s->end_reached = 1;
		}

		if(s->pattern_break) {
			s->pattern_break = 0;
			s->current_pattern = s->position_list[s->position];
			one_more_time = 1;
		}
	} while(one_more_time);
}

// [=]===^=[ aon_do_fx_portamento_up ]============================================================[=]
static void aon_do_fx_portamento_up(struct aon_voice *v, uint8_t arg) {
	v->per_slide = (int16_t)(v->per_slide - (int16_t)arg);
}

// [=]===^=[ aon_do_fx_portamento_down ]==========================================================[=]
static void aon_do_fx_portamento_down(struct aon_voice *v, uint8_t arg) {
	v->per_slide = (int16_t)(v->per_slide + (int16_t)arg);
}

// [=]===^=[ aon_do_fx_tone_slide ]===============================================================[=]
static void aon_do_fx_tone_slide(struct aon_voice *v) {
	if(v->slide_flag && v->per_slide != 0) {
		if(v->per_slide < 0) {
			v->per_slide = (int16_t)(v->per_slide + (int16_t)v->gliss_spd);
			if(v->per_slide >= 0) {
				v->per_slide = 0;
			}
		} else {
			v->per_slide = (int16_t)(v->per_slide - (int16_t)v->gliss_spd);
			if(v->per_slide < 0) {
				v->per_slide = 0;
			}
		}
	}
}

// [=]===^=[ aon_do_fx_vib_old_ampl ]=============================================================[=]
static void aon_do_fx_vib_old_ampl(struct aon_voice *v) {
	if(!v->vib_done) {
		v->vib_done = 1;
		uint8_t *table;
		uint8_t vib_ampl = (uint8_t)(v->vibrato_ampl & 0x60);
		if(vib_ampl == 0) {
			table = aon_vibrato_sine;
		} else if(vib_ampl == 32) {
			table = aon_vibrato_ramp_down;
		} else {
			table = aon_vibrato_square;
		}
		uint8_t vib_val = table[v->vibrato_pos & 0x1f];
		uint16_t add = (uint16_t)(((uint32_t)(v->vibrato_ampl & 0x0f) * (uint32_t)vib_val) >> 7);
		if((v->vibrato_ampl & 0x80) != 0) {
			v->period = (uint16_t)(v->period - add);
		} else {
			v->period = (uint16_t)(v->period + add);
		}
		v->vibrato_pos = (uint8_t)(v->vibrato_pos + v->vibrato_spd);
		if(v->vibrato_pos >= 32) {
			v->vibrato_pos &= 0x1f;
			v->vibrato_ampl ^= 0x80;
		}
	}
}

// [=]===^=[ aon_do_fx_volume_slide ]=============================================================[=]
static void aon_do_fx_volume_slide(struct aon_voice *v, uint8_t arg) {
	uint8_t arg_low = (uint8_t)(arg & 0x0f);
	uint8_t arg_hi = (uint8_t)((arg & 0xf0) >> 4);
	int32_t vol = (int32_t)(int8_t)v->volume;
	if(arg_hi == 0) {
		vol -= arg_low;
		if(vol < 0) {
			vol = 0;
		}
	} else {
		vol += arg_hi;
		if(vol > 64) {
			vol = 64;
		}
	}
	v->volume = (uint8_t)vol;
}

// [=]===^=[ aon_do_fx_break_to ]=================================================================[=]
static void aon_do_fx_break_to(struct artofnoise_state *s, uint8_t arg) {
	s->new_position = (uint8_t)(arg - 1);
	s->pat_cnt = 0;
	s->pattern_break = 1;
}

// [=]===^=[ aon_do_fx_set_volume ]===============================================================[=]
static void aon_do_fx_set_volume(struct aon_voice *v, uint8_t arg) {
	if(arg > 64) {
		arg = 64;
	}
	v->volume = arg;
}

// [=]===^=[ aon_do_fx_break_pat ]================================================================[=]
static void aon_do_fx_break_pat(struct artofnoise_state *s, uint8_t arg) {
	uint8_t arg_low = (uint8_t)(arg & 0x0f);
	uint8_t arg_hi = (uint8_t)(arg & 0xf0);
	uint8_t temp = (uint8_t)(arg_hi >> 1);
	uint8_t pos = (uint8_t)(temp + (temp >> 2));
	pos = (uint8_t)(pos + arg_low);
	if(pos >= 64) {
		pos = 0;
	}
	s->pat_cnt = pos;
	s->new_position = s->position;
	s->pattern_break = 1;
}

// [=]===^=[ aon_do_fx_set_speed ]================================================================[=]
static void aon_do_fx_set_speed(struct artofnoise_state *s, uint8_t arg) {
	if(arg != 0) {
		if(arg <= 32) {
			s->speed = arg;
		} else if(arg <= 200) {
			aon_set_bpm_tempo(s, arg);
		}
	} else {
		s->end_reached = 1;
		s->restart_song = 1;
	}
}

// [=]===^=[ aon_do_fx_set_vol_del ]==============================================================[=]
static void aon_do_fx_set_vol_del(struct aon_voice *v, uint8_t arg) {
	if(v->step_fx_cnt == 0) {
		v->volume = (uint8_t)(((arg & 0xf0) >> 4) * 4 + 4);
	} else {
		v->step_fx_cnt--;
	}
}

// [=]===^=[ aon_do_fx_set_wave_adsr_spd ]========================================================[=]
static void aon_do_fx_set_wave_adsr_spd(struct aon_voice *v, uint8_t arg) {
	v->synth_wave_spd = (uint8_t)((arg & 0xf0) >> 4);
}

// [=]===^=[ aon_do_fx_set_arp_spd ]==============================================================[=]
static void aon_do_fx_set_arp_spd(struct aon_voice *v, uint8_t arg) {
	uint8_t arg_low = (uint8_t)(arg & 0x0f);
	if(arg_low != 0) {
		v->arpeggio_spd = arg_low;
	}
}

// [=]===^=[ aon_do_fx_fine_vol_up ]==============================================================[=]
static void aon_do_fx_fine_vol_up(struct artofnoise_state *s, struct aon_voice *v, uint8_t arg) {
	if(s->frame_cnt == 0) {
		uint16_t vol = (uint16_t)(v->volume + arg);
		if(vol > 64) {
			vol = 64;
		}
		v->volume = (uint8_t)vol;
	}
}

// [=]===^=[ aon_do_fx_fine_vol_down ]============================================================[=]
static void aon_do_fx_fine_vol_down(struct artofnoise_state *s, struct aon_voice *v, uint8_t arg) {
	if(s->frame_cnt == 0) {
		int32_t new_vol = (int32_t)v->volume - (int32_t)arg;
		if(new_vol < 0) {
			new_vol = 0;
		}
		v->volume = (uint8_t)new_vol;
	}
}

// [=]===^=[ aon_do_fx_fine_vol_up_down ]=========================================================[=]
static void aon_do_fx_fine_vol_up_down(struct artofnoise_state *s, struct aon_voice *v, uint8_t arg) {
	uint8_t arg_hi = (uint8_t)((arg & 0xf0) >> 4);
	if(arg_hi == 0) {
		aon_do_fx_fine_vol_down(s, v, (uint8_t)(arg & 0x0f));
	} else {
		aon_do_fx_fine_vol_up(s, v, arg_hi);
	}
}

// [=]===^=[ aon_do_fx_vib_set_volume ]===========================================================[=]
static void aon_do_fx_vib_set_volume(struct aon_voice *v, uint8_t arg) {
	aon_do_fx_vib_old_ampl(v);
	aon_do_fx_set_volume(v, arg);
}

// [=]===^=[ aon_do_fx_port_vol_slide_up ]========================================================[=]
static void aon_do_fx_port_vol_slide_up(struct artofnoise_state *s, struct aon_voice *v, uint8_t arg) {
	uint8_t arg_low = (uint8_t)(arg & 0x0f);
	uint8_t arg_hi = (uint8_t)((arg & 0xf0) >> 4);
	int8_t nib_val = aon_nibble_tab[arg_hi];
	if(nib_val < 0) {
		aon_do_fx_fine_vol_down(s, v, (uint8_t)(-nib_val));
	} else {
		aon_do_fx_fine_vol_up(s, v, (uint8_t)nib_val);
	}
	if(s->frame_cnt != 0) {
		aon_do_fx_portamento_up(v, arg_low);
	}
}

// [=]===^=[ aon_do_fx_port_vol_slide_down ]======================================================[=]
static void aon_do_fx_port_vol_slide_down(struct artofnoise_state *s, struct aon_voice *v, uint8_t arg) {
	uint8_t arg_low = (uint8_t)(arg & 0x0f);
	uint8_t arg_hi = (uint8_t)((arg & 0xf0) >> 4);
	int8_t nib_val = aon_nibble_tab[arg_hi];
	if(nib_val < 0) {
		aon_do_fx_fine_vol_down(s, v, (uint8_t)(-nib_val));
	} else {
		aon_do_fx_fine_vol_up(s, v, (uint8_t)nib_val);
	}
	if(s->frame_cnt != 0) {
		aon_do_fx_portamento_down(v, arg_low);
	}
}

// [=]===^=[ aon_do_fx_synth_drums ]==============================================================[=]
static void aon_do_fx_synth_drums(struct aon_voice *v, uint8_t arg) {
	aon_do_fx_portamento_down(v, (uint8_t)((arg >> 4) * 8));
	aon_do_fx_volume_slide(v, (uint8_t)(arg & 0x0f));
}

// [=]===^=[ aon_do_fx_set_track_vol ]============================================================[=]
static void aon_do_fx_set_track_vol(struct aon_voice *v, uint8_t arg) {
	if(arg > 64) {
		arg = 64;
	}
	v->track_volume = arg;
}

// [=]===^=[ aon_do_fx_set_wave_cont ]============================================================[=]
static void aon_do_fx_set_wave_cont(struct aon_voice *v, uint8_t arg) {
	v->synth_wave_cont = (uint8_t)(arg & 0x0f);
	v->synth_wave_stop = (uint8_t)((arg & 0xf0) >> 4);
}

// [=]===^=[ aon_do_fx_external_event ]===========================================================[=]
static void aon_do_fx_external_event(struct artofnoise_state *s, uint8_t arg) {
	if(s->frame_cnt == 0) {
		s->event[0] = arg;
	}
}

// [=]===^=[ aon_do_fx_set_loop_point ]===========================================================[=]
static void aon_do_fx_set_loop_point(struct artofnoise_state *s) {
	uint8_t loop_point = (uint8_t)(s->pat_cnt - 1);
	if(loop_point != s->loop_point) {
		s->loop_point = loop_point;
		s->loop_cnt = 0;
	}
}

// [=]===^=[ aon_do_fx_jump2_loop ]===============================================================[=]
static void aon_do_fx_jump2_loop(struct artofnoise_state *s, uint8_t arg) {
	if(arg == 0) {
		aon_do_fx_set_loop_point(s);
	}
}

// [=]===^=[ aon_do_fx_retrig_note ]==============================================================[=]
static void aon_do_fx_retrig_note(struct aon_voice *v) {
	if(v->step_fx_cnt == 0) {
		v->ch_flag = 3;
		v->fx_com = 0xef;
	} else {
		v->step_fx_cnt--;
	}
}

// [=]===^=[ aon_do_fx_note_cut ]=================================================================[=]
static void aon_do_fx_note_cut(struct aon_voice *v) {
	if(v->step_fx_cnt == 0) {
		v->volume = 0;
	} else {
		v->step_fx_cnt--;
	}
}

// [=]===^=[ aon_do_fx_set_vibrato_wave ]=========================================================[=]
static void aon_do_fx_set_vibrato_wave(struct aon_voice *v, uint8_t arg) {
	arg &= 0x03;
	arg = (uint8_t)((uint8_t)(arg >> 3) | (uint8_t)(arg << 5));
	v->vibrato_ampl &= 0x9f;
	v->vibrato_ampl |= arg;
}

// [=]===^=[ aon_do_fx_e_commands ]===============================================================[=]
static void aon_do_fx_e_commands(struct artofnoise_state *s, struct aon_voice *v, uint8_t arg) {
	uint8_t arg_low = (uint8_t)(arg & 0x0f);
	uint8_t extra = (uint8_t)(arg & 0xf0);

	switch(extra) {
		case AON_EX_SET_FILTER: {
			break;
		}

		case AON_EX_FINE_SLIDE_UP: {
			if(s->frame_cnt == 0) {
				v->per_slide = (int16_t)(v->per_slide - (int16_t)arg_low);
			}
			break;
		}

		case AON_EX_FINE_SLIDE_DN: {
			if(s->frame_cnt == 0) {
				v->per_slide = (int16_t)(v->per_slide + (int16_t)arg_low);
			}
			break;
		}

		case AON_EX_VIB_WAVE: {
			aon_do_fx_set_vibrato_wave(v, arg_low);
			break;
		}

		case AON_EX_SET_LOOP: {
			aon_do_fx_set_loop_point(s);
			break;
		}

		case AON_EX_PATTERN_LOOP: {
			aon_do_fx_jump2_loop(s, arg_low);
			break;
		}

		case AON_EX_RETRIG: {
			aon_do_fx_retrig_note(v);
			break;
		}

		case AON_EX_FINE_VOL_UP: {
			aon_do_fx_fine_vol_up(s, v, arg_low);
			break;
		}

		case AON_EX_FINE_VOL_DN: {
			aon_do_fx_fine_vol_down(s, v, arg_low);
			break;
		}

		case AON_EX_NOTE_CUT: {
			aon_do_fx_note_cut(v);
			break;
		}

		case AON_EX_NOTE_DELAY: {
			aon_do_fx_retrig_note(v);
			break;
		}
	}
}

// [=]===^=[ aon_synth_reset_loop ]===============================================================[=]
static void aon_synth_reset_loop(struct aon_voice *v) {
	switch(v->synth_wave_rep_ctrl) {
		case 0: {
			v->synth_wave_act = v->synth_wave_rep;
			v->synth_wave_act_offset = v->synth_wave_rep_offset;
			v->synth_wave_end_offset = v->synth_wave_rep_end_offset;
			break;
		}

		case 1: {
			v->synth_wave_act = v->synth_wave_rep;
			v->synth_wave_act_offset = v->synth_wave_end_offset;
			int32_t off = v->synth_wave_add_bytes;
			if(off >= 0) {
				off = -off;
			}
			if(v->synth_wave_stop != 0) {
				return;
			}
			v->synth_wave_act_offset = (uint32_t)((int32_t)v->synth_wave_act_offset + off);
			v->synth_wave_add_bytes = off;
			break;
		}

		default: {
			v->synth_wave_act = v->synth_wave_rep;
			v->synth_wave_end_offset = v->synth_wave_rep_end_offset;
			v->synth_wave_act_offset = (uint32_t)((int32_t)v->synth_wave_act_offset - v->synth_wave_add_bytes);
			v->synth_wave_add_bytes = -v->synth_wave_add_bytes;
			break;
		}
	}
}

// [=]===^=[ aon_do_synth ]=======================================================================[=]
static void aon_do_synth(struct aon_voice *v) {
	v->vib_done = 0;

	if(v->ch_flag == 0) {
		if(v->ch_mode != 0 && v->wave_form != 0) {
			if(v->synth_wave_stop == 0) {
				v->synth_wave_cnt++;
				if(v->synth_wave_cnt >= v->synth_wave_spd) {
					v->synth_wave_cnt = 0;
					int32_t add_bytes = v->synth_wave_add_bytes;
					v->synth_wave_act_offset = (uint32_t)((int32_t)v->synth_wave_act_offset + add_bytes);
					if(add_bytes < 0) {
						if((int32_t)v->synth_wave_act_offset < (int32_t)v->synth_wave_rep_offset) {
							aon_synth_reset_loop(v);
						}
					} else {
						if((int32_t)v->synth_wave_act_offset >= (int32_t)v->synth_wave_end_offset) {
							aon_synth_reset_loop(v);
						}
					}
					v->ch_flag = 1;
					v->repeat_start = v->synth_wave_act;
					v->repeat_offset = v->synth_wave_act_offset;
				}
			}
		}

		if(v->wave_form != 0) {
			int32_t vol = v->synth_vol;
			switch(v->synth_env) {
				case AON_ENV_ADD: {
					vol += v->synth_add;
					if(vol > 127) {
						vol = 127;
						v->synth_env = AON_ENV_SUB;
					}
					break;
				}

				case AON_ENV_SUB: {
					vol -= v->synth_sub;
					if(vol <= v->synth_end) {
						vol = v->synth_end;
						v->synth_env = AON_ENV_DONE;
					}
					break;
				}
			}
			v->synth_vol = (uint8_t)vol;

			if(v->vib_on != 0x21) {
				if(v->vibrato_trig_delay == -1) {
					v->vib_on = 1;
				} else {
					v->vibrato_trig_delay--;
				}
			}
		}
	}

	if(v->vib_on == 1) {
		aon_do_fx_vib_old_ampl(v);
	}
}

// [=]===^=[ aon_do_fx ]==========================================================================[=]
static void aon_do_fx(struct artofnoise_state *s, struct aon_voice *v) {
	if(v->vib_cont == 0) {
		v->vib_on = 0x21;
	}

	v->arpeggio_cnt++;
	if(v->arpeggio_cnt >= v->arpeggio_spd) {
		v->arpeggio_cnt = 0;
	}

	int16_t period_offset = v->arpeggio_tab[v->arpeggio_off & 0x07];
	if(period_offset >= 0) {
		if(period_offset >= 60) {
			period_offset = 59;
		}
		v->period = aon_periods[v->arpeggio_fine_tune & 0x0f][period_offset];
		v->arpeggio_off++;
		v->arpeggio_off &= 0x07;
	} else {
		v->arpeggio_off = 0;
	}

	if(v->fx_com != AON_FX_ARPEGGIO) {
		uint8_t arg = v->fx_dat;

		if(s->frame_cnt != 0) {
			switch(v->fx_com) {
				case AON_FX_SLIDE_UP: {
					aon_do_fx_portamento_up(v, arg);
					break;
				}

				case AON_FX_SLIDE_DOWN: {
					aon_do_fx_portamento_down(v, arg);
					break;
				}

				case AON_FX_TONE_PORTA: {
					if(arg != 0) {
						v->gliss_spd = arg;
					}
					aon_do_fx_tone_slide(v);
					break;
				}

				case AON_FX_VIBRATO: {
					v->vib_on = 1;
					aon_do_fx_vibrato(v, arg);
					break;
				}

				case AON_FX_TONE_VOL: {
					aon_do_fx_tone_slide(v);
					aon_do_fx_volume_slide(v, arg);
					break;
				}

				case AON_FX_VIB_VOL: {
					aon_do_fx_vib_old_ampl(v);
					aon_do_fx_volume_slide(v, arg);
					break;
				}

				case AON_FX_VOL_SLIDE: {
					aon_do_fx_volume_slide(v, arg);
					break;
				}
			}
		}

		switch(v->fx_com) {
			case AON_FX_POS_JUMP: {
				aon_do_fx_break_to(s, arg);
				break;
			}

			case AON_FX_SET_VOLUME: {
				aon_do_fx_set_volume(v, arg);
				break;
			}

			case AON_FX_PATTERN_BREAK: {
				aon_do_fx_break_pat(s, arg);
				break;
			}

			case AON_FX_EXTRA: {
				aon_do_fx_e_commands(s, v, arg);
				break;
			}

			case AON_FX_SET_SPEED: {
				aon_do_fx_set_speed(s, arg);
				break;
			}

			case AON_FX_NEW_VOLUME: {
				aon_do_fx_set_vol_del(v, arg);
				break;
			}

			case AON_FX_WAVE_SPEED: {
				aon_do_fx_set_wave_adsr_spd(v, arg);
				break;
			}

			case AON_FX_SET_ARP_SPEED: {
				aon_do_fx_set_arp_spd(v, arg);
				break;
			}

			case AON_FX_VOL_VIB: {
				aon_do_fx_vib_set_volume(v, arg);
				break;
			}

			case AON_FX_FINESLIDE_PORTUP: {
				aon_do_fx_port_vol_slide_up(s, v, arg);
				break;
			}

			case AON_FX_FINESLIDE_PORTDN: {
				aon_do_fx_port_vol_slide_down(s, v, arg);
				break;
			}

			case AON_FX_AVOID_NOISE: {
				s->noise_avoid = (arg != 0) ? 1 : 0;
				break;
			}

			case AON_FX_OVERSIZE: {
				s->oversize = (arg != 0) ? 1 : 0;
				break;
			}

			case AON_FX_FINEVOL_VIB: {
				aon_do_fx_vib_old_ampl(v);
				aon_do_fx_fine_vol_up_down(s, v, arg);
				break;
			}

			case AON_FX_VOL_PORTDN: {
				aon_do_fx_synth_drums(v, arg);
				break;
			}

			case AON_FX_VOL_TONE: {
				aon_do_fx_tone_slide(v);
				aon_do_fx_set_volume(v, arg);
				break;
			}

			case AON_FX_FINEVOL_TONE: {
				aon_do_fx_tone_slide(v);
				aon_do_fx_fine_vol_up_down(s, v, arg);
				break;
			}

			case AON_FX_TRACK_VOLUME: {
				aon_do_fx_set_track_vol(v, arg);
				break;
			}

			case AON_FX_WAVE_MODE: {
				aon_do_fx_set_wave_cont(v, arg);
				break;
			}

			case AON_FX_EXTERNAL: {
				aon_do_fx_external_event(s, arg);
				break;
			}
		}
	}

	aon_do_synth(v);
}

// [=]===^=[ aon_play_fx ]========================================================================[=]
static void aon_play_fx(struct artofnoise_state *s) {
	uint8_t pattern = s->position_list[s->position];
	if(pattern != s->current_pattern) {
		s->current_pattern = pattern;
	}
	for(int32_t i = 0; i < s->num_channels; ++i) {
		aon_do_fx(s, &s->voices[i]);
	}
}

// [=]===^=[ aon_setup_channel ]==================================================================[=]
static void aon_setup_channel(struct artofnoise_state *s, struct aon_voice *v, int32_t ch) {
	if(v->fx_com == AON_FX_EXTRA && (uint8_t)(v->fx_dat & 0xf0) == AON_EX_NOTE_DELAY) {
		return;
	}

	if((v->ch_flag & 0x02) != 0 && v->wave_form != 0) {
		uint32_t length = (uint32_t)v->wave_len * 2;
		if(v->wave_form_buffer_len > 0) {
			uint32_t avail = (uint32_t)v->wave_form_buffer_len - v->wave_form_offset;
			if(length > avail) {
				length = avail;
			}
		}

		if(s->noise_avoid) {
			if(v->old_wave_len > 255 || v->old_wave_len == 0 || v->wave_len > 255) {
				paula_play_sample(&s->paula, ch, v->wave_form + v->wave_form_offset, length);
			} else {
				paula_queue_sample(&s->paula, ch, v->wave_form, v->wave_form_offset, length);
			}
		} else {
			paula_play_sample(&s->paula, ch, v->wave_form + v->wave_form_offset, length);
		}

		if(v->repeat_start != 0 && v->repeat_length > 1) {
			uint32_t rlen = (uint32_t)v->repeat_length * 2;
			if(v->repeat_buffer_len > 0) {
				uint32_t bufl = (uint32_t)v->repeat_buffer_len;
				if(v->repeat_offset + rlen > bufl) {
					if(v->repeat_offset < bufl) {
						rlen = bufl - v->repeat_offset;
					} else {
						rlen = 0;
					}
				}
			}
			if(rlen > 0) {
				paula_set_loop(&s->paula, ch, v->repeat_offset, rlen);
			}
		} else {
			paula_set_loop(&s->paula, ch, 0, 0);
		}
	} else {
		if(v->repeat_start != 0 && v->repeat_length > 1) {
			uint32_t rlen = (uint32_t)v->repeat_length * 2;
			if(v->repeat_buffer_len > 0) {
				uint32_t bufl = (uint32_t)v->repeat_buffer_len;
				if(v->repeat_offset + rlen > bufl) {
					if(v->repeat_offset < bufl) {
						rlen = bufl - v->repeat_offset;
					} else {
						rlen = 0;
					}
				}
			}
			if(rlen > 0) {
				paula_queue_sample(&s->paula, ch, v->repeat_start, v->repeat_offset, rlen);
				paula_set_loop(&s->paula, ch, v->repeat_offset, rlen);
			}
		}
	}

	int32_t period = (int32_t)v->period + (int32_t)v->per_slide;
	if(period < 103) {
		period = 103;
	}
	paula_set_period(&s->paula, ch, (uint16_t)period);

	uint16_t volume = (uint16_t)((((uint32_t)v->volume * (uint32_t)v->synth_vol) / 128) * (uint32_t)v->track_volume / 64);
	paula_set_volume(&s->paula, ch, volume);

	v->ch_flag = 0;
}

// [=]===^=[ aon_play ]===========================================================================[=]
static void aon_play(struct artofnoise_state *s) {
	s->frame_cnt++;
	if(s->speed != 0) {
		if(s->frame_cnt >= s->speed) {
			s->frame_cnt = 0;
			aon_play_new_step(s);
		}
	}

	aon_play_fx(s);

	uint8_t *pannings = (s->module_type == 8) ? aon_pan8 : aon_pan4;
	for(int32_t i = 0; i < s->num_channels; ++i) {
		aon_setup_channel(s, &s->voices[i], i);
		s->paula.ch[i].pan = pannings[i];
	}

	if(s->end_reached) {
		s->end_reached = 0;
		if(s->restart_song) {
			aon_initialize_sound(s, s->restart_position);
			s->restart_song = 0;
		}
	}
}

// [=]===^=[ artofnoise_init ]====================================================================[=]
static struct artofnoise_state *artofnoise_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 54 || sample_rate < 8000) {
		return 0;
	}

	int32_t mtype = aon_identify((uint8_t *)data, len);
	if(mtype == 0) {
		return 0;
	}

	struct artofnoise_state *s = (struct artofnoise_state *)calloc(1, sizeof(struct artofnoise_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;
	s->module_type = mtype;
	s->num_channels = mtype;

	if(s->num_channels > AON_MAX_CHANNELS) {
		s->num_channels = AON_MAX_CHANNELS;
	}

	if(!aon_load(s)) {
		aon_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, AON_TICK_HZ);
	aon_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ artofnoise_free ]====================================================================[=]
static void artofnoise_free(struct artofnoise_state *s) {
	if(!s) {
		return;
	}
	aon_cleanup(s);
	free(s);
}

// [=]===^=[ artofnoise_get_audio ]===============================================================[=]
static void artofnoise_get_audio(struct artofnoise_state *s, int16_t *output, int32_t frames) {
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
			aon_play(s);
		}
	}
}

// [=]===^=[ artofnoise_api_init ]================================================================[=]
static void *artofnoise_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return artofnoise_init(data, len, sample_rate);
}

// [=]===^=[ artofnoise_api_free ]================================================================[=]
static void artofnoise_api_free(void *state) {
	artofnoise_free((struct artofnoise_state *)state);
}

// [=]===^=[ artofnoise_api_get_audio ]===========================================================[=]
static void artofnoise_api_get_audio(void *state, int16_t *output, int32_t frames) {
	artofnoise_get_audio((struct artofnoise_state *)state, output, frames);
}

static const char *artofnoise_extensions[] = { "aon", "aon8", 0 };

static struct player_api artofnoise_api = {
	"Art Of Noise",
	artofnoise_extensions,
	artofnoise_api_init,
	artofnoise_api_free,
	artofnoise_api_get_audio,
	0,
};
