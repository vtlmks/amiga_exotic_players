// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// SoundMon (BP SoundMon) replayer, ported from NostalgicPlayer's C# implementation.
// Supports SoundMon V.2 (1.1) and V.3 (2.2). Drives a 4-channel Amiga Paula
// (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct soundmon_state *soundmon_init(void *data, uint32_t len, int32_t sample_rate);
//   void soundmon_free(struct soundmon_state *s);
//   void soundmon_get_audio(struct soundmon_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define SOUNDMON_TICK_HZ        50
#define SOUNDMON_NUM_INSTR      15
#define SOUNDMON_TRACK_ROWS     16
#define SOUNDMON_WAVE_BYTES     64

enum {
	SOUNDMON_TYPE_UNKNOWN = 0,
	SOUNDMON_TYPE_11      = 1,
	SOUNDMON_TYPE_22      = 2,
};

enum {
	SOUNDMON_OPT_ARPEGGIO_ONCE   = 0x0,
	SOUNDMON_OPT_SET_VOLUME      = 0x1,
	SOUNDMON_OPT_SET_SPEED       = 0x2,
	SOUNDMON_OPT_FILTER          = 0x3,
	SOUNDMON_OPT_PORT_UP         = 0x4,
	SOUNDMON_OPT_PORT_DOWN       = 0x5,
	SOUNDMON_OPT_VIBRATO         = 0x6,	// SoundMon 2.2 (also SetRepCount on 1.1)
	SOUNDMON_OPT_JUMP            = 0x7,	// SoundMon 2.2 (also DbraRepCount on 1.1)
	SOUNDMON_OPT_SET_AUTO_SLIDE  = 0x8,
	SOUNDMON_OPT_SET_ARPEGGIO    = 0x9,
	SOUNDMON_OPT_TRANSPOSE       = 0xa,
	SOUNDMON_OPT_CHANGE_FX       = 0xb,
	SOUNDMON_OPT_CHANGE_INVERSION = 0xd,
	SOUNDMON_OPT_RESET_ADSR      = 0xe,
	SOUNDMON_OPT_CHANGE_NOTE     = 0xf,
};

struct soundmon_synth_instrument {
	uint8_t  wave_table;
	uint16_t wave_length;
	uint8_t  adsr_control;
	uint8_t  adsr_table;
	uint16_t adsr_length;
	uint8_t  adsr_speed;
	uint8_t  lfo_control;
	uint8_t  lfo_table;
	uint8_t  lfo_depth;
	uint16_t lfo_length;
	uint8_t  lfo_delay;
	uint8_t  lfo_speed;
	uint8_t  eg_control;
	uint8_t  eg_table;
	uint16_t eg_length;
	uint8_t  eg_delay;
	uint8_t  eg_speed;
	uint8_t  fx_control;
	uint8_t  fx_speed;
	uint8_t  fx_delay;
	uint8_t  mod_control;
	uint8_t  mod_table;
	uint8_t  mod_speed;
	uint8_t  mod_delay;
	uint16_t mod_length;
};

struct soundmon_sample_instrument {
	uint16_t length;
	uint16_t loop_start;
	uint16_t loop_length;
	int8_t  *adr;
};

struct soundmon_instrument {
	uint8_t  is_synth;	// 1 = synth, 0 = sample
	uint16_t volume;
	struct soundmon_synth_instrument synth;
	struct soundmon_sample_instrument sample;
};

struct soundmon_step {
	uint16_t track_number;
	int8_t   sound_transpose;
	int8_t   transpose;
};

struct soundmon_track_row {
	int8_t  note;
	uint8_t instrument;
	uint8_t optional;
	uint8_t optional_data;
};

struct soundmon_voice {
	uint8_t  restart;
	uint8_t  use_default_volume;
	uint8_t  synth_mode;
	int32_t  synth_offset;
	uint16_t period;
	uint8_t  volume;
	uint8_t  instrument;
	uint8_t  note;
	uint8_t  arp_value;
	int8_t   auto_slide;
	uint8_t  auto_arp;
	uint16_t eg_ptr;
	uint16_t lfo_ptr;
	uint16_t adsr_ptr;
	uint16_t mod_ptr;
	uint8_t  eg_count;
	uint8_t  lfo_count;
	uint8_t  adsr_count;
	uint8_t  mod_count;
	uint8_t  fx_count;
	uint8_t  old_eg_value;
	uint8_t  eg_control;
	uint8_t  lfo_control;
	uint8_t  adsr_control;
	uint8_t  mod_control;
	uint8_t  fx_control;
	int8_t   vibrato;
};

struct soundmon_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint32_t module_type;

	uint8_t  wave_num;
	uint16_t step_num;
	uint16_t track_num;

	struct soundmon_instrument instruments[SOUNDMON_NUM_INSTR];

	// steps[channel][step_index]
	struct soundmon_step *steps[4];

	// tracks[track_index][row]: track_num blocks of 16 rows
	struct soundmon_track_row *tracks;

	// Original wave tables loaded from module: wave_num * 64 signed bytes.
	int8_t *wave_tables_original;

	// Working copy that is mutated by EG/MOD/FX. wave_num * 64 signed bytes.
	int8_t *wave_tables;

	// Per-voice synth backup buffer: 4 * 32 signed bytes.
	int8_t synth_buffer[4][32];

	struct soundmon_voice voices[4];

	uint16_t bp_step;
	uint8_t  vib_index;
	uint8_t  arp_count;
	uint8_t  bp_count;
	uint8_t  bp_delay;
	int8_t   st;
	int8_t   tr;
	uint8_t  bp_pat_count;
	uint8_t  bp_rep_count;
	uint8_t  new_pos;
	uint8_t  pos_flag;
	uint8_t  first_repeat;
	uint8_t  amiga_filter;
};

// [=]===^=[ soundmon_periods ]===================================================================[=]
static uint16_t soundmon_periods[] = {
	6848, 6464, 6080, 5760, 5440, 5120, 4832, 4576, 4320, 4064, 3840, 3616,
	3424, 3232, 3040, 2880, 2720, 2560, 2416, 2288, 2160, 2032, 1920, 1808,
	1712, 1616, 1520, 1440, 1360, 1280, 1208, 1144, 1080, 1016,  960,  904,
	 856,  808,  760,  720,  680,  640,  604,  572,  540,  508,  480,  452,
	 428,  404,  380,  360,  340,  320,  302,  286,  270,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
	 107,  101,   95,   90,   85,   80,   76,   72,   68,   64,   60,   57
};

#define SOUNDMON_NUM_PERIODS  (sizeof(soundmon_periods) / sizeof(soundmon_periods[0]))

// [=]===^=[ soundmon_vibrato_table ]=============================================================[=]
static int16_t soundmon_vibrato_table[8] = {
	0, 64, 128, 64, 0, -64, -128, -64
};

// [=]===^=[ soundmon_read_u16_be ]===============================================================[=]
static uint16_t soundmon_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ soundmon_period_lookup ]=============================================================[=]
static uint16_t soundmon_period_lookup(int32_t note) {
	int32_t idx = note + 36 - 1;
	if(idx < 0) {
		idx = 0;
	}
	if(idx >= (int32_t)SOUNDMON_NUM_PERIODS) {
		idx = (int32_t)SOUNDMON_NUM_PERIODS - 1;
	}
	return soundmon_periods[idx];
}

// [=]===^=[ soundmon_test_module ]===============================================================[=]
static int32_t soundmon_test_module(uint8_t *buf, uint32_t len, uint32_t *out_type) {
	if(len < 512) {
		return 0;
	}
	if((buf[26] == 'V') && (buf[27] == '.') && (buf[28] == '2')) {
		*out_type = SOUNDMON_TYPE_11;
		return 1;
	}
	if((buf[26] == 'V') && (buf[27] == '.') && (buf[28] == '3')) {
		*out_type = SOUNDMON_TYPE_22;
		return 1;
	}
	return 0;
}

// [=]===^=[ soundmon_cleanup ]===================================================================[=]
static void soundmon_cleanup(struct soundmon_state *s) {
	if(!s) {
		return;
	}
	for(int32_t i = 0; i < 4; ++i) {
		free(s->steps[i]);
		s->steps[i] = 0;
	}
	free(s->tracks); s->tracks = 0;
	free(s->wave_tables_original); s->wave_tables_original = 0;
	free(s->wave_tables); s->wave_tables = 0;
	for(int32_t i = 0; i < SOUNDMON_NUM_INSTR; ++i) {
		free(s->instruments[i].sample.adr);
		s->instruments[i].sample.adr = 0;
	}
}

// [=]===^=[ soundmon_load_module ]===============================================================[=]
static int32_t soundmon_load_module(struct soundmon_state *s) {
	uint8_t *buf = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = 0;

	// 26-byte name + 3-byte mark
	pos = 29;

	if(pos + 1 + 2 > len) {
		return 0;
	}
	s->wave_num = buf[pos++];
	s->step_num = soundmon_read_u16_be(buf + pos);
	pos += 2;

	// Read 15 instrument records (32 bytes each)
	for(int32_t i = 0; i < SOUNDMON_NUM_INSTR; ++i) {
		if(pos + 32 > len) {
			return 0;
		}
		uint8_t first = buf[pos];
		struct soundmon_instrument *inst = &s->instruments[i];

		if(first == 0xff) {
			// Synth instrument
			inst->is_synth = 1;
			pos += 1;
			struct soundmon_synth_instrument *si = &inst->synth;

			if(s->module_type == SOUNDMON_TYPE_11) {
				si->wave_table   = buf[pos++];
				si->wave_length  = (uint16_t)(soundmon_read_u16_be(buf + pos) * 2);
				pos += 2;
				si->adsr_control = buf[pos++];
				si->adsr_table   = buf[pos++];
				si->adsr_length  = soundmon_read_u16_be(buf + pos);
				pos += 2;
				si->adsr_speed   = buf[pos++];
				si->lfo_control  = buf[pos++];
				si->lfo_table    = buf[pos++];
				si->lfo_depth    = buf[pos++];
				si->lfo_length   = soundmon_read_u16_be(buf + pos);
				pos += 2;
				pos += 1;	// skip
				si->lfo_delay    = buf[pos++];
				si->lfo_speed    = buf[pos++];
				si->eg_control   = buf[pos++];
				si->eg_table     = buf[pos++];
				pos += 1;	// skip
				si->eg_length    = soundmon_read_u16_be(buf + pos);
				pos += 2;
				pos += 1;	// skip
				si->eg_delay     = buf[pos++];
				si->eg_speed     = buf[pos++];
				si->fx_control   = 0;
				si->fx_speed     = 1;
				si->fx_delay     = 0;
				si->mod_control  = 0;
				si->mod_table    = 0;
				si->mod_speed    = 1;
				si->mod_delay    = 0;
				inst->volume     = buf[pos++];
				si->mod_length   = 0;
				pos += 6;	// skip
			} else {
				si->wave_table   = buf[pos++];
				si->wave_length  = (uint16_t)(soundmon_read_u16_be(buf + pos) * 2);
				pos += 2;
				si->adsr_control = buf[pos++];
				si->adsr_table   = buf[pos++];
				si->adsr_length  = soundmon_read_u16_be(buf + pos);
				pos += 2;
				si->adsr_speed   = buf[pos++];
				si->lfo_control  = buf[pos++];
				si->lfo_table    = buf[pos++];
				si->lfo_depth    = buf[pos++];
				si->lfo_length   = soundmon_read_u16_be(buf + pos);
				pos += 2;
				si->lfo_delay    = buf[pos++];
				si->lfo_speed    = buf[pos++];
				si->eg_control   = buf[pos++];
				si->eg_table     = buf[pos++];
				si->eg_length    = soundmon_read_u16_be(buf + pos);
				pos += 2;
				si->eg_delay     = buf[pos++];
				si->eg_speed     = buf[pos++];
				si->fx_control   = buf[pos++];
				si->fx_speed     = buf[pos++];
				si->fx_delay     = buf[pos++];
				si->mod_control  = buf[pos++];
				si->mod_table    = buf[pos++];
				si->mod_speed    = buf[pos++];
				si->mod_delay    = buf[pos++];
				inst->volume     = buf[pos++];
				si->mod_length   = soundmon_read_u16_be(buf + pos);
				pos += 2;
			}

			if(inst->volume > 64) {
				inst->volume = 64;
			}
		} else {
			// Sample instrument: re-read the 32-byte block from the start
			inst->is_synth = 0;
			struct soundmon_sample_instrument *sa = &inst->sample;
			// 24-byte name (skip), 2 length, 2 loop_start, 2 loop_length, 2 volume
			pos += 24;
			sa->length      = (uint16_t)(soundmon_read_u16_be(buf + pos) * 2);
			pos += 2;
			sa->loop_start  = soundmon_read_u16_be(buf + pos);
			pos += 2;
			sa->loop_length = (uint16_t)(soundmon_read_u16_be(buf + pos) * 2);
			pos += 2;
			inst->volume    = soundmon_read_u16_be(buf + pos);
			pos += 2;
			sa->adr         = 0;

			if(inst->volume > 64) {
				inst->volume = 64;
			}
			if((sa->loop_start + sa->loop_length) > sa->length) {
				sa->loop_length = (uint16_t)(sa->length - sa->loop_start);
			}
		}
	}

	// Allocate steps
	for(int32_t i = 0; i < 4; ++i) {
		s->steps[i] = (struct soundmon_step *)calloc((size_t)s->step_num, sizeof(struct soundmon_step));
		if(!s->steps[i]) {
			return 0;
		}
	}

	// Read step data: for each step, 4 channels of (u16 track_number, i8 sound_transpose, i8 transpose)
	s->track_num = 0;
	for(uint32_t i = 0; i < s->step_num; ++i) {
		for(int32_t j = 0; j < 4; ++j) {
			if(pos + 4 > len) {
				return 0;
			}
			s->steps[j][i].track_number    = soundmon_read_u16_be(buf + pos);
			pos += 2;
			s->steps[j][i].sound_transpose = (int8_t)buf[pos++];
			s->steps[j][i].transpose       = (int8_t)buf[pos++];

			if(s->steps[j][i].track_number > s->track_num) {
				s->track_num = s->steps[j][i].track_number;
			}
		}
	}

	// Allocate tracks
	if(s->track_num == 0) {
		return 0;
	}
	uint32_t total_rows = (uint32_t)s->track_num * SOUNDMON_TRACK_ROWS;
	s->tracks = (struct soundmon_track_row *)calloc(total_rows, sizeof(struct soundmon_track_row));
	if(!s->tracks) {
		return 0;
	}

	for(uint32_t i = 0; i < s->track_num; ++i) {
		for(int32_t j = 0; j < SOUNDMON_TRACK_ROWS; ++j) {
			if(pos + 3 > len) {
				return 0;
			}
			struct soundmon_track_row *row = &s->tracks[i * SOUNDMON_TRACK_ROWS + j];
			row->note          = (int8_t)buf[pos++];
			uint8_t inst_byte  = buf[pos++];
			row->optional      = (uint8_t)(inst_byte & 0x0f);
			row->instrument    = (uint8_t)((inst_byte & 0xf0) >> 4);
			row->optional_data = buf[pos++];
		}
	}

	// Wave tables
	uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;
	if(pos + wt_bytes > len) {
		return 0;
	}
	s->wave_tables_original = (int8_t *)malloc(wt_bytes);
	if(!s->wave_tables_original) {
		return 0;
	}
	memcpy(s->wave_tables_original, buf + pos, wt_bytes);
	pos += wt_bytes;

	s->wave_tables = (int8_t *)malloc(wt_bytes);
	if(!s->wave_tables) {
		return 0;
	}
	memcpy(s->wave_tables, s->wave_tables_original, wt_bytes);

	// Sample data
	for(int32_t i = 0; i < SOUNDMON_NUM_INSTR; ++i) {
		struct soundmon_instrument *inst = &s->instruments[i];
		if(inst->is_synth) {
			continue;
		}
		if(inst->sample.length == 0) {
			continue;
		}
		if(pos + inst->sample.length > len) {
			return 0;
		}
		inst->sample.adr = (int8_t *)malloc(inst->sample.length);
		if(!inst->sample.adr) {
			return 0;
		}
		memcpy(inst->sample.adr, buf + pos, inst->sample.length);
		pos += inst->sample.length;
	}

	return 1;
}

// [=]===^=[ soundmon_initialize_sound ]==========================================================[=]
static void soundmon_initialize_sound(struct soundmon_state *s) {
	s->arp_count    = 1;
	s->bp_count     = 1;
	s->bp_delay     = 6;
	s->bp_rep_count = 0;
	s->vib_index    = 0;
	s->bp_step      = 0;
	s->bp_pat_count = 0;
	s->st           = 0;
	s->tr           = 0;
	s->new_pos      = 0;
	s->pos_flag     = 0;
	s->first_repeat = 0;
	s->amiga_filter = 0;

	for(int32_t i = 0; i < 4; ++i) {
		memset(&s->voices[i], 0, sizeof(struct soundmon_voice));
		s->voices[i].synth_offset = -1;
	}

	uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;
	memcpy(s->wave_tables, s->wave_tables_original, wt_bytes);

	for(int32_t i = 0; i < 4; ++i) {
		memset(s->synth_buffer[i], 0, 32);
	}
}

// [=]===^=[ soundmon_do_optionals ]==============================================================[=]
static void soundmon_do_optionals(struct soundmon_state *s, int32_t voice_idx, uint32_t optional, uint8_t optional_data) {
	struct soundmon_voice *cur = &s->voices[voice_idx];

	switch(optional) {
		case SOUNDMON_OPT_ARPEGGIO_ONCE: {
			cur->arp_value = optional_data;
			break;
		}

		case SOUNDMON_OPT_SET_VOLUME: {
			if(optional_data > 64) {
				optional_data = 64;
			}
			cur->volume = optional_data;
			cur->use_default_volume = 0;

			if(s->module_type == SOUNDMON_TYPE_11) {
				paula_set_volume(&s->paula, voice_idx, optional_data);
			} else {
				if(!cur->synth_mode) {
					paula_set_volume(&s->paula, voice_idx, optional_data);
				}
			}
			break;
		}

		case SOUNDMON_OPT_SET_SPEED: {
			s->bp_count = optional_data;
			s->bp_delay = optional_data;
			break;
		}

		case SOUNDMON_OPT_FILTER: {
			s->amiga_filter = (optional_data != 0) ? 1 : 0;
			break;
		}

		case SOUNDMON_OPT_PORT_UP: {
			cur->period = (uint16_t)(cur->period - optional_data);
			cur->arp_value = 0;
			break;
		}

		case SOUNDMON_OPT_PORT_DOWN: {
			cur->period = (uint16_t)(cur->period + optional_data);
			cur->arp_value = 0;
			break;
		}

		case SOUNDMON_OPT_VIBRATO: {
			if(s->module_type == SOUNDMON_TYPE_11) {
				if(s->bp_rep_count == 0) {
					s->bp_rep_count = optional_data;
					if(s->bp_rep_count != 0) {
						s->first_repeat = 1;
					}
				}
			} else {
				cur->vibrato = (int8_t)optional_data;
			}
			break;
		}

		case SOUNDMON_OPT_JUMP: {
			if(s->module_type == SOUNDMON_TYPE_11) {
				if(s->bp_rep_count != 0) {
					s->first_repeat = 0;
					s->bp_rep_count--;
					if(s->bp_rep_count != 0) {
						s->new_pos = optional_data;
						s->pos_flag = 1;
					}
				}
			} else {
				s->new_pos = optional_data;
				s->pos_flag = 1;
			}
			break;
		}

		case SOUNDMON_OPT_SET_AUTO_SLIDE: {
			cur->auto_slide = (int8_t)optional_data;
			break;
		}

		case SOUNDMON_OPT_SET_ARPEGGIO: {
			cur->auto_arp = optional_data;
			if(s->module_type == SOUNDMON_TYPE_22) {
				cur->adsr_ptr = 0;
				if(cur->adsr_control == 0) {
					cur->adsr_control = 1;
				}
			}
			break;
		}

		case SOUNDMON_OPT_CHANGE_FX: {
			cur->fx_control = optional_data;
			break;
		}

		case SOUNDMON_OPT_CHANGE_INVERSION: {
			cur->auto_arp = optional_data;
			cur->fx_control = (uint8_t)(cur->fx_control ^ 1);
			cur->adsr_ptr = 0;
			if(cur->adsr_control == 0) {
				cur->adsr_control = 1;
			}
			break;
		}

		case SOUNDMON_OPT_RESET_ADSR: {
			cur->auto_arp = optional_data;
			cur->adsr_ptr = 0;
			if(cur->adsr_control == 0) {
				cur->adsr_control = 1;
			}
			break;
		}

		case SOUNDMON_OPT_CHANGE_NOTE: {
			cur->auto_arp = optional_data;
			break;
		}

		default: break;
	}
}

// [=]===^=[ soundmon_play_it ]===================================================================[=]
static void soundmon_play_it(struct soundmon_state *s, int32_t voice_idx) {
	struct soundmon_voice *cur = &s->voices[voice_idx];

	cur->restart = 0;
	paula_set_period(&s->paula, voice_idx, cur->period);

	if(cur->instrument == 0) {
		return;
	}

	struct soundmon_instrument *inst = &s->instruments[cur->instrument - 1];

	if(inst->is_synth) {
		struct soundmon_synth_instrument *si = &inst->synth;

		cur->synth_mode = 1;
		cur->eg_ptr     = 0;
		cur->lfo_ptr    = 0;
		cur->adsr_ptr   = 0;
		cur->mod_ptr    = 0;

		cur->eg_count   = (uint8_t)(si->eg_delay + 1);
		cur->lfo_count  = (uint8_t)(si->lfo_delay + 1);
		cur->adsr_count = 1;
		cur->mod_count  = (uint8_t)(si->mod_delay + 1);
		cur->fx_count   = (uint8_t)(si->fx_delay + 1);

		cur->fx_control   = si->fx_control;
		cur->eg_control   = si->eg_control;
		cur->lfo_control  = si->lfo_control;
		cur->adsr_control = si->adsr_control;
		cur->mod_control  = si->mod_control;
		cur->old_eg_value = 0;

		uint32_t wave_offset = (uint32_t)si->wave_table * SOUNDMON_WAVE_BYTES;
		uint32_t wt_bytes    = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;
		if(wave_offset + si->wave_length <= wt_bytes) {
			paula_play_sample(&s->paula, voice_idx, s->wave_tables + wave_offset, si->wave_length);
			paula_set_loop(&s->paula, voice_idx, 0, si->wave_length);
		}

		if(cur->adsr_control != 0) {
			uint32_t adsr_idx = (uint32_t)si->adsr_table * SOUNDMON_WAVE_BYTES;
			int32_t tmp = 0;
			if(adsr_idx < wt_bytes) {
				tmp = ((int32_t)s->wave_tables[adsr_idx] + 128) / 4;
			}

			if(cur->use_default_volume) {
				cur->volume = (uint8_t)inst->volume;
				cur->use_default_volume = 0;
			}

			tmp = tmp * (int32_t)cur->volume / 16;
			if(tmp > 256) {
				tmp = 256;
			}
			paula_set_volume_256(&s->paula, voice_idx, (uint16_t)tmp);
		} else {
			int32_t tmp = (cur->use_default_volume ? (int32_t)inst->volume : (int32_t)cur->volume) * 4;
			if(tmp > 256) {
				tmp = 256;
			}
			paula_set_volume_256(&s->paula, voice_idx, (uint16_t)tmp);
		}

		if((cur->eg_control != 0) || (cur->mod_control != 0) || (cur->fx_control != 0)) {
			cur->synth_offset = (int32_t)wave_offset;
			if(wave_offset + 32 <= wt_bytes) {
				memcpy(s->synth_buffer[voice_idx], s->wave_tables + wave_offset, 32);
			}
		}
	} else {
		struct soundmon_sample_instrument *sa = &inst->sample;

		cur->synth_mode = 0;
		cur->lfo_control = 0;

		if(sa->adr == 0) {
			paula_mute(&s->paula, voice_idx);
		} else {
			paula_play_sample(&s->paula, voice_idx, sa->adr, sa->length);

			if(sa->loop_length > 2) {
				paula_set_loop(&s->paula, voice_idx, sa->loop_start, sa->loop_length);
			}

			int32_t tmp = (cur->use_default_volume ? (int32_t)inst->volume : (int32_t)cur->volume) * 4;
			if(tmp > 256) {
				tmp = 256;
			}
			paula_set_volume_256(&s->paula, voice_idx, (uint16_t)tmp);
		}
	}
}

// [=]===^=[ soundmon_bp_next ]===================================================================[=]
static void soundmon_bp_next(struct soundmon_state *s) {
	for(int32_t i = 0; i < 4; ++i) {
		struct soundmon_voice *cur = &s->voices[i];
		struct soundmon_step *step = &s->steps[i][s->bp_step];

		uint16_t track = step->track_number;
		s->st = step->sound_transpose;
		s->tr = step->transpose;

		if(track == 0 || track > s->track_num) {
			continue;
		}

		struct soundmon_track_row *cur_track = &s->tracks[(track - 1) * SOUNDMON_TRACK_ROWS + s->bp_pat_count];

		int8_t note = cur_track->note;
		if(note != 0) {
			cur->auto_slide = 0;
			cur->auto_arp = 0;
			cur->vibrato = 0;

			if((cur_track->optional != SOUNDMON_OPT_TRANSPOSE) || ((cur_track->optional_data & 0xf0) == 0)) {
				note = (int8_t)(note + s->tr);
			}

			cur->note = (uint8_t)note;
			cur->period = soundmon_period_lookup(note);
			cur->restart = 0;

			if(cur_track->optional < SOUNDMON_OPT_CHANGE_INVERSION) {
				cur->restart = 1;
				cur->use_default_volume = 1;
			}

			uint8_t ins = cur_track->instrument;
			if(ins == 0) {
				ins = cur->instrument;
			}

			if((ins != 0) && ((cur_track->optional != SOUNDMON_OPT_TRANSPOSE) || ((cur_track->optional_data & 0x0f) == 0))) {
				ins = (uint8_t)(ins + s->st);
				if((ins < 1) || (ins > 15)) {
					ins = (uint8_t)(ins - s->st);
				}
			}

			if((cur_track->optional < SOUNDMON_OPT_CHANGE_INVERSION) && (!cur->synth_mode || (cur->instrument != ins))) {
				cur->instrument = ins;
			}
		}

		soundmon_do_optionals(s, i, cur_track->optional, cur_track->optional_data);
	}

	if(s->pos_flag) {
		s->bp_pat_count = 0;
		s->bp_step = s->new_pos;
	} else {
		s->bp_pat_count++;
		if(s->bp_pat_count == SOUNDMON_TRACK_ROWS) {
			s->pos_flag = 1;
			s->bp_pat_count = 0;
			s->bp_step++;
			if(s->bp_step == s->step_num) {
				s->bp_step = 0;
			}
		}
	}

	if(s->pos_flag) {
		s->pos_flag = 0;
	}
}

// [=]===^=[ soundmon_do_effects ]================================================================[=]
static void soundmon_do_effects(struct soundmon_state *s) {
	s->arp_count = (uint8_t)((s->arp_count - 1) & 3);
	s->vib_index = (uint8_t)((s->vib_index + 1) & 7);

	for(int32_t i = 0; i < 4; ++i) {
		struct soundmon_voice *cur = &s->voices[i];

		cur->period = (uint16_t)(cur->period + cur->auto_slide);

		if(cur->vibrato != 0) {
			int32_t vib = soundmon_vibrato_table[s->vib_index] / cur->vibrato;
			paula_set_period(&s->paula, i, (uint16_t)(cur->period + vib));
		} else {
			paula_set_period(&s->paula, i, cur->period);
		}

		if((cur->arp_value != 0) || (cur->auto_arp != 0)) {
			int32_t note = (int32_t)(int8_t)cur->note;

			if(s->arp_count == 0) {
				note += ((cur->arp_value & 0xf0) >> 4) + ((cur->auto_arp & 0xf0) >> 4);
			} else if(s->arp_count == 1) {
				note += (cur->arp_value & 0x0f) + (cur->auto_arp & 0x0f);
			}

			cur->restart = 0;
			cur->period = soundmon_period_lookup(note);
			paula_set_period(&s->paula, i, cur->period);
		}
	}
}

// [=]===^=[ soundmon_do_adsr ]===================================================================[=]
static void soundmon_do_adsr(struct soundmon_state *s, int32_t voice_idx, struct soundmon_voice *cur, struct soundmon_synth_instrument *si) {
	if(cur->adsr_control == 0) {
		return;
	}
	cur->adsr_count--;
	if(cur->adsr_count != 0) {
		return;
	}
	cur->adsr_count = si->adsr_speed;

	uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;
	uint32_t idx = (uint32_t)si->adsr_table * SOUNDMON_WAVE_BYTES + cur->adsr_ptr;
	int32_t table_value = 0;
	if(idx < wt_bytes) {
		table_value = (((int32_t)s->wave_tables[idx] + 128) / 4) * (int32_t)cur->volume / 16;
	}
	if(table_value > 256) {
		table_value = 256;
	}
	paula_set_volume_256(&s->paula, voice_idx, (uint16_t)table_value);

	cur->adsr_ptr++;
	if(cur->adsr_ptr == si->adsr_length) {
		cur->adsr_ptr = 0;
		if(cur->adsr_control == 1) {
			cur->adsr_control = 0;
		}
	}
}

// [=]===^=[ soundmon_do_lfo ]====================================================================[=]
static void soundmon_do_lfo(struct soundmon_state *s, int32_t voice_idx, struct soundmon_voice *cur, struct soundmon_synth_instrument *si) {
	if(cur->lfo_control == 0) {
		return;
	}
	cur->lfo_count--;
	if(cur->lfo_count != 0) {
		return;
	}
	cur->lfo_count = si->lfo_speed;

	uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;
	uint32_t idx = (uint32_t)si->lfo_table * SOUNDMON_WAVE_BYTES + cur->lfo_ptr;
	int32_t table_value = 0;
	if(idx < wt_bytes) {
		table_value = (int32_t)s->wave_tables[idx];
	}
	if(si->lfo_depth != 0) {
		table_value /= (int32_t)si->lfo_depth;
	}

	paula_set_period(&s->paula, voice_idx, (uint16_t)((int32_t)cur->period + table_value));

	cur->lfo_ptr++;
	if(cur->lfo_ptr == si->lfo_length) {
		cur->lfo_ptr = 0;
		if(cur->lfo_control == 1) {
			cur->lfo_control = 0;
		}
	}
}

// [=]===^=[ soundmon_do_eg ]=====================================================================[=]
static void soundmon_do_eg(struct soundmon_state *s, struct soundmon_voice *cur, struct soundmon_synth_instrument *si, int32_t voice_idx) {
	(void)voice_idx;
	if(cur->eg_control == 0) {
		return;
	}
	cur->eg_count--;
	if(cur->eg_count != 0) {
		return;
	}
	cur->eg_count = si->eg_speed;

	uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;
	int32_t prev = (int32_t)cur->old_eg_value;
	uint32_t idx = (uint32_t)si->eg_table * SOUNDMON_WAVE_BYTES + cur->eg_ptr;
	int32_t new_val = 0;
	if(idx < wt_bytes) {
		new_val = ((int32_t)s->wave_tables[idx] + 128) / 8;
	}
	cur->old_eg_value = (uint8_t)new_val;

	if(new_val != prev) {
		int8_t *source = s->synth_buffer[voice_idx];
		int32_t source_offset = prev;
		int32_t dest_offset = cur->synth_offset + prev;

		if(new_val < prev) {
			int32_t n = prev - new_val;
			for(int32_t j = 0; j < n; ++j) {
				--dest_offset;
				--source_offset;
				if(dest_offset >= 0 && source_offset >= 0 && (uint32_t)dest_offset < wt_bytes && source_offset < 32) {
					s->wave_tables[dest_offset] = source[source_offset];
				}
			}
		} else {
			int32_t n = new_val - prev;
			for(int32_t j = 0; j < n; ++j) {
				if(dest_offset >= 0 && source_offset >= 0 && (uint32_t)dest_offset < wt_bytes && source_offset < 32) {
					s->wave_tables[dest_offset] = (int8_t)(-source[source_offset]);
				}
				dest_offset++;
				source_offset++;
			}
		}
	}

	cur->eg_ptr++;
	if(cur->eg_ptr == si->eg_length) {
		cur->eg_ptr = 0;
		if(cur->eg_control == 1) {
			cur->eg_control = 0;
		}
	}
}

// [=]===^=[ soundmon_averaging ]=================================================================[=]
static void soundmon_averaging(struct soundmon_state *s, int32_t voice_idx) {
	int32_t synth_offset = s->voices[voice_idx].synth_offset;
	uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;
	if(synth_offset < 0 || (uint32_t)(synth_offset + 32) > wt_bytes) {
		return;
	}
	int8_t last_val = s->wave_tables[synth_offset];
	for(int32_t i = 0; i < 31; ++i) {
		last_val = (int8_t)(((int32_t)last_val + (int32_t)s->wave_tables[synth_offset + 1]) / 2);
		s->wave_tables[synth_offset++] = last_val;
	}
}

// [=]===^=[ soundmon_transform2 ]================================================================[=]
static void soundmon_transform2(struct soundmon_state *s, int32_t voice_idx, int8_t delta) {
	int8_t *source = s->synth_buffer[voice_idx];
	int32_t source_offset = 31;
	int32_t dest_offset = s->voices[voice_idx].synth_offset;
	uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;

	for(int32_t i = 0; i < 32; ++i) {
		if(dest_offset < 0 || (uint32_t)dest_offset >= wt_bytes || source_offset < 0) {
			break;
		}
		int8_t source_val = source[source_offset];
		int8_t dest_val = s->wave_tables[dest_offset];

		if(source_val < dest_val) {
			s->wave_tables[dest_offset] = (int8_t)(s->wave_tables[dest_offset] - delta);
		} else if(source_val > dest_val) {
			s->wave_tables[dest_offset] = (int8_t)(s->wave_tables[dest_offset] + delta);
		}

		source_offset--;
		dest_offset++;
	}
}

// [=]===^=[ soundmon_transform3 ]================================================================[=]
static void soundmon_transform3(struct soundmon_state *s, int32_t voice_idx, int8_t delta) {
	int8_t *source = s->synth_buffer[voice_idx];
	int32_t source_offset = 0;
	int32_t dest_offset = s->voices[voice_idx].synth_offset;
	uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;

	for(int32_t i = 0; i < 32; ++i) {
		if(dest_offset < 0 || (uint32_t)dest_offset >= wt_bytes || source_offset >= 32) {
			break;
		}
		int8_t source_val = source[source_offset];
		int8_t dest_val = s->wave_tables[dest_offset];

		if(source_val < dest_val) {
			s->wave_tables[dest_offset] = (int8_t)(s->wave_tables[dest_offset] - delta);
		} else if(source_val > dest_val) {
			s->wave_tables[dest_offset] = (int8_t)(s->wave_tables[dest_offset] + delta);
		}

		source_offset++;
		dest_offset++;
	}
}

// [=]===^=[ soundmon_transform4 ]================================================================[=]
static void soundmon_transform4(struct soundmon_state *s, int32_t voice_idx, int8_t delta) {
	int32_t source_offset = s->voices[voice_idx].synth_offset + 64;
	int32_t dest_offset = s->voices[voice_idx].synth_offset;
	uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;

	for(int32_t i = 0; i < 32; ++i) {
		if(dest_offset < 0 || (uint32_t)dest_offset >= wt_bytes) {
			break;
		}
		int8_t source_val = ((source_offset < 0) || ((uint32_t)source_offset >= wt_bytes)) ? (int8_t)0 : s->wave_tables[source_offset];
		int8_t dest_val = s->wave_tables[dest_offset];

		if(source_val < dest_val) {
			s->wave_tables[dest_offset] = (int8_t)(s->wave_tables[dest_offset] - delta);
		} else if(source_val > dest_val) {
			s->wave_tables[dest_offset] = (int8_t)(s->wave_tables[dest_offset] + delta);
		}

		source_offset++;
		dest_offset++;
	}
}

// [=]===^=[ soundmon_do_fx ]=====================================================================[=]
static void soundmon_do_fx(struct soundmon_state *s, int32_t voice_idx, struct soundmon_voice *cur, struct soundmon_synth_instrument *si) {
	switch(cur->fx_control) {
		case 1: {
			cur->fx_count--;
			if(cur->fx_count == 0) {
				cur->fx_count = si->fx_speed;
				soundmon_averaging(s, voice_idx);
			}
			break;
		}

		case 2: {
			soundmon_transform2(s, voice_idx, (int8_t)si->fx_speed);
			break;
		}

		case 3:
		case 5: {
			soundmon_transform3(s, voice_idx, (int8_t)si->fx_speed);
			break;
		}

		case 4: {
			soundmon_transform4(s, voice_idx, (int8_t)si->fx_speed);
			break;
		}

		case 6: {
			cur->fx_count--;
			if(cur->fx_count == 0) {
				cur->fx_control = 0;
				cur->fx_count = 1;
				uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;
				int32_t src = cur->synth_offset + 64;
				int32_t dst = cur->synth_offset;
				if((src >= 0) && ((uint32_t)(src + 32) <= wt_bytes) && (dst >= 0) && ((uint32_t)(dst + 32) <= wt_bytes)) {
					memcpy(s->wave_tables + dst, s->wave_tables + src, 32);
				}
			}
			break;
		}

		default: break;
	}
}

// [=]===^=[ soundmon_do_mod ]====================================================================[=]
static void soundmon_do_mod(struct soundmon_state *s, struct soundmon_voice *cur, struct soundmon_synth_instrument *si) {
	if(cur->mod_control == 0) {
		return;
	}
	cur->mod_count--;
	if(cur->mod_count != 0) {
		return;
	}
	cur->mod_count = si->mod_speed;

	uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;
	int32_t dst = cur->synth_offset + 32;
	uint32_t src = (uint32_t)si->mod_table * SOUNDMON_WAVE_BYTES + cur->mod_ptr;
	if((dst >= 0) && ((uint32_t)dst < wt_bytes) && (src < wt_bytes)) {
		s->wave_tables[dst] = s->wave_tables[src];
	}

	cur->mod_ptr++;
	if(cur->mod_ptr == si->mod_length) {
		cur->mod_ptr = 0;
		if(cur->mod_control == 1) {
			cur->mod_control = 0;
		}
	}
}

// [=]===^=[ soundmon_do_synths ]=================================================================[=]
static void soundmon_do_synths(struct soundmon_state *s) {
	for(int32_t i = 0; i < 4; ++i) {
		struct soundmon_voice *cur = &s->voices[i];

		if(!cur->synth_mode) {
			continue;
		}
		if(cur->instrument == 0) {
			continue;
		}
		struct soundmon_instrument *inst = &s->instruments[cur->instrument - 1];
		if(!inst->is_synth) {
			continue;
		}
		struct soundmon_synth_instrument *si = &inst->synth;

		soundmon_do_adsr(s, i, cur, si);
		soundmon_do_lfo(s, i, cur, si);

		if(cur->synth_offset >= 0) {
			soundmon_do_eg(s, cur, si, i);
			soundmon_do_fx(s, i, cur, si);
			soundmon_do_mod(s, cur, si);
		}
	}
}

// [=]===^=[ soundmon_bp_play ]===================================================================[=]
static void soundmon_bp_play(struct soundmon_state *s) {
	soundmon_do_effects(s);
	soundmon_do_synths(s);

	s->bp_count--;
	if(s->bp_count == 0) {
		s->bp_count = s->bp_delay;
		soundmon_bp_next(s);

		for(int32_t i = 0; i < 4; ++i) {
			struct soundmon_voice *cur = &s->voices[i];
			if(cur->restart) {
				if(cur->synth_offset >= 0) {
					uint32_t wt_bytes = (uint32_t)s->wave_num * SOUNDMON_WAVE_BYTES;
					if((uint32_t)(cur->synth_offset + 32) <= wt_bytes) {
						memcpy(s->wave_tables + cur->synth_offset, s->synth_buffer[i], 32);
					}
					cur->synth_offset = -1;
				}
				soundmon_play_it(s, i);
			}
		}
	}
}

// [=]===^=[ soundmon_init ]======================================================================[=]
static struct soundmon_state *soundmon_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 512 || sample_rate < 8000) {
		return 0;
	}

	uint32_t module_type = SOUNDMON_TYPE_UNKNOWN;
	if(!soundmon_test_module((uint8_t *)data, len, &module_type)) {
		return 0;
	}

	struct soundmon_state *s = (struct soundmon_state *)calloc(1, sizeof(struct soundmon_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len  = len;
	s->module_type = module_type;

	if(!soundmon_load_module(s)) {
		soundmon_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, SOUNDMON_TICK_HZ);
	soundmon_initialize_sound(s);
	return s;
}

// [=]===^=[ soundmon_free ]======================================================================[=]
static void soundmon_free(struct soundmon_state *s) {
	if(!s) {
		return;
	}
	soundmon_cleanup(s);
	free(s);
}

// [=]===^=[ soundmon_get_audio ]=================================================================[=]
static void soundmon_get_audio(struct soundmon_state *s, int16_t *output, int32_t frames) {
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
			soundmon_bp_play(s);
		}
	}
}

// [=]===^=[ soundmon_api_init ]==================================================================[=]
static void *soundmon_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return soundmon_init(data, len, sample_rate);
}

// [=]===^=[ soundmon_api_free ]==================================================================[=]
static void soundmon_api_free(void *state) {
	soundmon_free((struct soundmon_state *)state);
}

// [=]===^=[ soundmon_api_get_audio ]=============================================================[=]
static void soundmon_api_get_audio(void *state, int16_t *output, int32_t frames) {
	soundmon_get_audio((struct soundmon_state *)state, output, frames);
}

static const char *soundmon_extensions[] = { "bp", "bp2", "bp3", 0 };

static struct player_api soundmon_api = {
	"SoundMon",
	soundmon_extensions,
	soundmon_api_init,
	soundmon_api_free,
	soundmon_api_get_audio,
	0,
};
