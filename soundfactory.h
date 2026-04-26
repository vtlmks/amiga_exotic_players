// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Sound Factory (.psf) replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz PAL tick rate.
//
// Public API:
//   struct soundfactory_state *soundfactory_init(void *data, uint32_t len, int32_t sample_rate);
//   void soundfactory_free(struct soundfactory_state *s);
//   void soundfactory_get_audio(struct soundfactory_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define SF_TICK_HZ         50
#define SF_MAX_SUBSONGS    16
#define SF_SOUND_TABLE_LEN 32
#define SF_STACK_DEPTH     64
#define SF_PHASING_BUF_LEN 256
#define SF_MAX_INSTRUMENTS 256

// InstrumentFlag bit values
#define SF_IF_ONESHOT    0x01
#define SF_IF_VIBRATO    0x02
#define SF_IF_ARPEGGIO   0x04
#define SF_IF_PHASING    0x08
#define SF_IF_PORTAMENTO 0x10
#define SF_IF_RELEASE    0x20
#define SF_IF_TREMOLO    0x40
#define SF_IF_FILTER     0x80

// Opcode values
#define SF_OP_PAUSE             0x80
#define SF_OP_SET_VOLUME        0x81
#define SF_OP_SET_FINETUNE      0x82
#define SF_OP_USE_INSTRUMENT    0x83
#define SF_OP_DEFINE_INSTRUMENT 0x84
#define SF_OP_RETURN            0x85
#define SF_OP_GOSUB             0x86
#define SF_OP_GOTO              0x87
#define SF_OP_FOR               0x88
#define SF_OP_NEXT              0x89
#define SF_OP_FADE_OUT          0x8a
#define SF_OP_NOP               0x8b
#define SF_OP_REQUEST           0x8c
#define SF_OP_LOOP              0x8d
#define SF_OP_END               0x8e
#define SF_OP_FADE_IN           0x8f
#define SF_OP_SET_ADSR          0x90
#define SF_OP_ONESHOT           0x91
#define SF_OP_LOOPING           0x92
#define SF_OP_VIBRATO           0x93
#define SF_OP_ARPEGGIO          0x94
#define SF_OP_PHASING           0x95
#define SF_OP_PORTAMENTO        0x96
#define SF_OP_TREMOLO           0x97
#define SF_OP_FILTER            0x98
#define SF_OP_STOP_AND_PAUSE    0x99
#define SF_OP_LED               0x9a
#define SF_OP_WAIT_FOR_REQUEST  0x9b
#define SF_OP_SET_TRANSPOSE     0x9c

// Envelope states
#define SF_ENV_ATTACK  0
#define SF_ENV_DECAY   1
#define SF_ENV_SUSTAIN 2
#define SF_ENV_RELEASE 3

struct sf_instrument {
	uint32_t opcode_offset;      // offset into opcodes where the instrument data begins (key)
	uint16_t sample_length;      // words
	uint16_t sampling_period;
	uint8_t effect_byte;         // SF_IF_*

	uint8_t tremolo_speed;
	uint8_t tremolo_step;
	uint8_t tremolo_range;

	uint16_t portamento_step;
	uint8_t portamento_speed;

	uint8_t arpeggio_speed;

	uint8_t vibrato_delay;
	uint8_t vibrato_speed;
	int8_t vibrato_step;
	uint8_t vibrato_amount;

	uint8_t attack_time;
	uint8_t decay_time;
	uint8_t sustain_level;
	uint8_t release_time;

	uint8_t phasing_start;
	uint8_t phasing_end;
	uint8_t phasing_speed;
	int8_t phasing_step;

	uint8_t wave_count;
	uint8_t octave;

	uint8_t filter_frequency;
	uint8_t filter_end;
	uint8_t filter_speed;

	uint16_t dasr_sustain_offset;
	uint16_t dasr_release_offset;

	int8_t *sample_data;         // pointer into opcode block; caller owns the buffer
	int16_t instrument_number;
};

struct sf_songinfo {
	uint8_t enabled_channels;
	uint32_t opcode_start_offsets[4];
};

struct sf_voice {
	int32_t channel_number;
	uint8_t voice_enabled;

	uint32_t start_position;
	uint32_t current_position;

	uint8_t current_instrument;

	uint16_t note_duration;
	uint16_t note_duration2;
	uint8_t note;
	int8_t transpose;

	uint8_t fine_tune;
	uint16_t period;

	uint8_t current_volume;
	uint8_t volume;

	uint16_t active_period;
	uint8_t portamento_counter;

	uint8_t arpeggio_flag;
	uint8_t arpeggio_counter;

	uint8_t vibrato_delay;
	uint8_t vibrato_counter;
	uint8_t vibrato_counter2;
	int16_t vibrato_relative;
	int8_t vibrato_step;

	uint8_t tremolo_counter;
	int8_t tremolo_step;
	uint8_t tremolo_volume;

	uint8_t envelope_state;
	uint8_t envelope_counter;

	uint8_t phasing_counter;
	int8_t phasing_step;
	int8_t phasing_relative;

	uint8_t filter_counter;
	int8_t filter_step;
	uint8_t filter_relative;

	uint32_t stack[SF_STACK_DEPTH];
	int32_t stack_top;

	uint8_t note_start_flag;
	uint8_t note_start_flag1;

	int8_t phasing_buffer[SF_PHASING_BUF_LEN];

	// Cached pointer to the sample currently assigned to the Paula channel.
	// When phasing/filter modify phasing_buffer, Paula already has its address,
	// so a mid-note re-render is picked up on the next loop wrap.
	int8_t *active_sample_data;
};

struct soundfactory_state {
	struct paula paula;

	uint8_t *module_data;                  // caller-owned
	uint32_t module_len;

	uint8_t *opcodes;                      // module_data + 276 (alias, not owned)
	uint32_t opcodes_len;

	struct sf_songinfo songs[SF_MAX_SUBSONGS];
	int32_t num_songs;

	struct sf_instrument *instruments;     // original template instruments, sorted by offset
	int32_t num_instruments;

	struct sf_instrument default_instrument;

	// Per-song runtime state
	struct sf_instrument *instrument_lookup;       // copies of original instruments (sound table references these by index+1, 0 = default)
	int32_t *sound_table;                          // 32 entries, each is an index; -1 means default_instrument
	struct sf_instrument current_instruments[SF_MAX_INSTRUMENTS];

	uint8_t fade_out_flag;
	uint8_t fade_in_flag;
	uint8_t fade_out_volume;
	uint8_t fade_out_counter;
	uint8_t fade_out_speed;

	uint8_t request_counter;

	uint8_t amiga_filter;

	struct sf_voice voices[4];

	int32_t cur_subsong;
};

// [=]===^=[ sf_multiply_table ]==================================================================[=]
static uint16_t sf_multiply_table[12] = {
	32768, 30929, 29193, 27555, 26008, 24549,
	23171, 21870, 20643, 19484, 18391, 17359
};

// [=]===^=[ sf_sample_table ]====================================================================[=]
static uint16_t sf_sample_table[12] = {
	54728, 51656, 48757, 46020, 43437, 40999,
	38698, 36526, 34476, 32541, 30715, 28964
};

// [=]===^=[ sf_default_instrument_sample ]=======================================================[=]
static int8_t sf_default_instrument_sample[2] = { 100, -100 };

// [=]===^=[ sf_read_u32_be ]=====================================================================[=]
static uint32_t sf_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ sf_read_u16_be ]=====================================================================[=]
static uint16_t sf_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ sf_fetch_word_at ]===================================================================[=]
static uint16_t sf_fetch_word_at(uint8_t *opcodes, uint32_t *offset) {
	uint16_t v = (uint16_t)(((uint16_t)opcodes[*offset] << 8) | (uint16_t)opcodes[*offset + 1]);
	*offset += 2;
	return v;
}

// [=]===^=[ sf_fetch_long_at ]===================================================================[=]
static int32_t sf_fetch_long_at(uint8_t *opcodes, uint32_t *offset) {
	int32_t v = ((int32_t)(int8_t)opcodes[*offset] << 24)
		| ((int32_t)opcodes[*offset + 1] << 16)
		| ((int32_t)opcodes[*offset + 2] << 8)
		| (int32_t)opcodes[*offset + 3];
	*offset += 4;
	return v;
}

// [=]===^=[ sf_identify ]========================================================================[=]
static int32_t sf_identify(uint8_t *data, uint32_t len) {
	if(len < 276) {
		return 0;
	}
	uint32_t module_length = sf_read_u32_be(data);
	if(module_length > len) {
		return 0;
	}
	for(int32_t i = 0; i < 16; ++i) {
		if(data[4 + i] > 15) {
			return 0;
		}
	}
	uint32_t min_offset = 0xffffffff;
	for(int32_t i = 0; i < 4 * 16; ++i) {
		uint32_t off = sf_read_u32_be(data + 20 + i * 4);
		if(off > len) {
			return 0;
		}
		if(off < min_offset) {
			min_offset = off;
		}
	}
	if(min_offset != 276) {
		return 0;
	}
	return 1;
}

// [=]===^=[ sf_init_default_instrument ]=========================================================[=]
static void sf_init_default_instrument(struct sf_instrument *ins) {
	memset(ins, 0, sizeof(*ins));
	ins->sample_length = 1;
	ins->attack_time = 1;
	ins->sustain_level = 64;
	ins->release_time = 30;
	ins->wave_count = 1;
	ins->filter_frequency = 1;
	ins->filter_end = 50;
	ins->filter_speed = 2;
	ins->sample_data = sf_default_instrument_sample;
	ins->instrument_number = -1;
}

// [=]===^=[ sf_fetch_instrument ]================================================================[=]
// Parses instrument data stored at `offset` in the opcode block into `ins`.
// sample_data is taken as an alias into the caller-owned module buffer.
static void sf_fetch_instrument(uint8_t *opcodes, uint32_t offset, struct sf_instrument *ins) {
	memset(ins, 0, sizeof(*ins));
	ins->opcode_offset = offset;
	ins->sample_length = sf_fetch_word_at(opcodes, &offset);
	ins->sampling_period = sf_fetch_word_at(opcodes, &offset);

	ins->effect_byte = opcodes[offset++];

	ins->tremolo_speed = opcodes[offset++];
	ins->tremolo_step = opcodes[offset++];
	ins->tremolo_range = opcodes[offset++];

	ins->portamento_step = sf_fetch_word_at(opcodes, &offset);
	ins->portamento_speed = opcodes[offset++];

	ins->arpeggio_speed = opcodes[offset++];

	ins->vibrato_delay = opcodes[offset++];
	ins->vibrato_speed = opcodes[offset++];
	ins->vibrato_step = (int8_t)opcodes[offset++];
	ins->vibrato_amount = opcodes[offset++];

	ins->attack_time = opcodes[offset++];
	ins->decay_time = opcodes[offset++];
	ins->sustain_level = opcodes[offset++];
	ins->release_time = opcodes[offset++];

	ins->phasing_start = opcodes[offset++];
	ins->phasing_end = opcodes[offset++];
	ins->phasing_speed = opcodes[offset++];
	ins->phasing_step = (int8_t)opcodes[offset++];

	ins->wave_count = opcodes[offset++];
	ins->octave = opcodes[offset++];

	ins->filter_frequency = opcodes[offset++];
	ins->filter_end = opcodes[offset++];
	ins->filter_speed = opcodes[offset++];
	offset++;

	ins->dasr_sustain_offset = sf_fetch_word_at(opcodes, &offset);
	ins->dasr_release_offset = sf_fetch_word_at(opcodes, &offset);

	ins->sample_data = (int8_t *)(opcodes + offset);
	ins->instrument_number = 0;
}

// [=]===^=[ sf_find_instrument_index ]===========================================================[=]
// Binary search for the instrument whose opcode offset matches `offset`.
// Returns index or -1.
static int32_t sf_find_instrument_index(struct soundfactory_state *s, uint32_t offset) {
	int32_t lo = 0;
	int32_t hi = s->num_instruments - 1;
	while(lo <= hi) {
		int32_t mid = (lo + hi) >> 1;
		uint32_t mof = s->instruments[mid].opcode_offset;
		if(mof == offset) {
			return mid;
		}
		if(mof < offset) {
			lo = mid + 1;
		} else {
			hi = mid - 1;
		}
	}
	return -1;
}

// [=]===^=[ sf_insert_instrument ]===============================================================[=]
// Inserts an instrument keyed by opcode offset, maintaining sorted order.
// Ignores duplicates (first discovery wins, matching SortedDictionary<> behavior).
static int32_t sf_insert_instrument(struct soundfactory_state *s, struct sf_instrument *ins) {
	int32_t lo = 0;
	int32_t hi = s->num_instruments;
	while(lo < hi) {
		int32_t mid = (lo + hi) >> 1;
		if(s->instruments[mid].opcode_offset < ins->opcode_offset) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	if(lo < s->num_instruments && s->instruments[lo].opcode_offset == ins->opcode_offset) {
		s->instruments[lo] = *ins;
		return 0;
	}
	struct sf_instrument *nbuf = (struct sf_instrument *)realloc(s->instruments, (size_t)(s->num_instruments + 1) * sizeof(struct sf_instrument));
	if(!nbuf) {
		return 0;
	}
	s->instruments = nbuf;
	if(lo < s->num_instruments) {
		memmove(&s->instruments[lo + 1], &s->instruments[lo], (size_t)(s->num_instruments - lo) * sizeof(struct sf_instrument));
	}
	s->instruments[lo] = *ins;
	s->num_instruments++;
	return 1;
}

// [=]===^=[ sf_taken_contains ]==================================================================[=]
static int32_t sf_taken_contains(uint8_t *taken, uint32_t offset, uint32_t len) {
	if(offset >= len) {
		return 1;
	}
	return taken[offset] ? 1 : 0;
}

// [=]===^=[ sf_find_samples_in_list ]=============================================================[=]
// Scans the opcode stream starting at `offset`, collecting all DefineInstrument
// blocks into s->instruments. `taken` marks visited offsets so cycles terminate.
static void sf_find_samples_in_list(struct soundfactory_state *s, uint32_t offset, uint8_t *taken) {
	uint32_t len = s->opcodes_len;
	uint8_t *opcodes = s->opcodes;
	int32_t stop = 0;

	while(!stop) {
		if(offset >= len) {
			return;
		}
		taken[offset] = 1;

		uint8_t opcode = opcodes[offset++];
		uint32_t bytes_to_skip;

		switch(opcode) {
			case SF_OP_NEXT:
			case SF_OP_NOP:
			case SF_OP_REQUEST:
			case SF_OP_ONESHOT:
			case SF_OP_LOOPING: {
				bytes_to_skip = 0;
				break;
			}

			case SF_OP_SET_VOLUME:
			case SF_OP_SET_FINETUNE:
			case SF_OP_USE_INSTRUMENT:
			case SF_OP_FOR:
			case SF_OP_FADE_OUT:
			case SF_OP_FADE_IN:
			case SF_OP_LED:
			case SF_OP_WAIT_FOR_REQUEST:
			case SF_OP_SET_TRANSPOSE: {
				bytes_to_skip = 1;
				break;
			}

			case SF_OP_PAUSE:
			case SF_OP_STOP_AND_PAUSE: {
				bytes_to_skip = 2;
				break;
			}

			case SF_OP_PORTAMENTO:
			case SF_OP_TREMOLO:
			case SF_OP_FILTER: {
				bytes_to_skip = 0;
				if(offset >= len) {
					return;
				}
				uint8_t enable = opcodes[offset++];
				if(enable) {
					bytes_to_skip = 3;
				}
				break;
			}

			case SF_OP_ARPEGGIO: {
				bytes_to_skip = 0;
				if(offset >= len) {
					return;
				}
				uint8_t enable = opcodes[offset++];
				if(enable) {
					bytes_to_skip = 1;
				}
				break;
			}

			case SF_OP_VIBRATO:
			case SF_OP_PHASING: {
				bytes_to_skip = 0;
				if(offset >= len) {
					return;
				}
				uint8_t enable = opcodes[offset++];
				if(enable) {
					bytes_to_skip = 4;
				}
				break;
			}

			case SF_OP_SET_ADSR: {
				bytes_to_skip = 4;
				if(offset + 3 >= len) {
					return;
				}
				offset += 3;
				uint8_t release_enabled = opcodes[offset++];
				if(release_enabled) {
					bytes_to_skip++;
				}
				break;
			}

			case SF_OP_DEFINE_INSTRUMENT: {
				if(offset + 3 >= len) {
					return;
				}
				offset++; // instrument number
				uint16_t instrument_length = sf_fetch_word_at(opcodes, &offset);

				struct sf_instrument tmp;
				sf_fetch_instrument(opcodes, offset, &tmp);
				sf_insert_instrument(s, &tmp);

				if((uint32_t)instrument_length * 2U < 4U) {
					return;
				}
				bytes_to_skip = (uint32_t)instrument_length * 2U - 4U;
				break;
			}

			case SF_OP_RETURN: {
				bytes_to_skip = 0;
				stop = 1;
				break;
			}

			case SF_OP_GOSUB: {
				if(offset + 4 > len) {
					return;
				}
				int32_t goto_offset = sf_fetch_long_at(opcodes, &offset);
				uint32_t new_offset = (uint32_t)((int32_t)offset + goto_offset);

				if(!sf_taken_contains(taken, new_offset, len)) {
					sf_find_samples_in_list(s, new_offset, taken);
				}
				bytes_to_skip = 0;
				break;
			}

			case SF_OP_GOTO: {
				if(offset + 4 > len) {
					return;
				}
				int32_t goto_offset = sf_fetch_long_at(opcodes, &offset);
				offset = (uint32_t)((int32_t)offset + goto_offset);

				if(sf_taken_contains(taken, offset, len)) {
					stop = 1;
				}
				bytes_to_skip = 0;
				break;
			}

			case SF_OP_LOOP:
			case SF_OP_END: {
				bytes_to_skip = 0;
				stop = 1;
				break;
			}

			default: {
				// Notes
				bytes_to_skip = 2;
				break;
			}
		}

		offset += bytes_to_skip;
	}
}

// [=]===^=[ sf_find_samples ]====================================================================[=]
static int32_t sf_find_samples(struct soundfactory_state *s) {
	uint8_t *taken = (uint8_t *)calloc(s->opcodes_len, 1);
	if(!taken) {
		return 0;
	}

	for(int32_t si = 0; si < s->num_songs; ++si) {
		for(int32_t ci = 0; ci < 4; ++ci) {
			sf_find_samples_in_list(s, s->songs[si].opcode_start_offsets[ci], taken);
		}
	}

	for(int32_t i = 0; i < s->num_instruments; ++i) {
		s->instruments[i].instrument_number = (int16_t)i;
	}

	free(taken);
	return 1;
}

// [=]===^=[ sf_load ]============================================================================[=]
static int32_t sf_load(struct soundfactory_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	uint32_t module_length = sf_read_u32_be(data);
	if(module_length > len || module_length < 276) {
		return 0;
	}

	// Sub-songs
	s->num_songs = 0;
	for(int32_t i = 0; i < 16; ++i) {
		uint8_t channels = data[4 + i];
		if(channels == 0) {
			continue;
		}
		s->songs[s->num_songs].enabled_channels = channels;
		s->num_songs++;
	}
	if(s->num_songs == 0) {
		return 0;
	}

	// File layout: 16 (channels) starting at offset 4, then 4 uint32 offsets per
	// non-empty sub-song starting at offset 20 (matches NostalgicPlayer behavior).
	uint32_t pos = 20;
	for(int32_t i = 0; i < s->num_songs; ++i) {
		if(pos + 16 > len) {
			return 0;
		}
		for(int32_t c = 0; c < 4; ++c) {
			uint32_t off = sf_read_u32_be(data + pos);
			pos += 4;
			if(off < 276) {
				return 0;
			}
			s->songs[i].opcode_start_offsets[c] = off - 276;
		}
	}

	s->opcodes = data + 276;
	s->opcodes_len = module_length - 276;

	if(!sf_find_samples(s)) {
		return 0;
	}

	return 1;
}

// [=]===^=[ sf_sound_table_lookup ]===============================================================[=]
// Returns a pointer to the active instrument for voice `cur`. Indices >= num_instruments
// or negative fall back to the default instrument.
static struct sf_instrument *sf_sound_table_lookup(struct soundfactory_state *s, uint8_t sound_table_idx) {
	int32_t idx = s->sound_table[sound_table_idx];
	if(idx < 0 || idx >= s->num_instruments) {
		return &s->default_instrument;
	}
	return &s->current_instruments[idx];
}

// [=]===^=[ sf_initialize_sound ]================================================================[=]
static void sf_initialize_sound(struct soundfactory_state *s, int32_t subsong) {
	s->cur_subsong = subsong;
	sf_init_default_instrument(&s->default_instrument);

	// Rebuild live instrument table from the templates (deep-ish copy of value types)
	for(int32_t i = 0; i < s->num_instruments; ++i) {
		s->current_instruments[i] = s->instruments[i];
	}

	for(int32_t i = 0; i < SF_SOUND_TABLE_LEN; ++i) {
		s->sound_table[i] = -1;
	}

	s->fade_out_flag = 0;
	s->fade_in_flag = 0;
	s->fade_out_volume = 0;
	s->fade_out_counter = 0;
	s->fade_out_speed = 0;
	s->request_counter = 0;
	s->amiga_filter = 0;

	for(int32_t i = 0; i < 4; ++i) {
		struct sf_voice *v = &s->voices[i];
		memset(v, 0, sizeof(*v));
		v->channel_number = i;
		v->voice_enabled = (s->songs[subsong].enabled_channels & (1u << i)) != 0;
		v->start_position = s->songs[subsong].opcode_start_offsets[i];
		v->current_position = v->start_position;
		v->note_duration = 1;
		v->envelope_state = SF_ENV_ATTACK;
	}
}

// [=]===^=[ sf_push ]============================================================================[=]
static void sf_push(struct sf_voice *v, uint32_t value) {
	if(v->stack_top >= SF_STACK_DEPTH) {
		return;
	}
	v->stack[v->stack_top++] = value;
}

// [=]===^=[ sf_pop ]=============================================================================[=]
static uint32_t sf_pop(struct sf_voice *v) {
	if(v->stack_top <= 0) {
		return 0;
	}
	return v->stack[--v->stack_top];
}

// [=]===^=[ sf_fetch_code ]======================================================================[=]
static uint8_t sf_fetch_code(struct soundfactory_state *s, struct sf_voice *v) {
	if(v->current_position >= s->opcodes_len) {
		return 0;
	}
	return s->opcodes[v->current_position++];
}

// [=]===^=[ sf_fetch_word ]======================================================================[=]
static uint16_t sf_fetch_word(struct soundfactory_state *s, struct sf_voice *v) {
	uint16_t hi = sf_fetch_code(s, v);
	uint16_t lo = sf_fetch_code(s, v);
	return (uint16_t)((hi << 8) | lo);
}

// [=]===^=[ sf_calc_period_sampling ]============================================================[=]
static uint16_t sf_calc_period_sampling(struct sf_instrument *ins, int32_t octave, int32_t note) {
	uint16_t multiplier = sf_multiply_table[note];
	int32_t period = (int32_t)ins->sampling_period * (int32_t)multiplier / 32768;
	int32_t cur_oct = (int32_t)ins->octave;

	while(cur_oct != octave) {
		if(cur_oct < octave) {
			period /= 2;
			cur_oct++;
		} else {
			period *= 2;
			cur_oct--;
		}
	}
	return (uint16_t)period;
}

// [=]===^=[ sf_calc_period_multi_octave ]========================================================[=]
static uint16_t sf_calc_period_multi_octave(struct sf_instrument *ins, int32_t octave, int32_t note) {
	uint32_t period = sf_sample_table[note];
	if(ins->wave_count != 1) {
		period *= ins->wave_count;
	}
	if(ins->sample_length != 1) {
		period /= ins->sample_length;
	}
	while(octave != 0) {
		period /= 2;
		octave--;
	}
	return (uint16_t)(period * 2);
}

// [=]===^=[ sf_calc_period ]=====================================================================[=]
static uint16_t sf_calc_period(struct sf_voice *v, struct sf_instrument *ins) {
	int32_t octave = v->note / 12;
	int32_t note = v->note % 12;
	if(ins->sampling_period != 0) {
		return sf_calc_period_sampling(ins, octave, note);
	}
	return sf_calc_period_multi_octave(ins, octave, note);
}

// [=]===^=[ sf_average ]=========================================================================[=]
static int16_t sf_average(int8_t *sample_data, uint16_t sample_length, uint16_t offset, int16_t count) {
	int16_t sum = 0;
	for(int16_t i = count; i > 0; --i) {
		sum = (int16_t)(sum + sample_data[offset]);
		offset++;
		if(offset == sample_length) {
			offset = 0;
		}
	}
	return (int16_t)(sum / count);
}

// [=]===^=[ sf_mix ]=============================================================================[=]
static void sf_mix(struct sf_voice *v, struct sf_instrument *ins) {
	uint16_t sample_length = (uint16_t)(ins->sample_length * 2);
	int8_t *sample_data = ins->sample_data;
	int8_t *pb = v->phasing_buffer;

	int32_t index = (int32_t)sample_length - (int32_t)v->phasing_relative;

	for(int32_t i = 0; i < sample_length && i < SF_PHASING_BUF_LEN; ++i) {
		int16_t s1 = (index >= (int32_t)sample_length) ? 0 : (int16_t)sample_data[index];
		int16_t s2 = (int16_t)sample_data[i];
		pb[i] = (int8_t)((s1 + s2) / 2);
		index++;
		if(index == (int32_t)sample_length) {
			index = 0;
		}
	}
}

// [=]===^=[ sf_filter ]==========================================================================[=]
static void sf_filter(struct sf_voice *v, struct sf_instrument *ins) {
	uint16_t sample_length = (uint16_t)(ins->sample_length * 2);
	int8_t *sample_data = ins->sample_data;
	int8_t *pb = v->phasing_buffer;

	if((ins->effect_byte & SF_IF_PHASING) != 0) {
		sample_data = pb;
	}

	if(v->filter_relative == 1) {
		if(sample_data != pb) {
			int32_t to_copy = (sample_length == 256) ? 256 : (sample_length + 4);
			if(to_copy > SF_PHASING_BUF_LEN) {
				to_copy = SF_PHASING_BUF_LEN;
			}
			memcpy(pb, sample_data, (size_t)to_copy);
		}
		return;
	}

	uint16_t sample_position = (uint16_t)(v->filter_relative / 2);
	int16_t average = sf_average(sample_data, sample_length, 0, (int16_t)v->filter_relative);

	int32_t finished = 0;
	while(!finished) {
		int16_t prev_average = average;

		uint16_t position = (uint16_t)((v->filter_relative / 2) + sample_position);
		if(position >= sample_length) {
			position = (uint16_t)(position - sample_length);
		}
		average = sf_average(sample_data, sample_length, position, (int16_t)v->filter_relative);

		int16_t difference = (int16_t)(average - prev_average);
		int16_t step = 1;
		if(difference < 0) {
			step = -1;
			difference = (int16_t)(-difference);
		}

		int16_t counter = 0;
		for(int32_t i = (int32_t)v->filter_relative; i > 0; --i) {
			if(sample_position < SF_PHASING_BUF_LEN) {
				pb[sample_position] = (int8_t)prev_average;
			}
			counter = (int16_t)(counter + difference);
			while(counter >= (int16_t)v->filter_relative) {
				counter = (int16_t)(counter - (int16_t)v->filter_relative);
				prev_average = (int16_t)(prev_average + step);
			}
			sample_position++;
			if(sample_position >= sample_length) {
				sample_position = (uint16_t)(sample_position - sample_length);
				finished = 1;
			}
		}
	}
}

// [=]===^=[ sf_in_hardware ]=====================================================================[=]
// Commits the voice's current period and volume to the Paula channel. Applies
// tremolo/vibrato/arpeggio/fade modifiers exactly like NostalgicPlayer's InHardware.
static void sf_in_hardware(struct soundfactory_state *s, struct sf_voice *v, int32_t chan, uint8_t instr_flag) {
	uint16_t volume = v->current_volume;

	if((instr_flag & SF_IF_TREMOLO) != 0 && v->tremolo_volume != 0) {
		volume = (uint16_t)((volume * v->tremolo_volume) / 256);
	}
	if(v->volume != 0) {
		volume = (uint16_t)((volume * v->volume) / 256);
	}
	if((s->fade_out_flag || s->fade_in_flag) && s->fade_out_volume != 0) {
		volume = (uint16_t)((volume * s->fade_out_volume) / 256);
	}

	paula_set_volume(&s->paula, chan, volume);

	uint16_t period = ((instr_flag & SF_IF_PORTAMENTO) != 0) ? v->active_period : v->period;
	if((instr_flag & SF_IF_VIBRATO) != 0 && v->vibrato_delay == 0) {
		period = (uint16_t)(period + v->vibrato_relative);
	}
	if((instr_flag & SF_IF_ARPEGGIO) != 0 && v->arpeggio_flag) {
		period /= 2;
	}
	period = (uint16_t)(period + v->fine_tune);

	paula_set_period(&s->paula, chan, period);
}

// [=]===^=[ sf_new_note ]========================================================================[=]
// Programs a fresh note on the voice: selects sample buffer (phasing/filter),
// configures envelope state, kicks Paula with PlaySample + optional loop.
static int32_t sf_new_note(struct soundfactory_state *s, struct sf_voice *v, int32_t chan, struct sf_instrument *ins, uint8_t note) {
	v->note = (uint8_t)((note + v->transpose) & 0x7f);
	uint8_t instr_flag = ins->effect_byte;

	if((instr_flag & SF_IF_PORTAMENTO) != 0) {
		v->active_period = v->period;
		v->portamento_counter = 1;
	}

	v->period = sf_calc_period(v, ins);

	uint16_t original_note_duration = sf_fetch_word(s, v);
	uint16_t note_duration = (uint16_t)(original_note_duration & 0x7fff);

	if(note_duration == 0) {
		paula_mute(&s->paula, chan);
		return 0;
	}

	v->note_duration = note_duration;
	v->note_duration2 = (uint16_t)(note_duration / 2);
	v->note_start_flag = 1;

	if((instr_flag & SF_IF_ARPEGGIO) != 0) {
		v->arpeggio_flag = 0;
		v->arpeggio_counter = ins->arpeggio_speed;
	}

	if((instr_flag & SF_IF_VIBRATO) != 0) {
		v->vibrato_delay = ins->vibrato_delay;
		if(v->vibrato_delay == 0) {
			v->vibrato_step = ins->vibrato_step;
			v->vibrato_relative = 0;
			v->vibrato_counter = ins->vibrato_speed;
			v->vibrato_counter2 = ins->vibrato_amount;
		}
	}

	if((instr_flag & SF_IF_TREMOLO) != 0) {
		v->tremolo_counter = 1;
		v->tremolo_step = (int8_t)(-ins->tremolo_step);
		v->tremolo_volume = 0;
	}

	if((original_note_duration & 0x8000) == 0) {
		v->envelope_counter = 0;
		if(ins->attack_time != 0) {
			v->current_volume = 0;
			v->envelope_state = SF_ENV_ATTACK;
		} else {
			if(ins->decay_time == 0 || ins->sustain_level == 64) {
				v->envelope_state = SF_ENV_SUSTAIN;
				v->current_volume = ins->sustain_level;
			} else {
				v->current_volume = 64;
				v->envelope_state = SF_ENV_DECAY;
			}
		}
	}

	int8_t *sample_data;

	if((instr_flag & SF_IF_PHASING) != 0) {
		v->phasing_counter = ins->phasing_speed;
		v->phasing_step = ins->phasing_step;
		v->phasing_relative = (int8_t)ins->phasing_start;

		sf_mix(v, ins);
		sample_data = v->phasing_buffer;
	} else {
		sample_data = ins->sample_data;
	}

	if((instr_flag & SF_IF_FILTER) != 0) {
		v->filter_counter = ins->filter_speed;
		v->filter_relative = ins->filter_frequency;
		v->filter_step = 1;

		sf_filter(v, ins);
		sample_data = v->phasing_buffer;
	}

	v->active_sample_data = sample_data;

	if(ins->dasr_sustain_offset != 0) {
		uint32_t play_len = (uint32_t)ins->dasr_release_offset * 2U;
		paula_play_sample(&s->paula, chan, sample_data, play_len);
		uint32_t loop_start = (uint32_t)ins->dasr_sustain_offset * 2U;
		uint32_t loop_length = ((uint32_t)ins->dasr_release_offset - (uint32_t)ins->dasr_sustain_offset) * 2U;
		paula_set_loop(&s->paula, chan, loop_start, loop_length);
	} else {
		uint32_t play_len = (uint32_t)ins->sample_length * 2U;
		paula_play_sample(&s->paula, chan, sample_data, play_len);
		if((instr_flag & SF_IF_ONESHOT) == 0) {
			paula_set_loop(&s->paula, chan, 0, play_len);
		}
	}

	sf_in_hardware(s, v, chan, instr_flag);
	return 1;
}

// [=]===^=[ sf_handle_fade ]=====================================================================[=]
static void sf_handle_fade(struct soundfactory_state *s) {
	if(!s->fade_in_flag && !s->fade_out_flag) {
		return;
	}

	uint8_t new_fade_counter = (uint8_t)(s->fade_out_counter + s->fade_out_speed);
	s->fade_out_counter = (uint8_t)(new_fade_counter & 3);

	new_fade_counter >>= 2;
	int32_t fade_volume = s->fade_out_volume;

	if(s->fade_in_flag) {
		fade_volume += new_fade_counter;
		if(fade_volume >= 256) {
			s->fade_in_flag = 0;
			s->fade_out_volume = 0;
		} else {
			s->fade_out_volume = (uint8_t)fade_volume;
		}
		return;
	}

	// Fade out
	if(fade_volume == 0) {
		fade_volume -= new_fade_counter;
	} else {
		fade_volume -= new_fade_counter;
		if(fade_volume <= 0) {
			s->fade_out_flag = 0;
			s->fade_in_flag = 0;
			for(int32_t i = 0; i < 4; ++i) {
				s->voices[i].voice_enabled = 0;
				paula_mute(&s->paula, i);
			}
			// Restart the current sub-song
			sf_initialize_sound(s, s->cur_subsong);
			return;
		}
	}
	s->fade_out_volume = (uint8_t)fade_volume;
}

// [=]===^=[ sf_opcode_pause ]====================================================================[=]
static void sf_opcode_pause(struct soundfactory_state *s, struct sf_voice *v) {
	v->note_duration = sf_fetch_word(s, v);
}

// [=]===^=[ sf_opcode_set_volume ]===============================================================[=]
static void sf_opcode_set_volume(struct soundfactory_state *s, struct sf_voice *v) {
	v->volume = sf_fetch_code(s, v);
}

// [=]===^=[ sf_opcode_set_finetune ]=============================================================[=]
static void sf_opcode_set_finetune(struct soundfactory_state *s, struct sf_voice *v) {
	v->fine_tune = sf_fetch_code(s, v);
}

// [=]===^=[ sf_opcode_use_instrument ]===========================================================[=]
static struct sf_instrument *sf_opcode_use_instrument(struct soundfactory_state *s, struct sf_voice *v) {
	v->current_instrument = sf_fetch_code(s, v);
	return sf_sound_table_lookup(s, v->current_instrument);
}

// [=]===^=[ sf_opcode_define_instrument ]========================================================[=]
static void sf_opcode_define_instrument(struct soundfactory_state *s, struct sf_voice *v) {
	uint8_t instrument_number = sf_fetch_code(s, v);
	uint16_t instrument_length = sf_fetch_word(s, v);

	int32_t idx = sf_find_instrument_index(s, v->current_position);
	s->sound_table[instrument_number & (SF_SOUND_TABLE_LEN - 1)] = idx;

	// Skip instrument data, we already parsed it
	uint32_t skip = (uint32_t)instrument_length * 2U - 4U;
	v->current_position += skip;
}

// [=]===^=[ sf_opcode_return ]===================================================================[=]
static void sf_opcode_return(struct sf_voice *v) {
	v->current_position = sf_pop(v);
}

// [=]===^=[ sf_opcode_gosub ]====================================================================[=]
static void sf_opcode_gosub(struct soundfactory_state *s, struct sf_voice *v) {
	uint32_t hi = sf_fetch_word(s, v);
	uint32_t lo = sf_fetch_word(s, v);
	int32_t new_position = (int32_t)((hi << 16) | lo);
	sf_push(v, v->current_position);
	v->current_position = (uint32_t)((int32_t)v->current_position + new_position);
}

// [=]===^=[ sf_opcode_goto ]=====================================================================[=]
static void sf_opcode_goto(struct soundfactory_state *s, struct sf_voice *v) {
	uint32_t hi = sf_fetch_word(s, v);
	uint32_t lo = sf_fetch_word(s, v);
	int32_t new_position = (int32_t)((hi << 16) | lo);
	v->current_position = (uint32_t)((int32_t)v->current_position + new_position);
}

// [=]===^=[ sf_opcode_for ]======================================================================[=]
static void sf_opcode_for(struct soundfactory_state *s, struct sf_voice *v) {
	uint8_t count = sf_fetch_code(s, v);
	sf_push(v, count);
	sf_push(v, v->current_position);
}

// [=]===^=[ sf_opcode_next ]=====================================================================[=]
static void sf_opcode_next(struct sf_voice *v) {
	uint32_t loop_position = sf_pop(v);
	uint8_t loop_count = (uint8_t)sf_pop(v);
	loop_count--;
	if(loop_count != 0) {
		v->current_position = loop_position;
		sf_push(v, loop_count);
		sf_push(v, loop_position);
	}
}

// [=]===^=[ sf_opcode_fade_out ]=================================================================[=]
static void sf_opcode_fade_out(struct soundfactory_state *s, struct sf_voice *v) {
	s->fade_out_speed = sf_fetch_code(s, v);
	s->fade_in_flag = 0;
	s->fade_out_counter = 0;
	s->fade_out_flag = 1;
}

// [=]===^=[ sf_opcode_request ]==================================================================[=]
static void sf_opcode_request(struct soundfactory_state *s) {
	s->request_counter++;
}

// [=]===^=[ sf_opcode_loop ]=====================================================================[=]
static void sf_opcode_loop(struct sf_voice *v) {
	v->current_position = v->start_position;
}

// [=]===^=[ sf_opcode_end ]======================================================================[=]
static void sf_opcode_end(struct soundfactory_state *s, struct sf_voice *v, int32_t chan) {
	paula_mute(&s->paula, chan);
	v->voice_enabled = 0;
}

// [=]===^=[ sf_opcode_fade_in ]==================================================================[=]
static void sf_opcode_fade_in(struct soundfactory_state *s, struct sf_voice *v) {
	s->fade_out_speed = sf_fetch_code(s, v);
	s->fade_in_flag = 1;
	if(s->fade_out_volume == 0) {
		s->fade_out_volume = 1;
	}
	s->fade_out_counter = 0;
	s->fade_out_flag = 0;
}

// [=]===^=[ sf_opcode_set_adsr ]=================================================================[=]
static void sf_opcode_set_adsr(struct soundfactory_state *s, struct sf_voice *v, struct sf_instrument *ins) {
	ins->attack_time = sf_fetch_code(s, v);
	ins->decay_time = sf_fetch_code(s, v);
	ins->sustain_level = sf_fetch_code(s, v);
	uint8_t release_enabled = sf_fetch_code(s, v);
	if(release_enabled) {
		ins->effect_byte = (uint8_t)(ins->effect_byte & ~SF_IF_RELEASE);
		ins->release_time = sf_fetch_code(s, v);
	} else {
		ins->effect_byte |= SF_IF_RELEASE;
	}
}

// [=]===^=[ sf_opcode_oneshot ]==================================================================[=]
static void sf_opcode_oneshot(struct sf_instrument *ins) {
	ins->effect_byte |= SF_IF_ONESHOT;
}

// [=]===^=[ sf_opcode_looping ]==================================================================[=]
static void sf_opcode_looping(struct sf_instrument *ins) {
	ins->effect_byte = (uint8_t)(ins->effect_byte & ~SF_IF_ONESHOT);
}

// [=]===^=[ sf_opcode_vibrato ]==================================================================[=]
static void sf_opcode_vibrato(struct soundfactory_state *s, struct sf_voice *v, struct sf_instrument *ins) {
	uint8_t enable = sf_fetch_code(s, v);
	if(enable) {
		ins->effect_byte |= SF_IF_VIBRATO;
		ins->vibrato_delay = sf_fetch_code(s, v);
		ins->vibrato_speed = sf_fetch_code(s, v);
		ins->vibrato_step = (int8_t)sf_fetch_code(s, v);
		ins->vibrato_amount = sf_fetch_code(s, v);
	} else {
		ins->effect_byte = (uint8_t)(ins->effect_byte & ~SF_IF_VIBRATO);
	}
}

// [=]===^=[ sf_opcode_arpeggio ]=================================================================[=]
static void sf_opcode_arpeggio(struct soundfactory_state *s, struct sf_voice *v, struct sf_instrument *ins) {
	uint8_t enable = sf_fetch_code(s, v);
	if(enable) {
		ins->effect_byte |= SF_IF_ARPEGGIO;
		ins->arpeggio_speed = sf_fetch_code(s, v);
	} else {
		ins->effect_byte = (uint8_t)(ins->effect_byte & ~SF_IF_ARPEGGIO);
	}
}

// [=]===^=[ sf_opcode_phasing ]==================================================================[=]
static void sf_opcode_phasing(struct soundfactory_state *s, struct sf_voice *v, struct sf_instrument *ins) {
	uint8_t enable = sf_fetch_code(s, v);
	if(enable) {
		ins->effect_byte |= SF_IF_PHASING;
		ins->phasing_start = sf_fetch_code(s, v);
		ins->phasing_end = sf_fetch_code(s, v);
		ins->phasing_speed = sf_fetch_code(s, v);
		ins->phasing_step = (int8_t)sf_fetch_code(s, v);
	} else {
		ins->effect_byte = (uint8_t)(ins->effect_byte & ~SF_IF_PHASING);
	}
}

// [=]===^=[ sf_opcode_portamento ]===============================================================[=]
static void sf_opcode_portamento(struct soundfactory_state *s, struct sf_voice *v, struct sf_instrument *ins) {
	uint8_t enable = sf_fetch_code(s, v);
	if(enable) {
		ins->effect_byte |= SF_IF_PORTAMENTO;
		ins->portamento_speed = sf_fetch_code(s, v);
		ins->portamento_step = sf_fetch_word(s, v);
	} else {
		ins->effect_byte = (uint8_t)(ins->effect_byte & ~SF_IF_PORTAMENTO);
	}
}

// [=]===^=[ sf_opcode_tremolo ]==================================================================[=]
static void sf_opcode_tremolo(struct soundfactory_state *s, struct sf_voice *v, struct sf_instrument *ins) {
	uint8_t enable = sf_fetch_code(s, v);
	if(enable) {
		ins->effect_byte |= SF_IF_TREMOLO;
		ins->tremolo_speed = sf_fetch_code(s, v);
		ins->tremolo_step = sf_fetch_code(s, v);
		ins->tremolo_range = sf_fetch_code(s, v);
	} else {
		ins->effect_byte = (uint8_t)(ins->effect_byte & ~SF_IF_TREMOLO);
	}
}

// [=]===^=[ sf_opcode_filter ]===================================================================[=]
static void sf_opcode_filter(struct soundfactory_state *s, struct sf_voice *v, struct sf_instrument *ins) {
	uint8_t enable = sf_fetch_code(s, v);
	if(enable) {
		ins->effect_byte |= SF_IF_FILTER;
		ins->filter_frequency = sf_fetch_code(s, v);
		ins->filter_end = sf_fetch_code(s, v);
		ins->filter_speed = sf_fetch_code(s, v);
	} else {
		ins->effect_byte = (uint8_t)(ins->effect_byte & ~SF_IF_FILTER);
	}
}

// [=]===^=[ sf_opcode_stop_and_pause ]===========================================================[=]
static void sf_opcode_stop_and_pause(struct soundfactory_state *s, struct sf_voice *v, int32_t chan) {
	v->note_duration = sf_fetch_word(s, v);
	paula_mute(&s->paula, chan);
}

// [=]===^=[ sf_opcode_led ]======================================================================[=]
static void sf_opcode_led(struct soundfactory_state *s, struct sf_voice *v) {
	uint8_t enable = sf_fetch_code(s, v);
	s->amiga_filter = enable ? 1 : 0;
}

// [=]===^=[ sf_opcode_wait_for_request ]=========================================================[=]
// Returns 1 if the request matched (caller continues parsing), 0 to break out.
static int32_t sf_opcode_wait_for_request(struct soundfactory_state *s, struct sf_voice *v) {
	uint8_t value = sf_fetch_code(s, v);
	if(value == s->request_counter) {
		return 1;
	}
	v->current_position -= 2;
	v->note_duration = 1;
	return 0;
}

// [=]===^=[ sf_opcode_set_transpose ]============================================================[=]
static void sf_opcode_set_transpose(struct soundfactory_state *s, struct sf_voice *v) {
	v->transpose = (int8_t)sf_fetch_code(s, v);
}

// [=]===^=[ sf_modulator ]=======================================================================[=]
// Per-tick effects processor: tremolo, portamento, arpeggio, vibrato, ADSR,
// phasing/filter re-render. Produces the final period/volume for this tick.
static void sf_modulator(struct soundfactory_state *s, struct sf_voice *v, int32_t chan, struct sf_instrument *ins) {
	uint8_t instr_flag = ins->effect_byte;

	v->note_start_flag1 = v->note_start_flag;
	v->note_start_flag = 0;

	if((instr_flag & SF_IF_TREMOLO) != 0) {
		v->tremolo_counter--;
		if(v->tremolo_counter == 0) {
			v->tremolo_counter = ins->tremolo_speed;
			v->tremolo_volume = (uint8_t)(v->tremolo_volume + v->tremolo_step);
			if(v->tremolo_volume <= ins->tremolo_range) {
				v->tremolo_step = (int8_t)(-v->tremolo_step);
			}
		}
	}

	if((instr_flag & SF_IF_PORTAMENTO) != 0) {
		if(v->period != v->active_period) {
			v->portamento_counter--;
			if(v->portamento_counter == 0) {
				v->portamento_counter = ins->portamento_speed;
				if(v->period < v->active_period) {
					v->active_period = (uint16_t)(v->active_period - ins->portamento_step);
					if(v->active_period < v->period) {
						v->active_period = v->period;
					}
				} else {
					v->active_period = (uint16_t)(v->active_period + ins->portamento_step);
					if(v->active_period > v->period) {
						v->active_period = v->period;
					}
				}
			}
		}
	}

	if((instr_flag & SF_IF_ARPEGGIO) != 0) {
		v->arpeggio_counter--;
		if(v->arpeggio_counter == 0) {
			v->arpeggio_counter = ins->arpeggio_speed;
			v->arpeggio_flag = v->arpeggio_flag ? 0 : 1;
		}
	}

	if((instr_flag & SF_IF_VIBRATO) != 0) {
		if(v->vibrato_delay == 0) {
			v->vibrato_counter--;
			if(v->vibrato_counter == 0) {
				v->vibrato_counter = ins->vibrato_speed;
				v->vibrato_relative = (int16_t)(v->vibrato_relative + v->vibrato_step);
				v->vibrato_counter2--;
				if(v->vibrato_counter2 == 0) {
					v->vibrato_counter2 = (uint8_t)(ins->vibrato_amount * 2);
					v->vibrato_step = (int8_t)(-v->vibrato_step);
				}
			}
		} else {
			v->vibrato_delay--;
			if(v->vibrato_delay == 0) {
				v->vibrato_counter = 1;
				v->vibrato_relative = 0;
				v->vibrato_step = ins->vibrato_step;
				v->vibrato_counter2 = ins->vibrato_amount;
			}
		}
	}

	if(v->envelope_state == SF_ENV_RELEASE) {
		if(v->current_volume != 0) {
			v->envelope_counter = (uint8_t)(v->envelope_counter + ins->sustain_level);
			for(;;) {
				if(v->envelope_counter < ins->release_time) {
					break;
				}
				v->envelope_counter = (uint8_t)(v->envelope_counter - ins->release_time);
				v->current_volume--;
				if(v->current_volume == 0) {
					break;
				}
			}
		}
	} else {
		int32_t do_release_skip = 0;
		if((instr_flag & SF_IF_RELEASE) != 0) {
			do_release_skip = 1;
		} else {
			if(v->note_duration2 != 0) {
				v->note_duration2--;
			}
			if(v->note_duration2 == 0) {
				if(v->envelope_state != SF_ENV_SUSTAIN) {
					do_release_skip = 1;
				} else {
					v->envelope_state = SF_ENV_RELEASE;
					v->envelope_counter = 0;
					if((instr_flag & SF_IF_ONESHOT) != 0 && ins->dasr_sustain_offset != 0) {
						uint32_t sus = (uint32_t)ins->dasr_sustain_offset * 2U;
						uint32_t len = ((uint32_t)ins->dasr_release_offset - (uint32_t)ins->dasr_sustain_offset) * 2U;
						// Queue the sustain/release tail; preserve the currently-active buffer
						// (phasing_buffer or instrument sample) via active_sample_data.
						paula_queue_sample(&s->paula, chan, v->active_sample_data, sus, len);
						paula_set_loop(&s->paula, chan, sus, len);
						v->note_start_flag = 2;
					}
				}
			} else {
				do_release_skip = 1;
			}
		}

		if(do_release_skip) {
			if(v->envelope_state == SF_ENV_DECAY) {
				v->envelope_counter = (uint8_t)(v->envelope_counter + (uint8_t)(64 - ins->sustain_level));
				for(;;) {
					if(v->envelope_counter < ins->decay_time) {
						break;
					}
					v->envelope_counter = (uint8_t)(v->envelope_counter - ins->decay_time);
					v->current_volume--;
					if(v->current_volume == 0) {
						break;
					}
				}
				if(v->current_volume <= ins->sustain_level) {
					v->envelope_state = SF_ENV_SUSTAIN;
				}
			} else if(v->envelope_state == SF_ENV_ATTACK) {
				uint8_t level = 64;
				if(ins->decay_time == 0) {
					level = ins->sustain_level;
				}
				v->envelope_counter = (uint8_t)(v->envelope_counter + level);
				if(ins->attack_time != 0) {
					while(v->envelope_counter >= ins->attack_time) {
						v->envelope_counter = (uint8_t)(v->envelope_counter - ins->attack_time);
						v->current_volume++;
					}
				}
				if(v->current_volume == level) {
					if(ins->decay_time == 0) {
						v->envelope_state = SF_ENV_SUSTAIN;
					} else {
						v->envelope_counter = 0;
						if(ins->sustain_level == 64) {
							v->envelope_state = SF_ENV_SUSTAIN;
						} else {
							v->envelope_state = SF_ENV_DECAY;
						}
					}
				}
			}
		}
	}

	int32_t mix_flag = 0;

	if((instr_flag & SF_IF_PHASING) != 0) {
		v->phasing_counter--;
		if(v->phasing_counter == 0) {
			v->phasing_counter = ins->phasing_speed;
			int16_t relative = (int16_t)(v->phasing_relative + v->phasing_step);
			v->phasing_relative = (int8_t)relative;
			if(relative < 0 || relative >= (int16_t)ins->phasing_end || relative <= (int16_t)ins->phasing_start) {
				v->phasing_step = (int8_t)(-v->phasing_step);
			}
			mix_flag = 1;
		}
	}

	if((instr_flag & SF_IF_FILTER) != 0) {
		v->filter_counter--;
		if(v->filter_counter == 0) {
			v->filter_counter = ins->filter_speed;
			v->filter_relative = (uint8_t)(v->filter_relative + v->filter_step);
			if(v->filter_relative == ins->filter_frequency || v->filter_relative == ins->filter_end) {
				v->filter_step = (int8_t)(-v->filter_step);
			}
			mix_flag = 1;
		}
	}

	if(mix_flag) {
		if((instr_flag & SF_IF_PHASING) != 0) {
			sf_mix(v, ins);
		}
		if((instr_flag & SF_IF_FILTER) != 0) {
			sf_filter(v, ins);
		}
	}

	sf_in_hardware(s, v, chan, instr_flag);
}

// [=]===^=[ sf_next_note ]========================================================================[=]
// Pulls opcodes until a note-producing opcode is consumed (or the voice ends).
static void sf_next_note(struct soundfactory_state *s, struct sf_voice *v, int32_t chan, struct sf_instrument *ins) {
	uint8_t opcode;
	int32_t keep_parsing = 1;

	while(keep_parsing) {
		if(v->current_position >= s->opcodes_len) {
			paula_mute(&s->paula, chan);
			v->voice_enabled = 0;
			return;
		}

		opcode = sf_fetch_code(s, v);

		if((opcode & 0x80) == 0) {
			int32_t ok = sf_new_note(s, v, chan, ins, opcode);
			opcode = ok ? 0 : 1;
		} else {
			switch(opcode) {
				case SF_OP_PAUSE: {
					sf_opcode_pause(s, v);
					break;
				}
				case SF_OP_SET_VOLUME: {
					sf_opcode_set_volume(s, v);
					break;
				}
				case SF_OP_SET_FINETUNE: {
					sf_opcode_set_finetune(s, v);
					break;
				}
				case SF_OP_USE_INSTRUMENT: {
					ins = sf_opcode_use_instrument(s, v);
					break;
				}
				case SF_OP_DEFINE_INSTRUMENT: {
					sf_opcode_define_instrument(s, v);
					break;
				}
				case SF_OP_RETURN: {
					sf_opcode_return(v);
					break;
				}
				case SF_OP_GOSUB: {
					sf_opcode_gosub(s, v);
					break;
				}
				case SF_OP_GOTO: {
					sf_opcode_goto(s, v);
					break;
				}
				case SF_OP_FOR: {
					sf_opcode_for(s, v);
					break;
				}
				case SF_OP_NEXT: {
					sf_opcode_next(v);
					break;
				}
				case SF_OP_FADE_OUT: {
					sf_opcode_fade_out(s, v);
					break;
				}
				case SF_OP_NOP: {
					break;
				}
				case SF_OP_REQUEST: {
					sf_opcode_request(s);
					break;
				}
				case SF_OP_LOOP: {
					sf_opcode_loop(v);
					break;
				}
				case SF_OP_END: {
					sf_opcode_end(s, v, chan);
					opcode = 0;
					break;
				}
				case SF_OP_FADE_IN: {
					sf_opcode_fade_in(s, v);
					break;
				}
				case SF_OP_SET_ADSR: {
					sf_opcode_set_adsr(s, v, ins);
					break;
				}
				case SF_OP_ONESHOT: {
					sf_opcode_oneshot(ins);
					break;
				}
				case SF_OP_LOOPING: {
					sf_opcode_looping(ins);
					break;
				}
				case SF_OP_VIBRATO: {
					sf_opcode_vibrato(s, v, ins);
					break;
				}
				case SF_OP_ARPEGGIO: {
					sf_opcode_arpeggio(s, v, ins);
					break;
				}
				case SF_OP_PHASING: {
					sf_opcode_phasing(s, v, ins);
					break;
				}
				case SF_OP_PORTAMENTO: {
					sf_opcode_portamento(s, v, ins);
					break;
				}
				case SF_OP_TREMOLO: {
					sf_opcode_tremolo(s, v, ins);
					break;
				}
				case SF_OP_FILTER: {
					sf_opcode_filter(s, v, ins);
					break;
				}
				case SF_OP_STOP_AND_PAUSE: {
					sf_opcode_stop_and_pause(s, v, chan);
					opcode = 0;
					break;
				}
				case SF_OP_LED: {
					sf_opcode_led(s, v);
					break;
				}
				case SF_OP_WAIT_FOR_REQUEST: {
					if(!sf_opcode_wait_for_request(s, v)) {
						opcode = 0;
					}
					break;
				}
				case SF_OP_SET_TRANSPOSE: {
					sf_opcode_set_transpose(s, v);
					break;
				}
				default: {
					break;
				}
			}
		}

		if((opcode & 0x7f) == 0) {
			keep_parsing = 0;
		}
	}
}

// [=]===^=[ sf_play_voice ]======================================================================[=]
static void sf_play_voice(struct soundfactory_state *s, struct sf_voice *v, int32_t chan) {
	struct sf_instrument *ins = sf_sound_table_lookup(s, v->current_instrument);
	v->note_duration--;
	if(v->note_duration == 0) {
		sf_next_note(s, v, chan, ins);
	} else {
		sf_modulator(s, v, chan, ins);
	}
}

// [=]===^=[ sf_tick ]============================================================================[=]
static void sf_tick(struct soundfactory_state *s) {
	sf_handle_fade(s);
	// Process voices from 3 down to 0 to match NostalgicPlayer ordering
	for(int32_t i = 3; i >= 0; --i) {
		if(s->voices[i].voice_enabled) {
			sf_play_voice(s, &s->voices[i], i);
		}
	}
}

// [=]===^=[ sf_cleanup ]=========================================================================[=]
static void sf_cleanup(struct soundfactory_state *s) {
	if(!s) {
		return;
	}
	free(s->instruments); s->instruments = 0;
	free(s->sound_table); s->sound_table = 0;
}

// [=]===^=[ soundfactory_init ]==================================================================[=]
static struct soundfactory_state *soundfactory_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 276 || sample_rate < 8000) {
		return 0;
	}
	if(!sf_identify((uint8_t *)data, len)) {
		return 0;
	}

	struct soundfactory_state *s = (struct soundfactory_state *)calloc(1, sizeof(struct soundfactory_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;

	s->sound_table = (int32_t *)calloc(SF_SOUND_TABLE_LEN, sizeof(int32_t));
	if(!s->sound_table) {
		free(s);
		return 0;
	}

	if(!sf_load(s)) {
		sf_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, SF_TICK_HZ);
	sf_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ soundfactory_free ]==================================================================[=]
static void soundfactory_free(struct soundfactory_state *s) {
	if(!s) {
		return;
	}
	sf_cleanup(s);
	free(s);
}

// [=]===^=[ soundfactory_get_audio ]=============================================================[=]
static void soundfactory_get_audio(struct soundfactory_state *s, int16_t *output, int32_t frames) {
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
			sf_tick(s);
		}
	}
}

// [=]===^=[ soundfactory_api_init ]==============================================================[=]
static void *soundfactory_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return soundfactory_init(data, len, sample_rate);
}

// [=]===^=[ soundfactory_api_free ]==============================================================[=]
static void soundfactory_api_free(void *state) {
	soundfactory_free((struct soundfactory_state *)state);
}

// [=]===^=[ soundfactory_api_get_audio ]=========================================================[=]
static void soundfactory_api_get_audio(void *state, int16_t *output, int32_t frames) {
	soundfactory_get_audio((struct soundfactory_state *)state, output, frames);
}

static const char *soundfactory_extensions[] = { "psf", 0 };

static struct player_api soundfactory_api = {
	"Sound Factory",
	soundfactory_extensions,
	soundfactory_api_init,
	soundfactory_api_free,
	soundfactory_api_get_audio,
	0,
};
