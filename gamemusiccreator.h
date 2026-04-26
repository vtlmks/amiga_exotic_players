// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Game Music Creator replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct gamemusiccreator_state *gamemusiccreator_init(void *data, uint32_t len, int32_t sample_rate);
//   void gamemusiccreator_free(struct gamemusiccreator_state *s);
//   void gamemusiccreator_get_audio(struct gamemusiccreator_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define GMC_TICK_HZ          50
#define GMC_NUM_CHANNELS     4
#define GMC_NUM_SAMPLES      15
#define GMC_PATTERN_ROWS     64
#define GMC_PATTERN_BYTES    1024
#define GMC_HEADER_SIZE      444

enum {
	GMC_EFFECT_NONE          = 0,
	GMC_EFFECT_SLIDE_UP      = 1,
	GMC_EFFECT_SLIDE_DOWN    = 2,
	GMC_EFFECT_SET_VOLUME    = 3,
	GMC_EFFECT_PATTERN_BREAK = 4,
	GMC_EFFECT_POSITION_JUMP = 5,
	GMC_EFFECT_ENABLE_FILTER = 6,
	GMC_EFFECT_DISABLE_FILTER= 7,
	GMC_EFFECT_SET_SPEED     = 8,
};

struct gmc_track_line {
	uint16_t period;
	uint8_t sample;
	uint8_t effect;
	uint8_t effect_arg;
};

struct gmc_pattern {
	struct gmc_track_line tracks[GMC_NUM_CHANNELS][GMC_PATTERN_ROWS];
};

struct gmc_sample {
	int8_t *data;
	uint16_t length;
	uint16_t loop_start;
	uint16_t loop_length;
	uint16_t volume;
};

struct gmc_channel_info {
	int32_t slide;
	uint16_t period;
	uint16_t volume;
};

struct gamemusiccreator_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	struct gmc_sample samples[GMC_NUM_SAMPLES];

	struct gmc_pattern *patterns;
	uint32_t num_patterns;

	uint8_t *position_list;
	int32_t number_of_positions;

	struct gmc_channel_info channels[GMC_NUM_CHANNELS];

	uint16_t song_speed;
	uint16_t song_step;
	uint16_t pattern_count;
	int16_t current_position;
	uint8_t current_pattern;

	uint8_t end_reached;
};

// [=]===^=[ gmc_periods ]========================================================================[=]
static int16_t gmc_periods[36] = {
	856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
	428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
	214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113
};

// [=]===^=[ gmc_read_u16_be ]====================================================================[=]
static uint16_t gmc_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ gmc_read_u32_be ]====================================================================[=]
static uint32_t gmc_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ gmc_read_i32_be ]====================================================================[=]
static int32_t gmc_read_i32_be(uint8_t *p) {
	return (int32_t)gmc_read_u32_be(p);
}

// [=]===^=[ gmc_period_in_table ]================================================================[=]
static int32_t gmc_period_in_table(uint32_t period) {
	for(int32_t i = 0; i < 36; ++i) {
		if((uint32_t)(uint16_t)gmc_periods[i] == period) {
			return 1;
		}
	}
	return 0;
}

// [=]===^=[ gmc_test_module ]====================================================================[=]
static int32_t gmc_test_module(uint8_t *buf, uint32_t len) {
	if(len < (uint32_t)GMC_HEADER_SIZE) {
		return 0;
	}

	uint32_t position_list_length = gmc_read_u32_be(buf + 240);
	if(position_list_length > 100) {
		return 0;
	}

	uint32_t temp = gmc_read_u32_be(buf + 244);
	temp |= gmc_read_u32_be(buf + 248);
	temp |= gmc_read_u32_be(buf + 252);
	temp |= gmc_read_u32_be(buf + 256);
	temp &= 0x03ff03ffu;
	if(temp != 0) {
		return 0;
	}

	uint32_t sample_size = 0;
	uint32_t pos = 0;

	for(int32_t i = 0; i < GMC_NUM_SAMPLES; ++i) {
		uint32_t start = gmc_read_u32_be(buf + pos);
		if(start > 0xf8000) {
			return 0;
		}
		pos += 4;

		uint16_t slen = gmc_read_u16_be(buf + pos);
		if(slen >= 0x8000) {
			return 0;
		}
		if((slen != 0) && (start == 0)) {
			return 0;
		}
		sample_size += (uint32_t)slen * 2u;
		pos += 2;

		uint16_t vol = gmc_read_u16_be(buf + pos);
		if(vol > 64) {
			return 0;
		}
		pos += 2;

		uint32_t loop_addr = gmc_read_u32_be(buf + pos);
		pos += 4;
		if(loop_addr != 0) {
			if((loop_addr > 0xf8000) || (start == 0) || (slen == 0)) {
				return 0;
			}
		}

		pos += 4;
	}

	uint16_t max_pos = 0;
	for(uint32_t i = 0; i < position_list_length; ++i) {
		uint16_t entry = gmc_read_u16_be(buf + 244 + i * 2);
		if((entry % 1024) != 0) {
			return 0;
		}
		if((entry < 0x8000) && (entry > max_pos)) {
			max_pos = entry;
		}
	}

	uint16_t pattern_num = (uint16_t)((max_pos / 1024) + 1);

	if(((uint32_t)GMC_HEADER_SIZE + (uint32_t)pattern_num * (uint32_t)GMC_PATTERN_BYTES + sample_size) > (len + 256)) {
		return 0;
	}

	int32_t period_count = 0;
	for(int32_t i = 0; i < 256; ++i) {
		uint32_t v = gmc_read_u32_be(buf + GMC_HEADER_SIZE + i * 4);
		v = (v & 0x0fff0000u) >> 16;
		if((v != 0) && (v != 0xffe)) {
			if(!gmc_period_in_table(v)) {
				return 0;
			}
			period_count++;
		}
	}
	if(period_count == 0) {
		return 0;
	}

	return 1;
}

// [=]===^=[ gmc_load_samples_info ]==============================================================[=]
static int32_t gmc_load_samples_info(struct gamemusiccreator_state *s) {
	uint8_t *buf = s->module_data;
	uint32_t pos = 0;

	for(int32_t i = 0; i < GMC_NUM_SAMPLES; ++i) {
		struct gmc_sample *smp = &s->samples[i];

		uint32_t start = gmc_read_u32_be(buf + pos); pos += 4;
		uint16_t len_words = gmc_read_u16_be(buf + pos); pos += 2;
		uint16_t volume = gmc_read_u16_be(buf + pos); pos += 2;
		uint32_t loop_start = gmc_read_u32_be(buf + pos); pos += 4;
		uint16_t loop_len_words = gmc_read_u16_be(buf + pos); pos += 2;
		pos += 2;

		smp->length = (uint16_t)(len_words * 2);
		smp->volume = volume;

		if((loop_len_words != 0) && (loop_len_words != 2)) {
			smp->loop_start = (uint16_t)(loop_start - start);
			smp->loop_length = (uint16_t)(loop_len_words * 2);
		} else {
			smp->loop_start = 0;
			smp->loop_length = 0;
		}
		smp->data = 0;
	}

	return 1;
}

// [=]===^=[ gmc_load_positions ]=================================================================[=]
static int32_t gmc_load_positions(struct gamemusiccreator_state *s, int32_t *out_num_patterns) {
	uint8_t *buf = s->module_data;

	int32_t num_positions = gmc_read_i32_be(buf + 240);
	int32_t kept = num_positions;

	s->position_list = (uint8_t *)calloc((size_t)num_positions, 1);
	if(!s->position_list) {
		return 0;
	}

	int32_t num_patterns = 0;
	int32_t out_idx = 0;

	for(int32_t i = 0; i < num_positions; ++i) {
		uint16_t entry = gmc_read_u16_be(buf + 244 + i * 2);
		if(entry < 0x8000) {
			s->position_list[out_idx] = (uint8_t)(entry / 1024);
			if(s->position_list[out_idx] > num_patterns) {
				num_patterns = s->position_list[out_idx];
			}
			out_idx++;
		} else {
			kept--;
		}
	}

	s->number_of_positions = kept;
	num_patterns++;
	*out_num_patterns = num_patterns;
	return 1;
}

// [=]===^=[ gmc_load_patterns ]==================================================================[=]
static int32_t gmc_load_patterns(struct gamemusiccreator_state *s, int32_t num_patterns) {
	uint8_t *buf = s->module_data;
	uint32_t len = s->module_len;

	if((uint32_t)(GMC_HEADER_SIZE + num_patterns * GMC_PATTERN_BYTES) > len) {
		return 0;
	}

	s->patterns = (struct gmc_pattern *)calloc((size_t)num_patterns, sizeof(struct gmc_pattern));
	if(!s->patterns) {
		return 0;
	}
	s->num_patterns = (uint32_t)num_patterns;

	uint32_t pos = GMC_HEADER_SIZE;
	for(int32_t i = 0; i < num_patterns; ++i) {
		struct gmc_pattern *pat = &s->patterns[i];
		for(int32_t j = 0; j < GMC_PATTERN_ROWS; ++j) {
			for(int32_t k = 0; k < GMC_NUM_CHANNELS; ++k) {
				uint16_t period = gmc_read_u16_be(buf + pos); pos += 2;
				uint8_t b3 = buf[pos++];
				uint8_t b4 = buf[pos++];

				struct gmc_track_line *tl = &pat->tracks[k][j];
				tl->period = period;
				tl->sample = (uint8_t)(b3 >> 4);
				tl->effect = (uint8_t)(b3 & 0x0f);
				tl->effect_arg = b4;
			}
		}
	}

	return 1;
}

// [=]===^=[ gmc_load_sample_data ]===============================================================[=]
static int32_t gmc_load_sample_data(struct gamemusiccreator_state *s, int32_t num_patterns) {
	uint8_t *buf = s->module_data;
	uint32_t len = s->module_len;

	uint32_t pos = (uint32_t)GMC_HEADER_SIZE + (uint32_t)num_patterns * (uint32_t)GMC_PATTERN_BYTES;

	for(int32_t i = 0; i < GMC_NUM_SAMPLES; ++i) {
		struct gmc_sample *smp = &s->samples[i];
		if(smp->length > 0) {
			if((pos + smp->length) > len) {
				// Tolerate truncation: clip length so we do not overrun.
				if(pos >= len) {
					smp->length = 0;
					smp->loop_length = 0;
					smp->data = 0;
					continue;
				}
				smp->length = (uint16_t)(len - pos);
				if(smp->loop_length > 0) {
					if(smp->loop_start >= smp->length) {
						smp->loop_length = 0;
					} else if((uint32_t)smp->loop_start + (uint32_t)smp->loop_length > smp->length) {
						smp->loop_length = (uint16_t)(smp->length - smp->loop_start);
					}
				}
			}
			smp->data = (int8_t *)(buf + pos);
			pos += smp->length;
		} else {
			smp->data = 0;
		}
	}

	return 1;
}

// [=]===^=[ gmc_initialize_sound ]===============================================================[=]
static void gmc_initialize_sound(struct gamemusiccreator_state *s, int32_t start_position) {
	s->current_position = (int16_t)(start_position - 1);
	s->current_pattern = 0;
	s->song_speed = 0;
	s->song_step = 6;
	s->pattern_count = 63;
	s->end_reached = 0;

	for(int32_t i = 0; i < GMC_NUM_CHANNELS; ++i) {
		s->channels[i].slide = 0;
		s->channels[i].period = 0;
		s->channels[i].volume = 0;
	}
}

// [=]===^=[ gmc_update_pattern_counters ]========================================================[=]
static void gmc_update_pattern_counters(struct gamemusiccreator_state *s) {
	s->pattern_count++;
	if(s->pattern_count == GMC_PATTERN_ROWS) {
		s->current_position++;
		if(s->current_position >= s->number_of_positions) {
			s->current_position = 0;
			s->end_reached = 1;
		}
		s->pattern_count = 0;
		s->current_pattern = s->position_list[s->current_position];
	}
}

// [=]===^=[ gmc_set_instrument ]=================================================================[=]
static void gmc_set_instrument(struct gamemusiccreator_state *s, int32_t voice_idx, struct gmc_track_line *tl) {
	struct gmc_channel_info *ci = &s->channels[voice_idx];

	if(tl->sample == 0) {
		return;
	}

	struct gmc_sample *smp = &s->samples[tl->sample - 1];

	ci->period = tl->period;
	ci->volume = smp->volume;
	ci->slide = 0;

	if(smp->data != 0) {
		paula_play_sample(&s->paula, voice_idx, smp->data, smp->length);
		if(smp->loop_length > 0) {
			paula_set_loop(&s->paula, voice_idx, smp->loop_start, smp->loop_length);
		} else {
			paula_set_loop(&s->paula, voice_idx, 0, 0);
		}
		paula_set_volume_256(&s->paula, voice_idx, 0);
		paula_set_period(&s->paula, voice_idx, ci->period);
	} else {
		paula_mute(&s->paula, voice_idx);
	}
}

// [=]===^=[ gmc_set_effect ]=====================================================================[=]
static void gmc_set_effect(struct gamemusiccreator_state *s, int32_t voice_idx, struct gmc_track_line *tl) {
	struct gmc_channel_info *ci = &s->channels[voice_idx];
	uint8_t arg = tl->effect_arg;

	switch(tl->effect) {
		case GMC_EFFECT_SLIDE_UP: {
			ci->slide = -(int32_t)arg;
			break;
		}

		case GMC_EFFECT_SLIDE_DOWN: {
			ci->slide = (int32_t)arg;
			break;
		}

		case GMC_EFFECT_SET_VOLUME: {
			if(arg > 64) {
				arg = 64;
			}
			ci->volume = arg;
			break;
		}

		case GMC_EFFECT_PATTERN_BREAK: {
			s->pattern_count = 63;
			break;
		}

		case GMC_EFFECT_POSITION_JUMP: {
			s->current_position = (int16_t)(arg - 1);
			s->pattern_count = 63;
			break;
		}

		case GMC_EFFECT_ENABLE_FILTER: {
			break;
		}

		case GMC_EFFECT_DISABLE_FILTER: {
			break;
		}

		case GMC_EFFECT_SET_SPEED: {
			s->song_step = arg;
			break;
		}

		default: {
			break;
		}
	}
}

// [=]===^=[ gmc_play_pattern_row ]===============================================================[=]
static void gmc_play_pattern_row(struct gamemusiccreator_state *s) {
	struct gmc_pattern *pat = &s->patterns[s->current_pattern];
	for(int32_t i = 0; i < GMC_NUM_CHANNELS; ++i) {
		struct gmc_track_line *tl = &pat->tracks[i][s->pattern_count];
		gmc_set_instrument(s, i, tl);
		gmc_set_effect(s, i, tl);
	}
}

// [=]===^=[ gmc_every_tick ]=====================================================================[=]
static void gmc_every_tick(struct gamemusiccreator_state *s) {
	for(int32_t i = 0; i < GMC_NUM_CHANNELS; ++i) {
		struct gmc_channel_info *ci = &s->channels[i];
		ci->period = (uint16_t)((int32_t)ci->period + ci->slide);
		paula_set_period(&s->paula, i, ci->period);
		paula_set_volume(&s->paula, i, ci->volume);
	}
}

// [=]===^=[ gmc_tick ]===========================================================================[=]
static void gmc_tick(struct gamemusiccreator_state *s) {
	gmc_every_tick(s);

	s->song_speed++;
	if(s->song_speed >= s->song_step) {
		s->song_speed = 0;
		gmc_update_pattern_counters(s);
		gmc_play_pattern_row(s);
	}
}

// [=]===^=[ gamemusiccreator_init ]==============================================================[=]
static struct gamemusiccreator_state *gamemusiccreator_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < (uint32_t)GMC_HEADER_SIZE || sample_rate < 8000) {
		return 0;
	}

	if(!gmc_test_module((uint8_t *)data, len)) {
		return 0;
	}

	struct gamemusiccreator_state *s = (struct gamemusiccreator_state *)calloc(1, sizeof(struct gamemusiccreator_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;

	int32_t num_patterns = 0;

	if(!gmc_load_samples_info(s)) {
		goto fail;
	}
	if(!gmc_load_positions(s, &num_patterns)) {
		goto fail;
	}
	if(!gmc_load_patterns(s, num_patterns)) {
		goto fail;
	}
	if(!gmc_load_sample_data(s, num_patterns)) {
		goto fail;
	}

	paula_init(&s->paula, sample_rate, GMC_TICK_HZ);
	gmc_initialize_sound(s, 0);
	return s;

fail:
	free(s->position_list);
	free(s->patterns);
	free(s);
	return 0;
}

// [=]===^=[ gamemusiccreator_free ]==============================================================[=]
static void gamemusiccreator_free(struct gamemusiccreator_state *s) {
	if(!s) {
		return;
	}
	free(s->position_list);
	free(s->patterns);
	free(s);
}

// [=]===^=[ gamemusiccreator_get_audio ]=========================================================[=]
static void gamemusiccreator_get_audio(struct gamemusiccreator_state *s, int16_t *output, int32_t frames) {
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
			gmc_tick(s);
		}
	}
}

// [=]===^=[ gamemusiccreator_api_init ]==========================================================[=]
static void *gamemusiccreator_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return gamemusiccreator_init(data, len, sample_rate);
}

// [=]===^=[ gamemusiccreator_api_free ]==========================================================[=]
static void gamemusiccreator_api_free(void *state) {
	gamemusiccreator_free((struct gamemusiccreator_state *)state);
}

// [=]===^=[ gamemusiccreator_api_get_audio ]=====================================================[=]
static void gamemusiccreator_api_get_audio(void *state, int16_t *output, int32_t frames) {
	gamemusiccreator_get_audio((struct gamemusiccreator_state *)state, output, frames);
}

static const char *gamemusiccreator_extensions[] = { "gmc", 0 };

static struct player_api gamemusiccreator_api = {
	"Game Music Creator",
	gamemusiccreator_extensions,
	gamemusiccreator_api_init,
	gamemusiccreator_api_free,
	gamemusiccreator_api_get_audio,
	0,
};
