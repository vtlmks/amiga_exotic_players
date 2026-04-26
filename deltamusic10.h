// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Delta Music 1.0 replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct deltamusic10_state *deltamusic10_init(void *data, uint32_t len, int32_t sample_rate);
//   void deltamusic10_free(struct deltamusic10_state *s);
//   void deltamusic10_get_audio(struct deltamusic10_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define DM1_TICK_HZ          50
#define DM1_NUM_INSTRUMENTS  20
#define DM1_TABLE_LEN        48
#define DM1_BLOCK_ROWS       16

enum {
	DM1_EFFECT_NONE                = 0x00,
	DM1_EFFECT_SET_SPEED           = 0x01,
	DM1_EFFECT_SLIDE_UP            = 0x02,
	DM1_EFFECT_SLIDE_DOWN          = 0x03,
	DM1_EFFECT_SET_FILTER          = 0x04,
	DM1_EFFECT_SET_VIBRATO_WAIT    = 0x05,
	DM1_EFFECT_SET_VIBRATO_STEP    = 0x06,
	DM1_EFFECT_SET_VIBRATO_LENGTH  = 0x07,
	DM1_EFFECT_SET_BEND_RATE       = 0x08,
	DM1_EFFECT_SET_PORTAMENTO      = 0x09,
	DM1_EFFECT_SET_VOLUME          = 0x0a,
	DM1_EFFECT_SET_ARP1            = 0x0b,
	DM1_EFFECT_SET_ARP2            = 0x0c,
	DM1_EFFECT_SET_ARP3            = 0x0d,
	DM1_EFFECT_SET_ARP4            = 0x0e,
	DM1_EFFECT_SET_ARP5            = 0x0f,
	DM1_EFFECT_SET_ARP6            = 0x10,
	DM1_EFFECT_SET_ARP7            = 0x11,
	DM1_EFFECT_SET_ARP8            = 0x12,
	DM1_EFFECT_SET_ARP1_5          = 0x13,
	DM1_EFFECT_SET_ARP2_6          = 0x14,
	DM1_EFFECT_SET_ARP3_7          = 0x15,
	DM1_EFFECT_SET_ARP4_8          = 0x16,
	DM1_EFFECT_SET_ATTACK_STEP     = 0x17,
	DM1_EFFECT_SET_ATTACK_DELAY    = 0x18,
	DM1_EFFECT_SET_DECAY_STEP      = 0x19,
	DM1_EFFECT_SET_DECAY_DELAY     = 0x1a,
	DM1_EFFECT_SET_SUSTAIN1        = 0x1b,
	DM1_EFFECT_SET_SUSTAIN2        = 0x1c,
	DM1_EFFECT_SET_RELEASE_STEP    = 0x1d,
	DM1_EFFECT_SET_RELEASE_DELAY   = 0x1e
};

struct dm1_track_entry {
	uint8_t block_number;
	int8_t transpose;
};

struct dm1_block_line {
	uint8_t instrument;
	uint8_t note;
	uint8_t effect;
	uint8_t effect_arg;
};

struct dm1_block {
	struct dm1_block_line lines[DM1_BLOCK_ROWS];
};

struct dm1_instrument {
	uint8_t valid;
	int16_t number;
	uint8_t attack_step;
	uint8_t attack_delay;
	uint8_t decay_step;
	uint8_t decay_delay;
	uint16_t sustain;
	uint8_t release_step;
	uint8_t release_delay;
	uint8_t volume;
	uint8_t vibrato_wait;
	uint8_t vibrato_step;
	uint8_t vibrato_length;
	int8_t bend_rate;
	uint8_t portamento;
	uint8_t is_sample;
	uint8_t table_delay;
	uint8_t arpeggio[8];
	uint16_t sample_length;
	uint16_t repeat_start;
	uint16_t repeat_length;
	uint8_t table[DM1_TABLE_LEN];
	int8_t *sample_data;     // points into module buffer (or owned dummy for synth)
};

struct dm1_channel {
	struct dm1_instrument *sound_data;
	uint16_t period;
	uint8_t *sound_table;
	uint8_t sound_table_counter;
	uint8_t sound_table_delay;
	struct dm1_track_entry *track;
	uint32_t track_length;
	uint16_t track_counter;
	struct dm1_block *block;
	uint32_t block_counter;
	uint8_t vibrato_wait;
	uint8_t vibrato_length;
	uint8_t vibrato_position;
	uint8_t vibrato_compare;
	uint16_t vibrato_frequency;
	uint8_t frequency_data;
	uint8_t actual_volume;
	uint8_t attack_delay;
	uint8_t decay_delay;
	uint16_t sustain;
	uint8_t release_delay;
	uint8_t play_speed;
	int16_t bend_rate_frequency;
	int8_t transpose;
	uint8_t status;
	uint8_t arpeggio_counter;
	uint8_t effect_number;
	uint8_t effect_data;
	uint8_t retrigger_sound;
};

struct deltamusic10_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	struct dm1_track_entry *tracks[4];
	uint32_t track_lengths[4];

	struct dm1_block *blocks;
	uint32_t num_blocks;

	struct dm1_instrument backup_instruments[DM1_NUM_INSTRUMENTS];
	struct dm1_instrument instruments[DM1_NUM_INSTRUMENTS];

	uint8_t play_speed;

	struct dm1_channel channels[4];
};

// [=]===^=[ dm1_periods ]========================================================================[=]
static uint16_t dm1_periods[] = {
	   0, 6848, 6464, 6096, 5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840,
	3616, 3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920,
	1808, 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076,  960,  904,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  452,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
	 113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113
};

#define DM1_PERIODS_LEN (sizeof(dm1_periods) / sizeof(dm1_periods[0]))

// [=]===^=[ dm1_read_u16_be ]====================================================================[=]
static uint16_t dm1_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ dm1_read_u32_be ]====================================================================[=]
static uint32_t dm1_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ dm1_read_i32_be ]====================================================================[=]
static int32_t dm1_read_i32_be(uint8_t *p) {
	return (int32_t)dm1_read_u32_be(p);
}

// [=]===^=[ dm1_period_at ]======================================================================[=]
// Index into the period table with bounds checking. Out-of-range returns 0.
static uint16_t dm1_period_at(int32_t index) {
	if(index < 0 || (uint32_t)index >= DM1_PERIODS_LEN) {
		return 0;
	}
	return dm1_periods[index];
}

// [=]===^=[ dm1_cleanup ]========================================================================[=]
static void dm1_cleanup(struct deltamusic10_state *s) {
	if(!s) {
		return;
	}
	for(int32_t i = 0; i < 4; ++i) {
		free(s->tracks[i]);
		s->tracks[i] = 0;
	}
	free(s->blocks); s->blocks = 0;
}

// [=]===^=[ dm1_identify ]=======================================================================[=]
static int32_t dm1_identify(uint8_t *data, uint32_t len) {
	if(len < 104) {
		return 0;
	}
	if(data[0] != 'A' || data[1] != 'L' || data[2] != 'L' || data[3] != ' ') {
		return 0;
	}

	uint64_t total_length = 104;
	uint32_t pos = 4;
	for(int32_t i = 0; i < 25; ++i) {
		// signed int32 BE; treat as signed sum to mirror the original test.
		total_length += (uint64_t)(int64_t)dm1_read_i32_be(data + pos);
		pos += 4;
	}
	if(total_length > (uint64_t)len) {
		return 0;
	}
	return 1;
}

// [=]===^=[ dm1_load ]===========================================================================[=]
static int32_t dm1_load(struct deltamusic10_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	uint32_t pos = 4;

	uint32_t track_byte_lengths[4];
	for(int32_t i = 0; i < 4; ++i) {
		track_byte_lengths[i] = dm1_read_u32_be(data + pos);
		pos += 4;
	}

	uint32_t block_length = dm1_read_u32_be(data + pos);
	pos += 4;

	uint32_t instrument_lengths[DM1_NUM_INSTRUMENTS];
	for(int32_t i = 0; i < DM1_NUM_INSTRUMENTS; ++i) {
		instrument_lengths[i] = dm1_read_u32_be(data + pos);
		pos += 4;
	}

	// Tracks: each track is track_byte_lengths[i]/2 entries of 2 bytes.
	for(int32_t i = 0; i < 4; ++i) {
		uint32_t entries = track_byte_lengths[i] / 2;
		s->track_lengths[i] = entries;
		if(entries == 0) {
			s->tracks[i] = 0;
			continue;
		}
		if((uint64_t)pos + (uint64_t)entries * 2 > len) {
			return 0;
		}
		s->tracks[i] = (struct dm1_track_entry *)calloc((size_t)entries, sizeof(struct dm1_track_entry));
		if(!s->tracks[i]) {
			return 0;
		}
		for(uint32_t j = 0; j < entries; ++j) {
			s->tracks[i][j].block_number = data[pos++];
			s->tracks[i][j].transpose    = (int8_t)data[pos++];
		}
	}

	// Blocks
	s->num_blocks = block_length / 64;
	if((uint64_t)pos + (uint64_t)s->num_blocks * 64 > len) {
		return 0;
	}
	if(s->num_blocks == 0) {
		s->blocks = 0;
	} else {
		s->blocks = (struct dm1_block *)calloc((size_t)s->num_blocks, sizeof(struct dm1_block));
		if(!s->blocks) {
			return 0;
		}
	}
	for(uint32_t i = 0; i < s->num_blocks; ++i) {
		for(int32_t j = 0; j < DM1_BLOCK_ROWS; ++j) {
			s->blocks[i].lines[j].instrument = data[pos++];
			s->blocks[i].lines[j].note       = data[pos++];
			s->blocks[i].lines[j].effect     = data[pos++];
			s->blocks[i].lines[j].effect_arg = data[pos++];
		}
	}

	// Instruments
	memset(s->backup_instruments, 0, sizeof(s->backup_instruments));
	for(int32_t i = 0; i < DM1_NUM_INSTRUMENTS; ++i) {
		uint32_t length = instrument_lengths[i];
		if(length == 0) {
			continue;
		}
		struct dm1_instrument *inst = &s->backup_instruments[i];
		inst->valid = 1;
		inst->number = (int16_t)i;

		// 30 bytes header (without the optional 48-byte synth table)
		if((uint64_t)pos + 30 > len) {
			return 0;
		}

		inst->attack_step    = data[pos++];
		inst->attack_delay   = data[pos++];
		inst->decay_step     = data[pos++];
		inst->decay_delay    = data[pos++];
		inst->sustain        = dm1_read_u16_be(data + pos); pos += 2;
		inst->release_step   = data[pos++];
		inst->release_delay  = data[pos++];
		inst->volume         = data[pos++];
		inst->vibrato_wait   = data[pos++];
		inst->vibrato_step   = data[pos++];
		inst->vibrato_length = data[pos++];
		inst->bend_rate      = (int8_t)data[pos++];
		inst->portamento     = data[pos++];
		inst->is_sample      = (data[pos++] != 0) ? 1 : 0;
		inst->table_delay    = data[pos++];

		memcpy(inst->arpeggio, data + pos, 8);
		pos += 8;

		inst->sample_length = dm1_read_u16_be(data + pos); pos += 2;
		inst->repeat_start  = dm1_read_u16_be(data + pos); pos += 2;
		inst->repeat_length = dm1_read_u16_be(data + pos); pos += 2;

		uint32_t header_used = 30;
		if(!inst->is_sample) {
			if((uint64_t)pos + DM1_TABLE_LEN > len) {
				return 0;
			}
			memcpy(inst->table, data + pos, DM1_TABLE_LEN);
			pos += DM1_TABLE_LEN;
			header_used = 78;
		}

		if(length < header_used) {
			return 0;
		}
		uint32_t sample_bytes = length - header_used;
		if((uint64_t)pos + (uint64_t)sample_bytes > len) {
			return 0;
		}
		inst->sample_data = (sample_bytes != 0) ? (int8_t *)(data + pos) : 0;
		pos += sample_bytes;
	}

	return 1;
}

// [=]===^=[ dm1_initialize_sound ]===============================================================[=]
static void dm1_initialize_sound(struct deltamusic10_state *s) {
	s->play_speed = 6;

	memcpy(s->instruments, s->backup_instruments, sizeof(s->instruments));

	// Find first valid instrument's table to seed default sound_table pointer.
	uint8_t *default_table = 0;
	for(int32_t i = 0; i < DM1_NUM_INSTRUMENTS; ++i) {
		if(s->instruments[i].valid) {
			default_table = s->instruments[i].table;
			break;
		}
	}

	for(int32_t i = 0; i < 4; ++i) {
		struct dm1_channel *c = &s->channels[i];
		memset(c, 0, sizeof(*c));
		c->sound_data = 0;
		c->period = 0;
		c->sound_table = default_table;
		c->sound_table_counter = 0;
		c->sound_table_delay = 0;
		c->track = s->tracks[i];
		c->track_length = s->track_lengths[i];
		c->track_counter = 0;
		c->block = (s->num_blocks > 0) ? &s->blocks[0] : 0;
		c->block_counter = 0;
		c->vibrato_wait = 0;
		c->vibrato_length = 0;
		c->vibrato_position = 0;
		c->vibrato_compare = 0;
		c->vibrato_frequency = 0;
		c->frequency_data = 0;
		c->actual_volume = 0;
		c->attack_delay = 0;
		c->decay_delay = 0;
		c->sustain = 0;
		c->release_delay = 0;
		c->play_speed = 1;
		c->bend_rate_frequency = 0;
		c->transpose = 0;
		c->status = 0;
		c->arpeggio_counter = 0;
		c->effect_number = DM1_EFFECT_NONE;
		c->effect_data = 0;
		c->retrigger_sound = 0;
	}
}

// [=]===^=[ dm1_sound_table_handler ]============================================================[=]
static void dm1_sound_table_handler(struct deltamusic10_state *s, int32_t chan, struct dm1_channel *c, struct dm1_instrument *inst) {
	c->sound_table_delay = inst->table_delay;

	for(;;) {
		if(c->sound_table_counter >= DM1_TABLE_LEN) {
			c->sound_table_counter = 0;
		}
		uint8_t entry = c->sound_table[c->sound_table_counter];
		if(entry == 0xff) {
			c->sound_table_counter = c->sound_table[c->sound_table_counter + 1];
		} else if(entry >= 0x80) {
			inst->table_delay = (uint8_t)(entry & 0x7f);
			c->sound_table_counter++;
		} else {
			c->sound_table_counter++;

			uint32_t offset = (uint32_t)entry * 32;
			if(offset > inst->sample_length) {
				offset = inst->sample_length;
			}
			uint32_t play_len = inst->sample_length - offset;

			if(c->retrigger_sound) {
				if(inst->sample_data && play_len > 0) {
					paula_play_sample(&s->paula, chan, inst->sample_data + offset, play_len);
					paula_set_loop(&s->paula, chan, 0, play_len);
				} else {
					paula_mute(&s->paula, chan);
				}
				c->retrigger_sound = 0;
			} else {
				if(inst->sample_data && play_len > 0) {
					paula_queue_sample(&s->paula, chan, inst->sample_data, offset, play_len);
					paula_set_loop(&s->paula, chan, offset, play_len);
				}
			}
			break;
		}
	}
}

// [=]===^=[ dm1_portamento_handler ]=============================================================[=]
static void dm1_portamento_handler(struct dm1_channel *c, struct dm1_instrument *inst) {
	if(inst->portamento == 0) {
		return;
	}

	uint16_t target = (uint16_t)((int32_t)dm1_period_at((int32_t)c->frequency_data) + (int32_t)c->bend_rate_frequency);

	if(c->period == 0) {
		c->period = target;
		return;
	}

	uint16_t period = c->period;
	if(period > target) {
		uint16_t np = (uint16_t)(period - inst->portamento);
		c->period = (np < target) ? target : np;
	} else if(period < target) {
		uint16_t np = (uint16_t)(period + inst->portamento);
		c->period = (np > target) ? target : np;
	}
}

// [=]===^=[ dm1_vibrato_handler ]================================================================[=]
static void dm1_vibrato_handler(struct dm1_channel *c, struct dm1_instrument *inst) {
	if(c->vibrato_wait != 0) {
		c->vibrato_wait--;
		return;
	}

	c->vibrato_frequency = (uint16_t)((uint32_t)c->vibrato_position * (uint32_t)inst->vibrato_step);

	if((c->status & 0x01) != 0) {
		c->vibrato_position--;
		if(c->vibrato_position == 0) {
			c->status ^= 0x01;
		}
	} else {
		c->vibrato_position++;
		if(c->vibrato_position == c->vibrato_compare) {
			c->status ^= 0x01;
		}
	}
}

// [=]===^=[ dm1_bend_rate_handler ]==============================================================[=]
static void dm1_bend_rate_handler(struct dm1_channel *c, struct dm1_instrument *inst) {
	if(inst->bend_rate >= 0) {
		c->bend_rate_frequency = (int16_t)(c->bend_rate_frequency - inst->bend_rate);
	} else {
		c->bend_rate_frequency = (int16_t)(c->bend_rate_frequency + (-inst->bend_rate));
	}
}

// [=]===^=[ dm1_change_speed ]===================================================================[=]
static void dm1_change_speed(struct deltamusic10_state *s, uint8_t new_speed) {
	if(new_speed != s->play_speed) {
		s->play_speed = new_speed;
	}
}

// [=]===^=[ dm1_effect_handler ]=================================================================[=]
static void dm1_effect_handler(struct deltamusic10_state *s, struct dm1_channel *c, struct dm1_instrument *inst) {
	uint8_t data = c->effect_data;

	switch(c->effect_number) {
		case DM1_EFFECT_SET_SPEED: {
			if(data != 0) {
				dm1_change_speed(s, data);
			}
			break;
		}

		case DM1_EFFECT_SLIDE_UP: {
			c->bend_rate_frequency = (int16_t)(c->bend_rate_frequency - data);
			break;
		}

		case DM1_EFFECT_SLIDE_DOWN: {
			c->bend_rate_frequency = (int16_t)(c->bend_rate_frequency + data);
			break;
		}

		case DM1_EFFECT_SET_FILTER: {
			// Amiga LED filter; not modeled.
			break;
		}

		case DM1_EFFECT_SET_VIBRATO_WAIT: {
			inst->vibrato_wait = data;
			break;
		}

		case DM1_EFFECT_SET_VIBRATO_STEP: {
			inst->vibrato_step = data;
			break;
		}

		case DM1_EFFECT_SET_VIBRATO_LENGTH: {
			inst->vibrato_length = data;
			break;
		}

		case DM1_EFFECT_SET_BEND_RATE: {
			inst->bend_rate = (int8_t)data;
			break;
		}

		case DM1_EFFECT_SET_PORTAMENTO: {
			inst->portamento = data;
			break;
		}

		case DM1_EFFECT_SET_VOLUME: {
			if(data > 64) {
				data = 64;
			}
			inst->volume = data;
			break;
		}

		case DM1_EFFECT_SET_ARP1: { inst->arpeggio[0] = data; break; }
		case DM1_EFFECT_SET_ARP2: { inst->arpeggio[1] = data; break; }
		case DM1_EFFECT_SET_ARP3: { inst->arpeggio[2] = data; break; }
		case DM1_EFFECT_SET_ARP4: { inst->arpeggio[3] = data; break; }
		case DM1_EFFECT_SET_ARP5: { inst->arpeggio[4] = data; break; }
		case DM1_EFFECT_SET_ARP6: { inst->arpeggio[5] = data; break; }
		case DM1_EFFECT_SET_ARP7: { inst->arpeggio[6] = data; break; }
		case DM1_EFFECT_SET_ARP8: { inst->arpeggio[7] = data; break; }

		case DM1_EFFECT_SET_ARP1_5: {
			inst->arpeggio[0] = data;
			inst->arpeggio[4] = data;
			break;
		}
		case DM1_EFFECT_SET_ARP2_6: {
			inst->arpeggio[1] = data;
			inst->arpeggio[5] = data;
			break;
		}
		case DM1_EFFECT_SET_ARP3_7: {
			inst->arpeggio[2] = data;
			inst->arpeggio[6] = data;
			break;
		}
		case DM1_EFFECT_SET_ARP4_8: {
			inst->arpeggio[3] = data;
			inst->arpeggio[7] = data;
			break;
		}

		case DM1_EFFECT_SET_ATTACK_STEP: {
			if(data > 64) {
				data = 64;
			}
			inst->attack_step = data;
			break;
		}

		case DM1_EFFECT_SET_ATTACK_DELAY: {
			inst->attack_delay = data;
			break;
		}

		case DM1_EFFECT_SET_DECAY_STEP: {
			if(data > 64) {
				data = 64;
			}
			inst->decay_step = data;
			break;
		}

		case DM1_EFFECT_SET_DECAY_DELAY: {
			inst->decay_delay = data;
			break;
		}

		case DM1_EFFECT_SET_SUSTAIN1: {
			inst->sustain = (uint16_t)((inst->sustain & 0x00ff) | ((uint16_t)data << 8));
			break;
		}

		case DM1_EFFECT_SET_SUSTAIN2: {
			inst->sustain = (uint16_t)((inst->sustain & 0xff00) | (uint16_t)data);
			break;
		}

		case DM1_EFFECT_SET_RELEASE_STEP: {
			if(data > 64) {
				data = 64;
			}
			inst->release_step = data;
			break;
		}

		case DM1_EFFECT_SET_RELEASE_DELAY: {
			inst->release_delay = data;
			break;
		}

		default: break;
	}
}

// [=]===^=[ dm1_arpeggio_handler ]===============================================================[=]
static void dm1_arpeggio_handler(struct deltamusic10_state *s, int32_t chan, struct dm1_channel *c, struct dm1_instrument *inst) {
	uint8_t arp = inst->arpeggio[c->arpeggio_counter];
	c->arpeggio_counter++;
	c->arpeggio_counter &= 0x07;

	int32_t idx = (int32_t)c->frequency_data + (int32_t)arp;
	uint16_t base = dm1_period_at(idx);
	int32_t new_period = (int32_t)base - ((int32_t)c->vibrato_length * (int32_t)inst->vibrato_step) + (int32_t)c->bend_rate_frequency;

	if(inst->portamento != 0) {
		new_period = (int32_t)c->period;
	} else {
		c->period = 0;
	}

	new_period += (int32_t)c->vibrato_frequency;

	uint16_t out_period = (uint16_t)new_period;
	paula_set_period(&s->paula, chan, out_period);
}

// [=]===^=[ dm1_volume_handler ]=================================================================[=]
static void dm1_volume_handler(struct deltamusic10_state *s, int32_t chan, struct dm1_channel *c, struct dm1_instrument *inst) {
	int32_t actual_volume = (int32_t)c->actual_volume;
	uint8_t status = (uint8_t)(c->status & 0x0e);

	if(status == 0x00) {
		if(c->attack_delay == 0) {
			c->attack_delay = inst->attack_delay;
			actual_volume += (int32_t)inst->attack_step;
			if(actual_volume >= 64) {
				actual_volume = 64;
				status |= 0x02;
				c->status |= 0x02;
			}
		} else {
			c->attack_delay--;
		}
	}

	if(status == 0x02) {
		if(c->decay_delay == 0) {
			c->decay_delay = inst->decay_delay;
			actual_volume -= (int32_t)inst->decay_step;
			if(actual_volume <= (int32_t)inst->volume) {
				actual_volume = (int32_t)inst->volume;
				status |= 0x06;
				c->status |= 0x06;
			}
		} else {
			c->decay_delay--;
		}
	}

	if(status == 0x06) {
		if(c->sustain == 0) {
			status |= 0x0e;
			c->status |= 0x0e;
		} else {
			c->sustain--;
		}
	}

	if(status == 0x0e) {
		if(c->release_delay == 0) {
			c->release_delay = inst->release_delay;
			actual_volume -= (int32_t)inst->release_step;
			if(actual_volume <= 0) {
				actual_volume = 0;
				c->status &= 0x09;
			}
		} else {
			c->release_delay--;
		}
	}

	c->actual_volume = (uint8_t)actual_volume;
	paula_set_volume(&s->paula, chan, (uint16_t)actual_volume);
}

// [=]===^=[ dm1_calculate_frequency ]============================================================[=]
static void dm1_calculate_frequency(struct deltamusic10_state *s, int32_t chan) {
	struct dm1_channel *c = &s->channels[chan];
	struct dm1_instrument *inst = c->sound_data;

	c->play_speed--;
	if(c->play_speed == 0) {
		c->play_speed = s->play_speed;

		if(c->block_counter == 0) {
			struct dm1_track_entry *new_track;

			if(c->track_length == 0) {
				return;
			}

			if((uint32_t)c->track_counter >= c->track_length) {
				c->track_counter = 0;
			}

			for(;;) {
				new_track = &c->track[c->track_counter];
				if((new_track->block_number != 0xff) || (new_track->transpose != -1)) {
					break;
				}

				if((uint32_t)(c->track_counter + 1) >= c->track_length) {
					// Malformed jump pair at end of track; fall back to start.
					c->track_counter = 0;
					new_track = &c->track[0];
					break;
				}

				struct dm1_track_entry *jump = &c->track[c->track_counter + 1];
				c->track_counter = (uint16_t)((((uint32_t)jump->block_number << 8) | (uint32_t)(uint8_t)jump->transpose) & 0x7ff);

				if((uint32_t)c->track_counter >= c->track_length) {
					c->track_counter = 0;
				}
			}

			c->transpose = new_track->transpose;
			uint8_t bn = new_track->block_number;
			if(bn >= s->num_blocks) {
				bn = 0;
			}
			c->block = (s->num_blocks > 0) ? &s->blocks[bn] : 0;
			c->track_counter++;
		}

		if(c->block != 0) {
			struct dm1_block_line *line = &c->block->lines[c->block_counter];

			if(line->effect != DM1_EFFECT_NONE) {
				c->effect_number = line->effect;
				c->effect_data   = line->effect_arg;
			}

			if(line->note != 0) {
				c->frequency_data = (uint8_t)((int32_t)line->note + (int32_t)c->transpose);
				c->status = 0;
				c->bend_rate_frequency = 0;
				c->arpeggio_counter = 0;

				c->effect_number = line->effect;
				c->effect_data   = line->effect_arg;

				if(line->instrument < DM1_NUM_INSTRUMENTS && s->instruments[line->instrument].valid) {
					inst = &s->instruments[line->instrument];
					c->sound_data = inst;

					c->sound_table = inst->table;
					c->sound_table_counter = 0;

					if(inst->is_sample) {
						if(inst->sample_data && inst->sample_length > 0) {
							paula_play_sample(&s->paula, chan, inst->sample_data, inst->sample_length);
							if(inst->repeat_length > 1) {
								paula_set_loop(&s->paula, chan, inst->repeat_start, inst->repeat_length);
							} else {
								paula_set_loop(&s->paula, chan, 0, 0);
							}
						} else {
							paula_mute(&s->paula, chan);
						}
					} else {
						c->retrigger_sound = 1;
					}

					c->vibrato_wait     = inst->vibrato_wait;
					uint8_t vib_len     = inst->vibrato_length;
					c->vibrato_length   = vib_len;
					c->vibrato_position = vib_len;
					c->vibrato_compare  = (uint8_t)(vib_len * 2);

					c->actual_volume       = 0;
					c->sound_table_delay   = 0;
					c->sound_table_counter = 0;
					c->attack_delay        = 0;
					c->decay_delay         = 0;
					c->sustain             = inst->sustain;
					c->release_delay       = 0;
				}
			}

			c->block_counter++;
			if(c->block_counter == DM1_BLOCK_ROWS) {
				c->block_counter = 0;
			}
		}
	}

	if(inst != 0) {
		if(!inst->is_sample) {
			if(c->sound_table_delay == 0) {
				dm1_sound_table_handler(s, chan, c, inst);
			} else {
				c->sound_table_delay--;
			}
		}

		dm1_portamento_handler(c, inst);
		dm1_vibrato_handler(c, inst);
		dm1_bend_rate_handler(c, inst);
		dm1_effect_handler(s, c, inst);
		dm1_arpeggio_handler(s, chan, c, inst);
		dm1_volume_handler(s, chan, c, inst);
	}
}

// [=]===^=[ dm1_tick ]===========================================================================[=]
static void dm1_tick(struct deltamusic10_state *s) {
	for(int32_t i = 0; i < 4; ++i) {
		dm1_calculate_frequency(s, i);
	}
}

// [=]===^=[ deltamusic10_init ]==================================================================[=]
static struct deltamusic10_state *deltamusic10_init(void *data, uint32_t len, int32_t sample_rate) {
	struct deltamusic10_state *s;

	if(!data || len < 104 || sample_rate < 8000) {
		return 0;
	}
	if(!dm1_identify((uint8_t *)data, len)) {
		return 0;
	}

	s = (struct deltamusic10_state *)calloc(1, sizeof(struct deltamusic10_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;

	if(!dm1_load(s)) {
		goto fail;
	}

	paula_init(&s->paula, sample_rate, DM1_TICK_HZ);
	dm1_initialize_sound(s);
	return s;

fail:
	dm1_cleanup(s);
	free(s);
	return 0;
}

// [=]===^=[ deltamusic10_free ]==================================================================[=]
static void deltamusic10_free(struct deltamusic10_state *s) {
	if(!s) {
		return;
	}
	dm1_cleanup(s);
	free(s);
}

// [=]===^=[ deltamusic10_get_audio ]==============================================================[=]
static void deltamusic10_get_audio(struct deltamusic10_state *s, int16_t *output, int32_t frames) {
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
			dm1_tick(s);
		}
	}
}

// [=]===^=[ deltamusic10_api_init ]==============================================================[=]
static void *deltamusic10_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return deltamusic10_init(data, len, sample_rate);
}

// [=]===^=[ deltamusic10_api_free ]==============================================================[=]
static void deltamusic10_api_free(void *state) {
	deltamusic10_free((struct deltamusic10_state *)state);
}

// [=]===^=[ deltamusic10_api_get_audio ]=========================================================[=]
static void deltamusic10_api_get_audio(void *state, int16_t *output, int32_t frames) {
	deltamusic10_get_audio((struct deltamusic10_state *)state, output, frames);
}

static const char *deltamusic10_extensions[] = { "dm1", 0 };

static struct player_api deltamusic10_api = {
	"Delta Music 1.0",
	deltamusic10_extensions,
	deltamusic10_api_init,
	deltamusic10_api_free,
	deltamusic10_api_get_audio,
	0,
};
