// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// IFF SMUS replayer, ported from NostalgicPlayer's C# implementation.
// Drives Amiga Paula (see paula.h). 50Hz PAL tick rate.
//
// IFF SMUS modules reference external instrument files (.instr / .ss).
// Since this single-buffer port has no access to external files, all
// instrument slots are left empty: the score engine still runs and
// updates timing/tempo, but no audio is produced. Hosts that wish to
// add instrument loading can populate state->instruments[] after init.
//
// Public API:
//   struct iffsmus_state *iffsmus_init(void *data, uint32_t len, int32_t sample_rate);
//   void iffsmus_free(struct iffsmus_state *s);
//   void iffsmus_get_audio(struct iffsmus_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define IFFSMUS_TICK_HZ        50
#define IFFSMUS_MAX_CHANNELS   PAULA_NUM_CHANNELS
#define IFFSMUS_MAX_INSTR      256

// Event type codes (note 0..127, then control codes)
#define IFFSMUS_EVT_FIRST_NOTE 0
#define IFFSMUS_EVT_LAST_NOTE  127
#define IFFSMUS_EVT_REST       128
#define IFFSMUS_EVT_INSTRUMENT 129
#define IFFSMUS_EVT_TIME_SIG   130
#define IFFSMUS_EVT_KEY_SIG    131
#define IFFSMUS_EVT_VOLUME     132
#define IFFSMUS_EVT_MIDI_CHNL  133
#define IFFSMUS_EVT_MIDI_PRESET 134
#define IFFSMUS_EVT_CLEF       135
#define IFFSMUS_EVT_TEMPO      136
#define IFFSMUS_EVT_MARK       255

// Voice status
#define IFFSMUS_VS_SILENCE  0
#define IFFSMUS_VS_PLAYING  1
#define IFFSMUS_VS_STOPPING 2

// Instrument setup sequence
#define IFFSMUS_IS_NOTHING     0
#define IFFSMUS_IS_INITIALIZE  1
#define IFFSMUS_IS_RELEASENOTE 2
#define IFFSMUS_IS_MUTE        3

struct iffsmus_event {
	uint8_t type;
	uint8_t data;
};

struct iffsmus_track {
	struct iffsmus_event *events;
	uint32_t num_events;
};

struct iffsmus_track_info {
	uint8_t instrument_number;
	uint8_t time_left;
};

// Reserved for host-supplied instrument data. The built-in port never
// populates these; format remains 0 (none) so the score plays silently.
struct iffsmus_instrument {
	uint8_t format;        // 0 = none, others reserved for host extension
	void *format_data;
};

struct iffsmus_voice {
	uint8_t status;
	uint8_t setup_sequence;
	int16_t instrument_number;
	uint8_t note;
	uint16_t volume;
};

struct iffsmus_state {
	struct paula paula;

	// Header / metadata
	uint16_t tempo_index;
	uint8_t global_volume;
	uint8_t num_channels;
	uint16_t transpose;
	uint16_t tune;
	uint16_t time_sig_num;
	uint16_t time_sig_den;

	uint16_t track_volumes[IFFSMUS_MAX_CHANNELS];
	uint32_t tracks_enabled_init[IFFSMUS_MAX_CHANNELS];

	// Tracks
	struct iffsmus_track tracks[IFFSMUS_MAX_CHANNELS];
	uint8_t has_tracks;

	// Instruments (host-extensible)
	int32_t instrument_mapper[IFFSMUS_MAX_INSTR];
	struct iffsmus_instrument instruments[IFFSMUS_MAX_INSTR];
	uint32_t num_instruments;

	// Playing state
	int32_t start_time;
	int32_t end_time;
	uint32_t tracks_enabled[IFFSMUS_MAX_CHANNELS];
	int32_t track_start_positions[IFFSMUS_MAX_CHANNELS];
	struct iffsmus_track_info tracks_info[IFFSMUS_MAX_CHANNELS];

	int16_t repeat_count;
	int32_t current_time;
	uint8_t flag;
	uint16_t speed_counter;
	uint16_t current_tempo;
	uint16_t new_tempo;
	uint16_t calculated_tempo;
	uint16_t calculated_speed;

	uint16_t max_volume;
	uint16_t new_volume;
	uint16_t current_volume;
	uint16_t volume_global;

	int16_t instrument_numbers[IFFSMUS_MAX_CHANNELS];
	int32_t current_instruments[IFFSMUS_MAX_CHANNELS];   // index into instruments[] or -1
	uint32_t hold_note_counters[IFFSMUS_MAX_CHANNELS];
	uint32_t release_note_counters[IFFSMUS_MAX_CHANNELS];
	int32_t current_track_positions[IFFSMUS_MAX_CHANNELS];

	struct iffsmus_voice voices[IFFSMUS_MAX_CHANNELS];

	uint8_t loaded_ok;
};

// [=]===^=[ iffsmus_tempo_table ]================================================================[=]
static uint16_t iffsmus_tempo_table[128] = {
	0xfa83, 0xf525, 0xefe4, 0xeac0, 0xe5b9, 0xe0cc, 0xdbfb, 0xd744,
	0xd2a8, 0xce24, 0xc9b9, 0xc567, 0xc12c, 0xbd08, 0xb8fb, 0xb504,
	0xb123, 0xad58, 0xa9a1, 0xa5fe, 0xa270, 0x9ef5, 0x9b8d, 0x9837,
	0x94f4, 0x91c3, 0x8ea4, 0x8b95, 0x8898, 0x85aa, 0x82cd, 0x8000,
	0x7d41, 0x7a92, 0x77f2, 0x7560, 0x72dc, 0x7066, 0x6dfd, 0x6ba2,
	0x6954, 0x6712, 0x64dc, 0x62b3, 0x6096, 0x5e84, 0x5c7d, 0x5a82,
	0x5891, 0x56ac, 0x54d0, 0x52ff, 0x5138, 0x4f7a, 0x4dc6, 0x4c1b,
	0x4a7a, 0x48e1, 0x4752, 0x45ca, 0x444c, 0x42d5, 0x4166, 0x4000,
	0x3ea0, 0x3d49, 0x3bf9, 0x3ab0, 0x396e, 0x3833, 0x36fe, 0x35d1,
	0x34aa, 0x3389, 0x326e, 0x3159, 0x304b, 0x2f42, 0x2e3e, 0x2d41,
	0x2c48, 0x2b56, 0x2a68, 0x297f, 0x289c, 0x27bd, 0x26e3, 0x260d,
	0x253d, 0x2470, 0x23a9, 0x22e5, 0x2226, 0x216a, 0x20b3, 0x2000,
	0x1f50, 0x1ea4, 0x1dfc, 0x1d58, 0x1cb7, 0x1c19, 0x1b7f, 0x1ae8,
	0x1a55, 0x19c4, 0x1937, 0x18ac, 0x1825, 0x17a1, 0x171f, 0x16a0,
	0x1624, 0x15ab, 0x1534, 0x14bf, 0x144e, 0x13de, 0x1371, 0x1306,
	0x129e, 0x1238, 0x11d4, 0x1172, 0x1113, 0x10b5, 0x1059, 0x1000,
};

// [=]===^=[ iffsmus_duration_table ]=============================================================[=]
static int8_t iffsmus_duration_table[16] = {
	32, 16, 8, 4, 2, -1, -1, -1,
	48, 24, 12,  6, 3, -1, -1, -1,
};

// [=]===^=[ iffsmus_read_be32 ]==================================================================[=]
static uint32_t iffsmus_read_be32(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ iffsmus_read_be16 ]==================================================================[=]
static uint16_t iffsmus_read_be16(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ iffsmus_check_mark ]=================================================================[=]
static int32_t iffsmus_check_mark(uint8_t *p, const char *mark) {
	return (p[0] == (uint8_t)mark[0]) && (p[1] == (uint8_t)mark[1]) && (p[2] == (uint8_t)mark[2]) && (p[3] == (uint8_t)mark[3]);
}

// [=]===^=[ iffsmus_parse_shdr ]=================================================================[=]
static void iffsmus_parse_shdr(struct iffsmus_state *s, uint8_t *p, uint32_t size) {
	if(size < 4) {
		return;
	}
	uint16_t tempo = iffsmus_read_be16(p);
	if(tempo >= 0xe11) {
		uint32_t find_tempo = 0xe100000u / (uint32_t)tempo;
		uint32_t i;
		for(i = 0; i < 128; ++i) {
			if(find_tempo >= (uint32_t)iffsmus_tempo_table[i]) {
				break;
			}
		}
		if(i == 128) {
			i--;
		}
		s->tempo_index = (uint16_t)i;
	} else {
		s->tempo_index = 0;
	}
	uint8_t gv = p[2];
	if(gv < 128) {
		gv = (uint8_t)(gv * 2);
	}
	s->global_volume = gv;
	s->num_channels = p[3];
	if(s->num_channels > IFFSMUS_MAX_CHANNELS) {
		s->num_channels = IFFSMUS_MAX_CHANNELS;
	}
	for(uint32_t i = 0; i < s->num_channels; ++i) {
		s->track_volumes[i] = 0xff;
		s->tracks_enabled_init[i] = 1;
	}
}

// [=]===^=[ iffsmus_parse_ins1 ]=================================================================[=]
static void iffsmus_parse_ins1(struct iffsmus_state *s, uint8_t *p, uint32_t size) {
	if(size < 4) {
		return;
	}
	uint8_t reg = p[0];
	uint8_t type = p[1];
	if(s->instrument_mapper[reg] != 0) {
		return;
	}
	if(type != 0) {
		return; // MIDI instruments not supported
	}
	// data1, data2 are at p[2], p[3]; instrument name follows but we don't load externals
	// Reserve a slot in instruments[] so register mapping resolves; format stays 0.
	if(s->num_instruments < IFFSMUS_MAX_INSTR) {
		uint32_t idx = s->num_instruments++;
		s->instruments[idx].format = 0;
		s->instruments[idx].format_data = 0;
		s->instrument_mapper[reg] = (int32_t)idx + 1;
	}
}

// [=]===^=[ iffsmus_parse_trak ]=================================================================[=]
static int32_t iffsmus_parse_trak(struct iffsmus_state *s, uint8_t *p, uint32_t size, uint32_t *track_number) {
	if(s->num_channels == 0) {
		return 0;
	}
	if(*track_number >= s->num_channels) {
		return 1;
	}
	uint32_t num_events = size / 2;
	struct iffsmus_event *events = (struct iffsmus_event *)malloc(sizeof(struct iffsmus_event) * (num_events + 1));
	if(!events) {
		return 0;
	}
	uint32_t out_count = 0;
	for(uint32_t i = 0; i < num_events; ++i) {
		uint8_t type = p[i * 2];
		uint8_t data = p[i * 2 + 1];
		if(type == IFFSMUS_EVT_MARK) {
			break;
		}
		if((type <= IFFSMUS_EVT_LAST_NOTE) || (type == IFFSMUS_EVT_REST)) {
			data &= 0x0f;
			int8_t new_data = iffsmus_duration_table[data];
			if(new_data < 0) {
				continue;
			}
			data = (uint8_t)new_data;
		} else if(type == IFFSMUS_EVT_INSTRUMENT) {
			// pass through
		} else if(type == IFFSMUS_EVT_TIME_SIG) {
			s->time_sig_num = (uint16_t)(((data >> 3) & 0x1f) + 1);
			s->time_sig_den = (uint16_t)(1 << (data & 0x07));
			continue;
		} else if(type == IFFSMUS_EVT_VOLUME) {
			s->track_volumes[*track_number] = (uint16_t)((data & 0x7f) * 2);
			continue;
		} else {
			continue;
		}
		events[out_count].type = type;
		events[out_count].data = data;
		out_count++;
	}
	events[out_count].type = IFFSMUS_EVT_MARK;
	events[out_count].data = 0xff;
	out_count++;
	s->tracks[*track_number].events = events;
	s->tracks[*track_number].num_events = out_count;
	(*track_number)++;
	s->has_tracks = 1;
	return 1;
}

// [=]===^=[ iffsmus_parse_snx ]==================================================================[=]
static void iffsmus_parse_snx(struct iffsmus_state *s, uint8_t *p, uint32_t size) {
	if(s->num_channels == 0 || size < 8) {
		return;
	}
	s->transpose = iffsmus_read_be16(p);
	s->tune = iffsmus_read_be16(p + 2);
	uint32_t off = 8;
	for(uint32_t i = 0; i < s->num_channels; ++i) {
		if(off + 4 > size) {
			break;
		}
		s->tracks_enabled_init[i] = iffsmus_read_be32(p + off);
		off += 4;
	}
}

// [=]===^=[ iffsmus_load ]=======================================================================[=]
static int32_t iffsmus_load(struct iffsmus_state *s, uint8_t *data, uint32_t len) {
	if(len < 12) {
		return 0;
	}
	if(!iffsmus_check_mark(data, "FORM")) {
		return 0;
	}
	if(!iffsmus_check_mark(data + 8, "SMUS")) {
		return 0;
	}
	uint32_t total_size = iffsmus_read_be32(data + 4);
	if(total_size < 4) {
		return 0;
	}
	total_size -= 4;
	uint32_t pos = 12;
	uint32_t track_number = 0;
	while(total_size > 0 && pos + 8 <= len) {
		uint8_t *chunk_name = data + pos;
		uint32_t chunk_size = iffsmus_read_be32(data + pos + 4);
		pos += 8;
		if(total_size < chunk_size + 8) {
			break;
		}
		total_size -= (chunk_size + 8);
		if(chunk_size > (len - pos)) {
			return 0;
		}
		uint8_t *chunk_data = data + pos;
		if(iffsmus_check_mark(chunk_name, "SHDR")) {
			iffsmus_parse_shdr(s, chunk_data, chunk_size);
		} else if(iffsmus_check_mark(chunk_name, "INS1")) {
			iffsmus_parse_ins1(s, chunk_data, chunk_size);
		} else if(iffsmus_check_mark(chunk_name, "TRAK")) {
			if(!iffsmus_parse_trak(s, chunk_data, chunk_size, &track_number)) {
				return 0;
			}
		} else if(chunk_name[0] == 'S' && chunk_name[1] == 'N' && chunk_name[2] == 'X' && (chunk_name[3] >= '1' && chunk_name[3] <= '9')) {
			iffsmus_parse_snx(s, chunk_data, chunk_size);
		}
		pos += chunk_size;
		if((chunk_size & 1) != 0) {
			pos++;
			if(total_size > 0) {
				total_size--;
			}
		}
	}
	if(s->num_channels == 0 || !s->has_tracks) {
		return 0;
	}
	return 1;
}

// [=]===^=[ iffsmus_begin_release_voice ]========================================================[=]
static void iffsmus_begin_release_voice(struct iffsmus_state *s, uint32_t ch) {
	struct iffsmus_voice *v = &s->voices[ch];
	v->setup_sequence = IFFSMUS_IS_NOTHING;
	if(v->status == IFFSMUS_VS_PLAYING) {
		v->setup_sequence = IFFSMUS_IS_RELEASENOTE;
	}
}

// [=]===^=[ iffsmus_release_all_voices ]=========================================================[=]
static void iffsmus_release_all_voices(struct iffsmus_state *s) {
	if(s->repeat_count != 0) {
		s->repeat_count = 0;
		for(uint32_t i = 0; i < s->num_channels; ++i) {
			if(s->tracks_enabled[i] != 0) {
				iffsmus_begin_release_voice(s, i);
			}
		}
	}
}

// [=]===^=[ iffsmus_initialize_tracks ]==========================================================[=]
static void iffsmus_initialize_tracks(struct iffsmus_state *s, int32_t start_time) {
	for(uint32_t i = 0; i < s->num_channels; ++i) {
		s->track_start_positions[i] = -1;
		s->tracks_info[i].instrument_number = 0;
		s->tracks_info[i].time_left = 0;
		if(s->tracks[i].events != 0) {
			struct iffsmus_track *track = &s->tracks[i];
			int32_t time_left = start_time;
			uint32_t event_pos = 0;
			while(time_left > 0 && event_pos < track->num_events) {
				struct iffsmus_event *e = &track->events[event_pos++];
				if(e->type == IFFSMUS_EVT_MARK) {
					break;
				}
				if((e->type <= IFFSMUS_EVT_LAST_NOTE) || (e->type == IFFSMUS_EVT_REST)) {
					time_left -= (int32_t)e->data;
					if(time_left < 0) {
						s->tracks_info[i].time_left = (uint8_t)(-time_left);
					}
				} else if(e->type == IFFSMUS_EVT_INSTRUMENT) {
					s->tracks_info[i].instrument_number = (uint8_t)(e->data + 1);
				}
			}
			s->track_start_positions[i] = (int32_t)event_pos;
		}
	}
}

// [=]===^=[ iffsmus_start_module_init_instruments ]==============================================[=]
static void iffsmus_start_module_init_instruments(struct iffsmus_state *s) {
	for(uint32_t i = 0; i < s->num_channels; ++i) {
		s->current_track_positions[i] = s->track_start_positions[i];
	}
	s->current_time = s->start_time;
	for(uint32_t i = 0; i < s->num_channels; ++i) {
		if(s->tracks_enabled[i] != 0) {
			iffsmus_begin_release_voice(s, i);
		}
		s->hold_note_counters[i] = 0;
		s->release_note_counters[i] = (uint32_t)s->tracks_info[i].time_left + 1u;
		uint8_t instr_num = s->tracks_info[i].instrument_number;
		s->current_instruments[i] = -1;
		if(instr_num != 0) {
			int32_t mapped = s->instrument_mapper[instr_num - 1];
			if(mapped != 0) {
				s->current_instruments[i] = mapped - 1;
				s->instrument_numbers[i] = (int16_t)(mapped - 1);
			}
		}
	}
}

// [=]===^=[ iffsmus_set_volume_internal ]========================================================[=]
static void iffsmus_set_volume_internal(struct iffsmus_state *s, uint16_t scale, uint16_t new_volume) {
	s->new_volume = 0;
	if(scale != 0) {
		s->max_volume = (uint16_t)(s->current_volume * 256);
		s->volume_global = (uint16_t)(new_volume * 256);
		int32_t volume = (int32_t)s->max_volume - (int32_t)s->volume_global;
		if(volume < 0) {
			volume = -volume;
		}
		volume /= scale;
		if(volume == 0) {
			volume++;
		}
		s->new_volume = (uint16_t)volume;
	} else {
		s->current_volume = new_volume;
	}
}

// [=]===^=[ iffsmus_initialize_sound ]===========================================================[=]
static void iffsmus_initialize_sound(struct iffsmus_state *s) {
	for(uint32_t i = 0; i < s->num_channels; ++i) {
		s->voices[i].status = IFFSMUS_VS_SILENCE;
		s->voices[i].setup_sequence = IFFSMUS_IS_NOTHING;
		s->voices[i].instrument_number = 0;
		s->voices[i].note = 0;
		s->voices[i].volume = 0;
	}
	s->flag &= 0xfe;
	iffsmus_release_all_voices(s);
	s->start_time = 0;
	s->end_time = -1;
	for(uint32_t i = 0; i < s->num_channels; ++i) {
		s->tracks_enabled[i] = s->tracks_enabled_init[i];
	}
	iffsmus_initialize_tracks(s, s->start_time);
	iffsmus_start_module_init_instruments(s);
	s->speed_counter = 0;
	s->current_volume = 0;
	s->current_tempo = 0;
	s->new_tempo = s->tempo_index;
	s->tune = s->tune;
	iffsmus_set_volume_internal(s, 1, s->global_volume);
	s->repeat_count = -1;
}

// [=]===^=[ iffsmus_get_next_note ]==============================================================[=]
static void iffsmus_get_next_note(struct iffsmus_state *s) {
	if(s->speed_counter != 0) {
		s->speed_counter--;
	}
	if(s->speed_counter != 0) {
		return;
	}
	if(s->current_tempo != s->new_tempo) {
		s->current_tempo = s->new_tempo;
		uint16_t tempo = iffsmus_tempo_table[s->current_tempo];
		s->calculated_speed = (uint16_t)(tempo >> 12);
		if(s->calculated_speed == 0) {
			s->calculated_speed = 1;
		}
		s->calculated_tempo = (uint16_t)(((uint32_t)tempo << 15) / ((uint32_t)s->calculated_speed << 12));
	}
	if(s->repeat_count == 0) {
		return;
	}
	s->speed_counter = s->calculated_speed;
	uint8_t begin_over;
	do {
		begin_over = 0;
		uint32_t tracks_done = 0;
		for(uint32_t i = 0; i < s->num_channels; ++i) {
			if(s->hold_note_counters[i] != 0) {
				s->hold_note_counters[i]--;
				if(s->hold_note_counters[i] == 0 && s->tracks_enabled[i] != 0) {
					iffsmus_begin_release_voice(s, i);
				}
				continue;
			}
			if(s->release_note_counters[i] != 0) {
				s->release_note_counters[i]--;
				if(s->release_note_counters[i] != 0) {
					continue;
				}
			}
			if(s->current_track_positions[i] == -1) {
				tracks_done++;
				continue;
			}
			struct iffsmus_track *track = &s->tracks[i];
			uint8_t one_more;
			do {
				one_more = 0;
				if((uint32_t)s->current_track_positions[i] >= track->num_events) {
					tracks_done++;
					break;
				}
				struct iffsmus_event *e = &track->events[s->current_track_positions[i]];
				if(e->type == IFFSMUS_EVT_MARK) {
					tracks_done++;
					break;
				}
				s->current_track_positions[i]++;
				if(e->type <= IFFSMUS_EVT_LAST_NOTE) {
					uint16_t duration = e->data;
					if(s->tracks_enabled[i] != 0) {
						int32_t instr_idx = s->current_instruments[i];
						if(instr_idx >= 0) {
							uint16_t note = (uint16_t)((uint16_t)e->type + (s->transpose / 16) - 8);
							uint16_t vol = s->track_volumes[i];
							if(s->tracks_enabled[i] != 1) {
								vol /= 2;
							}
							struct iffsmus_voice *v = &s->voices[i];
							v->instrument_number = s->instrument_numbers[i];
							v->note = (uint8_t)note;
							v->volume = (uint16_t)(vol & 0xff);
							v->setup_sequence = IFFSMUS_IS_INITIALIZE;
							uint16_t temp = (uint16_t)(((uint32_t)duration * 0xc000u) >> 16);
							s->hold_note_counters[i] = temp;
							duration -= temp;
						}
					}
					s->release_note_counters[i] = duration;
				} else if(e->type == IFFSMUS_EVT_REST) {
					s->release_note_counters[i] = e->data;
				} else if(e->type == IFFSMUS_EVT_INSTRUMENT) {
					int32_t mapped = s->instrument_mapper[e->data];
					if(mapped != 0) {
						s->current_instruments[i] = mapped - 1;
						s->instrument_numbers[i] = (int16_t)(mapped - 1);
					}
					one_more = 1;
				} else {
					one_more = 1;
				}
			} while(one_more);
		}
		int32_t current_time = s->current_time++;
		uint8_t end_reached = 0;
		if(s->end_time < 0) {
			if(tracks_done == s->num_channels) {
				end_reached = 1;
			}
		} else if(current_time == s->end_time) {
			end_reached = 1;
		}
		if(end_reached) {
			uint8_t restart = 0;
			if(s->repeat_count < 0) {
				restart = 1;
			} else {
				s->repeat_count--;
				if(s->repeat_count != 0) {
					restart = 1;
				}
			}
			if(restart) {
				iffsmus_start_module_init_instruments(s);
				begin_over = 1;
			}
		}
	} while(begin_over);
}

// [=]===^=[ iffsmus_find_volume ]================================================================[=]
static void iffsmus_find_volume(struct iffsmus_state *s) {
	if(s->new_volume == 0) {
		return;
	}
	int32_t temp1 = (int32_t)s->new_volume * (int32_t)s->calculated_tempo;
	if(temp1 >= 0) {
		temp1 >>= 15;
		temp1 &= 0xffff;
		int32_t temp2 = temp1;
		int32_t new_vol = (int32_t)s->volume_global - (int32_t)s->max_volume;
		if(new_vol < 0) {
			temp2 = -temp2;
			new_vol = -new_vol;
		}
		if(temp1 < new_vol) {
			temp2 += (int32_t)s->max_volume;
			s->max_volume = (uint16_t)temp2;
			s->current_volume = (uint16_t)(temp2 / 256);
			return;
		}
	}
	if(s->volume_global == 0) {
		uint8_t is_set = (s->flag & 0x01) != 0;
		s->flag &= 0xfe;
		if(is_set) {
			iffsmus_release_all_voices(s);
		}
	}
	s->new_volume = 0;
	s->current_volume = (uint16_t)(s->volume_global / 256);
}

// [=]===^=[ iffsmus_setup_and_play_instruments ]=================================================[=]
// Without external instrument data we have no sample data to drive Paula.
// Voice setup_sequence transitions are still tracked so engine state stays
// consistent if a host extends the player with custom instrument formats.
static void iffsmus_setup_and_play_instruments(struct iffsmus_state *s) {
	for(uint32_t i = 0; i < s->num_channels; ++i) {
		struct iffsmus_voice *v = &s->voices[i];
		int32_t instr_idx = s->current_instruments[i];
		uint8_t has_format = 0;
		if(instr_idx >= 0 && (uint32_t)instr_idx < s->num_instruments) {
			has_format = (s->instruments[instr_idx].format != 0);
		}
		if(has_format) {
			if(v->setup_sequence == IFFSMUS_IS_INITIALIZE) {
				v->status = IFFSMUS_VS_PLAYING;
			} else if(v->setup_sequence == IFFSMUS_IS_RELEASENOTE) {
				v->status = IFFSMUS_VS_STOPPING;
			} else if(v->setup_sequence == IFFSMUS_IS_MUTE) {
				v->status = IFFSMUS_VS_SILENCE;
				paula_mute(&s->paula, (int32_t)i);
			}
		} else {
			v->status = IFFSMUS_VS_SILENCE;
			if(v->setup_sequence == IFFSMUS_IS_MUTE || v->setup_sequence == IFFSMUS_IS_RELEASENOTE) {
				paula_mute(&s->paula, (int32_t)i);
			}
		}
		v->setup_sequence = IFFSMUS_IS_NOTHING;
	}
}

// [=]===^=[ iffsmus_tick ]=======================================================================[=]
static void iffsmus_tick(struct iffsmus_state *s) {
	iffsmus_find_volume(s);
	iffsmus_get_next_note(s);
	iffsmus_setup_and_play_instruments(s);
}

// [=]===^=[ iffsmus_cleanup ]====================================================================[=]
static void iffsmus_cleanup(struct iffsmus_state *s) {
	for(uint32_t i = 0; i < IFFSMUS_MAX_CHANNELS; ++i) {
		if(s->tracks[i].events) {
			free(s->tracks[i].events);
			s->tracks[i].events = 0;
			s->tracks[i].num_events = 0;
		}
	}
}

// [=]===^=[ iffsmus_init ]=======================================================================[=]
static struct iffsmus_state *iffsmus_init(void *data, uint32_t len, int32_t sample_rate) {
	struct iffsmus_state *s = (struct iffsmus_state *)malloc(sizeof(*s));
	if(!s) {
		return 0;
	}
	memset(s, 0, sizeof(*s));
	s->tune = 0x80;
	s->new_tempo = 64;
	s->current_volume = 0xffff;
	s->calculated_tempo = 0x8000;
	if(!iffsmus_load(s, (uint8_t *)data, len)) {
		iffsmus_cleanup(s);
		free(s);
		return 0;
	}
	s->loaded_ok = 1;
	paula_init(&s->paula, sample_rate, IFFSMUS_TICK_HZ);
	iffsmus_initialize_sound(s);
	return s;
}

// [=]===^=[ iffsmus_free ]=======================================================================[=]
static void iffsmus_free(struct iffsmus_state *s) {
	if(!s) {
		return;
	}
	iffsmus_cleanup(s);
	free(s);
}

// [=]===^=[ iffsmus_get_audio ]==================================================================[=]
static void iffsmus_get_audio(struct iffsmus_state *s, int16_t *output, int32_t frames) {
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
			iffsmus_tick(s);
		}
	}
}

// [=]===^=[ iffsmus_api_init ]===================================================================[=]
static void *iffsmus_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return iffsmus_init(data, len, sample_rate);
}

// [=]===^=[ iffsmus_api_free ]===================================================================[=]
static void iffsmus_api_free(void *state) {
	iffsmus_free((struct iffsmus_state *)state);
}

// [=]===^=[ iffsmus_api_get_audio ]==============================================================[=]
static void iffsmus_api_get_audio(void *state, int16_t *output, int32_t frames) {
	iffsmus_get_audio((struct iffsmus_state *)state, output, frames);
}

static const char *iffsmus_extensions[] = { "smus", 0 };

static struct player_api iffsmus_api = {
	"IFF SMUS",
	iffsmus_extensions,
	iffsmus_api_init,
	iffsmus_api_free,
	iffsmus_api_get_audio,
	0,
};
