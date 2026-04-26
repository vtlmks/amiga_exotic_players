// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// SidMon 2.0 (SidMon II - The MIDI Version) replayer, ported from
// NostalgicPlayer's C# implementation. Drives a 4-channel Amiga Paula
// (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct sidmon20_state *sidmon20_init(void *data, uint32_t len, int32_t sample_rate);
//   void sidmon20_free(struct sidmon20_state *s);
//   void sidmon20_get_audio(struct sidmon20_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define SIDMON20_TICK_HZ          50
#define SIDMON20_NUM_PERIODS      75
#define SIDMON20_LIST_ENTRY_SIZE  16
#define SIDMON20_INSTRUMENT_SIZE  32
#define SIDMON20_MAGIC_OFFSET     58
#define SIDMON20_MAGIC_LEN        28
#define SIDMON20_HEADER_SIZE      86

enum {
	SIDMON20_ENV_DONE    = 0,
	SIDMON20_ENV_ATTACK  = 1,
	SIDMON20_ENV_DECAY   = 2,
	SIDMON20_ENV_SUSTAIN = 3,
	SIDMON20_ENV_RELEASE = 4,
};

struct sidmon20_block_info {
	uint32_t offset;
	uint32_t length;
};

struct sidmon20_sequence {
	uint8_t track_number;
	int8_t note_transpose;
	int8_t instrument_transpose;
};

struct sidmon20_track {
	uint8_t *track_data;
	uint32_t track_length;
};

struct sidmon20_instrument {
	uint8_t waveform_list_number;
	uint8_t waveform_list_length;
	uint8_t waveform_list_speed;
	uint8_t waveform_list_delay;
	uint8_t arpeggio_number;
	uint8_t arpeggio_length;
	uint8_t arpeggio_speed;
	uint8_t arpeggio_delay;
	uint8_t vibrato_number;
	uint8_t vibrato_length;
	uint8_t vibrato_speed;
	uint8_t vibrato_delay;
	int8_t pitch_bend_speed;
	uint8_t pitch_bend_delay;
	uint8_t attack_max;
	uint8_t attack_speed;
	uint8_t decay_min;
	uint8_t decay_speed;
	uint8_t sustain_time;
	uint8_t release_min;
	uint8_t release_speed;
};

struct sidmon20_sample {
	int8_t *sample_data;
	uint32_t length;
	uint32_t loop_start;
	uint32_t loop_length;
};

struct sidmon20_negate_info {
	uint32_t start_offset;
	uint32_t end_offset;
	uint16_t loop_index;
	uint16_t status;
	int16_t speed;
	int32_t position;
	uint16_t index;
	int16_t do_negation;
};

struct sidmon20_voice {
	struct sidmon20_sequence *sequence_list;
	int8_t *sample_data;
	uint32_t sample_length;
	uint16_t sample_period;
	int16_t sample_volume;
	uint8_t envelope_state;
	uint16_t sustain_counter;
	struct sidmon20_instrument *instrument;
	int8_t *loop_sample;
	uint32_t loop_offset;
	uint32_t loop_length;
	uint16_t original_note;

	uint16_t wave_list_delay;
	int16_t wave_list_offset;
	uint16_t arpeggio_delay;
	int16_t arpeggio_offset;
	uint16_t vibrato_delay;
	int16_t vibrato_offset;

	uint16_t current_note;
	uint16_t current_instrument;
	uint16_t current_effect;
	uint16_t current_effect_arg;

	uint16_t pitch_bend_counter;
	int16_t instrument_transpose;
	int16_t pitch_bend_value;
	uint16_t note_slide_note;
	int16_t note_slide_speed;
	int32_t track_position;
	uint8_t *current_track;
	uint32_t current_track_length;
	uint16_t empty_notes_counter;
	int16_t note_transpose;
	uint16_t current_sample;
};

struct sidmon20_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint8_t (*waveform_info)[SIDMON20_LIST_ENTRY_SIZE];
	uint32_t num_waveform_info;

	int8_t (*arpeggios)[SIDMON20_LIST_ENTRY_SIZE];
	uint32_t num_arpeggios;

	int8_t (*vibratoes)[SIDMON20_LIST_ENTRY_SIZE];
	uint32_t num_vibratoes;

	struct sidmon20_sequence *sequences[4];

	struct sidmon20_track *tracks;
	uint32_t num_tracks;

	struct sidmon20_instrument *instruments;
	uint32_t num_instruments;

	struct sidmon20_sample *samples;
	struct sidmon20_negate_info *sample_negate_info;
	uint32_t num_samples;

	uint8_t number_of_positions;
	uint8_t start_speed;

	int8_t current_position;
	uint8_t current_row;
	uint8_t pattern_length;
	uint8_t current_rast;
	uint8_t current_rast2;
	uint8_t speed;

	struct sidmon20_voice voices[4];

	uint8_t end_reached;
};

// [=]===^=[ sidmon20_periods ]===================================================================[=]
static uint16_t sidmon20_periods[SIDMON20_NUM_PERIODS] = {
	   0,
	                  5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3616,
	3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1808,
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  904,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
	 107,  101,   95
};

// [=]===^=[ sidmon20_read_u16_be ]===============================================================[=]
static uint16_t sidmon20_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ sidmon20_read_i16_be ]===============================================================[=]
static int16_t sidmon20_read_i16_be(uint8_t *p) {
	return (int16_t)sidmon20_read_u16_be(p);
}

// [=]===^=[ sidmon20_read_u32_be ]===============================================================[=]
static uint32_t sidmon20_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ sidmon20_read_i32_be ]===============================================================[=]
static int32_t sidmon20_read_i32_be(uint8_t *p) {
	return (int32_t)sidmon20_read_u32_be(p);
}

// [=]===^=[ sidmon20_check_magic ]===============================================================[=]
static int32_t sidmon20_check_magic(uint8_t *buf, uint32_t len) {
	static char magic[SIDMON20_MAGIC_LEN] = {
		'S','I','D','M','O','N',' ','I','I',' ','-',' ','T','H','E',' ',
		'M','I','D','I',' ','V','E','R','S','I','O','N'
	};
	if(len < SIDMON20_HEADER_SIZE) {
		return 0;
	}
	for(int32_t i = 0; i < SIDMON20_MAGIC_LEN; ++i) {
		if(buf[SIDMON20_MAGIC_OFFSET + i] != (uint8_t)magic[i]) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ sidmon20_load_song_data ]============================================================[=]
static int32_t sidmon20_load_song_data(struct sidmon20_state *s, uint32_t *out_num_samples,
		struct sidmon20_block_info *position_info,
		struct sidmon20_block_info *note_transpose_info,
		struct sidmon20_block_info *instrument_transpose_info,
		struct sidmon20_block_info *instruments_info,
		struct sidmon20_block_info *waveform_list_info,
		struct sidmon20_block_info *arpeggios_info,
		struct sidmon20_block_info *vibratoes_info,
		struct sidmon20_block_info *sample_info_info,
		struct sidmon20_block_info *track_table_info,
		struct sidmon20_block_info *tracks_info) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	if(len < SIDMON20_MAGIC_OFFSET + 8) {
		return 0;
	}

	s->number_of_positions = (uint8_t)(data[2] + 1);
	s->start_speed = data[3];

	uint32_t num_samples = sidmon20_read_u16_be(data + 4) / 64;
	*out_num_samples = num_samples;

	uint32_t offset = SIDMON20_MAGIC_OFFSET;
	uint32_t pos = 6;

	if(pos + 4 > len) {
		return 0;
	}
	offset += sidmon20_read_u32_be(data + pos);
	pos += 4;

	if(pos + 4 > len) {
		return 0;
	}
	offset += sidmon20_read_u32_be(data + pos);
	pos += 4;

	struct sidmon20_block_info *blocks[10] = {
		position_info, note_transpose_info, instrument_transpose_info,
		instruments_info, waveform_list_info, arpeggios_info,
		vibratoes_info, sample_info_info, track_table_info, tracks_info
	};

	for(int32_t i = 0; i < 10; ++i) {
		if(pos + 4 > len) {
			return 0;
		}
		uint32_t length = sidmon20_read_u32_be(data + pos);
		pos += 4;
		blocks[i]->offset = offset;
		blocks[i]->length = length;
		offset += length;
	}

	return 1;
}

// [=]===^=[ sidmon20_load_single_list ]==========================================================[=]
static void *sidmon20_load_single_list(struct sidmon20_state *s, struct sidmon20_block_info *info, uint32_t *out_count) {
	if((info->length % SIDMON20_LIST_ENTRY_SIZE) != 0) {
		return 0;
	}
	if(info->offset + info->length > s->module_len) {
		return 0;
	}

	uint32_t count = info->length / SIDMON20_LIST_ENTRY_SIZE;
	uint8_t *list = (uint8_t *)calloc((size_t)count, SIDMON20_LIST_ENTRY_SIZE);
	if(!list) {
		return 0;
	}

	memcpy(list, s->module_data + info->offset, count * SIDMON20_LIST_ENTRY_SIZE);
	*out_count = count;
	return list;
}

// [=]===^=[ sidmon20_load_lists ]================================================================[=]
static int32_t sidmon20_load_lists(struct sidmon20_state *s,
		struct sidmon20_block_info *waveform_list_info,
		struct sidmon20_block_info *arpeggios_info,
		struct sidmon20_block_info *vibratoes_info) {
	s->waveform_info = (uint8_t (*)[SIDMON20_LIST_ENTRY_SIZE])sidmon20_load_single_list(s, waveform_list_info, &s->num_waveform_info);
	if(!s->waveform_info) {
		return 0;
	}

	s->arpeggios = (int8_t (*)[SIDMON20_LIST_ENTRY_SIZE])sidmon20_load_single_list(s, arpeggios_info, &s->num_arpeggios);
	if(!s->arpeggios) {
		return 0;
	}

	s->vibratoes = (int8_t (*)[SIDMON20_LIST_ENTRY_SIZE])sidmon20_load_single_list(s, vibratoes_info, &s->num_vibratoes);
	if(!s->vibratoes) {
		return 0;
	}

	return 1;
}

// [=]===^=[ sidmon20_load_sequences ]============================================================[=]
static int32_t sidmon20_load_sequences(struct sidmon20_state *s,
		struct sidmon20_block_info *position_info,
		struct sidmon20_block_info *note_transpose_info,
		struct sidmon20_block_info *instrument_transpose_info) {
	if((position_info->length != note_transpose_info->length) ||
	   (note_transpose_info->length != instrument_transpose_info->length)) {
		return 0;
	}

	uint32_t num_pos = s->number_of_positions;
	if(position_info->length != num_pos * 4) {
		return 0;
	}

	if(position_info->offset + position_info->length > s->module_len) {
		return 0;
	}
	if(note_transpose_info->offset + note_transpose_info->length > s->module_len) {
		return 0;
	}
	if(instrument_transpose_info->offset + instrument_transpose_info->length > s->module_len) {
		return 0;
	}

	uint8_t *positions = s->module_data + position_info->offset;
	int8_t *note_transpose = (int8_t *)(s->module_data + note_transpose_info->offset);
	int8_t *instr_transpose = (int8_t *)(s->module_data + instrument_transpose_info->offset);

	uint32_t start_offset = 0;
	for(int32_t i = 0; i < 4; ++i, start_offset += num_pos) {
		s->sequences[i] = (struct sidmon20_sequence *)calloc((size_t)num_pos, sizeof(struct sidmon20_sequence));
		if(!s->sequences[i]) {
			return 0;
		}
		for(uint32_t j = 0; j < num_pos; ++j) {
			s->sequences[i][j].track_number = positions[start_offset + j];
			s->sequences[i][j].note_transpose = note_transpose[start_offset + j];
			s->sequences[i][j].instrument_transpose = instr_transpose[start_offset + j];
		}
	}

	return 1;
}

// [=]===^=[ sidmon20_load_tracks ]===============================================================[=]
static int32_t sidmon20_load_tracks(struct sidmon20_state *s,
		struct sidmon20_block_info *track_table_info,
		struct sidmon20_block_info *tracks_info,
		uint32_t *out_sample_offset) {
	if((track_table_info->length % 2) != 0) {
		return 0;
	}
	if(track_table_info->offset + track_table_info->length > s->module_len) {
		return 0;
	}

	uint32_t num_tracks = track_table_info->length / 2;
	if(num_tracks == 0) {
		return 0;
	}

	s->tracks = (struct sidmon20_track *)calloc((size_t)num_tracks, sizeof(struct sidmon20_track));
	if(!s->tracks) {
		return 0;
	}
	s->num_tracks = num_tracks;

	for(uint32_t i = 0; i < num_tracks; ++i) {
		uint16_t track_offset = sidmon20_read_u16_be(s->module_data + track_table_info->offset + i * 2);
		uint16_t next_offset = (i == num_tracks - 1) ? (uint16_t)tracks_info->length : sidmon20_read_u16_be(s->module_data + track_table_info->offset + (i + 1) * 2);
		if(next_offset < track_offset) {
			return 0;
		}
		uint32_t track_length = (uint32_t)(next_offset - track_offset);

		uint32_t src_pos = tracks_info->offset + track_offset;
		if(src_pos + track_length > s->module_len) {
			return 0;
		}

		s->tracks[i].track_data = (uint8_t *)malloc(track_length > 0 ? track_length : 1);
		if(!s->tracks[i].track_data) {
			return 0;
		}
		if(track_length > 0) {
			memcpy(s->tracks[i].track_data, s->module_data + src_pos, track_length);
		}
		s->tracks[i].track_length = track_length;
	}

	*out_sample_offset = tracks_info->offset + tracks_info->length;
	if((*out_sample_offset & 1) != 0) {
		(*out_sample_offset)++;
	}

	return 1;
}

// [=]===^=[ sidmon20_load_instruments ]==========================================================[=]
static int32_t sidmon20_load_instruments(struct sidmon20_state *s, struct sidmon20_block_info *instruments_info) {
	if((instruments_info->length % SIDMON20_INSTRUMENT_SIZE) != 0) {
		return 0;
	}
	if(instruments_info->offset + instruments_info->length > s->module_len) {
		return 0;
	}

	uint32_t count = instruments_info->length / SIDMON20_INSTRUMENT_SIZE;
	s->instruments = (struct sidmon20_instrument *)calloc((size_t)count, sizeof(struct sidmon20_instrument));
	if(!s->instruments) {
		return 0;
	}
	s->num_instruments = count;

	for(uint32_t i = 0; i < count; ++i) {
		uint8_t *p = s->module_data + instruments_info->offset + i * SIDMON20_INSTRUMENT_SIZE;
		struct sidmon20_instrument *ins = &s->instruments[i];

		ins->waveform_list_number = p[0];
		ins->waveform_list_length = p[1];
		ins->waveform_list_speed  = p[2];
		ins->waveform_list_delay  = p[3];
		ins->arpeggio_number      = p[4];
		ins->arpeggio_length      = p[5];
		ins->arpeggio_speed       = p[6];
		ins->arpeggio_delay       = p[7];
		ins->vibrato_number       = p[8];
		ins->vibrato_length       = p[9];
		ins->vibrato_speed        = p[10];
		ins->vibrato_delay        = p[11];
		ins->pitch_bend_speed     = (int8_t)p[12];
		ins->pitch_bend_delay     = p[13];
		// p[14], p[15] reserved
		ins->attack_max     = p[16];
		ins->attack_speed   = p[17];
		ins->decay_min      = p[18];
		ins->decay_speed    = p[19];
		ins->sustain_time   = p[20];
		ins->release_min    = p[21];
		ins->release_speed  = p[22];
		// remaining bytes reserved
	}

	return 1;
}

// [=]===^=[ sidmon20_load_samples ]==============================================================[=]
static int32_t sidmon20_load_samples(struct sidmon20_state *s, uint32_t sample_offset,
		uint32_t num_samples, struct sidmon20_block_info *sample_info_info) {
	uint32_t info_size = 64;

	if(sample_info_info->length < num_samples * info_size) {
		return 0;
	}
	if(sample_info_info->offset + num_samples * info_size > s->module_len) {
		return 0;
	}

	s->samples = (struct sidmon20_sample *)calloc((size_t)num_samples, sizeof(struct sidmon20_sample));
	if(!s->samples) {
		return 0;
	}
	s->sample_negate_info = (struct sidmon20_negate_info *)calloc((size_t)num_samples, sizeof(struct sidmon20_negate_info));
	if(!s->sample_negate_info) {
		return 0;
	}
	s->num_samples = num_samples;

	uint32_t pos = sample_info_info->offset;
	uint32_t cur_sample_offset = sample_offset;

	for(uint32_t i = 0; i < num_samples; ++i) {
		struct sidmon20_sample *sample = &s->samples[i];
		struct sidmon20_negate_info *ni = &s->sample_negate_info[i];

		// Skip 4-byte sample data pointer
		uint8_t *p = s->module_data + pos + 4;

		sample->length      = (uint32_t)sidmon20_read_u16_be(p +  0) * 2;
		sample->loop_start  = (uint32_t)sidmon20_read_u16_be(p +  2) * 2;
		sample->loop_length = (uint32_t)sidmon20_read_u16_be(p +  4) * 2;

		if(sample->loop_start > sample->length) {
			sample->loop_start = 0;
			sample->loop_length = 0;
		} else if(sample->loop_start + sample->loop_length > sample->length) {
			sample->loop_length = sample->length - sample->loop_start;
		}

		ni->start_offset = (uint32_t)sidmon20_read_u16_be(p +  6) * 2;
		ni->end_offset   = (uint32_t)sidmon20_read_u16_be(p +  8) * 2;
		ni->loop_index   = sidmon20_read_u16_be(p + 10);
		ni->status       = sidmon20_read_u16_be(p + 12);
		ni->speed        = sidmon20_read_i16_be(p + 14);
		ni->position     = sidmon20_read_i32_be(p + 16);
		ni->index        = sidmon20_read_u16_be(p + 20);
		ni->do_negation  = sidmon20_read_i16_be(p + 22);

		// p+24..27 reserved, p+28..59 sample name
		pos += info_size;
	}

	// Now the sample data section. Allocate mutable copies (negation mutates samples).
	for(uint32_t i = 0; i < num_samples; ++i) {
		struct sidmon20_sample *sample = &s->samples[i];

		if(cur_sample_offset + sample->length > s->module_len) {
			return 0;
		}

		if(sample->length > 0) {
			sample->sample_data = (int8_t *)malloc(sample->length);
			if(!sample->sample_data) {
				return 0;
			}
			memcpy(sample->sample_data, s->module_data + cur_sample_offset, sample->length);
		} else {
			sample->sample_data = 0;
		}

		cur_sample_offset += sample->length;
	}

	return 1;
}

// [=]===^=[ sidmon20_cleanup ]===================================================================[=]
static void sidmon20_cleanup(struct sidmon20_state *s) {
	if(!s) {
		return;
	}

	free(s->waveform_info); s->waveform_info = 0;
	free(s->arpeggios);     s->arpeggios = 0;
	free(s->vibratoes);     s->vibratoes = 0;

	for(int32_t i = 0; i < 4; ++i) {
		free(s->sequences[i]);
		s->sequences[i] = 0;
	}

	if(s->tracks) {
		for(uint32_t i = 0; i < s->num_tracks; ++i) {
			free(s->tracks[i].track_data);
		}
		free(s->tracks);
		s->tracks = 0;
		s->num_tracks = 0;
	}

	free(s->instruments); s->instruments = 0;
	s->num_instruments = 0;

	if(s->samples) {
		for(uint32_t i = 0; i < s->num_samples; ++i) {
			free(s->samples[i].sample_data);
		}
		free(s->samples);
		s->samples = 0;
	}

	free(s->sample_negate_info); s->sample_negate_info = 0;
	s->num_samples = 0;
}

// [=]===^=[ sidmon20_paula_set_loop_data ]=======================================================[=]
// Replace the loop sample data without restarting the playback position.
// Used by the wave-list change effect (DoWaveForm) which swaps sample bodies in-flight.
static void sidmon20_paula_set_loop_data(struct paula *p, int32_t idx, int8_t *sample, uint32_t loop_start, uint32_t loop_length) {
	struct paula_channel *c = &p->ch[idx];
	c->sample = sample;
	c->loop_start_fp = loop_start << PAULA_FP_SHIFT;
	c->loop_length_fp = loop_length << PAULA_FP_SHIFT;
	c->length_fp = (loop_start + loop_length) << PAULA_FP_SHIFT;
	if(loop_length > 0) {
		c->active = 1;
	}
}

// [=]===^=[ sidmon20_find_note ]=================================================================[=]
static void sidmon20_find_note(struct sidmon20_state *s, struct sidmon20_voice *v) {
	if(s->current_position < 0 || (uint8_t)s->current_position >= s->number_of_positions) {
		return;
	}
	struct sidmon20_sequence *seq = &v->sequence_list[s->current_position];
	if(seq->track_number >= s->num_tracks) {
		return;
	}

	v->current_track = s->tracks[seq->track_number].track_data;
	v->current_track_length = s->tracks[seq->track_number].track_length;
	v->track_position = 0;
	v->note_transpose = seq->note_transpose;
	v->instrument_transpose = seq->instrument_transpose;
	v->empty_notes_counter = 0;
}

// [=]===^=[ sidmon20_get_note2 ]=================================================================[=]
static void sidmon20_get_note2(struct sidmon20_voice *v) {
	uint8_t *track_data = v->current_track;

	v->current_note = 0;
	v->current_instrument = 0;
	v->current_effect = 0;
	v->current_effect_arg = 0;

	if(v->empty_notes_counter == 0) {
		int8_t track_value;
		if((uint32_t)v->track_position < v->current_track_length) {
			track_value = (int8_t)track_data[v->track_position++];
		} else {
			track_value = -1;
		}

		if(track_value == 0) {
			if((uint32_t)(v->track_position + 1) <= v->current_track_length) {
				v->current_effect = track_data[v->track_position++];
			}
			if((uint32_t)(v->track_position + 1) <= v->current_track_length) {
				v->current_effect_arg = track_data[v->track_position++];
			}
			return;
		}

		if(track_value > 0) {
			if(track_value >= 0x70) {
				v->current_effect = (uint8_t)track_value;
				if((uint32_t)(v->track_position + 1) <= v->current_track_length) {
					v->current_effect_arg = track_data[v->track_position++];
				}
				return;
			}

			v->current_note = (uint16_t)track_value;

			int8_t next_value;
			if((uint32_t)v->track_position < v->current_track_length) {
				next_value = (int8_t)track_data[v->track_position++];
			} else {
				next_value = -1;
			}
			if(next_value >= 0) {
				if(next_value >= 0x70) {
					v->current_effect = (uint8_t)next_value;
					if((uint32_t)(v->track_position + 1) <= v->current_track_length) {
						v->current_effect_arg = track_data[v->track_position++];
					}
					return;
				}

				v->current_instrument = (uint16_t)next_value;

				int8_t third_value;
				if((uint32_t)v->track_position < v->current_track_length) {
					third_value = (int8_t)track_data[v->track_position++];
				} else {
					third_value = -1;
				}
				if(third_value >= 0) {
					v->current_effect = (uint8_t)third_value;
					if((uint32_t)(v->track_position + 1) <= v->current_track_length) {
						v->current_effect_arg = track_data[v->track_position++];
					}
					return;
				}
				track_value = third_value;
			} else {
				track_value = next_value;
			}
		}

		v->empty_notes_counter = (uint16_t)((uint8_t)~track_value);
	} else {
		v->empty_notes_counter--;
	}
}

// [=]===^=[ sidmon20_get_note ]==================================================================[=]
static void sidmon20_get_note(struct sidmon20_voice *v) {
	sidmon20_get_note2(v);

	if(v->current_note != 0) {
		v->current_note = (uint16_t)(v->current_note + v->note_transpose);
	}
}

// [=]===^=[ sidmon20_play_voice ]================================================================[=]
static void sidmon20_play_voice(struct sidmon20_state *s, struct sidmon20_voice *v, int32_t voice_idx) {
	v->pitch_bend_value = 0;

	if(v->current_note == 0) {
		return;
	}

	v->sample_volume = 0;
	v->wave_list_delay = 0;
	v->wave_list_offset = 0;
	v->arpeggio_delay = 0;
	v->arpeggio_offset = 0;
	v->vibrato_delay = 0;
	v->vibrato_offset = 0;
	v->pitch_bend_counter = 0;
	v->note_slide_speed = 0;

	v->envelope_state = SIDMON20_ENV_ATTACK;
	v->sustain_counter = 0;

	uint16_t instrument = v->current_instrument;
	if(instrument != 0) {
		int32_t idx = (int32_t)instrument - 1 + (int32_t)v->instrument_transpose;
		if(idx >= 0 && (uint32_t)idx < s->num_instruments) {
			v->instrument = &s->instruments[idx];

			uint8_t *waveform_list = (v->instrument->waveform_list_number < s->num_waveform_info) ?
				s->waveform_info[v->instrument->waveform_list_number] : 0;
			if(waveform_list) {
				v->current_sample = waveform_list[0];

				if(v->current_sample < s->num_samples) {
					struct sidmon20_sample *sample = &s->samples[v->current_sample];
					v->sample_data = sample->sample_data;
					v->sample_length = sample->length;
					v->loop_sample = sample->sample_data;
					v->loop_offset = sample->loop_start;
					v->loop_length = sample->loop_length;
				}
			}
		}
	}

	if(!v->instrument) {
		paula_mute(&s->paula, voice_idx);
		return;
	}

	int8_t *arpeggio = (v->instrument->arpeggio_number < s->num_arpeggios) ?
		s->arpeggios[v->instrument->arpeggio_number] : 0;
	if(arpeggio) {
		int32_t note = (int32_t)v->current_note + (int32_t)arpeggio[0];
		if(note >= 0 && note < SIDMON20_NUM_PERIODS) {
			v->original_note = (uint16_t)note;
			v->sample_period = sidmon20_periods[note];

			if(v->sample_length != 0 && v->sample_data != 0) {
				paula_play_sample(&s->paula, voice_idx, v->sample_data, v->sample_length);

				if(v->loop_length > 2) {
					paula_set_loop(&s->paula, voice_idx, v->loop_offset, v->loop_length);
				}
			} else {
				paula_mute(&s->paula, voice_idx);
			}
		} else {
			paula_mute(&s->paula, voice_idx);
		}
	} else {
		paula_mute(&s->paula, voice_idx);
	}

	paula_set_period(&s->paula, voice_idx, v->sample_period);
}

// [=]===^=[ sidmon20_do_adsr_curve ]=============================================================[=]
static void sidmon20_do_adsr_curve(struct sidmon20_state *s, struct sidmon20_voice *v, int32_t voice_idx) {
	struct sidmon20_instrument *instrument = v->instrument;
	if(!instrument) {
		return;
	}

	switch(v->envelope_state) {
		case SIDMON20_ENV_ATTACK: {
			v->sample_volume = (int16_t)(v->sample_volume + instrument->attack_speed);
			if(v->sample_volume >= instrument->attack_max) {
				v->sample_volume = instrument->attack_max;
				v->envelope_state = SIDMON20_ENV_DECAY;
			}
			break;
		}

		case SIDMON20_ENV_DECAY: {
			if(instrument->decay_speed == 0) {
				v->envelope_state = SIDMON20_ENV_SUSTAIN;
			} else {
				v->sample_volume = (int16_t)(v->sample_volume - instrument->decay_speed);
				if(v->sample_volume <= instrument->decay_min) {
					v->sample_volume = instrument->decay_min;
					v->envelope_state = SIDMON20_ENV_SUSTAIN;
				}
			}
			break;
		}

		case SIDMON20_ENV_SUSTAIN: {
			if(v->sustain_counter == instrument->sustain_time) {
				v->envelope_state = SIDMON20_ENV_RELEASE;
			} else {
				v->sustain_counter++;
			}
			break;
		}

		case SIDMON20_ENV_RELEASE: {
			if(instrument->release_speed == 0) {
				v->envelope_state = SIDMON20_ENV_DONE;
			} else {
				v->sample_volume = (int16_t)(v->sample_volume - instrument->release_speed);
				if(v->sample_volume <= instrument->release_min) {
					v->sample_volume = instrument->release_min;
					v->envelope_state = SIDMON20_ENV_DONE;
				}
			}
			break;
		}

		default: break;
	}

	int16_t vol = v->sample_volume;
	if(vol < 0) {
		vol = 0;
	}
	paula_set_volume_256(&s->paula, voice_idx, (uint16_t)vol);
}

// [=]===^=[ sidmon20_do_waveform ]===============================================================[=]
static void sidmon20_do_waveform(struct sidmon20_state *s, struct sidmon20_voice *v, int32_t voice_idx) {
	struct sidmon20_instrument *instrument = v->instrument;
	if(!instrument || instrument->waveform_list_length == 0) {
		return;
	}

	if(v->wave_list_delay != instrument->waveform_list_delay) {
		v->wave_list_delay++;
		return;
	}

	v->wave_list_delay = (uint16_t)(v->wave_list_delay - instrument->waveform_list_speed);

	if(v->wave_list_offset == instrument->waveform_list_length) {
		v->wave_list_offset = -1;
	}

	v->wave_list_offset++;

	if(instrument->waveform_list_number >= s->num_waveform_info) {
		v->wave_list_offset--;
		return;
	}
	if(v->wave_list_offset < 0 || v->wave_list_offset >= SIDMON20_LIST_ENTRY_SIZE) {
		v->wave_list_offset--;
		return;
	}

	int8_t waveform_value = (int8_t)s->waveform_info[instrument->waveform_list_number][v->wave_list_offset];
	if(waveform_value >= 0) {
		v->current_sample = (uint16_t)waveform_value;

		if(v->current_sample < s->num_samples) {
			struct sidmon20_sample *sample = &s->samples[v->current_sample];
			v->loop_sample = sample->sample_data;
			v->loop_offset = sample->loop_start;
			v->loop_length = sample->loop_length;

			if(s->paula.ch[voice_idx].active) {
				sidmon20_paula_set_loop_data(&s->paula, voice_idx, v->loop_sample, v->loop_offset, v->loop_length);
			}
		}
	} else {
		v->wave_list_offset--;
	}
}

// [=]===^=[ sidmon20_do_arpeggio ]===============================================================[=]
static void sidmon20_do_arpeggio(struct sidmon20_state *s, struct sidmon20_voice *v) {
	struct sidmon20_instrument *instrument = v->instrument;
	if(!instrument || instrument->arpeggio_length == 0) {
		return;
	}

	if(v->arpeggio_delay != instrument->arpeggio_delay) {
		v->arpeggio_delay++;
		return;
	}

	v->arpeggio_delay = (uint16_t)(v->arpeggio_delay - instrument->arpeggio_speed);

	if(v->arpeggio_offset == instrument->arpeggio_length) {
		v->arpeggio_offset = -1;
	}

	v->arpeggio_offset++;

	if(instrument->arpeggio_number >= s->num_arpeggios) {
		return;
	}
	if(v->arpeggio_offset < 0 || v->arpeggio_offset >= SIDMON20_LIST_ENTRY_SIZE) {
		return;
	}

	int8_t arpeggio_value = s->arpeggios[instrument->arpeggio_number][v->arpeggio_offset];
	int32_t new_note = (int32_t)v->original_note + (int32_t)arpeggio_value;
	if(new_note >= 0 && new_note < SIDMON20_NUM_PERIODS) {
		v->sample_period = sidmon20_periods[new_note];
	}
}

// [=]===^=[ sidmon20_do_vibrato ]================================================================[=]
static void sidmon20_do_vibrato(struct sidmon20_state *s, struct sidmon20_voice *v) {
	struct sidmon20_instrument *instrument = v->instrument;
	if(!instrument || instrument->vibrato_length == 0) {
		return;
	}

	if(v->vibrato_delay != instrument->vibrato_delay) {
		v->vibrato_delay++;
		return;
	}

	v->vibrato_delay = (uint16_t)(v->vibrato_delay - instrument->vibrato_speed);

	if(v->vibrato_offset == instrument->vibrato_length) {
		v->vibrato_offset = -1;
	}

	v->vibrato_offset++;

	if(instrument->vibrato_number >= s->num_vibratoes) {
		return;
	}
	if(v->vibrato_offset < 0 || v->vibrato_offset >= SIDMON20_LIST_ENTRY_SIZE) {
		return;
	}

	int8_t vibrato_value = s->vibratoes[instrument->vibrato_number][v->vibrato_offset];
	v->sample_period = (uint16_t)(v->sample_period + vibrato_value);
}

// [=]===^=[ sidmon20_do_pitch_bend ]=============================================================[=]
static void sidmon20_do_pitch_bend(struct sidmon20_voice *v) {
	struct sidmon20_instrument *instrument = v->instrument;
	if(!instrument || instrument->pitch_bend_speed == 0) {
		return;
	}

	if(v->pitch_bend_counter == instrument->pitch_bend_delay) {
		v->pitch_bend_value = (int16_t)(v->pitch_bend_value + instrument->pitch_bend_speed);
	} else {
		v->pitch_bend_counter++;
	}
}

// [=]===^=[ sidmon20_do_note_slide ]=============================================================[=]
static void sidmon20_do_note_slide(struct sidmon20_voice *v) {
	if((v->current_effect != 0) && (v->current_effect < 0x70) && (v->current_effect_arg != 0)) {
		if(v->current_effect < SIDMON20_NUM_PERIODS) {
			v->note_slide_note = sidmon20_periods[v->current_effect];

			int32_t direction = (int32_t)v->note_slide_note - (int32_t)v->sample_period;
			if(direction == 0) {
				return;
			}

			int16_t effect_arg = (int16_t)v->current_effect_arg;
			if(direction < 0) {
				effect_arg = (int16_t)-effect_arg;
			}

			v->note_slide_speed = effect_arg;
		}
	}

	int16_t speed = v->note_slide_speed;
	if(speed != 0) {
		if(speed < 0) {
			v->sample_period = (uint16_t)(v->sample_period + speed);
			if(v->sample_period <= v->note_slide_note) {
				v->sample_period = v->note_slide_note;
				v->note_slide_speed = 0;
			}
		} else {
			v->sample_period = (uint16_t)(v->sample_period + speed);
			if(v->sample_period >= v->note_slide_note) {
				v->sample_period = v->note_slide_note;
				v->note_slide_speed = 0;
			}
		}
	}
}

// [=]===^=[ sidmon20_do_sound_tracker ]==========================================================[=]
static void sidmon20_do_sound_tracker(struct sidmon20_state *s, struct sidmon20_voice *v, int32_t voice_idx) {
	uint16_t effect = v->current_effect;
	if(effect < 0x70) {
		return;
	}

	effect &= 0x0f;
	if((s->current_rast == 0) && (effect < 5)) {
		return;
	}

	switch(effect) {
		case 0x0: {
			uint8_t arp_tab[4];
			arp_tab[0] = (uint8_t)(v->current_effect_arg >> 4);
			arp_tab[1] = 0;
			arp_tab[2] = (uint8_t)(v->current_effect_arg & 0x0f);
			arp_tab[3] = 0;

			uint8_t arp_value = arp_tab[s->current_rast2];
			int32_t idx = (int32_t)v->original_note + (int32_t)arp_value;
			if(idx >= 0 && idx < SIDMON20_NUM_PERIODS) {
				v->sample_period = sidmon20_periods[idx];
			}
			break;
		}

		case 0x1: {
			v->pitch_bend_value = (int16_t)(-(int16_t)v->current_effect_arg);
			break;
		}

		case 0x2: {
			v->pitch_bend_value = (int16_t)v->current_effect_arg;
			break;
		}

		case 0x3: {
			if(v->envelope_state == SIDMON20_ENV_DONE) {
				if((s->current_rast == 0) && (v->current_instrument != 0) && v->instrument) {
					v->sample_volume = v->instrument->attack_speed;
				}

				int32_t volume = (int32_t)v->sample_volume + (int32_t)v->current_effect_arg * 4;
				if(volume >= 256) {
					volume = 255;
				}
				v->sample_volume = (int16_t)volume;
			}
			break;
		}

		case 0x4: {
			if(v->envelope_state == SIDMON20_ENV_DONE) {
				if((s->current_rast == 0) && (v->current_instrument != 0) && v->instrument) {
					v->sample_volume = v->instrument->attack_speed;
				}

				int32_t volume = (int32_t)v->sample_volume - (int32_t)v->current_effect_arg * 4;
				if(volume < 0) {
					volume = 0;
				}
				v->sample_volume = (int16_t)volume;
			}
			break;
		}

		case 0x5: {
			if(v->instrument) {
				v->instrument->attack_max = (uint8_t)v->current_effect_arg;
				v->instrument->attack_speed = (uint8_t)v->current_effect_arg;
			}
			break;
		}

		case 0x6: {
			if(v->current_effect_arg != 0) {
				s->pattern_length = (uint8_t)v->current_effect_arg;
			}
			break;
		}

		case 0xc: {
			uint16_t volume = v->current_effect_arg;
			uint16_t amiga_vol = (volume > 64) ? 64 : volume;
			paula_set_volume(&s->paula, voice_idx, amiga_vol);

			volume = (uint16_t)(volume * 4);
			if(volume >= 255) {
				volume = 255;
			}
			v->sample_volume = (int16_t)volume;
			break;
		}

		case 0xf: {
			uint8_t new_speed = (uint8_t)(v->current_effect_arg & 0x0f);
			if((new_speed != 0) && (new_speed != s->speed)) {
				s->speed = new_speed;
			}
			break;
		}

		default: break;
	}
}

// [=]===^=[ sidmon20_do_effect ]=================================================================[=]
static void sidmon20_do_effect(struct sidmon20_state *s, struct sidmon20_voice *v, int32_t voice_idx) {
	sidmon20_do_adsr_curve(s, v, voice_idx);
	sidmon20_do_waveform(s, v, voice_idx);
	sidmon20_do_arpeggio(s, v);
	sidmon20_do_sound_tracker(s, v, voice_idx);
	sidmon20_do_vibrato(s, v);
	sidmon20_do_pitch_bend(v);
	sidmon20_do_note_slide(v);

	v->sample_period = (uint16_t)(v->sample_period + v->pitch_bend_value);

	if(v->sample_period < 95) {
		v->sample_period = 95;
	} else if(v->sample_period > 5760) {
		v->sample_period = 5760;
	}

	paula_set_period(&s->paula, voice_idx, v->sample_period);
}

// [=]===^=[ sidmon20_do_negation ]===============================================================[=]
static void sidmon20_do_negation(struct sidmon20_state *s) {
	struct sidmon20_negate_info *working[4];

	for(int32_t i = 0; i < 4; ++i) {
		struct sidmon20_voice *voice = &s->voices[i];
		uint16_t sample_number = voice->current_sample;
		if(sample_number >= s->num_samples) {
			working[i] = 0;
			continue;
		}

		struct sidmon20_negate_info *ni = &s->sample_negate_info[sample_number];
		working[i] = ni;

		if(ni->do_negation != 0) {
			continue;
		}

		ni->do_negation = -1;

		if(ni->index != 0) {
			ni->index++;
			ni->index &= 0x1f;
			continue;
		}

		ni->index = ni->loop_index;

		if(ni->status == 0) {
			continue;
		}

		struct sidmon20_sample *sample = &s->samples[sample_number];
		if(!sample->sample_data) {
			continue;
		}

		uint32_t end_offset = ni->end_offset - 1;
		int32_t position = (int32_t)ni->start_offset + ni->position;
		if(position >= 0 && (uint32_t)position < sample->length) {
			sample->sample_data[position] = (int8_t)~sample->sample_data[position];
		}

		ni->position += ni->speed;
		if(ni->position < 0) {
			if(ni->status == 2) {
				ni->position = (int32_t)end_offset;
			} else {
				ni->position += -ni->speed;
				ni->speed = (int16_t)-ni->speed;
			}
		} else if(ni->position > (int32_t)end_offset) {
			if(ni->status == 1) {
				ni->position = 0;
			} else {
				ni->position += -ni->speed;
				ni->speed = (int16_t)-ni->speed;
			}
		}
	}

	for(int32_t i = 0; i < 4; ++i) {
		if(working[i]) {
			working[i]->do_negation = 0;
		}
	}
}

// [=]===^=[ sidmon20_initialize_sound ]==========================================================[=]
static void sidmon20_initialize_sound(struct sidmon20_state *s) {
	s->current_position = 0;
	s->current_row = 0;
	s->pattern_length = 64;
	s->current_rast = 0;
	s->current_rast2 = 0;
	s->speed = s->start_speed;
	s->end_reached = 0;

	for(int32_t i = 0; i < 4; ++i) {
		struct sidmon20_voice *v = &s->voices[i];
		memset(v, 0, sizeof(*v));

		v->sequence_list = s->sequences[i];
		v->envelope_state = SIDMON20_ENV_ATTACK;

		if(s->num_instruments > 0) {
			v->instrument = &s->instruments[0];

			uint8_t wf_num = v->instrument->waveform_list_number;
			if(wf_num < s->num_waveform_info) {
				uint8_t sample_number = s->waveform_info[wf_num][0];
				if(sample_number < s->num_samples) {
					struct sidmon20_sample *sample = &s->samples[sample_number];
					v->current_sample = sample_number;
					v->sample_data = sample->sample_data;
					v->sample_length = sample->length;
					v->loop_sample = sample->sample_data;
					v->loop_offset = sample->loop_start;
					v->loop_length = sample->loop_length;
				}
			}
		}

		sidmon20_find_note(s, v);
	}
}

// [=]===^=[ sidmon20_play_it ]===================================================================[=]
static void sidmon20_play_it(struct sidmon20_state *s) {
	s->current_rast2++;
	if(s->current_rast2 == 3) {
		s->current_rast2 = 0;
	}

	s->current_rast++;
	if(s->current_rast >= s->speed) {
		s->current_rast = 0;
		s->current_rast2 = 0;

		for(int32_t i = 0; i < 4; ++i) {
			sidmon20_get_note(&s->voices[i]);
		}

		for(int32_t i = 0; i < 4; ++i) {
			sidmon20_play_voice(s, &s->voices[i], i);
		}

		sidmon20_do_negation(s);

		s->current_row++;
		if(s->current_row == s->pattern_length) {
			s->current_row = 0;

			if((uint8_t)s->current_position == s->number_of_positions - 1) {
				s->current_position = -1;
				s->end_reached = 1;
			}

			s->current_position++;

			for(int32_t i = 0; i < 4; ++i) {
				sidmon20_find_note(s, &s->voices[i]);
			}
		}
	}

	for(int32_t i = 0; i < 4; ++i) {
		sidmon20_do_effect(s, &s->voices[i], i);
	}

	if(s->current_rast != 0) {
		sidmon20_do_negation(s);
	}
}

// [=]===^=[ sidmon20_init ]======================================================================[=]
static struct sidmon20_state *sidmon20_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < SIDMON20_HEADER_SIZE || sample_rate < 8000) {
		return 0;
	}

	if(!sidmon20_check_magic((uint8_t *)data, len)) {
		return 0;
	}

	struct sidmon20_state *s = (struct sidmon20_state *)calloc(1, sizeof(struct sidmon20_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;

	uint32_t num_samples = 0;
	struct sidmon20_block_info position_info;
	struct sidmon20_block_info note_transpose_info;
	struct sidmon20_block_info instrument_transpose_info;
	struct sidmon20_block_info instruments_info;
	struct sidmon20_block_info waveform_list_info;
	struct sidmon20_block_info arpeggios_info;
	struct sidmon20_block_info vibratoes_info;
	struct sidmon20_block_info sample_info_info;
	struct sidmon20_block_info track_table_info;
	struct sidmon20_block_info tracks_info;
	uint32_t sample_offset = 0;

	if(!sidmon20_load_song_data(s, &num_samples,
		&position_info, &note_transpose_info, &instrument_transpose_info,
		&instruments_info, &waveform_list_info, &arpeggios_info,
		&vibratoes_info, &sample_info_info, &track_table_info, &tracks_info)) {
		goto fail;
	}

	if(!sidmon20_load_lists(s, &waveform_list_info, &arpeggios_info, &vibratoes_info)) {
		goto fail;
	}

	if(!sidmon20_load_sequences(s, &position_info, &note_transpose_info, &instrument_transpose_info)) {
		goto fail;
	}

	if(!sidmon20_load_tracks(s, &track_table_info, &tracks_info, &sample_offset)) {
		goto fail;
	}

	if(!sidmon20_load_instruments(s, &instruments_info)) {
		goto fail;
	}

	if(!sidmon20_load_samples(s, sample_offset, num_samples, &sample_info_info)) {
		goto fail;
	}

	// The C# loader decrements numberOfPositions back to its on-disk value after loading,
	// turning "real count" into "max index". We keep number_of_positions as the real count
	// and adjust comparisons accordingly.
	if(s->number_of_positions == 0) {
		goto fail;
	}

	paula_init(&s->paula, sample_rate, SIDMON20_TICK_HZ);
	sidmon20_initialize_sound(s);
	return s;

fail:
	sidmon20_cleanup(s);
	free(s);
	return 0;
}

// [=]===^=[ sidmon20_free ]======================================================================[=]
static void sidmon20_free(struct sidmon20_state *s) {
	if(!s) {
		return;
	}
	sidmon20_cleanup(s);
	free(s);
}

// [=]===^=[ sidmon20_get_audio ]=================================================================[=]
static void sidmon20_get_audio(struct sidmon20_state *s, int16_t *output, int32_t frames) {
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
			sidmon20_play_it(s);
		}
	}
}

// [=]===^=[ sidmon20_api_init ]==================================================================[=]
static void *sidmon20_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return sidmon20_init(data, len, sample_rate);
}

// [=]===^=[ sidmon20_api_free ]==================================================================[=]
static void sidmon20_api_free(void *state) {
	sidmon20_free((struct sidmon20_state *)state);
}

// [=]===^=[ sidmon20_api_get_audio ]=============================================================[=]
static void sidmon20_api_get_audio(void *state, int16_t *output, int32_t frames) {
	sidmon20_get_audio((struct sidmon20_state *)state, output, frames);
}

static const char *sidmon20_extensions[] = { "sd2", "sid2", "sid", 0 };

static struct player_api sidmon20_api = {
	"SidMon 2.0",
	sidmon20_extensions,
	sidmon20_api_init,
	sidmon20_api_free,
	sidmon20_api_get_audio,
	0,
};
