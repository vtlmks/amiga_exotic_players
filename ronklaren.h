// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Ron Klaren replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Ron Klaren modules are ripped from m68k executables (Amiga hunk format),
// so the loader must scan the player code to discover the layout (sub-song
// table, IRQ routine, instrument table, arpeggio table, etc). The scanner
// here mirrors the C# Identify and Find* logic byte-for-byte.
//
// Public API:
//   struct ronklaren_state *ronklaren_init(void *data, uint32_t len, int32_t sample_rate);
//   void ronklaren_free(struct ronklaren_state *s);
//   void ronklaren_get_audio(struct ronklaren_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define RK_TICK_HZ            50
#define RK_HEADER_SIZE        32
#define RK_HUNK_SIZE          32
#define RK_MIN_FILE_SIZE      0xa40
#define RK_NUM_PERIODS        70
#define RK_INSTR_RECORD_SIZE  31
#define RK_NUM_VOICES         4

enum {
	RK_EFFECT_SET_ARPEGGIO     = 0x80,
	RK_EFFECT_SET_PORTAMENTO   = 0x81,
	RK_EFFECT_SET_INSTRUMENT   = 0x82,
	RK_EFFECT_END_SONG         = 0x83,
	RK_EFFECT_CHANGE_ADSR      = 0x84,
	RK_EFFECT_END_SONG_2       = 0x85,
	RK_EFFECT_END_OF_TRACK     = 0xff,
};

enum {
	RK_INSTR_TYPE_SYNTHESIS = 0,
	RK_INSTR_TYPE_SAMPLE    = 1,
};

struct rk_adsr_point {
	uint8_t point;
	uint8_t increment;
};

struct rk_instrument {
	int32_t sample_number;          // index into samples[] (post-resolve); pre-resolve: file offset
	int32_t vibrato_number;         // index into vibratos[] or -1; pre-resolve: file offset
	uint8_t type;                   // RK_INSTR_TYPE_*
	uint8_t phase_speed;
	uint8_t phase_length_in_words;
	uint8_t vibrato_speed;
	uint8_t vibrato_depth;
	uint8_t vibrato_delay;
	struct rk_adsr_point adsr[4];
	int8_t phase_value;
	uint8_t phase_direction;        // 0 = forward (++), 1 = backward (--)
	uint8_t phase_position;
};

struct rk_sample {
	int32_t sample_number;          // index into sample_data[] (post-resolve); pre-resolve: file offset
	uint16_t length_in_words;
	uint16_t phase_index;
};

struct rk_track_entry {
	int32_t track_number;
	int16_t transpose;
	uint16_t number_of_repeat_times;
};

struct rk_position_list {
	struct rk_track_entry *tracks;
	uint32_t num_tracks;
};

struct rk_song_info {
	struct rk_position_list positions[RK_NUM_VOICES];
};

struct rk_byte_seq {
	uint8_t *data;
	uint32_t length;
};

struct rk_sbyte_seq {
	int8_t *data;
	uint32_t length;
};

struct rk_offset_index {
	int32_t offset;
	int32_t index;
};

struct rk_voice_info {
	int32_t channel_number;

	struct rk_position_list *position_list;
	uint32_t track_list_position;

	uint8_t *track_data;
	uint32_t track_data_length;
	uint32_t track_data_position;
	uint16_t track_repeat_counter;
	uint8_t wait_counter;

	int32_t instrument_number;      // index into instruments[]

	int8_t *arpeggio_values;        // points into arpeggios[n].data, length 12
	uint16_t arpeggio_position;

	uint8_t current_note;
	int16_t transpose;
	uint16_t period;

	uint16_t portamento_end_period;
	uint8_t portamento_increment;

	uint8_t vibrato_delay;
	uint16_t vibrato_position;

	uint16_t adsr_state;
	uint8_t adsr_speed;
	int8_t adsr_speed_counter;
	uint8_t volume;

	uint8_t phase_speed_counter;

	uint8_t set_hardware;
	int16_t sample_number;
	int8_t *sample_data;
	uint32_t sample_length;
	uint8_t set_loop;
};

struct ronklaren_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint16_t cia_value;
	uint8_t clear_adsr_state_on_portamento;

	int32_t current_song;

	uint8_t setup_new_sub_song;
	uint16_t sub_song_number;
	uint8_t global_volume;

	struct rk_song_info *sub_songs;
	uint32_t num_sub_songs;

	struct rk_byte_seq *tracks;
	uint32_t num_tracks;

	struct rk_sbyte_seq *arpeggios;
	uint32_t num_arpeggios;

	struct rk_sbyte_seq *vibratos;
	uint32_t num_vibratos;

	struct rk_instrument *instruments;
	uint32_t num_instruments;

	struct rk_sample *samples;
	uint32_t num_samples;

	struct rk_sbyte_seq *sample_data;     // owned writable copies (mutated by phasing)
	uint32_t num_sample_data;

	struct rk_voice_info voices[RK_NUM_VOICES];
};

// [=]===^=[ rk_periods ]=========================================================================[=]
static uint16_t rk_periods[RK_NUM_PERIODS] = {
	6848, 6464, 6096, 5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3616,
	3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1808,
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  904,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  452,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127
};

// [=]===^=[ rk_read_b_u16 ]======================================================================[=]
static uint16_t rk_read_b_u16(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ rk_read_b_u32 ]======================================================================[=]
static uint32_t rk_read_b_u32(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ rk_read_b_i32 ]======================================================================[=]
static int32_t rk_read_b_i32(uint8_t *p) {
	return (int32_t)rk_read_b_u32(p);
}

// [=]===^=[ rk_read_b_i16 ]======================================================================[=]
static int16_t rk_read_b_i16(uint8_t *p) {
	return (int16_t)rk_read_b_u16(p);
}

// [=]===^=[ rk_identify ]========================================================================[=]
// Returns 1 if the buffer looks like a Ron Klaren module, 0 otherwise.
static int32_t rk_identify(uint8_t *data, uint32_t len) {
	if(len < RK_MIN_FILE_SIZE) {
		return 0;
	}
	if(rk_read_b_u32(data) != 0x3f3) {
		return 0;
	}
	if(memcmp(data + 40, "RON_KLAREN_SOUNDMODULE!", 23) != 0) {
		return 0;
	}
	return 1;
}

// [=]===^=[ rk_find_number_of_sub_songs ]========================================================[=]
// Mirrors C# FindNumberOfSubSongs. Operates on `search_buffer` which is module_data + 32.
static int32_t rk_find_number_of_sub_songs(uint8_t *buf, int32_t search_length, int32_t *out_count) {
	int32_t index;
	int32_t count = 0;

	for(index = RK_HEADER_SIZE; index < (search_length - 6); index += 6) {
		if((buf[index] != 0x4e) || (buf[index + 1] != 0xf9)) {
			break;
		}
	}

	if(index >= (search_length - 6)) {
		return 0;
	}

	for(; index < (search_length - 6); index += 8) {
		if((buf[index] != 0x30) || (buf[index + 1] != 0x3c)) {
			break;
		}
		count++;
	}

	*out_count = count;
	return (count != 0) ? 1 : 0;
}

// [=]===^=[ rk_check_for_vblank_player ]=========================================================[=]
static int32_t rk_check_for_vblank_player(struct ronklaren_state *s, uint8_t *buf, int32_t search_length, uint32_t *out_irq_offset) {
	int32_t index = RK_HEADER_SIZE + 6;
	int32_t play_offset;

	*out_irq_offset = 0;

	if((buf[index] != 0x4e) || (buf[index + 1] != 0xf9)) {
		return 0;
	}

	play_offset = (int32_t)(((uint32_t)buf[index + 2] << 24) | ((uint32_t)buf[index + 3] << 16) | ((uint32_t)buf[index + 4] << 8) | (uint32_t)buf[index + 5]);
	if((play_offset < 0) || (play_offset >= search_length)) {
		return 0;
	}

	index = play_offset;

	if((buf[index] != 0x41) || (buf[index + 1] != 0xfa)) {
		return 0;
	}

	*out_irq_offset = (uint32_t)play_offset;
	s->cia_value = 14187;
	return 1;
}

// [=]===^=[ rk_find_song_speed_and_irq_offset ]==================================================[=]
static int32_t rk_find_song_speed_and_irq_offset(struct ronklaren_state *s, uint8_t *buf, int32_t search_length, uint32_t *out_irq_offset) {
	int32_t index = RK_HEADER_SIZE;
	int32_t init_offset = 0;
	uint32_t irq_offset = 0;
	uint8_t cia_lo = 0;
	uint8_t cia_hi = 0;
	int32_t i;

	*out_irq_offset = 0;

	for(i = 0; i < 2; i++) {
		if((buf[index] != 0x4e) || (buf[index + 1] != 0xf9)) {
			return 0;
		}

		init_offset = (int32_t)(((uint32_t)buf[index + 2] << 24) | ((uint32_t)buf[index + 3] << 16) | ((uint32_t)buf[index + 4] << 8) | (uint32_t)buf[index + 5]);
		if((init_offset < 0) || (init_offset >= search_length)) {
			return 0;
		}

		if(((buf[init_offset] == 0x61) && (buf[init_offset + 1] == 0x00)) ||
			((buf[init_offset] == 0x33) && (buf[init_offset + 1] == 0xfc))) {
			break;
		}

		index += 6;
	}

	if(i == 2) {
		return rk_check_for_vblank_player(s, buf, search_length, out_irq_offset);
	}

	index = init_offset;

	for(; index < (search_length - 10); index += 2) {
		if((buf[index] == 0x4e) && (buf[index + 1] == 0x75)) {
			break;
		}

		if((buf[index] == 0x13) && (buf[index + 1] == 0xfc)) {
			uint8_t value = buf[index + 3];
			uint32_t adr = ((uint32_t)buf[index + 4] << 24) | ((uint32_t)buf[index + 5] << 16) | ((uint32_t)buf[index + 6] << 8) | (uint32_t)buf[index + 7];
			index += 6;

			if(adr == 0xbfd400) {
				cia_lo = value;
			} else if(adr == 0xbfd500) {
				cia_hi = value;
			}
		} else if((buf[index] == 0x23) && (buf[index + 1] == 0xfc)) {
			uint32_t adr = ((uint32_t)buf[index + 2] << 24) | ((uint32_t)buf[index + 3] << 16) | ((uint32_t)buf[index + 4] << 8) | (uint32_t)buf[index + 5];
			uint32_t dest_adr = ((uint32_t)buf[index + 6] << 24) | ((uint32_t)buf[index + 7] << 16) | ((uint32_t)buf[index + 8] << 8) | (uint32_t)buf[index + 9];
			index += 8;

			if(dest_adr == 0x00000078) {
				irq_offset = adr;
			}
		}
	}

	s->cia_value = (uint16_t)(((uint16_t)cia_hi << 8) | (uint16_t)cia_lo);
	*out_irq_offset = irq_offset;

	return ((irq_offset != 0) && (irq_offset < (uint32_t)search_length) && (s->cia_value != 0)) ? 1 : 0;
}

// [=]===^=[ rk_find_sub_song_info ]==============================================================[=]
static int32_t rk_find_sub_song_info(uint8_t *buf, int32_t search_length, uint32_t module_length, uint32_t irq_offset, uint32_t *out_sub_song_offset) {
	int32_t index;
	uint32_t global_offset;

	*out_sub_song_offset = 0;

	for(index = (int32_t)irq_offset; index < (search_length - 2); index += 2) {
		if((buf[index] == 0x41) && (buf[index + 1] == 0xfa)) {
			break;
		}
	}
	if(index >= (search_length - 2)) {
		return 0;
	}

	global_offset = (uint32_t)((((int32_t)buf[index + 2] << 8) | (int32_t)buf[index + 3]) + index + 2);
	index += 4;

	if(global_offset >= module_length) {
		return 0;
	}

	for(; index < (search_length - 12); index += 2) {
		if((buf[index] == 0x4e) && ((buf[index + 1] == 0x73) || (buf[index + 1] == 0x75))) {
			return 0;
		}

		if((buf[index] == 0x02) && (buf[index + 1] == 0x40) && (buf[index + 2] == 0x00) && (buf[index + 3] == 0x0f) &&
			(buf[index + 4] == 0x53) && (buf[index + 5] == 0x40) &&
			(buf[index + 6] == 0xe9) && (buf[index + 7] == 0x48) &&
			(buf[index + 8] == 0x47) && (buf[index + 9] == 0xf0)) {
			break;
		}
	}

	if(index >= (search_length - 12)) {
		return 0;
	}

	*out_sub_song_offset = global_offset + (uint32_t)(((int32_t)buf[index + 10] << 8) | (int32_t)buf[index + 11]) + RK_HUNK_SIZE;

	return ((*out_sub_song_offset) < module_length) ? 1 : 0;
}

// [=]===^=[ rk_find_instrument_arpeggio_offsets ]================================================[=]
static int32_t rk_find_instrument_arpeggio_offsets(uint8_t *buf, int32_t search_length, uint32_t module_length, uint32_t *out_instr, uint32_t *out_arp) {
	int32_t index;
	uint32_t instr_offset = 0;
	uint32_t arp_offset = 0;

	*out_instr = 0;
	*out_arp = 0;

	for(index = RK_HEADER_SIZE; index < (search_length - 4); index += 2) {
		if((buf[index] == 0x0c) && (buf[index + 1] == 0x12) && (buf[index + 2] == 0x00)) {
			if(buf[index + 3] == 0x82) {
				int32_t scan;
				for(scan = index; scan < (search_length - 4); scan += 2) {
					if((buf[scan] == 0x49) && (buf[scan + 1] == 0xfa)) {
						instr_offset = (uint32_t)((((int32_t)buf[scan + 2] << 8) | (int32_t)buf[scan + 3]) + scan + 2) + RK_HUNK_SIZE;
						break;
					}
				}
			}

			if(buf[index + 3] == 0x80) {
				int32_t scan;
				for(scan = index; scan < (search_length - 4); scan += 2) {
					if((buf[scan] == 0x49) && (buf[scan + 1] == 0xfa)) {
						arp_offset = (uint32_t)((((int32_t)buf[scan + 2] << 8) | (int32_t)buf[scan + 3]) + scan + 2) + RK_HUNK_SIZE;
						break;
					}
				}
			}
		}

		if((instr_offset != 0) && (arp_offset != 0)) {
			break;
		}
	}

	if(index >= (search_length - 4)) {
		return 0;
	}

	*out_instr = instr_offset;
	*out_arp = arp_offset;

	return ((instr_offset < module_length) && (arp_offset < module_length)) ? 1 : 0;
}

// [=]===^=[ rk_enable_track_features ]===========================================================[=]
static void rk_enable_track_features(struct ronklaren_state *s, uint8_t *buf, int32_t search_length) {
	int32_t index;

	s->clear_adsr_state_on_portamento = 0;

	for(index = RK_HEADER_SIZE; index < (search_length - 4); index += 2) {
		if((buf[index] == 0x0c) && (buf[index + 1] == 0x12) && (buf[index + 2] == 0x00)) {
			if(buf[index + 3] == 0x81) {
				if(index < (search_length - 10)) {
					if((buf[index + 8] == 0x42) && (buf[index + 9] == 0x68)) {
						s->clear_adsr_state_on_portamento = 1;
					}
				}
			}
		}
	}
}

// [=]===^=[ rk_find_effect_byte_count ]==========================================================[=]
// Returns -1 if invalid track byte detected.
static int32_t rk_find_effect_byte_count(uint8_t effect) {
	if(effect < 0x80) {
		return 1;
	}
	switch(effect) {
		case RK_EFFECT_SET_ARPEGGIO:
		case RK_EFFECT_SET_INSTRUMENT:
		case RK_EFFECT_CHANGE_ADSR: {
			return 1;
		}

		case RK_EFFECT_SET_PORTAMENTO: {
			return 3;
		}

		case RK_EFFECT_END_SONG:
		case RK_EFFECT_END_SONG_2:
		case RK_EFFECT_END_OF_TRACK: {
			return 0;
		}
	}
	return -1;
}

// [=]===^=[ rk_compare_offset_index ]============================================================[=]
// const-qualified per the C standard library's qsort comparator signature.
static int rk_compare_offset_index(const void *a, const void *b) {
	int32_t oa = ((const struct rk_offset_index *)a)->offset;
	int32_t ob = ((const struct rk_offset_index *)b)->offset;
	if(oa < ob) {
		return -1;
	}
	if(oa > ob) {
		return 1;
	}
	return 0;
}

// [=]===^=[ rk_find_index_for_offset ]===========================================================[=]
static int32_t rk_find_index_for_offset(struct rk_offset_index *map, uint32_t count, int32_t offset) {
	for(uint32_t i = 0; i < count; ++i) {
		if(map[i].offset == offset) {
			return map[i].index;
		}
	}
	return -1;
}

// [=]===^=[ rk_load_sub_song_info ]==============================================================[=]
// Loads track-list offsets per sub-song. Truncates the sub-song count if any offset
// would exceed the module size, matching C# behavior. Returns malloced array of
// uint32_t[num_sub_songs * 4] or 0 on failure (and 0 sub-songs).
static uint32_t *rk_load_sub_song_info(struct ronklaren_state *s, uint32_t sub_song_offset, int32_t *io_count) {
	int32_t count = *io_count;
	if(count <= 0) {
		return 0;
	}

	uint32_t *offsets = (uint32_t *)calloc((size_t)(count * 4), sizeof(uint32_t));
	if(!offsets) {
		return 0;
	}

	uint32_t pos = sub_song_offset;
	int32_t actual = count;

	for(int32_t i = 0; i < count; ++i) {
		for(int32_t j = 0; j < 4; ++j) {
			if((pos + 4) > s->module_len) {
				free(offsets);
				return 0;
			}
			uint32_t off = rk_read_b_u32(s->module_data + pos) + RK_HUNK_SIZE;
			pos += 4;
			offsets[i * 4 + j] = off;

			if(off >= s->module_len) {
				actual = i;
				*io_count = actual;
				return offsets;
			}
		}
	}

	*io_count = actual;
	return offsets;
}

// [=]===^=[ rk_load_single_track_list ]==========================================================[=]
// Reads a track list at module_data[offset] until a negative entry is encountered.
// Returns 1 on success, 0 on read error.
static int32_t rk_load_single_track_list(struct ronklaren_state *s, uint32_t offset, struct rk_track_entry **out_list, uint32_t *out_count) {
	uint32_t pos = offset;
	struct rk_track_entry *list = 0;
	uint32_t count = 0;
	uint32_t cap = 0;

	for(;;) {
		if((pos + 12) > s->module_len) {
			free(list);
			return 0;
		}
		int32_t track_offset = rk_read_b_i32(s->module_data + pos);
		pos += 4;
		if(track_offset < 0) {
			break;
		}

		pos += 2;
		int16_t transpose = rk_read_b_i16(s->module_data + pos);
		pos += 2;
		pos += 2;
		uint16_t repeat_times = rk_read_b_u16(s->module_data + pos);
		pos += 2;

		if(count == cap) {
			uint32_t newcap = (cap == 0) ? 16 : cap * 2;
			struct rk_track_entry *newlist = (struct rk_track_entry *)realloc(list, newcap * sizeof(struct rk_track_entry));
			if(!newlist) {
				free(list);
				return 0;
			}
			list = newlist;
			cap = newcap;
		}

		list[count].track_number = track_offset + RK_HUNK_SIZE;
		list[count].transpose = transpose;
		list[count].number_of_repeat_times = repeat_times;
		count++;
	}

	*out_list = list;
	*out_count = count;
	return 1;
}

// [=]===^=[ rk_load_track_lists ]================================================================[=]
static int32_t rk_load_track_lists(struct ronklaren_state *s, int32_t num_sub_songs, uint32_t *track_offsets) {
	s->sub_songs = (struct rk_song_info *)calloc((size_t)num_sub_songs, sizeof(struct rk_song_info));
	if(!s->sub_songs) {
		return 0;
	}
	s->num_sub_songs = (uint32_t)num_sub_songs;

	for(int32_t i = 0; i < num_sub_songs; ++i) {
		for(int32_t j = 0; j < 4; ++j) {
			uint32_t off = track_offsets[i * 4 + j];
			if(!rk_load_single_track_list(s, off, &s->sub_songs[i].positions[j].tracks, &s->sub_songs[i].positions[j].num_tracks)) {
				return 0;
			}
		}
	}

	return 1;
}

// [=]===^=[ rk_load_single_track ]===============================================================[=]
static int32_t rk_load_single_track(struct ronklaren_state *s, uint32_t offset, uint8_t **out_data, uint32_t *out_len) {
	uint32_t pos = offset;
	uint8_t *data = 0;
	uint32_t count = 0;
	uint32_t cap = 0;

	for(;;) {
		if(pos >= s->module_len) {
			free(data);
			return 0;
		}
		uint8_t byt = s->module_data[pos++];

		if(count == cap) {
			uint32_t newcap = (cap == 0) ? 64 : cap * 2;
			uint8_t *newdata = (uint8_t *)realloc(data, newcap);
			if(!newdata) {
				free(data);
				return 0;
			}
			data = newdata;
			cap = newcap;
		}
		data[count++] = byt;

		if(byt == 0xff) {
			break;
		}

		int32_t effect_count = rk_find_effect_byte_count(byt);
		if(effect_count < 0) {
			free(data);
			return 0;
		}

		for(; effect_count > 0; --effect_count) {
			if(pos >= s->module_len) {
				free(data);
				return 0;
			}
			uint8_t b = s->module_data[pos++];

			if(count == cap) {
				uint32_t newcap = cap * 2;
				uint8_t *newdata = (uint8_t *)realloc(data, newcap);
				if(!newdata) {
					free(data);
					return 0;
				}
				data = newdata;
				cap = newcap;
			}
			data[count++] = b;
		}
	}

	*out_data = data;
	*out_len = count;
	return 1;
}

// [=]===^=[ rk_load_tracks ]=====================================================================[=]
// First pass: collect unique track offsets across all sub-songs/positions.
// Second pass: load each unique track and remap track_number to a contiguous index.
static int32_t rk_load_tracks(struct ronklaren_state *s) {
	struct rk_offset_index *map = 0;
	uint32_t map_count = 0;
	uint32_t map_cap = 0;

	for(uint32_t si = 0; si < s->num_sub_songs; ++si) {
		for(int32_t j = 0; j < 4; ++j) {
			struct rk_position_list *pl = &s->sub_songs[si].positions[j];
			for(uint32_t k = 0; k < pl->num_tracks; ++k) {
				int32_t off = pl->tracks[k].track_number;
				int32_t found = 0;
				for(uint32_t m = 0; m < map_count; ++m) {
					if(map[m].offset == off) {
						found = 1;
						break;
					}
				}
				if(!found) {
					if(map_count == map_cap) {
						uint32_t newcap = (map_cap == 0) ? 32 : map_cap * 2;
						struct rk_offset_index *newmap = (struct rk_offset_index *)realloc(map, newcap * sizeof(struct rk_offset_index));
						if(!newmap) {
							free(map);
							return 0;
						}
						map = newmap;
						map_cap = newcap;
					}
					map[map_count].offset = off;
					map[map_count].index = -1;
					map_count++;
				}
			}
		}
	}

	// Sort by offset (ascending) then assign indices
	qsort(map, map_count, sizeof(struct rk_offset_index), rk_compare_offset_index);
	for(uint32_t i = 0; i < map_count; ++i) {
		map[i].index = (int32_t)i;
	}

	s->tracks = (struct rk_byte_seq *)calloc(map_count, sizeof(struct rk_byte_seq));
	if(!s->tracks) {
		free(map);
		return 0;
	}
	s->num_tracks = map_count;

	for(uint32_t i = 0; i < map_count; ++i) {
		if(!rk_load_single_track(s, (uint32_t)map[i].offset, &s->tracks[i].data, &s->tracks[i].length)) {
			free(map);
			return 0;
		}
	}

	// Remap track_number in track lists.
	for(uint32_t si = 0; si < s->num_sub_songs; ++si) {
		for(int32_t j = 0; j < 4; ++j) {
			struct rk_position_list *pl = &s->sub_songs[si].positions[j];
			for(uint32_t k = 0; k < pl->num_tracks; ++k) {
				int32_t idx = rk_find_index_for_offset(map, map_count, pl->tracks[k].track_number);
				if(idx < 0) {
					free(map);
					return 0;
				}
				pl->tracks[k].track_number = idx;
			}
		}
	}

	free(map);
	return 1;
}

// [=]===^=[ rk_find_max_effects ]================================================================[=]
static void rk_find_max_effects(struct ronklaren_state *s, int32_t *out_instr_count, int32_t *out_arp_count) {
	int32_t instr_count = 0;
	int32_t arp_count = 0;

	for(uint32_t t = 0; t < s->num_tracks; ++t) {
		uint8_t *data = s->tracks[t].data;
		uint32_t len = s->tracks[t].length;

		for(uint32_t i = 0; i < len; ) {
			uint8_t effect = data[i];

			if((effect == RK_EFFECT_SET_INSTRUMENT) && ((i + 1) < len)) {
				int32_t v = (int32_t)data[i + 1] + 1;
				if(v > instr_count) {
					instr_count = v;
				}
			} else if((effect == RK_EFFECT_SET_ARPEGGIO) && ((i + 1) < len)) {
				int32_t v = (int32_t)data[i + 1] + 1;
				if(v > arp_count) {
					arp_count = v;
				}
			}

			int32_t bc = rk_find_effect_byte_count(effect);
			if(bc < 0) {
				break;
			}
			i += 1 + (uint32_t)bc;

			// C# does `i += FindEffectByteCount(effect);` after the for-loop's `i++`,
			// so total advance is 1 + bc. Done above.
		}
	}

	*out_instr_count = instr_count;
	*out_arp_count = arp_count;
}

// [=]===^=[ rk_load_arpeggios ]==================================================================[=]
static int32_t rk_load_arpeggios(struct ronklaren_state *s, uint32_t arpeggio_offset, int32_t arpeggio_count) {
	if(arpeggio_count <= 0) {
		s->arpeggios = 0;
		s->num_arpeggios = 0;
		return 1;
	}

	s->arpeggios = (struct rk_sbyte_seq *)calloc((size_t)arpeggio_count, sizeof(struct rk_sbyte_seq));
	if(!s->arpeggios) {
		return 0;
	}
	s->num_arpeggios = (uint32_t)arpeggio_count;

	uint32_t pos = arpeggio_offset;
	for(int32_t i = 0; i < arpeggio_count; ++i) {
		if((pos + 12) > s->module_len) {
			return 0;
		}
		s->arpeggios[i].data = (int8_t *)malloc(12);
		if(!s->arpeggios[i].data) {
			return 0;
		}
		s->arpeggios[i].length = 12;
		memcpy(s->arpeggios[i].data, s->module_data + pos, 12);
		pos += 12;
	}

	return 1;
}

// [=]===^=[ rk_load_instruments ]================================================================[=]
static int32_t rk_load_instruments(struct ronklaren_state *s, uint32_t instrument_offset, int32_t instrument_count) {
	if(instrument_count <= 0) {
		return 0;
	}

	s->instruments = (struct rk_instrument *)calloc((size_t)instrument_count, sizeof(struct rk_instrument));
	if(!s->instruments) {
		return 0;
	}
	s->num_instruments = (uint32_t)instrument_count;

	uint32_t pos = instrument_offset;
	for(int32_t i = 0; i < instrument_count; ++i) {
		if((pos + RK_INSTR_RECORD_SIZE) > s->module_len) {
			return 0;
		}
		struct rk_instrument *ins = &s->instruments[i];

		ins->sample_number = rk_read_b_i32(s->module_data + pos) + RK_HUNK_SIZE;     pos += 4;
		ins->vibrato_number = rk_read_b_i32(s->module_data + pos) + RK_HUNK_SIZE;    pos += 4;
		ins->type = (s->module_data[pos] == 0) ? RK_INSTR_TYPE_SYNTHESIS : RK_INSTR_TYPE_SAMPLE; pos += 1;
		ins->phase_speed = s->module_data[pos++];
		ins->phase_length_in_words = s->module_data[pos++];
		ins->vibrato_speed = s->module_data[pos++];
		ins->vibrato_depth = s->module_data[pos++];
		ins->vibrato_delay = s->module_data[pos++];

		for(int32_t j = 0; j < 4; ++j) {
			ins->adsr[j].point = s->module_data[pos++];
		}
		for(int32_t j = 0; j < 4; ++j) {
			ins->adsr[j].increment = s->module_data[pos++];
		}

		ins->phase_value = (int8_t)s->module_data[pos++];
		ins->phase_direction = ((int8_t)s->module_data[pos++] < 0) ? 1 : 0;
		ins->phase_position = s->module_data[pos++];

		pos += 7; // Reserved

		if(ins->vibrato_speed == 0) {
			ins->vibrato_number = -1;
		}
	}

	return 1;
}

// [=]===^=[ rk_load_vibratos ]===================================================================[=]
static int32_t rk_load_vibratos(struct ronklaren_state *s) {
	struct rk_offset_index *map = 0;
	uint32_t map_count = 0;
	uint32_t map_cap = 0;

	for(uint32_t i = 0; i < s->num_instruments; ++i) {
		int32_t vn = s->instruments[i].vibrato_number;
		if(vn == -1) {
			continue;
		}
		int32_t found = 0;
		for(uint32_t m = 0; m < map_count; ++m) {
			if(map[m].offset == vn) {
				found = 1;
				break;
			}
		}
		if(!found) {
			if(map_count == map_cap) {
				uint32_t newcap = (map_cap == 0) ? 16 : map_cap * 2;
				struct rk_offset_index *newmap = (struct rk_offset_index *)realloc(map, newcap * sizeof(struct rk_offset_index));
				if(!newmap) {
					free(map);
					return 0;
				}
				map = newmap;
				map_cap = newcap;
			}
			map[map_count].offset = vn;
			map[map_count].index = -1;
			map_count++;
		}
	}

	if(map_count == 0) {
		s->vibratos = 0;
		s->num_vibratos = 0;
		free(map);
		return 1;
	}

	qsort(map, map_count, sizeof(struct rk_offset_index), rk_compare_offset_index);
	for(uint32_t i = 0; i < map_count; ++i) {
		map[i].index = (int32_t)i;
	}

	s->vibratos = (struct rk_sbyte_seq *)calloc(map_count, sizeof(struct rk_sbyte_seq));
	if(!s->vibratos) {
		free(map);
		return 0;
	}
	s->num_vibratos = map_count;

	for(uint32_t i = 0; i < map_count; ++i) {
		uint32_t header_pos = (uint32_t)map[i].offset;
		if((header_pos + 6) > s->module_len) {
			free(map);
			return 0;
		}
		uint32_t table_offset = rk_read_b_u32(s->module_data + header_pos) + RK_HUNK_SIZE;
		uint16_t length = rk_read_b_u16(s->module_data + header_pos + 4);
		uint32_t bytes = (uint32_t)length * 2;

		if((table_offset + bytes) > s->module_len) {
			free(map);
			return 0;
		}

		s->vibratos[i].data = (int8_t *)malloc(bytes);
		if(!s->vibratos[i].data) {
			free(map);
			return 0;
		}
		s->vibratos[i].length = bytes;
		memcpy(s->vibratos[i].data, s->module_data + table_offset, bytes);
	}

	for(uint32_t i = 0; i < s->num_instruments; ++i) {
		int32_t vn = s->instruments[i].vibrato_number;
		if(vn == -1) {
			continue;
		}
		int32_t idx = rk_find_index_for_offset(map, map_count, vn);
		if(idx < 0) {
			free(map);
			return 0;
		}
		s->instruments[i].vibrato_number = idx;
	}

	free(map);
	return 1;
}

// [=]===^=[ rk_load_samples ]====================================================================[=]
static int32_t rk_load_samples(struct ronklaren_state *s) {
	struct rk_offset_index *map = 0;
	uint32_t map_count = 0;
	uint32_t map_cap = 0;

	for(uint32_t i = 0; i < s->num_instruments; ++i) {
		int32_t sn = s->instruments[i].sample_number;
		int32_t found = 0;
		for(uint32_t m = 0; m < map_count; ++m) {
			if(map[m].offset == sn) {
				found = 1;
				break;
			}
		}
		if(!found) {
			if(map_count == map_cap) {
				uint32_t newcap = (map_cap == 0) ? 16 : map_cap * 2;
				struct rk_offset_index *newmap = (struct rk_offset_index *)realloc(map, newcap * sizeof(struct rk_offset_index));
				if(!newmap) {
					free(map);
					return 0;
				}
				map = newmap;
				map_cap = newcap;
			}
			map[map_count].offset = sn;
			map[map_count].index = -1;
			map_count++;
		}
	}

	if(map_count == 0) {
		free(map);
		return 0;
	}

	qsort(map, map_count, sizeof(struct rk_offset_index), rk_compare_offset_index);
	for(uint32_t i = 0; i < map_count; ++i) {
		map[i].index = (int32_t)i;
	}

	s->samples = (struct rk_sample *)calloc(map_count, sizeof(struct rk_sample));
	if(!s->samples) {
		free(map);
		return 0;
	}
	s->num_samples = map_count;

	for(uint32_t i = 0; i < map_count; ++i) {
		uint32_t pos = (uint32_t)map[i].offset;
		if((pos + 8) > s->module_len) {
			free(map);
			return 0;
		}
		s->samples[i].sample_number = rk_read_b_i32(s->module_data + pos) + RK_HUNK_SIZE;
		s->samples[i].length_in_words = rk_read_b_u16(s->module_data + pos + 4);
		s->samples[i].phase_index = rk_read_b_u16(s->module_data + pos + 6);
	}

	for(uint32_t i = 0; i < s->num_instruments; ++i) {
		int32_t idx = rk_find_index_for_offset(map, map_count, s->instruments[i].sample_number);
		if(idx < 0) {
			free(map);
			return 0;
		}
		s->instruments[i].sample_number = idx;
	}

	free(map);
	return 1;
}

// [=]===^=[ rk_load_sample_data ]================================================================[=]
static int32_t rk_load_sample_data(struct ronklaren_state *s) {
	struct rk_offset_index *map = 0;
	uint32_t map_count = 0;
	uint32_t map_cap = 0;

	for(uint32_t i = 0; i < s->num_samples; ++i) {
		int32_t sn = s->samples[i].sample_number;
		int32_t found = 0;
		for(uint32_t m = 0; m < map_count; ++m) {
			if(map[m].offset == sn) {
				found = 1;
				break;
			}
		}
		if(!found) {
			if(map_count == map_cap) {
				uint32_t newcap = (map_cap == 0) ? 16 : map_cap * 2;
				struct rk_offset_index *newmap = (struct rk_offset_index *)realloc(map, newcap * sizeof(struct rk_offset_index));
				if(!newmap) {
					free(map);
					return 0;
				}
				map = newmap;
				map_cap = newcap;
			}
			map[map_count].offset = sn;
			map[map_count].index = -1;
			map_count++;
		}
	}

	if(map_count == 0) {
		free(map);
		return 0;
	}

	qsort(map, map_count, sizeof(struct rk_offset_index), rk_compare_offset_index);
	for(uint32_t i = 0; i < map_count; ++i) {
		map[i].index = (int32_t)i;
	}

	s->sample_data = (struct rk_sbyte_seq *)calloc(map_count, sizeof(struct rk_sbyte_seq));
	if(!s->sample_data) {
		free(map);
		return 0;
	}
	s->num_sample_data = map_count;

	// Determine sample byte length per offset by finding any sample whose
	// sample_number resolves to this offset.
	for(uint32_t i = 0; i < map_count; ++i) {
		int32_t target_offset = map[i].offset;
		uint32_t bytes = 0;
		for(uint32_t j = 0; j < s->num_samples; ++j) {
			if(s->samples[j].sample_number == target_offset) {
				bytes = (uint32_t)s->samples[j].length_in_words * 2u;
				break;
			}
		}
		if(((uint32_t)target_offset + bytes) > s->module_len) {
			free(map);
			return 0;
		}
		s->sample_data[i].data = (int8_t *)malloc(bytes ? bytes : 1);
		if(!s->sample_data[i].data) {
			free(map);
			return 0;
		}
		s->sample_data[i].length = bytes;
		if(bytes > 0) {
			memcpy(s->sample_data[i].data, s->module_data + target_offset, bytes);
		}
	}

	for(uint32_t i = 0; i < s->num_samples; ++i) {
		int32_t idx = rk_find_index_for_offset(map, map_count, s->samples[i].sample_number);
		if(idx < 0) {
			free(map);
			return 0;
		}
		s->samples[i].sample_number = idx;
	}

	free(map);
	return 1;
}

// [=]===^=[ rk_initialize_voice ]================================================================[=]
static void rk_initialize_voice(struct ronklaren_state *s, int32_t voice) {
	struct rk_position_list *pl = &s->sub_songs[s->sub_song_number - 1].positions[voice];
	struct rk_voice_info *v = &s->voices[voice];

	memset(v, 0, sizeof(*v));

	v->channel_number = voice;
	v->position_list = pl;
	v->track_list_position = 0;

	uint32_t track_idx = (uint32_t)pl->tracks[0].track_number;
	v->track_data = s->tracks[track_idx].data;
	v->track_data_length = s->tracks[track_idx].length;
	v->track_data_position = 0;
	v->track_repeat_counter = (uint16_t)(pl->tracks[0].number_of_repeat_times - 1);
	v->wait_counter = 0;

	v->instrument_number = 0;
	v->arpeggio_values = 0;
	v->arpeggio_position = 0;

	v->current_note = 0;
	v->transpose = pl->tracks[0].transpose;
	v->period = 0;

	v->portamento_end_period = 0;
	v->portamento_increment = 0;

	v->vibrato_delay = 0;
	v->vibrato_position = 0;

	v->adsr_state = 0;
	v->adsr_speed = 0;
	v->adsr_speed_counter = 0;
	v->volume = 0;

	v->phase_speed_counter = 0;

	v->set_hardware = 0;
	v->sample_number = 0;
	v->sample_data = 0;
	v->sample_length = 0;
	v->set_loop = 0;
}

// [=]===^=[ rk_initialize_sound ]================================================================[=]
static void rk_initialize_sound(struct ronklaren_state *s, int32_t song_number) {
	s->global_volume = 0x3f;
	s->setup_new_sub_song = 1;
	s->sub_song_number = (uint16_t)(song_number + 1);

	for(int32_t i = 0; i < RK_NUM_VOICES; ++i) {
		rk_initialize_voice(s, i);
	}
}

// [=]===^=[ rk_switch_sub_song ]=================================================================[=]
static void rk_switch_sub_song(struct ronklaren_state *s) {
	if(s->sub_song_number == 0) {
		s->sub_song_number = (uint16_t)(s->current_song + 1);
	}

	for(int32_t i = 0; i < RK_NUM_VOICES; ++i) {
		rk_initialize_voice(s, i);
	}

	s->setup_new_sub_song = 0;
	s->sub_song_number = 0;
}

// [=]===^=[ rk_play_sample ]=====================================================================[=]
static void rk_play_sample(struct ronklaren_state *s, struct rk_voice_info *v) {
	struct rk_instrument *ins = &s->instruments[v->instrument_number];
	struct rk_sample *smp = &s->samples[ins->sample_number];
	int8_t *data = s->sample_data[smp->sample_number].data;

	v->set_hardware = 1;
	v->sample_number = (int16_t)v->instrument_number;
	v->sample_data = data;
	v->sample_length = (uint32_t)smp->length_in_words * 2u;
	v->set_loop = (ins->type == RK_INSTR_TYPE_SYNTHESIS) ? 1 : 0;

	paula_mute(&s->paula, v->channel_number);
}

// [=]===^=[ rk_parse_track_arpeggio ]============================================================[=]
static void rk_parse_track_arpeggio(struct ronklaren_state *s, struct rk_voice_info *v) {
	uint8_t arp = v->track_data[v->track_data_position + 1];
	v->track_data_position += 2;

	if((uint32_t)arp < s->num_arpeggios) {
		v->arpeggio_values = s->arpeggios[arp].data;
	}
}

// [=]===^=[ rk_parse_track_portamento ]==========================================================[=]
static void rk_parse_track_portamento(struct ronklaren_state *s, struct rk_voice_info *v) {
	uint8_t end_note = v->track_data[v->track_data_position + 1];
	uint8_t increment = v->track_data[v->track_data_position + 2];
	uint8_t wait_counter = v->track_data[v->track_data_position + 3];
	v->track_data_position += 4;

	int32_t transposed_note = (int32_t)end_note + (int32_t)v->transpose;
	if(transposed_note >= RK_NUM_PERIODS) {
		transposed_note = RK_NUM_PERIODS - 1;
	}
	if(transposed_note < 0) {
		transposed_note = 0;
	}

	if(s->clear_adsr_state_on_portamento) {
		v->adsr_state = 0;
	}

	v->portamento_end_period = rk_periods[transposed_note];
	v->portamento_increment = increment;
	v->wait_counter = (uint8_t)(wait_counter * 4 - 1);
}

// [=]===^=[ rk_parse_track_instrument ]==========================================================[=]
static void rk_parse_track_instrument(struct ronklaren_state *s, struct rk_voice_info *v) {
	uint8_t ins_num = v->track_data[v->track_data_position + 1];
	v->track_data_position += 2;

	v->volume = 0;
	v->adsr_state = 0;
	v->vibrato_position = 0;

	if((uint32_t)ins_num < s->num_instruments) {
		v->instrument_number = (int32_t)ins_num;
	}

	rk_play_sample(s, v);
}

// [=]===^=[ rk_parse_track_end_song ]============================================================[=]
static void rk_parse_track_end_song(struct ronklaren_state *s) {
	s->setup_new_sub_song = 1;
	s->sub_song_number = 0;
}

// [=]===^=[ rk_parse_track_change_adsr_speed ]===================================================[=]
static void rk_parse_track_change_adsr_speed(struct rk_voice_info *v) {
	uint8_t speed = v->track_data[v->track_data_position + 1];
	v->track_data_position += 2;
	v->adsr_speed = speed;
}

// [=]===^=[ rk_parse_track_end_of_track ]========================================================[=]
static void rk_parse_track_end_of_track(struct ronklaren_state *s, struct rk_voice_info *v) {
	if(v->track_repeat_counter == 0) {
		v->track_list_position++;
		if(v->track_list_position == v->position_list->num_tracks) {
			v->track_list_position = 0;
		}

		struct rk_track_entry *track = &v->position_list->tracks[v->track_list_position];
		uint32_t track_idx = (uint32_t)track->track_number;
		v->track_data = s->tracks[track_idx].data;
		v->track_data_length = s->tracks[track_idx].length;
		v->track_data_position = 0;
		v->transpose = track->transpose;
		v->track_repeat_counter = (uint16_t)(track->number_of_repeat_times - 1);
	} else {
		v->track_repeat_counter--;
		v->track_data_position = 0;
		v->transpose = v->position_list->tracks[v->track_list_position].transpose;
	}
}

// [=]===^=[ rk_parse_track_new_note ]============================================================[=]
// Returns 1 if parsing should continue (no wait), 0 to stop the parse loop.
static int32_t rk_parse_track_new_note(struct ronklaren_state *s, struct rk_voice_info *v) {
	uint8_t note = v->track_data[v->track_data_position];
	uint8_t wait_count = v->track_data[v->track_data_position + 1];
	v->track_data_position += 2;

	int32_t transposed_note = (int32_t)note + (int32_t)v->transpose;
	if(transposed_note >= RK_NUM_PERIODS) {
		transposed_note = RK_NUM_PERIODS - 1;
	}
	if(transposed_note < 0) {
		transposed_note = 0;
	}

	v->current_note = note;
	v->period = rk_periods[transposed_note];

	if(wait_count == 0) {
		return 1;
	}

	v->wait_counter = (uint8_t)(wait_count * 4 - 1);
	v->adsr_state = 0;

	rk_play_sample(s, v);
	return 0;
}

// [=]===^=[ rk_parse_track_data ]================================================================[=]
static void rk_parse_track_data(struct ronklaren_state *s, struct rk_voice_info *v) {
	struct rk_instrument *ins = &s->instruments[v->instrument_number];

	v->portamento_increment = 0;

	if(ins->vibrato_delay != 0) {
		v->vibrato_delay = (uint8_t)(ins->vibrato_delay * 4 - 1);
	}

	uint8_t take_one_more = 1;

	do {
		if(v->track_data_position >= v->track_data_length) {
			break;
		}

		uint8_t cmd = v->track_data[v->track_data_position];

		if(cmd == RK_EFFECT_SET_ARPEGGIO) {
			rk_parse_track_arpeggio(s, v);
		}

		cmd = v->track_data[v->track_data_position];
		if(cmd == RK_EFFECT_SET_PORTAMENTO) {
			rk_parse_track_portamento(s, v);
			take_one_more = 0;
			continue;
		}

		if(cmd == RK_EFFECT_SET_INSTRUMENT) {
			rk_parse_track_instrument(s, v);
		}

		cmd = v->track_data[v->track_data_position];
		if(cmd == RK_EFFECT_END_SONG) {
			rk_parse_track_end_song(s);
			return;
		}

		if(cmd == RK_EFFECT_CHANGE_ADSR) {
			rk_parse_track_change_adsr_speed(v);
		}

		cmd = v->track_data[v->track_data_position];
		if(cmd == RK_EFFECT_END_SONG_2) {
			rk_parse_track_end_song(s);
			return;
		}

		if(v->track_data[v->track_data_position] >= 0x80) {
			rk_parse_track_end_of_track(s, v);
			take_one_more = 1;
		} else {
			take_one_more = (uint8_t)rk_parse_track_new_note(s, v);
		}
	} while(take_one_more);

	paula_set_period(&s->paula, v->channel_number, v->period);
}

// [=]===^=[ rk_do_effect_phasing_voice ]=========================================================[=]
// The phase counter is per-voice but phase_position/direction/value live on the
// instrument. The C# code mutates the shared instrument data (and the shared
// sample_data buffer); we reproduce that exact behavior so the phasing waveform
// matches across voices that share the instrument.
static void rk_do_effect_phasing_voice(struct ronklaren_state *s, struct rk_voice_info *v, struct rk_instrument *ins) {
	if(ins->phase_speed == 0) {
		return;
	}

	if(v->phase_speed_counter == 0) {
		v->phase_speed_counter = (uint8_t)(ins->phase_speed - 1);

		struct rk_sample *smp = &s->samples[ins->sample_number];
		int8_t *data = s->sample_data[smp->sample_number].data;
		uint32_t data_len = s->sample_data[smp->sample_number].length;

		int32_t phase_start = (int32_t)smp->phase_index - (int32_t)ins->phase_length_in_words;
		int32_t phase_index = phase_start + (int32_t)ins->phase_position;

		if((phase_index >= 0) && ((uint32_t)phase_index < data_len)) {
			data[phase_index] = ins->phase_value;
		}

		if(ins->phase_direction) {
			ins->phase_position--;
			if(ins->phase_position == 0) {
				ins->phase_direction = ins->phase_direction ? 0 : 1;
				ins->phase_value = (int8_t)~ins->phase_value;
			}
		} else {
			ins->phase_position++;
			if((ins->phase_length_in_words * 2) == ins->phase_position) {
				ins->phase_direction = ins->phase_direction ? 0 : 1;
				ins->phase_value = (int8_t)~ins->phase_value;
			}
		}
	} else {
		v->phase_speed_counter--;
	}
}

// [=]===^=[ rk_do_effect_portamento ]============================================================[=]
static void rk_do_effect_portamento(struct ronklaren_state *s, struct rk_voice_info *v) {
	if(v->portamento_increment == 0) {
		return;
	}

	uint16_t period = v->period;

	if(period <= v->portamento_end_period) {
		period = (uint16_t)(period + v->portamento_increment);
		if(period > v->portamento_end_period) {
			period = v->portamento_end_period;
		}
	} else {
		period = (uint16_t)(period - v->portamento_increment);
		if(period < v->portamento_end_period) {
			period = v->portamento_end_period;
		}
	}

	v->period = period;
	paula_set_period(&s->paula, v->channel_number, period);
}

// [=]===^=[ rk_do_effect_arpeggio ]==============================================================[=]
static void rk_do_effect_arpeggio(struct ronklaren_state *s, struct rk_voice_info *v) {
	if(v->portamento_increment != 0) {
		return;
	}
	if(v->arpeggio_values == 0) {
		return;
	}

	int32_t arp_val = (int32_t)v->arpeggio_values[v->arpeggio_position];
	int32_t transposed_note = arp_val + (int32_t)v->current_note + (int32_t)v->transpose;
	if(transposed_note >= RK_NUM_PERIODS) {
		transposed_note = RK_NUM_PERIODS - 1;
	}
	if(transposed_note < 0) {
		transposed_note = 0;
	}

	v->period = rk_periods[transposed_note];
	paula_set_period(&s->paula, v->channel_number, v->period);

	if(v->arpeggio_position == 0) {
		v->arpeggio_position = 11;
	} else {
		v->arpeggio_position--;
	}
}

// [=]===^=[ rk_do_effect_vibrato ]===============================================================[=]
static void rk_do_effect_vibrato(struct ronklaren_state *s, struct rk_voice_info *v, struct rk_instrument *ins) {
	if(ins->vibrato_speed == 0) {
		return;
	}

	if(v->vibrato_delay != 0) {
		v->vibrato_delay--;
		return;
	}

	if((ins->vibrato_number < 0) || ((uint32_t)ins->vibrato_number >= s->num_vibratos)) {
		return;
	}

	struct rk_sbyte_seq *vt = &s->vibratos[ins->vibrato_number];
	if(v->vibrato_position >= vt->length) {
		v->vibrato_position = 0;
	}

	int32_t mod = (int32_t)vt->data[v->vibrato_position] * (int32_t)ins->vibrato_depth;
	uint16_t period = (uint16_t)(mod + (int32_t)v->period);
	paula_set_period(&s->paula, v->channel_number, period);

	int32_t new_position = (int32_t)v->vibrato_position - (int32_t)ins->vibrato_speed;
	if(new_position < 0) {
		new_position = (int32_t)vt->length - 1;
	}
	v->vibrato_position = (uint16_t)new_position;
}

// [=]===^=[ rk_do_effect_adsr ]==================================================================[=]
static void rk_do_effect_adsr(struct ronklaren_state *s, struct rk_voice_info *v, struct rk_instrument *ins) {
	v->adsr_speed_counter--;

	if(v->adsr_speed_counter < 0) {
		v->adsr_speed_counter = (int8_t)v->adsr_speed;

		struct rk_adsr_point *point = &ins->adsr[v->adsr_state];
		int32_t volume = (int32_t)v->volume;
		uint8_t advance = 0;

		if(volume > (int32_t)point->point) {
			volume -= (int32_t)point->increment;
			if(volume <= (int32_t)point->point) {
				volume = (int32_t)point->point;
				advance = 1;
			}
		} else {
			volume += (int32_t)point->increment;
			if(volume >= (int32_t)s->global_volume) {
				volume = (int32_t)s->global_volume;
				advance = 1;
			} else if(volume >= (int32_t)point->point) {
				volume = (int32_t)point->point;
				advance = 1;
			}
		}

		if(advance && (v->adsr_state < 3)) {
			v->adsr_state++;
		}

		v->volume = (uint8_t)volume;
		paula_set_volume(&s->paula, v->channel_number, (uint16_t)(volume & 0x3f));
	}
}

// [=]===^=[ rk_effect_handler ]==================================================================[=]
static void rk_effect_handler(struct ronklaren_state *s, struct rk_voice_info *v) {
	struct rk_instrument *ins = &s->instruments[v->instrument_number];

	rk_do_effect_phasing_voice(s, v, ins);
	rk_do_effect_portamento(s, v);
	rk_do_effect_arpeggio(s, v);
	rk_do_effect_vibrato(s, v, ins);
	rk_do_effect_adsr(s, v, ins);
}

// [=]===^=[ rk_process_voice ]===================================================================[=]
static void rk_process_voice(struct ronklaren_state *s, struct rk_voice_info *v) {
	if(v->set_hardware) {
		paula_play_sample(&s->paula, v->channel_number, v->sample_data, v->sample_length);
		if(v->set_loop) {
			paula_set_loop(&s->paula, v->channel_number, 0, v->sample_length);
		} else {
			paula_set_loop(&s->paula, v->channel_number, 0, 0);
		}
		v->set_hardware = 0;
	}

	if(v->wait_counter == 0) {
		rk_parse_track_data(s, v);
	} else {
		v->wait_counter--;
		rk_effect_handler(s, v);
	}
}

// [=]===^=[ rk_play ]============================================================================[=]
static void rk_play(struct ronklaren_state *s) {
	if(s->setup_new_sub_song) {
		rk_switch_sub_song(s);
	} else {
		for(int32_t i = 0; i < RK_NUM_VOICES; ++i) {
			rk_process_voice(s, &s->voices[i]);
		}
	}
}

// [=]===^=[ rk_cleanup ]=========================================================================[=]
static void rk_cleanup(struct ronklaren_state *s) {
	if(!s) {
		return;
	}

	if(s->sub_songs) {
		for(uint32_t i = 0; i < s->num_sub_songs; ++i) {
			for(int32_t j = 0; j < RK_NUM_VOICES; ++j) {
				free(s->sub_songs[i].positions[j].tracks);
				s->sub_songs[i].positions[j].tracks = 0;
			}
		}
		free(s->sub_songs);
		s->sub_songs = 0;
	}

	if(s->tracks) {
		for(uint32_t i = 0; i < s->num_tracks; ++i) {
			free(s->tracks[i].data);
		}
		free(s->tracks);
		s->tracks = 0;
	}

	if(s->arpeggios) {
		for(uint32_t i = 0; i < s->num_arpeggios; ++i) {
			free(s->arpeggios[i].data);
		}
		free(s->arpeggios);
		s->arpeggios = 0;
	}

	if(s->vibratos) {
		for(uint32_t i = 0; i < s->num_vibratos; ++i) {
			free(s->vibratos[i].data);
		}
		free(s->vibratos);
		s->vibratos = 0;
	}

	if(s->instruments) {
		free(s->instruments);
		s->instruments = 0;
	}

	if(s->samples) {
		free(s->samples);
		s->samples = 0;
	}

	if(s->sample_data) {
		for(uint32_t i = 0; i < s->num_sample_data; ++i) {
			free(s->sample_data[i].data);
		}
		free(s->sample_data);
		s->sample_data = 0;
	}
}

// [=]===^=[ ronklaren_init ]=====================================================================[=]
static struct ronklaren_state *ronklaren_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || (len < RK_MIN_FILE_SIZE) || (sample_rate < 8000)) {
		return 0;
	}

	if(!rk_identify((uint8_t *)data, len)) {
		return 0;
	}

	struct ronklaren_state *s = (struct ronklaren_state *)calloc(1, sizeof(struct ronklaren_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;

	uint8_t *search_buffer = s->module_data + RK_HUNK_SIZE;
	int32_t search_length = (int32_t)RK_MIN_FILE_SIZE;
	if((uint32_t)search_length > (len - RK_HUNK_SIZE)) {
		search_length = (int32_t)(len - RK_HUNK_SIZE);
	}

	int32_t num_sub_songs = 0;
	uint32_t irq_offset = 0;
	uint32_t sub_song_offset = 0;
	uint32_t instrument_offset = 0;
	uint32_t arpeggio_offset = 0;
	uint32_t *sub_song_track_offsets = 0;
	int32_t instrument_count = 0;
	int32_t arpeggio_count = 0;

	if(!rk_find_number_of_sub_songs(search_buffer, search_length, &num_sub_songs)) {
		goto fail;
	}
	if(!rk_find_song_speed_and_irq_offset(s, search_buffer, search_length, &irq_offset)) {
		goto fail;
	}
	if(!rk_find_sub_song_info(search_buffer, search_length, len, irq_offset, &sub_song_offset)) {
		goto fail;
	}
	if(!rk_find_instrument_arpeggio_offsets(search_buffer, search_length, len, &instrument_offset, &arpeggio_offset)) {
		goto fail;
	}

	rk_enable_track_features(s, search_buffer, search_length);

	sub_song_track_offsets = rk_load_sub_song_info(s, sub_song_offset, &num_sub_songs);
	if(!sub_song_track_offsets || (num_sub_songs <= 0)) {
		free(sub_song_track_offsets);
		sub_song_track_offsets = 0;
		goto fail;
	}

	if(!rk_load_track_lists(s, num_sub_songs, sub_song_track_offsets)) {
		free(sub_song_track_offsets);
		sub_song_track_offsets = 0;
		goto fail;
	}
	free(sub_song_track_offsets);
	sub_song_track_offsets = 0;

	if(!rk_load_tracks(s)) {
		goto fail;
	}

	rk_find_max_effects(s, &instrument_count, &arpeggio_count);

	if(!rk_load_arpeggios(s, arpeggio_offset, arpeggio_count)) {
		goto fail;
	}
	if(!rk_load_instruments(s, instrument_offset, instrument_count)) {
		goto fail;
	}
	if(!rk_load_vibratos(s)) {
		goto fail;
	}
	if(!rk_load_samples(s)) {
		goto fail;
	}
	if(!rk_load_sample_data(s)) {
		goto fail;
	}

	paula_init(&s->paula, sample_rate, RK_TICK_HZ);
	s->current_song = 0;
	rk_initialize_sound(s, 0);
	return s;

fail:
	rk_cleanup(s);
	free(s);
	return 0;
}

// [=]===^=[ ronklaren_free ]=====================================================================[=]
static void ronklaren_free(struct ronklaren_state *s) {
	if(!s) {
		return;
	}
	rk_cleanup(s);
	free(s);
}

// [=]===^=[ ronklaren_get_audio ]================================================================[=]
static void ronklaren_get_audio(struct ronklaren_state *s, int16_t *output, int32_t frames) {
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
			rk_play(s);
		}
	}
}

// [=]===^=[ ronklaren_api_init ]=================================================================[=]
static void *ronklaren_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return ronklaren_init(data, len, sample_rate);
}

// [=]===^=[ ronklaren_api_free ]=================================================================[=]
static void ronklaren_api_free(void *state) {
	ronklaren_free((struct ronklaren_state *)state);
}

// [=]===^=[ ronklaren_api_get_audio ]============================================================[=]
static void ronklaren_api_get_audio(void *state, int16_t *output, int32_t frames) {
	ronklaren_get_audio((struct ronklaren_state *)state, output, frames);
}

static const char *ronklaren_extensions[] = { "rk", 0 };

static struct player_api ronklaren_api = {
	"Ron Klaren",
	ronklaren_extensions,
	ronklaren_api_init,
	ronklaren_api_free,
	ronklaren_api_get_audio,
	0,
};
