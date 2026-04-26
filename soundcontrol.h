// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Sound Control replayer (3.x / 4.0 / 5.0), ported from NostalgicPlayer's C#
// implementation. Drives a 4-channel Amiga Paula (see paula.h). 50 Hz PAL tick.
//
// Public API:
//   struct soundcontrol_state *soundcontrol_init(void *data, uint32_t len, int32_t sample_rate);
//   void soundcontrol_free(struct soundcontrol_state *s);
//   void soundcontrol_get_audio(struct soundcontrol_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define SC_TICK_HZ              50
#define SC_NUM_TRACKS           256
#define SC_NUM_SAMPLES          256
#define SC_NUM_INSTRUMENTS      256
#define SC_REPEAT_LIST_SIZE     71
#define SC_REPEAT_STACK_SIZE    32

// Module type
#define SC_TYPE_UNKNOWN         0
#define SC_TYPE_3X              1
#define SC_TYPE_40              2
#define SC_TYPE_50              3

// Sample command opcodes (low 5 bits)
#define SC_CMD_STOP                       0x00
#define SC_CMD_SWITCH_SAMPLE              0x01
#define SC_CMD_WAIT                       0x02
#define SC_CMD_CHANGE_ADDRESS             0x03
#define SC_CMD_SWITCH_SAMPLE_AND_ADDRESS  0x04
#define SC_CMD_CHANGE_LENGTH              0x05
#define SC_CMD_SWITCH_SAMPLE_AND_LENGTH   0x06
#define SC_CMD_CHANGE_PERIOD              0x07
#define SC_CMD_TRANSPOSE                  0x08
#define SC_CMD_CHANGE_VOLUME              0x09
#define SC_CMD_SET_LIST_REPEAT            0x0a
#define SC_CMD_DO_LIST_REPEAT             0x0b
#define SC_CMD_CHANGE_LIST_REPEAT_VALUE   0x0c
#define SC_CMD_SET_LIST_REPEAT_VALUE      0x0d
#define SC_CMD_PLAY_SAMPLE                0x0f

// Envelope phase
#define SC_ENV_ATTACK   0
#define SC_ENV_DECAY    1
#define SC_ENV_SUSTAIN  2
#define SC_ENV_RELEASE  3
#define SC_ENV_DONE     4

// PlaySample sub-command
#define SC_PLAY_MUTE    0
#define SC_PLAY_PLAY    1

struct sc_sample {
	int8_t *data;             // points into module buffer
	uint32_t data_length;
	uint16_t length;          // 16-bit "word" length used by player (not bytes)
	uint16_t loop_start;
	uint16_t loop_end;
	int16_t note_transpose;
};

struct sc_sample_command {
	uint16_t command;         // raw 16-bit BE value (high bits encode arg-source flags)
	uint16_t arg1;
	uint16_t arg2;
};

struct sc_envelope {
	uint8_t attack_speed;
	uint8_t attack_increment;
	uint8_t decay_speed;
	uint8_t decay_decrement;
	uint16_t decay_value;
	uint8_t release_speed;
	uint8_t release_decrement;
	uint8_t valid;
};

struct sc_instrument {
	struct sc_envelope envelope;
	struct sc_sample_command *sample_commands;
	uint32_t num_sample_commands;
	uint8_t valid;
};

struct sc_track {
	uint8_t *data;
	uint32_t length;
	uint8_t valid;
};

struct sc_voice_3x {
	uint16_t wait_counter;
	uint8_t *track;
	uint32_t track_length;
	uint32_t track_position;
	int8_t transpose;
};

struct sc_voice_4x {
	uint16_t wait_counter;
	uint8_t *track;
	uint32_t track_length;
	uint32_t track_position;

	int16_t transpose;
	uint16_t transposed_note;
	uint16_t sample_transposed_note;
	uint16_t period;

	uint16_t instrument_number;
	uint16_t sample_command_wait_counter;
	struct sc_sample_command *sample_command_list;
	uint32_t sample_command_list_length;
	uint32_t sample_command_position;
	uint8_t play_sample_command;

	uint16_t sample_number;
	struct sc_sample *sample;
	uint16_t sample_length;
	int8_t *sample_data;
	uint32_t sample_data_length;

	uint16_t volume;

	uint32_t repeat_stack[SC_REPEAT_STACK_SIZE];
	uint32_t repeat_stack_top;
	uint16_t repeat_list_values[SC_REPEAT_LIST_SIZE];

	uint8_t envelope_command;
	uint16_t envelope_counter;
	int16_t envelope_volume;
	uint8_t start_envelope_release;

	int8_t *hardware_sample_data;
	uint32_t hardware_sample_data_length;
	uint32_t hardware_start_offset;
	uint16_t hardware_sample_length;
};

struct soundcontrol_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint32_t module_type;
	uint16_t default_speed;

	struct sc_track tracks[SC_NUM_TRACKS];
	uint32_t num_used_tracks;

	struct sc_sample samples[SC_NUM_SAMPLES];
	uint32_t num_samples;

	struct sc_instrument instruments[SC_NUM_INSTRUMENTS];
	uint32_t num_instruments;

	// position_list[voice][position] -> track number
	uint8_t *position_list[6];
	uint32_t num_positions;

	uint16_t periods[8 * 16];

	// 3.x specific
	uint8_t is_version_32;
	uint16_t max_speed_counter;
	uint16_t song_position_3x;
	uint16_t start_position_3x;
	uint16_t end_position_3x;
	uint16_t speed_counter_3x;
	uint16_t speed_counter2_3x;
	struct sc_voice_3x voices_3x[6];

	// 4.0 / 5.0 specific
	uint16_t song_position_4x;
	uint16_t start_position_4x;
	uint16_t end_position_4x;
	uint16_t speed_4x;
	uint16_t max_speed_4x;
	uint16_t speed_counter_4x;
	uint16_t channel_counter_4x;
	struct sc_voice_4x voices_4x[6];

	uint8_t end_reached;
};

// [=]===^=[ sc_read_u16_be ]=====================================================================[=]
static uint16_t sc_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ sc_read_u32_be ]=====================================================================[=]
static uint32_t sc_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// Tables.cs: BasePeriod (16 entries; last 3 are zeros).
// [=]===^=[ sc_base_period ]=====================================================================[=]
static uint16_t sc_base_period[16] = {
	0xd600, 0xca00, 0xbe80, 0xb400, 0xa980, 0xa000, 0x9700, 0x8e80,
	0x8680, 0x7f00, 0x7800, 0x7100, 0x6b00, 0x0000, 0x0000, 0x0000
};

// [=]===^=[ sc_calculate_period_table ]==========================================================[=]
static void sc_calculate_period_table(struct soundcontrol_state *s) {
	uint32_t index = 0;
	for(uint32_t i = 2; i < 10; ++i) {
		for(uint32_t j = 0; j < 16; ++j) {
			s->periods[index++] = (uint16_t)(sc_base_period[j] >> i);
		}
	}
}

// [=]===^=[ sc_identify ]========================================================================[=]
// Mirrors SoundControlIdentifier.TestModule. Returns SC_TYPE_* for the variant.
static uint32_t sc_identify(uint8_t *data, uint32_t len) {
	if(len < 576) {
		return SC_TYPE_UNKNOWN;
	}

	uint32_t offset = sc_read_u32_be(data + 16);
	if((offset >= 0x8000) || (offset & 0x1) != 0) {
		return SC_TYPE_UNKNOWN;
	}

	offset += 64 - 2;
	if(offset + 6 > len) {
		return SC_TYPE_UNKNOWN;
	}

	if(sc_read_u16_be(data + offset) != 0xffff) {
		return SC_TYPE_UNKNOWN;
	}
	if(sc_read_u32_be(data + offset + 2) != 0x00000400) {
		return SC_TYPE_UNKNOWN;
	}

	if(28 + 6 > len) {
		return SC_TYPE_UNKNOWN;
	}
	uint32_t off2 = sc_read_u32_be(data + 28);
	uint16_t version = sc_read_u16_be(data + 32);

	if((version == 2) && (off2 == 0)) {
		return SC_TYPE_3X;
	}
	if((version == 3) && (off2 != 0)) {
		// 4.0 vs 5.0 distinction in NostalgicPlayer relies on a hard-coded
		// length table for known 4.0 modules. Without that database we treat
		// version 3 as the more general 5.0 variant; speed comes from the
		// header so playback still works for both subtypes.
		return SC_TYPE_50;
	}

	return SC_TYPE_UNKNOWN;
}

// [=]===^=[ sc_load_track ]======================================================================[=]
// Returns 1 on success, 0 on out-of-range. Allocates and stores in s->tracks[idx].
static int32_t sc_load_track(struct soundcontrol_state *s, uint32_t base, uint32_t track_offset, uint32_t idx) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = base + track_offset;

	// Skip 16-byte track name
	if(pos + 16 > len) {
		return 0;
	}
	pos += 16;

	uint32_t start = pos;
	for(;;) {
		if(pos + 2 > len) {
			return 0;
		}
		uint8_t dat1 = data[pos];
		pos += 2;
		if(dat1 == 0xff) {
			break;
		}
		if(pos + 2 > len) {
			return 0;
		}
		pos += 2;
	}

	uint32_t row_bytes = pos - start;
	// Pad with extra zero bytes so trackPosition += 4 past the 0xff terminator
	// stays inside an allocated buffer (it is only read again after a reset).
	uint32_t alloc = row_bytes + 4;
	uint8_t *track = (uint8_t *)calloc(alloc, 1);
	if(!track) {
		return 0;
	}
	memcpy(track, data + start, row_bytes);
	s->tracks[idx].data = track;
	s->tracks[idx].length = row_bytes;
	s->tracks[idx].valid = 1;
	return 1;
}

// [=]===^=[ sc_load_tracks ]=====================================================================[=]
static int32_t sc_load_tracks(struct soundcontrol_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	if(64 + 512 > len) {
		return 0;
	}
	uint32_t last = 0;
	for(uint32_t i = 0; i < SC_NUM_TRACKS; ++i) {
		uint16_t off = sc_read_u16_be(data + 64 + i * 2);
		if(off == 0) {
			continue;
		}
		if(!sc_load_track(s, 64, off, i)) {
			return 0;
		}
		last = i;
	}
	s->num_used_tracks = last + 1;
	return 1;
}

// [=]===^=[ sc_load_position_list ]==============================================================[=]
static int32_t sc_load_position_list(struct soundcontrol_state *s, uint32_t start_offset, uint32_t length) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	uint32_t entries = length / 12;
	if(start_offset + entries * 12 > len) {
		return 0;
	}

	for(uint32_t i = 0; i < 6; ++i) {
		s->position_list[i] = (uint8_t *)calloc(entries ? entries : 1, 1);
		if(!s->position_list[i]) {
			return 0;
		}
	}
	s->num_positions = entries;

	uint32_t pos = start_offset;
	for(uint32_t i = 0; i < entries; ++i) {
		for(uint32_t j = 0; j < 6; ++j) {
			s->position_list[j][i] = data[pos];
			pos += 2;
		}
	}
	return 1;
}

// [=]===^=[ sc_load_envelope ]===================================================================[=]
static int32_t sc_load_envelope(struct soundcontrol_state *s, uint32_t pos, struct sc_envelope *env) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	if(pos + 8 > len) {
		return 0;
	}
	env->attack_speed      = data[pos + 0];
	env->attack_increment  = data[pos + 1];
	env->decay_speed       = data[pos + 2];
	env->decay_decrement   = data[pos + 3];
	env->decay_value       = sc_read_u16_be(data + pos + 4);
	env->release_speed     = data[pos + 6];
	env->release_decrement = data[pos + 7];
	env->valid             = 1;
	return 1;
}

// [=]===^=[ sc_load_instruments ]================================================================[=]
static int32_t sc_load_instruments(struct soundcontrol_state *s, uint32_t start_offset, uint32_t instruments_length) {
	if(instruments_length == 0) {
		return 1;
	}
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	if(start_offset + 512 > len) {
		return 0;
	}

	uint32_t last = 0;
	for(uint32_t i = 0; i < SC_NUM_INSTRUMENTS; ++i) {
		uint16_t off = sc_read_u16_be(data + start_offset + i * 2);
		if(off == 0) {
			continue;
		}

		uint32_t pos = start_offset + off;
		// Skip 16-byte name
		if(pos + 16 + 2 > len) {
			return 0;
		}
		pos += 16;
		uint16_t cmd_length = sc_read_u16_be(data + pos);
		pos += 2;

		struct sc_instrument *ins = &s->instruments[i];
		if(!sc_load_envelope(s, pos, &ins->envelope)) {
			return 0;
		}
		pos += 8;

		// Skip 22 bytes of unused data
		if(pos + 22 > len) {
			return 0;
		}
		pos += 22;

		uint32_t num_cmds = cmd_length / 6;
		if(pos + num_cmds * 6 > len) {
			return 0;
		}
		ins->sample_commands = (struct sc_sample_command *)calloc(num_cmds ? num_cmds : 1, sizeof(struct sc_sample_command));
		if(!ins->sample_commands) {
			return 0;
		}
		ins->num_sample_commands = num_cmds;
		for(uint32_t c = 0; c < num_cmds; ++c) {
			ins->sample_commands[c].command = sc_read_u16_be(data + pos);     pos += 2;
			ins->sample_commands[c].arg1    = sc_read_u16_be(data + pos);     pos += 2;
			ins->sample_commands[c].arg2    = sc_read_u16_be(data + pos);     pos += 2;
		}
		ins->valid = 1;
		last = i;
	}
	s->num_instruments = last + 1;
	return 1;
}

// [=]===^=[ sc_load_sample ]=====================================================================[=]
static int32_t sc_load_sample(struct soundcontrol_state *s, uint32_t pos, struct sc_sample *sample) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	// 16-byte name
	if(pos + 16 > len) {
		return 0;
	}
	pos += 16;

	if(pos + 6 > len) {
		return 0;
	}
	sample->length     = sc_read_u16_be(data + pos + 0);
	sample->loop_start = sc_read_u16_be(data + pos + 2);
	sample->loop_end   = sc_read_u16_be(data + pos + 4);
	pos += 6;

	if(pos + 20 + 2 > len) {
		return 0;
	}
	pos += 20;
	sample->note_transpose = (int16_t)sc_read_u16_be(data + pos);
	pos += 2;

	if(pos + 16 + 4 > len) {
		return 0;
	}
	pos += 16;
	uint32_t real_length = sc_read_u32_be(data + pos);
	pos += 4;
	if(real_length < 64) {
		return 0;
	}
	real_length -= 64;
	if(pos + real_length > len) {
		return 0;
	}
	sample->data = (int8_t *)(data + pos);
	sample->data_length = real_length;
	return 1;
}

// [=]===^=[ sc_load_samples ]====================================================================[=]
static int32_t sc_load_samples(struct soundcontrol_state *s, uint32_t start_offset) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	if(start_offset + 1024 > len) {
		return 0;
	}

	uint32_t last = 0;
	for(uint32_t i = 0; i < SC_NUM_SAMPLES; ++i) {
		uint32_t off = sc_read_u32_be(data + start_offset + i * 4);
		if(off == 0) {
			continue;
		}
		if(!sc_load_sample(s, start_offset + off, &s->samples[i])) {
			return 0;
		}
		last = i;
	}
	s->num_samples = last + 1;
	return 1;
}

// [=]===^=[ sc_cleanup ]=========================================================================[=]
static void sc_cleanup(struct soundcontrol_state *s) {
	if(!s) {
		return;
	}
	for(uint32_t i = 0; i < SC_NUM_TRACKS; ++i) {
		free(s->tracks[i].data);
		s->tracks[i].data = 0;
	}
	for(uint32_t i = 0; i < SC_NUM_INSTRUMENTS; ++i) {
		free(s->instruments[i].sample_commands);
		s->instruments[i].sample_commands = 0;
	}
	for(uint32_t i = 0; i < 6; ++i) {
		free(s->position_list[i]);
		s->position_list[i] = 0;
	}
}

// [=]===^=[ sc_set_tracks_3x ]===================================================================[=]
static void sc_set_tracks_3x(struct soundcontrol_state *s) {
	uint16_t pos = s->song_position_3x;
	for(uint32_t i = 0; i < 6; ++i) {
		struct sc_voice_3x *v = &s->voices_3x[i];
		v->wait_counter = 0;
		uint8_t track_no = 0;
		if(pos < s->num_positions && s->position_list[i]) {
			track_no = s->position_list[i][pos];
		}
		v->track = s->tracks[track_no].data;
		v->track_length = s->tracks[track_no].length;
		v->track_position = 0;
	}
}

// [=]===^=[ sc_set_tracks_4x ]===================================================================[=]
static void sc_set_tracks_4x(struct soundcontrol_state *s) {
	uint16_t pos = s->song_position_4x;
	for(uint32_t i = 0; i < 6; ++i) {
		struct sc_voice_4x *v = &s->voices_4x[i];
		v->wait_counter = 0;
		uint8_t track_no = 0;
		if(pos < s->num_positions && s->position_list[i]) {
			track_no = s->position_list[i][pos];
		}
		v->track = s->tracks[track_no].data;
		v->track_length = s->tracks[track_no].length;
		v->track_position = 0;
	}
}

// [=]===^=[ sc_handle_transpose_3x ]=============================================================[=]
static uint8_t sc_handle_transpose_3x(int8_t transpose, uint8_t note) {
	while(transpose != 0) {
		if(transpose > 0) {
			transpose--;
			note++;
			if((note & 0x0f) == 12) {
				note = (uint8_t)(note + 4);
			}
		} else {
			transpose++;
			note--;
			if((note & 0x0f) == 15) {
				note = (uint8_t)(note - 4);
			}
		}
	}
	return note;
}

// [=]===^=[ sc_play_sample_3x ]==================================================================[=]
static void sc_play_sample_3x(struct soundcontrol_state *s, int32_t channel, uint8_t sample_number, uint8_t note, uint8_t volume) {
	if(sample_number >= s->num_samples) {
		return;
	}
	struct sc_sample *sample = &s->samples[sample_number];

	if(sample->note_transpose != 0) {
		for(int32_t i = 0; i < sample->note_transpose; ++i) {
			note++;
			if((note & 0x0f) == 12) {
				note = (uint8_t)(note + 4);
			}
		}
	}

	if(note >= 8 * 16) {
		note = 8 * 16 - 1;
	}
	uint16_t period = s->periods[note];
	paula_set_period(&s->paula, channel, period);
	paula_set_volume(&s->paula, channel, volume);
	paula_play_sample(&s->paula, channel, sample->data, (uint32_t)sample->length);

	if(sample->loop_start != 0) {
		paula_set_loop(&s->paula, channel, sample->loop_start, (uint32_t)(sample->loop_end - sample->loop_start));
	} else {
		paula_set_loop(&s->paula, channel, 0, 0);
	}
}

// [=]===^=[ sc_process_voice6_3x ]===============================================================[=]
static void sc_process_voice6_3x(struct soundcontrol_state *s) {
	struct sc_voice_3x *v = &s->voices_3x[5];
	uint8_t *track = v->track;
	if(!track) {
		return;
	}
	uint32_t tp = v->track_position;
	if(track[tp] == 0xff) {
		return;
	}

	if(v->wait_counter == 0) {
		v->wait_counter = (uint16_t)(track[tp + 1] - 1);
		v->track_position += 4;

		if(track[tp] != 0x00) {
			uint8_t dat1 = (uint8_t)(track[tp + 2] - 1);
			uint16_t dat2 = (uint16_t)(track[tp + 3] & 0x3f);
			if((dat2 & 0x40) != 0) {
				dat2 |= 0xff10;
			}
			if(dat1 == 4) {
				s->voices_3x[0].transpose = (int8_t)dat2;
				s->voices_3x[1].transpose = (int8_t)dat2;
				s->voices_3x[2].transpose = (int8_t)dat2;
				s->voices_3x[3].transpose = (int8_t)dat2;
			}
			uint32_t index = (uint32_t)((~(dat1 & 3)) & 3);
			s->voices_3x[index].transpose = (int8_t)dat2;
		}
	} else {
		v->wait_counter--;
	}
}

// [=]===^=[ sc_process_track_3x ]================================================================[=]
static void sc_process_track_3x(struct soundcontrol_state *s) {
	uint8_t redo;
	do {
		redo = 0;
		if(s->is_version_32) {
			sc_process_voice6_3x(s);
		}
		for(uint32_t i = 0; i < 4; ++i) {
			struct sc_voice_3x *v = &s->voices_3x[i];
			if(!v->track) {
				continue;
			}
			if(v->wait_counter == 0) {
				uint8_t *track = v->track;
				uint32_t tp = v->track_position;
				v->wait_counter = (uint16_t)(track[tp + 1] - 1);
				v->track_position += 4;

				if(track[tp] == 0xff) {
					s->song_position_3x++;
					if(s->song_position_3x == s->end_position_3x) {
						s->song_position_3x = s->start_position_3x;
						s->end_reached = 1;
					}
					sc_set_tracks_3x(s);
					redo = 1;
					break;
				}
				if(track[tp] != 0x00) {
					uint8_t note = track[tp];
					uint8_t sample_number = track[tp + 2];
					uint8_t volume = track[tp + 3];
					if(s->is_version_32) {
						note = sc_handle_transpose_3x(v->transpose, note);
						if(sample_number == 0xff) {
							sample_number = 1;
							volume = 0;
							v->wait_counter = 0;
						}
					}
					if(volume != 0x80) {
						sc_play_sample_3x(s, (int32_t)i, sample_number, note, volume);
					}
				}
			} else {
				v->wait_counter--;
			}
		}
	} while(redo);
}

// [=]===^=[ sc_process_counter_3x ]==============================================================[=]
static void sc_process_counter_3x(struct soundcontrol_state *s) {
	s->speed_counter2_3x++;
	if(s->speed_counter2_3x == 2) {
		sc_process_track_3x(s);
		s->speed_counter2_3x = 0;
	}
}

// [=]===^=[ sc_play_3x ]=========================================================================[=]
static void sc_play_3x(struct soundcontrol_state *s) {
	s->end_reached = 0;
	s->speed_counter_3x++;
	if(s->speed_counter_3x == s->max_speed_counter) {
		sc_process_counter_3x(s);
		s->speed_counter_3x = 0;
	}
	sc_process_counter_3x(s);
}

// [=]===^=[ sc_new_position_4x ]=================================================================[=]
static void sc_new_position_4x(struct soundcontrol_state *s) {
	s->song_position_4x++;
	if(s->song_position_4x == s->end_position_4x) {
		s->song_position_4x = s->start_position_4x;
		s->end_reached = 1;
	}
	sc_set_tracks_4x(s);
}

// [=]===^=[ sc_setup_voice_4x ]==================================================================[=]
static void sc_setup_voice_4x(struct soundcontrol_state *s, struct sc_voice_4x *v, uint16_t note, uint16_t instrument_number, uint16_t volume) {
	v->instrument_number = instrument_number;
	v->sample_command_list = 0;
	v->sample_command_list_length = 0;
	if(instrument_number < s->num_instruments && s->instruments[instrument_number].valid) {
		v->sample_command_list = s->instruments[instrument_number].sample_commands;
		v->sample_command_list_length = s->instruments[instrument_number].num_sample_commands;
	}
	v->sample_command_wait_counter = 0;
	v->sample_command_position = 0;
	v->play_sample_command = SC_PLAY_MUTE;
	v->volume = volume;
	v->repeat_stack_top = 0;
	v->transposed_note = (uint16_t)((note & 0x0f) + (((note & 0xf0) >> 2) * 3) + v->transpose);
	v->envelope_volume = 0;
	v->envelope_counter = 1;
	v->envelope_command = SC_ENV_ATTACK;
	v->start_envelope_release = 0;
}

// [=]===^=[ sc_process_voice6_4x ]===============================================================[=]
static void sc_process_voice6_4x(struct soundcontrol_state *s) {
	struct sc_voice_4x *v = &s->voices_4x[5];
	uint8_t *track = v->track;
	if(!track) {
		return;
	}
	uint32_t tp = v->track_position;

	if(v->wait_counter == 0) {
		v->wait_counter = (uint16_t)(track[tp + 1] - 1);
		v->track_position += 4;

		if(track[tp] != 0x00) {
			if(track[tp] == 0xff) {
				v->track_position -= 4;
				return;
			}
			uint8_t dat1 = (uint8_t)(track[tp + 2] - 1);
			uint16_t dat2 = (uint16_t)(track[tp + 3] & 0x3f);
			if((dat2 & 0x40) != 0) {
				dat2 |= 0xff10;
			}
			if(dat1 == 4) {
				s->voices_4x[0].transpose = (int16_t)dat2;
				s->voices_4x[1].transpose = (int16_t)dat2;
				s->voices_4x[2].transpose = (int16_t)dat2;
				s->voices_4x[3].transpose = (int16_t)dat2;
			}
			s->voices_4x[dat1 & 3].transpose = (int16_t)(int8_t)dat2;
		}
	} else {
		v->wait_counter--;
	}
}

// [=]===^=[ sc_process_track_4x ]================================================================[=]
static void sc_process_track_4x(struct soundcontrol_state *s) {
	uint8_t redo;
	do {
		redo = 0;
		sc_process_voice6_4x(s);

		for(uint32_t i = 0; i < 4; ++i) {
			struct sc_voice_4x *v = &s->voices_4x[i];
			if(!v->track) {
				continue;
			}
			if(v->wait_counter == 0) {
				uint8_t *track = v->track;
				uint32_t tp = v->track_position;
				v->wait_counter = (uint16_t)(track[tp + 1] - 1);
				v->track_position += 4;

				if(track[tp] == 0xff) {
					sc_new_position_4x(s);
					redo = 1;
					break;
				}
				uint8_t note = track[tp];
				if(note == 0) {
					continue;
				}
				uint8_t instrument_number = track[tp + 2];
				if(instrument_number == 0xff) {
					sc_new_position_4x(s);
					redo = 1;
					break;
				}
				uint8_t volume = track[tp + 3];
				if(volume > 64) {
					volume = 64;
				}
				sc_setup_voice_4x(s, v, note, instrument_number, volume);
			} else {
				v->wait_counter--;
			}
		}
	} while(redo);
}

// [=]===^=[ sc_process_counter_4x ]==============================================================[=]
static void sc_process_counter_4x(struct soundcontrol_state *s) {
	s->speed_counter_4x = (uint16_t)(s->speed_counter_4x + s->speed_4x);
	while(s->speed_counter_4x > s->max_speed_4x) {
		s->speed_counter_4x = (uint16_t)(s->speed_counter_4x - s->max_speed_4x);
		if(s->module_type == SC_TYPE_50) {
			s->channel_counter_4x++;
			if((s->channel_counter_4x & 3) != 0) {
				continue;
			}
		}
		sc_process_track_4x(s);
	}
}

// [=]===^=[ sc_handle_sample_commands_voice ]====================================================[=]
static void sc_handle_sample_commands_voice(struct soundcontrol_state *s, struct sc_voice_4x *v, int32_t channel) {
	if(!v->sample_command_list) {
		return;
	}
	for(;;) {
		if(v->sample_command_wait_counter != 0) {
			v->sample_command_wait_counter--;
			break;
		}
		if(v->sample_command_position >= v->sample_command_list_length) {
			break;
		}
		struct sc_sample_command *sci = &v->sample_command_list[v->sample_command_position++];
		uint16_t raw_cmd = sci->command;
		uint16_t arg1 = sci->arg1;
		uint16_t arg2 = sci->arg2;
		if((raw_cmd & 0x4000) != 0) {
			if(arg1 < SC_REPEAT_LIST_SIZE) {
				arg1 = v->repeat_list_values[arg1];
			}
		}
		if((raw_cmd & 0x8000) != 0) {
			if(arg2 < SC_REPEAT_LIST_SIZE) {
				arg2 = v->repeat_list_values[arg2];
			}
		}
		uint16_t command = (uint16_t)(raw_cmd & 0x1f);

		switch(command) {
			case SC_CMD_STOP: {
				v->sample_command_position--;
				v->sample_command_wait_counter = 1;
				break;
			}

			case SC_CMD_SWITCH_SAMPLE: {
				v->sample_number = arg1;
				if(arg1 < s->num_samples) {
					struct sc_sample *sample = &s->samples[arg1];
					v->sample = sample;
					v->sample_length = sample->length;
					v->sample_data = sample->data;
					v->sample_data_length = sample->data_length;
					v->sample_transposed_note = (uint16_t)(v->transposed_note + sample->note_transpose);
					if(v->sample_transposed_note < 8 * 16) {
						v->period = s->periods[v->sample_transposed_note];
					}
				}
				break;
			}

			case SC_CMD_WAIT: {
				v->sample_command_wait_counter = arg1;
				break;
			}

			case SC_CMD_CHANGE_ADDRESS: {
				if(v->sample) {
					v->hardware_sample_data = v->sample->data;
					v->hardware_sample_data_length = v->sample->data_length;
					v->hardware_start_offset = arg1;
				}
				break;
			}

			case SC_CMD_SWITCH_SAMPLE_AND_ADDRESS: {
				v->sample_number = arg1;
				if(arg1 < s->num_samples) {
					struct sc_sample *sample = &s->samples[arg1];
					v->sample = sample;
					v->sample_data = sample->data;
					v->sample_data_length = sample->data_length;
					v->hardware_sample_data = v->sample_data;
					v->hardware_sample_data_length = v->sample_data_length;
					v->hardware_start_offset = 0;
				}
				break;
			}

			case SC_CMD_CHANGE_LENGTH: {
				v->sample_length = arg1;
				v->hardware_sample_length = arg1;
				break;
			}

			case SC_CMD_SWITCH_SAMPLE_AND_LENGTH: {
				v->sample_number = arg1;
				if(arg1 < s->num_samples) {
					struct sc_sample *sample = &s->samples[arg1];
					v->sample = sample;
					v->sample_length = sample->length;
					v->hardware_sample_length = v->sample_length;
				}
				break;
			}

			case SC_CMD_CHANGE_PERIOD: {
				v->period = (uint16_t)(v->period + (int16_t)arg1);
				paula_set_period(&s->paula, channel, v->period);
				break;
			}

			case SC_CMD_TRANSPOSE: {
				v->sample_transposed_note = (uint16_t)(v->sample_transposed_note + (int16_t)arg1);
				if(v->sample_transposed_note < 8 * 16) {
					v->period = s->periods[v->sample_transposed_note];
				}
				paula_set_period(&s->paula, channel, v->period);
				break;
			}

			case SC_CMD_CHANGE_VOLUME: {
				int32_t vol = (int32_t)v->volume + (int16_t)arg1;
				if(vol < 0) {
					vol = 0;
				} else if(vol > 64) {
					vol = 64;
				}
				v->volume = (uint16_t)vol;
				break;
			}

			case SC_CMD_SET_LIST_REPEAT: {
				if(v->repeat_stack_top < SC_REPEAT_STACK_SIZE) {
					v->repeat_stack[v->repeat_stack_top++] = v->sample_command_position - 1;
				}
				if(arg1 < SC_REPEAT_LIST_SIZE) {
					v->repeat_list_values[arg1] = arg2;
				}
				break;
			}

			case SC_CMD_DO_LIST_REPEAT: {
				int16_t arg2_signed = (int16_t)arg2;
				if(v->repeat_stack_top == 0) {
					break;
				}
				uint32_t repeat_pos = v->repeat_stack[v->repeat_stack_top - 1];
				if(repeat_pos >= v->sample_command_list_length) {
					break;
				}
				struct sc_sample_command *repeat_cmd = &v->sample_command_list[repeat_pos];
				uint16_t key = repeat_cmd->arg1;
				if(key >= SC_REPEAT_LIST_SIZE) {
					break;
				}
				v->repeat_list_values[key] = (uint16_t)(v->repeat_list_values[key] + arg2_signed);
				if(arg2_signed < 0) {
					if(v->repeat_list_values[key] > arg1) {
						v->sample_command_position = repeat_pos + 1;
					} else {
						v->repeat_stack_top--;
					}
				} else {
					if(v->repeat_list_values[key] < arg1) {
						v->sample_command_position = repeat_pos + 1;
					} else {
						v->repeat_stack_top--;
					}
				}
				break;
			}

			case SC_CMD_CHANGE_LIST_REPEAT_VALUE: {
				if(arg1 < SC_REPEAT_LIST_SIZE) {
					v->repeat_list_values[arg1] = (uint16_t)(v->repeat_list_values[arg1] + (int16_t)arg2);
				}
				break;
			}

			case SC_CMD_SET_LIST_REPEAT_VALUE: {
				if(arg1 < SC_REPEAT_LIST_SIZE) {
					v->repeat_list_values[arg1] = arg2;
				}
				break;
			}

			case SC_CMD_PLAY_SAMPLE: {
				if(v->play_sample_command == SC_PLAY_MUTE) {
					paula_mute(&s->paula, channel);
					v->sample_command_position--;
					v->sample_command_wait_counter = 1;
					v->play_sample_command = SC_PLAY_PLAY;
				} else {
					paula_set_period(&s->paula, channel, v->period);
					paula_play_sample(&s->paula, channel, v->sample_data, (uint32_t)v->sample_length);
					if(v->sample && v->sample->loop_end != 0) {
						v->hardware_sample_data = v->sample_data;
						v->hardware_sample_data_length = v->sample_data_length;
						v->hardware_start_offset = v->sample->loop_start;
						v->hardware_sample_length = (uint16_t)(v->sample->loop_end - v->sample->loop_start);
						paula_set_loop(&s->paula, channel, v->hardware_start_offset, (uint32_t)v->hardware_sample_length);
					} else {
						v->hardware_sample_length = 0;
						paula_set_loop(&s->paula, channel, 0, 0);
					}
					v->sample_command_wait_counter = 1;
					v->play_sample_command = SC_PLAY_MUTE;
				}
				break;
			}

			default: {
				break;
			}
		}
	}
}

// [=]===^=[ sc_handle_sample_commands_4x ]=======================================================[=]
static void sc_handle_sample_commands_4x(struct soundcontrol_state *s) {
	for(uint32_t i = 0; i < 4; ++i) {
		sc_handle_sample_commands_voice(s, &s->voices_4x[i], (int32_t)i);
	}
}

// [=]===^=[ sc_handle_envelope_4x ]==============================================================[=]
static void sc_handle_envelope_4x(struct soundcontrol_state *s) {
	for(uint32_t i = 0; i < 4; ++i) {
		struct sc_voice_4x *v = &s->voices_4x[i];

		if(v->envelope_counter == 0) {
			if(v->instrument_number < s->num_instruments && s->instruments[v->instrument_number].valid) {
				struct sc_envelope *env = &s->instruments[v->instrument_number].envelope;
				switch(v->envelope_command) {
					case SC_ENV_ATTACK: {
						v->envelope_volume = (int16_t)(v->envelope_volume + env->attack_increment);
						if(v->envelope_volume >= 256) {
							v->envelope_volume = 256;
							v->envelope_command = SC_ENV_DECAY;
						}
						v->envelope_counter = env->attack_speed;
						break;
					}

					case SC_ENV_DECAY: {
						v->envelope_volume = (int16_t)(v->envelope_volume - env->decay_decrement);
						if(v->envelope_volume <= (int16_t)env->decay_value) {
							v->envelope_volume = (int16_t)env->decay_value;
							v->envelope_command = SC_ENV_SUSTAIN;
						}
						v->envelope_counter = env->decay_speed;
						break;
					}

					case SC_ENV_SUSTAIN: {
						if(v->start_envelope_release) {
							v->envelope_command = SC_ENV_RELEASE;
						}
						break;
					}

					case SC_ENV_RELEASE: {
						v->envelope_volume = (int16_t)(v->envelope_volume - env->release_decrement);
						if(v->envelope_volume <= 0) {
							v->envelope_volume = 0;
							v->envelope_command = SC_ENV_DONE;
						}
						v->envelope_counter = env->release_speed;
						break;
					}

					default: {
						break;
					}
				}
			}
		} else {
			v->envelope_counter--;
		}

		int32_t volume = ((int32_t)v->volume * v->envelope_volume) / 256;
		if(volume < 0) {
			volume = 0;
		} else if(volume > 64) {
			volume = 64;
		}
		paula_set_volume(&s->paula, (int32_t)i, (uint16_t)volume);
	}
}

// [=]===^=[ sc_play_4x ]=========================================================================[=]
static void sc_play_4x(struct soundcontrol_state *s) {
	s->end_reached = 0;
	sc_process_counter_4x(s);
	sc_handle_sample_commands_4x(s);
	sc_handle_envelope_4x(s);

	for(uint32_t i = 0; i < 4; ++i) {
		struct sc_voice_4x *v = &s->voices_4x[i];
		if(v->hardware_sample_data) {
			uint32_t length = v->hardware_sample_length;
			if((v->hardware_start_offset + length) > v->hardware_sample_data_length) {
				if(v->hardware_start_offset > v->hardware_sample_data_length) {
					length = 0;
				} else {
					length = v->hardware_sample_data_length - v->hardware_start_offset;
				}
			}
			if(length != 0) {
				paula_queue_sample(&s->paula, (int32_t)i, v->hardware_sample_data, v->hardware_start_offset, length);
				paula_set_loop(&s->paula, (int32_t)i, v->hardware_start_offset, length);
			}
		}
	}
}

// [=]===^=[ sc_init_sound_3x ]===================================================================[=]
static void sc_init_sound_3x(struct soundcontrol_state *s) {
	s->start_position_3x = 0;
	s->end_position_3x = (uint16_t)s->num_positions;
	s->song_position_3x = s->start_position_3x;
	s->speed_counter_3x = 0;
	s->speed_counter2_3x = 0;
	memset(s->voices_3x, 0, sizeof(s->voices_3x));
	sc_set_tracks_3x(s);
}

// [=]===^=[ sc_init_sound_4x ]===================================================================[=]
static void sc_init_sound_4x(struct soundcontrol_state *s) {
	s->start_position_4x = 0;
	s->end_position_4x = (uint16_t)s->num_positions;
	s->song_position_4x = s->start_position_4x;
	s->speed_4x = (s->default_speed != 0) ? s->default_speed : 1;
	s->max_speed_4x = (s->module_type == SC_TYPE_40) ? 187 : 46;
	s->speed_counter_4x = 0;
	s->channel_counter_4x = 0;
	memset(s->voices_4x, 0, sizeof(s->voices_4x));
	sc_set_tracks_4x(s);
}

// [=]===^=[ sc_tick ]============================================================================[=]
static void sc_tick(struct soundcontrol_state *s) {
	if(s->module_type == SC_TYPE_3X) {
		sc_play_3x(s);
	} else {
		sc_play_4x(s);
	}
}

// [=]===^=[ soundcontrol_init ]==================================================================[=]
static struct soundcontrol_state *soundcontrol_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 64 || sample_rate < 8000) {
		return 0;
	}

	uint32_t module_type = sc_identify((uint8_t *)data, len);
	if(module_type == SC_TYPE_UNKNOWN) {
		return 0;
	}

	struct soundcontrol_state *s = (struct soundcontrol_state *)calloc(1, sizeof(struct soundcontrol_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;
	s->module_type = module_type;

	uint8_t *header = (uint8_t *)data;
	uint32_t tracks_len    = sc_read_u32_be(header + 16);
	uint32_t samples_len   = sc_read_u32_be(header + 20);
	uint32_t positions_len = sc_read_u32_be(header + 24);
	uint32_t cmds_len      = sc_read_u32_be(header + 28);
	s->default_speed = sc_read_u16_be(header + 34);

	if(64 + tracks_len + samples_len + positions_len + cmds_len > len) {
		sc_cleanup(s);
		free(s);
		return 0;
	}

	if(!sc_load_tracks(s)) {
		sc_cleanup(s);
		free(s);
		return 0;
	}
	if(!sc_load_samples(s, 64 + tracks_len)) {
		sc_cleanup(s);
		free(s);
		return 0;
	}
	if(!sc_load_position_list(s, 64 + tracks_len + samples_len, positions_len)) {
		sc_cleanup(s);
		free(s);
		return 0;
	}
	if(!sc_load_instruments(s, 64 + tracks_len + samples_len + positions_len, cmds_len)) {
		sc_cleanup(s);
		free(s);
		return 0;
	}

	sc_calculate_period_table(s);

	if(module_type == SC_TYPE_3X) {
		s->is_version_32 = 0;
		s->max_speed_counter = s->is_version_32 ? 2 : 3;
		sc_init_sound_3x(s);
	} else {
		sc_init_sound_4x(s);
	}

	paula_init(&s->paula, sample_rate, SC_TICK_HZ);
	return s;
}

// [=]===^=[ soundcontrol_free ]==================================================================[=]
static void soundcontrol_free(struct soundcontrol_state *s) {
	if(!s) {
		return;
	}
	sc_cleanup(s);
	free(s);
}

// [=]===^=[ soundcontrol_get_audio ]=============================================================[=]
static void soundcontrol_get_audio(struct soundcontrol_state *s, int16_t *output, int32_t frames) {
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
			sc_tick(s);
		}
	}
}

// [=]===^=[ soundcontrol_api_init ]==============================================================[=]
static void *soundcontrol_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return soundcontrol_init(data, len, sample_rate);
}

// [=]===^=[ soundcontrol_api_free ]==============================================================[=]
static void soundcontrol_api_free(void *state) {
	soundcontrol_free((struct soundcontrol_state *)state);
}

// [=]===^=[ soundcontrol_api_get_audio ]=========================================================[=]
static void soundcontrol_api_get_audio(void *state, int16_t *output, int32_t frames) {
	soundcontrol_get_audio((struct soundcontrol_state *)state, output, frames);
}

static const char *soundcontrol_extensions[] = { "sc", "sct", 0 };

static struct player_api soundcontrol_api = {
	"Sound Control",
	soundcontrol_extensions,
	soundcontrol_api_init,
	soundcontrol_api_free,
	soundcontrol_api_get_audio,
	0,
};
