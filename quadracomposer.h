// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// QuadraComposer (EMOD) replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct quadracomposer_state *quadracomposer_init(void *data, uint32_t len, int32_t sample_rate);
//   void quadracomposer_free(struct quadracomposer_state *s);
//   void quadracomposer_get_audio(struct quadracomposer_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define QC_TICK_HZ       50
#define QC_NUM_CHANNELS  4
#define QC_MAX_SAMPLES   256
#define QC_MAX_PATTERNS  256

// Effect numbers (high nibble of effect byte after masking)
enum {
	QC_EFF_ARPEGGIO                       = 0x0,
	QC_EFF_SLIDE_UP                       = 0x1,
	QC_EFF_SLIDE_DOWN                     = 0x2,
	QC_EFF_TONE_PORTAMENTO                = 0x3,
	QC_EFF_VIBRATO                        = 0x4,
	QC_EFF_TONE_PORTAMENTO_AND_VOL_SLIDE  = 0x5,
	QC_EFF_VIBRATO_AND_VOL_SLIDE          = 0x6,
	QC_EFF_TREMOLO                        = 0x7,
	QC_EFF_SET_SAMPLE_OFFSET              = 0x9,
	QC_EFF_VOLUME_SLIDE                   = 0xa,
	QC_EFF_POSITION_JUMP                  = 0xb,
	QC_EFF_SET_VOLUME                     = 0xc,
	QC_EFF_PATTERN_BREAK                  = 0xd,
	QC_EFF_EXTRA                          = 0xe,
	QC_EFF_SET_SPEED                      = 0xf,
};

// Extra effect numbers (high nibble of effect arg)
enum {
	QC_EXT_SET_FILTER             = 0x00,
	QC_EXT_FINE_SLIDE_UP          = 0x10,
	QC_EXT_FINE_SLIDE_DOWN        = 0x20,
	QC_EXT_SET_GLISSANDO          = 0x30,
	QC_EXT_SET_VIBRATO_WAVEFORM   = 0x40,
	QC_EXT_SET_FINE_TUNE          = 0x50,
	QC_EXT_PATTERN_LOOP           = 0x60,
	QC_EXT_SET_TREMOLO_WAVEFORM   = 0x70,
	QC_EXT_RETRIG_NOTE            = 0x90,
	QC_EXT_FINE_VOL_SLIDE_UP      = 0xa0,
	QC_EXT_FINE_VOL_SLIDE_DOWN    = 0xb0,
	QC_EXT_NOTE_CUT               = 0xc0,
	QC_EXT_NOTE_DELAY             = 0xd0,
	QC_EXT_PATTERN_DELAY          = 0xe0,
};

#define QC_SAMPLE_FLAG_LOOP 0x01

struct qc_track_line {
	uint8_t sample;
	int8_t  note;
	uint8_t effect;
	uint8_t effect_arg;
};

struct qc_pattern {
	uint8_t num_rows;
	uint8_t used;
	struct qc_track_line *tracks;   // [4 * (num_rows + 1)], indexed [chan * (num_rows + 1) + row]
};

struct qc_sample {
	uint8_t  used;
	uint8_t  volume;          // 0..64
	uint8_t  control_byte;    // QC_SAMPLE_FLAG_LOOP
	uint8_t  fine_tune;       // 0..15
	uint32_t length;          // bytes
	uint32_t loop_start;      // bytes
	uint32_t loop_length;     // bytes
	int8_t  *data;            // owned by player (allocated separately)
};

struct qc_channel_info {
	struct qc_track_line track_line;
	uint32_t loop;
	uint32_t loop_length;
	uint16_t period;
	uint16_t volume;
	uint32_t length;
	uint32_t start;
	int16_t  note_nr;
	uint16_t wanted_period;
	uint8_t  port_direction;
	uint8_t  vibrato_wave;
	uint8_t  glissando_control;
	uint8_t  vibrato_command;
	uint16_t vibrato_position;
	uint8_t  tremolo_wave;
	uint8_t  tremolo_command;
	uint16_t tremolo_position;
	uint8_t  sample_offset;
	uint8_t  retrig;
	uint16_t port_speed;
	uint8_t  fine_tune;
	int8_t  *sample_data;
};

struct qc_global {
	struct qc_pattern *current_pattern;
	uint16_t current_position;
	uint16_t new_position;
	uint16_t break_row;
	uint16_t new_row;
	uint16_t row_count;
	uint16_t loop_row;
	uint8_t  pattern_wait;
	uint16_t tempo;
	uint16_t speed;
	uint16_t speed_count;
	uint8_t  new_position_flag;
	uint8_t  jump_break_flag;
	uint8_t  loop_count;
	uint8_t  intro_row;
	uint8_t  set_tempo;
};

struct quadracomposer_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint8_t start_tempo;
	uint8_t number_of_samples;
	uint8_t number_of_patterns;
	uint8_t number_of_positions;
	uint8_t position_list[256];

	struct qc_sample samples[QC_MAX_SAMPLES];
	struct qc_pattern patterns[QC_MAX_PATTERNS];

	struct qc_global g;
	struct qc_channel_info channels[QC_NUM_CHANNELS];

	uint8_t end_reached;
	uint8_t amiga_filter;
};

// [=]===^=[ qc_periods ]=========================================================================[=]
// [16 finetune tables][36 notes]. Index 0..7 = finetune 0..7, index 8..15 = finetune -8..-1.
static uint16_t qc_periods[16][36] = {
	{ 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113 },
	{ 850, 802, 757, 715, 674, 637, 601, 567, 535, 505, 477, 450, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 239, 225, 213, 201, 189, 179, 169, 159, 150, 142, 134, 126, 119, 113 },
	{ 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447, 422, 398, 376, 355, 335, 316, 298, 282, 266, 251, 237, 224, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118, 112 },
	{ 838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470, 444, 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235, 222, 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118, 111 },
	{ 832, 785, 741, 699, 660, 623, 588, 555, 524, 495, 467, 441, 416, 392, 370, 350, 330, 312, 294, 278, 262, 247, 233, 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 124, 117, 110 },
	{ 826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437, 413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232, 219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116, 109 },
	{ 820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434, 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230, 217, 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115, 109 },
	{ 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431, 407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228, 216, 204, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114, 108 },
	{ 907, 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120 },
	{ 900, 850, 802, 757, 715, 675, 636, 601, 567, 535, 505, 477, 450, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 238, 225, 212, 200, 189, 179, 169, 159, 150, 142, 134, 126, 119 },
	{ 894, 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447, 422, 398, 376, 355, 335, 316, 298, 282, 266, 251, 237, 223, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118 },
	{ 887, 838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470, 444, 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235, 222, 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118 },
	{ 881, 832, 785, 741, 699, 660, 623, 588, 555, 524, 494, 467, 441, 416, 392, 370, 350, 330, 312, 294, 278, 262, 247, 233, 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 123, 117 },
	{ 875, 826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437, 413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232, 219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116 },
	{ 868, 820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434, 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230, 217, 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115 },
	{ 862, 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431, 407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228, 216, 203, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114 },
};

// [=]===^=[ qc_arp_offsets ]=====================================================================[=]
static int8_t qc_arp_offsets[3] = { -1, 0, 1 };

// [=]===^=[ qc_vibrato ]=========================================================================[=]
static int16_t qc_vibrato[3][64] = {
	{
		     0,   3211,   6392,   9511,  12539,  15446,  18204,  20787,
		 23169,  25329,  27244,  28897,  30272,  31356,  32137,  32609,
		 32767,  32609,  32137,  31356,  30272,  28897,  27244,  25329,
		 23169,  20787,  18204,  15446,  12539,   9511,   6392,   3211,
		     0,  -3211,  -6392,  -9511, -12539, -15446, -18204, -20787,
		-23169, -25329, -27244, -28897, -30272, -31356, -32137, -32609,
		-32767, -32609, -32137, -31356, -30272, -28897, -27244, -25329,
		-23169, -20787, -18204, -15446, -12539,  -9511,  -6392,  -3211
	},
	{
		 32767,  31744,  30720,  29696,  28672,  27648,  26624,  25600,
		 24576,  23552,  22528,  21504,  20480,  19456,  18432,  17408,
		 16384,  15360,  14336,  13312,  12288,  11264,  10240,   9216,
		  8192,   7168,   6144,   5120,   4096,   3072,   2048,   1024,
		     0,  -1024,  -2048,  -3072,  -4096,  -5120,  -6144,  -7168,
		 -8192,  -9216, -10240, -11264, -12288, -13312, -14336, -15360,
		-16384, -17408, -18432, -19456, -20480, -21504, -22528, -23552,
		-24576, -25600, -26624, -27648, -28672, -29696, -30720, -31744
	},
	{
		 32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,
		 32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,
		 32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,
		 32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,
		-32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767,
		-32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767,
		-32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767,
		-32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767
	}
};

// [=]===^=[ qc_read_u16_be ]=====================================================================[=]
static uint16_t qc_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ qc_read_u32_be ]=====================================================================[=]
static uint32_t qc_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ qc_check_mark ]======================================================================[=]
static int32_t qc_check_mark(uint8_t *p, const char *mark, int32_t len) {
	for(int32_t i = 0; i < len; ++i) {
		if((char)p[i] != mark[i]) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ qc_cleanup ]=========================================================================[=]
static void qc_cleanup(struct quadracomposer_state *s) {
	if(!s) {
		return;
	}
	for(int32_t i = 0; i < QC_MAX_SAMPLES; ++i) {
		if(s->samples[i].data) {
			free(s->samples[i].data);
			s->samples[i].data = 0;
		}
	}
	for(int32_t i = 0; i < QC_MAX_PATTERNS; ++i) {
		if(s->patterns[i].tracks) {
			free(s->patterns[i].tracks);
			s->patterns[i].tracks = 0;
		}
	}
}

// [=]===^=[ qc_identify ]========================================================================[=]
static int32_t qc_identify(uint8_t *data, uint32_t len) {
	if(len < 64) {
		return 0;
	}
	if(!qc_check_mark(data, "FORM", 4)) {
		return 0;
	}
	if(!qc_check_mark(data + 8, "EMODEMIC", 8)) {
		return 0;
	}
	if(qc_read_u16_be(data + 20) != 1) {
		return 0;
	}
	return 1;
}

// [=]===^=[ qc_parse_emic ]======================================================================[=]
static int32_t qc_parse_emic(struct quadracomposer_state *s, uint8_t *chunk, uint32_t chunk_size) {
	if(chunk_size < 2 + 20 + 20 + 1 + 1) {
		return 0;
	}
	uint32_t pos = 0;

	pos += 2;             // skip version
	pos += 20;            // song name
	pos += 20;            // composer

	s->start_tempo = chunk[pos++];
	s->number_of_samples = chunk[pos++];

	for(int32_t i = 0; i < s->number_of_samples; ++i) {
		if(pos + 32 > chunk_size) {
			return 0;
		}
		uint8_t number   = chunk[pos++];
		uint8_t volume   = chunk[pos++];
		uint32_t length  = (uint32_t)qc_read_u16_be(chunk + pos) * 2; pos += 2;
		pos += 20;        // name
		uint8_t ctrl     = chunk[pos++];
		uint8_t finetune = (uint8_t)(chunk[pos++] & 0x0f);
		uint32_t lstart  = (uint32_t)qc_read_u16_be(chunk + pos) * 2; pos += 2;
		uint32_t llen    = (uint32_t)qc_read_u16_be(chunk + pos) * 2; pos += 2;
		pos += 4;         // skip offset

		if(number == 0) {
			continue;
		}
		struct qc_sample *smp = &s->samples[number - 1];
		smp->used = 1;
		smp->volume = volume;
		smp->length = length;
		smp->control_byte = ctrl;
		smp->fine_tune = finetune;
		smp->loop_start = lstart;
		smp->loop_length = llen;
	}

	if(pos + 1 + 1 > chunk_size) {
		return 0;
	}
	pos += 1;  // skip
	s->number_of_patterns = chunk[pos++];

	for(int32_t i = 0; i < s->number_of_patterns; ++i) {
		if(pos + 26 > chunk_size) {
			return 0;
		}
		uint8_t number = chunk[pos++];
		uint8_t rows   = chunk[pos++];
		pos += 24;        // skip name and offset

		s->patterns[number].used = 1;
		s->patterns[number].num_rows = rows;
	}

	if(pos + 1 > chunk_size) {
		return 0;
	}
	s->number_of_positions = chunk[pos++];
	if(pos + s->number_of_positions > chunk_size) {
		return 0;
	}
	memcpy(s->position_list, chunk + pos, s->number_of_positions);
	return 1;
}

// [=]===^=[ qc_parse_patt ]======================================================================[=]
static int32_t qc_parse_patt(struct quadracomposer_state *s, uint8_t *chunk, uint32_t chunk_size) {
	uint32_t pos = 0;
	for(int32_t i = 0; i < QC_MAX_PATTERNS; ++i) {
		if(!s->patterns[i].used) {
			continue;
		}
		uint32_t total_rows = (uint32_t)s->patterns[i].num_rows + 1;
		uint32_t row_bytes  = total_rows * 4 * 4;
		if(pos + row_bytes > chunk_size) {
			return 0;
		}
		s->patterns[i].tracks = (struct qc_track_line *)calloc(total_rows * 4, sizeof(struct qc_track_line));
		if(!s->patterns[i].tracks) {
			return 0;
		}
		for(uint32_t row = 0; row < total_rows; ++row) {
			for(int32_t chan = 0; chan < 4; ++chan) {
				uint8_t b1 = chunk[pos++];
				int8_t  b2 = (int8_t)chunk[pos++];
				uint8_t b3 = chunk[pos++];
				uint8_t b4 = chunk[pos++];
				struct qc_track_line *tl = &s->patterns[i].tracks[chan * total_rows + row];
				tl->sample = b1;
				tl->note = b2;
				tl->effect = (uint8_t)(b3 & 0x0f);
				tl->effect_arg = b4;
			}
		}
	}
	return 1;
}

// [=]===^=[ qc_parse_8smp ]======================================================================[=]
static int32_t qc_parse_8smp(struct quadracomposer_state *s, uint8_t *chunk, uint32_t chunk_size) {
	uint32_t pos = 0;
	for(int32_t i = 0; i < QC_MAX_SAMPLES; ++i) {
		struct qc_sample *smp = &s->samples[i];
		if(!smp->used || smp->length == 0) {
			continue;
		}
		if(pos + smp->length > chunk_size) {
			return 0;
		}
		smp->data = (int8_t *)malloc(smp->length);
		if(!smp->data) {
			return 0;
		}
		memcpy(smp->data, chunk + pos, smp->length);
		pos += smp->length;
	}
	return 1;
}

// [=]===^=[ qc_load ]============================================================================[=]
static int32_t qc_load(struct quadracomposer_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = 12;

	while(pos + 8 <= len) {
		uint8_t *cn = data + pos;
		uint32_t csize = qc_read_u32_be(data + pos + 4);
		pos += 8;
		if(pos + csize > len) {
			return 0;
		}
		uint8_t *cd = data + pos;
		if(qc_check_mark(cn, "EMIC", 4)) {
			if(!qc_parse_emic(s, cd, csize)) {
				return 0;
			}
		} else if(qc_check_mark(cn, "PATT", 4)) {
			if(!qc_parse_patt(s, cd, csize)) {
				return 0;
			}
		} else if(qc_check_mark(cn, "8SMP", 4)) {
			if(!qc_parse_8smp(s, cd, csize)) {
				return 0;
			}
		}
		pos += csize;
		if((pos & 1) != 0) {
			pos++;
		}
	}
	return 1;
}

// [=]===^=[ qc_set_bpm_tempo ]===================================================================[=]
// Standard ProTracker BPM-to-samples_per_tick formula. NostalgicPlayer's
// SetBpmTempo is the host-side call; we replicate the equivalent here.
static void qc_set_bpm_tempo(struct quadracomposer_state *s, uint16_t bpm) {
	if(bpm == 0) {
		bpm = 125;
	}
	s->paula.samples_per_tick = (int32_t)((uint64_t)s->paula.sample_rate * 5 / ((uint32_t)bpm * 2));
}

// [=]===^=[ qc_initialize_sound ]================================================================[=]
static void qc_initialize_sound(struct quadracomposer_state *s, int32_t start_position) {
	uint8_t patt_idx = s->position_list[start_position];
	struct qc_pattern *pattern = &s->patterns[patt_idx];

	memset(&s->g, 0, sizeof(s->g));
	s->g.current_position = (uint16_t)start_position;
	s->g.current_pattern = pattern;
	s->g.break_row = pattern->num_rows;
	s->g.new_row = 0;
	s->g.row_count = 0;
	s->g.tempo = s->start_tempo;
	s->g.speed = 6;
	s->g.speed_count = 6;
	s->g.set_tempo = 1;
	s->g.intro_row = 1;

	s->end_reached = 0;
	memset(s->channels, 0, sizeof(s->channels));
}

// [=]===^=[ qc_initialize_new_position ]=========================================================[=]
static void qc_initialize_new_position(struct quadracomposer_state *s) {
	if(s->g.current_position >= s->number_of_positions) {
		s->g.current_position = 0;
		s->end_reached = 1;
	}

	struct qc_pattern *pattern = &s->patterns[s->position_list[s->g.current_position]];
	s->g.current_pattern = pattern;
	s->g.break_row = pattern->num_rows;

	s->g.row_count = s->g.new_row;
	s->g.new_row = 0;

	if(s->g.break_row < s->g.row_count) {
		s->g.row_count = s->g.break_row;
	}
}

// [=]===^=[ qc_change_tempo_if_needed ]==========================================================[=]
static void qc_change_tempo_if_needed(struct quadracomposer_state *s) {
	if(s->g.set_tempo) {
		s->g.set_tempo = 0;
		qc_set_bpm_tempo(s, s->g.tempo);
	}
}

// [=]===^=[ qc_change_speed ]====================================================================[=]
static void qc_change_speed(struct quadracomposer_state *s, uint16_t new_speed) {
	if(new_speed != s->g.speed) {
		s->g.speed = new_speed;
	}
}

// [=]===^=[ qc_set_tone_portamento ]=============================================================[=]
static void qc_set_tone_portamento(struct qc_channel_info *ci) {
	ci->wanted_period = qc_periods[ci->fine_tune][ci->note_nr];
	ci->port_direction = (ci->wanted_period > ci->period) ? 1 : 0;
}

// [=]===^=[ qc_do_arpeggio ]=====================================================================[=]
static void qc_do_arpeggio(struct quadracomposer_state *s, struct qc_channel_info *ci, int32_t chan) {
	if(ci->track_line.effect_arg != 0) {
		int8_t offset = qc_arp_offsets[s->g.speed_count % 3];
		if(offset < 0) {
			paula_set_period(&s->paula, chan, ci->period);
		} else {
			uint16_t note;
			if(offset == 0) {
				note = (uint16_t)((ci->track_line.effect_arg >> 4) + ci->note_nr);
			} else {
				note = (uint16_t)((ci->track_line.effect_arg & 0x0f) + ci->note_nr);
			}
			if(note > 35) {
				note = 35;
			}
			paula_set_period(&s->paula, chan, qc_periods[ci->fine_tune][note]);
		}
	}
}

// [=]===^=[ qc_do_slide_up ]=====================================================================[=]
static void qc_do_slide_up(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	int32_t np = (int32_t)ci->period - (int32_t)ci->track_line.effect_arg;
	if(np < 113) {
		np = 113;
	}
	ci->period = (uint16_t)np;
	paula_set_period(p, chan, ci->period);
}

// [=]===^=[ qc_do_slide_down ]===================================================================[=]
static void qc_do_slide_down(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	int32_t np = (int32_t)ci->period + (int32_t)ci->track_line.effect_arg;
	if(np > 856) {
		np = 856;
	}
	ci->period = (uint16_t)np;
	paula_set_period(p, chan, ci->period);
}

// [=]===^=[ qc_do_actual_tone_portamento ]=======================================================[=]
static void qc_do_actual_tone_portamento(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	uint16_t port_speed = ci->port_speed;

	if(ci->port_direction) {
		ci->period = (uint16_t)(ci->period + port_speed);
		if(ci->period >= ci->wanted_period) {
			ci->period = ci->wanted_period;
			ci->wanted_period = 0;
			paula_set_period(p, chan, ci->period);
			return;
		}
	} else {
		int32_t new_period = (int32_t)ci->period - (int32_t)port_speed;
		if(new_period <= ci->wanted_period) {
			ci->period = ci->wanted_period;
			ci->wanted_period = 0;
			paula_set_period(p, chan, ci->period);
			return;
		}
		ci->period = (uint16_t)new_period;
	}

	if(ci->glissando_control != 0) {
		for(int32_t i = 0; i < 36; ++i) {
			if(ci->period >= qc_periods[ci->fine_tune][i]) {
				paula_set_period(p, chan, qc_periods[ci->fine_tune][i]);
				return;
			}
		}
	} else {
		paula_set_period(p, chan, ci->period);
	}
}

// [=]===^=[ qc_do_tone_portamento ]==============================================================[=]
static void qc_do_tone_portamento(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	if(ci->wanted_period == 0) {
		return;
	}
	if(ci->track_line.effect_arg != 0) {
		ci->port_speed = ci->track_line.effect_arg;
	}
	qc_do_actual_tone_portamento(ci, p, chan);
}

// [=]===^=[ qc_do_actual_vibrato ]===============================================================[=]
static void qc_do_actual_vibrato(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	ci->vibrato_position = (uint16_t)((ci->vibrato_position + (ci->vibrato_command >> 4)) & 0x3f);
	int16_t vib_val = qc_vibrato[ci->vibrato_wave & 3][ci->vibrato_position];
	int32_t new_period = (int32_t)ci->period + (((int32_t)(ci->vibrato_command & 0x0f) * (int32_t)vib_val) >> 14);
	if(new_period > 856) {
		new_period = 856;
	} else if(new_period < 113) {
		new_period = 113;
	}
	paula_set_period(p, chan, (uint16_t)new_period);
}

// [=]===^=[ qc_do_vibrato ]======================================================================[=]
static void qc_do_vibrato(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	uint8_t arg = ci->track_line.effect_arg;
	if(arg != 0) {
		if((arg & 0x0f) != 0) {
			ci->vibrato_command = (uint8_t)((ci->vibrato_command & 0xf0) | (arg & 0x0f));
		}
		if((arg & 0xf0) != 0) {
			ci->vibrato_command = (uint8_t)((ci->vibrato_command & 0x0f) | (arg & 0xf0));
		}
	}
	qc_do_actual_vibrato(ci, p, chan);
}

// [=]===^=[ qc_do_volume_slide ]=================================================================[=]
static void qc_do_volume_slide(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	int32_t new_volume = (int32_t)ci->volume;
	uint8_t volume_speed = (uint8_t)(ci->track_line.effect_arg >> 4);
	if(volume_speed != 0) {
		new_volume += volume_speed;
		if(new_volume > 64) {
			new_volume = 64;
		}
	} else {
		new_volume -= (int32_t)(ci->track_line.effect_arg & 0x0f);
		if(new_volume < 0) {
			new_volume = 0;
		}
	}
	ci->volume = (uint16_t)new_volume;
	paula_set_volume(p, chan, ci->volume);
}

// [=]===^=[ qc_do_tone_portamento_and_volume_slide ]=============================================[=]
static void qc_do_tone_portamento_and_volume_slide(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	if(ci->wanted_period != 0) {
		qc_do_actual_tone_portamento(ci, p, chan);
	}
	qc_do_volume_slide(ci, p, chan);
}

// [=]===^=[ qc_do_vibrato_and_volume_slide ]=====================================================[=]
static void qc_do_vibrato_and_volume_slide(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	qc_do_actual_vibrato(ci, p, chan);
	qc_do_volume_slide(ci, p, chan);
}

// [=]===^=[ qc_do_tremolo ]======================================================================[=]
static void qc_do_tremolo(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	uint8_t arg = ci->track_line.effect_arg;
	if(arg != 0) {
		if((arg & 0x0f) != 0) {
			ci->tremolo_command = (uint8_t)((ci->tremolo_command & 0xf0) | (arg & 0x0f));
		}
		if((arg & 0xf0) != 0) {
			ci->tremolo_command = (uint8_t)((ci->tremolo_command & 0x0f) | (arg & 0xf0));
		}
	}
	ci->tremolo_position = (uint16_t)((ci->tremolo_position + (ci->tremolo_command >> 4)) & 0x3f);
	int16_t vib_val = qc_vibrato[ci->tremolo_wave & 3][ci->tremolo_position];
	int32_t new_volume = (int32_t)ci->volume + (((int32_t)(ci->tremolo_command & 0x0f) * (int32_t)vib_val) >> 14);
	if(new_volume > 64) {
		new_volume = 64;
	} else if(new_volume < 0) {
		new_volume = 0;
	}
	paula_set_volume(p, chan, (uint16_t)new_volume);
}

// [=]===^=[ qc_do_set_sample_offset ]============================================================[=]
static void qc_do_set_sample_offset(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	if(ci->track_line.effect_arg != 0) {
		ci->sample_offset = ci->track_line.effect_arg;
	}
	uint32_t offset = (uint32_t)ci->sample_offset * 256u * 2u;
	if(offset < ci->length) {
		ci->start = offset;
	} else {
		ci->start = ci->loop;
		ci->length = ci->loop_length;
	}

	if(ci->length > 0) {
		paula_play_sample(p, chan, ci->sample_data, 0);
		paula_queue_sample(p, chan, ci->sample_data, ci->start, ci->length - ci->start);
		paula_set_loop(p, chan, ci->loop, ci->loop_length);
	} else {
		paula_mute(p, chan);
	}
}

// [=]===^=[ qc_do_position_jump ]================================================================[=]
static void qc_do_position_jump(struct quadracomposer_state *s, struct qc_channel_info *ci) {
	s->g.new_position = ci->track_line.effect_arg;
	s->g.new_position_flag = 1;
	s->g.new_row = 0;
}

// [=]===^=[ qc_do_set_volume ]===================================================================[=]
static void qc_do_set_volume(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	ci->volume = ci->track_line.effect_arg;
	if(ci->volume > 64) {
		ci->volume = 64;
	}
	paula_set_volume(p, chan, ci->volume);
}

// [=]===^=[ qc_do_pattern_break ]================================================================[=]
static void qc_do_pattern_break(struct quadracomposer_state *s, struct qc_channel_info *ci) {
	s->g.new_position = (uint16_t)(s->g.current_position + 1);
	s->g.new_row = ci->track_line.effect_arg;
	s->g.new_position_flag = 1;
}

// [=]===^=[ qc_do_set_speed ]====================================================================[=]
static void qc_do_set_speed(struct quadracomposer_state *s, struct qc_channel_info *ci) {
	if(ci->track_line.effect_arg > 31) {
		s->g.tempo = ci->track_line.effect_arg;
		s->g.set_tempo = 1;
	} else {
		uint16_t new_speed = ci->track_line.effect_arg;
		if(new_speed == 0) {
			new_speed = 1;
		}
		qc_change_speed(s, new_speed);
		s->g.speed_count = 0;
	}
}

// [=]===^=[ qc_do_set_filter ]===================================================================[=]
static void qc_do_set_filter(struct quadracomposer_state *s, struct qc_channel_info *ci) {
	s->amiga_filter = ((ci->track_line.effect_arg & 0x01) == 0) ? 1 : 0;
}

// [=]===^=[ qc_do_fine_slide_up ]================================================================[=]
static void qc_do_fine_slide_up(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	int32_t np = (int32_t)ci->period - (int32_t)(ci->track_line.effect_arg & 0x0f);
	if(np < 113) {
		np = 113;
	}
	ci->period = (uint16_t)np;
	paula_set_period(p, chan, ci->period);
}

// [=]===^=[ qc_do_fine_slide_down ]==============================================================[=]
static void qc_do_fine_slide_down(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	int32_t np = (int32_t)ci->period + (int32_t)(ci->track_line.effect_arg & 0x0f);
	if(np > 856) {
		np = 856;
	}
	ci->period = (uint16_t)np;
	paula_set_period(p, chan, ci->period);
}

// [=]===^=[ qc_do_set_glissando ]================================================================[=]
static void qc_do_set_glissando(struct qc_channel_info *ci) {
	ci->glissando_control = (uint8_t)(ci->track_line.effect_arg & 0x0f);
}

// [=]===^=[ qc_do_set_vibrato_waveform ]=========================================================[=]
static void qc_do_set_vibrato_waveform(struct qc_channel_info *ci) {
	ci->vibrato_wave = (uint8_t)(ci->track_line.effect_arg & 0x0f);
}

// [=]===^=[ qc_do_set_fine_tune ]================================================================[=]
static void qc_do_set_fine_tune(struct qc_channel_info *ci) {
	ci->fine_tune = (uint8_t)(ci->track_line.effect_arg & 0x0f);
}

// [=]===^=[ qc_do_pattern_loop ]=================================================================[=]
static void qc_do_pattern_loop(struct quadracomposer_state *s, struct qc_channel_info *ci) {
	uint8_t arg = (uint8_t)(ci->track_line.effect_arg & 0x0f);
	if(arg == 0) {
		s->g.loop_row = s->g.row_count;
	} else {
		if(s->g.loop_count == 0) {
			s->g.loop_count = arg;
			s->g.jump_break_flag = 1;
		} else {
			s->g.loop_count--;
			if(s->g.loop_count != 0) {
				s->g.jump_break_flag = 1;
			}
		}
	}
}

// [=]===^=[ qc_do_set_tremolo_waveform ]=========================================================[=]
static void qc_do_set_tremolo_waveform(struct qc_channel_info *ci) {
	ci->tremolo_wave = (uint8_t)(ci->track_line.effect_arg & 0x0f);
}

// [=]===^=[ qc_do_retrig_note ]==================================================================[=]
static void qc_do_retrig_note(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	uint8_t arg = (uint8_t)(ci->track_line.effect_arg & 0x0f);
	ci->retrig++;
	if(ci->retrig >= arg) {
		ci->retrig = 0;
		paula_play_sample(p, chan, ci->sample_data, ci->length - ci->start);
		paula_set_loop(p, chan, ci->loop, ci->loop_length);
		paula_set_period(p, chan, ci->period);
	}
}

// [=]===^=[ qc_do_init_retrig ]==================================================================[=]
static void qc_do_init_retrig(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	ci->retrig = 0;
	qc_do_retrig_note(ci, p, chan);
}

// [=]===^=[ qc_do_fine_volume_slide_up ]=========================================================[=]
static void qc_do_fine_volume_slide_up(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	ci->volume = (uint16_t)(ci->volume + (ci->track_line.effect_arg & 0x0f));
	if(ci->volume > 64) {
		ci->volume = 64;
	}
	paula_set_volume(p, chan, ci->volume);
}

// [=]===^=[ qc_do_fine_volume_slide_down ]=======================================================[=]
static void qc_do_fine_volume_slide_down(struct qc_channel_info *ci, struct paula *p, int32_t chan) {
	int32_t new_volume = (int32_t)ci->volume - (int32_t)(ci->track_line.effect_arg & 0x0f);
	if(new_volume < 0) {
		new_volume = 0;
	}
	ci->volume = (uint16_t)new_volume;
	paula_set_volume(p, chan, ci->volume);
}

// [=]===^=[ qc_do_note_cut ]=====================================================================[=]
static void qc_do_note_cut(struct quadracomposer_state *s, struct qc_channel_info *ci, int32_t chan) {
	if((ci->track_line.effect_arg & 0x0f) <= s->g.speed_count) {
		ci->volume = 0;
		paula_set_volume_256(&s->paula, chan, 0);
	}
}

// [=]===^=[ qc_do_note_delay ]===================================================================[=]
static void qc_do_note_delay(struct quadracomposer_state *s, struct qc_channel_info *ci, int32_t chan) {
	if(ci->note_nr >= 0) {
		if((ci->track_line.effect_arg & 0x0f) == s->g.speed_count) {
			paula_play_sample(&s->paula, chan, ci->sample_data, ci->length - ci->start);
			paula_set_loop(&s->paula, chan, ci->loop, ci->loop_length);
			paula_set_period(&s->paula, chan, ci->period);
		}
	}
}

// [=]===^=[ qc_do_pattern_delay ]================================================================[=]
static void qc_do_pattern_delay(struct quadracomposer_state *s, struct qc_channel_info *ci) {
	s->g.pattern_wait = (uint8_t)(ci->track_line.effect_arg & 0x0f);
}

// [=]===^=[ qc_play_tick_extra_effect ]==========================================================[=]
static void qc_play_tick_extra_effect(struct quadracomposer_state *s, struct qc_channel_info *ci, int32_t chan) {
	uint8_t hi = (uint8_t)(ci->track_line.effect_arg & 0xf0);
	switch(hi) {
		case QC_EXT_RETRIG_NOTE: {
			qc_do_retrig_note(ci, &s->paula, chan);
			break;
		}

		case QC_EXT_NOTE_CUT: {
			qc_do_note_cut(s, ci, chan);
			break;
		}

		case QC_EXT_NOTE_DELAY: {
			qc_do_note_delay(s, ci, chan);
			break;
		}

		default: break;
	}
}

// [=]===^=[ qc_play_tick_effect ]================================================================[=]
static void qc_play_tick_effect(struct quadracomposer_state *s, struct qc_channel_info *ci, int32_t chan) {
	switch(ci->track_line.effect) {
		case QC_EFF_ARPEGGIO: {
			qc_do_arpeggio(s, ci, chan);
			break;
		}

		case QC_EFF_SLIDE_UP: {
			qc_do_slide_up(ci, &s->paula, chan);
			break;
		}

		case QC_EFF_SLIDE_DOWN: {
			qc_do_slide_down(ci, &s->paula, chan);
			break;
		}

		case QC_EFF_TONE_PORTAMENTO: {
			qc_do_tone_portamento(ci, &s->paula, chan);
			break;
		}

		case QC_EFF_VIBRATO: {
			qc_do_vibrato(ci, &s->paula, chan);
			break;
		}

		case QC_EFF_TONE_PORTAMENTO_AND_VOL_SLIDE: {
			qc_do_tone_portamento_and_volume_slide(ci, &s->paula, chan);
			break;
		}

		case QC_EFF_VIBRATO_AND_VOL_SLIDE: {
			qc_do_vibrato_and_volume_slide(ci, &s->paula, chan);
			break;
		}

		case QC_EFF_TREMOLO: {
			qc_do_tremolo(ci, &s->paula, chan);
			break;
		}

		case QC_EFF_VOLUME_SLIDE: {
			qc_do_volume_slide(ci, &s->paula, chan);
			break;
		}

		case QC_EFF_EXTRA: {
			qc_play_tick_extra_effect(s, ci, chan);
			break;
		}

		default: break;
	}
}

// [=]===^=[ qc_play_after_period_extra_effect ]==================================================[=]
static void qc_play_after_period_extra_effect(struct quadracomposer_state *s, struct qc_channel_info *ci, int32_t chan) {
	uint8_t hi = (uint8_t)(ci->track_line.effect_arg & 0xf0);
	switch(hi) {
		case QC_EXT_SET_FILTER: {
			qc_do_set_filter(s, ci);
			break;
		}

		case QC_EXT_FINE_SLIDE_UP: {
			qc_do_fine_slide_up(ci, &s->paula, chan);
			break;
		}

		case QC_EXT_FINE_SLIDE_DOWN: {
			qc_do_fine_slide_down(ci, &s->paula, chan);
			break;
		}

		case QC_EXT_SET_GLISSANDO: {
			qc_do_set_glissando(ci);
			break;
		}

		case QC_EXT_SET_VIBRATO_WAVEFORM: {
			qc_do_set_vibrato_waveform(ci);
			break;
		}

		case QC_EXT_SET_FINE_TUNE: {
			qc_do_set_fine_tune(ci);
			break;
		}

		case QC_EXT_PATTERN_LOOP: {
			qc_do_pattern_loop(s, ci);
			break;
		}

		case QC_EXT_SET_TREMOLO_WAVEFORM: {
			qc_do_set_tremolo_waveform(ci);
			break;
		}

		case QC_EXT_RETRIG_NOTE: {
			qc_do_init_retrig(ci, &s->paula, chan);
			break;
		}

		case QC_EXT_FINE_VOL_SLIDE_UP: {
			qc_do_fine_volume_slide_up(ci, &s->paula, chan);
			break;
		}

		case QC_EXT_FINE_VOL_SLIDE_DOWN: {
			qc_do_fine_volume_slide_down(ci, &s->paula, chan);
			break;
		}

		case QC_EXT_NOTE_CUT: {
			qc_do_note_cut(s, ci, chan);
			break;
		}

		case QC_EXT_NOTE_DELAY: {
			qc_do_note_delay(s, ci, chan);
			break;
		}

		case QC_EXT_PATTERN_DELAY: {
			qc_do_pattern_delay(s, ci);
			break;
		}

		default: break;
	}
}

// [=]===^=[ qc_play_after_period_effect ]========================================================[=]
static void qc_play_after_period_effect(struct quadracomposer_state *s, struct qc_channel_info *ci, int32_t chan) {
	switch(ci->track_line.effect) {
		case QC_EFF_ARPEGGIO: {
			qc_do_arpeggio(s, ci, chan);
			break;
		}

		case QC_EFF_SET_SAMPLE_OFFSET: {
			qc_do_set_sample_offset(ci, &s->paula, chan);
			break;
		}

		case QC_EFF_POSITION_JUMP: {
			qc_do_position_jump(s, ci);
			break;
		}

		case QC_EFF_SET_VOLUME: {
			qc_do_set_volume(ci, &s->paula, chan);
			break;
		}

		case QC_EFF_PATTERN_BREAK: {
			qc_do_pattern_break(s, ci);
			break;
		}

		case QC_EFF_EXTRA: {
			qc_play_after_period_extra_effect(s, ci, chan);
			break;
		}

		case QC_EFF_SET_SPEED: {
			qc_do_set_speed(s, ci);
			break;
		}

		default: break;
	}
}

// [=]===^=[ qc_play_note ]=======================================================================[=]
static void qc_play_note(struct quadracomposer_state *s, int32_t chan, struct qc_track_line *tl) {
	struct qc_channel_info *ci = &s->channels[chan];
	ci->track_line = *tl;

	if(tl->sample != 0) {
		{
			struct qc_sample *smp = &s->samples[tl->sample - 1];
			if(smp->used) {
				ci->volume = smp->volume;
				ci->length = smp->length;
				ci->fine_tune = smp->fine_tune;
				ci->sample_data = smp->data;
				ci->start = 0;

				paula_set_volume(&s->paula, chan, ci->volume);

				if((smp->control_byte & QC_SAMPLE_FLAG_LOOP) != 0) {
					ci->loop = smp->loop_start;
					ci->length = smp->loop_start + smp->loop_length;
					ci->loop_length = smp->loop_length;
				} else {
					ci->loop = 0;
					ci->loop_length = 0;
				}
			}
		}
	}

	if(tl->note >= 0) {
		ci->note_nr = tl->note;

		if((tl->effect == QC_EFF_EXTRA) && ((tl->effect_arg & 0xf0) == QC_EXT_SET_FINE_TUNE)) {
			ci->fine_tune = (uint8_t)(tl->effect_arg & 0x0f);
		} else if((tl->effect == QC_EFF_TONE_PORTAMENTO) || (tl->effect == QC_EFF_TONE_PORTAMENTO_AND_VOL_SLIDE)) {
			qc_set_tone_portamento(ci);
			return;
		}

		ci->period = qc_periods[ci->fine_tune][ci->note_nr];

		if((tl->effect == QC_EFF_EXTRA) && ((tl->effect_arg & 0xf0) == QC_EXT_NOTE_DELAY)) {
			qc_do_note_delay(s, ci, chan);
			return;
		}

		uint32_t play_len = (ci->length > ci->start) ? (ci->length - ci->start) : 0;
		paula_play_sample(&s->paula, chan, ci->sample_data, play_len);
		paula_set_loop(&s->paula, chan, ci->loop, ci->loop_length);
		paula_set_period(&s->paula, chan, ci->period);
	}

	qc_play_after_period_effect(s, ci, chan);
}

// [=]===^=[ qc_get_notes ]=======================================================================[=]
static void qc_get_notes(struct quadracomposer_state *s) {
	if(!s->g.intro_row) {
		qc_change_tempo_if_needed(s);

		if(s->g.new_position_flag) {
			s->g.new_position_flag = 0;
			s->g.current_position = s->g.new_position;
			qc_initialize_new_position(s);
		} else {
			if(s->g.jump_break_flag) {
				s->g.jump_break_flag = 0;
				if(s->g.loop_row <= s->g.break_row) {
					s->g.row_count = s->g.loop_row;
				}
			} else {
				s->g.row_count++;
				if(s->g.row_count > s->g.break_row) {
					s->g.current_position++;
					qc_initialize_new_position(s);
				}
			}
		}
	}

	s->g.intro_row = 0;
	s->g.speed_count = 0;

	struct qc_pattern *pattern = s->g.current_pattern;
	uint32_t total_rows = (uint32_t)pattern->num_rows + 1;
	if(s->g.row_count >= total_rows || pattern->tracks == 0) {
		return;
	}
	for(int32_t i = 0; i < QC_NUM_CHANNELS; ++i) {
		struct qc_track_line *tl = &pattern->tracks[i * total_rows + s->g.row_count];
		qc_play_note(s, i, tl);
	}
}

// [=]===^=[ qc_run_tick_effects ]================================================================[=]
static void qc_run_tick_effects(struct quadracomposer_state *s) {
	for(int32_t i = 0; i < QC_NUM_CHANNELS; ++i) {
		qc_play_tick_effect(s, &s->channels[i], i);
	}
}

// [=]===^=[ qc_play ]============================================================================[=]
static void qc_play(struct quadracomposer_state *s) {
	qc_change_tempo_if_needed(s);

	s->g.speed_count++;
	if(s->g.speed_count < s->g.speed) {
		qc_run_tick_effects(s);
	} else {
		if(s->g.pattern_wait != 0) {
			s->g.pattern_wait--;
			s->g.speed_count = 0;
			qc_run_tick_effects(s);
		} else {
			qc_get_notes(s);
		}
	}

	if(s->end_reached) {
		s->end_reached = 0;
	}
}

// [=]===^=[ quadracomposer_init ]================================================================[=]
static struct quadracomposer_state *quadracomposer_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 64 || sample_rate < 8000) {
		return 0;
	}
	if(!qc_identify((uint8_t *)data, len)) {
		return 0;
	}

	struct quadracomposer_state *s = (struct quadracomposer_state *)calloc(1, sizeof(struct quadracomposer_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;

	if(!qc_load(s)) {
		qc_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, QC_TICK_HZ);
	qc_initialize_sound(s, 0);
	qc_set_bpm_tempo(s, s->g.tempo ? s->g.tempo : 125);
	return s;
}

// [=]===^=[ quadracomposer_free ]================================================================[=]
static void quadracomposer_free(struct quadracomposer_state *s) {
	if(!s) {
		return;
	}
	qc_cleanup(s);
	free(s);
}

// [=]===^=[ quadracomposer_get_audio ]===========================================================[=]
static void quadracomposer_get_audio(struct quadracomposer_state *s, int16_t *output, int32_t frames) {
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
			qc_play(s);
		}
	}
}

// [=]===^=[ quadracomposer_api_init ]============================================================[=]
static void *quadracomposer_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return quadracomposer_init(data, len, sample_rate);
}

// [=]===^=[ quadracomposer_api_free ]============================================================[=]
static void quadracomposer_api_free(void *state) {
	quadracomposer_free((struct quadracomposer_state *)state);
}

// [=]===^=[ quadracomposer_api_get_audio ]=======================================================[=]
static void quadracomposer_api_get_audio(void *state, int16_t *output, int32_t frames) {
	quadracomposer_get_audio((struct quadracomposer_state *)state, output, frames);
}

static const char *quadracomposer_extensions[] = { "emod", 0 };

static struct player_api quadracomposer_api = {
	"QuadraComposer",
	quadracomposer_extensions,
	quadracomposer_api_init,
	quadracomposer_api_free,
	quadracomposer_api_get_audio,
	0,
};
