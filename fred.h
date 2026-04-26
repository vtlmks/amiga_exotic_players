// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Fred Editor replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct fred_state *fred_init(void *data, uint32_t len, int32_t sample_rate);
//   void fred_free(struct fred_state *s);
//   void fred_get_audio(struct fred_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define FRED_TICK_HZ          50
#define FRED_MAX_SUBSONGS     10
#define FRED_NUM_TRACKS       128
#define FRED_TRACK_END_CODE   0x80
#define FRED_TRACK_PORT_CODE  0x81
#define FRED_TRACK_TEMPO_CODE 0x82
#define FRED_TRACK_INST_CODE  0x83
#define FRED_TRACK_PAUSE_CODE 0x84
#define FRED_TRACK_MAX_CODE   0xa0

enum {
	FRED_INST_SAMPLE = 0x00,
	FRED_INST_PULSE  = 0x01,
	FRED_INST_BLEND  = 0x02,
	FRED_INST_UNUSED = 0xff,
};

enum {
	FRED_SYNC_PULSE_X_SHOT = 0x01,
	FRED_SYNC_PULSE_SYNC   = 0x02,
	FRED_SYNC_BLEND_X_SHOT = 0x04,
	FRED_SYNC_BLEND_SYNC   = 0x08,
};

enum {
	FRED_VIB_NONE             = 0x00,
	FRED_VIB_DIRECTION        = 0x01,
	FRED_VIB_PERIOD_DIRECTION = 0x02,
};

enum {
	FRED_ENV_ATTACK  = 0,
	FRED_ENV_DECAY   = 1,
	FRED_ENV_SUSTAIN = 2,
	FRED_ENV_RELEASE = 3,
	FRED_ENV_DONE    = 4,
};

struct fred_instrument {
	int16_t instrument_number;
	uint16_t repeat_len;
	uint16_t length;
	uint16_t period;
	uint8_t vib_delay;
	int8_t vib_speed;
	int8_t vib_ampl;
	uint8_t env_vol;
	uint8_t attack_speed;
	uint8_t attack_volume;
	uint8_t decay_speed;
	uint8_t decay_volume;
	uint8_t sustain_delay;
	uint8_t release_speed;
	uint8_t release_volume;
	int8_t arpeggio[16];
	uint8_t arp_speed;
	uint8_t inst_type;
	int8_t pulse_rate_min;
	int8_t pulse_rate_plus;
	uint8_t pulse_speed;
	uint8_t pulse_start;
	uint8_t pulse_end;
	uint8_t pulse_delay;
	uint8_t inst_sync;
	uint8_t blend;
	uint8_t blend_delay;
	uint8_t pulse_shot_counter;
	uint8_t blend_shot_counter;
	uint8_t arp_count;
	int8_t *sample_addr;
	uint32_t inst_index;
};

struct fred_track {
	uint8_t *data;
	uint32_t size;
};

struct fred_channel {
	int32_t chan_num;
	int8_t *position_table;
	uint8_t *track_table;
	uint16_t position;
	uint16_t track_position;
	uint16_t track_duration;
	uint8_t track_note;
	uint16_t track_period;
	int16_t track_volume;
	struct fred_instrument *instrument;
	uint8_t vib_flags;
	uint8_t vib_delay;
	int8_t vib_speed;
	int8_t vib_ampl;
	int8_t vib_value;
	uint8_t port_running;
	uint16_t port_delay;
	uint16_t port_limit;
	uint8_t port_target_note;
	uint16_t port_start_period;
	int16_t period_diff;
	uint16_t port_counter;
	uint16_t port_speed;
	uint8_t env_state;
	uint8_t sustain_delay;
	uint8_t arp_position;
	uint8_t arp_speed;
	uint8_t pulse_way;
	uint8_t pulse_position;
	uint8_t pulse_delay;
	uint8_t pulse_speed;
	uint8_t pulse_shot;
	uint8_t blend_way;
	uint16_t blend_position;
	uint8_t blend_delay;
	uint8_t blend_shot;
	int8_t synth_sample[64];
};

struct fred_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint16_t sub_song_num;
	uint16_t inst_num;

	uint8_t start_tempos[FRED_MAX_SUBSONGS];

	// positions[subsong][channel] -> 256 signed bytes
	int8_t *positions[FRED_MAX_SUBSONGS][4];

	struct fred_track tracks[FRED_NUM_TRACKS];

	struct fred_instrument *instruments;

	int8_t **owned_samples;     // heap-allocated sample buffers, freed in cleanup
	uint32_t num_owned_samples;

	int32_t current_song;
	uint16_t current_tempo;

	uint8_t has_notes[FRED_MAX_SUBSONGS][4];

	struct fred_channel channels[4];
};

// [=]===^=[ fred_period_table ]==================================================================[=]
static uint32_t fred_period_table[] = {
	8192, 7728, 7296, 6888, 6504, 6136, 5792, 5464, 5160, 4872, 4600, 4336,
	4096, 3864, 3648, 3444, 3252, 3068, 2896, 2732, 2580, 2436, 2300, 2168,
	2048, 1932, 1824, 1722, 1626, 1534, 1448, 1366, 1290, 1218, 1150, 1084,
	1024,  966,  912,  861,  813,  767,  724,  683,  645,  609,  575,  542,
	 512,  483,  456,  430,  406,  383,  362,  341,  322,  304,  287,  271,
	 256,  241,  228,  215,  203,  191,  181,  170,  161,  152,  143,  135,
};

// [=]===^=[ fred_read_u16_be ]===================================================================[=]
static uint16_t fred_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ fred_read_u32_be ]===================================================================[=]
static uint32_t fred_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ fred_test_module ]===================================================================[=]
static int32_t fred_test_module(uint8_t *buf, uint32_t len) {
	if(len < 20) {
		return 0;
	}
	if(memcmp(buf, "Fred Editor ", 12) != 0) {
		return 0;
	}
	if(fred_read_u16_be(buf + 12) != 0x0000) {
		return 0;
	}
	if(fred_read_u16_be(buf + 14) > FRED_MAX_SUBSONGS) {
		return 0;
	}
	if(fred_read_u32_be(buf + len - 4) != 0x12345678u) {
		return 0;
	}
	return 1;
}

// [=]===^=[ fred_cleanup ]=======================================================================[=]
static void fred_cleanup(struct fred_state *s) {
	if(!s) {
		return;
	}
	for(uint32_t i = 0; i < FRED_MAX_SUBSONGS; ++i) {
		for(int32_t j = 0; j < 4; ++j) {
			free(s->positions[i][j]);
			s->positions[i][j] = 0;
		}
	}
	for(int32_t i = 0; i < FRED_NUM_TRACKS; ++i) {
		free(s->tracks[i].data);
		s->tracks[i].data = 0;
		s->tracks[i].size = 0;
	}
	free(s->instruments);
	s->instruments = 0;
	if(s->owned_samples) {
		for(uint32_t i = 0; i < s->num_owned_samples; ++i) {
			free(s->owned_samples[i]);
		}
		free(s->owned_samples);
		s->owned_samples = 0;
		s->num_owned_samples = 0;
	}
}

// [=]===^=[ fred_load_module ]===================================================================[=]
static int32_t fred_load_module(struct fred_state *s) {
	uint8_t *buf = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = 14;

	if(pos + 2 > len) {
		return 0;
	}
	s->sub_song_num = fred_read_u16_be(buf + pos);
	pos += 2;

	if(s->sub_song_num > FRED_MAX_SUBSONGS) {
		return 0;
	}
	if(pos + s->sub_song_num > len) {
		return 0;
	}
	memcpy(s->start_tempos, buf + pos, s->sub_song_num);
	pos += s->sub_song_num;

	for(uint32_t i = 0; i < s->sub_song_num; ++i) {
		for(int32_t j = 0; j < 4; ++j) {
			if(pos + 256 > len) {
				return 0;
			}
			s->positions[i][j] = (int8_t *)malloc(256);
			if(!s->positions[i][j]) {
				return 0;
			}
			memcpy(s->positions[i][j], buf + pos, 256);
			pos += 256;
		}
	}

	for(int32_t i = 0; i < FRED_NUM_TRACKS; ++i) {
		if(pos + 4 > len) {
			return 0;
		}
		uint32_t track_size = fred_read_u32_be(buf + pos);
		pos += 4;
		if(pos + track_size > len) {
			return 0;
		}
		s->tracks[i].size = track_size;
		if(track_size > 0) {
			s->tracks[i].data = (uint8_t *)malloc(track_size);
			if(!s->tracks[i].data) {
				return 0;
			}
			memcpy(s->tracks[i].data, buf + pos, track_size);
		}
		pos += track_size;
	}

	if(pos + 2 > len) {
		return 0;
	}
	s->inst_num = fred_read_u16_be(buf + pos);
	pos += 2;

	if(s->inst_num > 0) {
		s->instruments = (struct fred_instrument *)calloc(s->inst_num, sizeof(struct fred_instrument));
		if(!s->instruments) {
			return 0;
		}
	}

	for(uint16_t i = 0; i < s->inst_num; ++i) {
		if(pos + 96 > len) {
			return 0;
		}
		struct fred_instrument *ins = &s->instruments[i];
		ins->instrument_number = (int16_t)i;
		uint8_t *p = buf + pos;
		// 32 bytes name (skipped)
		ins->inst_index      = fred_read_u32_be(p + 32);
		ins->repeat_len      = fred_read_u16_be(p + 36);
		ins->length          = (uint16_t)(fred_read_u16_be(p + 38) * 2);
		ins->period          = fred_read_u16_be(p + 40);
		ins->vib_delay       = p[42];
		// p[43] skipped
		ins->vib_speed       = (int8_t)p[44];
		ins->vib_ampl        = (int8_t)p[45];
		ins->env_vol         = p[46];
		ins->attack_speed    = p[47];
		ins->attack_volume   = p[48];
		ins->decay_speed     = p[49];
		ins->decay_volume    = p[50];
		ins->sustain_delay   = p[51];
		ins->release_speed   = p[52];
		ins->release_volume  = p[53];
		memcpy(ins->arpeggio, p + 54, 16);
		ins->arp_speed       = p[70];
		ins->inst_type       = p[71];
		ins->pulse_rate_min  = (int8_t)p[72];
		ins->pulse_rate_plus = (int8_t)p[73];
		ins->pulse_speed     = p[74];
		ins->pulse_start     = p[75];
		ins->pulse_end       = p[76];
		ins->pulse_delay     = p[77];
		ins->inst_sync       = p[78];
		ins->blend           = p[79];
		ins->blend_delay     = p[80];
		ins->pulse_shot_counter = p[81];
		ins->blend_shot_counter = p[82];
		ins->arp_count       = p[83];
		// 12 bytes padding skipped (84..95)
		ins->sample_addr = 0;
		pos += 96;
	}

	if(pos + 2 > len) {
		return 0;
	}
	uint16_t samp_num = fred_read_u16_be(buf + pos);
	pos += 2;

	if(samp_num > 0) {
		s->owned_samples = (int8_t **)calloc(samp_num, sizeof(int8_t *));
		if(!s->owned_samples) {
			return 0;
		}
	}

	for(uint16_t i = 0; i < samp_num; ++i) {
		if(pos + 4 > len) {
			return 0;
		}
		uint32_t inst_index = (uint32_t)fred_read_u16_be(buf + pos);
		uint32_t samp_size = (uint32_t)fred_read_u16_be(buf + pos + 2);
		pos += 4;
		if(pos + samp_size > len) {
			return 0;
		}

		// First instrument with matching inst_index gets the sample.
		struct fred_instrument *target = 0;
		for(uint16_t j = 0; j < s->inst_num; ++j) {
			if(s->instruments[j].inst_index == inst_index) {
				target = &s->instruments[j];
				break;
			}
		}
		if(!target) {
			return 0;
		}

		int8_t *smp = (int8_t *)malloc(samp_size > 0 ? samp_size : 1);
		if(!smp) {
			return 0;
		}
		if(samp_size > 0) {
			memcpy(smp, buf + pos, samp_size);
		}
		s->owned_samples[s->num_owned_samples++] = smp;
		// Only assign to first matching instrument; do not overwrite an existing one.
		if(!target->sample_addr) {
			target->sample_addr = smp;
		}
		pos += samp_size;
	}

	return 1;
}

// [=]===^=[ fred_calc_has_notes ]================================================================[=]
static void fred_calc_has_notes(struct fred_state *s) {
	memset(s->has_notes, 0, sizeof(s->has_notes));
	for(uint32_t i = 0; i < s->sub_song_num; ++i) {
		for(int32_t j = 0; j < 4; ++j) {
			int8_t *position_list = s->positions[i][j];
			for(int32_t k = 0; k < 256; ++k) {
				if(position_list[k] < 0) {
					break;
				}
				uint8_t track_idx = (uint8_t)position_list[k];
				if(track_idx >= FRED_NUM_TRACKS) {
					break;
				}
				struct fred_track *tr = &s->tracks[track_idx];
				if(!tr->data) {
					continue;
				}
				for(uint32_t m = 0; m < tr->size; ++m) {
					uint8_t cmd = tr->data[m];
					if(cmd < 0x80) {
						s->has_notes[i][j] = 1;
						break;
					}
					if(cmd == FRED_TRACK_END_CODE) {
						break;
					}
					if((cmd == FRED_TRACK_INST_CODE) || (cmd == FRED_TRACK_TEMPO_CODE)) {
						m++;
					} else if(cmd == FRED_TRACK_PORT_CODE) {
						m += 3;
					}
				}
				if(s->has_notes[i][j]) {
					break;
				}
			}
		}
	}
}

// [=]===^=[ fred_initialize_sound ]==============================================================[=]
static void fred_initialize_sound(struct fred_state *s, int32_t song_number) {
	s->current_song = song_number;
	s->current_tempo = s->start_tempos[song_number];

	for(int32_t i = 0; i < 4; ++i) {
		struct fred_channel *c = &s->channels[i];
		memset(c, 0, sizeof(*c));
		c->chan_num = i;
		c->position_table = s->positions[song_number][i];
		uint8_t track_idx = (uint8_t)c->position_table[0];
		if(track_idx < FRED_NUM_TRACKS) {
			c->track_table = s->tracks[track_idx].data;
		}
		c->position = 0;
		c->track_position = 0;
		c->track_duration = 1;
		c->env_state = FRED_ENV_ATTACK;
		c->vib_flags = FRED_VIB_NONE;
	}
}

// [=]===^=[ fred_create_synth_sample_pulse ]=====================================================[=]
static void fred_create_synth_sample_pulse(struct fred_instrument *ins, struct fred_channel *c) {
	c->pulse_shot = ins->pulse_shot_counter;
	c->pulse_delay = ins->pulse_delay;
	c->pulse_speed = ins->pulse_speed;
	c->pulse_way = 0;
	c->pulse_position = ins->pulse_start;

	uint32_t i;
	uint32_t end = ins->pulse_start;
	if(end > sizeof(c->synth_sample)) {
		end = sizeof(c->synth_sample);
	}
	for(i = 0; i < end; ++i) {
		c->synth_sample[i] = ins->pulse_rate_min;
	}
	uint32_t total = ins->length;
	if(total > sizeof(c->synth_sample)) {
		total = sizeof(c->synth_sample);
	}
	for(; i < total; ++i) {
		c->synth_sample[i] = ins->pulse_rate_plus;
	}
}

// [=]===^=[ fred_create_synth_sample_blend ]=====================================================[=]
static void fred_create_synth_sample_blend(struct fred_instrument *ins, struct fred_channel *c) {
	c->blend_way = 0;
	c->blend_position = 1;
	c->blend_shot = ins->blend_shot_counter;
	c->blend_delay = ins->blend_delay;

	if(ins->sample_addr) {
		for(int32_t i = 0; i < 32; ++i) {
			c->synth_sample[i] = ins->sample_addr[i];
		}
	} else {
		memset(c->synth_sample, 0, 32);
	}
}

// [=]===^=[ fred_do_new_line ]===================================================================[=]
// Returns 0 = take next channel, 1 = pause encountered, take next channel, 2 = repeat same channel
static uint8_t fred_do_new_line(struct fred_state *s, struct fred_channel *c, int32_t voice_idx) {
	uint8_t inst_change = 0;
	uint16_t track_pos = c->track_position;

	for(;;) {
		if(!c->track_table) {
			paula_mute(&s->paula, voice_idx);
			return 0;
		}
		uint8_t cmd = c->track_table[track_pos++];

		if(cmd < 0x80) {
			struct fred_instrument *ins = c->instrument;
			c->track_position = track_pos;
			if(!ins) {
				c->port_running = 0;
				c->vib_flags = FRED_VIB_NONE;
				c->track_volume = 0;
				paula_mute(&s->paula, voice_idx);
				return 0;
			}

			c->track_note = cmd;
			c->arp_position = 0;
			c->arp_speed = ins->arp_speed;
			c->vib_delay = ins->vib_delay;
			c->vib_speed = ins->vib_speed;
			c->vib_ampl = ins->vib_ampl;
			c->vib_flags = (uint8_t)(FRED_VIB_DIRECTION | FRED_VIB_PERIOD_DIRECTION);
			c->vib_value = 0;

			if((ins->inst_type == FRED_INST_PULSE) && (inst_change || ((ins->inst_sync & FRED_SYNC_PULSE_SYNC) != 0))) {
				fred_create_synth_sample_pulse(ins, c);
			} else if((ins->inst_type == FRED_INST_BLEND) && (inst_change || ((ins->inst_sync & FRED_SYNC_BLEND_SYNC) != 0))) {
				fred_create_synth_sample_blend(ins, c);
			}

			c->track_duration = s->current_tempo;

			if(ins->inst_type == FRED_INST_SAMPLE) {
				if(ins->sample_addr) {
					paula_play_sample(&s->paula, voice_idx, ins->sample_addr, ins->length);
					if((ins->repeat_len != 0) && (ins->repeat_len != 0xffff)) {
						// Mirrors the original player's quirk: it computes start = repeat_len
						// and length = total_length - repeat_len, which is "wrong" but matches.
						uint32_t loop_start = ins->repeat_len;
						uint32_t loop_length = (uint32_t)ins->length - (uint32_t)ins->repeat_len;
						paula_set_loop(&s->paula, voice_idx, loop_start, loop_length);
					}
				}
			} else {
				paula_play_sample(&s->paula, voice_idx, c->synth_sample, ins->length);
				paula_set_loop(&s->paula, voice_idx, 0, ins->length);
			}

			paula_set_volume_256(&s->paula, voice_idx, 0);

			c->track_volume = 0;
			c->env_state = FRED_ENV_ATTACK;
			c->sustain_delay = ins->sustain_delay;

			c->track_period = (uint16_t)((fred_period_table[c->track_note] * (uint32_t)ins->period) / 1024);
			paula_set_period(&s->paula, voice_idx, c->track_period);

			if(c->port_running && (c->port_start_period == 0)) {
				c->period_diff = (int16_t)(c->port_limit - c->track_period);
				c->port_counter = 1;
				c->port_start_period = c->track_period;
			}

			return 0;
		}

		switch(cmd) {
			case FRED_TRACK_INST_CODE: {
				uint8_t new_inst = c->track_table[track_pos++];
				if(new_inst >= s->inst_num) {
					c->instrument = 0;
				} else {
					c->instrument = &s->instruments[new_inst];
					if(c->instrument->inst_type == FRED_INST_UNUSED) {
						c->instrument = 0;
					} else {
						inst_change = 1;
					}
				}
				break;
			}

			case FRED_TRACK_TEMPO_CODE: {
				s->current_tempo = c->track_table[track_pos++];
				break;
			}

			case FRED_TRACK_PORT_CODE: {
				uint16_t inst_period = 428;
				if(c->instrument) {
					inst_period = c->instrument->period;
				}
				c->port_speed = (uint16_t)((uint32_t)c->track_table[track_pos++] * (uint32_t)s->current_tempo);
				c->port_target_note = c->track_table[track_pos++];
				c->port_limit = (uint16_t)((fred_period_table[c->port_target_note] * (uint32_t)inst_period) / 1024);
				c->port_start_period = 0;
				c->port_delay = (uint16_t)((uint32_t)c->track_table[track_pos++] * (uint32_t)s->current_tempo);
				c->port_running = 1;
				break;
			}

			case FRED_TRACK_PAUSE_CODE: {
				c->track_duration = s->current_tempo;
				c->track_position = track_pos;
				paula_mute(&s->paula, voice_idx);
				return 1;
			}

			case FRED_TRACK_END_CODE: {
				c->position++;
				for(;;) {
					if(c->position_table[c->position] == -1) {
						c->position = 0;
						s->current_tempo = s->start_tempos[s->current_song];
						continue;
					}
					if(c->position_table[c->position] < 0) {
						c->position = (uint16_t)(c->position_table[c->position] & 0x7f);
						continue;
					}
					break;
				}
				uint8_t track_idx = (uint8_t)c->position_table[c->position];
				c->track_table = (track_idx < FRED_NUM_TRACKS) ? s->tracks[track_idx].data : 0;
				c->track_position = 0;
				c->track_duration = 1;
				return 2;
			}

			default: {
				// Note delay: cmd is in 0x85..0xff range, treated as signed
				int32_t delay = -(int32_t)(int8_t)cmd;
				c->track_duration = (uint16_t)((uint32_t)delay * (uint32_t)s->current_tempo);
				c->track_position = track_pos;
				return 0;
			}
		}
	}
}

// [=]===^=[ fred_modify_sound ]==================================================================[=]
static void fred_modify_sound(struct fred_state *s, struct fred_channel *c, int32_t voice_idx) {
	struct fred_instrument *ins = c->instrument;
	if(!ins) {
		return;
	}

	uint8_t new_note = (uint8_t)((int32_t)c->track_note + (int32_t)ins->arpeggio[c->arp_position]);

	c->arp_speed--;
	if(c->arp_speed == 0) {
		c->arp_speed = ins->arp_speed;
		c->arp_position++;
		if(c->arp_position >= ins->arp_count) {
			c->arp_position = 0;
		}
	}

	if(new_note < (sizeof(fred_period_table) / sizeof(fred_period_table[0]))) {
		c->track_period = (uint16_t)((fred_period_table[new_note] * (uint32_t)ins->period) / 1024);
	}

	if(c->port_running) {
		if(c->port_delay != 0) {
			c->port_delay--;
		} else {
			if(c->port_speed != 0) {
				c->track_period = (uint16_t)((int32_t)c->track_period + ((int32_t)c->port_counter * (int32_t)c->period_diff) / (int32_t)c->port_speed);
			}
			c->port_counter++;
			if(c->port_counter > c->port_speed) {
				c->track_note = c->port_target_note;
				c->port_running = 0;
			}
		}
	}

	uint16_t period = c->track_period;

	if(c->vib_delay != 0) {
		c->vib_delay--;
	} else if(c->vib_flags != FRED_VIB_NONE) {
		if((c->vib_flags & FRED_VIB_DIRECTION) != 0) {
			c->vib_value = (int8_t)(c->vib_value + c->vib_speed);
			if(c->vib_value == c->vib_ampl) {
				c->vib_flags &= (uint8_t)~FRED_VIB_DIRECTION;
			}
		} else {
			c->vib_value = (int8_t)(c->vib_value - c->vib_speed);
			if(c->vib_value == 0) {
				c->vib_flags |= FRED_VIB_DIRECTION;
			}
		}
		if(c->vib_value == 0) {
			c->vib_flags ^= FRED_VIB_PERIOD_DIRECTION;
		}
		if((c->vib_flags & FRED_VIB_PERIOD_DIRECTION) != 0) {
			period = (uint16_t)(period + c->vib_value);
		} else {
			period = (uint16_t)(period - c->vib_value);
		}
	}

	paula_set_period(&s->paula, voice_idx, period);

	switch(c->env_state) {
		case FRED_ENV_ATTACK: {
			c->track_volume = (int16_t)(c->track_volume + ins->attack_speed);
			if(c->track_volume >= ins->attack_volume) {
				c->track_volume = ins->attack_volume;
				c->env_state = FRED_ENV_DECAY;
			}
			break;
		}

		case FRED_ENV_DECAY: {
			c->track_volume = (int16_t)(c->track_volume - ins->decay_speed);
			if(c->track_volume <= ins->decay_volume) {
				c->track_volume = ins->decay_volume;
				c->env_state = FRED_ENV_SUSTAIN;
			}
			break;
		}

		case FRED_ENV_SUSTAIN: {
			if(c->sustain_delay == 0) {
				c->env_state = FRED_ENV_RELEASE;
			} else {
				c->sustain_delay--;
			}
			break;
		}

		case FRED_ENV_RELEASE: {
			c->track_volume = (int16_t)(c->track_volume - ins->release_speed);
			if(c->track_volume <= ins->release_volume) {
				c->track_volume = ins->release_volume;
				c->env_state = FRED_ENV_DONE;
			}
			break;
		}

		default: break;
	}

	uint16_t out_vol = (uint16_t)(((int32_t)ins->env_vol * (int32_t)c->track_volume) / 256);
	paula_set_volume_256(&s->paula, voice_idx, out_vol);

	if(ins->inst_type == FRED_INST_PULSE) {
		if(c->pulse_delay != 0) {
			c->pulse_delay--;
		} else if(c->pulse_speed != 0) {
			c->pulse_speed--;
		} else {
			if(((ins->inst_sync & FRED_SYNC_PULSE_X_SHOT) == 0) || (c->pulse_shot != 0)) {
				c->pulse_speed = ins->pulse_speed;
				for(;;) {
					if(c->pulse_way) {
						if(c->pulse_position >= ins->pulse_start) {
							if(c->pulse_position < sizeof(c->synth_sample)) {
								c->synth_sample[c->pulse_position] = ins->pulse_rate_plus;
							}
							c->pulse_position--;
							break;
						}
						c->pulse_way = 0;
						c->pulse_shot--;
						c->pulse_position++;
					} else {
						if(c->pulse_position <= ins->pulse_end) {
							if(c->pulse_position < sizeof(c->synth_sample)) {
								c->synth_sample[c->pulse_position] = ins->pulse_rate_min;
							}
							c->pulse_position++;
							break;
						}
						c->pulse_way = 1;
						c->pulse_shot--;
						c->pulse_position--;
					}
				}
			}
		}
	}

	if(ins->inst_type == FRED_INST_BLEND) {
		if(c->blend_delay != 0) {
			c->blend_delay--;
		} else {
			uint8_t do_blend = 1;
			for(;;) {
				if(((ins->inst_sync & FRED_SYNC_BLEND_X_SHOT) == 0) || (c->blend_shot != 0)) {
					if(c->blend_way) {
						if(c->blend_position == 1) {
							c->blend_way = 0;
							c->blend_shot--;
							continue;
						}
						c->blend_position--;
						break;
					}
					if(c->blend_position == (uint16_t)(1u << ins->blend)) {
						c->blend_way = 1;
						c->blend_shot--;
						continue;
					}
					c->blend_position++;
					break;
				}
				do_blend = 0;
				break;
			}
			if(do_blend && ins->sample_addr) {
				for(int32_t i = 0; i < 32; ++i) {
					int32_t v = (((int32_t)c->blend_position * (int32_t)ins->sample_addr[i + 32]) >> ins->blend) + (int32_t)ins->sample_addr[i];
					c->synth_sample[i] = (int8_t)v;
				}
			}
		}
	}
}

// [=]===^=[ fred_tick ]==========================================================================[=]
static void fred_tick(struct fred_state *s) {
	for(int32_t i = 0; i < 4; ++i) {
		struct fred_channel *c = &s->channels[i];
		c->track_duration--;
		if(c->track_duration == 0) {
			uint8_t ret_val = fred_do_new_line(s, c, i);
			if(ret_val == 1) {
				continue;
			}
			if(ret_val == 2) {
				--i;
				continue;
			}
		} else {
			if((c->track_duration == 1) && c->track_table && (c->track_table[c->track_position] < FRED_TRACK_MAX_CODE)) {
				paula_mute(&s->paula, i);
			}
		}
		fred_modify_sound(s, c, i);
	}
}

// [=]===^=[ fred_init ]==========================================================================[=]
static struct fred_state *fred_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 20 || sample_rate < 8000) {
		return 0;
	}
	if(!fred_test_module((uint8_t *)data, len)) {
		return 0;
	}

	struct fred_state *s = (struct fred_state *)calloc(1, sizeof(struct fred_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;

	if(!fred_load_module(s)) {
		fred_cleanup(s);
		free(s);
		return 0;
	}

	fred_calc_has_notes(s);
	paula_init(&s->paula, sample_rate, FRED_TICK_HZ);
	fred_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ fred_free ]==========================================================================[=]
static void fred_free(struct fred_state *s) {
	if(!s) {
		return;
	}
	fred_cleanup(s);
	free(s);
}

// [=]===^=[ fred_get_audio ]=====================================================================[=]
static void fred_get_audio(struct fred_state *s, int16_t *output, int32_t frames) {
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
			fred_tick(s);
		}
	}
}

// [=]===^=[ fred_api_init ]======================================================================[=]
static void *fred_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return fred_init(data, len, sample_rate);
}

// [=]===^=[ fred_api_free ]======================================================================[=]
static void fred_api_free(void *state) {
	fred_free((struct fred_state *)state);
}

// [=]===^=[ fred_api_get_audio ]=================================================================[=]
static void fred_api_get_audio(void *state, int16_t *output, int32_t frames) {
	fred_get_audio((struct fred_state *)state, output, frames);
}

static const char *fred_extensions[] = { "frd", "fred", 0 };

static struct player_api fred_api = {
	"Fred Editor",
	fred_extensions,
	fred_api_init,
	fred_api_free,
	fred_api_get_audio,
	0,
};
