// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// SoundFX replayer, ported from NostalgicPlayer's C# implementation.
// Handles both SoundFX 2.0 ("SO31", 31 samples, mark at offset 124) and the
// older SoundFX 1.x ("SONG", 15 samples, mark at offset 60). Internally both
// are loaded into the same 31-slot state; for SONG modules the upper 16
// slots stay empty.
// Drives a 4-channel Amiga Paula (see paula.h). Tick rate is derived from the
// module's CIA timer value (PlayingFrequency = 709379 / delay).
//
// Public API:
//   struct soundfx_state *soundfx_init(void *data, uint32_t len, int32_t sample_rate);
//   void soundfx_free(struct soundfx_state *s);
//   void soundfx_get_audio(struct soundfx_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define SOUNDFX_NUM_SAMPLES   31
#define SOUNDFX_PATTERN_ROWS  64
#define SOUNDFX_ORDER_COUNT   128
#define SOUNDFX_NOTETABLE_LEN 87

struct soundfx_sample {
	int8_t *sample_addr;
	uint32_t length;
	uint16_t volume;
	uint32_t loop_start;
	uint32_t loop_length;
};

struct soundfx_channel {
	uint32_t pattern_data;
	int16_t sample_number;
	int8_t *sample;
	uint32_t sample_len;
	uint32_t loop_start;
	uint32_t loop_length;
	uint16_t current_note;
	uint16_t volume;
	int16_t step_value;
	uint16_t step_note;
	uint16_t step_end_note;
	uint16_t slide_control;
	uint8_t slide_direction;
	uint16_t slide_param;
	uint16_t slide_period;
	uint16_t slide_speed;
};

struct soundfx_state {
	struct paula paula;

	struct soundfx_sample samples[SOUNDFX_NUM_SAMPLES];
	uint8_t orders[SOUNDFX_ORDER_COUNT];
	uint32_t **patterns;
	uint16_t max_pattern;
	uint16_t delay;
	uint32_t song_length;

	uint32_t num_samples;     // 15 (SONG) or 31 (SO31)

	uint16_t timer;
	uint32_t track_pos;
	uint32_t pos_counter;
	uint8_t break_flag;

	struct soundfx_channel channels[4];
};

// [=]===^=[ soundfx_note_table ]=================================================================[=]
// Period table for arpeggio / step lookups. Last entry (-1 in C# source) is
// represented here as a sentinel 0xffff that no real period will match.
static int16_t soundfx_note_table[SOUNDFX_NOTETABLE_LEN] = {
	1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076,
	1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1016,  960,  906,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
	 113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,
	 113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,
	 113,  113,   -1
};

// [=]===^=[ soundfx_read_u16_be ]================================================================[=]
static uint16_t soundfx_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ soundfx_read_u32_be ]================================================================[=]
static uint32_t soundfx_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ soundfx_identify ]===================================================================[=]
// Returns 31 for SO31 modules, 15 for SONG modules, 0 if neither.
static uint32_t soundfx_identify(uint8_t *buf, uint32_t len) {
	if(len >= 144 && buf[124] == 'S' && buf[125] == 'O' && buf[126] == '3' && buf[127] == '1') {
		uint64_t total = 0;
		for(uint32_t i = 0; i < 31; ++i) {
			total += soundfx_read_u32_be(buf + i * 4);
		}
		if(total <= len) {
			return 31;
		}
	}
	if(len >= 80 && buf[60] == 'S' && buf[61] == 'O' && buf[62] == 'N' && buf[63] == 'G') {
		uint64_t total = 0;
		for(uint32_t i = 0; i < 15; ++i) {
			total += soundfx_read_u32_be(buf + i * 4);
		}
		if(total <= len) {
			return 15;
		}
	}
	return 0;
}

// [=]===^=[ soundfx_cleanup ]====================================================================[=]
static void soundfx_cleanup(struct soundfx_state *s) {
	if(!s) {
		return;
	}
	if(s->patterns) {
		for(uint32_t i = 0; i < s->max_pattern; ++i) {
			free(s->patterns[i]);
		}
		free(s->patterns);
		s->patterns = 0;
	}
}

// [=]===^=[ soundfx_load ]=======================================================================[=]
static int32_t soundfx_load(struct soundfx_state *s, uint8_t *buf, uint32_t len) {
	uint32_t sample_sizes[SOUNDFX_NUM_SAMPLES];
	uint32_t pos = 0;

	if(len < 80) {
		return 0;
	}

	for(uint32_t i = 0; i < s->num_samples; ++i) {
		sample_sizes[i] = soundfx_read_u32_be(buf + pos);
		pos += 4;
	}
	for(uint32_t i = s->num_samples; i < SOUNDFX_NUM_SAMPLES; ++i) {
		sample_sizes[i] = 0;
	}

	// Skip the "SO31" / "SONG" mark (4 bytes already validated by caller).
	pos += 4;

	s->delay = soundfx_read_u16_be(buf + pos);
	pos += 2;

	// Skip 14 pad bytes.
	pos += 14;

	for(uint32_t i = 0; i < s->num_samples; ++i) {
		struct soundfx_sample *smp = &s->samples[i];

		if(pos + 22 + 8 > len) {
			return 0;
		}

		// Skip 22-byte name.
		pos += 22;

		uint32_t length     = (uint32_t)soundfx_read_u16_be(buf + pos) * 2; pos += 2;
		uint16_t volume     = soundfx_read_u16_be(buf + pos);                pos += 2;
		uint32_t loop_start = (uint32_t)soundfx_read_u16_be(buf + pos);      pos += 2;
		uint32_t loop_len   = (uint32_t)soundfx_read_u16_be(buf + pos) * 2;  pos += 2;

		if((loop_start + loop_len) > sample_sizes[i]) {
			loop_len = (sample_sizes[i] > loop_start) ? (sample_sizes[i] - loop_start) : 0;
		}

		if((length != 0) && (loop_start == length)) {
			length += loop_len;
		}

		if(length > 2) {
			uint32_t end = loop_start + loop_len;
			if(end > length) {
				length = end;
			}
		}

		// Tracker quirk: zero-length declared but bytes present (e.g. "September").
		if((length == 0) && (sample_sizes[i] != 0)) {
			length = sample_sizes[i];
		}

		if(volume > 64) {
			volume = 64;
		}

		smp->length      = length;
		smp->volume      = volume;
		smp->loop_start  = loop_start;
		smp->loop_length = loop_len;
		smp->sample_addr = 0;
	}

	if(pos + 2 > len) {
		return 0;
	}
	s->song_length = buf[pos]; pos += 1;
	pos += 1; // skip pad

	if(pos + SOUNDFX_ORDER_COUNT + 4 > len) {
		return 0;
	}
	memcpy(s->orders, buf + pos, SOUNDFX_ORDER_COUNT);
	pos += SOUNDFX_ORDER_COUNT;
	if(s->num_samples == 31) {
		pos += 4; // SO31 has 4 pad bytes after orders; SONG does not.
	}

	s->max_pattern = 0;
	for(uint32_t i = 0; i < s->song_length; ++i) {
		if(s->orders[i] > s->max_pattern) {
			s->max_pattern = s->orders[i];
		}
	}
	s->max_pattern++;

	uint32_t pattern_bytes = 4 * SOUNDFX_PATTERN_ROWS * 4;
	if(pos + (uint32_t)s->max_pattern * pattern_bytes > len) {
		return 0;
	}

	s->patterns = (uint32_t **)calloc(s->max_pattern, sizeof(uint32_t *));
	if(!s->patterns) {
		return 0;
	}

	for(uint32_t i = 0; i < s->max_pattern; ++i) {
		s->patterns[i] = (uint32_t *)malloc(pattern_bytes);
		if(!s->patterns[i]) {
			return 0;
		}
		for(uint32_t j = 0; j < 4 * SOUNDFX_PATTERN_ROWS; ++j) {
			s->patterns[i][j] = soundfx_read_u32_be(buf + pos);
			pos += 4;
		}
	}

	for(uint32_t i = 0; i < s->num_samples; ++i) {
		uint32_t sz = sample_sizes[i];
		if(sz == 0) {
			continue;
		}
		// Tolerate truncated final sample by up to 512 bytes (matches loader).
		uint32_t avail = (pos < len) ? (len - pos) : 0;
		if(avail + 512 < sz) {
			return 0;
		}
		if(avail < sz) {
			sz = avail;
		}
		s->samples[i].sample_addr = (int8_t *)(buf + pos);
		pos += sz;
	}

	return 1;
}

// [=]===^=[ soundfx_initialize_sound ]===========================================================[=]
static void soundfx_initialize_sound(struct soundfx_state *s) {
	s->timer = 0;
	s->track_pos = 0;
	s->pos_counter = 0;
	s->break_flag = 0;
	memset(s->channels, 0, sizeof(s->channels));
}

// [=]===^=[ soundfx_play_note ]==================================================================[=]
static void soundfx_play_note(struct soundfx_state *s, int32_t voice_idx, uint32_t pattern_data) {
	struct soundfx_channel *ch = &s->channels[voice_idx];

	ch->pattern_data = pattern_data;

	if((pattern_data & 0xffff0000) != 0xfffd0000) {
		uint8_t sample_num = (uint8_t)((pattern_data & 0x0000f000) >> 12);
		if((pattern_data & 0x10000000) != 0) {
			sample_num += 16;
		}

		if(sample_num != 0) {
			struct soundfx_sample *cur = &s->samples[sample_num - 1];

			ch->sample_number = (int16_t)(sample_num - 1);
			ch->sample        = cur->sample_addr;
			ch->sample_len    = cur->length;
			ch->volume        = cur->volume;
			ch->loop_start    = cur->loop_start;
			ch->loop_length   = cur->loop_length;

			int16_t volume = (int16_t)ch->volume;

			switch((pattern_data & 0x00000f00) >> 8) {
				case 5: {
					volume += (int16_t)(pattern_data & 0xff);
					if(volume > 64) {
						volume = 64;
					}
					break;
				}

				case 6: {
					volume -= (int16_t)(pattern_data & 0xff);
					if(volume < 0) {
						volume = 0;
					}
					break;
				}
			}

			paula_set_volume(&s->paula, voice_idx, (uint16_t)volume);
		}
	}

	if((pattern_data & 0xffff0000) == 0xfffd0000) {
		ch->pattern_data &= 0xffff0000;
		return;
	}

	if((pattern_data & 0xffff0000) == 0) {
		return;
	}

	ch->slide_speed = 0;
	ch->step_value  = 0;

	ch->current_note = (uint16_t)(((pattern_data & 0xffff0000) >> 16) & 0xefff);

	if((pattern_data & 0xffff0000) == 0xfffe0000) {
		paula_mute(&s->paula, voice_idx);
		return;
	}

	if((pattern_data & 0xffff0000) == 0xfffc0000) {
		s->break_flag = 1;
		ch->pattern_data &= 0xefffffff;
		return;
	}

	if((pattern_data & 0xffff0000) == 0xfffb0000) {
		ch->pattern_data &= 0xefffffff;
		return;
	}

	if(ch->sample != 0) {
		paula_play_sample(&s->paula, voice_idx, ch->sample, ch->sample_len);
		paula_set_period(&s->paula, voice_idx, ch->current_note);

		if(ch->loop_length > 2) {
			paula_set_loop(&s->paula, voice_idx, ch->loop_start, ch->loop_length);
		} else {
			paula_set_loop(&s->paula, voice_idx, 0, 0);
		}
	}
}

// [=]===^=[ soundfx_arpeggio ]===================================================================[=]
static void soundfx_arpeggio(struct soundfx_state *s, struct soundfx_channel *ch, int32_t voice_idx) {
	int16_t index;
	int32_t note;

	switch(s->timer) {
		case 1:
		case 5: {
			index = (int16_t)((ch->pattern_data & 0x000000f0) >> 4);
			break;
		}

		case 2:
		case 4: {
			index = (int16_t)(ch->pattern_data & 0x0000000f);
			break;
		}

		default: {
			paula_set_period(&s->paula, voice_idx, ch->current_note);
			return;
		}
	}

	note = 20;
	for(;;) {
		if(note >= SOUNDFX_NOTETABLE_LEN || soundfx_note_table[note] == -1) {
			return;
		}
		if(soundfx_note_table[note] == (int16_t)ch->current_note) {
			break;
		}
		note++;
	}

	int32_t target = note + index;
	if(target < 0 || target >= SOUNDFX_NOTETABLE_LEN) {
		return;
	}
	int16_t period = soundfx_note_table[target];
	if(period == -1) {
		return;
	}
	paula_set_period(&s->paula, voice_idx, (uint16_t)period);
}

// [=]===^=[ soundfx_step_finder ]================================================================[=]
static void soundfx_step_finder(struct soundfx_channel *ch, uint8_t step_down) {
	int16_t step_value = (int16_t)(ch->pattern_data & 0x0000000f);
	int16_t end_index  = (int16_t)((ch->pattern_data & 0x000000f0) >> 4);
	int32_t note;

	if(step_down) {
		step_value = (int16_t)(-step_value);
	} else {
		end_index = (int16_t)(-end_index);
	}

	ch->step_note  = ch->current_note;
	ch->step_value = step_value;

	note = 20;
	for(;;) {
		if(note >= SOUNDFX_NOTETABLE_LEN || soundfx_note_table[note] == -1) {
			ch->step_end_note = ch->current_note;
			return;
		}
		if(soundfx_note_table[note] == (int16_t)ch->current_note) {
			break;
		}
		note++;
	}

	int32_t target = note + end_index;
	if(target < 0 || target >= SOUNDFX_NOTETABLE_LEN) {
		ch->step_end_note = ch->current_note;
		return;
	}
	int16_t period = soundfx_note_table[target];
	if(period == -1) {
		ch->step_end_note = ch->current_note;
		return;
	}
	ch->step_end_note = (uint16_t)period;
}

// [=]===^=[ soundfx_make_effects ]===============================================================[=]
static void soundfx_make_effects(struct soundfx_state *s, int32_t voice_idx) {
	struct soundfx_channel *ch = &s->channels[voice_idx];

	if(ch->step_value != 0) {
		if(ch->step_value < 0) {
			ch->step_note = (uint16_t)(ch->step_note + ch->step_value);
			if(ch->step_note <= ch->step_end_note) {
				ch->step_value = 0;
				ch->step_note = ch->step_end_note;
			}
		} else {
			ch->step_note = (uint16_t)(ch->step_note + ch->step_value);
			if(ch->step_note >= ch->step_end_note) {
				ch->step_value = 0;
				ch->step_note = ch->step_end_note;
			}
		}

		ch->current_note = ch->step_note;
		paula_set_period(&s->paula, voice_idx, ch->current_note);
		return;
	}

	if(ch->slide_speed != 0) {
		uint16_t value = (uint16_t)(ch->slide_param & 0x0f);
		if(value != 0) {
			if(++ch->slide_control == value) {
				ch->slide_control = 0;
				value = (uint16_t)((ch->slide_param << 4) << 3);

				if(!ch->slide_direction) {
					ch->slide_period = (uint16_t)(ch->slide_period + 8);
					value = (uint16_t)(value + ch->slide_speed);
					if(value == ch->slide_period) {
						ch->slide_direction = 1;
					}
				} else {
					ch->slide_period = (uint16_t)(ch->slide_period - 8);
					value = (uint16_t)(value - ch->slide_speed);
					if(value == ch->slide_period) {
						ch->slide_direction = 0;
					}
				}

				ch->current_note = ch->slide_period;
				paula_set_period(&s->paula, voice_idx, ch->slide_period);
			}
		}
	}

	switch((ch->pattern_data & 0x00000f00) >> 8) {
		case 1: {
			soundfx_arpeggio(s, ch, voice_idx);
			break;
		}

		case 2: {
			uint16_t new_period;
			int16_t bend_value = (int16_t)((ch->pattern_data & 0x000000f0) >> 4);

			if(bend_value != 0) {
				new_period = (uint16_t)(((ch->pattern_data & 0xefff0000) >> 16) + bend_value);
			} else {
				bend_value = (int16_t)(ch->pattern_data & 0x0000000f);
				if(bend_value == 0) {
					break;
				}
				new_period = (uint16_t)(((ch->pattern_data & 0xefff0000) >> 16) - bend_value);
			}

			paula_set_period(&s->paula, voice_idx, new_period);
			ch->pattern_data = (ch->pattern_data & 0x1000ffff) | ((uint32_t)new_period << 16);
			break;
		}

		// Cases 3 and 4 toggle the Amiga LED filter; the C# port noted the
		// original tracker swapped the on/off labels. We have no real filter
		// to model in this offline mixer, so they are no-ops.
		case 3: { break; }
		case 4: { break; }

		case 7: {
			soundfx_step_finder(ch, 0);
			break;
		}

		case 8: {
			soundfx_step_finder(ch, 1);
			break;
		}

		// Auto slide. Not in the official replayers; "Alcatrash" by Kerni
		// uses it. Implementation taken from Flod 4.1 via the C# port.
		case 9: {
			ch->slide_speed = ch->current_note;
			ch->slide_period = ch->current_note;
			ch->slide_param = (uint16_t)(ch->pattern_data & 0x000000ff);
			ch->slide_direction = 0;
			ch->slide_control = 0;
			break;
		}
	}
}

// [=]===^=[ soundfx_play_sound ]=================================================================[=]
static void soundfx_play_sound(struct soundfx_state *s) {
	uint32_t *pattern = s->patterns[s->orders[s->track_pos]];

	soundfx_play_note(s, 0, pattern[s->pos_counter + 0]);
	soundfx_play_note(s, 1, pattern[s->pos_counter + 1]);
	soundfx_play_note(s, 2, pattern[s->pos_counter + 2]);
	soundfx_play_note(s, 3, pattern[s->pos_counter + 3]);

	if(s->break_flag) {
		s->break_flag = 0;
		s->pos_counter = 4 * 63;
	}

	s->pos_counter += 4;
	if(s->pos_counter == 4 * SOUNDFX_PATTERN_ROWS) {
		s->pos_counter = 0;
		s->track_pos++;
		if(s->track_pos == s->song_length) {
			s->track_pos = 0;
		}
	}
}

// [=]===^=[ soundfx_tick ]=======================================================================[=]
static void soundfx_tick(struct soundfx_state *s) {
	s->timer++;
	if(s->timer == 6) {
		s->timer = 0;
		soundfx_play_sound(s);
	} else {
		soundfx_make_effects(s, 0);
		soundfx_make_effects(s, 1);
		soundfx_make_effects(s, 2);
		soundfx_make_effects(s, 3);
	}
}

// [=]===^=[ soundfx_init ]=======================================================================[=]
static struct soundfx_state *soundfx_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || sample_rate < 8000) {
		return 0;
	}
	uint32_t num_samples = soundfx_identify((uint8_t *)data, len);
	if(num_samples == 0) {
		return 0;
	}

	struct soundfx_state *s = (struct soundfx_state *)calloc(1, sizeof(struct soundfx_state));
	if(!s) {
		return 0;
	}
	s->num_samples = num_samples;

	if(!soundfx_load(s, (uint8_t *)data, len)) {
		soundfx_cleanup(s);
		free(s);
		return 0;
	}

	int32_t tick_rate = (s->delay != 0) ? (709379 / (int32_t)s->delay) : 50;
	if(tick_rate < 1) {
		tick_rate = 50;
	}
	paula_init(&s->paula, sample_rate, tick_rate);
	soundfx_initialize_sound(s);
	return s;
}

// [=]===^=[ soundfx_free ]=======================================================================[=]
static void soundfx_free(struct soundfx_state *s) {
	if(!s) {
		return;
	}
	soundfx_cleanup(s);
	free(s);
}

// [=]===^=[ soundfx_get_audio ]==================================================================[=]
static void soundfx_get_audio(struct soundfx_state *s, int16_t *output, int32_t frames) {
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
			soundfx_tick(s);
		}
	}
}

// [=]===^=[ soundfx_api_init ]===================================================================[=]
static void *soundfx_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return soundfx_init(data, len, sample_rate);
}

// [=]===^=[ soundfx_api_free ]===================================================================[=]
static void soundfx_api_free(void *state) {
	soundfx_free((struct soundfx_state *)state);
}

// [=]===^=[ soundfx_api_get_audio ]==============================================================[=]
static void soundfx_api_get_audio(void *state, int16_t *output, int32_t frames) {
	soundfx_get_audio((struct soundfx_state *)state, output, frames);
}

static const char *soundfx_extensions[] = { "sfx", "sfx2", 0 };

static struct player_api soundfx_api = {
	"SoundFX",
	soundfx_extensions,
	soundfx_api_init,
	soundfx_api_free,
	soundfx_api_get_audio,
	0,
};
