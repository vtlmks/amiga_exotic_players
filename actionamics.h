// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Actionamics Sound Tool replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h) at 50Hz tick rate (PAL).
//
// Public API:
//   struct actionamics_state *actionamics_init(void *data, uint32_t len, int32_t sample_rate);
//   void actionamics_free(struct actionamics_state *s);
//   void actionamics_get_audio(struct actionamics_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define ACTIONAMICS_TICK_HZ 50

enum {
	ACTIONAMICS_ENV_DONE    = 0,
	ACTIONAMICS_ENV_ATTACK  = 1,
	ACTIONAMICS_ENV_DECAY   = 2,
	ACTIONAMICS_ENV_SUSTAIN = 3,
	ACTIONAMICS_ENV_RELEASE = 4,
};

enum {
	ACTIONAMICS_EFF_NONE                       = 0x00,
	ACTIONAMICS_EFF_ARPEGGIO                   = 0x70,
	ACTIONAMICS_EFF_SLIDE_UP                   = 0x71,
	ACTIONAMICS_EFF_SLIDE_DOWN                 = 0x72,
	ACTIONAMICS_EFF_VOLUME_SLIDE_AFTER_ENV     = 0x73,
	ACTIONAMICS_EFF_VIBRATO                    = 0x74,
	ACTIONAMICS_EFF_SET_ROWS                   = 0x75,
	ACTIONAMICS_EFF_SET_SAMPLE_OFFSET          = 0x76,
	ACTIONAMICS_EFF_NOTE_DELAY                 = 0x77,
	ACTIONAMICS_EFF_MUTE                       = 0x78,
	ACTIONAMICS_EFF_SAMPLE_RESTART             = 0x79,
	ACTIONAMICS_EFF_TREMOLO                    = 0x7a,
	ACTIONAMICS_EFF_BREAK                      = 0x7b,
	ACTIONAMICS_EFF_SET_VOLUME                 = 0x7c,
	ACTIONAMICS_EFF_VOLUME_SLIDE               = 0x7d,
	ACTIONAMICS_EFF_VOLUME_SLIDE_AND_VIBRATO   = 0x7e,
	ACTIONAMICS_EFF_SET_SPEED                  = 0x7f,
};

struct actionamics_position {
	uint8_t track_number;
	int8_t note_transpose;
	int8_t instrument_transpose;
};

struct actionamics_inst_list {
	uint8_t list_number;
	uint8_t num_values;
	uint8_t start_counter_delta;
	uint8_t counter_end;
};

struct actionamics_instrument {
	struct actionamics_inst_list sample_number_list;
	struct actionamics_inst_list arpeggio_list;
	struct actionamics_inst_list frequency_list;
	int8_t portamento_increment;
	uint8_t portamento_delay;
	int8_t note_transpose;
	uint8_t attack_end_volume;
	uint8_t attack_speed;
	uint8_t decay_end_volume;
	uint8_t decay_speed;
	uint8_t sustain_delay;
	uint8_t release_end_volume;
	uint8_t release_speed;
};

struct actionamics_sample {
	int8_t *data;             // points into module buffer (read-only)
	uint16_t length;          // in words
	uint16_t loop_start;      // in words
	uint16_t loop_length;     // in words
	uint8_t arpeggio_list_number;
	uint16_t effect_start_position;
	uint16_t effect_length;
	uint16_t effect_speed;
	uint16_t effect_mode;
	uint16_t counter_init_value;   // never set in source data, kept for parity
};

struct actionamics_sample_extra {
	int8_t *modified_data;    // owned clone, used when effect_mode != 0
	uint32_t modified_length;
	int16_t effect_increment_value;
	int32_t effect_position;
	uint16_t effect_speed_counter;
	uint8_t already_taken;
};

struct actionamics_song {
	uint8_t start_position;
	uint8_t end_position;
	uint8_t loop_position;
	uint8_t speed;
};

struct actionamics_track {
	uint8_t *data;
	uint32_t length;
};

struct actionamics_voice {
	struct actionamics_position *position_list;
	uint8_t *track_data;
	uint32_t track_length;
	uint32_t track_position;
	uint8_t delay_counter;

	uint16_t instrument_number;
	struct actionamics_instrument *instrument;
	int8_t instrument_transpose;

	uint16_t sample_number;
	int8_t *sample_data;
	uint32_t sample_data_length;
	uint32_t sample_offset;
	uint16_t sample_length;
	uint32_t sample_loop_start;
	uint16_t sample_loop_length;

	uint16_t note;
	int8_t note_transpose;
	uint16_t note_period;

	uint16_t final_note;
	uint16_t final_period;
	int16_t final_volume;
	uint16_t global_voice_volume;

	uint8_t envelope_state;
	uint16_t sustain_counter;

	uint8_t sample_number_list_speed_counter;
	int16_t sample_number_list_position;

	uint8_t arpeggio_list_speed_counter;
	int16_t arpeggio_list_position;

	uint8_t frequency_list_speed_counter;
	int16_t frequency_list_position;

	uint16_t effect;
	uint16_t effect_argument;

	uint8_t portamento_delay_counter;
	int16_t portamento_value;

	uint16_t tone_portamento_end_period;
	int16_t tone_portamento_increment_value;

	uint8_t vibrato_effect_argument;
	int8_t vibrato_table_index;

	uint8_t tremolo_effect_argument;
	int8_t tremolo_table_index;
	uint16_t tremolo_volume;

	uint16_t sample_offset_effect_argument;
	uint16_t note_delay_counter;

	uint16_t restart_delay_counter;
	int8_t *restart_sample_data;
	uint32_t restart_sample_data_length;
	uint32_t restart_sample_offset;
	uint16_t restart_sample_length;

	uint8_t trig_sample;
};

struct actionamics_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint16_t tempo;

	struct actionamics_song *songs;
	uint32_t num_songs;
	struct actionamics_song *current_song;

	struct actionamics_position *positions[4];
	uint32_t num_positions;

	struct actionamics_instrument *instruments;
	uint32_t num_instruments;

	struct actionamics_sample *samples;
	struct actionamics_sample_extra *sample_extras;
	uint32_t num_samples;

	int8_t (*sample_number_list)[16];
	uint32_t num_sample_number_lists;

	int8_t (*arpeggio_list)[16];
	uint32_t num_arpeggio_lists;

	int8_t (*frequency_list)[16];
	uint32_t num_frequency_lists;

	struct actionamics_track *tracks;
	uint32_t num_tracks;

	uint8_t speed_counter;
	uint8_t current_speed;
	uint8_t measure_counter;
	uint8_t current_position;
	uint8_t loop_position;
	uint8_t end_position;
	uint8_t current_row_position;
	uint8_t number_of_rows;

	struct actionamics_voice voices[4];
};

// [=]===^=[ actionamics_periods ]=================================================================[=]
static uint16_t actionamics_periods[] = {
	   0,
	                  5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3816,
	3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1808,
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  904,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
	 107,  101,   95
};

// [=]===^=[ actionamics_sinus ]===================================================================[=]
static uint8_t actionamics_sinus[] = {
	  0,  24,  49,  74,  97, 120, 141, 161,
	180, 197, 212, 224, 235, 244, 250, 253,
	255, 253, 250, 244, 235, 224, 212, 197,
	180, 161, 141, 120,  97,  74,  49,  24
};

// [=]===^=[ actionamics_read_u16_be ]=============================================================[=]
static uint16_t actionamics_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ actionamics_read_u32_be ]=============================================================[=]
static uint32_t actionamics_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ actionamics_read_i16_be ]=============================================================[=]
static int16_t actionamics_read_i16_be(uint8_t *p) {
	return (int16_t)actionamics_read_u16_be(p);
}

// [=]===^=[ actionamics_read_i32_be ]=============================================================[=]
static int32_t actionamics_read_i32_be(uint8_t *p) {
	return (int32_t)actionamics_read_u32_be(p);
}

// [=]===^=[ actionamics_cleanup ]=================================================================[=]
static void actionamics_cleanup(struct actionamics_state *s) {
	if(!s) {
		return;
	}
	free(s->songs); s->songs = 0;
	for(int32_t i = 0; i < 4; ++i) {
		free(s->positions[i]); s->positions[i] = 0;
	}
	free(s->instruments); s->instruments = 0;
	if(s->sample_extras) {
		for(uint32_t i = 0; i < s->num_samples; ++i) {
			free(s->sample_extras[i].modified_data);
		}
		free(s->sample_extras);
		s->sample_extras = 0;
	}
	free(s->samples); s->samples = 0;
	free(s->sample_number_list); s->sample_number_list = 0;
	free(s->arpeggio_list); s->arpeggio_list = 0;
	free(s->frequency_list); s->frequency_list = 0;
	if(s->tracks) {
		for(uint32_t i = 0; i < s->num_tracks; ++i) {
			free(s->tracks[i].data);
		}
		free(s->tracks);
		s->tracks = 0;
	}
}

// [=]===^=[ actionamics_load ]====================================================================[=]
static int32_t actionamics_load(struct actionamics_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	if(len < 90) {
		return 0;
	}
	if(memcmp(data + 62, "ACTIONAMICS SOUND TOOL", 22) != 0) {
		return 0;
	}

	s->tempo = actionamics_read_u16_be(data + 0);

	uint32_t lengths[15];
	for(uint32_t i = 0; i < 15; ++i) {
		lengths[i] = actionamics_read_u32_be(data + 2 + i * 4);
	}

	uint32_t cursor = 2 + 15 * 4;
	cursor += lengths[0];

	if((cursor + 4) > len) {
		return 0;
	}
	uint32_t total_length = actionamics_read_u32_be(data + cursor);
	if(total_length > len) {
		return 0;
	}
	cursor += lengths[1];

	uint32_t track_number_length      = lengths[2];
	uint32_t instrument_transpose_len = lengths[3];
	uint32_t note_transpose_len       = lengths[4];

	if((track_number_length != instrument_transpose_len) || (track_number_length != note_transpose_len)) {
		return 0;
	}

	uint32_t pos_count = track_number_length / 4;
	s->num_positions = pos_count;
	for(int32_t i = 0; i < 4; ++i) {
		s->positions[i] = (struct actionamics_position *)calloc(pos_count, sizeof(struct actionamics_position));
		if(!s->positions[i]) {
			return 0;
		}
	}

	if((cursor + track_number_length + note_transpose_len + instrument_transpose_len) > len) {
		return 0;
	}

	uint32_t base = cursor;
	for(int32_t i = 0; i < 4; ++i) {
		for(uint32_t j = 0; j < pos_count; ++j) {
			s->positions[i][j].track_number = data[base + i * pos_count + j];
		}
	}
	base += track_number_length;
	for(int32_t i = 0; i < 4; ++i) {
		for(uint32_t j = 0; j < pos_count; ++j) {
			s->positions[i][j].note_transpose = (int8_t)data[base + i * pos_count + j];
		}
	}
	base += note_transpose_len;
	for(int32_t i = 0; i < 4; ++i) {
		for(uint32_t j = 0; j < pos_count; ++j) {
			s->positions[i][j].instrument_transpose = (int8_t)data[base + i * pos_count + j];
		}
	}
	cursor += track_number_length + note_transpose_len + instrument_transpose_len;

	uint32_t instrument_length = lengths[5];
	if((cursor + instrument_length) > len) {
		return 0;
	}
	uint32_t inst_count = instrument_length / 32;
	s->num_instruments = inst_count;
	s->instruments = (struct actionamics_instrument *)calloc(inst_count, sizeof(struct actionamics_instrument));
	if(!s->instruments && inst_count > 0) {
		return 0;
	}

	for(uint32_t i = 0; i < inst_count; ++i) {
		uint8_t *p = data + cursor + i * 32;
		struct actionamics_instrument *ins = &s->instruments[i];
		ins->sample_number_list.list_number          = p[0];
		ins->sample_number_list.num_values           = p[1];
		ins->sample_number_list.start_counter_delta  = p[2];
		ins->sample_number_list.counter_end          = p[3];
		ins->arpeggio_list.list_number               = p[4];
		ins->arpeggio_list.num_values                = p[5];
		ins->arpeggio_list.start_counter_delta       = p[6];
		ins->arpeggio_list.counter_end               = p[7];
		ins->frequency_list.list_number              = p[8];
		ins->frequency_list.num_values               = p[9];
		ins->frequency_list.start_counter_delta      = p[10];
		ins->frequency_list.counter_end              = p[11];
		ins->portamento_increment                    = (int8_t)p[12];
		ins->portamento_delay                        = p[13];
		ins->note_transpose                          = (int8_t)p[14];
		// p[15] skipped
		ins->attack_end_volume                       = p[16];
		ins->attack_speed                            = p[17];
		ins->decay_end_volume                        = p[18];
		ins->decay_speed                             = p[19];
		ins->sustain_delay                           = p[20];
		ins->release_end_volume                      = p[21];
		ins->release_speed                           = p[22];
		// p[23..31] skipped
	}
	cursor += instrument_length;

	uint32_t sn_len = lengths[6];
	uint32_t ar_len = lengths[7];
	uint32_t fr_len = lengths[8];

	if((cursor + sn_len) > len) {
		return 0;
	}
	s->num_sample_number_lists = sn_len / 16;
	s->sample_number_list = (int8_t (*)[16])calloc(s->num_sample_number_lists, 16);
	if(!s->sample_number_list && s->num_sample_number_lists > 0) {
		return 0;
	}
	memcpy(s->sample_number_list, data + cursor, s->num_sample_number_lists * 16);
	cursor += sn_len;

	if((cursor + ar_len) > len) {
		return 0;
	}
	s->num_arpeggio_lists = ar_len / 16;
	s->arpeggio_list = (int8_t (*)[16])calloc(s->num_arpeggio_lists, 16);
	if(!s->arpeggio_list && s->num_arpeggio_lists > 0) {
		return 0;
	}
	memcpy(s->arpeggio_list, data + cursor, s->num_arpeggio_lists * 16);
	cursor += ar_len;

	if((cursor + fr_len) > len) {
		return 0;
	}
	s->num_frequency_lists = fr_len / 16;
	s->frequency_list = (int8_t (*)[16])calloc(s->num_frequency_lists, 16);
	if(!s->frequency_list && s->num_frequency_lists > 0) {
		return 0;
	}
	memcpy(s->frequency_list, data + cursor, s->num_frequency_lists * 16);
	cursor += fr_len;

	cursor += lengths[9] + lengths[10];

	uint32_t subsong_length = lengths[11];
	if((cursor + subsong_length) > len) {
		return 0;
	}
	uint32_t raw_count = subsong_length / 4;
	struct actionamics_song *temp = (struct actionamics_song *)calloc(raw_count, sizeof(struct actionamics_song));
	if(!temp && raw_count > 0) {
		return 0;
	}
	uint32_t valid = 0;
	for(uint32_t i = 0; i < raw_count; ++i) {
		uint8_t *p = data + cursor + i * 4;
		uint8_t sp = p[0], ep = p[1], lp = p[2], sd = p[3];
		if((sp != 0) || (ep != 0) || (lp != 0)) {
			temp[valid].start_position = sp;
			temp[valid].end_position = ep;
			temp[valid].loop_position = lp;
			temp[valid].speed = sd;
			valid++;
		}
	}
	s->songs = temp;
	s->num_songs = valid;
	cursor += subsong_length;

	cursor += lengths[12];

	uint32_t sample_info_length = lengths[13];
	if((cursor + sample_info_length) > len) {
		return 0;
	}
	uint32_t scount = sample_info_length / 64;
	s->num_samples = scount;
	s->samples = (struct actionamics_sample *)calloc(scount, sizeof(struct actionamics_sample));
	s->sample_extras = (struct actionamics_sample_extra *)calloc(scount, sizeof(struct actionamics_sample_extra));
	if((!s->samples || !s->sample_extras) && scount > 0) {
		return 0;
	}
	for(uint32_t i = 0; i < scount; ++i) {
		uint8_t *p = data + cursor + i * 64;
		struct actionamics_sample *sm = &s->samples[i];
		struct actionamics_sample_extra *se = &s->sample_extras[i];
		// p[0..3] pointer to data, skipped
		sm->length          = actionamics_read_u16_be(p + 4);
		sm->loop_start      = actionamics_read_u16_be(p + 6);
		sm->loop_length     = actionamics_read_u16_be(p + 8);
		sm->effect_start_position = actionamics_read_u16_be(p + 10);
		sm->effect_length   = actionamics_read_u16_be(p + 12);
		sm->arpeggio_list_number = (uint8_t)(sm->effect_length >> 8);
		sm->effect_speed    = actionamics_read_u16_be(p + 14);
		sm->effect_mode     = actionamics_read_u16_be(p + 16);
		se->effect_increment_value = actionamics_read_i16_be(p + 18);
		se->effect_position = actionamics_read_i32_be(p + 20);
		se->effect_speed_counter = actionamics_read_u16_be(p + 24);
		se->already_taken = (actionamics_read_u16_be(p + 26) != 0) ? 1 : 0;
		// p[28..31] skipped
		// p[32..63] is name, skipped
		sm->counter_init_value = 0;
	}
	cursor += sample_info_length;

	uint32_t track_offset_length = lengths[14];
	if((cursor + track_offset_length) > len) {
		return 0;
	}
	uint32_t offsets_count = track_offset_length / 2;
	if(offsets_count < 2) {
		return 0;
	}
	uint16_t *offsets = (uint16_t *)malloc(offsets_count * sizeof(uint16_t));
	if(!offsets) {
		return 0;
	}
	for(uint32_t i = 0; i < offsets_count; ++i) {
		offsets[i] = actionamics_read_u16_be(data + cursor + i * 2);
	}
	cursor += track_offset_length;

	uint32_t track_count = offsets_count - 1;
	s->num_tracks = track_count;
	s->tracks = (struct actionamics_track *)calloc(track_count, sizeof(struct actionamics_track));
	if(!s->tracks && track_count > 0) {
		free(offsets);
		return 0;
	}

	uint32_t track_data_base = cursor;
	for(uint32_t i = 0; i < track_count; ++i) {
		uint32_t track_off = track_data_base + offsets[i];
		uint32_t track_len = (uint32_t)offsets[i + 1] - (uint32_t)offsets[i];
		if((track_off + track_len) > len) {
			free(offsets);
			return 0;
		}
		s->tracks[i].length = track_len;
		s->tracks[i].data = (uint8_t *)malloc(track_len);
		if(!s->tracks[i].data) {
			free(offsets);
			return 0;
		}
		memcpy(s->tracks[i].data, data + track_off, track_len);
	}
	free(offsets);

	// Sample data lives at the end of the file.
	uint32_t total_sample_bytes = 0;
	for(uint32_t i = 0; i < scount; ++i) {
		total_sample_bytes += (uint32_t)s->samples[i].length * 2u;
	}
	if(total_sample_bytes > total_length) {
		return 0;
	}
	uint32_t sample_data_pos = total_length - total_sample_bytes;
	if(sample_data_pos > len) {
		return 0;
	}
	for(uint32_t i = 0; i < scount; ++i) {
		uint32_t sl = (uint32_t)s->samples[i].length * 2u;
		if(sl > 0) {
			if((sample_data_pos + sl) > len) {
				return 0;
			}
			s->samples[i].data = (int8_t *)(data + sample_data_pos);
			sample_data_pos += sl;
		} else {
			s->samples[i].data = 0;
		}
	}

	return 1;
}

// [=]===^=[ actionamics_initialize_sound ]========================================================[=]
static void actionamics_initialize_sound(struct actionamics_state *s, uint32_t subsong) {
	if(subsong >= s->num_songs) {
		subsong = 0;
	}
	s->current_song = &s->songs[subsong];

	s->speed_counter = 0;
	s->current_speed = s->current_song->speed;
	s->measure_counter = 0;
	s->current_position = s->current_song->start_position;
	s->loop_position = s->current_song->loop_position;
	s->end_position = s->current_song->end_position;
	s->current_row_position = 0;
	s->number_of_rows = 64;

	for(int32_t i = 0; i < 4; ++i) {
		struct actionamics_voice *v = &s->voices[i];
		memset(v, 0, sizeof(*v));
		v->position_list = s->positions[i];
		v->envelope_state = ACTIONAMICS_ENV_DONE;
		v->global_voice_volume = 0x4041;

		// SetupTrack
		struct actionamics_position *pi = &v->position_list[s->current_position];
		if(pi->track_number < s->num_tracks) {
			v->track_data = s->tracks[pi->track_number].data;
			v->track_length = s->tracks[pi->track_number].length;
		}
		v->track_position = 0;
		v->note_transpose = pi->note_transpose;
		v->instrument_transpose = pi->instrument_transpose;
		v->delay_counter = 0;
	}

	// Allocate modified sample data clones for samples with effect_mode != 0.
	for(int32_t i = (int32_t)s->num_samples - 1; i >= 0; --i) {
		struct actionamics_sample *sm = &s->samples[i];
		struct actionamics_sample_extra *se = &s->sample_extras[i];
		if((sm->effect_mode != 0) && (sm->data != 0) && (sm->length > 0)) {
			free(se->modified_data);
			uint32_t bytes = (uint32_t)sm->length * 2u;
			se->modified_data = (int8_t *)malloc(bytes);
			if(se->modified_data) {
				memcpy(se->modified_data, sm->data, bytes);
				se->modified_length = bytes;
			}
		}
	}
}

// [=]===^=[ actionamics_setup_track ]=============================================================[=]
static void actionamics_setup_track(struct actionamics_state *s, struct actionamics_voice *v) {
	struct actionamics_position *pi = &v->position_list[s->current_position];
	if(pi->track_number < s->num_tracks) {
		v->track_data = s->tracks[pi->track_number].data;
		v->track_length = s->tracks[pi->track_number].length;
	} else {
		v->track_data = 0;
		v->track_length = 0;
	}
	v->track_position = 0;
	v->note_transpose = pi->note_transpose;
	v->instrument_transpose = pi->instrument_transpose;
	v->delay_counter = 0;
}

// [=]===^=[ actionamics_read_track_data ]=========================================================[=]
static void actionamics_read_track_data(struct actionamics_voice *v) {
	v->note = 0;
	v->instrument_number = 0;
	v->effect = 0;
	v->effect_argument = 0;

	if(v->delay_counter == 0) {
		uint8_t *td = v->track_data;
		if(!td || v->track_position >= v->track_length) {
			return;
		}

		uint8_t d = td[v->track_position++];
		if((d & 0x80) != 0) {
			v->delay_counter = (uint8_t)(~d);
			return;
		}
		if(d >= 0x70) {
			v->effect = d;
			if(v->track_position < v->track_length) {
				v->effect_argument = td[v->track_position++];
			}
			return;
		}
		v->note = d;

		if(v->track_position >= v->track_length) {
			return;
		}
		d = td[v->track_position++];
		if((d & 0x80) != 0) {
			v->delay_counter = (uint8_t)(~d);
			return;
		}
		if(d >= 0x70) {
			v->effect = d;
			if(v->track_position < v->track_length) {
				v->effect_argument = td[v->track_position++];
			}
			return;
		}
		v->instrument_number = d;

		if(v->track_position >= v->track_length) {
			return;
		}
		d = td[v->track_position++];
		if((d & 0x80) != 0) {
			v->delay_counter = (uint8_t)(~d);
			return;
		}
		v->effect = d;
		if(v->track_position < v->track_length) {
			v->effect_argument = td[v->track_position++];
		}
	} else {
		v->delay_counter--;
	}
}

// [=]===^=[ actionamics_read_next_row ]===========================================================[=]
static void actionamics_read_next_row(struct actionamics_voice *v) {
	actionamics_read_track_data(v);
	if(v->note != 0) {
		v->note = (uint16_t)(v->note + v->note_transpose);
		v->trig_sample = 1;
	}
}

// [=]===^=[ actionamics_setup_note_and_sample ]===================================================[=]
static void actionamics_setup_note_and_sample(struct actionamics_state *s, struct actionamics_voice *v) {
	v->portamento_value = 0;

	if(v->note != 0) {
		v->final_volume = 0;
		v->sample_number_list_speed_counter = 0;
		v->sample_number_list_position = 0;
		v->arpeggio_list_speed_counter = 0;
		v->arpeggio_list_position = 0;
		v->frequency_list_speed_counter = 0;
		v->frequency_list_position = 0;
		v->portamento_delay_counter = 0;
		v->tone_portamento_increment_value = 0;
		v->envelope_state = ACTIONAMICS_ENV_ATTACK;
		v->sustain_counter = 0;

		if(v->instrument_number != 0) {
			int32_t idx = (int32_t)v->instrument_number - 1 + (int32_t)v->instrument_transpose;
			if((idx >= 0) && ((uint32_t)idx < s->num_instruments)) {
				v->instrument = &s->instruments[idx];
			}
		}

		if(!v->instrument) {
			return;
		}

		v->final_note = (uint16_t)(v->note + v->instrument->note_transpose);

		uint8_t list_num = v->instrument->sample_number_list.list_number;
		if(list_num >= s->num_sample_number_lists) {
			return;
		}
		int8_t sample_number = s->sample_number_list[list_num][0];
		if(sample_number < 0 || (uint32_t)sample_number >= s->num_samples) {
			return;
		}
		v->sample_number = (uint16_t)sample_number;
		struct actionamics_sample *sm = &s->samples[sample_number];

		v->sample_data = sm->data;
		v->sample_data_length = (uint32_t)sm->length * 2u;
		v->sample_offset = 0;
		v->sample_length = sm->length;
		v->sample_loop_start = (uint32_t)sm->loop_start * 2u;
		v->sample_loop_length = sm->loop_length;

		uint8_t arp_list = sm->arpeggio_list_number;
		int8_t arp_off = 0;
		if(arp_list < s->num_arpeggio_lists) {
			arp_off = s->arpeggio_list[arp_list][0];
		}
		int32_t note_idx = (int32_t)v->final_note + (int32_t)arp_off;
		if(note_idx >= 0 && note_idx < (int32_t)(sizeof(actionamics_periods) / sizeof(actionamics_periods[0]))) {
			v->note_period = actionamics_periods[note_idx];
		} else {
			v->note_period = 0;
		}
		v->final_period = v->note_period;
	}
}

// [=]===^=[ actionamics_do_sample_inversion ]=====================================================[=]
static void actionamics_do_sample_inversion(struct actionamics_state *s) {
	struct actionamics_sample_extra *taken[4];
	uint32_t taken_count = 0;

	for(int32_t i = 0; i < 4; ++i) {
		struct actionamics_voice *v = &s->voices[i];
		if(v->sample_number >= s->num_samples) {
			continue;
		}
		struct actionamics_sample *sm = &s->samples[v->sample_number];
		struct actionamics_sample_extra *se = &s->sample_extras[v->sample_number];
		taken[taken_count++] = se;

		if(!se->already_taken) {
			se->already_taken = 1;

			if(se->effect_speed_counter == 0) {
				se->effect_speed_counter = sm->counter_init_value;

				if((sm->effect_mode != 0) && (se->modified_data != 0)) {
					int32_t end_position = (int32_t)sm->effect_length * 2 - 1;
					int32_t position = (int32_t)sm->effect_start_position * 2 + se->effect_position;

					if(position >= 0 && (uint32_t)position < se->modified_length) {
						se->modified_data[position] = (int8_t)(~se->modified_data[position]);
					}

					se->effect_position += se->effect_increment_value;

					if(se->effect_position < 0) {
						if(sm->effect_mode == 2) {
							se->effect_position = end_position;
						} else {
							se->effect_position -= se->effect_increment_value;
							se->effect_increment_value = (int16_t)(-se->effect_increment_value);
						}
					} else {
						if(se->effect_position <= end_position) {
							if(sm->effect_mode == 1) {
								se->effect_position = 0;
							} else {
								se->effect_position -= se->effect_increment_value;
								se->effect_increment_value = (int16_t)(-se->effect_increment_value);
							}
						}
					}
				}
			} else {
				se->effect_speed_counter--;
				se->effect_speed_counter &= 0x1f;
			}
		}
	}

	for(uint32_t i = 0; i < taken_count; ++i) {
		taken[i]->already_taken = 0;
	}
}

// [=]===^=[ actionamics_do_envelope ]=============================================================[=]
static void actionamics_do_envelope(struct actionamics_voice *v) {
	struct actionamics_instrument *instr = v->instrument;
	if(!instr) {
		return;
	}
	switch(v->envelope_state) {
		case ACTIONAMICS_ENV_ATTACK: {
			v->final_volume += instr->attack_speed;
			if(v->final_volume >= instr->attack_end_volume) {
				v->final_volume = instr->attack_end_volume;
				v->envelope_state = ACTIONAMICS_ENV_DECAY;
			}
			break;
		}

		case ACTIONAMICS_ENV_DECAY: {
			if(instr->decay_speed != 0) {
				v->final_volume -= instr->decay_speed;
				if(v->final_volume <= instr->decay_end_volume) {
					v->final_volume = instr->decay_end_volume;
					v->envelope_state = ACTIONAMICS_ENV_SUSTAIN;
				}
			} else {
				v->envelope_state = ACTIONAMICS_ENV_SUSTAIN;
			}
			break;
		}

		case ACTIONAMICS_ENV_SUSTAIN: {
			if(v->sustain_counter != instr->sustain_delay) {
				v->sustain_counter++;
			} else {
				v->envelope_state = ACTIONAMICS_ENV_RELEASE;
			}
			break;
		}

		case ACTIONAMICS_ENV_RELEASE: {
			if(instr->release_speed != 0) {
				v->final_volume -= instr->release_speed;
				if(v->final_volume <= instr->release_end_volume) {
					v->final_volume = instr->release_end_volume;
					v->envelope_state = ACTIONAMICS_ENV_DONE;
				}
			} else {
				v->envelope_state = ACTIONAMICS_ENV_DONE;
			}
			break;
		}

		default: break;
	}
}

// [=]===^=[ actionamics_set_volume ]==============================================================[=]
static void actionamics_set_volume(struct actionamics_state *s, struct actionamics_voice *v, int32_t voice_idx) {
	(void)s;
	actionamics_do_envelope(v);

	v->tremolo_effect_argument = 0;
	v->tremolo_table_index = (int8_t)v->final_volume;

	uint16_t volume = (uint16_t)(((uint32_t)v->final_volume * (uint32_t)v->global_voice_volume) >> 16);
	paula_set_volume(&s->paula, voice_idx, volume);
}

// [=]===^=[ actionamics_do_sample_number_list ]===================================================[=]
static void actionamics_do_sample_number_list(struct actionamics_state *s, struct actionamics_voice *v, int32_t voice_idx) {
	struct actionamics_instrument *instr = v->instrument;
	if(!instr) {
		return;
	}
	struct actionamics_inst_list *list = &instr->sample_number_list;
	if(list->num_values == 0) {
		return;
	}
	if(v->sample_number_list_speed_counter == list->counter_end) {
		v->sample_number_list_speed_counter = (uint8_t)(list->counter_end - list->start_counter_delta);

		if(v->sample_number_list_position == list->num_values) {
			v->sample_number_list_position = -1;
		}
		v->sample_number_list_position++;

		if(list->list_number >= s->num_sample_number_lists) {
			return;
		}
		int8_t sample_number = s->sample_number_list[list->list_number][v->sample_number_list_position];

		if(sample_number < 0) {
			v->sample_number_list_position--;
		} else {
			if((uint32_t)sample_number >= s->num_samples) {
				return;
			}
			v->sample_number = (uint16_t)sample_number;
			struct actionamics_sample *sm = &s->samples[sample_number];

			v->sample_data = sm->data;
			v->sample_data_length = (uint32_t)sm->length * 2u;
			v->sample_offset = 0;
			v->sample_loop_start = 0;
			v->sample_loop_length = sm->length;

			// SetSample maps to paula_queue_sample (deferred swap on next wrap).
			uint32_t loop_bytes = (uint32_t)v->sample_loop_length * 2u;
			paula_queue_sample(&s->paula, voice_idx, sm->data, v->sample_loop_start, loop_bytes);
			paula_set_loop(&s->paula, voice_idx, v->sample_loop_start, loop_bytes);
		}
	} else {
		v->sample_number_list_speed_counter++;
	}
}

// [=]===^=[ actionamics_do_arpeggio_list ]========================================================[=]
static void actionamics_do_arpeggio_list(struct actionamics_state *s, struct actionamics_voice *v) {
	struct actionamics_instrument *instr = v->instrument;
	if(!instr) {
		return;
	}
	struct actionamics_inst_list *list = &instr->arpeggio_list;
	if(list->num_values == 0) {
		return;
	}
	if(v->arpeggio_list_speed_counter == list->counter_end) {
		v->arpeggio_list_speed_counter = (uint8_t)(list->counter_end - list->start_counter_delta);

		if(v->arpeggio_list_position == list->num_values) {
			v->arpeggio_list_position = -1;
		}
		v->arpeggio_list_position++;

		if(list->list_number >= s->num_arpeggio_lists) {
			return;
		}
		int8_t arp = s->arpeggio_list[list->list_number][v->arpeggio_list_position];
		int32_t idx = (int32_t)arp + (int32_t)v->final_note;
		if(idx >= 0 && idx < (int32_t)(sizeof(actionamics_periods) / sizeof(actionamics_periods[0]))) {
			v->final_period = actionamics_periods[idx];
		}
	} else {
		v->arpeggio_list_speed_counter++;
	}
}

// [=]===^=[ actionamics_do_frequency_list ]=======================================================[=]
static void actionamics_do_frequency_list(struct actionamics_state *s, struct actionamics_voice *v) {
	struct actionamics_instrument *instr = v->instrument;
	if(!instr) {
		return;
	}
	struct actionamics_inst_list *list = &instr->frequency_list;
	if(list->num_values == 0) {
		return;
	}
	if(v->frequency_list_speed_counter == list->counter_end) {
		v->frequency_list_speed_counter = (uint8_t)(list->counter_end - list->start_counter_delta);

		if(v->frequency_list_position == list->num_values) {
			v->frequency_list_position = -1;
		}
		v->frequency_list_position++;

		if(list->list_number >= s->num_frequency_lists) {
			return;
		}
		int8_t freq_val = s->frequency_list[list->list_number][v->frequency_list_position];
		v->final_period = (uint16_t)(v->final_period + freq_val);
	} else {
		v->frequency_list_speed_counter++;
	}
}

// [=]===^=[ actionamics_do_portamento ]===========================================================[=]
static void actionamics_do_portamento(struct actionamics_voice *v) {
	struct actionamics_instrument *instr = v->instrument;
	if(!instr) {
		return;
	}
	if(instr->portamento_increment != 0) {
		if(v->portamento_delay_counter == instr->portamento_delay) {
			v->portamento_value += instr->portamento_increment;
		} else {
			v->portamento_delay_counter++;
		}
	}
}

// [=]===^=[ actionamics_do_tone_portamento ]======================================================[=]
static void actionamics_do_tone_portamento(struct actionamics_voice *v) {
	if((v->effect != ACTIONAMICS_EFF_NONE) && (v->effect < ACTIONAMICS_EFF_ARPEGGIO) && (v->effect_argument != 0)) {
		struct actionamics_instrument *instr = v->instrument;
		if(instr) {
			int32_t end_note = (int32_t)((int8_t)v->effect) + (int32_t)v->note_transpose + (int32_t)instr->note_transpose;
			if(end_note >= 0 && end_note < (int32_t)(sizeof(actionamics_periods) / sizeof(actionamics_periods[0]))) {
				v->tone_portamento_end_period = actionamics_periods[end_note];
			}
			int32_t speed = (int32_t)v->effect_argument;
			int32_t delta = (int32_t)v->tone_portamento_end_period - (int32_t)v->final_period;
			if(delta != 0) {
				if(delta < 0) {
					speed = -speed;
				}
				v->tone_portamento_increment_value = (int16_t)speed;
			}
		}
	}

	if(v->tone_portamento_increment_value != 0) {
		if(v->tone_portamento_increment_value < 0) {
			v->final_period = (uint16_t)(v->final_period + v->tone_portamento_increment_value);
			if(v->final_period <= v->tone_portamento_end_period) {
				v->tone_portamento_increment_value = 0;
				v->final_period = v->tone_portamento_end_period;
				v->note_period = v->final_period;
			}
		} else {
			v->final_period = (uint16_t)(v->final_period + v->tone_portamento_increment_value);
			if(v->final_period >= v->tone_portamento_end_period) {
				v->tone_portamento_increment_value = 0;
				v->final_period = v->tone_portamento_end_period;
				v->note_period = v->final_period;
			}
		}
	}
}

// [=]===^=[ actionamics_do_volume_slide ]=========================================================[=]
static void actionamics_do_volume_slide(struct actionamics_voice *v) {
	uint16_t arg = v->effect_argument;
	if((arg & 0x0f) != 0) {
		int32_t volume = (int32_t)v->final_volume - (int32_t)(arg * 4);
		if(volume < 0) {
			volume = 0;
		}
		v->final_volume = (int16_t)volume;
		v->tremolo_effect_argument = 0;
		v->tremolo_table_index = (int8_t)volume;
	} else {
		int32_t volume = (int32_t)v->final_volume + (int32_t)((arg >> 4) * 4);
		if(volume > 255) {
			volume = 255;
		}
		v->final_volume = (int16_t)volume;
		v->tremolo_effect_argument = 0;
		v->tremolo_table_index = (int8_t)volume;
	}
}

// [=]===^=[ actionamics_do_vibrato ]==============================================================[=]
static void actionamics_do_vibrato(struct actionamics_voice *v) {
	uint8_t val = actionamics_sinus[(v->vibrato_table_index >> 2) & 0x1f];
	int32_t vib_val = (((int32_t)(v->vibrato_effect_argument & 0x0f) * (int32_t)val) >> 7);
	if(v->vibrato_table_index >= 0) {
		vib_val = -vib_val;
	}
	v->final_period = (uint16_t)((int32_t)v->note_period - vib_val);
	v->vibrato_table_index += (int8_t)(v->vibrato_effect_argument >> 2);
}

// [=]===^=[ actionamics_eff_arpeggio ]============================================================[=]
static void actionamics_eff_arpeggio(struct actionamics_state *s, struct actionamics_voice *v) {
	uint16_t arg = v->effect_argument;
	uint8_t arp[4];
	arp[0] = (uint8_t)(arg >> 4);
	arp[1] = 0;
	arp[2] = (uint8_t)(arg & 0x0f);
	arp[3] = 0;
	uint8_t arp_val = arp[s->measure_counter & 3];
	int32_t idx = (int32_t)v->final_note + (int32_t)arp_val;
	if(idx >= 0 && idx < (int32_t)(sizeof(actionamics_periods) / sizeof(actionamics_periods[0]))) {
		v->final_period = actionamics_periods[idx];
	}
}

// [=]===^=[ actionamics_eff_slide_up ]============================================================[=]
static void actionamics_eff_slide_up(struct actionamics_voice *v) {
	v->portamento_value = (int16_t)(-(int32_t)v->effect_argument);
	v->note_period = (uint16_t)((int32_t)v->note_period + v->portamento_value);
}

// [=]===^=[ actionamics_eff_slide_down ]==========================================================[=]
static void actionamics_eff_slide_down(struct actionamics_voice *v) {
	v->portamento_value = (int16_t)v->effect_argument;
	v->note_period = (uint16_t)((int32_t)v->note_period + v->portamento_value);
}

// [=]===^=[ actionamics_eff_volume_slide_after_envelope ]=========================================[=]
static void actionamics_eff_volume_slide_after_envelope(struct actionamics_state *s, struct actionamics_voice *v) {
	if(v->envelope_state == ACTIONAMICS_ENV_DONE) {
		if((s->speed_counter == 0) && (v->instrument_number != 0)) {
			if(!v->instrument) {
				return;
			}
			v->final_volume = v->instrument->attack_speed;
		}
		actionamics_do_volume_slide(v);
	}
}

// [=]===^=[ actionamics_eff_vibrato ]=============================================================[=]
static void actionamics_eff_vibrato(struct actionamics_voice *v) {
	uint16_t arg = v->effect_argument;
	if(arg != 0) {
		uint8_t new_arg = v->vibrato_effect_argument;
		if((arg & 0x0f) != 0) {
			new_arg = (uint8_t)((new_arg & 0xf0) | (arg & 0x0f));
		}
		if((arg & 0xf0) != 0) {
			new_arg = (uint8_t)((new_arg & 0x0f) | (arg & 0xf0));
		}
		v->vibrato_effect_argument = new_arg;
	}
	actionamics_do_vibrato(v);
}

// [=]===^=[ actionamics_eff_set_rows ]============================================================[=]
static void actionamics_eff_set_rows(struct actionamics_state *s, struct actionamics_voice *v) {
	s->number_of_rows = (uint8_t)v->effect_argument;
}

// [=]===^=[ actionamics_eff_set_sample_offset ]===================================================[=]
static void actionamics_eff_set_sample_offset(struct actionamics_voice *v) {
	if(v->effect_argument != 0) {
		v->sample_offset_effect_argument = v->effect_argument;
	}
	uint16_t offset = (uint16_t)(v->sample_offset_effect_argument << 7);
	if(offset < v->sample_length) {
		v->sample_length = (uint16_t)(v->sample_length - offset);
		v->sample_offset += (uint32_t)offset * 2u;
	} else {
		v->sample_length = 1;
	}
}

// [=]===^=[ actionamics_eff_note_delay ]==========================================================[=]
static void actionamics_eff_note_delay(struct actionamics_voice *v) {
	if(v->effect_argument != 0) {
		v->note_delay_counter = v->effect_argument;
		v->effect = ACTIONAMICS_EFF_NONE;
		v->effect_argument = 0;
	}
}

// [=]===^=[ actionamics_eff_mute ]================================================================[=]
static void actionamics_eff_mute(struct actionamics_state *s, struct actionamics_voice *v, int32_t voice_idx) {
	if((v->effect_argument != 0) && (s->speed_counter == 0)) {
		v->final_volume = 0;
		v->tremolo_effect_argument = 0;
		v->tremolo_table_index = 0;
		paula_set_volume(&s->paula, voice_idx, 0);
	}
}

// [=]===^=[ actionamics_eff_sample_restart ]======================================================[=]
static void actionamics_eff_sample_restart(struct actionamics_voice *v) {
	if(v->effect_argument != 0) {
		v->restart_delay_counter = v->effect_argument;
		v->restart_sample_data = v->sample_data;
		v->restart_sample_data_length = v->sample_data_length;
		v->restart_sample_offset = v->sample_offset;
		v->restart_sample_length = v->sample_length;
		v->effect = ACTIONAMICS_EFF_NONE;
		v->effect_argument = 0;
	}
}

// [=]===^=[ actionamics_eff_tremolo ]=============================================================[=]
static void actionamics_eff_tremolo(struct actionamics_state *s, struct actionamics_voice *v, int32_t voice_idx) {
	uint16_t arg = v->effect_argument;
	if(arg != 0) {
		uint8_t new_arg = v->tremolo_effect_argument;
		if((arg & 0x0f) != 0) {
			new_arg = (uint8_t)((new_arg & 0xf0) | (arg & 0x0f));
		}
		if((arg & 0xf0) != 0) {
			new_arg = (uint8_t)((new_arg & 0x0f) | (arg & 0xf0));
		}
		v->tremolo_effect_argument = new_arg;
	}

	uint8_t val = actionamics_sinus[(v->tremolo_table_index >> 2) & 0x1f];
	int32_t vib_val = (((int32_t)(v->tremolo_effect_argument & 0x0f) * (int32_t)val) >> 6);
	if(v->tremolo_table_index >= 0) {
		vib_val = -vib_val;
	}
	int32_t volume = (int32_t)v->tremolo_volume - vib_val;
	if(volume < 0) {
		volume = 0;
	}
	if(volume > 64) {
		volume = 64;
	}
	volume *= 4;
	if(volume == 256) {
		volume = 255;
	}
	v->final_volume = (int16_t)volume;
	uint16_t out_vol = (uint16_t)(((uint32_t)volume * (uint32_t)v->global_voice_volume) >> 16);
	paula_set_volume(&s->paula, voice_idx, out_vol);

	v->tremolo_table_index += (int8_t)((v->tremolo_effect_argument >> 2) & 0x3c);
}

// [=]===^=[ actionamics_eff_break ]===============================================================[=]
static void actionamics_eff_break(struct actionamics_state *s) {
	s->current_row_position = (uint8_t)(s->number_of_rows - 1);
}

// [=]===^=[ actionamics_eff_set_volume ]==========================================================[=]
static void actionamics_eff_set_volume(struct actionamics_state *s, struct actionamics_voice *v, int32_t voice_idx) {
	int32_t volume = (int32_t)v->effect_argument * 4;
	if(volume > 255) {
		volume = 255;
	}
	v->final_volume = (int16_t)volume;
	uint16_t out_vol = (uint16_t)(((uint32_t)volume * (uint32_t)v->global_voice_volume) >> 16);
	paula_set_volume(&s->paula, voice_idx, out_vol);
}

// [=]===^=[ actionamics_eff_volume_slide ]========================================================[=]
static void actionamics_eff_volume_slide(struct actionamics_state *s, struct actionamics_voice *v) {
	if(s->speed_counter == 0) {
		if(!v->instrument) {
			return;
		}
		v->final_volume = v->instrument->attack_speed;
	}
	actionamics_do_volume_slide(v);
}

// [=]===^=[ actionamics_eff_volume_slide_and_vibrato ]============================================[=]
static void actionamics_eff_volume_slide_and_vibrato(struct actionamics_state *s, struct actionamics_voice *v) {
	actionamics_eff_volume_slide_after_envelope(s, v);
	actionamics_do_vibrato(v);
}

// [=]===^=[ actionamics_eff_set_speed ]===========================================================[=]
static void actionamics_eff_set_speed(struct actionamics_state *s, struct actionamics_voice *v) {
	if(v->effect_argument < 31) {
		s->current_speed = (uint8_t)v->effect_argument;
	}
}

// [=]===^=[ actionamics_handle_track_effects ]====================================================[=]
static void actionamics_handle_track_effects(struct actionamics_state *s, struct actionamics_voice *v, int32_t voice_idx) {
	if(s->speed_counter != 0) {
		switch(v->effect) {
			case ACTIONAMICS_EFF_ARPEGGIO: {
				actionamics_eff_arpeggio(s, v);
				break;
			}

			case ACTIONAMICS_EFF_SLIDE_UP: {
				actionamics_eff_slide_up(v);
				break;
			}

			case ACTIONAMICS_EFF_SLIDE_DOWN: {
				actionamics_eff_slide_down(v);
				break;
			}

			case ACTIONAMICS_EFF_VOLUME_SLIDE_AFTER_ENV: {
				actionamics_eff_volume_slide_after_envelope(s, v);
				break;
			}

			case ACTIONAMICS_EFF_VIBRATO: {
				actionamics_eff_vibrato(v);
				break;
			}
		}
	}

	switch(v->effect) {
		case ACTIONAMICS_EFF_SET_ROWS: {
			actionamics_eff_set_rows(s, v);
			break;
		}

		case ACTIONAMICS_EFF_SET_SAMPLE_OFFSET: {
			actionamics_eff_set_sample_offset(v);
			break;
		}

		case ACTIONAMICS_EFF_NOTE_DELAY: {
			actionamics_eff_note_delay(v);
			break;
		}

		case ACTIONAMICS_EFF_MUTE: {
			actionamics_eff_mute(s, v, voice_idx);
			break;
		}

		case ACTIONAMICS_EFF_SAMPLE_RESTART: {
			actionamics_eff_sample_restart(v);
			break;
		}

		case ACTIONAMICS_EFF_TREMOLO: {
			actionamics_eff_tremolo(s, v, voice_idx);
			break;
		}

		case ACTIONAMICS_EFF_BREAK: {
			actionamics_eff_break(s);
			break;
		}

		case ACTIONAMICS_EFF_SET_VOLUME: {
			actionamics_eff_set_volume(s, v, voice_idx);
			break;
		}

		case ACTIONAMICS_EFF_VOLUME_SLIDE: {
			actionamics_eff_volume_slide(s, v);
			break;
		}

		case ACTIONAMICS_EFF_VOLUME_SLIDE_AND_VIBRATO: {
			actionamics_eff_volume_slide_and_vibrato(s, v);
			break;
		}

		case ACTIONAMICS_EFF_SET_SPEED: {
			actionamics_eff_set_speed(s, v);
			break;
		}
	}
}

// [=]===^=[ actionamics_do_effects ]==============================================================[=]
static void actionamics_do_effects(struct actionamics_state *s, struct actionamics_voice *v, int32_t voice_idx) {
	actionamics_set_volume(s, v, voice_idx);
	actionamics_do_sample_number_list(s, v, voice_idx);
	actionamics_handle_track_effects(s, v, voice_idx);
	actionamics_do_arpeggio_list(s, v);
	actionamics_do_frequency_list(s, v);
	actionamics_do_portamento(v);
	actionamics_do_tone_portamento(v);

	int32_t period = (int32_t)v->final_period + (int32_t)v->portamento_value;
	if(period < 95) {
		period = 95;
	} else if(period > 5760) {
		period = 5760;
	}
	v->final_period = (uint16_t)period;

	paula_set_period(&s->paula, voice_idx, v->final_period);
}

// [=]===^=[ actionamics_tick ]====================================================================[=]
static void actionamics_tick(struct actionamics_state *s) {
	s->measure_counter++;
	if(s->measure_counter == 3) {
		s->measure_counter = 0;
	}

	s->speed_counter++;

	if(s->speed_counter == s->current_speed) {
		s->speed_counter = 0;
		s->measure_counter = 0;

		for(int32_t i = 0; i < 4; ++i) {
			struct actionamics_voice *v = &s->voices[i];
			actionamics_read_next_row(v);
			actionamics_setup_note_and_sample(s, v);
		}

		actionamics_do_sample_inversion(s);

		s->current_row_position++;
		if(s->current_row_position == s->number_of_rows) {
			s->current_row_position = 0;
			uint8_t position = s->current_position;
			s->current_position++;
			if(position == s->end_position) {
				s->current_position = s->loop_position;
			}
			for(int32_t i = 0; i < 4; ++i) {
				actionamics_setup_track(s, &s->voices[i]);
			}
		}
	}

	for(int32_t i = 0; i < 4; ++i) {
		struct actionamics_voice *v = &s->voices[i];

		if(v->restart_delay_counter != 0) {
			v->restart_delay_counter--;
			if(v->restart_delay_counter == 0) {
				v->sample_data = v->restart_sample_data;
				v->sample_data_length = v->restart_sample_data_length;
				v->sample_offset = v->restart_sample_offset;
				v->sample_length = v->restart_sample_length;
				v->trig_sample = 1;
			}
		}

		actionamics_do_effects(s, v, i);

		if(v->note_delay_counter == 0) {
			if(v->trig_sample) {
				v->trig_sample = 0;
				if(v->sample_data && v->sample_data_length > 0) {
					uint32_t play_len = (uint32_t)v->sample_length * 2u;
					int8_t *base = v->sample_data;
					uint32_t base_len = v->sample_data_length;
					if(v->sample_number < s->num_samples) {
						struct actionamics_sample *sm = &s->samples[v->sample_number];
						struct actionamics_sample_extra *se = &s->sample_extras[v->sample_number];
						if((sm->effect_mode != 0) && (se->modified_data != 0)) {
							base = se->modified_data;
							base_len = se->modified_length;
						}
					}
					uint32_t total_off = v->sample_offset;
					if(total_off > base_len) {
						total_off = base_len;
					}
					if(total_off + play_len > base_len) {
						play_len = base_len - total_off;
					}
					paula_play_sample(&s->paula, i, base + total_off, play_len);
					if(v->sample_loop_length > 1) {
						uint32_t loop_bytes = (uint32_t)v->sample_loop_length * 2u;
						paula_set_loop(&s->paula, i, v->sample_loop_start, loop_bytes);
					}
				}
			}
			paula_set_period(&s->paula, i, v->final_period);
		} else {
			v->note_delay_counter--;
		}
	}

	if(s->speed_counter != 0) {
		actionamics_do_sample_inversion(s);
	}
}

// [=]===^=[ actionamics_init ]====================================================================[=]
static struct actionamics_state *actionamics_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || (len < 90) || (sample_rate < 8000)) {
		return 0;
	}

	struct actionamics_state *s = (struct actionamics_state *)calloc(1, sizeof(struct actionamics_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;

	if(!actionamics_load(s)) {
		actionamics_cleanup(s);
		free(s);
		return 0;
	}
	if(s->num_songs == 0) {
		actionamics_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, ACTIONAMICS_TICK_HZ);
	actionamics_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ actionamics_free ]====================================================================[=]
static void actionamics_free(struct actionamics_state *s) {
	if(!s) {
		return;
	}
	actionamics_cleanup(s);
	free(s);
}

// [=]===^=[ actionamics_get_audio ]===============================================================[=]
static void actionamics_get_audio(struct actionamics_state *s, int16_t *output, int32_t frames) {
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
			actionamics_tick(s);
		}
	}
}

// [=]===^=[ actionamics_api_init ]================================================================[=]
static void *actionamics_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return actionamics_init(data, len, sample_rate);
}

// [=]===^=[ actionamics_api_free ]================================================================[=]
static void actionamics_api_free(void *state) {
	actionamics_free((struct actionamics_state *)state);
}

// [=]===^=[ actionamics_api_get_audio ]===========================================================[=]
static void actionamics_api_get_audio(void *state, int16_t *output, int32_t frames) {
	actionamics_get_audio((struct actionamics_state *)state, output, frames);
}

static const char *actionamics_extensions[] = { "ast", 0 };

static struct player_api actionamics_api = {
	"Actionamics Sound Tool",
	actionamics_extensions,
	actionamics_api_init,
	actionamics_api_free,
	actionamics_api_get_audio,
	0,
};
