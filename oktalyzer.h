// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Oktalyzer 8-channel replayer, ported from NostalgicPlayer's C# implementation.
// Drives the Amiga Paula emulator (see paula.h) at 50Hz tick rate.
//
// Public API:
//   struct oktalyzer_state *oktalyzer_init(void *data, uint32_t len, int32_t sample_rate);
//   void oktalyzer_free(struct oktalyzer_state *s);
//   void oktalyzer_get_audio(struct oktalyzer_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define OKTALYZER_TICK_HZ      50
#define OKTALYZER_MAX_CHANNELS 8
#define OKTALYZER_MAX_PATTERNS 256
#define OKTALYZER_PATTERN_TABLE_LEN 128

// [=]===^=[ oktalyzer_periods ]===================================================================[=]
static int16_t oktalyzer_periods[36] = {
	856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
	428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
	214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113
};

// [=]===^=[ oktalyzer_arp10 ]=====================================================================[=]
static int8_t oktalyzer_arp10[16] = {
	0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0
};

// [=]===^=[ oktalyzer_arp12 ]=====================================================================[=]
static int8_t oktalyzer_arp12[16] = {
	0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3
};

// LRRL panning matching the C# Tables.PanPos (Left, Left, Right, Right, Right, Right, Left, Left).
// [=]===^=[ oktalyzer_pan_table ]=================================================================[=]
static uint8_t oktalyzer_pan_table[8] = {
	0, 0, 127, 127, 127, 127, 0, 0
};

struct oktalyzer_pattern_line {
	uint8_t note;
	uint8_t sample_num;
	uint8_t effect;
	uint8_t effect_arg;
};

struct oktalyzer_pattern {
	int16_t line_num;
	struct oktalyzer_pattern_line *lines;   // line_num * chan_num entries
};

struct oktalyzer_sample {
	uint32_t length;
	uint16_t repeat_start;
	uint16_t repeat_length;
	uint16_t volume;
	uint16_t mode;                          // 0 = "8" (normal only), 1 = "4" (mixed only), 2 = "B" (both)
	int8_t *sample_data;                    // owned, allocated buffer (post 7->8 bit conversion if needed)
	uint32_t alloc_len;
};

struct oktalyzer_channel_info {
	uint8_t curr_note;
	int16_t curr_period;
	uint32_t release_start;
	uint32_t release_length;
};

struct oktalyzer_state {
	struct paula paula;

	uint32_t samp_num;
	uint16_t patt_num;
	uint16_t song_length;
	uint16_t chan_num;
	uint16_t start_speed;

	uint8_t channel_flags[4];               // 0/1 per logical channel; set => mixed pair
	uint8_t pattern_table[OKTALYZER_PATTERN_TABLE_LEN];
	struct oktalyzer_sample *samples;
	struct oktalyzer_pattern *patterns;
	uint8_t chan_index[OKTALYZER_MAX_CHANNELS];

	uint16_t current_speed;
	uint16_t speed_counter;
	int16_t song_pos;
	int16_t new_song_pos;
	int16_t patt_pos;
	uint8_t filter_status;
	uint8_t end_reached;

	struct oktalyzer_pattern_line curr_line[OKTALYZER_MAX_CHANNELS];
	struct oktalyzer_channel_info chan_info[OKTALYZER_MAX_CHANNELS];
	int8_t chan_vol[OKTALYZER_MAX_CHANNELS];
};

// [=]===^=[ oktalyzer_read_u32_be ]===============================================================[=]
static uint32_t oktalyzer_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ oktalyzer_read_u16_be ]===============================================================[=]
static uint16_t oktalyzer_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ oktalyzer_cleanup ]===================================================================[=]
static void oktalyzer_cleanup(struct oktalyzer_state *s) {
	if(!s) {
		return;
	}
	if(s->patterns) {
		for(uint32_t i = 0; i < s->patt_num; ++i) {
			free(s->patterns[i].lines);
		}
		free(s->patterns);
		s->patterns = 0;
	}
	if(s->samples) {
		for(uint32_t i = 0; i < s->samp_num; ++i) {
			free(s->samples[i].sample_data);
		}
		free(s->samples);
		s->samples = 0;
	}
}

// [=]===^=[ oktalyzer_parse_cmod ]================================================================[=]
static int32_t oktalyzer_parse_cmod(struct oktalyzer_state *s, uint8_t *data, uint32_t chunk_size) {
	if(chunk_size != 8) {
		return 0;
	}
	s->chan_num = 4;
	for(int32_t i = 0; i < 4; ++i) {
		uint16_t flag = oktalyzer_read_u16_be(data + i * 2);
		if(flag == 0) {
			s->channel_flags[i] = 0;
		} else {
			s->channel_flags[i] = 1;
			s->chan_num++;
		}
	}
	return 1;
}

// [=]===^=[ oktalyzer_parse_samp ]================================================================[=]
static int32_t oktalyzer_parse_samp(struct oktalyzer_state *s, uint8_t *data, uint32_t chunk_size) {
	if(chunk_size < 32) {
		return 0;
	}
	s->samp_num = chunk_size / 32;
	s->samples = (struct oktalyzer_sample *)calloc(s->samp_num, sizeof(struct oktalyzer_sample));
	if(!s->samples) {
		return 0;
	}
	for(uint32_t i = 0; i < s->samp_num; ++i) {
		uint8_t *p = data + i * 32;
		struct oktalyzer_sample *smp = &s->samples[i];
		// p[0..19] = sample name (skipped)
		smp->length = oktalyzer_read_u32_be(p + 20);
		smp->repeat_start = (uint16_t)(oktalyzer_read_u16_be(p + 24) * 2);
		smp->repeat_length = (uint16_t)(oktalyzer_read_u16_be(p + 26) * 2);
		if(smp->repeat_length <= 2) {
			smp->repeat_start = 0;
			smp->repeat_length = 0;
		}
		// p[28] reserved
		smp->volume = p[29];
		smp->mode = oktalyzer_read_u16_be(p + 30);
	}
	return 1;
}

// [=]===^=[ oktalyzer_parse_pbod ]================================================================[=]
static int32_t oktalyzer_parse_pbod(struct oktalyzer_state *s, uint32_t patt_index, uint8_t *data, uint32_t chunk_size) {
	if(chunk_size < 2) {
		return 0;
	}
	uint16_t line_num = oktalyzer_read_u16_be(data);
	uint32_t needed = 2 + (uint32_t)line_num * (uint32_t)s->chan_num * 4;
	if(needed > chunk_size) {
		return 0;
	}
	struct oktalyzer_pattern *pat = &s->patterns[patt_index];
	pat->line_num = (int16_t)line_num;
	pat->lines = (struct oktalyzer_pattern_line *)calloc((size_t)line_num * s->chan_num, sizeof(struct oktalyzer_pattern_line));
	if(!pat->lines) {
		return 0;
	}
	uint8_t *p = data + 2;
	for(int32_t i = 0; i < line_num; ++i) {
		for(int32_t j = 0; j < s->chan_num; ++j) {
			struct oktalyzer_pattern_line *ln = &pat->lines[i * s->chan_num + j];
			ln->note       = p[0];
			ln->sample_num = p[1];
			ln->effect     = p[2];
			ln->effect_arg = p[3];
			p += 4;
		}
	}
	return 1;
}

// [=]===^=[ oktalyzer_convert_7bit_to_8bit ]======================================================[=]
static void oktalyzer_convert_7bit_to_8bit(int8_t *data, uint32_t length) {
	for(uint32_t i = 0; i < length; ++i) {
		data[i] = (int8_t)((int32_t)data[i] * 2);
	}
}

// [=]===^=[ oktalyzer_parse_sbod ]================================================================[=]
static int32_t oktalyzer_parse_sbod(struct oktalyzer_state *s, uint32_t *read_samp, uint8_t *data, uint32_t chunk_size) {
	while((*read_samp < s->samp_num) && (s->samples[*read_samp].length == 0)) {
		(*read_samp)++;
	}
	if(*read_samp >= s->samp_num) {
		return 1;
	}
	struct oktalyzer_sample *smp = &s->samples[*read_samp];
	uint32_t alloc_len = (chunk_size > smp->length) ? chunk_size : smp->length;
	smp->sample_data = (int8_t *)calloc(alloc_len, 1);
	if(!smp->sample_data) {
		return 0;
	}
	smp->alloc_len = alloc_len;
	uint32_t copy_len = (chunk_size < alloc_len) ? chunk_size : alloc_len;
	memcpy(smp->sample_data, data, copy_len);
	if((smp->mode == 0) || (smp->mode == 2)) {
		oktalyzer_convert_7bit_to_8bit(smp->sample_data, smp->length);
	}
	(*read_samp)++;
	return 1;
}

// [=]===^=[ oktalyzer_load ]======================================================================[=]
static int32_t oktalyzer_load(struct oktalyzer_state *s, uint8_t *buf, uint32_t len) {
	if(len < 1368) {
		return 0;
	}
	if(memcmp(buf, "OKTASONG", 8) != 0) {
		return 0;
	}

	s->samp_num = 0;
	s->patt_num = 0;
	s->song_length = 0;
	s->start_speed = 6;

	uint32_t pos = 8;
	uint32_t read_patt = 0;
	uint32_t read_samp = 0;
	uint32_t pbod_seen = 0;
	uint32_t sbod_seen = 0;

	while(pos + 8 <= len) {
		uint8_t *name = buf + pos;
		uint32_t chunk_size = oktalyzer_read_u32_be(buf + pos + 4);
		pos += 8;
		if(pos + chunk_size > len) {
			return 0;
		}
		uint8_t *cdata = buf + pos;

		if(memcmp(name, "CMOD", 4) == 0) {
			if(!oktalyzer_parse_cmod(s, cdata, chunk_size)) {
				return 0;
			}
		} else if(memcmp(name, "SAMP", 4) == 0) {
			if(!oktalyzer_parse_samp(s, cdata, chunk_size)) {
				return 0;
			}
		} else if(memcmp(name, "SPEE", 4) == 0) {
			if(chunk_size != 2) {
				return 0;
			}
			s->start_speed = oktalyzer_read_u16_be(cdata);
		} else if(memcmp(name, "SLEN", 4) == 0) {
			if(chunk_size != 2) {
				return 0;
			}
			s->patt_num = oktalyzer_read_u16_be(cdata);
			if(s->patt_num > OKTALYZER_MAX_PATTERNS) {
				return 0;
			}
			s->patterns = (struct oktalyzer_pattern *)calloc(s->patt_num, sizeof(struct oktalyzer_pattern));
			if(!s->patterns) {
				return 0;
			}
		} else if(memcmp(name, "PLEN", 4) == 0) {
			if(chunk_size != 2) {
				return 0;
			}
			s->song_length = oktalyzer_read_u16_be(cdata);
		} else if(memcmp(name, "PATT", 4) == 0) {
			if(chunk_size != 128) {
				return 0;
			}
			memcpy(s->pattern_table, cdata, 128);
		} else if(memcmp(name, "PBOD", 4) == 0) {
			pbod_seen++;
			if((read_patt < s->patt_num) && (s->patterns != 0)) {
				if(!oktalyzer_parse_pbod(s, read_patt, cdata, chunk_size)) {
					return 0;
				}
				read_patt++;
			}
		} else if(memcmp(name, "SBOD", 4) == 0) {
			sbod_seen++;
			if((read_samp < s->samp_num) && (s->samples != 0)) {
				if(!oktalyzer_parse_sbod(s, &read_samp, cdata, chunk_size)) {
					return 0;
				}
			}
		} else {
			// Unknown chunk: tolerate trailing junk after samples loaded.
			if(read_samp == 0) {
				return 0;
			}
			break;
		}

		pos += chunk_size;
	}

	if((s->chan_num == 0) || (s->patt_num == 0) || (s->song_length == 0)) {
		return 0;
	}
	return 1;
}

// [=]===^=[ oktalyzer_initialize_sound ]==========================================================[=]
static void oktalyzer_initialize_sound(struct oktalyzer_state *s, int32_t start_position) {
	s->song_pos = (int16_t)start_position;
	s->new_song_pos = -1;
	s->patt_pos = -1;
	s->current_speed = s->start_speed;
	s->speed_counter = 0;
	s->filter_status = 0;
	s->end_reached = 0;

	memset(s->chan_info, 0, sizeof(s->chan_info));
	memset(s->curr_line, 0, sizeof(s->curr_line));
	for(int32_t i = 0; i < OKTALYZER_MAX_CHANNELS; ++i) {
		s->chan_vol[i] = 64;
	}

	// Build the channel index table and pan the virtual channels.
	for(int32_t i = 0, pan_num = 0; i < s->chan_num; ++i, ++pan_num) {
		s->paula.ch[i].pan = oktalyzer_pan_table[pan_num];
		s->chan_index[i] = (uint8_t)(pan_num / 2);
		if(!s->channel_flags[pan_num / 2]) {
			pan_num++;
		}
	}
}

// [=]===^=[ oktalyzer_find_next_pattern_line ]====================================================[=]
static void oktalyzer_find_next_pattern_line(struct oktalyzer_state *s) {
	struct oktalyzer_pattern *patt = &s->patterns[s->pattern_table[s->song_pos]];

	s->patt_pos++;

	if((s->patt_pos >= patt->line_num) || (s->new_song_pos != -1)) {
		s->patt_pos = 0;
		if(s->new_song_pos != -1) {
			s->song_pos = s->new_song_pos;
			s->new_song_pos = -1;
		} else {
			s->song_pos++;
		}
		if(s->song_pos == (int16_t)s->song_length) {
			s->song_pos = 0;
			s->end_reached = 1;
		}
		patt = &s->patterns[s->pattern_table[s->song_pos]];
	}

	for(int32_t i = 0; i < s->chan_num; ++i) {
		s->curr_line[i] = patt->lines[s->patt_pos * s->chan_num + i];
	}
}

// [=]===^=[ oktalyzer_play_channel ]==============================================================[=]
static void oktalyzer_play_channel(struct oktalyzer_state *s, uint32_t channel_num) {
	struct oktalyzer_pattern_line *patt_data = &s->curr_line[channel_num];
	struct oktalyzer_channel_info *chan_data = &s->chan_info[channel_num];

	if(patt_data->note == 0) {
		return;
	}
	uint8_t note = (uint8_t)(patt_data->note - 1);
	if(note >= 36) {
		return;
	}
	if(patt_data->sample_num >= s->samp_num) {
		return;
	}

	struct oktalyzer_sample *samp = &s->samples[patt_data->sample_num];
	if((samp->sample_data == 0) || (samp->length == 0)) {
		return;
	}

	if(s->channel_flags[s->chan_index[channel_num]]) {
		// Mixed channel: mode 1 = "4" plays here, mode 0 = "8" doesn't.
		if(samp->mode == 1) {
			return;
		}
		paula_play_sample(&s->paula, (int32_t)channel_num, samp->sample_data, samp->length);
		chan_data->release_start = 0;
		chan_data->release_length = 0;
	} else {
		// Normal channel: mode 0 = "8" plays here, mode 1 = "4" doesn't.
		if(samp->mode == 0) {
			return;
		}
		s->chan_vol[s->chan_index[channel_num]] = (int8_t)samp->volume;

		if(samp->repeat_length == 0) {
			paula_play_sample(&s->paula, (int32_t)channel_num, samp->sample_data, samp->length);
			chan_data->release_start = 0;
			chan_data->release_length = 0;
		} else {
			uint32_t play_len = (uint32_t)samp->repeat_start + (uint32_t)samp->repeat_length;
			paula_play_sample(&s->paula, (int32_t)channel_num, samp->sample_data, play_len);
			paula_set_loop(&s->paula, (int32_t)channel_num, samp->repeat_start, samp->repeat_length);
			chan_data->release_start = play_len;
			chan_data->release_length = (samp->length > play_len) ? (samp->length - play_len) : 0;
		}
	}

	chan_data->curr_note = note;
	chan_data->curr_period = oktalyzer_periods[note];
	paula_set_period(&s->paula, (int32_t)channel_num, (uint16_t)chan_data->curr_period);
}

// [=]===^=[ oktalyzer_play_pattern_line ]=========================================================[=]
static void oktalyzer_play_pattern_line(struct oktalyzer_state *s) {
	for(uint32_t i = 0, j = 0; i < 4; ++i, ++j) {
		oktalyzer_play_channel(s, j);
		if(s->channel_flags[i]) {
			oktalyzer_play_channel(s, ++j);
		}
	}
}

// [=]===^=[ oktalyzer_play_note ]=================================================================[=]
static void oktalyzer_play_note(struct oktalyzer_state *s, uint32_t channel_num, struct oktalyzer_channel_info *chan_data, int8_t note) {
	if(note < 0) {
		note = 0;
	}
	if(note > 35) {
		note = 35;
	}
	chan_data->curr_period = oktalyzer_periods[note];
	paula_set_period(&s->paula, (int32_t)channel_num, (uint16_t)chan_data->curr_period);
}

// [=]===^=[ oktalyzer_do_channel_effect ]=========================================================[=]
static void oktalyzer_do_channel_effect(struct oktalyzer_state *s, uint32_t channel_num) {
	struct oktalyzer_pattern_line *patt_data = &s->curr_line[channel_num];
	struct oktalyzer_channel_info *chan_data = &s->chan_info[channel_num];
	int8_t work_note;
	int8_t arp_num;
	int32_t vol_index;
	uint8_t eff_arg;
	uint16_t new_pos;

	switch(patt_data->effect) {
		case 1: {
			// Portamento down
			chan_data->curr_period -= patt_data->effect_arg;
			if(chan_data->curr_period < 113) {
				chan_data->curr_period = 113;
			}
			paula_set_period(&s->paula, (int32_t)channel_num, (uint16_t)chan_data->curr_period);
			break;
		}

		case 2: {
			// Portamento up
			chan_data->curr_period += patt_data->effect_arg;
			if(chan_data->curr_period > 856) {
				chan_data->curr_period = 856;
			}
			paula_set_period(&s->paula, (int32_t)channel_num, (uint16_t)chan_data->curr_period);
			break;
		}

		case 10: {
			// Arpeggio type 1
			work_note = (int8_t)chan_data->curr_note;
			arp_num = oktalyzer_arp10[s->speed_counter & 0x0f];
			if(arp_num == 0) {
				work_note -= (int8_t)((patt_data->effect_arg & 0xf0) >> 4);
			} else if(arp_num == 2) {
				work_note += (int8_t)(patt_data->effect_arg & 0x0f);
			}
			oktalyzer_play_note(s, channel_num, chan_data, work_note);
			break;
		}

		case 11: {
			// Arpeggio type 2
			work_note = (int8_t)chan_data->curr_note;
			switch(s->speed_counter & 0x3) {
				case 0:
				case 2: {
					break;
				}
				case 1: {
					work_note += (int8_t)(patt_data->effect_arg & 0x0f);
					break;
				}
				case 3: {
					work_note -= (int8_t)((patt_data->effect_arg & 0xf0) >> 4);
					break;
				}
			}
			oktalyzer_play_note(s, channel_num, chan_data, work_note);
			break;
		}

		case 12: {
			// Arpeggio type 3
			work_note = (int8_t)chan_data->curr_note;
			arp_num = oktalyzer_arp12[s->speed_counter & 0x0f];
			if(arp_num == 0) {
				break;
			}
			if(arp_num == 1) {
				work_note -= (int8_t)((patt_data->effect_arg & 0xf0) >> 4);
			} else if(arp_num == 2) {
				work_note += (int8_t)(patt_data->effect_arg & 0x0f);
			}
			oktalyzer_play_note(s, channel_num, chan_data, work_note);
			break;
		}

		case 17: {
			// Increase note once per line: only on tick 0, fall through into 30.
			if(s->speed_counter != 0) {
				break;
			}
			chan_data->curr_note += patt_data->effect_arg;
			oktalyzer_play_note(s, channel_num, chan_data, (int8_t)chan_data->curr_note);
			break;
		}

		case 30: {
			// Increase note once per tick
			chan_data->curr_note += patt_data->effect_arg;
			oktalyzer_play_note(s, channel_num, chan_data, (int8_t)chan_data->curr_note);
			break;
		}

		case 21: {
			// Decrease note once per line: only on tick 0, fall through into 13.
			if(s->speed_counter != 0) {
				break;
			}
			chan_data->curr_note -= patt_data->effect_arg;
			oktalyzer_play_note(s, channel_num, chan_data, (int8_t)chan_data->curr_note);
			break;
		}

		case 13: {
			// Decrease note once per tick
			chan_data->curr_note -= patt_data->effect_arg;
			oktalyzer_play_note(s, channel_num, chan_data, (int8_t)chan_data->curr_note);
			break;
		}

		case 15: {
			// Filter control
			if(s->speed_counter == 0) {
				s->filter_status = (patt_data->effect_arg != 0) ? 1 : 0;
			}
			break;
		}

		case 25: {
			// Position jump
			if(s->speed_counter == 0) {
				new_pos = (uint16_t)(((patt_data->effect_arg & 0xf0) >> 4) * 10 + (patt_data->effect_arg & 0x0f));
				if(new_pos < s->song_length) {
					s->new_song_pos = (int16_t)new_pos;
				}
			}
			break;
		}

		case 27: {
			// Release sample: switch to release portion when current loop section finishes.
			if((chan_data->release_start != 0) && (chan_data->release_length != 0)) {
				struct oktalyzer_sample *samp = (patt_data->sample_num < s->samp_num) ? &s->samples[patt_data->sample_num] : 0;
				// SetSample takes effect via Paula's pending-buffer mechanism on next wrap.
				if(samp && samp->sample_data) {
					paula_queue_sample(&s->paula, (int32_t)channel_num, samp->sample_data, chan_data->release_start, chan_data->release_length);
					paula_set_loop(&s->paula, (int32_t)channel_num, chan_data->release_start, chan_data->release_length);
				}
			}
			break;
		}

		case 28: {
			// Set speed
			if((s->speed_counter == 0) && ((patt_data->effect_arg & 0xf) != 0)) {
				s->current_speed = (uint16_t)(patt_data->effect_arg & 0xf);
			}
			break;
		}

		case 24: {
			// Volume control with retrig: copy mirrored vol back, then fall through into V (31).
			s->chan_vol[s->chan_index[channel_num]] = s->chan_vol[s->chan_index[channel_num] + 4];
			// fall-through
		}
		// fall-through
		case 31: {
			// Volume control
			vol_index = s->chan_index[channel_num];
			eff_arg = patt_data->effect_arg;
			if(eff_arg <= 64) {
				s->chan_vol[vol_index] = (int8_t)eff_arg;
				break;
			}
			eff_arg -= 64;
			if(eff_arg < 16) {
				s->chan_vol[vol_index] -= (int8_t)eff_arg;
				if(s->chan_vol[vol_index] < 0) {
					s->chan_vol[vol_index] = 0;
				}
				break;
			}
			eff_arg -= 16;
			if(eff_arg < 16) {
				s->chan_vol[vol_index] += (int8_t)eff_arg;
				if(s->chan_vol[vol_index] > 64) {
					s->chan_vol[vol_index] = 64;
				}
				break;
			}
			eff_arg -= 16;
			if(eff_arg < 16) {
				if(s->speed_counter == 0) {
					s->chan_vol[vol_index] -= (int8_t)eff_arg;
					if(s->chan_vol[vol_index] < 0) {
						s->chan_vol[vol_index] = 0;
					}
				}
				break;
			}
			eff_arg -= 16;
			if(eff_arg < 16) {
				if(s->speed_counter == 0) {
					s->chan_vol[vol_index] += (int8_t)eff_arg;
					if(s->chan_vol[vol_index] > 64) {
						s->chan_vol[vol_index] = 64;
					}
				}
				break;
			}
			break;
		}
	}
}

// [=]===^=[ oktalyzer_do_effects ]================================================================[=]
static void oktalyzer_do_effects(struct oktalyzer_state *s) {
	for(uint32_t i = 0, j = 0; i < 4; ++i, ++j) {
		oktalyzer_do_channel_effect(s, j);
		if(s->channel_flags[i]) {
			oktalyzer_do_channel_effect(s, ++j);
		}
	}
}

// [=]===^=[ oktalyzer_set_volumes ]===============================================================[=]
static void oktalyzer_set_volumes(struct oktalyzer_state *s) {
	// Mirror the four primary volumes into 4..7 so effect 'O' has something to copy back.
	s->chan_vol[4] = s->chan_vol[0];
	s->chan_vol[5] = s->chan_vol[1];
	s->chan_vol[6] = s->chan_vol[2];
	s->chan_vol[7] = s->chan_vol[3];

	for(uint32_t i = 0, j = 0; i < 4; ++i, ++j) {
		paula_set_volume(&s->paula, (int32_t)j, (uint16_t)(uint8_t)s->chan_vol[i]);
		if(s->channel_flags[i]) {
			++j;
			paula_set_volume(&s->paula, (int32_t)j, (uint16_t)(uint8_t)s->chan_vol[i]);
		}
	}
}

// [=]===^=[ oktalyzer_tick ]======================================================================[=]
static void oktalyzer_tick(struct oktalyzer_state *s) {
	s->speed_counter++;
	if(s->speed_counter >= s->current_speed) {
		s->speed_counter = 0;
		oktalyzer_find_next_pattern_line(s);
		oktalyzer_play_pattern_line(s);
	}

	oktalyzer_do_effects(s);
	oktalyzer_set_volumes(s);
}

// [=]===^=[ oktalyzer_init ]======================================================================[=]
static struct oktalyzer_state *oktalyzer_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || (len < 1368) || (sample_rate < 8000)) {
		return 0;
	}

	struct oktalyzer_state *s = (struct oktalyzer_state *)calloc(1, sizeof(struct oktalyzer_state));
	if(!s) {
		return 0;
	}

	if(!oktalyzer_load(s, (uint8_t *)data, len)) {
		oktalyzer_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, OKTALYZER_TICK_HZ);
	oktalyzer_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ oktalyzer_free ]======================================================================[=]
static void oktalyzer_free(struct oktalyzer_state *s) {
	if(!s) {
		return;
	}
	oktalyzer_cleanup(s);
	free(s);
}

// [=]===^=[ oktalyzer_get_audio ]=================================================================[=]
static void oktalyzer_get_audio(struct oktalyzer_state *s, int16_t *output, int32_t frames) {
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
			oktalyzer_tick(s);
		}
	}
}

// [=]===^=[ oktalyzer_api_init ]==================================================================[=]
static void *oktalyzer_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return oktalyzer_init(data, len, sample_rate);
}

// [=]===^=[ oktalyzer_api_free ]==================================================================[=]
static void oktalyzer_api_free(void *state) {
	oktalyzer_free((struct oktalyzer_state *)state);
}

// [=]===^=[ oktalyzer_api_get_audio ]=============================================================[=]
static void oktalyzer_api_get_audio(void *state, int16_t *output, int32_t frames) {
	oktalyzer_get_audio((struct oktalyzer_state *)state, output, frames);
}

static const char *oktalyzer_extensions[] = { "okt", "okta", 0 };

static struct player_api oktalyzer_api = {
	"Oktalyzer",
	oktalyzer_extensions,
	oktalyzer_api_init,
	oktalyzer_api_free,
	oktalyzer_api_get_audio,
	0,
};
