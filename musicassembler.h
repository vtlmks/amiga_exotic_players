// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Music Assembler replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate (PAL).
//
// Public API:
//   struct musicassembler_state *musicassembler_init(void *data, uint32_t len, int32_t sample_rate);
//   void musicassembler_free(struct musicassembler_state *s);
//   void musicassembler_get_audio(struct musicassembler_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define MUSICASSEMBLER_TICK_HZ   50

#define MA_FLAG_NONE        0x00
#define MA_FLAG_SET_LOOP    0x02
#define MA_FLAG_SYNTHESIS   0x04
#define MA_FLAG_PORTAMENTO  0x20
#define MA_FLAG_RELEASE     0x40
#define MA_FLAG_RETRIG      0x80

struct ma_position_info {
	uint8_t track_number;
	uint8_t transpose;
	int8_t  repeat_counter;
};

struct ma_position_list {
	uint16_t key;            // original offset, used as identity
	struct ma_position_info *entries;
	uint32_t length;
};

struct ma_song_info {
	uint8_t  start_speed;
	uint16_t position_lists[4];
};

struct ma_instrument {
	uint8_t sample_number;
	uint8_t attack;
	uint8_t decay_sustain;
	uint8_t vibrato_delay;
	uint8_t release;
	uint8_t vibrato_speed;
	uint8_t vibrato_level;
	uint8_t arpeggio;
	uint8_t fx_arp_spd_lp;
	uint8_t hold;
	uint8_t key_wave_rate;
	uint8_t wave_level_speed;
};

struct ma_sample {
	int8_t  *sample_data;
	uint16_t length;          // in words (Amiga convention)
	uint16_t loop_length;
	uint8_t  is_empty;        // 1 if sample_data points to ma_empty_sample
};

struct ma_track {
	uint8_t *data;
	uint32_t length;
};

struct ma_voice_info {
	int32_t  channel_number;
	struct ma_position_info *position_list;
	uint32_t position_list_length;
	uint16_t current_position;
	uint16_t current_track_row;
	int8_t   track_repeat_counter;
	int8_t   row_delay_counter;
	uint8_t  flag;
	struct ma_instrument *current_instrument;
	uint8_t  current_note;
	uint8_t  transpose;
	uint8_t  volume;
	uint8_t  decrease_volume;
	uint8_t  sustain_counter;
	uint8_t  arpeggio_counter;
	uint8_t  arpeggio_value_to_use;
	uint8_t  portamento_or_vibrato_value;
	int16_t  portamento_add_value;
	uint8_t  vibrato_delay_counter;
	uint8_t  vibrato_direction;
	uint8_t  vibrato_speed_counter;
	int16_t  vibrato_add_value;
	uint16_t wave_length_modifier;
	uint8_t  wave_direction;
	uint8_t  wave_speed_counter;
	int8_t  *sample_data;
	uint32_t sample_start_offset;
	uint16_t sample_length;
};

struct musicassembler_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	int32_t  sub_song_count;
	int32_t  sub_song_speed_offset;
	int32_t  sub_song_position_list_offset;
	int32_t  module_start_offset;
	int32_t  instrument_info_offset_offset;
	int32_t  sample_info_offset_offset;
	int32_t  tracks_offset_offset;

	struct ma_song_info *sub_songs;
	uint32_t num_sub_songs;

	struct ma_position_list *position_lists;
	uint32_t num_position_lists;

	struct ma_track *tracks;
	uint32_t num_tracks;

	struct ma_instrument *instruments;
	uint32_t num_instruments;

	struct ma_sample *samples;
	uint32_t num_samples;

	uint16_t master_volume;
	uint8_t  speed;
	uint8_t  speed_counter;

	struct ma_voice_info voices[4];
};

// [=]===^=[ ma_periods ]=========================================================================[=]
static uint16_t ma_periods[48] = {
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  906,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113
};

// [=]===^=[ ma_channel_map ]=====================================================================[=]
static int32_t ma_channel_map[4] = { 0, 3, 1, 2 };

// [=]===^=[ ma_empty_sample ]====================================================================[=]
static int8_t ma_empty_sample[4] = { 0, 0, 0, 0 };

// [=]===^=[ ma_read_u16_be ]=====================================================================[=]
static uint16_t ma_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ ma_read_u32_be ]=====================================================================[=]
static uint32_t ma_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ ma_read_i32_be ]=====================================================================[=]
static int32_t ma_read_i32_be(uint8_t *p) {
	return (int32_t)ma_read_u32_be(p);
}

// [=]===^=[ ma_extract_init ]====================================================================[=]
static int32_t ma_extract_init(struct musicassembler_state *s, uint8_t *buf, int32_t len) {
	int32_t index;

	int32_t start_of_init = (((int32_t)(int8_t)buf[2] << 8) | buf[3]) + 2;
	if(start_of_init >= len) {
		return 0;
	}

	for(index = start_of_init; index < (len - 4); index += 2) {
		if((buf[index] == 0xb0) && (buf[index + 1] == 0x7c)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	s->sub_song_count = ((int32_t)buf[index + 2] << 8) | buf[index + 3];
	index += 4;

	for(; index < (len - 4); index += 2) {
		if((buf[index] == 0x49) && (buf[index + 1] == 0xfa)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	s->sub_song_speed_offset = (((int32_t)(int8_t)buf[index + 2] << 8) | buf[index + 3]) + index + 2;
	index += 4;

	for(; index < (len - 4); index += 2) {
		if((buf[index] == 0x49) && (buf[index + 1] == 0xfb)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	s->sub_song_position_list_offset = (int32_t)(int8_t)buf[index + 3] + index + 2;
	return 1;
}

// [=]===^=[ ma_extract_play ]====================================================================[=]
static int32_t ma_extract_play(struct musicassembler_state *s, uint8_t *buf, int32_t len) {
	int32_t index;
	int32_t start_of_play = 0x0c;

	for(index = start_of_play; index < (len - 4); index += 2) {
		if((buf[index] == 0x43) && (buf[index + 1] == 0xfa)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	s->module_start_offset = (((int32_t)(int8_t)buf[index + 2] << 8) | buf[index + 3]) + index + 2;
	index += 4;

	for(; index < (len - 8); index += 2) {
		if((buf[index] == 0xd3) && (buf[index + 1] == 0xfa)) {
			break;
		}
	}
	if(index >= (len - 8)) {
		return 0;
	}

	s->instrument_info_offset_offset = (((int32_t)(int8_t)buf[index + 2] << 8) | buf[index + 3]) + index + 2;

	if((buf[index + 4] != 0xd5) || (buf[index + 5] != 0xfa)) {
		return 0;
	}

	s->sample_info_offset_offset = (((int32_t)(int8_t)buf[index + 6] << 8) | buf[index + 7]) + index + 6;
	index += 8;

	for(; index < (len - 2); index += 2) {
		if(buf[index] == 0x61) {
			break;
		}
	}
	if(index >= (len - 2)) {
		return 0;
	}

	index = (int32_t)(int8_t)buf[index + 1] + index + 2;
	if(index >= len) {
		return 0;
	}

	for(; index < (len - 4); index += 2) {
		if((buf[index] == 0xdb) && (buf[index + 1] == 0xfa)) {
			break;
		}
	}
	if(index >= (len - 4)) {
		return 0;
	}

	s->tracks_offset_offset = (((int32_t)(int8_t)buf[index + 2] << 8) | buf[index + 3]) + index + 2;
	return 1;
}

// [=]===^=[ ma_test_module ]=====================================================================[=]
static int32_t ma_test_module(struct musicassembler_state *s, uint8_t *buf, int32_t len) {
	if(len < 0x622) {
		return 0;
	}
	if((buf[0] != 0x60) || (buf[1] != 0x00) || (buf[4] != 0x60) || (buf[5] != 0x00) ||
	   (buf[8] != 0x60) || (buf[9] != 0x00) || (buf[12] != 0x48) || (buf[13] != 0xe7)) {
		return 0;
	}
	if(!ma_extract_init(s, buf, len)) {
		return 0;
	}
	if(!ma_extract_play(s, buf, len)) {
		return 0;
	}
	return 1;
}

// [=]===^=[ ma_load_sub_songs ]==================================================================[=]
static int32_t ma_load_sub_songs(struct musicassembler_state *s) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;
	int32_t cnt = s->sub_song_count;

	if(cnt <= 0) {
		return 0;
	}
	if((uint32_t)(s->sub_song_speed_offset + cnt) > data_len) {
		return 0;
	}
	if((uint32_t)(s->sub_song_position_list_offset + cnt * 4 * 2) > data_len) {
		return 0;
	}

	uint8_t *speeds = data + s->sub_song_speed_offset;
	uint8_t *poslist_raw = data + s->sub_song_position_list_offset;

	s->sub_songs = (struct ma_song_info *)calloc((size_t)cnt, sizeof(struct ma_song_info));
	if(!s->sub_songs) {
		return 0;
	}
	s->num_sub_songs = 0;

	for(int32_t i = 0; i < cnt; ++i) {
		uint16_t v1 = ma_read_u16_be(poslist_raw + (i * 4 + 0) * 2);
		uint16_t v2 = ma_read_u16_be(poslist_raw + (i * 4 + 1) * 2);
		uint16_t v3 = ma_read_u16_be(poslist_raw + (i * 4 + 2) * 2);
		uint16_t v4 = ma_read_u16_be(poslist_raw + (i * 4 + 3) * 2);

		if(((v1 + 2) == v2) && ((v2 + 2) == v3) && ((v3 + 2) == v4)) {
			continue;
		}

		struct ma_song_info *si = &s->sub_songs[s->num_sub_songs++];
		si->start_speed = speeds[i];
		si->position_lists[0] = v1;
		si->position_lists[1] = v2;
		si->position_lists[2] = v3;
		si->position_lists[3] = v4;
	}

	if(s->num_sub_songs == 0) {
		return 0;
	}
	return 1;
}

// [=]===^=[ ma_find_position_list ]==============================================================[=]
static struct ma_position_list *ma_find_position_list(struct musicassembler_state *s, uint16_t key) {
	for(uint32_t i = 0; i < s->num_position_lists; ++i) {
		if(s->position_lists[i].key == key) {
			return &s->position_lists[i];
		}
	}
	return 0;
}

// [=]===^=[ ma_load_single_position_list ]=======================================================[=]
static int32_t ma_load_single_position_list(struct musicassembler_state *s, uint32_t start, struct ma_position_list *out) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	uint32_t pos = start;
	uint32_t capacity = 16;
	struct ma_position_info *list = (struct ma_position_info *)malloc(capacity * sizeof(struct ma_position_info));
	if(!list) {
		return 0;
	}
	uint32_t count = 0;

	for(;;) {
		if(pos + 2 > data_len) {
			free(list);
			return 0;
		}
		if(count == capacity) {
			capacity *= 2;
			struct ma_position_info *nlist = (struct ma_position_info *)realloc(list, capacity * sizeof(struct ma_position_info));
			if(!nlist) {
				free(list);
				return 0;
			}
			list = nlist;
		}

		uint8_t track_number = data[pos++];
		uint8_t byt = data[pos++];

		uint16_t val = (uint16_t)(byt << 4);
		uint8_t low = (uint8_t)((val & 0xff) >> 1);

		struct ma_position_info *pi = &list[count++];
		pi->track_number = track_number;
		pi->transpose = (uint8_t)(val >> 8);
		pi->repeat_counter = (int8_t)low;

		if((track_number == 0xff) || (track_number == 0xfe)) {
			break;
		}
	}

	out->entries = list;
	out->length = count;
	return 1;
}

// [=]===^=[ ma_load_position_lists ]=============================================================[=]
static int32_t ma_load_position_lists(struct musicassembler_state *s) {
	// Worst case: 4 unique keys per sub-song.
	uint32_t capacity = s->num_sub_songs * 4;
	s->position_lists = (struct ma_position_list *)calloc(capacity, sizeof(struct ma_position_list));
	if(!s->position_lists) {
		return 0;
	}
	s->num_position_lists = 0;

	for(uint32_t i = 0; i < s->num_sub_songs; ++i) {
		struct ma_song_info *song = &s->sub_songs[i];
		for(int32_t v = 0; v < 4; ++v) {
			uint16_t key = song->position_lists[v];
			if(ma_find_position_list(s, key)) {
				continue;
			}

			uint32_t start = (uint32_t)s->module_start_offset + key;
			if(start >= s->module_len) {
				return 0;
			}

			struct ma_position_list *pl = &s->position_lists[s->num_position_lists];
			pl->key = key;
			if(!ma_load_single_position_list(s, start, pl)) {
				return 0;
			}
			s->num_position_lists++;
		}
	}
	return 1;
}

// [=]===^=[ ma_compute_track_count ]=============================================================[=]
static uint32_t ma_compute_track_count(struct musicassembler_state *s) {
	int32_t maximum = -1;
	for(uint32_t i = 0; i < s->num_position_lists; ++i) {
		struct ma_position_list *pl = &s->position_lists[i];
		for(uint32_t j = 0; j < pl->length; ++j) {
			uint8_t tn = pl->entries[j].track_number;
			if((tn != 0xfe) && (tn != 0xff)) {
				if((int32_t)tn > maximum) {
					maximum = (int32_t)tn;
				}
			}
		}
	}
	if(maximum < 0) {
		return 0;
	}
	return (uint32_t)(maximum + 1);
}

// [=]===^=[ ma_load_single_track ]===============================================================[=]
static int32_t ma_load_single_track(struct musicassembler_state *s, uint32_t start, struct ma_track *out) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	uint32_t pos = start;
	uint32_t capacity = 32;
	uint8_t *buf = (uint8_t *)malloc(capacity);
	if(!buf) {
		return 0;
	}
	uint32_t length = 0;

	for(;;) {
		if(length + 6 > capacity) {
			capacity *= 2;
			uint8_t *nbuf = (uint8_t *)realloc(buf, capacity);
			if(!nbuf) {
				free(buf);
				return 0;
			}
			buf = nbuf;
		}

		if(pos >= data_len) {
			free(buf);
			return 0;
		}
		uint8_t byt = data[pos++];
		buf[length++] = byt;

		if((byt & 0x80) != 0) {
			if((byt & 0x40) != 0) {
				if(pos >= data_len) {
					free(buf);
					return 0;
				}
				byt = data[pos++];
				buf[length++] = byt;

				if(pos >= data_len) {
					free(buf);
					return 0;
				}
				byt = data[pos++];
				buf[length++] = byt;

				if((byt & 0x80) != 0) {
					if(pos >= data_len) {
						free(buf);
						return 0;
					}
					byt = data[pos++];
					buf[length++] = byt;
				}
			}
		} else {
			if(pos >= data_len) {
				free(buf);
				return 0;
			}
			byt = data[pos++];
			buf[length++] = byt;

			if((byt & 0x80) != 0) {
				if(pos >= data_len) {
					free(buf);
					return 0;
				}
				byt = data[pos++];
				buf[length++] = byt;
			}
		}

		if(pos >= data_len) {
			free(buf);
			return 0;
		}
		uint8_t next_byte = data[pos];

		if(next_byte == 0xff) {
			buf[length++] = next_byte;
			break;
		}
		// stay at pos: do not consume next_byte
	}

	out->data = buf;
	out->length = length;
	return 1;
}

// [=]===^=[ ma_load_tracks ]=====================================================================[=]
static int32_t ma_load_tracks(struct musicassembler_state *s) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	uint32_t num_tracks = ma_compute_track_count(s);
	if(num_tracks == 0) {
		return 0;
	}

	if((uint32_t)(s->tracks_offset_offset + 4) > data_len) {
		return 0;
	}
	int32_t tracks_start_offset = ma_read_i32_be(data + s->tracks_offset_offset) + s->module_start_offset;

	if((uint32_t)(s->module_start_offset + num_tracks * 2) > data_len) {
		return 0;
	}
	uint8_t *table = data + s->module_start_offset;

	s->tracks = (struct ma_track *)calloc(num_tracks, sizeof(struct ma_track));
	if(!s->tracks) {
		return 0;
	}
	s->num_tracks = num_tracks;

	for(uint32_t i = 0; i < num_tracks; ++i) {
		uint16_t track_offset = ma_read_u16_be(table + i * 2);
		uint32_t start = (uint32_t)tracks_start_offset + track_offset;
		if(start >= data_len) {
			return 0;
		}
		if(!ma_load_single_track(s, start, &s->tracks[i])) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ ma_load_instruments ]================================================================[=]
static int32_t ma_load_instruments(struct musicassembler_state *s) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	if((uint32_t)(s->instrument_info_offset_offset + 4) > data_len) {
		return 0;
	}
	int32_t instr_start = ma_read_i32_be(data + s->instrument_info_offset_offset);

	if((uint32_t)(s->sample_info_offset_offset + 4) > data_len) {
		return 0;
	}
	int32_t sample_start = ma_read_i32_be(data + s->sample_info_offset_offset);

	int32_t num = (sample_start - instr_start) / 16;
	if(num <= 0) {
		return 0;
	}

	s->instruments = (struct ma_instrument *)calloc((size_t)num, sizeof(struct ma_instrument));
	if(!s->instruments) {
		return 0;
	}
	s->num_instruments = (uint32_t)num;

	uint32_t pos = (uint32_t)(s->module_start_offset + instr_start);
	if(pos + (uint32_t)num * 16 > data_len) {
		return 0;
	}

	for(int32_t i = 0; i < num; ++i) {
		struct ma_instrument *ins = &s->instruments[i];
		uint8_t *p = data + pos;
		ins->sample_number    = p[0];
		ins->attack           = p[1];
		ins->decay_sustain    = p[2];
		ins->vibrato_delay    = p[3];
		ins->release          = p[4];
		ins->vibrato_speed    = p[5];
		ins->vibrato_level    = p[6];
		ins->arpeggio         = p[7];
		ins->fx_arp_spd_lp    = p[8];
		ins->hold             = p[9];
		ins->key_wave_rate    = p[10];
		ins->wave_level_speed = p[11];
		// 4 reserved bytes follow
		pos += 16;
	}
	return 1;
}

// [=]===^=[ ma_min_position_list_key ]===========================================================[=]
static uint16_t ma_min_position_list_key(struct musicassembler_state *s) {
	uint16_t mn = 0xffff;
	for(uint32_t i = 0; i < s->num_position_lists; ++i) {
		if(s->position_lists[i].key < mn) {
			mn = s->position_lists[i].key;
		}
	}
	return mn;
}

// [=]===^=[ ma_load_samples ]====================================================================[=]
static int32_t ma_load_samples(struct musicassembler_state *s) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	if((uint32_t)(s->sample_info_offset_offset + 4) > data_len) {
		return 0;
	}
	int32_t sample_start_offset = ma_read_i32_be(data + s->sample_info_offset_offset);
	sample_start_offset += s->module_start_offset;

	uint16_t min_pl_key = ma_min_position_list_key(s);
	int32_t num = ((int32_t)min_pl_key + s->module_start_offset - sample_start_offset) / 24;
	if(num <= 0) {
		return 0;
	}

	s->samples = (struct ma_sample *)calloc((size_t)num, sizeof(struct ma_sample));
	if(!s->samples) {
		return 0;
	}
	s->num_samples = (uint32_t)num;

	uint32_t pos = (uint32_t)sample_start_offset;
	if(pos + (uint32_t)num * 24 > data_len) {
		return 0;
	}

	for(int32_t i = 0; i < num; ++i) {
		struct ma_sample *smp = &s->samples[i];
		uint8_t *p = data + pos;
		int32_t offset = ma_read_i32_be(p + 0);
		smp->length      = ma_read_u16_be(p + 4);
		smp->loop_length = ma_read_u16_be(p + 6);
		// 16-byte name follows but is unused for playback
		pos += 24;

		if(offset < 0) {
			smp->sample_data = ma_empty_sample;
			smp->length = 1;
			smp->is_empty = 1;
		} else {
			uint32_t sd_pos = (uint32_t)(sample_start_offset + offset);
			uint32_t sd_len = (uint32_t)smp->length * 2;
			if(sd_pos + sd_len > data_len) {
				return 0;
			}
			smp->sample_data = (int8_t *)(data + sd_pos);
			smp->is_empty = 0;
		}
	}
	return 1;
}

// [=]===^=[ ma_initialize_sound ]================================================================[=]
static void ma_initialize_sound(struct musicassembler_state *s, uint32_t sub_song) {
	if(sub_song >= s->num_sub_songs) {
		sub_song = 0;
	}
	struct ma_song_info *song = &s->sub_songs[sub_song];

	s->master_volume = 64;
	s->speed = song->start_speed;
	s->speed_counter = 1;

	for(int32_t i = 0; i < 4; ++i) {
		struct ma_voice_info *v = &s->voices[i];
		memset(v, 0, sizeof(*v));

		uint16_t key = song->position_lists[ma_channel_map[i]];
		struct ma_position_list *pl = ma_find_position_list(s, key);

		v->channel_number = i;
		v->position_list = pl ? pl->entries : 0;
		v->position_list_length = pl ? pl->length : 0;
		v->current_position = 0;
		v->current_track_row = 0;
		v->track_repeat_counter = (pl && pl->length > 0) ? pl->entries[0].repeat_counter : 0;
		v->row_delay_counter = 0;
		v->flag = MA_FLAG_NONE;
		v->current_instrument = (s->num_instruments > 0) ? &s->instruments[0] : 0;
		v->current_note = 0;
		v->transpose = (pl && pl->length > 0) ? pl->entries[0].transpose : 0;
		v->volume = 0;
		v->decrease_volume = 0;
		v->sustain_counter = 0;
		v->arpeggio_counter = 0;
		v->arpeggio_value_to_use = 0;
		v->portamento_or_vibrato_value = 0;
		v->portamento_add_value = 0;
		v->vibrato_delay_counter = 0;
		v->vibrato_direction = 0;
		v->vibrato_speed_counter = 0;
		v->vibrato_add_value = 0;
		v->wave_length_modifier = 0;
		v->wave_direction = 0;
		v->wave_speed_counter = 0;
		v->sample_data = 0;
		v->sample_start_offset = 0;
		v->sample_length = 0;
	}
}

// [=]===^=[ ma_cleanup ]=========================================================================[=]
static void ma_cleanup(struct musicassembler_state *s) {
	if(!s) {
		return;
	}
	free(s->sub_songs); s->sub_songs = 0;
	if(s->position_lists) {
		for(uint32_t i = 0; i < s->num_position_lists; ++i) {
			free(s->position_lists[i].entries);
		}
		free(s->position_lists);
		s->position_lists = 0;
	}
	if(s->tracks) {
		for(uint32_t i = 0; i < s->num_tracks; ++i) {
			free(s->tracks[i].data);
		}
		free(s->tracks);
		s->tracks = 0;
	}
	free(s->instruments); s->instruments = 0;
	free(s->samples); s->samples = 0;
}

// [=]===^=[ ma_force_retrigger ]=================================================================[=]
static void ma_force_retrigger(struct musicassembler_state *s, struct ma_voice_info *v) {
	v->flag &= MA_FLAG_RELEASE;
	v->flag |= MA_FLAG_RETRIG;
	paula_mute(&s->paula, v->channel_number);
}

// [=]===^=[ ma_initialize_voice ]================================================================[=]
static void ma_initialize_voice(struct ma_voice_info *v) {
	struct ma_instrument *instr = v->current_instrument;
	v->decrease_volume = 0;
	v->arpeggio_value_to_use = 0;
	v->volume = 0;
	v->vibrato_direction = 0;
	v->vibrato_speed_counter = 0;
	v->vibrato_add_value = 0;
	v->flag = MA_FLAG_NONE;

	if(instr) {
		v->vibrato_delay_counter = instr->vibrato_delay;
		v->arpeggio_counter = instr->fx_arp_spd_lp;
		v->sustain_counter = instr->hold;
	}
}

// [=]===^=[ ma_get_next_row_in_track ]===========================================================[=]
// Returns 1 if play tick should still call DoVoice (mirrors C# return value).
static int32_t ma_get_next_row_in_track(struct musicassembler_state *s, struct ma_voice_info *v) {
	v->row_delay_counter--;

	if(v->row_delay_counter >= 0) {
		return 1;
	}

	struct ma_position_info *pi = &v->position_list[v->current_position];

	if(pi->track_number == 0xfe) {
		return 1;
	}

	if(pi->track_number >= s->num_tracks) {
		return 1;
	}

	struct ma_track *track = &s->tracks[pi->track_number];
	uint8_t *track_data = track->data;
	uint32_t track_row = v->current_track_row;

	uint8_t track_byte = track_data[track_row++];

	if((track_byte & 0x80) == 0) {
		if((track_byte & 0x40) == 0) {
			ma_initialize_voice(v);
			ma_force_retrigger(s, v);
		}

		// SetNote
		v->portamento_add_value = 0;
		v->current_note = (uint8_t)((track_byte & 0x3f) + v->transpose);

		track_byte = track_data[track_row++];

		if((track_byte & 0x80) == 0) {
			v->portamento_or_vibrato_value = 0;
		} else {
			track_byte &= 0x7f;
			v->portamento_or_vibrato_value = track_data[track_row++];
		}
	} else {
		if((track_byte & 0x40) == 0) {
			track_byte &= 0x3f;
			v->flag |= MA_FLAG_RELEASE;
			v->sustain_counter = 0;
		} else {
			uint8_t instr_idx = (uint8_t)(track_byte & 0x3f);
			if(instr_idx < s->num_instruments) {
				v->current_instrument = &s->instruments[instr_idx];
			}

			track_byte = track_data[track_row++];

			if((track_byte & 0x40) == 0) {
				v->wave_length_modifier = 0;
				v->wave_direction = 0;
				v->wave_speed_counter = 1;
				ma_initialize_voice(v);
			}

			ma_force_retrigger(s, v);

			// SetNote
			v->portamento_add_value = 0;
			v->current_note = (uint8_t)((track_byte & 0x3f) + v->transpose);

			track_byte = track_data[track_row++];

			if((track_byte & 0x80) == 0) {
				v->portamento_or_vibrato_value = 0;
			} else {
				track_byte &= 0x7f;
				v->portamento_or_vibrato_value = track_data[track_row++];
			}
		}
	}

	v->row_delay_counter = (int8_t)track_byte;

	if(track_data[track_row] == 0xff) {
		v->track_repeat_counter -= 8;

		if(v->track_repeat_counter < 0) {
			v->current_position++;

			if(v->current_position >= v->position_list_length) {
				v->current_position = 0;
			}

			pi = &v->position_list[v->current_position];

			if(pi->track_number == 0xff) {
				v->current_position = 0;
				pi = &v->position_list[0];
			}

			v->transpose = pi->transpose;
			v->track_repeat_counter = pi->repeat_counter;
		}

		track_row = 0;
	}

	v->current_track_row = (uint16_t)track_row;
	return 0;
}

// [=]===^=[ ma_do_wave_length_modifying ]========================================================[=]
static void ma_do_wave_length_modifying(struct ma_instrument *instr, struct ma_sample *sample, struct ma_voice_info *v) {
	int8_t  wave_length = (int8_t)((int8_t)instr->wave_level_speed >> 4);
	uint8_t wave_speed  = (uint8_t)(instr->wave_level_speed & 0x0f);

	if(wave_speed != 0) {
		wave_speed *= 2;
		v->wave_speed_counter++;

		if(wave_speed < v->wave_speed_counter) {
			v->wave_speed_counter = 0;
			v->wave_direction = !v->wave_direction;
		}

		if(v->wave_direction) {
			wave_length = (int8_t)-wave_length;
		}
	}

	v->wave_length_modifier = (uint16_t)(v->wave_length_modifier + (uint16_t)(int16_t)wave_length);

	uint16_t length = sample->length;
	uint32_t mask = (length > 0) ? (uint32_t)(length - 1) : 0;
	uint32_t start_offset = (((uint32_t)v->wave_length_modifier / 4u) + (uint32_t)instr->key_wave_rate) & mask;

	v->sample_data = sample->sample_data;
	v->sample_start_offset = start_offset;
}

// [=]===^=[ ma_setup_sample ]====================================================================[=]
static void ma_setup_sample(struct musicassembler_state *s, struct ma_instrument *instr, struct ma_voice_info *v) {
	if(instr->sample_number >= s->num_samples) {
		return;
	}
	struct ma_sample *sample = &s->samples[instr->sample_number];
	int32_t set_sample = 0;

	if(v->flag & MA_FLAG_SET_LOOP) {
		v->flag &= ~MA_FLAG_SET_LOOP;

		if(sample->loop_length != 0) {
			v->sample_length = sample->loop_length;
			set_sample = 1;

			if(sample->length <= 128) {
				ma_do_wave_length_modifying(instr, sample, v);
			} else {
				v->sample_start_offset = (uint32_t)(sample->length - sample->loop_length) * 2u;
				v->sample_data = sample->sample_data;
			}
		} else {
			v->sample_data = ma_empty_sample;
			v->sample_start_offset = 0;
			v->sample_length = 2;
			set_sample = 1;
			v->flag &= ~MA_FLAG_SYNTHESIS;
		}
	} else if(v->flag & MA_FLAG_RETRIG) {
		v->flag &= MA_FLAG_RELEASE;
		v->flag |= MA_FLAG_SET_LOOP;

		int8_t  *sample_data = sample->sample_data;
		uint32_t start_offset = 0;
		uint16_t sample_length = sample->length;
		uint16_t loop_length = sample->loop_length;
		uint8_t  key_wave_rate = instr->key_wave_rate;

		if((sample_length <= 128) && (sample_data != ma_empty_sample)) {
			v->flag |= MA_FLAG_SYNTHESIS;
			start_offset = key_wave_rate;
			sample_length = loop_length;
			if(sample_length == 0) {
				sample_length = 1;
			}
		}

		v->sample_data = sample_data;
		v->sample_start_offset = start_offset;
		v->sample_length = sample_length;
		set_sample = 1;
	} else if(v->flag & MA_FLAG_SYNTHESIS) {
		ma_do_wave_length_modifying(instr, sample, v);
		set_sample = 1;
	}

	if(set_sample) {
		uint32_t byte_len = (uint32_t)v->sample_length * 2u;
		paula_queue_sample(&s->paula, v->channel_number, v->sample_data, v->sample_start_offset, byte_len);
		if(v->sample_data != ma_empty_sample) {
			paula_set_loop(&s->paula, v->channel_number, v->sample_start_offset, byte_len);
		}
	}
}

// [=]===^=[ ma_do_arpeggio ]=====================================================================[=]
static uint16_t ma_do_arpeggio(struct ma_instrument *instr, struct ma_voice_info *v) {
	uint8_t arp = instr->arpeggio;
	int8_t  arp_to_use = (int8_t)((int8_t)v->arpeggio_value_to_use - 2);

	if(arp != 0) {
		if(arp_to_use == 0) {
			arp >>= 4;
		} else if(arp_to_use < 0) {
			arp = 0;
		} else {
			arp &= 0x0f;
		}
	}

	if(instr->fx_arp_spd_lp != 0) {
		v->arpeggio_counter += 0x10;

		if(v->arpeggio_counter >= instr->fx_arp_spd_lp) {
			v->arpeggio_counter = 0;
			uint8_t spd = (uint8_t)(instr->fx_arp_spd_lp & 0x03);

			if(spd == 0) {
				v->decrease_volume = !v->decrease_volume;
				spd = 1;
			}

			arp_to_use += 3;

			if(arp_to_use >= 4) {
				arp_to_use = (int8_t)spd;
			}

			v->arpeggio_value_to_use = (uint8_t)arp_to_use;
		}
	}

	uint8_t note = (uint8_t)(v->current_note + arp);

	if(!(v->flag & MA_FLAG_SYNTHESIS)) {
		note = (uint8_t)(note + instr->key_wave_rate);
	}

	if(note >= 48) {
		note = 47;
	}

	return ma_periods[note];
}

// [=]===^=[ ma_do_portamento ]===================================================================[=]
static uint16_t ma_do_portamento(uint16_t period, struct ma_voice_info *v) {
	if(v->portamento_or_vibrato_value != 0) {
		uint8_t flag_set = (v->flag & MA_FLAG_PORTAMENTO) != 0;
		v->flag ^= MA_FLAG_PORTAMENTO;

		if(!flag_set || ((v->portamento_or_vibrato_value & 0x01) == 0)) {
			period <<= 1;
			period = (uint16_t)(period + (uint16_t)v->portamento_add_value);
			period >>= 1;

			if((v->portamento_or_vibrato_value & 0x80) != 0) {
				int16_t delta = (int16_t)(((-(int8_t)v->portamento_or_vibrato_value) + 1) >> 1);
				v->portamento_add_value = (int16_t)(v->portamento_add_value + delta);
			} else {
				int16_t delta = (int16_t)(((int16_t)v->portamento_or_vibrato_value + 1) >> 1);
				v->portamento_add_value = (int16_t)(v->portamento_add_value - delta);
			}
		}
	}
	return period;
}

// [=]===^=[ ma_do_vibrato ]======================================================================[=]
static uint16_t ma_do_vibrato(uint16_t period, struct ma_instrument *instr, struct ma_voice_info *v) {
	if(v->vibrato_delay_counter == 0) {
		if((v->portamento_or_vibrato_value == 0) && (instr->vibrato_level != 0)) {
			int8_t  level = (int8_t)instr->vibrato_level;
			uint8_t direction = (uint8_t)(v->vibrato_direction & 0x03);

			if((direction != 0) && (direction != 3)) {
				level = (int8_t)-level;
			}

			v->vibrato_add_value = (int16_t)(v->vibrato_add_value + (int16_t)level);
			v->vibrato_speed_counter++;

			if(v->vibrato_speed_counter == instr->vibrato_speed) {
				v->vibrato_speed_counter = 0;
				v->vibrato_direction++;
			}

			period <<= 1;
			period = (uint16_t)(period + (uint16_t)v->vibrato_add_value);
			period >>= 1;
		}
	} else {
		v->vibrato_delay_counter--;
	}
	return period;
}

// [=]===^=[ ma_do_adsr ]=========================================================================[=]
static void ma_do_adsr(struct ma_instrument *instr, struct ma_voice_info *v) {
	if(v->flag & MA_FLAG_RELEASE) {
		if(v->sustain_counter == 0) {
			int32_t new_volume = (int32_t)v->volume - (int32_t)instr->release;
			if(new_volume < 0) {
				new_volume = 0;
			}
			v->volume = (uint8_t)new_volume;
		} else {
			v->sustain_counter--;
		}
	} else {
		int32_t new_volume = (int32_t)v->volume + (int32_t)instr->attack;
		int32_t decay_sustain = ((int32_t)instr->decay_sustain & 0xf0) | 0x0f;

		if(new_volume >= decay_sustain) {
			v->flag |= MA_FLAG_RELEASE;
			new_volume = (int32_t)instr->decay_sustain << 4;
		}

		v->volume = (uint8_t)new_volume;
	}
}

// [=]===^=[ ma_do_voice ]========================================================================[=]
static void ma_do_voice(struct musicassembler_state *s, struct ma_voice_info *v) {
	struct ma_instrument *instr = v->current_instrument;
	if(!instr) {
		return;
	}

	ma_setup_sample(s, instr, v);

	uint16_t period = ma_do_arpeggio(instr, v);
	period = ma_do_portamento(period, v);
	period = ma_do_vibrato(period, instr, v);

	ma_do_adsr(instr, v);

	uint16_t volume = (uint16_t)(((uint32_t)v->volume * (uint32_t)s->master_volume) / 256u);

	if((v->vibrato_delay_counter == 0) && v->decrease_volume) {
		volume /= 4;
	}

	if(period < 127) {
		period = 127;
	}

	paula_set_volume(&s->paula, v->channel_number, volume);
	paula_set_period(&s->paula, v->channel_number, period);
}

// [=]===^=[ ma_play ]============================================================================[=]
static void ma_play(struct musicassembler_state *s) {
	s->speed_counter--;

	if(s->speed_counter == 0) {
		s->speed_counter = s->speed;

		for(int32_t i = 0; i < 4; ++i) {
			struct ma_voice_info *v = &s->voices[i];
			if(v->position_list == 0) {
				continue;
			}
			if(ma_get_next_row_in_track(s, v)) {
				ma_do_voice(s, v);
			}
		}
	} else {
		for(int32_t i = 0; i < 4; ++i) {
			struct ma_voice_info *v = &s->voices[i];
			if(v->position_list == 0) {
				continue;
			}
			ma_do_voice(s, v);
		}
	}
}

// [=]===^=[ musicassembler_init ]================================================================[=]
static struct musicassembler_state *musicassembler_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 0x622 || sample_rate < 8000) {
		return 0;
	}

	struct musicassembler_state *s = (struct musicassembler_state *)calloc(1, sizeof(struct musicassembler_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;

	if(!ma_test_module(s, s->module_data, (int32_t)len)) {
		free(s);
		return 0;
	}

	if(!ma_load_sub_songs(s)) {
		goto fail;
	}
	if(!ma_load_position_lists(s)) {
		goto fail;
	}
	if(!ma_load_tracks(s)) {
		goto fail;
	}
	if(!ma_load_instruments(s)) {
		goto fail;
	}
	if(!ma_load_samples(s)) {
		goto fail;
	}

	paula_init(&s->paula, sample_rate, MUSICASSEMBLER_TICK_HZ);
	ma_initialize_sound(s, 0);
	return s;

fail:
	ma_cleanup(s);
	free(s);
	return 0;
}

// [=]===^=[ musicassembler_free ]================================================================[=]
static void musicassembler_free(struct musicassembler_state *s) {
	if(!s) {
		return;
	}
	ma_cleanup(s);
	free(s);
}

// [=]===^=[ musicassembler_get_audio ]===========================================================[=]
static void musicassembler_get_audio(struct musicassembler_state *s, int16_t *output, int32_t frames) {
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
			ma_play(s);
		}
	}
}

// [=]===^=[ musicassembler_api_init ]============================================================[=]
static void *musicassembler_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return musicassembler_init(data, len, sample_rate);
}

// [=]===^=[ musicassembler_api_free ]============================================================[=]
static void musicassembler_api_free(void *state) {
	musicassembler_free((struct musicassembler_state *)state);
}

// [=]===^=[ musicassembler_api_get_audio ]=======================================================[=]
static void musicassembler_api_get_audio(void *state, int16_t *output, int32_t frames) {
	musicassembler_get_audio((struct musicassembler_state *)state, output, frames);
}

static const char *musicassembler_extensions[] = { "ma", 0 };

static struct player_api musicassembler_api = {
	"Music Assembler",
	musicassembler_extensions,
	musicassembler_api_init,
	musicassembler_api_free,
	musicassembler_api_get_audio,
	0,
};
