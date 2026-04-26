// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Delta Music 2.0 replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct deltamusic20_state *deltamusic20_init(void *data, uint32_t len, int32_t sample_rate);
//   void deltamusic20_free(struct deltamusic20_state *s);
//   void deltamusic20_get_audio(struct deltamusic20_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define DM2_TICK_HZ        50
#define DM2_NUM_ARPEGGIOS  64
#define DM2_ARPEGGIO_LEN   16
#define DM2_NUM_INSTR      128
#define DM2_NUM_SAMPLE_SLOTS 8
#define DM2_WAVEFORM_LEN   256
#define DM2_INSTR_TABLE_LEN 48
#define DM2_BLOCK_ROWS     16

enum {
	DM2_EFFECT_NONE             = 0x00,
	DM2_EFFECT_SET_SPEED        = 0x01,
	DM2_EFFECT_SET_FILTER       = 0x02,
	DM2_EFFECT_SET_BEND_RATE_UP = 0x03,
	DM2_EFFECT_SET_BEND_RATE_DN = 0x04,
	DM2_EFFECT_SET_PORTAMENTO   = 0x05,
	DM2_EFFECT_SET_VOLUME       = 0x06,
	DM2_EFFECT_SET_GLOBAL_VOL   = 0x07,
	DM2_EFFECT_SET_ARP          = 0x08
};

struct dm2_volume_info {
	uint8_t speed;
	uint8_t level;
	uint8_t sustain;
};

struct dm2_vibrato_info {
	uint8_t speed;
	uint8_t delay;
	uint8_t sustain;
};

struct dm2_block_line {
	uint8_t note;
	uint8_t instrument;
	uint8_t effect;
	uint8_t effect_arg;
};

struct dm2_block {
	struct dm2_block_line lines[DM2_BLOCK_ROWS];
};

struct dm2_track {
	uint8_t block_number;
	int8_t transpose;
};

struct dm2_track_info {
	uint16_t loop_position;
	uint16_t length;             // number of track entries
	struct dm2_track *entries;   // length entries
};

struct dm2_instrument {
	int16_t number;              // index into instruments[]
	uint8_t valid;               // non-zero when entry is populated
	uint8_t is_sample;
	uint8_t sample_number;       // 0..7 index into sampleOffsets[]
	uint16_t sample_length;
	uint16_t repeat_start;
	uint16_t repeat_length;
	struct dm2_volume_info volume_table[5];
	struct dm2_vibrato_info vibrato_table[5];
	uint16_t pitch_bend;
	uint8_t table[DM2_INSTR_TABLE_LEN];
	int8_t *sample_data;         // points into module buffer (samples) or 0
};

struct dm2_channel {
	struct dm2_instrument *instrument;
	struct dm2_track *track;
	uint16_t track_loop_position;
	uint16_t track_length;
	int16_t current_track_position;
	uint16_t next_track_position;
	uint16_t block_position;
	struct dm2_block *block;
	int8_t transpose;
	uint8_t note;
	uint16_t period;
	uint16_t final_period;
	uint8_t sound_table_delay;
	uint8_t sound_table_position;
	int16_t actual_volume;
	uint16_t volume_position;
	uint8_t volume_sustain;
	uint8_t portamento;
	int16_t pitch_bend;
	int8_t *arpeggio;
	uint16_t arpeggio_position;
	uint8_t vibrato_direction;
	uint16_t vibrato_period;
	uint8_t vibrato_delay;
	uint8_t vibrato_position;
	uint8_t vibrato_sustain;
	uint8_t max_volume;
	uint8_t retrigger_sound;
};

struct deltamusic20_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	int8_t arpeggios[DM2_NUM_ARPEGGIOS][DM2_ARPEGGIO_LEN];
	int8_t start_speed;

	struct dm2_track_info tracks[4];

	struct dm2_block *blocks;
	uint32_t num_blocks;

	struct dm2_instrument instruments[DM2_NUM_INSTR];
	uint32_t num_instruments_used;

	int8_t *waveforms_data;       // owned: noise waveform lives here, others copied/aliased
	uint32_t num_waveforms;
	int8_t **waveforms;           // array of 256-byte pointers, length num_waveforms

	uint32_t last_noise_value;
	uint8_t global_volume;        // 0..63
	int8_t play_speed;
	int8_t tick;

	struct dm2_channel channels[4];
};

// [=]===^=[ dm2_periods ]========================================================================[=]
static uint16_t dm2_periods[] = {
	   0,
	6848, 6464, 6096, 5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3616,
	3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1808,
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  904,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  452,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
	 113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113
};

#define DM2_PERIODS_LEN (sizeof(dm2_periods) / sizeof(dm2_periods[0]))

// [=]===^=[ dm2_read_u16_be ]====================================================================[=]
static uint16_t dm2_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ dm2_read_i16_be ]====================================================================[=]
static int16_t dm2_read_i16_be(uint8_t *p) {
	return (int16_t)dm2_read_u16_be(p);
}

// [=]===^=[ dm2_read_u32_be ]====================================================================[=]
static uint32_t dm2_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ dm2_rotl32 ]=========================================================================[=]
static uint32_t dm2_rotl32(uint32_t v, uint32_t n) {
	n &= 31;
	return (v << n) | (v >> (32 - n));
}

// [=]===^=[ dm2_cleanup ]========================================================================[=]
static void dm2_cleanup(struct deltamusic20_state *s) {
	if(!s) {
		return;
	}
	for(int32_t i = 0; i < 4; ++i) {
		free(s->tracks[i].entries);
		s->tracks[i].entries = 0;
	}
	free(s->blocks); s->blocks = 0;
	free(s->waveforms); s->waveforms = 0;
	free(s->waveforms_data); s->waveforms_data = 0;
}

// [=]===^=[ dm2_identify ]=======================================================================[=]
static int32_t dm2_identify(uint8_t *data, uint32_t len) {
	if(len < 0xfda) {
		return 0;
	}
	if(data[0xbc6] != '.' || data[0xbc7] != 'F' || data[0xbc8] != 'N' || data[0xbc9] != 'L') {
		return 0;
	}

	uint64_t total_length = 0xfda;
	uint32_t pos = 0xfca;

	uint64_t track_bytes = 0;
	for(int32_t i = 0; i < 4; ++i) {
		if((uint64_t)pos + 4 > len) {
			return 0;
		}
		pos += 2; // loop_position
		track_bytes += dm2_read_u16_be(data + pos);
		pos += 2;
	}
	total_length += track_bytes;

	uint64_t block_pos = pos + track_bytes;
	if(block_pos + 4 > len) {
		return 0;
	}
	uint32_t block_length = dm2_read_u32_be(data + block_pos);
	total_length += (uint64_t)block_length + 4;

	uint64_t instr_pos = block_pos + 4 + block_length + 256 - 2;
	if(instr_pos + 2 > len) {
		return 0;
	}
	uint32_t instr_length = dm2_read_u16_be(data + instr_pos);
	total_length += (uint64_t)instr_length + 256;

	uint64_t wave_pos = instr_pos + 2 + instr_length;
	if(wave_pos + 4 > len) {
		return 0;
	}
	uint32_t wave_length = dm2_read_u32_be(data + wave_pos);
	total_length += (uint64_t)wave_length + 4 + 64;

	if(total_length > len) {
		return 0;
	}
	return 1;
}

// [=]===^=[ dm2_load ]===========================================================================[=]
static int32_t dm2_load(struct deltamusic20_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	// Start speed
	s->start_speed = (int8_t)data[0xbbb];

	// Arpeggios
	uint32_t pos = 0xbca;
	if(pos + DM2_NUM_ARPEGGIOS * DM2_ARPEGGIO_LEN > len) {
		return 0;
	}
	for(int32_t i = 0; i < DM2_NUM_ARPEGGIOS; ++i) {
		for(int32_t j = 0; j < DM2_ARPEGGIO_LEN; ++j) {
			s->arpeggios[i][j] = (int8_t)data[pos++];
		}
	}

	// Track headers
	for(int32_t i = 0; i < 4; ++i) {
		if(pos + 4 > len) {
			return 0;
		}
		s->tracks[i].loop_position = dm2_read_u16_be(data + pos); pos += 2;
		s->tracks[i].length = (uint16_t)(dm2_read_u16_be(data + pos) / 2); pos += 2;
		s->tracks[i].entries = 0;
	}

	// Track entries
	for(int32_t i = 0; i < 4; ++i) {
		uint32_t need = (uint32_t)s->tracks[i].length * 2;
		if(pos + need > len) {
			return 0;
		}
		s->tracks[i].entries = (struct dm2_track *)calloc((size_t)s->tracks[i].length + 1, sizeof(struct dm2_track));
		if(!s->tracks[i].entries) {
			return 0;
		}
		for(uint32_t j = 0; j < s->tracks[i].length; ++j) {
			s->tracks[i].entries[j].block_number = data[pos++];
			s->tracks[i].entries[j].transpose = (int8_t)data[pos++];
		}
	}

	// Blocks
	if(pos + 4 > len) {
		return 0;
	}
	uint32_t block_bytes = dm2_read_u32_be(data + pos); pos += 4;
	s->num_blocks = block_bytes / 64;
	if(pos + (uint64_t)s->num_blocks * 64 > len) {
		return 0;
	}
	s->blocks = (struct dm2_block *)calloc((size_t)s->num_blocks + 1, sizeof(struct dm2_block));
	if(!s->blocks) {
		return 0;
	}
	for(uint32_t i = 0; i < s->num_blocks; ++i) {
		for(uint32_t j = 0; j < DM2_BLOCK_ROWS; ++j) {
			s->blocks[i].lines[j].note       = data[pos++];
			s->blocks[i].lines[j].instrument = data[pos++];
			s->blocks[i].lines[j].effect     = data[pos++];
			s->blocks[i].lines[j].effect_arg = data[pos++];
		}
	}

	// Instrument offsets: u16[128] where index 0 is unused, indices 1..127 are
	// real offsets, then a final breakOffset terminator. We read 256 bytes and
	// consume them as: offsets[1..127] = data[pos+0..253], breakOffset = data[pos+254..255].
	if(pos + 256 > len) {
		return 0;
	}
	uint16_t instr_offsets[DM2_NUM_INSTR];
	instr_offsets[0] = 0;
	for(int32_t i = 0; i < 127; ++i) {
		instr_offsets[i + 1] = dm2_read_u16_be(data + pos + i * 2);
	}
	uint16_t break_offset = dm2_read_u16_be(data + pos + 254);
	pos += 256;
	uint32_t instr_data_start = pos;

	memset(s->instruments, 0, sizeof(s->instruments));
	s->num_instruments_used = 0;

	for(int32_t i = 0; i < DM2_NUM_INSTR; ++i) {
		if(instr_offsets[i] == break_offset) {
			break;
		}

		uint32_t ip = instr_data_start + (uint32_t)instr_offsets[i];
		if(ip + 6 + 5 * 3 + 5 * 3 + 4 + DM2_INSTR_TABLE_LEN > len) {
			return 0;
		}

		struct dm2_instrument *inst = &s->instruments[i];
		inst->valid = 1;
		inst->number = (int16_t)i;
		inst->sample_length = (uint16_t)(dm2_read_u16_be(data + ip) * 2); ip += 2;
		inst->repeat_start  = dm2_read_u16_be(data + ip); ip += 2;
		inst->repeat_length = (uint16_t)(dm2_read_u16_be(data + ip) * 2); ip += 2;

		if(inst->repeat_start + inst->repeat_length >= inst->sample_length) {
			inst->repeat_length = (uint16_t)(inst->sample_length - inst->repeat_start);
		}

		for(int32_t j = 0; j < 5; ++j) {
			inst->volume_table[j].speed   = data[ip++];
			inst->volume_table[j].level   = data[ip++];
			inst->volume_table[j].sustain = data[ip++];
		}
		for(int32_t j = 0; j < 5; ++j) {
			inst->vibrato_table[j].speed   = data[ip++];
			inst->vibrato_table[j].delay   = data[ip++];
			inst->vibrato_table[j].sustain = data[ip++];
		}

		inst->pitch_bend = dm2_read_u16_be(data + ip); ip += 2;
		inst->is_sample = (data[ip] == 0xff) ? 1 : 0; ip += 1;
		inst->sample_number = (uint8_t)(data[ip] & 0x07); ip += 1;

		memcpy(inst->table, data + ip, DM2_INSTR_TABLE_LEN);
		ip += DM2_INSTR_TABLE_LEN;

		s->num_instruments_used = (uint32_t)(i + 1);
	}

	// The 128th u16 (break_offset) doubles as the total size of the instrument data block.
	if((uint64_t)instr_data_start + (uint64_t)break_offset > len) {
		return 0;
	}
	pos = instr_data_start + (uint32_t)break_offset;

	// Waveforms
	if(pos + 4 > len) {
		return 0;
	}
	uint32_t wave_bytes = dm2_read_u32_be(data + pos); pos += 4;
	s->num_waveforms = wave_bytes / DM2_WAVEFORM_LEN;
	if(pos + (uint64_t)s->num_waveforms * DM2_WAVEFORM_LEN > len) {
		return 0;
	}
	if(s->num_waveforms == 0) {
		return 0;
	}
	// Waveform 0 is the noise generator slot, regenerated each tick.
	s->waveforms_data = (int8_t *)calloc((size_t)s->num_waveforms * DM2_WAVEFORM_LEN, 1);
	if(!s->waveforms_data) {
		return 0;
	}
	memcpy(s->waveforms_data, data + pos, (size_t)s->num_waveforms * DM2_WAVEFORM_LEN);
	s->waveforms = (int8_t **)calloc((size_t)s->num_waveforms, sizeof(int8_t *));
	if(!s->waveforms) {
		return 0;
	}
	for(uint32_t i = 0; i < s->num_waveforms; ++i) {
		s->waveforms[i] = s->waveforms_data + i * DM2_WAVEFORM_LEN;
	}
	pos += (uint32_t)(s->num_waveforms * DM2_WAVEFORM_LEN);

	// Skip 64 bytes
	if(pos + 64 > len) {
		return 0;
	}
	pos += 64;

	// Sample offsets: u32[8]
	if(pos + DM2_NUM_SAMPLE_SLOTS * 4 > len) {
		return 0;
	}
	uint32_t sample_offsets[DM2_NUM_SAMPLE_SLOTS];
	for(int32_t i = 0; i < DM2_NUM_SAMPLE_SLOTS; ++i) {
		sample_offsets[i] = dm2_read_u32_be(data + pos); pos += 4;
	}
	uint32_t sample_data_start = pos;

	for(int32_t i = 0; i < DM2_NUM_INSTR; ++i) {
		struct dm2_instrument *inst = &s->instruments[i];
		if(!inst->valid || !inst->is_sample) {
			continue;
		}
		uint32_t off = sample_data_start + sample_offsets[inst->sample_number];
		if(off >= len) {
			return 0;
		}
		uint32_t avail = len - off;
		// The C# loader tolerates the last 256 bytes being missing; clamp length to whatever exists.
		uint32_t take = inst->sample_length;
		if(take > avail) {
			if(take > avail + 256) {
				return 0;
			}
			take = avail;
		}
		inst->sample_data = (int8_t *)(data + off);
		// Reflect the actual playable length so the Paula mixer never reads past EOF.
		inst->sample_length = (uint16_t)take;
		if(inst->repeat_start + inst->repeat_length > inst->sample_length) {
			if(inst->repeat_start >= inst->sample_length) {
				inst->repeat_start = 0;
				inst->repeat_length = 0;
			} else {
				inst->repeat_length = (uint16_t)(inst->sample_length - inst->repeat_start);
			}
		}
	}

	return 1;
}

// [=]===^=[ dm2_initialize_sound ]===============================================================[=]
static void dm2_initialize_sound(struct deltamusic20_state *s) {
	s->global_volume = 63;
	s->play_speed = s->start_speed;
	s->tick = 1;
	s->last_noise_value = 0;

	for(int32_t i = 0; i < 4; ++i) {
		struct dm2_channel *c = &s->channels[i];
		memset(c, 0, sizeof(*c));
		c->track = s->tracks[i].entries;
		c->track_loop_position = s->tracks[i].loop_position;
		c->track_length = s->tracks[i].length;
		c->block_position = 0;
		c->current_track_position = -1;
		c->next_track_position = 0;
		c->instrument = 0;
		c->arpeggio_position = 0;
		c->arpeggio = s->arpeggios[0];
		c->actual_volume = 0;
		c->volume_position = 0;
		c->volume_sustain = 0;
		c->portamento = 0;
		c->pitch_bend = 0;
		c->max_volume = 63;
		c->retrigger_sound = 0;
	}
}

// [=]===^=[ dm2_generate_noise ]=================================================================[=]
// Refresh the contents of waveforms[0] (the dedicated noise slot).
static void dm2_generate_noise(struct deltamusic20_state *s) {
	if(s->num_waveforms == 0) {
		return;
	}
	uint32_t noise_value = s->last_noise_value;
	uint32_t *wf = (uint32_t *)s->waveforms[0];
	// 256 bytes / 4 = 64 u32, but the C# version writes only 16 entries. Match that.
	for(int32_t i = 0; i < 16; ++i) {
		noise_value = dm2_rotl32(noise_value, 7);
		noise_value += 0x6eca756du;
		noise_value ^= 0x9e59a92bu;
		wf[i] = noise_value;
	}
	s->last_noise_value = noise_value;
}

// [=]===^=[ dm2_parse_effect ]===================================================================[=]
static void dm2_parse_effect(struct deltamusic20_state *s, struct dm2_channel *c, uint8_t effect, uint8_t arg) {
	switch(effect) {
		case DM2_EFFECT_SET_SPEED: {
			int8_t new_speed = (int8_t)(arg & 0x0f);
			if(new_speed != s->play_speed) {
				s->play_speed = new_speed;
			}
			break;
		}

		case DM2_EFFECT_SET_FILTER: {
			// Amiga filter toggle, no audible effect here.
			break;
		}

		case DM2_EFFECT_SET_BEND_RATE_UP: {
			c->pitch_bend = (int16_t)-(int16_t)arg;
			break;
		}

		case DM2_EFFECT_SET_BEND_RATE_DN: {
			c->pitch_bend = (int16_t)arg;
			break;
		}

		case DM2_EFFECT_SET_PORTAMENTO: {
			c->portamento = arg;
			break;
		}

		case DM2_EFFECT_SET_VOLUME: {
			c->max_volume = (uint8_t)(arg & 0x3f);
			break;
		}

		case DM2_EFFECT_SET_GLOBAL_VOL: {
			s->global_volume = (uint8_t)(arg & 0x3f);
			break;
		}

		case DM2_EFFECT_SET_ARP: {
			c->arpeggio = s->arpeggios[arg & 0x3f];
			break;
		}

		default: break;
	}
}

// [=]===^=[ dm2_sound_table_handler ]============================================================[=]
static void dm2_sound_table_handler(struct deltamusic20_state *s, int32_t chan, struct dm2_channel *c, struct dm2_instrument *inst) {
	if(inst->is_sample) {
		return;
	}
	if(c->sound_table_delay != 0) {
		c->sound_table_delay--;
		return;
	}

	c->sound_table_delay = inst->sample_number;

	uint8_t entry = inst->table[c->sound_table_position];
	if(entry == 0xff) {
		uint8_t new_pos = inst->table[c->sound_table_position + 1];
		if(new_pos >= DM2_INSTR_TABLE_LEN) {
			new_pos = 0;
		}
		c->sound_table_position = new_pos;
		entry = inst->table[c->sound_table_position];
		if(entry == 0xff) {
			return;
		}
	}

	if(entry >= s->num_waveforms) {
		entry = 0;
	}
	int8_t *wave = s->waveforms[entry];

	if(c->retrigger_sound) {
		if(inst->sample_length > 0) {
			paula_play_sample(&s->paula, chan, wave, inst->sample_length);
			paula_set_loop(&s->paula, chan, 0, inst->sample_length);
		} else {
			paula_mute(&s->paula, chan);
		}
		c->retrigger_sound = 0;
	} else {
		if(inst->sample_length > 0) {
			paula_queue_sample(&s->paula, chan, wave, 0, inst->sample_length);
			paula_set_loop(&s->paula, chan, 0, inst->sample_length);
		}
	}

	c->sound_table_position++;
	if(c->sound_table_position >= DM2_INSTR_TABLE_LEN) {
		c->sound_table_position = 0;
	}
}

// [=]===^=[ dm2_vibrato_handler ]================================================================[=]
static void dm2_vibrato_handler(struct dm2_channel *c, struct dm2_instrument *inst) {
	struct dm2_vibrato_info *info = &inst->vibrato_table[c->vibrato_position];

	if(c->vibrato_direction) {
		c->vibrato_period = (uint16_t)(c->vibrato_period - info->speed);
	} else {
		c->vibrato_period = (uint16_t)(c->vibrato_period + info->speed);
	}

	c->vibrato_delay--;
	if(c->vibrato_delay == 0) {
		c->vibrato_delay = info->delay;
		c->vibrato_direction = (uint8_t)(c->vibrato_direction ? 0 : 1);
	}

	if(c->vibrato_sustain != 0) {
		c->vibrato_sustain--;
	} else {
		c->vibrato_position++;
		if(c->vibrato_position == 5) {
			c->vibrato_position = 4;
		}
		c->vibrato_sustain = inst->vibrato_table[c->vibrato_position].sustain;
	}
}

// [=]===^=[ dm2_volume_handler ]=================================================================[=]
static void dm2_volume_handler(struct dm2_channel *c, struct dm2_instrument *inst) {
	if(c->volume_sustain != 0) {
		c->volume_sustain--;
		return;
	}

	struct dm2_volume_info *info = &inst->volume_table[c->volume_position];

	if(c->actual_volume >= (int16_t)info->level) {
		c->actual_volume = (int16_t)(c->actual_volume - (int16_t)info->speed);
		if(c->actual_volume < (int16_t)info->level) {
			c->actual_volume = (int16_t)info->level;
			c->volume_position++;
			if(c->volume_position == 5) {
				c->volume_position = 4;
			}
			c->volume_sustain = info->sustain;
		}
	} else {
		c->actual_volume = (int16_t)(c->actual_volume + (int16_t)info->speed);
		if(c->actual_volume > (int16_t)info->level) {
			c->actual_volume = (int16_t)info->level;
			c->volume_position++;
			if(c->volume_position == 5) {
				c->volume_position = 4;
			}
			c->volume_sustain = info->sustain;
		}
	}
}

// [=]===^=[ dm2_portamento_handler ]=============================================================[=]
static void dm2_portamento_handler(struct dm2_channel *c) {
	if(c->portamento == 0) {
		return;
	}
	if(c->final_period >= c->period) {
		c->final_period = (uint16_t)(c->final_period - c->portamento);
		if(c->final_period < c->period) {
			c->final_period = c->period;
		}
	} else {
		c->final_period = (uint16_t)(c->final_period + c->portamento);
		if(c->final_period > c->period) {
			c->final_period = c->period;
		}
	}
}

// [=]===^=[ dm2_arpeggio_handler ]===============================================================[=]
static void dm2_arpeggio_handler(struct dm2_channel *c) {
	int8_t arp = c->arpeggio[c->arpeggio_position];

	if((c->arpeggio_position != 0) && (arp == -128)) {
		c->arpeggio_position = 0;
		arp = c->arpeggio[0];
	}

	c->arpeggio_position++;
	c->arpeggio_position &= 0x0f;

	if(c->portamento == 0) {
		uint8_t index = (uint8_t)((int32_t)arp + (int32_t)c->note + (int32_t)c->transpose);
		if(index >= DM2_PERIODS_LEN) {
			index = (uint8_t)(DM2_PERIODS_LEN - 1);
		}
		c->final_period = dm2_periods[index];
	}
}

// [=]===^=[ dm2_process_channel ]================================================================[=]
static void dm2_process_channel(struct deltamusic20_state *s, int32_t chan_index) {
	struct dm2_channel *c = &s->channels[chan_index];

	if(c->track_length == 0) {
		return;
	}

	struct dm2_instrument *inst = c->instrument;

	if(s->tick == 0) {
		if(c->block_position == 0) {
			struct dm2_track *t = &c->track[c->next_track_position];
			c->transpose = t->transpose;
			uint8_t bn = t->block_number;
			if(bn >= s->num_blocks) {
				bn = 0;
			}
			c->block = &s->blocks[bn];

			c->current_track_position = (int16_t)c->next_track_position;

			c->next_track_position++;
			if(c->next_track_position >= c->track_length) {
				c->next_track_position = (c->track_loop_position >= c->track_length) ? 0 : c->track_loop_position;
			}
		}

		struct dm2_block_line *line = &c->block->lines[c->block_position];

		if(line->note != 0) {
			c->note = line->note;
			int32_t period_index = (int32_t)c->note + (int32_t)c->transpose;
			if(period_index < 0) {
				period_index = 0;
			} else if((uint32_t)period_index >= DM2_PERIODS_LEN) {
				period_index = (int32_t)(DM2_PERIODS_LEN - 1);
			}
			c->period = dm2_periods[period_index];

			if(line->instrument < DM2_NUM_INSTR && s->instruments[line->instrument].valid) {
				inst = c->instrument = &s->instruments[line->instrument];

				if(inst->is_sample) {
					if(inst->sample_data && inst->sample_length > 0) {
						paula_play_sample(&s->paula, chan_index, inst->sample_data, inst->sample_length);
						if(inst->repeat_length > 1) {
							paula_set_loop(&s->paula, chan_index, inst->repeat_start, inst->repeat_length);
						}
					} else {
						paula_mute(&s->paula, chan_index);
					}
				} else {
					c->retrigger_sound = 1;
				}

				c->sound_table_delay = 0;
				c->sound_table_position = 0;
				c->actual_volume = 0;
				c->volume_sustain = 0;
				c->volume_position = 0;
				c->arpeggio_position = 0;
				c->vibrato_direction = 0;
				c->vibrato_period = 0;
				c->vibrato_delay = inst->vibrato_table[0].delay;
				c->vibrato_position = 0;
				c->vibrato_sustain = inst->vibrato_table[0].sustain;
			} else {
				inst = c->instrument = 0;
				paula_mute(&s->paula, chan_index);
			}
		}

		dm2_parse_effect(s, c, line->effect, line->effect_arg);

		c->block_position++;
		c->block_position &= 0x0f;
	}

	if(inst != 0) {
		dm2_sound_table_handler(s, chan_index, c, inst);
		dm2_vibrato_handler(c, inst);
		dm2_volume_handler(c, inst);
		dm2_portamento_handler(c);
		dm2_arpeggio_handler(c);

		c->vibrato_period = (uint16_t)(c->vibrato_period - (uint16_t)((int32_t)inst->pitch_bend - (int32_t)c->pitch_bend));

		uint16_t new_period = (uint16_t)(c->final_period + c->vibrato_period);
		paula_set_period(&s->paula, chan_index, new_period);

		uint8_t new_volume = (uint8_t)((c->actual_volume >> 2) & 0x3f);
		if(new_volume > c->max_volume) {
			new_volume = c->max_volume;
		}
		if(new_volume > s->global_volume) {
			new_volume = s->global_volume;
		}
		// Delta Music caps at 63; Paula expects 0..64.
		paula_set_volume(&s->paula, chan_index, new_volume);
	}
}

// [=]===^=[ dm2_tick ]===========================================================================[=]
static void dm2_tick(struct deltamusic20_state *s) {
	dm2_generate_noise(s);

	s->tick--;
	if(s->tick < 0) {
		s->tick = s->play_speed;
	}

	for(int32_t i = 0; i < 4; ++i) {
		dm2_process_channel(s, i);
	}
}

// [=]===^=[ deltamusic20_init ]==================================================================[=]
static struct deltamusic20_state *deltamusic20_init(void *data, uint32_t len, int32_t sample_rate) {
	struct deltamusic20_state *s;

	if(!data || len < 0xfda || sample_rate < 8000) {
		return 0;
	}
	if(!dm2_identify((uint8_t *)data, len)) {
		return 0;
	}

	s = (struct deltamusic20_state *)calloc(1, sizeof(struct deltamusic20_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;

	if(!dm2_load(s)) {
		goto fail;
	}

	paula_init(&s->paula, sample_rate, DM2_TICK_HZ);
	dm2_initialize_sound(s);
	return s;

fail:
	dm2_cleanup(s);
	free(s);
	return 0;
}

// [=]===^=[ deltamusic20_free ]==================================================================[=]
static void deltamusic20_free(struct deltamusic20_state *s) {
	if(!s) {
		return;
	}
	dm2_cleanup(s);
	free(s);
}

// [=]===^=[ deltamusic20_get_audio ]==============================================================[=]
static void deltamusic20_get_audio(struct deltamusic20_state *s, int16_t *output, int32_t frames) {
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
			dm2_tick(s);
		}
	}
}

// [=]===^=[ deltamusic20_api_init ]==============================================================[=]
static void *deltamusic20_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return deltamusic20_init(data, len, sample_rate);
}

// [=]===^=[ deltamusic20_api_free ]==============================================================[=]
static void deltamusic20_api_free(void *state) {
	deltamusic20_free((struct deltamusic20_state *)state);
}

// [=]===^=[ deltamusic20_api_get_audio ]=========================================================[=]
static void deltamusic20_api_get_audio(void *state, int16_t *output, int32_t frames) {
	deltamusic20_get_audio((struct deltamusic20_state *)state, output, frames);
}

static const char *deltamusic20_extensions[] = { "dm2", 0 };

static struct player_api deltamusic20_api = {
	"Delta Music 2.0",
	deltamusic20_extensions,
	deltamusic20_api_init,
	deltamusic20_api_free,
	deltamusic20_api_get_audio,
	0,
};
