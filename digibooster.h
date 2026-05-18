// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// DigiBooster 1.x replayer, ported from NostalgicPlayer's C# implementation.
// Drives an 8-channel Amiga Paula (see paula.h). 50Hz PAL base tick, with
// ProTracker-style BPM scaling on the F effect.
//
// Public API:
//   struct digibooster_state *digibooster_init(void *data, uint32_t len, int32_t sample_rate);
//   void digibooster_free(struct digibooster_state *s);
//   void digibooster_get_audio(struct digibooster_state *s, float *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define DIGIBOOSTER_BASE_TICK_HZ 50
#define DIGIBOOSTER_NUM_SAMPLES  31
#define DIGIBOOSTER_NUM_ORDERS   128
#define DIGIBOOSTER_PAT_ROWS     64
#define DIGIBOOSTER_MAX_CHANNELS 8
// Paula DMA period of a 5-8ch Split base channel's CPU submix.
// Fidelity-only: each soft voice steps at its own pitch. A/B'd vs UADE.
#define DIGIBOOSTER_MIX_PERIOD   320
#define DIGIBOOSTER_ROBOT_BUFLEN 2500

// Effects (high nibble of word 4 in the packed track-line).
enum {
	DB_FX_ARPEGGIO              = 0x0,
	DB_FX_PORTAMENTO_UP         = 0x1,
	DB_FX_PORTAMENTO_DOWN       = 0x2,
	DB_FX_GLISSANDO             = 0x3,
	DB_FX_VIBRATO               = 0x4,
	DB_FX_GLISSANDO_VOLUMESLIDE = 0x5,
	DB_FX_VIBRATO_VOLUMESLIDE   = 0x6,
	DB_FX_ROBOT                 = 0x8,
	DB_FX_SAMPLE_OFFSET         = 0x9,
	DB_FX_VOLUME_SLIDE          = 0xa,
	DB_FX_SONG_REPEAT           = 0xb,
	DB_FX_SET_VOLUME            = 0xc,
	DB_FX_PATTERN_BREAK         = 0xd,
	DB_FX_EXTRA                 = 0xe,
	DB_FX_SET_SPEED             = 0xf,
};

// Extended (E) effects, identified by the high nibble of the argument byte.
enum {
	DB_EX_FILTER          = 0x00,
	DB_EX_FINE_SLIDE_UP   = 0x10,
	DB_EX_FINE_SLIDE_DOWN = 0x20,
	DB_EX_BACKWARD_PLAY   = 0x30,
	DB_EX_STOP_PLAYING    = 0x40,
	DB_EX_CHANNEL_ON_OFF  = 0x50,
	DB_EX_LOOP            = 0x60,
	DB_EX_SAMPLE_OFFSET   = 0x80,
	DB_EX_RETRACE         = 0x90,
	DB_EX_FINE_VOL_UP     = 0xa0,
	DB_EX_FINE_VOL_DOWN   = 0xb0,
	DB_EX_CUT_SAMPLE      = 0xc0,
	DB_EX_PATTERN_DELAY   = 0xe0,
};

struct digibooster_track_line {
	uint16_t period;
	uint8_t sample_number;
	uint8_t effect;
	uint8_t effect_arg;
};

struct digibooster_pattern {
	struct digibooster_track_line rows[DIGIBOOSTER_MAX_CHANNELS][DIGIBOOSTER_PAT_ROWS];
};

struct digibooster_sample {
	int8_t *sample_data;        // points into caller's module buffer
	uint32_t length;
	uint32_t loop_start;
	uint32_t loop_length;
	uint8_t volume;
	int8_t fine_tune;
};

struct digibooster_last_round_info {
	uint16_t period;
	uint8_t sample_number;
	uint8_t effect;
	uint8_t effect_arg;
};

struct digibooster_channel {
	uint8_t volume;
	uint8_t slide_volume_old;
	uint8_t off_enable;
	uint8_t sample_offset;
	uint8_t retrace_count;
	uint8_t old_sample_number;
	uint8_t robot_old_value;
	uint8_t robot_enable;
	int32_t robot_bytes_to_play;
	int32_t robot_current_position;
	int8_t *robot_buffers[2];
	int16_t main_period;
	uint8_t main_volume;
	uint8_t play_pointer;
	struct digibooster_last_round_info last_round_info;
	int8_t loop_pattern_position;
	uint8_t loop_song_position;
	uint8_t loop_how_many;
	uint8_t backward_enabled;   // 0 = forward, 1 = once, 2 = with loop
	uint8_t portamento_up_old;
	uint8_t portamento_down_old;
	uint16_t vibrato_period;
	int8_t vibrato_value;
	uint8_t vibrato_old_value;
	uint8_t glissando_old_value;
	uint8_t glissando_enable;
	uint16_t glissando_old_period;
	uint16_t glissando_new_period;
	uint8_t on_off_channel;
	uint16_t original_period;
	uint8_t old_volume;
	uint8_t is_active;
	int8_t *sample_data;
	uint32_t start_offset;
};

// DigiBooster 1.x (.digi) Split soft voice. <=4 channel modules use the
// real hardware path; 5-8 channel modules pair tracks two-per-Paula-
// channel (base = channel/2) reproducing digibooster_pan's LLRRRRLL via
// the four base channels' fixed hard pan. DigiBooster sums its voices
// in software (no pre-attenuation).
struct digibooster_softvoice {
	int8_t *sample;
	uint32_t length;
	uint32_t loop_start;
	uint32_t loop_length;          // 0 => one-shot
	uint32_t pos;
	uint32_t period;
	uint64_t step_q;               // Q32 sample bytes per output sample
	uint64_t step_acc;
	int8_t *pending_sample;
	uint32_t pending_pos;
	uint32_t pending_length;
	uint16_t volume;               // 0..64
	uint8_t has_pending;
	uint8_t active;
};

struct digibooster_state {
	struct paula paula;

	uint8_t *module_data;       // caller-owned, kept for sample data refs
	uint32_t module_len;

	uint8_t version;
	uint8_t number_of_channels;
	uint8_t number_of_patterns;
	uint8_t song_length;

	uint8_t orders[DIGIBOOSTER_NUM_ORDERS];
	struct digibooster_sample samples[DIGIBOOSTER_NUM_SAMPLES];
	struct digibooster_pattern *patterns;

	// Robot effect double-buffers, allocated per channel on first use.
	int8_t *robot_storage[DIGIBOOSTER_MAX_CHANNELS][2];

	struct digibooster_channel channels[DIGIBOOSTER_MAX_CHANNELS];
	struct digibooster_track_line current_row[DIGIBOOSTER_MAX_CHANNELS];

	uint16_t cia_tempo;
	uint8_t tempo;
	uint8_t count;
	uint8_t song_position;
	int8_t pattern_position;
	uint16_t pause_vbl;
	uint8_t pause_enabled;
	uint8_t end_reached;
	uint8_t amiga_filter;

	// 5-8 channel Split submixer: 4 base buffers (one block), one per
	// Paula channel, regenerated each tick from the soft voices.
	struct digibooster_softvoice softv[DIGIBOOSTER_MAX_CHANNELS];
	int8_t *softmix_buf;
	uint32_t softmix_cap;
	int32_t softmix_period;
};

// [=]===^=[ digibooster_periods ]================================================================[=]
// Tuning -8..+7 (16 entries), 36 notes each.
static uint16_t digibooster_periods[16][36] = {
	{ 907, 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120 },
	{ 900, 850, 802, 757, 715, 675, 636, 601, 567, 535, 505, 477, 450, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 238, 225, 212, 200, 189, 179, 169, 159, 150, 142, 134, 126, 119 },
	{ 894, 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447, 422, 398, 376, 355, 335, 316, 298, 282, 266, 251, 237, 223, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118 },
	{ 887, 838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470, 444, 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235, 222, 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118 },
	{ 881, 832, 785, 741, 699, 660, 623, 588, 555, 524, 494, 467, 441, 416, 392, 370, 350, 330, 312, 294, 278, 262, 247, 233, 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 123, 117 },
	{ 875, 826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437, 413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232, 219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116 },
	{ 868, 820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434, 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230, 217, 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115 },
	{ 862, 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431, 407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228, 216, 203, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114 },
	{ 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113 },
	{ 850, 802, 757, 715, 674, 637, 601, 567, 535, 505, 477, 450, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 239, 225, 213, 201, 189, 179, 169, 159, 150, 142, 134, 126, 119, 113 },
	{ 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447, 422, 398, 376, 355, 335, 316, 298, 282, 266, 251, 237, 224, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118, 112 },
	{ 838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470, 444, 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235, 222, 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118, 111 },
	{ 832, 785, 741, 699, 660, 623, 588, 555, 524, 495, 467, 441, 416, 392, 370, 350, 330, 312, 294, 278, 262, 247, 233, 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 124, 117, 110 },
	{ 826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437, 413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232, 219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116, 109 },
	{ 820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434, 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230, 217, 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115, 109 },
	{ 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431, 407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228, 216, 204, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114, 108 },
};

// [=]===^=[ digibooster_arpeggio_list ]==========================================================[=]
static uint8_t digibooster_arpeggio_list[32] = {
	0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0,
	1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1,
};

// [=]===^=[ digibooster_vibrato_sin ]============================================================[=]
static uint8_t digibooster_vibrato_sin[32] = {
	0x00, 0x18, 0x31, 0x4a, 0x61, 0x78, 0x8d, 0xa1, 0xb4, 0xc5, 0xd4, 0xe0, 0xeb, 0xf4, 0xfa, 0xfd,
	0xff, 0xfd, 0xfa, 0xf4, 0xeb, 0xe0, 0xd4, 0xc5, 0xb4, 0xa1, 0x8d, 0x78, 0x61, 0x4a, 0x31, 0x18,
};

// [=]===^=[ digibooster_hex ]====================================================================[=]
// BCD decode for the D effect (pattern break).
static uint8_t digibooster_hex[100] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
	10, 11, 12, 13, 14, 15, 16, 17, 18, 19,  0,  0,  0,  0,  0,  0,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29,  0,  0,  0,  0,  0,  0,
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39,  0,  0,  0,  0,  0,  0,
	40, 41, 42, 43, 44, 45, 46, 47, 48, 49,  0,  0,  0,  0,  0,  0,
	50, 51, 52, 53, 54, 55, 56, 57, 58, 59,  0,  0,  0,  0,  0,  0,
	60, 61, 62, 63,
};

// [=]===^=[ digibooster_read_u16_be ]============================================================[=]
static uint16_t digibooster_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ digibooster_read_u32_be ]============================================================[=]
static uint32_t digibooster_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ digibooster_check_mark ]=============================================================[=]
static int32_t digibooster_check_mark(uint8_t *p, uint32_t len, uint32_t off, const char *mark, uint32_t mlen) {
	if(off + mlen > len) {
		return 0;
	}
	for(uint32_t i = 0; i < mlen; ++i) {
		if(p[off + i] != (uint8_t)mark[i]) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ digibooster_set_bpm_tempo ]==========================================================[=]
// ProTracker BPM scale: 50Hz at BPM=125. samples_per_tick = sample_rate * 5 / (2 * bpm).
static void digibooster_set_bpm_tempo(struct digibooster_state *s, uint16_t bpm) {
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

// [=]===^=[ digibooster_parse_track_line ]=======================================================[=]
static struct digibooster_track_line digibooster_parse_track_line(uint32_t data) {
	struct digibooster_track_line tl;
	tl.period = (uint16_t)((data >> 16) & 0x0fff);
	tl.sample_number = (uint8_t)(((data >> 24) & 0xf0) | ((data >> 12) & 0x0f));
	tl.effect = (uint8_t)((data >> 8) & 0x0f);
	tl.effect_arg = (uint8_t)(data & 0xff);
	return tl;
}

// [=]===^=[ digibooster_identify ]===============================================================[=]
static int32_t digibooster_identify(uint8_t *data, uint32_t len) {
	if(len < 1572) {
		return 0;
	}
	return digibooster_check_mark(data, len, 0, "DIGI Booster module\0", 20);
}

// [=]===^=[ digibooster_load_sample_info ]=======================================================[=]
static int32_t digibooster_load_sample_info(struct digibooster_state *s) {
	uint8_t *d = s->module_data;
	uint32_t len = s->module_len;
	// Header layout: 24 bytes mark+pad, [24]=version, [25]=channels, [26]=packed,
	// [27..45] reserved/padding, [46]=numPatterns-1, [47]=songLength-1,
	// [48..175]=orders[128], [176..]=sample info table.
	uint32_t base = 176;
	uint32_t lengths_off = base;
	uint32_t loop_starts_off = base + 31 * 4;
	uint32_t loop_lengths_off = base + 31 * 8;
	uint32_t volumes_off = base + 31 * 12;
	uint32_t finetunes_off = volumes_off + 31;
	if(finetunes_off + 31 > len) {
		return 0;
	}
	for(int32_t i = 0; i < DIGIBOOSTER_NUM_SAMPLES; ++i) {
		struct digibooster_sample *sm = &s->samples[i];
		uint32_t length = digibooster_read_u32_be(&d[lengths_off + i * 4]);
		uint32_t loop_start = digibooster_read_u32_be(&d[loop_starts_off + i * 4]);
		uint32_t loop_length = digibooster_read_u32_be(&d[loop_lengths_off + i * 4]);
		if(loop_start > length || loop_length == 0) {
			loop_start = 0;
			loop_length = 0;
		} else if(loop_start + loop_length >= length) {
			loop_length = length - loop_start;
		}
		sm->length = length;
		sm->loop_start = loop_start;
		sm->loop_length = loop_length;
		sm->volume = d[volumes_off + i];
		int8_t ft = (int8_t)d[finetunes_off + i];
		if(s->version >= 0x10 && s->version <= 0x13) {
			ft = 0;
		}
		sm->fine_tune = ft;
		sm->sample_data = 0;
	}
	return 1;
}

// [=]===^=[ digibooster_load_packed_patterns ]===================================================[=]
static int32_t digibooster_load_packed_patterns(struct digibooster_state *s, uint32_t *off) {
	uint8_t *d = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = *off;
	for(int32_t i = 0; i < s->number_of_patterns; ++i) {
		if(pos + 2 > len) {
			return 0;
		}
		int32_t length = (int32_t)digibooster_read_u16_be(&d[pos]);
		pos += 2;
		if(pos + 64 > len) {
			return 0;
		}
		uint8_t bit_masks[64];
		memcpy(bit_masks, &d[pos], 64);
		pos += 64;
		length -= 64;
		struct digibooster_pattern *pat = &s->patterns[i];
		for(int32_t j = 0; j < 64; ++j) {
			uint8_t mask = bit_masks[j];
			int32_t m = 0;
			for(int32_t k = 0x80; k > 0; k >>= 1, ++m) {
				uint32_t data = 0;
				if((mask & k) != 0) {
					if(pos + 4 > len) {
						return 0;
					}
					data = digibooster_read_u32_be(&d[pos]);
					pos += 4;
					length -= 4;
				}
				pat->rows[m][j] = digibooster_parse_track_line(data);
			}
		}
		if(length != 0) {
			return 0;
		}
	}
	*off = pos;
	return 1;
}

// [=]===^=[ digibooster_load_unpacked_patterns ]=================================================[=]
static int32_t digibooster_load_unpacked_patterns(struct digibooster_state *s, uint32_t *off) {
	uint8_t *d = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = *off;
	for(int32_t i = 0; i < s->number_of_patterns; ++i) {
		struct digibooster_pattern *pat = &s->patterns[i];
		for(int32_t j = 0; j < 8; ++j) {
			for(int32_t k = 0; k < 64; ++k) {
				if(pos + 4 > len) {
					return 0;
				}
				uint32_t data = digibooster_read_u32_be(&d[pos]);
				pos += 4;
				pat->rows[j][k] = digibooster_parse_track_line(data);
			}
		}
	}
	*off = pos;
	return 1;
}

// [=]===^=[ digibooster_load ]===================================================================[=]
static int32_t digibooster_load(struct digibooster_state *s) {
	uint8_t *d = s->module_data;
	uint32_t len = s->module_len;
	if(len < 1572) {
		return 0;
	}
	s->version = d[24];
	s->number_of_channels = d[25];
	uint8_t packed = d[26];
	if(s->number_of_channels == 0 || s->number_of_channels > DIGIBOOSTER_MAX_CHANNELS) {
		return 0;
	}
	s->number_of_patterns = (uint8_t)(d[46] + 1);
	s->song_length = (uint8_t)(d[47] + 1);

	memcpy(s->orders, &d[48], DIGIBOOSTER_NUM_ORDERS);

	if(!digibooster_load_sample_info(s)) {
		return 0;
	}

	// 176 (orders end) + 31 * 14 (lengths/loop_starts/loop_lengths/vols/fts) = 610.
	// Then song name (32) and 31 sample names (30 each) = 962. Total = 1572.
	uint32_t pos = 176 + 31 * (4 + 4 + 4 + 1 + 1);
	pos += 32;
	pos += 31 * 30;

	s->patterns = (struct digibooster_pattern *)calloc(s->number_of_patterns, sizeof(struct digibooster_pattern));
	if(!s->patterns) {
		return 0;
	}

	int32_t ok;
	if(packed) {
		ok = digibooster_load_packed_patterns(s, &pos);
	} else {
		ok = digibooster_load_unpacked_patterns(s, &pos);
	}
	if(!ok) {
		return 0;
	}

	for(int32_t i = 0; i < DIGIBOOSTER_NUM_SAMPLES; ++i) {
		struct digibooster_sample *sm = &s->samples[i];
		if(sm->length != 0) {
			if(pos + sm->length > len) {
				return 0;
			}
			sm->sample_data = (int8_t *)&d[pos];
			pos += sm->length;
		}
	}
	return 1;
}

// [=]===^=[ digibooster_initialize_sound ]=======================================================[=]
static void digibooster_initialize_sound(struct digibooster_state *s) {
	s->cia_tempo = 125;
	s->tempo = 6;
	s->count = 6;
	s->song_position = 0;
	s->pattern_position = 0;
	s->pause_vbl = 0;
	s->pause_enabled = 0;
	s->end_reached = 0;
	s->amiga_filter = 0;

	for(int32_t i = 0; i < DIGIBOOSTER_MAX_CHANNELS; ++i) {
		memset(&s->channels[i], 0, sizeof(struct digibooster_channel));
		memset(&s->current_row[i], 0, sizeof(struct digibooster_track_line));
	}

	digibooster_set_bpm_tempo(s, s->cia_tempo);
}

// [=]===^=[ digibooster_find_period ]============================================================[=]
static uint16_t digibooster_find_period(struct digibooster_state *s, struct digibooster_channel *ch, struct digibooster_track_line *tl) {
	uint8_t sample_number = tl->sample_number;
	if(sample_number == 0) {
		sample_number = ch->old_sample_number;
	}
	if(sample_number == 0) {
		return 0;
	}
	struct digibooster_sample *sm = &s->samples[sample_number - 1];
	if(sm->fine_tune == 0) {
		return tl->period;
	}
	int8_t ft = sm->fine_tune;
	uint16_t period = tl->period;
	if(ft >= -8 && ft <= 7) {
		for(int32_t i = 0; i < 36; ++i) {
			if(digibooster_periods[8][i] == period) {
				return digibooster_periods[ft + 8][i];
			}
		}
	}
	return (uint16_t)(period - ((period * ft) / 140));
}

// [=]===^=[ digibooster_test_period ]============================================================[=]
static void digibooster_test_period(struct digibooster_channel *ch) {
	if(ch->last_round_info.period == 0) {
		ch->last_round_info.sample_number = 0;
		ch->last_round_info.effect = DB_FX_ARPEGGIO;
		ch->last_round_info.effect_arg = 0;
	} else if(ch->last_round_info.period < 113) {
		ch->last_round_info.period = 113;
	}
}

// [=]===^=[ digibooster_arpeggio ]===============================================================[=]
static void digibooster_arpeggio(struct digibooster_state *s, struct digibooster_channel *ch, uint8_t arg) {
	uint8_t arp = digibooster_arpeggio_list[s->count - 1];
	if(arp == 0) {
		ch->last_round_info.period = ch->original_period;
		return;
	}
	if(arp == 2) {
		arg &= 0x0f;
	} else {
		arg = (uint8_t)((arg & 0xf0) >> 4);
	}
	uint16_t period = ch->original_period;
	for(uint8_t i = 0; i < 36; ++i) {
		if(period >= digibooster_periods[8][i]) {
			arg += i;
			if(arg >= 35) {
				arg = 35;
			}
			period = digibooster_periods[8][arg];
			break;
		}
	}
	ch->last_round_info.period = period;
}

// [=]===^=[ digibooster_portamento_up ]==========================================================[=]
static void digibooster_portamento_up(struct digibooster_channel *ch, uint8_t arg) {
	if(arg == 0) {
		arg = ch->portamento_up_old;
	} else {
		ch->portamento_up_old = arg;
	}
	ch->last_round_info.period = (uint16_t)(ch->last_round_info.period - arg);
	if(ch->last_round_info.period < 113) {
		ch->last_round_info.period = 113;
	}
}

// [=]===^=[ digibooster_portamento_down ]========================================================[=]
static void digibooster_portamento_down(struct digibooster_channel *ch, uint8_t arg) {
	if(arg == 0) {
		arg = ch->portamento_down_old;
	} else {
		ch->portamento_down_old = arg;
	}
	ch->last_round_info.period = (uint16_t)(ch->last_round_info.period + arg);
	if(ch->last_round_info.period > 856) {
		ch->last_round_info.period = 856;
	}
}

// [=]===^=[ digibooster_volume_slide ]===========================================================[=]
static void digibooster_volume_slide(struct digibooster_channel *ch, uint8_t arg) {
	if(arg == 0) {
		arg = ch->slide_volume_old;
	} else {
		ch->slide_volume_old = arg;
	}
	if(arg < 0x10) {
		int16_t v = (int16_t)((int8_t)ch->volume - (int8_t)arg);
		if(v < 0) {
			v = 0;
		}
		ch->volume = (uint8_t)v;
	} else {
		uint16_t v = (uint16_t)(ch->volume + (arg >> 4));
		if(v > 64) {
			v = 64;
		}
		ch->volume = (uint8_t)v;
	}
}

// [=]===^=[ digibooster_glissando ]==============================================================[=]
static void digibooster_glissando(struct digibooster_state *s, struct digibooster_channel *ch, uint8_t arg) {
	if(arg == 0) {
		arg = ch->glissando_old_value;
	}
	if(s->count == 1) {
		ch->glissando_old_value = arg;
	}
	if(ch->glissando_old_period == 0) {
		return;
	}
	if(ch->glissando_new_period == 0) {
		ch->glissando_new_period = ch->last_round_info.period;
		ch->last_round_info.period = ch->glissando_old_period;
		ch->glissando_enable = 0;
		if(ch->glissando_new_period == ch->last_round_info.period) {
			ch->glissando_new_period = 0;
			return;
		}
		if(ch->glissando_new_period < ch->last_round_info.period) {
			ch->glissando_enable = 1;
		}
	} else {
		if(ch->glissando_enable) {
			ch->glissando_old_period = (uint16_t)(ch->glissando_old_period - arg);
			if(ch->glissando_old_period <= ch->glissando_new_period) {
				ch->glissando_old_period = ch->glissando_new_period;
				ch->glissando_new_period = 0;
			}
		} else {
			ch->glissando_old_period = (uint16_t)(ch->glissando_old_period + arg);
			if(ch->glissando_old_period >= ch->glissando_new_period) {
				ch->glissando_old_period = ch->glissando_new_period;
				ch->glissando_new_period = 0;
			}
		}
		ch->last_round_info.period = ch->glissando_old_period;
	}
}

// [=]===^=[ digibooster_vibrato ]================================================================[=]
static void digibooster_vibrato(struct digibooster_state *s, struct digibooster_channel *ch, uint8_t arg) {
	if(s->count == s->tempo && ch->vibrato_period == 0) {
		ch->vibrato_period = ch->last_round_info.period;
	}
	uint16_t period = ch->vibrato_period;
	if(s->count == (s->tempo - 1)) {
		ch->vibrato_period = 0;
		ch->last_round_info.period = period;
		return;
	}
	if((arg & 0x0f) == 0) {
		arg = (uint8_t)(arg | (ch->vibrato_old_value & 0x0f));
	}
	if((arg & 0xf0) == 0) {
		arg = (uint8_t)(arg | (ch->vibrato_old_value & 0xf0));
	}
	ch->vibrato_old_value = arg;
	uint16_t vib_val = digibooster_vibrato_sin[(ch->vibrato_value >> 2) & 0x1f];
	vib_val = (uint16_t)(((arg & 0x0f) * vib_val) >> 7);
	if(ch->vibrato_value < 0) {
		period = (uint16_t)(period - vib_val);
	} else {
		period = (uint16_t)(period + vib_val);
	}
	ch->vibrato_value = (int8_t)(ch->vibrato_value + (int8_t)((arg >> 2) & 0x3c));
	ch->last_round_info.period = period;
}

// [=]===^=[ digibooster_glissando_volume_slide ]=================================================[=]
static void digibooster_glissando_volume_slide(struct digibooster_state *s, struct digibooster_channel *ch, uint8_t arg) {
	digibooster_volume_slide(ch, arg);
	digibooster_glissando(s, ch, 0);
}

// [=]===^=[ digibooster_vibrato_volume_slide ]===================================================[=]
static void digibooster_vibrato_volume_slide(struct digibooster_state *s, struct digibooster_channel *ch, uint8_t arg) {
	digibooster_volume_slide(ch, arg);
	digibooster_vibrato(s, ch, 0);
}

// [=]===^=[ digibooster_make_robot_buffer ]======================================================[=]
// Copy part of the playing sample into a small private buffer to fake a tight loop.
static void digibooster_make_robot_buffer(struct digibooster_state *s, struct digibooster_channel *ch, int32_t chan_idx, int32_t tick_bytes) {
	if(ch->robot_buffers[0] == 0) {
		s->robot_storage[chan_idx][0] = (int8_t *)calloc(DIGIBOOSTER_ROBOT_BUFLEN, 1);
		s->robot_storage[chan_idx][1] = (int8_t *)calloc(DIGIBOOSTER_ROBOT_BUFLEN, 1);
		ch->robot_buffers[0] = s->robot_storage[chan_idx][0];
		ch->robot_buffers[1] = s->robot_storage[chan_idx][1];
	}
	if(tick_bytes > DIGIBOOSTER_ROBOT_BUFLEN) {
		tick_bytes = DIGIBOOSTER_ROBOT_BUFLEN;
	}
	struct digibooster_sample *sm = &s->samples[ch->old_sample_number - 1];
	if(sm->loop_length == 0) {
		int32_t todo = tick_bytes;
		int32_t avail = (int32_t)sm->length - ch->robot_current_position;
		if(todo > avail) {
			todo = avail;
		}
		if(todo < 0) {
			todo = 0;
		}
		if(sm->sample_data && todo > 0) {
			memcpy(ch->robot_buffers[0], &sm->sample_data[ch->robot_current_position], todo);
		}
		ch->robot_current_position += todo;
		if(ch->robot_current_position >= (int32_t)sm->length) {
			int32_t left = tick_bytes - todo;
			if(left > 0) {
				memset(&ch->robot_buffers[0][todo], 0, left);
			}
			ch->last_round_info.period = 0;
			ch->last_round_info.effect = DB_FX_ARPEGGIO;
			ch->last_round_info.effect_arg = 0;
		}
	} else {
		int32_t loop_end = (int32_t)(sm->loop_start + sm->loop_length);
		int32_t dest_pos = 0;
		while(tick_bytes > 0) {
			int32_t todo = tick_bytes;
			int32_t avail = loop_end - ch->robot_current_position;
			if(todo > avail) {
				todo = avail;
			}
			if(todo < 0) {
				todo = 0;
			}
			if(dest_pos + todo > DIGIBOOSTER_ROBOT_BUFLEN) {
				todo = DIGIBOOSTER_ROBOT_BUFLEN - dest_pos;
			}
			if(todo <= 0) {
				break;
			}
			if(sm->sample_data) {
				memcpy(&ch->robot_buffers[0][dest_pos], &sm->sample_data[ch->robot_current_position], todo);
			}
			ch->robot_current_position += todo;
			tick_bytes -= todo;
			dest_pos += todo;
			if(ch->robot_current_position >= loop_end) {
				ch->robot_current_position = (int32_t)sm->loop_start;
			}
		}
	}
}

// [=]===^=[ digibooster_robot_effect ]===========================================================[=]
static void digibooster_robot_effect(struct digibooster_state *s, struct digibooster_channel *ch, int32_t chan_idx, uint8_t arg) {
	if(!ch->robot_enable) {
		ch->off_enable = 1;
		ch->robot_enable = 1;
		ch->robot_current_position = 0;
	}
	int32_t tick_bytes = 2;
	if(ch->main_period > 0 && s->cia_tempo > 0) {
		tick_bytes = ((35468 * 125 / (int32_t)s->cia_tempo) / (int32_t)ch->main_period) * 2 + 2;
	}
	digibooster_make_robot_buffer(s, ch, chan_idx, tick_bytes);
	if(arg == 0) {
		arg = ch->robot_old_value;
	} else {
		ch->robot_old_value = arg;
	}
	int32_t to_play_bytes = (tick_bytes >> 6) * (((int32_t)arg + 80) >> 2);
	if(tick_bytes > to_play_bytes) {
		to_play_bytes = tick_bytes - to_play_bytes + 1;
	} else {
		to_play_bytes = 2;
	}
	ch->robot_bytes_to_play = to_play_bytes;
}

// [=]===^=[ digibooster_loop_effect ]============================================================[=]
static void digibooster_loop_effect(struct digibooster_state *s, struct digibooster_channel *ch, uint8_t arg) {
	if(arg == 0) {
		if(ch->loop_how_many == 0) {
			ch->loop_pattern_position = (int8_t)(s->pattern_position - 1);
			ch->loop_song_position = s->song_position;
		}
	} else {
		if(ch->loop_how_many != 0) {
			ch->loop_how_many--;
			if(ch->loop_how_many == 0) {
				ch->loop_pattern_position = 0;
				ch->loop_song_position = 0;
			} else {
				s->pattern_position = ch->loop_pattern_position;
				s->song_position = ch->loop_song_position;
			}
		} else {
			ch->loop_how_many = arg;
			s->pattern_position = ch->loop_pattern_position;
			s->song_position = ch->loop_song_position;
		}
	}
}

// [=]===^=[ digibooster_pattern_delay ]==========================================================[=]
static void digibooster_pattern_delay(struct digibooster_state *s, uint8_t arg) {
	if(!s->pause_enabled) {
		if(arg != 0) {
			s->pause_vbl = (uint16_t)(s->tempo * arg + 1);
		}
	}
}

// [=]===^=[ digibooster_song_repeat ]============================================================[=]
static void digibooster_song_repeat(struct digibooster_state *s, uint8_t arg) {
	s->pattern_position = -1;
	if(arg > 127) {
		arg = 127;
	}
	if(arg <= s->song_position) {
		s->end_reached = 1;
	}
	s->song_position = arg;
}

// [=]===^=[ digibooster_pattern_break ]==========================================================[=]
static void digibooster_pattern_break(struct digibooster_state *s, uint8_t arg) {
	if(arg > 0x63) {
		arg = 0x63;
	}
	if(s->pattern_position != -1) {
		s->song_position++;
		if(s->song_position >= s->song_length) {
			s->song_position = 0;
		}
	}
	s->pattern_position = (int8_t)(digibooster_hex[arg] - 1);
}

// [=]===^=[ digibooster_sample_offset_main ]=====================================================[=]
static void digibooster_sample_offset_main(struct digibooster_state *s, struct digibooster_channel *ch, uint8_t arg) {
	ch->start_offset += (uint32_t)ch->sample_offset * 65536u + (uint32_t)arg * 256u;
	uint8_t sn = ch->old_sample_number;
	if(sn != 0) {
		uint32_t slen = s->samples[sn - 1].length;
		if(ch->start_offset > slen) {
			ch->start_offset = slen;
		}
	}
}

// [=]===^=[ digibooster_set_speed_or_tempo ]=====================================================[=]
static void digibooster_set_speed_or_tempo(struct digibooster_state *s, uint8_t arg) {
	if(arg > 31) {
		if(arg != s->cia_tempo) {
			s->cia_tempo = arg;
			digibooster_set_bpm_tempo(s, arg);
		}
	} else {
		s->count = arg;
		s->tempo = arg;
	}
}

// [=]===^=[ digibooster_retrace ]================================================================[=]
static void digibooster_retrace(struct digibooster_state *s, struct digibooster_channel *ch, uint8_t arg) {
	if(s->count == 1) {
		ch->retrace_count = 0;
	}
	if(ch->retrace_count == (uint8_t)(arg - 1)) {
		ch->off_enable = 1;
		ch->start_offset = 0;
		ch->retrace_count = 0;
	} else {
		ch->retrace_count++;
	}
}

// [=]===^=[ digibooster_cut_sample ]=============================================================[=]
static void digibooster_cut_sample(struct digibooster_state *s, struct digibooster_channel *ch, uint8_t arg) {
	if(s->count == arg) {
		ch->volume = 0;
	}
}

// [=]===^=[ digibooster_effect_commands ]========================================================[=]
// Per-tick effects.
static void digibooster_effect_commands(struct digibooster_state *s, struct digibooster_channel *ch) {
	uint8_t effect = ch->last_round_info.effect;
	uint8_t arg = ch->last_round_info.effect_arg;
	if(effect == DB_FX_ARPEGGIO && arg == 0) {
		return;
	}
	switch(effect) {
		case DB_FX_ARPEGGIO: {
			digibooster_arpeggio(s, ch, arg);
			break;
		}

		case DB_FX_PORTAMENTO_UP: {
			digibooster_portamento_up(ch, arg);
			break;
		}

		case DB_FX_PORTAMENTO_DOWN: {
			digibooster_portamento_down(ch, arg);
			break;
		}

		case DB_FX_GLISSANDO: {
			digibooster_glissando(s, ch, arg);
			break;
		}

		case DB_FX_VIBRATO: {
			digibooster_vibrato(s, ch, arg);
			break;
		}

		case DB_FX_GLISSANDO_VOLUMESLIDE: {
			digibooster_glissando_volume_slide(s, ch, arg);
			break;
		}

		case DB_FX_VIBRATO_VOLUMESLIDE: {
			digibooster_vibrato_volume_slide(s, ch, arg);
			break;
		}

		case DB_FX_VOLUME_SLIDE: {
			digibooster_volume_slide(ch, arg);
			break;
		}

		case DB_FX_EXTRA: {
			uint8_t ex = (uint8_t)(arg & 0xf0);
			uint8_t exa = (uint8_t)(arg & 0x0f);
			switch(ex) {
				case DB_EX_RETRACE: {
					digibooster_retrace(s, ch, exa);
					break;
				}

				case DB_EX_CUT_SAMPLE: {
					digibooster_cut_sample(s, ch, exa);
					break;
				}
			}
			break;
		}
	}
}

// [=]===^=[ digibooster_effect_commands2 ]=======================================================[=]
// Trigger-tick effects (run only on the first tick of a row).
static void digibooster_effect_commands2(struct digibooster_state *s, struct digibooster_channel *ch) {
	uint8_t effect = ch->last_round_info.effect;
	uint8_t arg = ch->last_round_info.effect_arg;
	if(effect == DB_FX_ARPEGGIO && arg == 0) {
		return;
	}
	switch(effect) {
		case DB_FX_SAMPLE_OFFSET: {
			digibooster_sample_offset_main(s, ch, arg);
			break;
		}

		case DB_FX_SONG_REPEAT: {
			digibooster_song_repeat(s, arg);
			break;
		}

		case DB_FX_SET_VOLUME: {
			ch->volume = arg;
			break;
		}

		case DB_FX_PATTERN_BREAK: {
			digibooster_pattern_break(s, arg);
			break;
		}

		case DB_FX_SET_SPEED: {
			digibooster_set_speed_or_tempo(s, arg);
			break;
		}

		case DB_FX_EXTRA: {
			uint8_t ex = (uint8_t)(arg & 0xf0);
			uint8_t exa = (uint8_t)(arg & 0x0f);
			switch(ex) {
				case DB_EX_FILTER: {
					if(exa == 0) {
						s->amiga_filter = 1;
					} else if(exa == 1) {
						s->amiga_filter = 0;
					}
					break;
				}

				case DB_EX_CHANNEL_ON_OFF: {
					if(exa == 0) {
						ch->on_off_channel = 1;
					} else if(exa == 1) {
						ch->on_off_channel = 0;
					}
					break;
				}

				case DB_EX_FINE_SLIDE_UP: {
					ch->last_round_info.period = (uint16_t)(ch->last_round_info.period - exa);
					if(ch->last_round_info.period < 113) {
						ch->last_round_info.period = 113;
					}
					ch->last_round_info.effect = DB_FX_ARPEGGIO;
					ch->last_round_info.effect_arg = 0;
					break;
				}

				case DB_EX_FINE_SLIDE_DOWN: {
					ch->last_round_info.period = (uint16_t)(ch->last_round_info.period + exa);
					if(ch->last_round_info.period > 856) {
						ch->last_round_info.period = 856;
					}
					ch->last_round_info.effect = DB_FX_ARPEGGIO;
					ch->last_round_info.effect_arg = 0;
					break;
				}

				case DB_EX_LOOP: {
					digibooster_loop_effect(s, ch, exa);
					break;
				}

				case DB_EX_SAMPLE_OFFSET: {
					ch->sample_offset = exa;
					break;
				}

				case DB_EX_FINE_VOL_UP: {
					uint16_t v = (uint16_t)(ch->volume + exa);
					if(v > 64) {
						v = 64;
					}
					ch->volume = (uint8_t)v;
					ch->last_round_info.effect = DB_FX_ARPEGGIO;
					ch->last_round_info.effect_arg = 0;
					break;
				}

				case DB_EX_FINE_VOL_DOWN: {
					int16_t v = (int16_t)((int8_t)ch->volume - (int8_t)exa);
					if(v < 0) {
						v = 0;
					}
					ch->volume = (uint8_t)v;
					ch->last_round_info.effect = DB_FX_ARPEGGIO;
					ch->last_round_info.effect_arg = 0;
					break;
				}

				case DB_EX_PATTERN_DELAY: {
					digibooster_pattern_delay(s, exa);
					break;
				}
			}
			break;
		}
	}
}

// [=]===^=[ digibooster_parse_row_data ]=========================================================[=]
static void digibooster_parse_row_data(struct digibooster_state *s, struct digibooster_channel *ch, struct digibooster_track_line *tl) {
	uint8_t old_period = 1;

	if(!s->pause_enabled && !ch->on_off_channel && tl->period != 0) {
		if(tl->effect == DB_FX_GLISSANDO) {
			ch->glissando_new_period = 0;
		}
		ch->vibrato_period = 0;
		ch->off_enable = 1;
		ch->original_period = digibooster_find_period(s, ch, tl);
		ch->last_round_info.period = ch->original_period;
		old_period = 0;
	}

	uint8_t sample_number = ch->last_round_info.sample_number;
	uint8_t effect = DB_FX_ARPEGGIO;
	uint8_t effect_arg = 0;
	uint8_t early_return = 0;

	if(tl->sample_number != 0 || tl->effect != DB_FX_ARPEGGIO || tl->effect_arg != 0) {
		effect = tl->effect;
		effect_arg = tl->effect_arg;

		if(tl->sample_number != 0) {
			uint8_t number = (sample_number == 0) ? ch->old_sample_number : sample_number;
			if(old_period && number != 0) {
				ch->volume = s->samples[number - 1].volume;
			} else {
				sample_number = tl->sample_number;
				if(effect == DB_FX_GLISSANDO) {
					if(!ch->is_active) {
						sample_number = 0;
						effect = DB_FX_ARPEGGIO;
						effect_arg = 0;
						early_return = 1;
					}
				} else {
					ch->sample_data = s->samples[sample_number - 1].sample_data;
					ch->start_offset = 0;
				}
				if(!early_return) {
					ch->volume = s->samples[sample_number - 1].volume;
					ch->old_sample_number = sample_number;
					ch->backward_enabled = 0;
					early_return = 1;
				}
			}
		}
	}

	if(!early_return && tl->period != 0) {
		sample_number = ch->old_sample_number;
		if(effect == DB_FX_GLISSANDO || effect == DB_FX_GLISSANDO_VOLUMESLIDE) {
			if(!ch->is_active) {
				sample_number = 0;
				effect = DB_FX_ARPEGGIO;
				effect_arg = 0;
			}
		} else {
			if(sample_number == 0) {
				ch->sample_data = 0;
				ch->start_offset = 0;
			} else {
				ch->sample_data = s->samples[sample_number - 1].sample_data;
				ch->start_offset = 0;
			}
		}
	}

	ch->last_round_info.sample_number = sample_number;
	ch->last_round_info.effect = effect;
	ch->last_round_info.effect_arg = effect_arg;
}

// [=]===^=[ digibooster_do_trigger_effects ]=====================================================[=]
static void digibooster_do_trigger_effects(struct digibooster_state *s, struct digibooster_channel *ch) {
	if(ch->last_round_info.sample_number == 0 || s->samples[ch->last_round_info.sample_number - 1].sample_data == 0) {
		ch->last_round_info.period = 0;
		ch->last_round_info.sample_number = 0;
	}
	digibooster_effect_commands2(s, ch);
	if(ch->on_off_channel) {
		ch->last_round_info.period = 0;
		ch->last_round_info.sample_number = 0;
		ch->last_round_info.effect = DB_FX_ARPEGGIO;
		ch->last_round_info.effect_arg = 0;
	} else {
		if(ch->last_round_info.effect == DB_FX_EXTRA && ch->last_round_info.effect_arg == DB_EX_STOP_PLAYING) {
			ch->off_enable = 1;
			ch->last_round_info.period = 0;
			ch->last_round_info.sample_number = 0;
			ch->last_round_info.effect = DB_FX_ARPEGGIO;
			ch->last_round_info.effect_arg = 0;
		}
	}
}

// [=]===^=[ digibooster_parse_voice ]============================================================[=]
static void digibooster_parse_voice(struct digibooster_state *s, struct digibooster_channel *ch, struct digibooster_track_line *tl) {
	ch->volume = ch->old_volume;

	if(s->tempo != 0 && s->count >= s->tempo) {
		digibooster_parse_row_data(s, ch, tl);
		digibooster_do_trigger_effects(s, ch);
	}

	if((s->tempo - 1) == s->count) {
		uint8_t effect = ch->last_round_info.effect;
		if(effect == DB_FX_GLISSANDO_VOLUMESLIDE) {
			ch->last_round_info.effect = DB_FX_GLISSANDO;
			ch->last_round_info.effect_arg = 0;
		} else {
			if(effect == DB_FX_EXTRA) {
				uint8_t ex = (uint8_t)(ch->last_round_info.effect_arg & 0xf0);
				if(ex != DB_EX_CUT_SAMPLE && ex != DB_EX_RETRACE) {
					ch->last_round_info.effect = DB_FX_ARPEGGIO;
					ch->last_round_info.effect_arg = 0;
				}
			} else if(effect != DB_FX_ROBOT && effect != DB_FX_GLISSANDO && effect != DB_FX_VIBRATO && effect != DB_FX_ARPEGGIO) {
				ch->last_round_info.effect = DB_FX_ARPEGGIO;
				ch->last_round_info.effect_arg = 0;
			}
		}
	}

	digibooster_test_period(ch);
	digibooster_effect_commands(s, ch);
	digibooster_test_period(ch);

	ch->glissando_old_period = ch->last_round_info.period;
	ch->old_volume = ch->volume;

	if(ch->last_round_info.period == 0) {
		if(ch->main_period != 0) {
			ch->main_period = -1;
		}
	} else {
		ch->main_period = (int16_t)ch->last_round_info.period;
		ch->main_volume = ch->volume;

		if(ch->backward_enabled == 0) {
			if(ch->last_round_info.effect == DB_FX_EXTRA && (ch->last_round_info.effect_arg & 0xf0) == DB_EX_BACKWARD_PLAY) {
				ch->backward_enabled = (uint8_t)(((ch->last_round_info.effect_arg & 0x0f) == 0) ? 1 : 2);
			} else {
				if(ch->last_round_info.effect == DB_FX_ROBOT) {
					// Note: chan_idx not directly available here; resolved in caller via channel pointer arithmetic.
					int32_t chan_idx = (int32_t)(ch - s->channels);
					digibooster_robot_effect(s, ch, chan_idx, ch->last_round_info.effect_arg);
				} else {
					if(ch->robot_enable) {
						ch->off_enable = 1;
						ch->robot_enable = 0;
					}
				}
			}
		}
	}
}

// <=4 channel modules use the real hardware path; 5-8 channel modules
// route every channel through a soft voice (base = chan/2). Single point.
// [=]===^=[ dgb_is_soft ]========================================================================[=]
static int32_t dgb_is_soft(struct digibooster_state *s) {
	return s->number_of_channels > 4;
}

// [=]===^=[ dgb_pch_play_sample ]================================================================[=]
static void dgb_pch_play_sample(struct digibooster_state *s, int32_t ci, int8_t *sample, uint32_t length) {
	if(!dgb_is_soft(s)) {
		paula_play_sample(&s->paula, ci, sample, length);
		return;
	}
	struct digibooster_softvoice *v = &s->softv[ci];
	v->sample = sample;
	v->length = length;
	v->loop_start = 0;
	v->loop_length = 0;
	v->pos = 0;
	v->step_acc = 0;
	v->has_pending = 0;
	v->pending_sample = 0;
	v->active = (sample != 0) && (length > 0);
}

// [=]===^=[ dgb_pch_queue_sample ]===============================================================[=]
static void dgb_pch_queue_sample(struct digibooster_state *s, int32_t ci, int8_t *sample, uint32_t start_offset, uint32_t length) {
	if(!dgb_is_soft(s)) {
		paula_queue_sample(&s->paula, ci, sample, start_offset, length);
		return;
	}
	struct digibooster_softvoice *v = &s->softv[ci];
	if(!v->active && sample != 0 && length > 0) {
		v->sample = sample;
		v->pos = start_offset;
		v->length = start_offset + length;
		v->step_acc = 0;
		v->has_pending = 0;
		v->pending_sample = 0;
		v->active = 1;
		return;
	}
	v->pending_sample = sample;
	v->pending_pos = start_offset;
	v->pending_length = start_offset + length;
	v->has_pending = (sample != 0) && (length > 0);
}

// [=]===^=[ dgb_pch_set_loop ]===================================================================[=]
static void dgb_pch_set_loop(struct digibooster_state *s, int32_t ci, uint32_t start, uint32_t length) {
	if(!dgb_is_soft(s)) {
		paula_set_loop(&s->paula, ci, start, length);
		return;
	}
	struct digibooster_softvoice *v = &s->softv[ci];
	v->loop_start = start;
	v->loop_length = length;
}

// [=]===^=[ dgb_pch_set_period ]=================================================================[=]
static void dgb_pch_set_period(struct digibooster_state *s, int32_t ci, uint16_t period) {
	if(!dgb_is_soft(s)) {
		paula_set_period(&s->paula, ci, period);
		return;
	}
	s->softv[ci].period = period;
}

// [=]===^=[ dgb_pch_set_volume ]=================================================================[=]
static void dgb_pch_set_volume(struct digibooster_state *s, int32_t ci, uint16_t volume) {
	if(!dgb_is_soft(s)) {
		paula_set_volume(&s->paula, ci, volume);
		return;
	}
	if(volume > 64) {
		volume = 64;
	}
	s->softv[ci].volume = volume;
}

// [=]===^=[ dgb_pch_mute ]=======================================================================[=]
static void dgb_pch_mute(struct digibooster_state *s, int32_t ci) {
	if(!dgb_is_soft(s)) {
		paula_mute(&s->paula, ci);
		return;
	}
	s->softv[ci].active = 0;
}

// [=]===^=[ dgb_softvoice_advance ]==============================================================[=]
static void dgb_softvoice_advance(struct digibooster_softvoice *v) {
	uint32_t np = v->pos + 1;
	if(np >= v->length) {
		if(v->has_pending) {
			v->sample = v->pending_sample;
			np = v->pending_pos;
			v->length = v->pending_length;
			v->has_pending = 0;
			v->pending_sample = 0;
		} else if(v->loop_length > 0) {
			uint32_t over = np - v->length;
			np = v->loop_start + (over % v->loop_length);
			v->length = v->loop_start + v->loop_length;
		} else {
			v->active = 0;
			return;
		}
	}
	v->pos = np;
}

// [=]===^=[ digibooster_softmix_refill ]=========================================================[=]
// Regenerate the four Split base buffers (base = channel/2; the <=2
// channels on a base summed without pre-attenuation) and arm Paula 0..3.
static void digibooster_softmix_refill(struct digibooster_state *s) {
	int32_t cp = s->softmix_period;
	uint64_t n64 = ((uint64_t)s->paula.clock * (uint64_t)s->paula.samples_per_tick)
		/ ((uint64_t)cp * (uint64_t)s->paula.sample_rate);
	uint32_t n = (uint32_t)n64 + 2;
	if(n > s->softmix_cap) {
		uint32_t nc = s->softmix_cap ? s->softmix_cap : 1024;
		while(nc < n) {
			nc <<= 1;
		}
		int8_t *nb = (int8_t *)realloc(s->softmix_buf, (size_t)nc * 4);
		if(!nb) {
			return;
		}
		s->softmix_buf = nb;
		s->softmix_cap = nc;
	}
	for(int32_t i = 0; i < DIGIBOOSTER_MAX_CHANNELS; ++i) {
		struct digibooster_softvoice *v = &s->softv[i];
		v->step_q = (v->period != 0)
			? (((uint64_t)(uint32_t)cp << 32) / (uint64_t)v->period)
			: 0;
	}
	for(int32_t b = 0; b < 4; ++b) {
		int8_t *buf = s->softmix_buf + (size_t)b * s->softmix_cap;
		for(uint32_t k = 0; k < n; ++k) {
			int32_t sam = 0;
			for(int32_t i = 2 * b; i < 2 * b + 2 && i < (int32_t)s->number_of_channels; ++i) {
				struct digibooster_softvoice *v = &s->softv[i];
				if(!v->active || v->sample == 0 || v->step_q == 0) {
					continue;
				}
				// DigiBooster's software mixer applies a ~3/4 master gain
				// (derived by A/B vs UADE: the raw sum runs ~1.35x hot at
				// a consistent ratio across modules).
				sam += ((int32_t)v->sample[v->pos] * (int32_t)v->volume * 3) / 256;
				v->step_acc += v->step_q;
				while(v->step_acc >= ((uint64_t)1 << 32)) {
					v->step_acc -= ((uint64_t)1 << 32);
					dgb_softvoice_advance(v);
					if(!v->active) {
						break;
					}
				}
			}
			if(sam > 127) {
				sam = 127;
			}
			if(sam < -128) {
				sam = -128;
			}
			buf[k] = (int8_t)sam;
		}
		paula_play_sample(&s->paula, b, buf, n);
		paula_set_loop(&s->paula, b, 0, n);
		paula_set_period(&s->paula, b, (uint16_t)cp);
		paula_set_volume(&s->paula, b, 64);
	}
}

// [=]===^=[ digibooster_play_voice ]=============================================================[=]
static void digibooster_play_voice(struct digibooster_state *s, struct digibooster_channel *ch, int32_t chan_idx) {
	if(ch->main_period != 0) {
		uint8_t retrig = 0;
		if(ch->off_enable) {
			ch->off_enable = 0;
			retrig = 1;
			if(ch->play_pointer) {
				if(ch->last_round_info.effect == DB_FX_GLISSANDO || ch->last_round_info.effect == DB_FX_GLISSANDO_VOLUMESLIDE) {
					retrig = 0;
				}
			}
		}

		if(ch->main_period == -1) {
			dgb_pch_mute(s, chan_idx);
			ch->play_pointer = 1;
			ch->main_period = 0;
		} else {
			dgb_pch_set_period(s, chan_idx, (uint16_t)ch->main_period);
			uint8_t vol = ch->main_volume;
			if(vol > 64) {
				vol = 64;
			}
			dgb_pch_set_volume(s, chan_idx, vol);

			if(ch->robot_enable) {
				int8_t *buf = ch->robot_buffers[0];
				uint32_t play_len = (uint32_t)ch->robot_bytes_to_play;
				if(retrig) {
					dgb_pch_play_sample(s, chan_idx, buf, play_len);
				} else {
					dgb_pch_queue_sample(s, chan_idx, buf, 0, play_len);
				}
				dgb_pch_set_loop(s, chan_idx, 0, play_len);

				int8_t *tmp = ch->robot_buffers[0];
				ch->robot_buffers[0] = ch->robot_buffers[1];
				ch->robot_buffers[1] = tmp;
			} else {
				if(retrig) {
					struct digibooster_sample *sm = &s->samples[ch->old_sample_number - 1];
					if(ch->backward_enabled != 0) {
						// Backwards playback would need a Paula extension; play forward as a fallback.
						dgb_pch_play_sample(s, chan_idx, ch->sample_data, sm->length);
						if(ch->backward_enabled == 2 && sm->loop_length > 0) {
							dgb_pch_set_loop(s, chan_idx, sm->loop_start, sm->loop_length);
						} else {
							dgb_pch_set_loop(s, chan_idx, 0, 0);
						}
					} else {
						uint32_t length = sm->length - ch->start_offset;
						if(length > 0 && ch->sample_data) {
							dgb_pch_play_sample(s, chan_idx, ch->sample_data + ch->start_offset, length);
							if(sm->loop_length > 0) {
								dgb_pch_set_loop(s, chan_idx, sm->loop_start, sm->loop_length);
							} else {
								dgb_pch_set_loop(s, chan_idx, 0, 0);
							}
						} else {
							if(sm->loop_length > 0 && ch->sample_data) {
								dgb_pch_play_sample(s, chan_idx, ch->sample_data + sm->loop_start, sm->loop_length);
								dgb_pch_set_loop(s, chan_idx, 0, sm->loop_length);
							} else {
								dgb_pch_mute(s, chan_idx);
							}
						}
					}
				}
			}
			ch->play_pointer = 1;
		}
	}
	ch->is_active = s->paula.ch[chan_idx].active;
}

// [=]===^=[ digibooster_tick ]===================================================================[=]
static void digibooster_tick(struct digibooster_state *s) {
	if(s->count >= s->tempo) {
		if(s->tempo != 0 && s->pattern_position == 64) {
			s->pattern_position = 0;
			s->song_position++;
		}

		if(s->song_position >= s->song_length) {
			s->song_position = 0;
			s->pattern_position = 0;
		}

		if(s->tempo == 0) {
			s->song_position = 0;
			s->pattern_position = 0;
			s->tempo = 6;
			for(int32_t i = 0; i < s->number_of_channels; ++i) {
				s->channels[i].loop_pattern_position = 0;
				s->channels[i].loop_song_position = 0;
				s->channels[i].loop_how_many = 0;
			}
		}

		struct digibooster_pattern *pat = &s->patterns[s->orders[s->song_position]];
		for(int32_t i = 0; i < 8; ++i) {
			s->current_row[i] = pat->rows[i][s->pattern_position];
		}
	}

	for(int32_t i = 0; i < s->number_of_channels; ++i) {
		digibooster_play_voice(s, &s->channels[i], i);
	}

	for(int32_t i = 0; i < s->number_of_channels; ++i) {
		digibooster_parse_voice(s, &s->channels[i], &s->current_row[i]);
	}

	if(s->pause_vbl > 0) {
		s->pause_enabled = 1;
		s->pause_vbl--;
	}

	if(s->count >= s->tempo) {
		s->count = 0;
		if(s->pause_vbl == 0) {
			s->pattern_position++;
			s->pause_enabled = 0;
		}
	}

	s->count++;
	s->end_reached = 0;
}

// [=]===^=[ digibooster_cleanup ]================================================================[=]
static void digibooster_cleanup(struct digibooster_state *s) {
	if(s->patterns) {
		free(s->patterns);
		s->patterns = 0;
	}
	for(int32_t i = 0; i < DIGIBOOSTER_MAX_CHANNELS; ++i) {
		if(s->robot_storage[i][0]) {
			free(s->robot_storage[i][0]);
			s->robot_storage[i][0] = 0;
		}
		if(s->robot_storage[i][1]) {
			free(s->robot_storage[i][1]);
			s->robot_storage[i][1] = 0;
		}
	}
	free(s->softmix_buf);
	s->softmix_buf = 0;
}

// [=]===^=[ digibooster_init ]===================================================================[=]
static struct digibooster_state *digibooster_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 1572 || sample_rate < 8000) {
		return 0;
	}
	if(!digibooster_identify((uint8_t *)data, len)) {
		return 0;
	}

	struct digibooster_state *s = (struct digibooster_state *)calloc(1, sizeof(struct digibooster_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;

	if(!digibooster_load(s)) {
		digibooster_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, DIGIBOOSTER_BASE_TICK_HZ);
	s->softmix_period = DIGIBOOSTER_MIX_PERIOD;
	digibooster_initialize_sound(s);
	if(s->number_of_channels > 4) {
		digibooster_softmix_refill(s);
	}
	return s;
}

// [=]===^=[ digibooster_free ]===================================================================[=]
static void digibooster_free(struct digibooster_state *s) {
	if(!s) {
		return;
	}
	digibooster_cleanup(s);
	free(s);
}

// [=]===^=[ digibooster_get_audio ]==============================================================[=]
static void digibooster_get_audio(struct digibooster_state *s, float *output, int32_t frames) {
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
			digibooster_tick(s);
			if(s->number_of_channels > 4) {
				digibooster_softmix_refill(s);
			}
		}
	}
}

// [=]===^=[ digibooster_api_init ]===============================================================[=]
static void *digibooster_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return digibooster_init(data, len, sample_rate);
}

// [=]===^=[ digibooster_api_free ]===============================================================[=]
static void digibooster_api_free(void *state) {
	digibooster_free((struct digibooster_state *)state);
}

// [=]===^=[ digibooster_api_get_audio ]==========================================================[=]
static void digibooster_api_get_audio(void *state, float *output, int32_t frames) {
	digibooster_get_audio((struct digibooster_state *)state, output, frames);
}

static const char *digibooster_extensions[] = { "digi", 0 };

static struct player_api digibooster_api = {
	"DigiBooster 1.x",
	digibooster_extensions,
	digibooster_api_init,
	digibooster_api_free,
	digibooster_api_get_audio,
	0,
};
