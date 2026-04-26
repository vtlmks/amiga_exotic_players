// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Digital Sound Studio replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). PAL 50Hz tick rate at BPM 125,
// dynamically scaled when tempo effects fire.
//
// Public API:
//   struct digitalsoundstudio_state *digitalsoundstudio_init(void *data, uint32_t len, int32_t sample_rate);
//   void digitalsoundstudio_free(struct digitalsoundstudio_state *s);
//   void digitalsoundstudio_get_audio(struct digitalsoundstudio_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define DSS_NUM_SAMPLES   31
#define DSS_NUM_CHANNELS  4
#define DSS_NUM_ROWS      64
#define DSS_PATTERN_BYTES 1024
#define DSS_HEADER_END    0x61e
#define DSS_POS_OFFSET    0x59c

enum {
	DSS_EFF_ARPEGGIO                    = 0x00,
	DSS_EFF_SLIDE_UP                    = 0x01,
	DSS_EFF_SLIDE_DOWN                  = 0x02,
	DSS_EFF_SET_VOLUME                  = 0x03,
	DSS_EFF_SET_MASTER_VOLUME           = 0x04,
	DSS_EFF_SET_SONG_SPEED              = 0x05,
	DSS_EFF_POSITION_JUMP               = 0x06,
	DSS_EFF_SET_FILTER                  = 0x07,
	DSS_EFF_PITCH_UP                    = 0x08,
	DSS_EFF_PITCH_DOWN                  = 0x09,
	DSS_EFF_PITCH_CONTROL               = 0x0a,
	DSS_EFF_SET_SONG_TEMPO              = 0x0b,
	DSS_EFF_VOLUME_UP                   = 0x0c,
	DSS_EFF_VOLUME_DOWN                 = 0x0d,
	DSS_EFF_VOLUME_SLIDE_UP             = 0x0e,
	DSS_EFF_VOLUME_SLIDE_DOWN           = 0x0f,
	DSS_EFF_MASTER_VOLUME_UP            = 0x10,
	DSS_EFF_MASTER_VOLUME_DOWN          = 0x11,
	DSS_EFF_MASTER_VOLUME_SLIDE_UP      = 0x12,
	DSS_EFF_MASTER_VOLUME_SLIDE_DOWN    = 0x13,
	DSS_EFF_SET_LOOP_START              = 0x14,
	DSS_EFF_JUMP_TO_LOOP                = 0x15,
	DSS_EFF_RETRIG_NOTE                 = 0x16,
	DSS_EFF_NOTE_DELAY                  = 0x17,
	DSS_EFF_NOTE_CUT                    = 0x18,
	DSS_EFF_SET_SAMPLE_OFFSET           = 0x19,
	DSS_EFF_SET_FINE_TUNE               = 0x1a,
	DSS_EFF_PORTAMENTO                  = 0x1b,
	DSS_EFF_PORTAMENTO_VOLUME_SLIDE_UP  = 0x1c,
	DSS_EFF_PORTAMENTO_VOLUME_SLIDE_DOWN= 0x1d,
	DSS_EFF_PORTAMENTO_CONTROL          = 0x1e,
};

struct dss_sample {
	uint32_t start_offset;
	uint16_t length;          // in words (oneshot)
	uint32_t loop_start;      // in bytes from sample start
	uint16_t loop_length;     // in words
	uint8_t  fine_tune;
	uint8_t  volume;
	uint16_t frequency;
	int8_t  *data;            // points into module buffer
};

struct dss_track_line {
	uint8_t  sample;
	uint16_t period;
	uint8_t  effect;
	uint8_t  effect_arg;
};

struct dss_pattern {
	struct dss_track_line tracks[DSS_NUM_CHANNELS][DSS_NUM_ROWS];
};

struct dss_voice {
	uint8_t  sample;
	uint16_t period;
	uint8_t  effect;
	uint8_t  effect_arg;

	uint8_t  fine_tune;
	uint8_t  volume;

	uint8_t  playing_sample_number;
	int8_t  *sample_data;
	uint32_t sample_start_offset;
	uint16_t sample_length;
	uint32_t loop_start;
	uint16_t loop_length;
	uint16_t sample_offset;

	uint16_t pitch_period;
	uint8_t  portamento_direction;
	uint8_t  portamento_speed;
	uint16_t portamento_end_period;

	uint8_t  use_tone_portamento_for_slide_effects;
	uint8_t  use_tone_portamento_for_portamento_effects;

	int16_t  loop_row;
	uint16_t loop_counter;
};

struct digitalsoundstudio_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint8_t start_song_tempo;
	uint8_t start_song_speed;

	struct dss_sample samples[DSS_NUM_SAMPLES];

	uint8_t *positions;
	uint16_t num_positions;

	struct dss_pattern *patterns;
	uint16_t num_patterns;

	uint8_t  current_song_tempo;
	uint8_t  current_song_speed;
	uint8_t  song_speed_counter;

	uint16_t current_position;
	int16_t  current_row;
	uint8_t  position_jump;
	uint16_t new_position;

	uint8_t  set_loop_row;
	int16_t  loop_row;

	uint16_t inverse_master_volume;
	uint16_t next_retrig_tick_number;
	uint16_t arpeggio_counter;

	struct dss_voice voices[DSS_NUM_CHANNELS];
};

// [=]===^=[ dss_periods ]========================================================================[=]
// 16 fine-tune rows of 48 periods. Index 0 = normal, 1..7 = positive tunings,
// 8..15 = negative tunings (matching the C# layout: tuning -8 .. tuning -1).
static uint16_t dss_periods[16][48] = {
	{ 1712,1616,1524,1440,1356,1280,1208,1140,1076,1016, 960, 906,
	   856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
	   428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
	   214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113 },
	{ 1700,1604,1514,1430,1348,1274,1202,1134,1070,1010, 954, 900,
	   850, 802, 757, 715, 674, 637, 601, 567, 535, 505, 477, 450,
	   425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 239, 225,
	   213, 201, 189, 179, 169, 159, 150, 142, 134, 126, 119, 113 },
	{ 1688,1592,1504,1418,1340,1264,1194,1126,1064,1004, 948, 894,
	   844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447,
	   422, 398, 376, 355, 335, 316, 298, 282, 266, 251, 237, 224,
	   211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118, 112 },
	{ 1676,1582,1492,1408,1330,1256,1184,1118,1056, 996, 940, 888,
	   838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470, 444,
	   419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235, 222,
	   209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118, 111 },
	{ 1664,1570,1482,1398,1320,1246,1176,1110,1048, 990, 934, 882,
	   832, 785, 741, 699, 660, 623, 588, 555, 524, 495, 467, 441,
	   416, 392, 370, 350, 330, 312, 294, 278, 262, 247, 233, 220,
	   208, 196, 185, 175, 165, 156, 147, 139, 131, 124, 117, 110 },
	{ 1652,1558,1472,1388,1310,1238,1168,1102,1040, 982, 926, 874,
	   826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437,
	   413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232, 219,
	   206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116, 109 },
	{ 1640,1548,1460,1378,1302,1228,1160,1094,1032, 974, 920, 868,
	   820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434,
	   410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230, 217,
	   205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115, 109 },
	{ 1628,1536,1450,1368,1292,1220,1150,1086,1026, 968, 914, 862,
	   814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431,
	   407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228, 216,
	   204, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114, 108 },
	{ 1814,1712,1616,1524,1440,1356,1280,1208,1140,1076,1016, 960,
	   907, 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480,
	   453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240,
	   226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120 },
	{ 1800,1700,1604,1514,1430,1350,1272,1202,1134,1070,1010, 954,
	   900, 850, 802, 757, 715, 675, 636, 601, 567, 535, 505, 477,
	   450, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 238,
	   225, 212, 200, 189, 179, 169, 159, 150, 142, 134, 126, 119 },
	{ 1788,1688,1592,1504,1418,1340,1264,1194,1126,1064,1004, 948,
	   894, 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474,
	   447, 422, 398, 376, 355, 335, 316, 298, 282, 266, 251, 237,
	   223, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118 },
	{ 1774,1676,1582,1492,1408,1330,1256,1184,1118,1056, 996, 940,
	   887, 838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470,
	   444, 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235,
	   222, 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118 },
	{ 1762,1664,1570,1482,1398,1320,1246,1176,1110,1048, 988, 934,
	   881, 832, 785, 741, 699, 660, 623, 588, 555, 524, 494, 467,
	   441, 416, 392, 370, 350, 330, 312, 294, 278, 262, 247, 233,
	   220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 123, 117 },
	{ 1750,1652,1558,1472,1388,1310,1238,1168,1102,1040, 982, 926,
	   875, 826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463,
	   437, 413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232,
	   219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116 },
	{ 1736,1640,1548,1460,1378,1302,1228,1160,1094,1032, 974, 920,
	   868, 820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460,
	   434, 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230,
	   217, 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115 },
	{ 1724,1628,1536,1450,1368,1292,1220,1150,1086,1026, 968, 914,
	   862, 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457,
	   431, 407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228,
	   216, 203, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114 },
};

// [=]===^=[ dss_period_limits ]==================================================================[=]
static uint16_t dss_period_limits[16][2] = {
	{ 1712, 113 },
	{ 1700, 113 },
	{ 1688, 112 },
	{ 1676, 111 },
	{ 1664, 110 },
	{ 1652, 109 },
	{ 1640, 109 },
	{ 1628, 108 },
	{ 1814, 120 },
	{ 1800, 119 },
	{ 1788, 118 },
	{ 1774, 118 },
	{ 1762, 117 },
	{ 1750, 116 },
	{ 1736, 115 },
	{ 1724, 114 },
};

// [=]===^=[ dss_read_u16_be ]====================================================================[=]
static uint16_t dss_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ dss_read_u32_be ]====================================================================[=]
static uint32_t dss_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ dss_test_module ]====================================================================[=]
static int32_t dss_test_module(uint8_t *buf, uint32_t len) {
	uint32_t i;
	uint16_t num_positions;
	uint8_t highest;

	if(len < DSS_HEADER_END) {
		return 0;
	}
	if((buf[0] != 'M') || (buf[1] != 'M') || (buf[2] != 'U') || (buf[3] != '2')) {
		return 0;
	}

	num_positions = dss_read_u16_be(buf + DSS_POS_OFFSET);
	if(num_positions > 128) {
		return 0;
	}

	highest = 0;
	for(i = 0; i < num_positions; ++i) {
		uint8_t v = buf[DSS_POS_OFFSET + 2 + i];
		if(v > highest) {
			highest = v;
		}
	}

	if((((uint32_t)(highest + 1) * DSS_PATTERN_BYTES) + DSS_HEADER_END) < len) {
		return 1;
	}
	return 0;
}

// [=]===^=[ dss_set_bpm_tempo ]==================================================================[=]
// Standard ProTracker BPM->ticks: ticks_per_sec = bpm * 0.4, so a BPM of 125
// yields the canonical 50Hz tick.
static void dss_set_bpm_tempo(struct digitalsoundstudio_state *s, uint8_t bpm) {
	if(bpm < 28) {
		bpm = 28;
	}
	int32_t spt = (s->paula.sample_rate * 5) / ((int32_t)bpm * 2);
	if(spt < 1) {
		spt = 1;
	}
	s->paula.samples_per_tick = spt;
	if(s->paula.tick_offset >= spt) {
		s->paula.tick_offset = 0;
	}
}

// [=]===^=[ dss_load_sample_info ]===============================================================[=]
static int32_t dss_load_sample_info(struct digitalsoundstudio_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = 10;

	for(uint32_t i = 0; i < DSS_NUM_SAMPLES; ++i) {
		if(pos + 46 > len) {
			return -1;
		}
		struct dss_sample *smp = &s->samples[i];
		// Skip 30-byte name field; we don't surface it.
		smp->start_offset = dss_read_u32_be(data + pos + 30) & 0xfffffffeu;
		smp->length       = dss_read_u16_be(data + pos + 34);
		smp->loop_start   = dss_read_u32_be(data + pos + 36);
		smp->loop_length  = dss_read_u16_be(data + pos + 40);
		smp->fine_tune    = data[pos + 42];
		smp->volume       = data[pos + 43];
		smp->frequency    = dss_read_u16_be(data + pos + 44);
		smp->data         = 0;
		pos += 46;
	}

	return (int32_t)pos;
}

// [=]===^=[ dss_load_positions ]=================================================================[=]
static int32_t dss_load_positions(struct digitalsoundstudio_state *s, int32_t pos) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	if((uint32_t)(pos + 2) > len) {
		return -1;
	}
	uint16_t num = dss_read_u16_be(data + pos);
	pos += 2;
	if((uint32_t)pos + num > len || num > 128) {
		return -1;
	}

	s->positions = (uint8_t *)malloc(num);
	if(!s->positions) {
		return -1;
	}
	memcpy(s->positions, data + pos, num);
	s->num_positions = num;

	pos += 128;
	return pos;
}

// [=]===^=[ dss_load_patterns ]==================================================================[=]
static int32_t dss_load_patterns(struct digitalsoundstudio_state *s, int32_t pos) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	uint8_t highest = 0;
	for(uint32_t i = 0; i < s->num_positions; ++i) {
		if(s->positions[i] > highest) {
			highest = s->positions[i];
		}
	}
	uint16_t num_patterns = (uint16_t)(highest + 1);

	s->patterns = (struct dss_pattern *)calloc(num_patterns, sizeof(struct dss_pattern));
	if(!s->patterns) {
		return -1;
	}
	s->num_patterns = num_patterns;

	for(uint16_t i = 0; i < num_patterns; ++i) {
		struct dss_pattern *pat = &s->patterns[i];
		for(uint32_t j = 0; j < DSS_NUM_ROWS; ++j) {
			for(uint32_t k = 0; k < DSS_NUM_CHANNELS; ++k) {
				if((uint32_t)pos + 4 > len) {
					return -1;
				}
				uint8_t b1 = data[pos + 0];
				uint8_t b2 = data[pos + 1];
				uint8_t b3 = data[pos + 2];
				uint8_t b4 = data[pos + 3];
				struct dss_track_line *tl = &pat->tracks[k][j];
				tl->sample     = (uint8_t)(b1 >> 3);
				tl->period     = (uint16_t)(((b1 & 0x07) << 8) | b2);
				tl->effect     = b3;
				tl->effect_arg = b4;
				pos += 4;
			}
		}
	}

	return pos;
}

// [=]===^=[ dss_load_sample_data ]===============================================================[=]
static int32_t dss_load_sample_data(struct digitalsoundstudio_state *s, int32_t pos) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	for(uint32_t i = 0; i < DSS_NUM_SAMPLES; ++i) {
		struct dss_sample *smp = &s->samples[i];
		uint32_t total = smp->length;
		if(total == 0) {
			continue;
		}
		if(smp->loop_length > 1) {
			total += smp->loop_length;
		}
		total *= 2;
		total += smp->start_offset;

		if((uint32_t)pos + total > len) {
			return -1;
		}
		smp->data = (int8_t *)(data + pos);
		pos += (int32_t)total;
	}
	return pos;
}

// [=]===^=[ dss_initialize_sound ]===============================================================[=]
static void dss_initialize_sound(struct digitalsoundstudio_state *s, int32_t start_position) {
	s->current_song_tempo = (s->start_song_tempo == 0) ? 125 : s->start_song_tempo;
	s->current_song_speed = s->start_song_speed;
	s->song_speed_counter = 0;

	s->current_position = (uint16_t)start_position;
	s->current_row = 0;
	s->position_jump = 0;
	s->new_position = 0;

	s->set_loop_row = 0;
	s->loop_row = 0;

	s->inverse_master_volume = 0;
	s->next_retrig_tick_number = 0;
	s->arpeggio_counter = 3;

	memset(s->voices, 0, sizeof(s->voices));

	dss_set_bpm_tempo(s, s->current_song_tempo);
}

// [=]===^=[ dss_set_volume ]=====================================================================[=]
static void dss_set_volume(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	int32_t new_vol = (int32_t)v->volume - (int32_t)s->inverse_master_volume;
	if(new_vol < 0) {
		new_vol = 0;
	}
	paula_set_volume(&s->paula, channel, (uint16_t)new_vol);
}

// [=]===^=[ dss_set_volume_on_all ]==============================================================[=]
static void dss_set_volume_on_all(struct digitalsoundstudio_state *s) {
	for(int32_t i = 0; i < DSS_NUM_CHANNELS; ++i) {
		dss_set_volume(s, &s->voices[i], i);
	}
}

// [=]===^=[ dss_add_volume ]=====================================================================[=]
static void dss_add_volume(struct digitalsoundstudio_state *s, uint8_t add, struct dss_voice *v, int32_t channel) {
	int32_t nv = (int32_t)v->volume + (int32_t)add;
	if(nv > 64) {
		nv = 64;
	}
	v->volume = (uint8_t)nv;
	dss_set_volume(s, v, channel);
}

// [=]===^=[ dss_sub_volume ]=====================================================================[=]
static void dss_sub_volume(struct digitalsoundstudio_state *s, uint8_t sub, struct dss_voice *v, int32_t channel) {
	int32_t nv = (int32_t)v->volume - (int32_t)sub;
	if(nv < 0) {
		nv = 0;
	}
	v->volume = (uint8_t)nv;
	dss_set_volume(s, v, channel);
}

// [=]===^=[ dss_add_master_volume ]==============================================================[=]
static void dss_add_master_volume(struct digitalsoundstudio_state *s, uint8_t add) {
	int32_t nv = (int32_t)s->inverse_master_volume - (int32_t)add;
	if(nv < 0) {
		nv = 0;
	}
	s->inverse_master_volume = (uint16_t)nv;
	dss_set_volume_on_all(s);
}

// [=]===^=[ dss_sub_master_volume ]==============================================================[=]
static void dss_sub_master_volume(struct digitalsoundstudio_state *s, uint8_t sub) {
	int32_t nv = (int32_t)s->inverse_master_volume + (int32_t)sub;
	if(nv > 64) {
		nv = 64;
	}
	s->inverse_master_volume = (uint16_t)nv;
	dss_set_volume_on_all(s);
}

// [=]===^=[ dss_adjust_fine_tune ]===============================================================[=]
static uint16_t dss_adjust_fine_tune(uint16_t period, struct dss_voice *v) {
	if(v->fine_tune != 0) {
		int32_t i;
		for(i = 0; i < 48; ++i) {
			if(dss_periods[0][i] == period) {
				++i;
				break;
			}
		}
		if(i == 0) {
			return period;
		}
		period = dss_periods[v->fine_tune][i - 1];
	}
	return period;
}

// [=]===^=[ dss_adjust_for_tone_portamento ]=====================================================[=]
static uint16_t dss_adjust_for_tone_portamento(uint16_t period, struct dss_voice *v) {
	int32_t i;
	for(i = 0; i < 48; ++i) {
		if(period >= dss_periods[v->fine_tune][i]) {
			++i;
			break;
		}
	}
	if(i == 0) {
		i = 1;
	}
	return dss_periods[v->fine_tune][i - 1];
}

// [=]===^=[ dss_apply_pitch_period ]=============================================================[=]
static void dss_apply_pitch_period(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	uint16_t period = v->pitch_period;
	if(v->use_tone_portamento_for_slide_effects) {
		period = dss_adjust_for_tone_portamento(period, v);
	}
	paula_set_period(&s->paula, channel, period);
}

// [=]===^=[ dss_setup_portamento ]===============================================================[=]
static void dss_setup_portamento(uint16_t period, struct dss_voice *v) {
	v->portamento_end_period = dss_adjust_fine_tune(period, v);
	v->portamento_direction = 0;
	if(v->portamento_end_period == v->pitch_period) {
		v->portamento_end_period = 0;
	} else if(v->portamento_end_period < v->pitch_period) {
		v->portamento_direction = 1;
	}
}

// [=]===^=[ dss_play_sample ]====================================================================[=]
// Triggers the queued (deferred) sample swap on Paula and pairs the queue with
// a matching loop region as required by the contract.
static void dss_play_sample(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	if((v->sample_data != 0) && (v->sample_length > 0)) {
		uint32_t total_words = (uint32_t)v->sample_length + (uint32_t)v->loop_length;
		paula_queue_sample(&s->paula, channel, v->sample_data, v->sample_start_offset, total_words * 2u);
		if(v->loop_length != 0) {
			paula_set_loop(&s->paula, channel, v->loop_start, (uint32_t)v->loop_length * 2u);
		} else {
			paula_set_loop(&s->paula, channel, v->sample_start_offset, total_words * 2u);
		}
	}
}

// [=]===^=[ dss_do_portamento ]==================================================================[=]
static void dss_do_portamento(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	if(v->portamento_end_period != 0) {
		if(v->portamento_direction) {
			v->pitch_period = (uint16_t)(v->pitch_period - v->portamento_speed);
			if(v->pitch_period <= v->portamento_end_period) {
				v->pitch_period = v->portamento_end_period;
				v->portamento_end_period = 0;
			}
		} else {
			v->pitch_period = (uint16_t)(v->pitch_period + v->portamento_speed);
			if(v->pitch_period >= v->portamento_end_period) {
				v->pitch_period = v->portamento_end_period;
				v->portamento_end_period = 0;
			}
		}
		uint16_t period = v->pitch_period;
		if(v->use_tone_portamento_for_portamento_effects) {
			period = dss_adjust_for_tone_portamento(period, v);
		}
		paula_set_period(&s->paula, channel, period);
	}
}

// [=]===^=[ dss_eff_arpeggio ]===================================================================[=]
// NOTE(peter): faithful port of the C# code, which computes arpeggio_offset
// but never indexes by it. The lookup loop scans the period table for a match
// against pitch_period and emits whatever it finds first.
static void dss_eff_arpeggio(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	int32_t arp = (int32_t)s->arpeggio_counter - (int32_t)s->song_speed_counter;

	if(arp == 0) {
		s->arpeggio_counter += 3;
		paula_set_period(&s->paula, channel, v->pitch_period);
		return;
	}

	for(int32_t i = 0; ; ++i) {
		uint16_t period = dss_periods[v->fine_tune][i];
		if((period == dss_period_limits[v->fine_tune][1]) || (period == v->pitch_period)) {
			paula_set_period(&s->paula, channel, period);
			break;
		}
		if(i >= 47) {
			paula_set_period(&s->paula, channel, dss_periods[v->fine_tune][47]);
			break;
		}
	}
}

// [=]===^=[ dss_eff_slide_up ]===================================================================[=]
static void dss_eff_slide_up(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	int32_t np = (int32_t)v->pitch_period - (int32_t)v->effect_arg;
	uint16_t lim = dss_period_limits[v->fine_tune][1];
	if(np < (int32_t)lim) {
		np = lim;
	}
	v->pitch_period = (uint16_t)np;
	dss_apply_pitch_period(s, v, channel);
}

// [=]===^=[ dss_eff_slide_down ]=================================================================[=]
static void dss_eff_slide_down(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	int32_t np = (int32_t)v->pitch_period + (int32_t)v->effect_arg;
	uint16_t lim = dss_period_limits[v->fine_tune][0];
	if(np > (int32_t)lim) {
		np = lim;
	}
	v->pitch_period = (uint16_t)np;
	dss_apply_pitch_period(s, v, channel);
}

// [=]===^=[ dss_eff_set_volume ]=================================================================[=]
static void dss_eff_set_volume(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	v->volume = v->effect_arg;
	dss_set_volume(s, v, channel);
}

// [=]===^=[ dss_eff_set_master_volume ]==========================================================[=]
static void dss_eff_set_master_volume(struct digitalsoundstudio_state *s, struct dss_voice *v) {
	int32_t nv = 64 - (int32_t)v->effect_arg;
	if(nv >= 0) {
		s->inverse_master_volume = (uint16_t)nv;
	}
}

// [=]===^=[ dss_eff_set_song_speed ]=============================================================[=]
static void dss_eff_set_song_speed(struct digitalsoundstudio_state *s, struct dss_voice *v) {
	if(v->effect_arg != 0) {
		s->song_speed_counter = 0;
		s->next_retrig_tick_number = 0;
		s->arpeggio_counter = 3;
		s->current_song_speed = v->effect_arg;
	}
}

// [=]===^=[ dss_eff_position_jump ]==============================================================[=]
static void dss_eff_position_jump(struct digitalsoundstudio_state *s, struct dss_voice *v) {
	if((v->effect_arg == 0xff) || (v->effect_arg == 0)) {
		s->new_position = s->current_position;
		s->position_jump = 1;
	} else if(v->effect_arg <= s->num_positions) {
		s->new_position = (uint16_t)((int32_t)v->effect_arg - 2);
		s->position_jump = 1;
	}
}

// [=]===^=[ dss_eff_set_filter ]=================================================================[=]
// Mirrors C# DigitalSoundStudioWorker.DoEffectSetFilter: EffectArg!=0 -> on,
// EffectArg==0 -> off. paula's LED filter is a 1-pole LP applied post-mix.
static void dss_eff_set_filter(struct digitalsoundstudio_state *s, struct dss_voice *v) {
	paula_set_lp_filter(&s->paula, v->effect_arg != 0 ? 1 : 0);
}

// [=]===^=[ dss_eff_pitch_up ]===================================================================[=]
static void dss_eff_pitch_up(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	if(v->effect_arg != 0) {
		uint8_t old = v->use_tone_portamento_for_slide_effects;
		v->use_tone_portamento_for_slide_effects = 0;
		dss_eff_slide_up(s, v, channel);
		v->use_tone_portamento_for_slide_effects = old;
	}
}

// [=]===^=[ dss_eff_pitch_down ]=================================================================[=]
static void dss_eff_pitch_down(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	if(v->effect_arg != 0) {
		uint8_t old = v->use_tone_portamento_for_slide_effects;
		v->use_tone_portamento_for_slide_effects = 0;
		dss_eff_slide_down(s, v, channel);
		v->use_tone_portamento_for_slide_effects = old;
	}
}

// [=]===^=[ dss_eff_pitch_control ]==============================================================[=]
static void dss_eff_pitch_control(struct dss_voice *v) {
	v->use_tone_portamento_for_slide_effects = (v->effect_arg != 0) ? 1 : 0;
}

// [=]===^=[ dss_eff_set_song_tempo ]=============================================================[=]
static void dss_eff_set_song_tempo(struct digitalsoundstudio_state *s, struct dss_voice *v) {
	if(v->effect_arg >= 28) {
		s->current_song_tempo = v->effect_arg;
		dss_set_bpm_tempo(s, s->current_song_tempo);
		s->song_speed_counter = 0;
		s->next_retrig_tick_number = 0;
		s->arpeggio_counter = 3;
	}
}

// [=]===^=[ dss_eff_volume_up ]==================================================================[=]
static void dss_eff_volume_up(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	if(v->effect_arg != 0) {
		dss_add_volume(s, v->effect_arg, v, channel);
	}
}

// [=]===^=[ dss_eff_volume_down ]================================================================[=]
static void dss_eff_volume_down(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	if(v->effect_arg != 0) {
		dss_sub_volume(s, v->effect_arg, v, channel);
	}
}

// [=]===^=[ dss_eff_volume_slide_up ]============================================================[=]
static void dss_eff_volume_slide_up(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	dss_add_volume(s, v->effect_arg, v, channel);
}

// [=]===^=[ dss_eff_volume_slide_down ]==========================================================[=]
static void dss_eff_volume_slide_down(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	dss_sub_volume(s, v->effect_arg, v, channel);
}

// [=]===^=[ dss_eff_master_volume_up ]===========================================================[=]
static void dss_eff_master_volume_up(struct digitalsoundstudio_state *s, struct dss_voice *v) {
	if(v->effect_arg != 0) {
		dss_add_master_volume(s, v->effect_arg);
	}
}

// [=]===^=[ dss_eff_master_volume_down ]=========================================================[=]
static void dss_eff_master_volume_down(struct digitalsoundstudio_state *s, struct dss_voice *v) {
	if(v->effect_arg != 0) {
		dss_sub_master_volume(s, v->effect_arg);
	}
}

// [=]===^=[ dss_eff_master_volume_slide_up ]=====================================================[=]
static void dss_eff_master_volume_slide_up(struct digitalsoundstudio_state *s, struct dss_voice *v) {
	dss_add_master_volume(s, v->effect_arg);
}

// [=]===^=[ dss_eff_master_volume_slide_down ]===================================================[=]
static void dss_eff_master_volume_slide_down(struct digitalsoundstudio_state *s, struct dss_voice *v) {
	dss_sub_master_volume(s, v->effect_arg);
}

// [=]===^=[ dss_eff_set_loop_start ]=============================================================[=]
static void dss_eff_set_loop_start(struct digitalsoundstudio_state *s, struct dss_voice *v) {
	if(v->effect_arg != 0) {
		v->loop_row = s->current_row;
	}
}

// [=]===^=[ dss_eff_jump_to_loop ]===============================================================[=]
static void dss_eff_jump_to_loop(struct digitalsoundstudio_state *s, struct dss_voice *v) {
	if(v->effect_arg == 0) {
		v->loop_row = -1;
	} else if(v->loop_row >= 0) {
		if(v->loop_counter == 0) {
			v->loop_counter = v->effect_arg;
		} else {
			v->loop_counter--;
			if(v->loop_counter == 0) {
				v->loop_row = -1;
				return;
			}
		}
		s->loop_row = (int16_t)(v->loop_row - 1);
		s->set_loop_row = 1;
		v->loop_row = -1;
	}
}

// [=]===^=[ dss_eff_retrig_note ]================================================================[=]
static void dss_eff_retrig_note(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	if(v->effect_arg < s->current_song_speed) {
		if(s->next_retrig_tick_number == 0) {
			s->next_retrig_tick_number = v->effect_arg;
		}
		if(s->song_speed_counter == s->next_retrig_tick_number) {
			s->next_retrig_tick_number += v->effect_arg;
			dss_play_sample(s, v, channel);
		}
	}
}

// [=]===^=[ dss_eff_note_delay ]=================================================================[=]
static void dss_eff_note_delay(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	if(v->effect_arg < s->current_song_speed) {
		if(v->effect_arg == s->song_speed_counter) {
			if(v->sample != 0) {
				if(v->period != 0) {
					uint16_t period = dss_adjust_fine_tune(v->period, v);
					v->pitch_period = period;
					paula_set_period(&s->paula, channel, period);
				}
				dss_play_sample(s, v, channel);
			}
		}
	} else {
		v->effect = 0;
		v->effect_arg = 0;
	}
}

// [=]===^=[ dss_eff_note_cut ]===================================================================[=]
static void dss_eff_note_cut(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	if(v->effect_arg < s->current_song_speed) {
		if(v->effect_arg == s->song_speed_counter) {
			v->volume = 0;
			paula_mute(&s->paula, channel);
		}
	} else {
		v->effect = 0;
		v->effect_arg = 0;
	}
}

// [=]===^=[ dss_eff_set_sample_offset ]==========================================================[=]
static void dss_eff_set_sample_offset(struct dss_voice *v) {
	if(v->effect_arg != 0) {
		v->sample_offset = (uint16_t)((uint16_t)v->effect_arg << 7);
	}
	if(v->sample_offset != 0) {
		if(v->sample_offset >= v->sample_length) {
			v->sample_length = 0;
		} else {
			v->sample_length = (uint16_t)(v->sample_length - v->sample_offset);
			v->sample_start_offset += (uint32_t)v->sample_offset * 2u;
		}
	}
}

// [=]===^=[ dss_eff_set_fine_tune ]==============================================================[=]
static void dss_eff_set_fine_tune(struct dss_voice *v) {
	v->fine_tune = (uint8_t)(v->effect_arg & 0x0f);
}

// [=]===^=[ dss_eff_portamento ]=================================================================[=]
static void dss_eff_portamento(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	if(v->effect_arg != 0) {
		v->portamento_speed = v->effect_arg;
		v->effect_arg = 0;
	}
	dss_do_portamento(s, v, channel);
}

// [=]===^=[ dss_eff_portamento_volume_slide_up ]=================================================[=]
static void dss_eff_portamento_volume_slide_up(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	dss_do_portamento(s, v, channel);
	dss_eff_volume_slide_up(s, v, channel);
}

// [=]===^=[ dss_eff_portamento_volume_slide_down ]===============================================[=]
static void dss_eff_portamento_volume_slide_down(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	dss_do_portamento(s, v, channel);
	dss_eff_volume_slide_down(s, v, channel);
}

// [=]===^=[ dss_eff_portamento_control ]=========================================================[=]
static void dss_eff_portamento_control(struct dss_voice *v) {
	v->use_tone_portamento_for_portamento_effects = (v->effect_arg != 0) ? 1 : 0;
}

// [=]===^=[ dss_setup_instrument ]===============================================================[=]
static void dss_setup_instrument(struct digitalsoundstudio_state *s, struct dss_track_line *tl, struct dss_voice *v, int32_t channel) {
	v->sample = tl->sample;
	v->period = tl->period;
	v->effect = tl->effect;
	v->effect_arg = tl->effect_arg;

	if(v->sample != 0) {
		struct dss_sample *smp = &s->samples[v->sample - 1];
		v->playing_sample_number = (uint8_t)(v->sample - 1);
		v->sample_data = smp->data;
		v->loop_start = 0;
		v->loop_length = 0;

		if(v->sample_data != 0) {
			v->sample_start_offset = smp->start_offset;
			v->sample_length = smp->length;

			if(smp->loop_length > 1) {
				v->loop_start = smp->start_offset + smp->loop_start;
				v->loop_length = smp->loop_length;
			}

			v->fine_tune = smp->fine_tune;
			v->volume = smp->volume;
		}
	}

	uint16_t period = v->period;

	if((v->sample != 0) && (period != 0)) {
		if((v->effect == DSS_EFF_PORTAMENTO) || (v->effect == DSS_EFF_PORTAMENTO_VOLUME_SLIDE_UP) || (v->effect == DSS_EFF_PORTAMENTO_VOLUME_SLIDE_DOWN)) {
			dss_setup_portamento(period, v);
			dss_set_volume(s, v, channel);
			return;
		}

		if(period == 0x7ff) {
			paula_mute(&s->paula, channel);
			return;
		}

		if(v->effect == DSS_EFF_SET_FINE_TUNE) {
			dss_eff_set_fine_tune(v);
		} else if(v->effect == DSS_EFF_NOTE_DELAY) {
			dss_set_volume(s, v, channel);
			return;
		} else if(v->effect == DSS_EFF_SET_SAMPLE_OFFSET) {
			dss_eff_set_sample_offset(v);
		}

		if((v->sample_data != 0) && (v->sample_length > 0)) {
			uint32_t total_words = (uint32_t)v->sample_length + (uint32_t)v->loop_length;
			paula_queue_sample(&s->paula, channel, v->sample_data, v->sample_start_offset, total_words * 2u);
			if(v->loop_length != 0) {
				paula_set_loop(&s->paula, channel, v->loop_start, (uint32_t)v->loop_length * 2u);
			} else {
				paula_set_loop(&s->paula, channel, v->sample_start_offset, total_words * 2u);
			}

			period = dss_adjust_fine_tune(period, v);
			v->pitch_period = period;
			paula_set_period(&s->paula, channel, period);
		} else {
			paula_mute(&s->paula, channel);
		}
	}

	switch(v->effect) {
		case DSS_EFF_SET_MASTER_VOLUME: {
			dss_eff_set_master_volume(s, v);
			break;
		}

		case DSS_EFF_VOLUME_UP: {
			dss_eff_volume_up(s, v, channel);
			break;
		}

		case DSS_EFF_VOLUME_DOWN: {
			dss_eff_volume_down(s, v, channel);
			break;
		}

		case DSS_EFF_MASTER_VOLUME_UP: {
			dss_eff_master_volume_up(s, v);
			break;
		}

		case DSS_EFF_MASTER_VOLUME_DOWN: {
			dss_eff_master_volume_down(s, v);
			break;
		}

		case DSS_EFF_SET_VOLUME: {
			dss_eff_set_volume(s, v, channel);
			return;
		}
	}

	if(period != 0) {
		dss_set_volume(s, v, channel);
	}

	switch(v->effect) {
		case DSS_EFF_SET_SONG_SPEED: {
			dss_eff_set_song_speed(s, v);
			break;
		}

		case DSS_EFF_POSITION_JUMP: {
			dss_eff_position_jump(s, v);
			break;
		}

		case DSS_EFF_SET_FILTER: {
			dss_eff_set_filter(s, v);
			break;
		}

		case DSS_EFF_PITCH_UP: {
			dss_eff_pitch_up(s, v, channel);
			break;
		}

		case DSS_EFF_PITCH_DOWN: {
			dss_eff_pitch_down(s, v, channel);
			break;
		}

		case DSS_EFF_PITCH_CONTROL: {
			dss_eff_pitch_control(v);
			break;
		}

		case DSS_EFF_SET_SONG_TEMPO: {
			dss_eff_set_song_tempo(s, v);
			break;
		}

		case DSS_EFF_SET_LOOP_START: {
			dss_eff_set_loop_start(s, v);
			break;
		}

		case DSS_EFF_JUMP_TO_LOOP: {
			dss_eff_jump_to_loop(s, v);
			break;
		}

		case DSS_EFF_PORTAMENTO_CONTROL: {
			dss_eff_portamento_control(v);
			break;
		}
	}
}

// [=]===^=[ dss_make_effects ]===================================================================[=]
static void dss_make_effects(struct digitalsoundstudio_state *s, struct dss_voice *v, int32_t channel) {
	if(v->effect_arg != 0) {
		switch(v->effect) {
			case DSS_EFF_ARPEGGIO: {
				dss_eff_arpeggio(s, v, channel);
				break;
			}

			case DSS_EFF_SLIDE_UP: {
				dss_eff_slide_up(s, v, channel);
				break;
			}

			case DSS_EFF_SLIDE_DOWN: {
				dss_eff_slide_down(s, v, channel);
				break;
			}

			case DSS_EFF_VOLUME_SLIDE_UP: {
				dss_eff_volume_slide_up(s, v, channel);
				break;
			}

			case DSS_EFF_VOLUME_SLIDE_DOWN: {
				dss_eff_volume_slide_down(s, v, channel);
				break;
			}

			case DSS_EFF_MASTER_VOLUME_SLIDE_UP: {
				dss_eff_master_volume_slide_up(s, v);
				break;
			}

			case DSS_EFF_MASTER_VOLUME_SLIDE_DOWN: {
				dss_eff_master_volume_slide_down(s, v);
				break;
			}

			case DSS_EFF_RETRIG_NOTE: {
				dss_eff_retrig_note(s, v, channel);
				break;
			}

			case DSS_EFF_NOTE_DELAY: {
				dss_eff_note_delay(s, v, channel);
				break;
			}

			case DSS_EFF_NOTE_CUT: {
				dss_eff_note_cut(s, v, channel);
				break;
			}

			case DSS_EFF_PORTAMENTO_VOLUME_SLIDE_UP: {
				dss_eff_portamento_volume_slide_up(s, v, channel);
				break;
			}

			case DSS_EFF_PORTAMENTO_VOLUME_SLIDE_DOWN: {
				dss_eff_portamento_volume_slide_down(s, v, channel);
				break;
			}
		}
	}

	if(v->effect == DSS_EFF_PORTAMENTO) {
		dss_eff_portamento(s, v, channel);
	}
}

// [=]===^=[ dss_play_next_row ]==================================================================[=]
static void dss_play_next_row(struct digitalsoundstudio_state *s) {
	s->new_position = s->current_position;

	struct dss_pattern *pattern = &s->patterns[s->positions[s->current_position]];
	int16_t row = s->current_row;

	for(int32_t i = 0; i < DSS_NUM_CHANNELS; ++i) {
		dss_setup_instrument(s, &pattern->tracks[i][row], &s->voices[i], i);
	}

	for(int32_t i = 0; i < DSS_NUM_CHANNELS; ++i) {
		struct dss_voice *v = &s->voices[i];
		if(v->effect != DSS_EFF_NOTE_DELAY) {
			if(v->loop_length != 0) {
				paula_set_loop(&s->paula, i, v->loop_start, (uint32_t)v->loop_length * 2u);
			}
		}
	}

	if(s->set_loop_row) {
		s->current_row = s->loop_row;
		s->set_loop_row = 0;
	}

	s->current_row++;

	if((s->current_row >= DSS_NUM_ROWS) || s->position_jump) {
		s->position_jump = 0;
		s->current_row = 0;

		s->voices[0].loop_row = -1;
		s->voices[1].loop_row = -1;
		s->voices[2].loop_row = -1;
		s->voices[3].loop_row = -1;

		s->current_position = (uint16_t)(s->new_position + 1);
		if(s->current_position == s->num_positions) {
			s->current_position = 0;
		}
	}
}

// [=]===^=[ dss_tick ]===========================================================================[=]
static void dss_tick(struct digitalsoundstudio_state *s) {
	s->song_speed_counter++;

	if(s->song_speed_counter == s->current_song_speed) {
		s->song_speed_counter = 0;
		s->next_retrig_tick_number = 0;
		s->arpeggio_counter = 3;
		dss_play_next_row(s);
	} else {
		for(int32_t i = 0; i < DSS_NUM_CHANNELS; ++i) {
			struct dss_voice *v = &s->voices[i];
			if(v->period != 0) {
				dss_make_effects(s, v, i);
			}
		}
	}
}

// [=]===^=[ dss_cleanup ]========================================================================[=]
static void dss_cleanup(struct digitalsoundstudio_state *s) {
	if(!s) {
		return;
	}
	free(s->positions);   s->positions = 0;
	free(s->patterns);    s->patterns = 0;
}

// [=]===^=[ digitalsoundstudio_init ]============================================================[=]
static struct digitalsoundstudio_state *digitalsoundstudio_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < DSS_HEADER_END || sample_rate < 8000) {
		return 0;
	}

	if(!dss_test_module((uint8_t *)data, len)) {
		return 0;
	}

	struct digitalsoundstudio_state *s = (struct digitalsoundstudio_state *)calloc(1, sizeof(struct digitalsoundstudio_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len  = len;

	// Header layout: 'MMU2' (4) | unused (4) | tempo (1) | speed (1) | samples ...
	s->start_song_tempo = s->module_data[8];
	s->start_song_speed = s->module_data[9];

	int32_t pos = dss_load_sample_info(s);
	if(pos < 0) {
		goto fail;
	}

	pos = dss_load_positions(s, pos);
	if(pos < 0) {
		goto fail;
	}

	pos = dss_load_patterns(s, pos);
	if(pos < 0) {
		goto fail;
	}

	pos = dss_load_sample_data(s, pos);
	if(pos < 0) {
		goto fail;
	}

	paula_init(&s->paula, sample_rate, 50);
	dss_initialize_sound(s, 0);
	return s;

fail:
	dss_cleanup(s);
	free(s);
	return 0;
}

// [=]===^=[ digitalsoundstudio_free ]============================================================[=]
static void digitalsoundstudio_free(struct digitalsoundstudio_state *s) {
	if(!s) {
		return;
	}
	dss_cleanup(s);
	free(s);
}

// [=]===^=[ digitalsoundstudio_get_audio ]=======================================================[=]
static void digitalsoundstudio_get_audio(struct digitalsoundstudio_state *s, int16_t *output, int32_t frames) {
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
			dss_tick(s);
		}
	}
}

// [=]===^=[ digitalsoundstudio_api_init ]========================================================[=]
static void *digitalsoundstudio_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return digitalsoundstudio_init(data, len, sample_rate);
}

// [=]===^=[ digitalsoundstudio_api_free ]========================================================[=]
static void digitalsoundstudio_api_free(void *state) {
	digitalsoundstudio_free((struct digitalsoundstudio_state *)state);
}

// [=]===^=[ digitalsoundstudio_api_get_audio ]===================================================[=]
static void digitalsoundstudio_api_get_audio(void *state, int16_t *output, int32_t frames) {
	digitalsoundstudio_get_audio((struct digitalsoundstudio_state *)state, output, frames);
}

static const char *digitalsoundstudio_extensions[] = { "dss", 0 };

static struct player_api digitalsoundstudio_api = {
	"Digital Sound Studio",
	digitalsoundstudio_extensions,
	digitalsoundstudio_api_init,
	digitalsoundstudio_api_free,
	digitalsoundstudio_api_get_audio,
	0,
};
