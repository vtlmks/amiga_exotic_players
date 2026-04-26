// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// AMOS Music Bank replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate (PAL).
//
// Public API:
//   struct amosmusicbank_state *amosmusicbank_init(void *data, uint32_t len, int32_t sample_rate);
//   void amosmusicbank_free(struct amosmusicbank_state *s);
//   void amosmusicbank_get_audio(struct amosmusicbank_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define AMB_TICK_HZ        50
#define AMB_TEMPO_BASE_PAL 100

enum {
	AMB_EFF_NONE        = 0,
	AMB_EFF_ARPEGGIO    = 1,
	AMB_EFF_PORTAMENTO  = 2,
	AMB_EFF_VIBRATO     = 3,
	AMB_EFF_VOLUMESLIDE = 4,
	AMB_EFF_SLIDE       = 5,
};

struct amb_sample {
	int8_t *data;          // points into module buffer (or empty marker)
	uint32_t length;
	uint32_t loop_start;
	uint32_t loop_length;
	uint16_t volume;
};

struct amb_track {
	uint16_t *data;        // contiguous run of u16 words ending with 0x8xxx or 0x91xx
	uint32_t length;       // number of words
};

struct amb_position_list {
	int16_t *track_numbers;
	uint32_t length;
};

struct amb_song_info {
	struct amb_position_list position_lists[4];
};

struct amb_voice {
	int32_t channel_number;
	uint16_t *voi_adr;       // pointer to current track word array
	int32_t voi_adr_index;
	int32_t voi_deb;
	struct amb_sample *voi_inst;
	int16_t voi_inst_number;
	int32_t voi_pat_d_index;
	struct amb_position_list *voi_pat;
	int32_t voi_pat_index;
	uint16_t voi_cpt;
	uint16_t voi_rep;
	uint16_t voi_note;
	uint16_t voi_d_vol;
	uint16_t voi_vol;
	int16_t voi_value;
	uint16_t voi_p_to_to;
	uint8_t voi_p_tone;
	int8_t voi_vib;
	uint8_t voi_effect;
};

struct amosmusicbank_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	struct amb_sample *samples;
	uint32_t num_samples;

	struct amb_track *tracks;
	uint32_t num_tracks;

	struct amb_song_info *songs;
	uint32_t num_songs;

	uint16_t *track_words;        // owning storage for all track word arrays
	uint32_t num_track_words;
	int16_t *position_words;      // owning storage for all position list arrays
	uint32_t num_position_words;

	uint16_t tempo_base;          // 100 = PAL
	uint16_t mu_cpt;
	uint16_t mu_tempo;

	struct amb_voice voices[4];

	uint16_t empty_track[1];      // single-word 0x8000 sentinel
};

// [=]===^=[ amb_periods ]========================================================================[=]
static uint16_t amb_periods[51] = {
	856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
	428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
	214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0
};

// [=]===^=[ amb_sinus ]==========================================================================[=]
static uint8_t amb_sinus[32] = {
	0x00, 0x18, 0x31, 0x4a, 0x61, 0x78, 0x8d, 0xa1, 0xb4, 0xc5, 0xd4, 0xe0, 0xeb, 0xf4, 0xfa, 0xfd,
	0xff, 0xfd, 0xfa, 0xf4, 0xeb, 0xe0, 0xd4, 0xc5, 0xb4, 0xa1, 0x8d, 0x78, 0x61, 0x4a, 0x31, 0x18
};

// [=]===^=[ amb_read_u16_be ]====================================================================[=]
static uint16_t amb_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ amb_read_i16_be ]====================================================================[=]
static int16_t amb_read_i16_be(uint8_t *p) {
	return (int16_t)amb_read_u16_be(p);
}

// [=]===^=[ amb_read_u32_be ]====================================================================[=]
static uint32_t amb_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ amb_identify ]=======================================================================[=]
static int32_t amb_identify(uint8_t *data, uint32_t len) {
	if(len < 36) {
		return 0;
	}
	if((data[0] != 'A') || (data[1] != 'm') || (data[2] != 'B') || (data[3] != 'k')) {
		return 0;
	}
	uint16_t type = amb_read_u16_be(data + 4);
	if((type != 3) && (type < 5)) {
		return 0;
	}
	if(memcmp(data + 12, "Music   ", 8) != 0) {
		return 0;
	}
	return 1;
}

// [=]===^=[ amb_load_samples ]===================================================================[=]
static int32_t amb_load_samples(struct amosmusicbank_state *s, uint32_t sample_info_offset) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	if((sample_info_offset + 2) > data_len) {
		return 0;
	}

	uint16_t num_samples = amb_read_u16_be(data + sample_info_offset);
	sample_info_offset += 2;

	s->samples = (struct amb_sample *)calloc(num_samples, sizeof(struct amb_sample));
	if(!s->samples) {
		return 0;
	}
	s->num_samples = num_samples;

	uint32_t header_size = (uint32_t)num_samples * 30;
	if((sample_info_offset + header_size) > data_len) {
		return 0;
	}

	for(uint32_t i = 0; i < num_samples; ++i) {
		uint8_t *p = data + sample_info_offset + i * 30;
		uint32_t start_position  = amb_read_u32_be(p +  0);
		uint32_t loop_position   = amb_read_u32_be(p +  4);
		uint16_t non_loop_length = (uint16_t)(amb_read_u16_be(p +  8) * 2);
		uint16_t loop_length     = (uint16_t)(amb_read_u16_be(p + 10) * 2);
		uint16_t volume          = amb_read_u16_be(p + 12);
		uint16_t length          = (uint16_t)(amb_read_u16_be(p + 14) * 2);

		if(volume > 64) {
			volume = 64;
		}
		if(length <= 4) {
			length = non_loop_length;
		}

		struct amb_sample *smp = &s->samples[i];
		smp->volume = volume;

		if((loop_position - start_position) > length) {
			loop_length = 0;
		}

		if(loop_length > 4) {
			if((loop_position - start_position + loop_length) > length) {
				loop_length = (uint16_t)(length - (loop_position - start_position));
			}
			smp->loop_start = loop_position - start_position;
			smp->loop_length = loop_length;
			// Match original player: total playback length is "before loop" + "loop length"
			smp->length = smp->loop_start + smp->loop_length;
		} else {
			smp->loop_start = 0;
			smp->loop_length = 0;
			smp->length = length;
		}

		uint32_t sample_pos = sample_info_offset + start_position;
		if((sample_pos + smp->length) > data_len) {
			return 0;
		}
		smp->data = (int8_t *)(data + sample_pos);
	}

	return 1;
}

// [=]===^=[ amb_load_tracks ]====================================================================[=]
static int32_t amb_load_tracks(struct amosmusicbank_state *s, uint32_t track_offset, uint16_t **out_pattern_tracks, uint32_t *out_num_patterns) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	if((track_offset + 2) > data_len) {
		return 0;
	}

	uint16_t num_patterns = amb_read_u16_be(data + track_offset);
	uint32_t pattern_table_offset = track_offset + 2;

	if((pattern_table_offset + (uint32_t)num_patterns * 8) > data_len) {
		return 0;
	}

	// Map per-channel pattern offset -> assigned track number, build pattern->track table.
	// We use a flat dynamic dictionary: parallel arrays of u16 offset => u16 track index.
	uint16_t *taken_offsets = (uint16_t *)malloc((uint32_t)num_patterns * 4 * sizeof(uint16_t));
	if(!taken_offsets) {
		return 0;
	}
	uint32_t taken_count = 0;

	uint16_t *pattern_tracks = (uint16_t *)malloc((uint32_t)num_patterns * 4 * sizeof(uint16_t));
	if(!pattern_tracks) {
		free(taken_offsets);
		return 0;
	}

	for(uint32_t i = 0; i < num_patterns; ++i) {
		for(uint32_t j = 0; j < 4; ++j) {
			uint16_t offset = amb_read_u16_be(data + pattern_table_offset + (i * 4 + j) * 2);
			uint32_t k;
			for(k = 0; k < taken_count; ++k) {
				if(taken_offsets[k] == offset) {
					break;
				}
			}
			if(k == taken_count) {
				taken_offsets[taken_count++] = offset;
			}
			pattern_tracks[i * 4 + j] = (uint16_t)k;
		}
	}

	// Walk each unique track to determine total word count, then allocate.
	uint32_t total_words = 0;
	uint32_t *track_lengths = (uint32_t *)malloc(taken_count * sizeof(uint32_t));
	if(!track_lengths) {
		free(taken_offsets);
		free(pattern_tracks);
		return 0;
	}

	for(uint32_t k = 0; k < taken_count; ++k) {
		uint32_t pos = track_offset + (uint32_t)taken_offsets[k];
		uint32_t count = 0;
		for(;;) {
			if((pos + 2) > data_len) {
				free(taken_offsets);
				free(pattern_tracks);
				free(track_lengths);
				return 0;
			}
			uint16_t value = amb_read_u16_be(data + pos);
			pos += 2;
			count++;
			uint16_t hi = (uint16_t)(value & 0xff00);
			if((hi == 0x8000) || (hi == 0x9100)) {
				break;
			}
		}
		track_lengths[k] = count;
		total_words += count;
	}

	s->track_words = (uint16_t *)malloc(total_words * sizeof(uint16_t));
	if(!s->track_words) {
		free(taken_offsets);
		free(pattern_tracks);
		free(track_lengths);
		return 0;
	}
	s->num_track_words = total_words;

	s->tracks = (struct amb_track *)calloc(taken_count, sizeof(struct amb_track));
	if(!s->tracks) {
		free(taken_offsets);
		free(pattern_tracks);
		free(track_lengths);
		return 0;
	}
	s->num_tracks = taken_count;

	uint32_t write_pos = 0;
	for(uint32_t k = 0; k < taken_count; ++k) {
		uint32_t pos = track_offset + (uint32_t)taken_offsets[k];
		uint32_t count = track_lengths[k];
		s->tracks[k].data = &s->track_words[write_pos];
		s->tracks[k].length = count;
		for(uint32_t w = 0; w < count; ++w) {
			s->track_words[write_pos++] = amb_read_u16_be(data + pos);
			pos += 2;
		}
	}

	free(taken_offsets);
	free(track_lengths);

	*out_pattern_tracks = pattern_tracks;
	*out_num_patterns = num_patterns;
	return 1;
}

// [=]===^=[ amb_load_subsongs ]==================================================================[=]
static int32_t amb_load_subsongs(struct amosmusicbank_state *s, uint32_t song_data_offset, uint16_t *pattern_tracks, uint32_t num_patterns) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	if((song_data_offset + 2) > data_len) {
		return 0;
	}

	uint16_t num_songs = amb_read_u16_be(data + song_data_offset);
	if((song_data_offset + 2 + (uint32_t)num_songs * 4) > data_len) {
		return 0;
	}

	s->songs = (struct amb_song_info *)calloc(num_songs, sizeof(struct amb_song_info));
	if(!s->songs) {
		return 0;
	}
	s->num_songs = num_songs;

	uint32_t *song_offsets = (uint32_t *)malloc((uint32_t)num_songs * sizeof(uint32_t));
	if(!song_offsets) {
		return 0;
	}
	for(uint32_t i = 0; i < num_songs; ++i) {
		song_offsets[i] = amb_read_u32_be(data + song_data_offset + 2 + i * 4);
	}

	// First pass: count total position words required.
	uint32_t total_pos_words = 0;
	for(uint32_t i = 0; i < num_songs; ++i) {
		uint32_t song_offset = song_data_offset + song_offsets[i];
		if((song_offset + 8) > data_len) {
			free(song_offsets);
			return 0;
		}
		uint16_t pl_offsets[4];
		for(uint32_t j = 0; j < 4; ++j) {
			pl_offsets[j] = amb_read_u16_be(data + song_offset + j * 2);
		}
		for(uint32_t j = 0; j < 4; ++j) {
			uint32_t pos = song_offset + (uint32_t)pl_offsets[j];
			uint32_t count = 0;
			for(;;) {
				if((pos + 2) > data_len) {
					free(song_offsets);
					return 0;
				}
				int16_t value = amb_read_i16_be(data + pos);
				pos += 2;
				count++;
				if((value == -2) || (value == -1)) {
					break;
				}
			}
			total_pos_words += count;
		}
	}

	s->position_words = (int16_t *)malloc(total_pos_words * sizeof(int16_t));
	if(!s->position_words) {
		free(song_offsets);
		return 0;
	}
	s->num_position_words = total_pos_words;

	uint32_t write_pos = 0;
	for(uint32_t i = 0; i < num_songs; ++i) {
		uint32_t song_offset = song_data_offset + song_offsets[i];
		uint16_t pl_offsets[4];
		for(uint32_t j = 0; j < 4; ++j) {
			pl_offsets[j] = amb_read_u16_be(data + song_offset + j * 2);
		}

		for(uint32_t j = 0; j < 4; ++j) {
			uint32_t pos = song_offset + (uint32_t)pl_offsets[j];
			s->songs[i].position_lists[j].track_numbers = &s->position_words[write_pos];
			uint32_t count = 0;
			for(;;) {
				int16_t value = amb_read_i16_be(data + pos);
				pos += 2;
				if(value >= 0) {
					if((uint32_t)value < num_patterns) {
						value = (int16_t)pattern_tracks[(uint32_t)value * 4 + j];
					} else {
						value = -3;
					}
				}
				s->position_words[write_pos++] = value;
				count++;
				if((value == -2) || (value == -1)) {
					break;
				}
			}
			s->songs[i].position_lists[j].length = count;
		}
	}

	free(song_offsets);
	return 1;
}

// [=]===^=[ amb_cleanup ]========================================================================[=]
static void amb_cleanup(struct amosmusicbank_state *s) {
	if(!s) {
		return;
	}
	free(s->samples); s->samples = 0;
	free(s->tracks); s->tracks = 0;
	free(s->track_words); s->track_words = 0;
	free(s->songs); s->songs = 0;
	free(s->position_words); s->position_words = 0;
}

// [=]===^=[ amb_load ]===========================================================================[=]
static int32_t amb_load(struct amosmusicbank_state *s) {
	uint8_t *data = s->module_data;
	uint32_t data_len = s->module_len;

	if(data_len < 32) {
		return 0;
	}

	uint32_t sample_info_offset = amb_read_u32_be(data + 20) + 20;
	uint32_t song_data_offset   = amb_read_u32_be(data + 24) + 20;
	uint32_t track_offset       = amb_read_u32_be(data + 28) + 20;

	if(!amb_load_samples(s, sample_info_offset)) {
		return 0;
	}

	uint16_t *pattern_tracks = 0;
	uint32_t num_patterns = 0;
	if(!amb_load_tracks(s, track_offset, &pattern_tracks, &num_patterns)) {
		return 0;
	}

	int32_t ok = amb_load_subsongs(s, song_data_offset, pattern_tracks, num_patterns);
	free(pattern_tracks);
	if(!ok) {
		return 0;
	}

	return 1;
}

// [=]===^=[ amb_initialize_sound ]===============================================================[=]
static void amb_initialize_sound(struct amosmusicbank_state *s, uint32_t subsong) {
	struct amb_song_info *song = &s->songs[subsong];

	s->mu_cpt = s->tempo_base;
	s->mu_tempo = 17;

	s->empty_track[0] = 0x8000;

	for(int32_t i = 0; i < 4; ++i) {
		struct amb_voice *v = &s->voices[i];
		memset(v, 0, sizeof(*v));
		v->channel_number = i;
		v->voi_inst = 0;
		v->voi_inst_number = -1;
		v->voi_deb = -1;
		v->voi_cpt = 1;
		v->voi_adr = s->empty_track;
		v->voi_adr_index = 0;
		v->voi_pat = &song->position_lists[i];
		v->voi_pat_index = 0;
		v->voi_pat_d_index = 0;
		v->voi_effect = AMB_EFF_NONE;
	}
}

// [=]===^=[ amb_find_arpeggio_period ]===========================================================[=]
static uint16_t amb_find_arpeggio_period(uint16_t current_note, int32_t arp_value) {
	uint32_t periods_len = sizeof(amb_periods) / sizeof(amb_periods[0]);
	for(uint32_t i = 0; i < periods_len; ++i) {
		uint32_t target = i + (uint32_t)arp_value;
		if(target >= periods_len) {
			break;
		}
		uint16_t period = amb_periods[target];
		if(current_note >= amb_periods[i]) {
			return period;
		}
	}
	return 0;
}

// [=]===^=[ amb_no_effect ]======================================================================[=]
static void amb_no_effect(struct amosmusicbank_state *s, struct amb_voice *v) {
	paula_set_period(&s->paula, v->channel_number, v->voi_note);
}

// [=]===^=[ amb_arpeggio_effect ]================================================================[=]
static void amb_arpeggio_effect(struct amosmusicbank_state *s, struct amb_voice *v) {
	uint8_t val1 = (uint8_t)(v->voi_value & 0x00ff);
	uint8_t val2 = (uint8_t)((uint16_t)v->voi_value >> 8);

	if(val2 >= 3) {
		val2 = 2;
	}
	val2--;
	v->voi_value = (int16_t)(((uint16_t)val2 << 8) | val1);

	uint16_t new_period;
	if(val2 == 0) {
		new_period = v->voi_note;
	} else if((int8_t)val2 < 0) {
		new_period = amb_find_arpeggio_period(v->voi_note, (val1 >> 4));
	} else {
		new_period = amb_find_arpeggio_period(v->voi_note, (val1 & 0x0f));
	}

	paula_set_period(&s->paula, v->channel_number, new_period);
}

// [=]===^=[ amb_portamento_effect ]==============================================================[=]
static void amb_portamento_effect(struct amosmusicbank_state *s, struct amb_voice *v) {
	uint16_t new_period = v->voi_note;

	if(new_period == v->voi_p_to_to) {
		v->voi_effect = AMB_EFF_NONE;
	} else if(new_period < v->voi_p_to_to) {
		new_period = (uint16_t)(new_period + (uint16_t)v->voi_value);
		if(new_period >= v->voi_p_to_to) {
			new_period = v->voi_p_to_to;
			v->voi_effect = AMB_EFF_NONE;
		}
	} else {
		new_period = (uint16_t)(new_period - (uint16_t)v->voi_value);
		if(new_period <= v->voi_p_to_to) {
			new_period = v->voi_p_to_to;
			v->voi_effect = AMB_EFF_NONE;
		}
	}

	v->voi_note = new_period;
	paula_set_period(&s->paula, v->channel_number, new_period);
}

// [=]===^=[ amb_vibrato_effect ]=================================================================[=]
static void amb_vibrato_effect(struct amosmusicbank_state *s, struct amb_voice *v) {
	int32_t vib_index = (v->voi_vib / 4) & 0x1f;
	uint8_t vib_value = amb_sinus[vib_index];
	uint16_t speed = (uint16_t)(((uint16_t)v->voi_value & 0x0f) * vib_value / 64);

	uint16_t new_period = v->voi_note;
	if(v->voi_vib < 0) {
		new_period = (uint16_t)(new_period - speed);
	} else {
		new_period = (uint16_t)(new_period + speed);
	}

	paula_set_period(&s->paula, v->channel_number, new_period);

	v->voi_vib = (int8_t)(v->voi_vib + (int8_t)((v->voi_value / 4) & 0x3c));
}

// [=]===^=[ amb_volume_slide_effect ]============================================================[=]
static void amb_volume_slide_effect(struct amosmusicbank_state *s, struct amb_voice *v) {
	(void)s;
	int16_t new_volume = (int16_t)((int32_t)v->voi_d_vol + (int32_t)v->voi_value);
	if(new_volume < 0) {
		new_volume = 0;
	} else if(new_volume >= 64) {
		new_volume = 63;
	}
	v->voi_d_vol = (uint16_t)new_volume;
	v->voi_vol = (uint16_t)new_volume;
}

// [=]===^=[ amb_slide_effect ]===================================================================[=]
static void amb_slide_effect(struct amosmusicbank_state *s, struct amb_voice *v) {
	uint16_t new_period = v->voi_note;

	if(v->voi_value == 0) {
		v->voi_effect = AMB_EFF_NONE;
		return;
	}

	new_period = (uint16_t)(new_period + (uint16_t)v->voi_value);

	if(new_period < 113) {
		new_period = 113;
		v->voi_effect = AMB_EFF_NONE;
	} else if(new_period >= 856) {
		new_period = 856;
		v->voi_effect = AMB_EFF_NONE;
	}

	v->voi_note = new_period;
	paula_set_period(&s->paula, v->channel_number, new_period);
}

// [=]===^=[ amb_dispatch_effect ]================================================================[=]
static void amb_dispatch_effect(struct amosmusicbank_state *s, struct amb_voice *v) {
	switch(v->voi_effect) {
		case AMB_EFF_NONE: {
			amb_no_effect(s, v);
			break;
		}

		case AMB_EFF_ARPEGGIO: {
			amb_arpeggio_effect(s, v);
			break;
		}

		case AMB_EFF_PORTAMENTO: {
			amb_portamento_effect(s, v);
			break;
		}

		case AMB_EFF_VIBRATO: {
			amb_vibrato_effect(s, v);
			break;
		}

		case AMB_EFF_VOLUMESLIDE: {
			amb_volume_slide_effect(s, v);
			break;
		}

		case AMB_EFF_SLIDE: {
			amb_slide_effect(s, v);
			break;
		}
	}
}

// [=]===^=[ amb_fin_pattern_command ]============================================================[=]
static void amb_fin_pattern_command(struct amosmusicbank_state *s, struct amb_voice *v, int32_t *voi_adr) {
	v->voi_cpt = 0;
	v->voi_rep = 0;
	v->voi_deb = -1;
	v->voi_effect = AMB_EFF_NONE;

	int32_t voi_pat = v->voi_pat_index;
	int32_t one_more;

	do {
		one_more = 0;

		if((uint32_t)voi_pat >= v->voi_pat->length) {
			v->voi_pat_index = voi_pat;
			return;
		}

		int16_t position_value = v->voi_pat->track_numbers[voi_pat++];
		if(position_value < 0) {
			if(position_value == -3) {
				v->voi_pat_index = voi_pat;
				return;
			}

			if(position_value == -1) {
				return;
			}

			voi_pat = v->voi_pat_d_index;
			one_more = 1;
		} else {
			if((uint32_t)position_value >= s->num_tracks) {
				v->voi_pat_index = voi_pat;
				return;
			}
			v->voi_pat_index = voi_pat;
			v->voi_adr = s->tracks[position_value].data;
			*voi_adr = 0;
		}
	} while(one_more);
}

// [=]===^=[ amb_set_volume_command ]=============================================================[=]
static void amb_set_volume_command(struct amb_voice *v, uint16_t arg) {
	if(arg >= 64) {
		arg = 63;
	}
	v->voi_d_vol = arg;
	v->voi_vol = arg;
}

// [=]===^=[ amb_repeat_command ]=================================================================[=]
static void amb_repeat_command(struct amb_voice *v, uint16_t arg, int32_t *voi_adr) {
	if(arg == 0) {
		v->voi_deb = *voi_adr;
	} else {
		if(v->voi_rep == 0) {
			v->voi_rep = arg;
		} else {
			v->voi_rep--;
			if((v->voi_rep != 0) && (v->voi_deb != -1)) {
				*voi_adr = v->voi_deb;
			}
		}
	}
}

// [=]===^=[ amb_set_instrument_command ]=========================================================[=]
static void amb_set_instrument_command(struct amosmusicbank_state *s, struct amb_voice *v, uint16_t arg) {
	if(arg < s->num_samples) {
		v->voi_inst = &s->samples[arg];
		v->voi_inst_number = (int16_t)arg;
		uint16_t volume = v->voi_inst->volume;
		if(volume >= 64) {
			volume = 63;
		}
		v->voi_d_vol = volume;
		v->voi_vol = volume;
	}
}

// [=]===^=[ amb_position_jump_command ]==========================================================[=]
static void amb_position_jump_command(struct amosmusicbank_state *s, struct amb_voice *v, uint16_t arg, int32_t *voi_adr) {
	int32_t new_position = v->voi_pat_d_index + (int32_t)arg;
	v->voi_pat_index = new_position;
	amb_fin_pattern_command(s, v, voi_adr);
	v->voi_cpt = 1;
}

// [=]===^=[ amb_mu_step ]========================================================================[=]
static void amb_mu_step(struct amosmusicbank_state *s, struct amb_voice *v) {
	int32_t voi_adr = v->voi_adr_index;
	int32_t channel = v->channel_number;

	for(;;) {
		// safety: never run past the track buffer
		uint16_t track_value = v->voi_adr[voi_adr++];
		uint16_t command_argument = (uint16_t)(track_value & 0x00ff);

		if((track_value & 0x8000) == 0x8000) {
			switch(track_value & 0x7f00) {
				case 0x0000: {
					amb_fin_pattern_command(s, v, &voi_adr);
					break;
				}

				case 0x0100:
				case 0x0200: {
					break;
				}

				case 0x0300: {
					amb_set_volume_command(v, command_argument);
					break;
				}

				case 0x0400: {
					v->voi_effect = AMB_EFF_NONE;
					break;
				}

				case 0x0500: {
					amb_repeat_command(v, command_argument, &voi_adr);
					break;
				}

				case 0x0600: {
					paula_set_lp_filter(&s->paula, 1);
					break;
				}

				case 0x0700: {
					paula_set_lp_filter(&s->paula, 0);
					break;
				}

				case 0x0800: {
					s->mu_tempo = command_argument;
					break;
				}

				case 0x0900: {
					amb_set_instrument_command(s, v, command_argument);
					break;
				}

				case 0x0a00: {
					v->voi_value = (int16_t)command_argument;
					v->voi_effect = AMB_EFF_ARPEGGIO;
					break;
				}

				case 0x0b00: {
					v->voi_p_tone = 1;
					v->voi_value = (int16_t)command_argument;
					v->voi_effect = AMB_EFF_PORTAMENTO;
					break;
				}

				case 0x0c00: {
					v->voi_value = (int16_t)command_argument;
					v->voi_effect = AMB_EFF_VIBRATO;
					break;
				}

				case 0x0d00: {
					int16_t value = (int16_t)(command_argument >> 4);
					if(value == 0) {
						value = (int16_t)(-(int16_t)(command_argument & 0x0f));
					}
					v->voi_value = value;
					v->voi_effect = AMB_EFF_VOLUMESLIDE;
					break;
				}

				case 0x0e00: {
					v->voi_value = (int16_t)(-(int16_t)command_argument);
					v->voi_effect = AMB_EFF_SLIDE;
					break;
				}

				case 0x0f00: {
					v->voi_value = (int16_t)command_argument;
					v->voi_effect = AMB_EFF_SLIDE;
					break;
				}

				case 0x1000: {
					v->voi_cpt = command_argument;
					v->voi_adr_index = voi_adr;
					return;
				}

				case 0x1100: {
					amb_position_jump_command(s, v, command_argument, &voi_adr);
					v->voi_adr_index = voi_adr;
					return;
				}
			}
		} else {
			struct amb_sample *sample = v->voi_inst;

			if((track_value & 0x4000) == 0x4000) {
				v->voi_cpt = (uint16_t)(track_value & 0x00ff);

				track_value = v->voi_adr[voi_adr++];
				if(track_value != 0) {
					track_value &= 0x0fff;
					v->voi_note = track_value;
					paula_set_period(&s->paula, channel, track_value);

					if(sample != 0) {
						paula_play_sample(&s->paula, channel, sample->data, sample->length);
						if(sample->loop_length != 0) {
							paula_set_loop(&s->paula, channel, sample->loop_start, sample->loop_length);
						}
					}
				}

				v->voi_adr_index = voi_adr;
				break;
			}

			track_value &= 0x0fff;

			if(sample != 0) {
				paula_play_sample(&s->paula, channel, sample->data, sample->length);
				if(sample->loop_length != 0) {
					paula_set_loop(&s->paula, channel, sample->loop_start, sample->loop_length);
				}
			}

			paula_set_volume(&s->paula, channel, v->voi_vol);

			if(v->voi_p_tone) {
				v->voi_p_tone = 0;
				v->voi_p_to_to = track_value;
				v->voi_effect = AMB_EFF_PORTAMENTO;
			} else {
				v->voi_note = track_value;
				paula_set_period(&s->paula, channel, track_value);
			}

			v->voi_adr_index = voi_adr;
			break;
		}
	}
}

// [=]===^=[ amb_do_effects ]=====================================================================[=]
static void amb_do_effects(struct amosmusicbank_state *s) {
	for(int32_t i = 0; i < 4; ++i) {
		struct amb_voice *v = &s->voices[i];
		amb_dispatch_effect(s, v);
		paula_set_volume(&s->paula, v->channel_number, v->voi_vol);
	}
}

// [=]===^=[ amb_tick ]===========================================================================[=]
static void amb_tick(struct amosmusicbank_state *s) {
	s->mu_cpt = (uint16_t)(s->mu_cpt + s->mu_tempo);

	if(s->mu_cpt >= s->tempo_base) {
		s->mu_cpt = (uint16_t)(s->mu_cpt - s->tempo_base);

		for(int32_t i = 0; i < 4; ++i) {
			struct amb_voice *v = &s->voices[i];
			if(v->voi_cpt != 0) {
				v->voi_cpt--;
				if(v->voi_cpt == 0) {
					amb_mu_step(s, v);
				}
			}
		}
	} else {
		amb_do_effects(s);
	}
}

// [=]===^=[ amosmusicbank_init ]=================================================================[=]
static struct amosmusicbank_state *amosmusicbank_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || (len < 36) || (sample_rate < 8000)) {
		return 0;
	}
	if(!amb_identify((uint8_t *)data, len)) {
		return 0;
	}

	struct amosmusicbank_state *s = (struct amosmusicbank_state *)calloc(1, sizeof(struct amosmusicbank_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;
	s->tempo_base = AMB_TEMPO_BASE_PAL;

	if(!amb_load(s)) {
		amb_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, AMB_TICK_HZ);
	amb_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ amosmusicbank_free ]=================================================================[=]
static void amosmusicbank_free(struct amosmusicbank_state *s) {
	if(!s) {
		return;
	}
	amb_cleanup(s);
	free(s);
}

// [=]===^=[ amosmusicbank_get_audio ]============================================================[=]
static void amosmusicbank_get_audio(struct amosmusicbank_state *s, int16_t *output, int32_t frames) {
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
			amb_tick(s);
		}
	}
}

// [=]===^=[ amosmusicbank_api_init ]=============================================================[=]
static void *amosmusicbank_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return amosmusicbank_init(data, len, sample_rate);
}

// [=]===^=[ amosmusicbank_api_free ]=============================================================[=]
static void amosmusicbank_api_free(void *state) {
	amosmusicbank_free((struct amosmusicbank_state *)state);
}

// [=]===^=[ amosmusicbank_api_get_audio ]========================================================[=]
static void amosmusicbank_api_get_audio(void *state, int16_t *output, int32_t frames) {
	amosmusicbank_get_audio((struct amosmusicbank_state *)state, output, frames);
}

static const char *amosmusicbank_extensions[] = { "abk", 0 };

static struct player_api amosmusicbank_api = {
	"AMOS Music Bank",
	amosmusicbank_extensions,
	amosmusicbank_api_init,
	amosmusicbank_api_free,
	amosmusicbank_api_get_audio,
	0,
};
