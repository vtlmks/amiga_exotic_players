// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// IFF SMUS replayer, ported from NostalgicPlayer's C# implementation.
// Drives Amiga Paula (see paula.h). Variable tick rate based on a CIA-timer
// reload value derived from the song's calculated tempo.
//
// IFF SMUS modules reference external instrument files (.instr / .ss). The
// init_ex entry point uses the host-supplied player_loader callback to fetch
// "Instruments/<name>.instr" (and, for SampledSound instruments, the matching
// "Instruments/<name>.ss"). Instruments come in three flavours:
//   - SampledSound: 8-bit PCM with multi-octave layout, ADSR envelope, vibrato
//   - Synthesis:    additive oscillator with LFO, envelope, phase distortion
//   - Form:         IFF 8SVX FORM/VHDR/BODY with multi-octave layout
//
// Public API:
//   struct iffsmus_state *iffsmus_init(void *data, uint32_t len, int32_t sample_rate);
//   struct iffsmus_state *iffsmus_init_ex(void *data, uint32_t len, int32_t sample_rate, struct player_loader *loader);
//   void iffsmus_free(struct iffsmus_state *s);
//   void iffsmus_get_audio(struct iffsmus_state *s, float *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define IFFSMUS_MAX_CHANNELS   PAULA_NUM_CHANNELS
#define IFFSMUS_MAX_INSTR      256
#define IFFSMUS_INSTR_NAME_LEN 64
#define IFFSMUS_CIA_FREQ       709379u            // PAL CIA-A timer base
#define IFFSMUS_CIA_DEFAULT    11932u             // ~59.4 Hz initial tick rate
#define IFFSMUS_SYNTH_BUF_LEN  256                // double-buffered synth waveform

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

// SetSample sequence (per-tick PlaySample/SetSample/SetLoop ordering)
#define IFFSMUS_SS_NOTHING     0
#define IFFSMUS_SS_START       1
#define IFFSMUS_SS_SET_LOOP    2

// Instrument format kinds
#define IFFSMUS_FMT_NONE         0
#define IFFSMUS_FMT_SAMPLEDSOUND 1
#define IFFSMUS_FMT_SYNTHESIS    2
#define IFFSMUS_FMT_FORM         3

// LFO enabled (synthesis)
#define IFFSMUS_LFO_OFF  0
#define IFFSMUS_LFO_ON   1
#define IFFSMUS_LFO_ONCE 2

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

// --- per-format instrument data --------------------------------------------

// SampledSound: per-octave sample layout. SampleData is a contiguous run
// where octave N occupies `LengthOfOctaveOne * (1 << N)` bytes starting at
// offset `(1<<N - 1<<StartOctave) * LengthOfOctaveOne` from the buffer base.
struct iffsmus_sampledsound {
	uint16_t volume;
	uint16_t envelope_levels[4];
	uint16_t envelope_rates[4];
	int16_t vibrato_depth;
	uint16_t vibrato_speed;
	uint16_t vibrato_delay;

	uint16_t length_of_octave_one;
	uint16_t loop_length_of_octave_one;
	uint8_t start_octave;
	uint8_t end_octave;
	int8_t *sample_data;
	uint32_t sample_data_len;
};

// Synthesis: oscillator + LFO + filter + ADSR. The 64x128 calculated samples
// are derived once at load time from Oscillator[128] via CalculateSamples().
struct iffsmus_synthesis {
	int8_t oscillator[128];
	int8_t lfo[256];
	uint16_t waveform;
	uint16_t wave_amt;
	uint16_t amplitude_volume;
	uint16_t amplitude_enabled;
	uint16_t amplitude_lfo;
	uint16_t frequency_port;
	uint16_t frequency_lfo;
	uint16_t filter_frequency;
	uint16_t filter_eg;
	uint16_t filter_lfo;
	uint16_t lfo_speed;
	uint16_t lfo_enabled;
	uint16_t lfo_delay;
	uint16_t phase_speed;
	uint16_t phase_depth;
	uint16_t envelope_levels[4];
	uint16_t envelope_rates[4];
	int8_t samples[64][128];
};

// Form (IFF 8SVX): one shot + repeat sample, doubled per octave.
struct iffsmus_form {
	uint32_t one_shot_hi_samples;
	uint32_t repeat_hi_samples;
	uint32_t samples_per_hi_cycle;
	uint16_t octaves;
	uint32_t volume;                  // 16.16 fixed point, scaled to 0..0xffff
	int8_t *sample_data;
	uint32_t sample_data_len;
	uint16_t hi_octaves_to_skip;
};

struct iffsmus_instrument {
	uint8_t format;                                // IFFSMUS_FMT_*
	char name[IFFSMUS_INSTR_NAME_LEN];
	struct iffsmus_sampledsound *sampledsound;
	struct iffsmus_synthesis *synthesis;
	struct iffsmus_form *form;
};

// --- per-channel play info -------------------------------------------------

struct iffsmus_sampled_play {
	int8_t *loop_sample_data;
	uint32_t loop_start;
	uint16_t loop_length_in_words;
	uint16_t mapped_note;
	uint16_t envelope_index;
	int32_t envelope_volume;
	uint16_t vibrato_index;
	uint16_t vibrato_delay_counter;
	int16_t vibrato_value;
	uint8_t octave;
	uint8_t note;
};

struct iffsmus_synth_play {
	uint16_t mapped_note;
	int16_t frequency_counter;
	int16_t frequency_speed;
	uint16_t sample_start_index;
	uint16_t playing_octave;
	uint16_t envelope_index;
	int32_t envelope_volume;
	uint16_t lfo_index;
	int16_t lfo_counter;
	int16_t lfo_value;
	int16_t phase_index;
	int16_t phase_direction;
	uint8_t octave;
	uint8_t note;
};

struct iffsmus_form_play {
	int8_t *loop_sample_data;
	uint32_t loop_start;
	uint16_t loop_length_in_words;
	uint16_t mapped_note;
	uint8_t volume_multiply;
	uint8_t octave;
	uint8_t note;
};

struct iffsmus_voice {
	uint8_t status;
	uint8_t setup_sequence;                        // IFFSMUS_IS_*
	uint8_t set_sample_sequence;                   // IFFSMUS_SS_*
	uint8_t format;                                // current format kind
	int16_t instrument_number;
	uint8_t note;
	uint16_t volume;
	int8_t *sample_data;
	uint32_t sample_start_offset;
	uint16_t sample_length_in_words;
	uint16_t period;
	uint16_t final_volume;
	int8_t synth_buf[IFFSMUS_SYNTH_BUF_LEN];       // double-buffered synthesis waveform
};

struct iffsmus_state {
	struct paula paula;
	int32_t sample_rate;

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

	// Instruments
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
	int32_t current_instruments[IFFSMUS_MAX_CHANNELS];
	uint32_t hold_note_counters[IFFSMUS_MAX_CHANNELS];
	uint32_t release_note_counters[IFFSMUS_MAX_CHANNELS];
	int32_t current_track_positions[IFFSMUS_MAX_CHANNELS];

	struct iffsmus_voice voices[IFFSMUS_MAX_CHANNELS];

	struct iffsmus_sampled_play sampled_play[IFFSMUS_MAX_CHANNELS];
	struct iffsmus_synth_play synth_play[IFFSMUS_MAX_CHANNELS];
	struct iffsmus_form_play form_play[IFFSMUS_MAX_CHANNELS];

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

// [=]===^=[ iffsmus_note_table ]=================================================================[=]
static uint16_t iffsmus_note_table[13] = {
	0x8000, 0x78d1, 0x7209, 0x6ba2, 0x6598, 0x5fe4, 0x5a82, 0x556e,
	0x50a3, 0x4c1c, 0x47d6, 0x43ce, 0x4000,
};

// [=]===^=[ iffsmus_x_table ]====================================================================[=]
// Synthesis oscillator filter coefficient table (one entry per 64-sample-set
// row). Used only by CalculateSamples to convolve the source oscillator into
// 64 distinct sample variations.
static uint16_t iffsmus_x_table[64] = {
	0x8000, 0x7683, 0x6dba, 0x6597, 0x5e10, 0x5717, 0x50a2, 0x4aa8,
	0x451f, 0x4000, 0x3b41, 0x36dd, 0x32cb, 0x2f08, 0x2b8b, 0x2851,
	0x2554, 0x228f, 0x2000, 0x1da0, 0x1b6e, 0x1965, 0x1784, 0x15c5,
	0x1428, 0x12aa, 0x1147, 0x1000, 0x0ed0, 0x0db7, 0x0cb2, 0x0bc2,
	0x0ae2, 0x0a14, 0x0955, 0x08a3, 0x0800, 0x0768, 0x06db, 0x0659,
	0x05e1, 0x0571, 0x050a, 0x04aa, 0x0451, 0x0400, 0x03b4, 0x036d,
	0x032c, 0x02f0, 0x02b8, 0x0285, 0x0255, 0x0228, 0x0200, 0x01da,
	0x01b6, 0x0196, 0x0178, 0x015c, 0x0142, 0x012a, 0x0114, 0x0100,
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

// [=]===^=[ iffsmus_starts_with ]================================================================[=]
// Case-insensitive ASCII prefix compare; matches NostalgicPlayer's Identify
// semantics for SampledSound / Synthesis / FORM detection.
static int32_t iffsmus_starts_with(uint8_t *buf, uint32_t buf_len, const char *prefix) {
	uint32_t i = 0;
	while(prefix[i] != 0) {
		if(i >= buf_len) {
			return 0;
		}
		uint8_t a = buf[i];
		uint8_t b = (uint8_t)prefix[i];
		if(a >= 'A' && a <= 'Z') { a = (uint8_t)(a + 32); }
		if(b >= 'A' && b <= 'Z') { b = (uint8_t)(b + 32); }
		if(a != b) {
			return 0;
		}
		i++;
	}
	return 1;
}

// [=]===^=[ iffsmus_copy_str ]===================================================================[=]
// Copy a possibly-NUL-padded fixed-width string from `src` (max `src_max` bytes
// or until first 0) into the IFFSMUS_INSTR_NAME_LEN-sized destination, always
// null-terminating.
static void iffsmus_copy_str(char *dst, uint8_t *src, uint32_t src_max) {
	uint32_t n = src_max < (IFFSMUS_INSTR_NAME_LEN - 1u) ? src_max : (IFFSMUS_INSTR_NAME_LEN - 1u);
	uint32_t i = 0;
	for(; i < n; ++i) {
		uint8_t b = src[i];
		if(b == 0) {
			break;
		}
		dst[i] = (char)b;
	}
	dst[i] = 0;
}

// [=]===^=[ iffsmus_set_cia_tempo ]==============================================================[=]
// Translate a CIA-A timer reload value to Paula samples_per_tick. PAL CIA fires
// at IFFSMUS_CIA_FREQ Hz, so tick interval = cia_tempo / 709379 seconds and
// samples_per_tick = sample_rate * cia_tempo / 709379.
static void iffsmus_set_cia_tempo(struct iffsmus_state *s, uint32_t cia_tempo) {
	if(cia_tempo == 0) {
		cia_tempo = 1;
	}
	uint64_t spt = ((uint64_t)s->sample_rate * (uint64_t)cia_tempo) / IFFSMUS_CIA_FREQ;
	if(spt < 1) {
		spt = 1;
	}
	s->paula.samples_per_tick = (int32_t)spt;
	if(s->paula.tick_offset > s->paula.samples_per_tick) {
		s->paula.tick_offset = s->paula.samples_per_tick;
	}
}

// --- chunk parsers ---------------------------------------------------------

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
// Reserves an instrument slot and saves the name; actual instrument file
// loading happens later in iffsmus_init_ex when a host-supplied loader is
// available. Without a loader the slot's format stays IFFSMUS_FMT_NONE and
// the score plays silently.
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
	if(s->num_instruments >= IFFSMUS_MAX_INSTR) {
		return;
	}
	uint32_t idx = s->num_instruments++;
	s->instruments[idx].format = IFFSMUS_FMT_NONE;
	s->instruments[idx].sampledsound = 0;
	s->instruments[idx].synthesis = 0;
	s->instruments[idx].form = 0;
	// data1 (p[2]) and data2 (p[3]) are unused; name is the rest of the chunk.
	if(size > 4) {
		iffsmus_copy_str(s->instruments[idx].name, p + 4, size - 4);
	} else {
		s->instruments[idx].name[0] = 0;
	}
	s->instrument_mapper[reg] = (int32_t)idx + 1;
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

// --- instrument loaders ----------------------------------------------------

// [=]===^=[ iffsmus_load_sampledsound ]==========================================================[=]
// Parse a "SampledSound" .instr blob into instr->sampledsound, then fetch the
// matching .ss file via the loader and populate the per-octave sample data.
// The .instr layout (from NostalgicPlayer SampledSoundFormat.Load):
//   0x00..0x0c  "SampledSound" magic (case-insensitive)
//   0x44..0x64  32-byte sample name (NUL-padded)
//   0x68        u16 BE   volume
//   0x6a..0x72  u16[4] BE envelope levels
//   0x72..0x7a  u16[4] BE envelope rates
//   0x7a        s16 BE   vibrato depth
//   0x7c        u16 BE   vibrato speed
//   0x7e        u16 BE   vibrato delay
// The .ss file layout:
//   0x00 u16 BE   length_of_octave_one
//   0x02 u16 BE   loop_length_of_octave_one
//   0x04 u8       start_octave
//   0x05 u8       end_octave
//   0x3e..        raw int8 sample data
static int32_t iffsmus_load_sampledsound(struct iffsmus_instrument *instr, uint8_t *idata, uint32_t ilen, struct player_loader *loader) {
	if(ilen < 0x80) {
		return 0;
	}
	struct iffsmus_sampledsound *fd = (struct iffsmus_sampledsound *)calloc(1, sizeof(*fd));
	if(!fd) {
		return 0;
	}
	char sample_name[33];
	uint32_t i = 0;
	for(; i < 32; ++i) {
		uint8_t b = idata[0x44 + i];
		if(b == 0) {
			break;
		}
		sample_name[i] = (char)b;
	}
	sample_name[i] = 0;

	// Read instrument metadata starting at 0x68 (0x44 + 32 bytes name + 4 bytes pointer).
	uint8_t *p = idata + 0x68;
	fd->volume = iffsmus_read_be16(p + 0);
	for(uint32_t j = 0; j < 4; ++j) {
		fd->envelope_levels[j] = iffsmus_read_be16(p + 2 + j * 2);
	}
	for(uint32_t j = 0; j < 4; ++j) {
		fd->envelope_rates[j] = iffsmus_read_be16(p + 10 + j * 2);
	}
	fd->vibrato_depth = (int16_t)iffsmus_read_be16(p + 18);
	fd->vibrato_speed = iffsmus_read_be16(p + 20);
	fd->vibrato_delay = iffsmus_read_be16(p + 22);

	// Build the sample file name: "Instruments/<sample_name>.ss"
	char path[IFFSMUS_INSTR_NAME_LEN + 32];
	uint32_t plen = 0;
	const char *prefix = "Instruments/";
	while(prefix[plen] != 0) {
		path[plen] = prefix[plen];
		plen++;
	}
	uint32_t k = 0;
	while(sample_name[k] != 0 && plen < sizeof(path) - 5) {
		path[plen++] = sample_name[k++];
	}
	path[plen++] = '.';
	path[plen++] = 's';
	path[plen++] = 's';
	path[plen] = 0;

	uint32_t slen = 0;
	uint8_t *sdata = loader->fetch(loader->ctx, path, &slen);
	if(!sdata || slen < 0x3e) {
		if(sdata) { free(sdata); }
		free(fd);
		return 0;
	}
	fd->length_of_octave_one      = iffsmus_read_be16(sdata + 0);
	fd->loop_length_of_octave_one = iffsmus_read_be16(sdata + 2);
	fd->start_octave              = sdata[4];
	fd->end_octave                = sdata[5];

	uint32_t sample_len = slen - 0x3e;
	int8_t *sbuf = (int8_t *)malloc(sample_len);
	if(!sbuf) {
		free(sdata);
		free(fd);
		return 0;
	}
	memcpy(sbuf, sdata + 0x3e, sample_len);
	free(sdata);
	fd->sample_data = sbuf;
	fd->sample_data_len = sample_len;

	instr->sampledsound = fd;
	instr->format = IFFSMUS_FMT_SAMPLEDSOUND;
	return 1;
}

// [=]===^=[ iffsmus_synth_calc_samples ]=========================================================[=]
// Pre-compute the 64x128 oscillator-derived samples used during playback. This
// is a state-machine convolution of the source oscillator with the X-table
// coefficients, run once per instrument at load time.
static void iffsmus_synth_calc_samples(struct iffsmus_synthesis *fd) {
	int16_t d4 = (int16_t)((int16_t)fd->oscillator[127] << 7);
	int16_t d3 = 0;
	for(uint32_t i = 0; i < 64; ++i) {
		int8_t *dest = fd->samples[i];
		uint16_t d1 = iffsmus_x_table[i];
		uint16_t d2 = (uint16_t)(((0x8000u - (uint32_t)d1) * 58982u) >> 16);
		d1 = (uint16_t)(d1 / 2);
		for(uint32_t j = 0; j < 128; ++j) {
			d3 = (int16_t)(d3 + (int16_t)((((int32_t)((int32_t)fd->oscillator[j] << 7) - (int32_t)d4) * (int32_t)d1) >> 14));
			d4 = (int16_t)(d4 + d3);
			int16_t sb = (int16_t)((d4 >> 7) | (d4 << 9));
			dest[j] = (int8_t)sb;
			d3 = (int16_t)(((int32_t)d3 * (int32_t)d2) >> 15);
		}
	}
}

// [=]===^=[ iffsmus_load_synthesis ]=============================================================[=]
// Synthesis instruments either start with the literal "Synthesis" magic or
// have first byte 0x00. The body layout (from NostalgicPlayer):
//   0x44..0xc4  oscillator (128 signed bytes)
//   0xc4..0x1c4 LFO (256 signed bytes)
//   0x1c4 u16 BE waveform
//   0x1c6 u32    skipped
//   0x1ca u16 BE wave_amt
//   then a long run of u16 BE control words ending in envelope_levels[4] and
//   envelope_rates[4].
static int32_t iffsmus_load_synthesis(struct iffsmus_instrument *instr, uint8_t *idata, uint32_t ilen) {
	if(ilen < 0x1f6) {
		return 0;
	}
	struct iffsmus_synthesis *fd = (struct iffsmus_synthesis *)calloc(1, sizeof(*fd));
	if(!fd) {
		return 0;
	}
	memcpy(fd->oscillator, idata + 0x44, 128);
	for(uint32_t i = 0; i < 128; ++i) {
		fd->oscillator[i] = (int8_t)idata[0x44 + i];
	}
	for(uint32_t i = 0; i < 256; ++i) {
		fd->lfo[i] = (int8_t)idata[0xc4 + i];
	}
	uint8_t *p = idata + 0x1c4;
	fd->waveform          = iffsmus_read_be16(p + 0);
	// skip 4 bytes (p+2 .. p+6)
	fd->wave_amt          = iffsmus_read_be16(p + 6);
	fd->amplitude_volume  = iffsmus_read_be16(p + 8);
	fd->amplitude_enabled = iffsmus_read_be16(p + 10);
	fd->amplitude_lfo     = iffsmus_read_be16(p + 12);
	fd->frequency_port    = iffsmus_read_be16(p + 14);
	fd->frequency_lfo     = iffsmus_read_be16(p + 16);
	fd->filter_frequency  = iffsmus_read_be16(p + 18);
	fd->filter_eg         = iffsmus_read_be16(p + 20);
	fd->filter_lfo        = iffsmus_read_be16(p + 22);
	fd->lfo_speed         = iffsmus_read_be16(p + 24);
	fd->lfo_enabled       = iffsmus_read_be16(p + 26);
	fd->lfo_delay         = iffsmus_read_be16(p + 28);
	fd->phase_speed       = iffsmus_read_be16(p + 30);
	fd->phase_depth       = iffsmus_read_be16(p + 32);
	for(uint32_t j = 0; j < 4; ++j) {
		fd->envelope_levels[j] = iffsmus_read_be16(p + 34 + j * 2);
	}
	for(uint32_t j = 0; j < 4; ++j) {
		fd->envelope_rates[j]  = iffsmus_read_be16(p + 42 + j * 2);
	}
	iffsmus_synth_calc_samples(fd);
	instr->synthesis = fd;
	instr->format = IFFSMUS_FMT_SYNTHESIS;
	return 1;
}

// [=]===^=[ iffsmus_load_form ]==================================================================[=]
// Parse an IFF FORM/8SVX instrument: VHDR provides oneShotHiSamples,
// repeatHiSamples, samplesPerHiCycle, octaves, volume; BODY is raw int8 PCM.
// See EA's 8SVX spec for chunk semantics. We also compute hi_octaves_to_skip
// per NostalgicPlayer FormFormat.Load — this packs short loops into the high
// octaves so the rendered note range matches the original instrument design.
static int32_t iffsmus_load_form(struct iffsmus_instrument *instr, uint8_t *idata, uint32_t ilen) {
	if(ilen < 12) {
		return 0;
	}
	if(!iffsmus_check_mark(idata, "FORM")) {
		return 0;
	}
	if(!iffsmus_check_mark(idata + 8, "8SVX")) {
		return 0;
	}
	uint32_t total_size = iffsmus_read_be32(idata + 4);
	if(total_size < 4 || total_size + 8 > ilen) {
		return 0;
	}
	struct iffsmus_form *fd = (struct iffsmus_form *)calloc(1, sizeof(*fd));
	if(!fd) {
		return 0;
	}
	fd->volume = 0x10000;     // 1.0 default if VHDR is absent
	uint32_t pos = 12;
	uint32_t end = total_size + 8;
	while(pos + 8 <= end) {
		uint8_t *cn = idata + pos;
		uint32_t cs = iffsmus_read_be32(idata + pos + 4);
		pos += 8;
		if(cs > end - pos) {
			free(fd);
			return 0;
		}
		uint8_t *cd = idata + pos;
		if(iffsmus_check_mark(cn, "VHDR") && cs >= 20) {
			fd->one_shot_hi_samples  = iffsmus_read_be32(cd + 0);
			fd->repeat_hi_samples    = iffsmus_read_be32(cd + 4);
			fd->samples_per_hi_cycle = iffsmus_read_be32(cd + 8);
			// samples_per_sec = u16 at cd+12 (unused; Paula plays at note pitch)
			fd->octaves              = (uint16_t)cd[14];
			// compression = cd[15]; we only support uncompressed (0)
			if(cd[15] != 0) {
				free(fd);
				return 0;
			}
			fd->volume = iffsmus_read_be32(cd + 16);
		} else if(iffsmus_check_mark(cn, "BODY")) {
			int8_t *sbuf = (int8_t *)malloc(cs);
			if(!sbuf) {
				free(fd);
				return 0;
			}
			memcpy(sbuf, cd, cs);
			fd->sample_data = sbuf;
			fd->sample_data_len = cs;
		}
		pos += cs;
		if((cs & 1) != 0) {
			pos++;
		}
	}
	if(fd->sample_data == 0 || fd->octaves == 0) {
		if(fd->sample_data) { free(fd->sample_data); }
		free(fd);
		return 0;
	}

	// hi_octaves_to_skip: pack short loops into high octaves until the loop
	// reaches a single sample per cycle or until octaves run out.
	uint32_t one_shot = fd->one_shot_hi_samples;
	uint32_t repeat = fd->repeat_hi_samples;
	uint32_t spc = fd->samples_per_hi_cycle;
	uint16_t count = 0;
	for(;;) {
		one_shot /= 2;
		repeat /= 2;
		if(spc > 1) {
			spc /= 2;
		}
		count++;
		if(((one_shot | repeat) & 1u) != 0) {
			break;
		}
		if(spc == 1) {
			break;
		}
		if((uint32_t)(count + fd->octaves) >= 8) {
			break;
		}
	}
	fd->hi_octaves_to_skip = count;

	instr->form = fd;
	instr->format = IFFSMUS_FMT_FORM;
	return 1;
}

// [=]===^=[ iffsmus_fix_filename ]===============================================================[=]
// Mirror NostalgicPlayer's FixFileName: replace filesystem-hostile characters
// in instrument names so paths line up across platforms.
static void iffsmus_fix_filename(char *dst, const char *src) {
	uint32_t i = 0;
	while(src[i] != 0 && i < IFFSMUS_INSTR_NAME_LEN - 1) {
		char c = src[i];
		if(c == '?' || c == '<' || c == '>') {
			c = '!';
		}
		dst[i] = c;
		i++;
	}
	dst[i] = 0;
}

// [=]===^=[ iffsmus_load_one_instrument ]========================================================[=]
// Resolve a single instrument by name: build "Instruments/<name>.instr",
// fetch via loader, dispatch to the matching format loader. Failure leaves
// the instrument in IFFSMUS_FMT_NONE state (silent on its tracks).
static void iffsmus_load_one_instrument(struct iffsmus_instrument *instr, struct player_loader *loader) {
	if(instr->format != IFFSMUS_FMT_NONE) {
		return;
	}
	if(instr->name[0] == 0) {
		return;
	}
	char fixed[IFFSMUS_INSTR_NAME_LEN];
	iffsmus_fix_filename(fixed, instr->name);

	char path[IFFSMUS_INSTR_NAME_LEN + 32];
	uint32_t plen = 0;
	const char *prefix = "Instruments/";
	while(prefix[plen] != 0) {
		path[plen] = prefix[plen];
		plen++;
	}
	uint32_t k = 0;
	while(fixed[k] != 0 && plen < sizeof(path) - 8) {
		path[plen++] = fixed[k++];
	}
	path[plen++] = '.';
	path[plen++] = 'i';
	path[plen++] = 'n';
	path[plen++] = 's';
	path[plen++] = 't';
	path[plen++] = 'r';
	path[plen] = 0;

	uint32_t ilen = 0;
	uint8_t *idata = loader->fetch(loader->ctx, path, &ilen);
	if(!idata || ilen < 32) {
		if(idata) { free(idata); }
		return;
	}
	if(iffsmus_starts_with(idata, ilen, "SampledSound")) {
		iffsmus_load_sampledsound(instr, idata, ilen, loader);
	} else if(iffsmus_check_mark(idata, "FORM")) {
		iffsmus_load_form(instr, idata, ilen);
	} else if(idata[0] == 0x00 || iffsmus_starts_with(idata, ilen, "Synthesis")) {
		iffsmus_load_synthesis(instr, idata, ilen);
	}
	free(idata);
}

// --- score engine ----------------------------------------------------------

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
		struct iffsmus_voice *v = &s->voices[i];
		v->status = IFFSMUS_VS_SILENCE;
		v->setup_sequence = IFFSMUS_IS_NOTHING;
		v->set_sample_sequence = IFFSMUS_SS_NOTHING;
		v->format = IFFSMUS_FMT_NONE;
		v->instrument_number = 0;
		v->note = 0;
		v->volume = 0;
		v->sample_data = 0;
		v->sample_start_offset = 0;
		v->sample_length_in_words = 0;
		v->period = 0;
		v->final_volume = 0;
		memset(v->synth_buf, 0, sizeof(v->synth_buf));
		memset(&s->sampled_play[i], 0, sizeof(s->sampled_play[i]));
		memset(&s->synth_play[i], 0, sizeof(s->synth_play[i]));
		memset(&s->form_play[i], 0, sizeof(s->form_play[i]));
		s->form_play[i].volume_multiply = 0;
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
	iffsmus_set_volume_internal(s, 1, s->global_volume);
	s->repeat_count = -1;
	iffsmus_set_cia_tempo(s, IFFSMUS_CIA_DEFAULT);
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
		uint32_t cia = ((uint32_t)s->calculated_tempo * IFFSMUS_CIA_DEFAULT) >> 15;
		iffsmus_set_cia_tempo(s, cia);
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
							struct iffsmus_instrument *instr = &s->instruments[instr_idx];
							uint16_t note = (uint16_t)((uint16_t)e->type + (s->transpose / 16) - 8);
							uint16_t vol = s->track_volumes[i];
							if(s->tracks_enabled[i] != 1) {
								vol /= 2;
							}
							struct iffsmus_voice *v = &s->voices[i];
							// If the format kind changes mid-stream, mute the
							// channel so the new format starts cleanly.
							if(v->status != IFFSMUS_VS_SILENCE && v->format != instr->format) {
								v->setup_sequence = IFFSMUS_IS_MUTE;
							}
							v->format = instr->format;
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

// --- per-format setup + play -----------------------------------------------

// [=]===^=[ iffsmus_sampled_setup ]==============================================================[=]
// Run the per-tick SampledSound state update: vibrato LFO, ADSR envelope step,
// resulting period/volume. Returns 0 if the note was rejected (out of octave
// range) and the voice should be marked as nothing-to-do.
static int32_t iffsmus_sampled_setup(struct iffsmus_state *s, uint32_t ch, struct iffsmus_instrument *instr) {
	struct iffsmus_voice *v = &s->voices[ch];
	struct iffsmus_sampled_play *pi = &s->sampled_play[ch];
	struct iffsmus_sampledsound *fd = instr->sampledsound;

	switch(v->setup_sequence) {
	case IFFSMUS_IS_INITIALIZE: {
		int32_t octave = -(((int32_t)v->note / 12) - 10);
		int32_t note = (int32_t)v->note % 12;
		if(octave > fd->end_octave || octave < fd->start_octave) {
			v->setup_sequence = IFFSMUS_IS_NOTHING;
			if(v->status == IFFSMUS_VS_SILENCE) {
				return 0;
			}
			break;
		}
		if(v->status == IFFSMUS_VS_SILENCE) {
			pi->envelope_volume = 0;
		}
		if(v->status != IFFSMUS_VS_PLAYING) {
			pi->envelope_index = 0;
		}
		pi->mapped_note = (uint16_t)(((uint32_t)iffsmus_note_table[note] * 54728u) >> 15);
		pi->octave = (uint8_t)((int32_t)v->note / 12);
		pi->note = (uint8_t)note;
		int32_t temp1 = 1 << octave;
		int32_t temp2 = 1 << fd->start_octave;
		v->sample_data = fd->sample_data;
		v->sample_start_offset = (uint32_t)((temp1 - temp2) * fd->length_of_octave_one);
		v->sample_length_in_words = (uint16_t)((fd->length_of_octave_one * temp1) / 2);
		if(v->sample_start_offset + (uint32_t)(v->sample_length_in_words * 2u) > fd->sample_data_len) {
			if(fd->sample_data_len > v->sample_start_offset) {
				v->sample_length_in_words = (uint16_t)((fd->sample_data_len - v->sample_start_offset) / 2);
			} else {
				v->sample_length_in_words = 0;
			}
		}
		if(fd->length_of_octave_one != fd->loop_length_of_octave_one) {
			pi->loop_sample_data = v->sample_data;
			pi->loop_start = (uint32_t)(v->sample_start_offset + fd->loop_length_of_octave_one * temp1);
			pi->loop_length_in_words = (uint16_t)(((fd->length_of_octave_one - fd->loop_length_of_octave_one) * temp1) / 2);
			if(pi->loop_start + (uint32_t)(pi->loop_length_in_words * 2u) > fd->sample_data_len) {
				if(fd->sample_data_len > pi->loop_start) {
					pi->loop_length_in_words = (uint16_t)((fd->sample_data_len - pi->loop_start) / 2);
				} else {
					pi->loop_length_in_words = 0;
				}
			}
		} else {
			pi->loop_sample_data = 0;
			pi->loop_start = 0;
			pi->loop_length_in_words = 0;
		}
		pi->vibrato_index = 0;
		pi->vibrato_delay_counter = (uint16_t)((((uint32_t)fd->vibrato_delay << 15) / s->calculated_tempo) / 2);
		v->set_sample_sequence = IFFSMUS_SS_START;
		paula_mute(&s->paula, (int32_t)ch);
		break;
	}
	case IFFSMUS_IS_RELEASENOTE:
		pi->envelope_index = 3;
		break;
	case IFFSMUS_IS_MUTE:
		paula_mute(&s->paula, (int32_t)ch);
		return 0;
	default:
		break;
	}

	if(pi->vibrato_delay_counter == 0) {
		pi->vibrato_index = (uint16_t)(pi->vibrato_index + (((uint32_t)fd->vibrato_speed * s->calculated_tempo) >> 9) + 64);
	} else {
		pi->vibrato_delay_counter--;
	}
	uint16_t vib_val = (uint16_t)((pi->vibrato_index >> 7) + 128);
	if((vib_val & 0x100) != 0) {
		vib_val ^= 0xff;
	}
	vib_val ^= 0x80;
	pi->vibrato_value = (int16_t)(-(int32_t)((int8_t)vib_val));

	int32_t t1 = (int32_t)((uint32_t)fd->envelope_levels[pi->envelope_index] << 16);
	int32_t t2 = pi->envelope_volume;
	int32_t t3 = (int32_t)fd->envelope_rates[pi->envelope_index];
	int32_t t0 = (t3 >> 5) ^ 7;
	t3 = (((((t3 & 0x1f) + 0x21) * (int32_t)s->calculated_tempo) << 3) >> t0);

	t0 = t1 - t2;
	if(t0 < 0) {
		t0 = -t0;
	}
	if(t0 > t3) {
		if(t2 < t1) {
			t2 += t3;
		} else {
			t2 -= t3;
		}
	} else {
		t2 = t1;
		if(pi->envelope_index < 2) {
			pi->envelope_index++;
		}
	}
	pi->envelope_volume = t2;

	v->period = (uint16_t)(((((((int32_t)fd->vibrato_depth * (int32_t)pi->vibrato_value) >> 7) - ((int32_t)s->tune - 0x80) + 0x1000) * (int32_t)pi->mapped_note) >> 0x13));
	v->final_volume = (uint16_t)(((((((((int32_t)s->current_volume + 1) * (int32_t)v->volume) / 256) + 1) * (int32_t)fd->volume) / 256) * (pi->envelope_volume >> 16)) >> 10);
	return 1;
}

// [=]===^=[ iffsmus_sampled_play ]===============================================================[=]
static void iffsmus_sampled_play(struct iffsmus_state *s, uint32_t ch) {
	struct iffsmus_voice *v = &s->voices[ch];
	struct iffsmus_sampled_play *pi = &s->sampled_play[ch];
	if(v->set_sample_sequence == IFFSMUS_SS_START) {
		paula_queue_sample(&s->paula, (int32_t)ch, v->sample_data, v->sample_start_offset, (uint32_t)v->sample_length_in_words * 2u);
		v->set_sample_sequence = IFFSMUS_SS_SET_LOOP;
	}
	if(v->set_sample_sequence == IFFSMUS_SS_SET_LOOP) {
		if(pi->loop_sample_data != 0) {
			paula_set_loop(&s->paula, (int32_t)ch, pi->loop_start, (uint32_t)pi->loop_length_in_words * 2u);
		}
		v->set_sample_sequence = IFFSMUS_SS_NOTHING;
	}
	paula_set_period(&s->paula, (int32_t)ch, v->period);
	paula_set_volume_256(&s->paula, (int32_t)ch, v->final_volume);
}

// [=]===^=[ iffsmus_synth_setup ]================================================================[=]
// Per-tick: regenerate one half of the per-voice 256-byte synth buffer using
// the current LFO/phase/envelope state and the pre-computed oscillator-derived
// samples. The double-buffer layout (sample_start_index ^= 0x80) lets Paula
// keep playing the previous half until the queued swap takes effect.
static int32_t iffsmus_synth_setup(struct iffsmus_state *s, uint32_t ch, struct iffsmus_instrument *instr) {
	struct iffsmus_voice *v = &s->voices[ch];
	struct iffsmus_synth_play *pi = &s->synth_play[ch];
	struct iffsmus_synthesis *fd = instr->synthesis;

	switch(v->setup_sequence) {
	case IFFSMUS_IS_INITIALIZE: {
		if(v->note < 36 || v->note >= 108) {
			v->setup_sequence = IFFSMUS_IS_NOTHING;
			if(v->status == IFFSMUS_VS_SILENCE) {
				return 0;
			}
			break;
		}
		int32_t note = (int32_t)v->note - 36;
		if(v->status == IFFSMUS_VS_SILENCE) {
			pi->envelope_volume = 0;
		}
		if(v->status != IFFSMUS_VS_PLAYING) {
			pi->envelope_index = 0;
		}
		int32_t octave = note / 12;
		note %= 12;
		uint16_t mapped_note = (uint16_t)(((uint32_t)iffsmus_note_table[note] * 54728u) >> (octave + 17));
		pi->octave = (uint8_t)(octave + 3);
		pi->note = (uint8_t)note;
		if(pi->mapped_note == 0) {
			pi->frequency_counter = 0;
		} else {
			int32_t difference = (int32_t)mapped_note - (int32_t)pi->mapped_note;
			pi->frequency_counter = (int16_t)(((((int32_t)fd->frequency_port << 15) / (int32_t)s->calculated_tempo) >> 3) + 1);
			pi->frequency_speed = (int16_t)(difference / pi->frequency_counter);
			mapped_note = (uint16_t)(mapped_note - (uint16_t)((int32_t)pi->frequency_speed * (int32_t)pi->frequency_counter));
		}
		pi->mapped_note = mapped_note;
		pi->phase_direction = 1;
		if(fd->phase_speed == 0) {
			pi->phase_index = 0;
		}
		pi->lfo_counter = 0;
		if(fd->lfo_enabled != IFFSMUS_LFO_OFF) {
			pi->lfo_index = 0;
			pi->lfo_counter = (int16_t)((((int32_t)fd->lfo_delay << 15) / (int32_t)s->calculated_tempo) >> 2);
			pi->lfo_value = (int16_t)fd->lfo[0];
		}
		v->set_sample_sequence = IFFSMUS_SS_START;
		break;
	}
	case IFFSMUS_IS_RELEASENOTE:
		pi->envelope_index = 3;
		break;
	case IFFSMUS_IS_MUTE:
		paula_mute(&s->paula, (int32_t)ch);
		return 0;
	default:
		break;
	}

	if(pi->lfo_counter > 0) {
		pi->lfo_counter--;
	} else if(pi->lfo_counter == 0) {
		int16_t index = (int16_t)(pi->lfo_index + (((int32_t)fd->lfo_speed * (int32_t)s->calculated_tempo) >> 10));
		if(index < 0 && fd->lfo_enabled == IFFSMUS_LFO_ONCE) {
			pi->lfo_counter = -1;
		} else {
			pi->lfo_index = (uint16_t)index;
			pi->lfo_value = (int16_t)fd->lfo[pi->lfo_index >> 8];
		}
	}

	int32_t t1 = (int32_t)((uint32_t)fd->envelope_levels[pi->envelope_index] << 16);
	int32_t t2 = pi->envelope_volume;
	int32_t t3 = (int32_t)fd->envelope_rates[pi->envelope_index];
	int32_t t0 = (t3 >> 5) ^ 7;
	t3 = (((((t3 & 0x1f) + 0x21) * (int32_t)s->calculated_tempo) << 3) >> t0);
	t0 = t1 - t2;
	if(t0 < 0) {
		t0 = -t0;
	}
	if(t0 > t3) {
		if(t2 < t1) {
			t2 += t3;
		} else {
			t2 -= t3;
		}
	} else {
		t2 = t1;
		if(pi->envelope_index < 2) {
			pi->envelope_index++;
		}
	}
	pi->envelope_volume = t2;

	uint16_t oct = 5;
	if(pi->frequency_counter != 0) {
		pi->frequency_counter--;
		pi->mapped_note = (uint16_t)(pi->mapped_note + pi->frequency_speed);
	}
	uint16_t temp_note = pi->mapped_note;
	while(temp_note > 428) {
		temp_note = (uint16_t)(temp_note / 2);
		oct--;
	}
	pi->playing_octave = oct;
	v->sample_length_in_words = (uint16_t)(64u >> oct);
	v->period = (uint16_t)(((((((int32_t)fd->frequency_lfo * (int32_t)pi->lfo_value) >> 7) - ((int32_t)s->tune - 0x80) + 0x1000) * (int32_t)temp_note) >> 12));

	uint16_t vol = (uint16_t)((((int32_t)fd->amplitude_lfo * -(int32_t)pi->lfo_value) >> 8) + (int32_t)fd->amplitude_volume);
	if(fd->amplitude_enabled != 0) {
		vol = (uint16_t)((((pi->envelope_volume >> 16) * (int32_t)vol) >> 8));
	} else if(pi->envelope_index == 3) {
		vol = 0;
	}
	v->final_volume = (uint16_t)(((((((((int32_t)(vol & 0xff) + 1) * (int32_t)s->current_volume) / 256) + 1) * (int32_t)v->volume) / 256) + 1) / 4);

	int32_t sample_index = ((((int32_t)(fd->filter_frequency ^ 0xff) - (((pi->envelope_volume >> 16) * (int32_t)fd->filter_eg) >> 8)) + (((int32_t)pi->lfo_value * (int32_t)fd->filter_lfo) >> 8)) & 0xff) >> 2;
	if(sample_index < 0) { sample_index = 0; }
	if(sample_index > 63) { sample_index = 63; }
	int8_t *source_sample = fd->samples[sample_index];

	pi->sample_start_index ^= 0x80;
	uint32_t dest_index = pi->sample_start_index;
	v->sample_data = v->synth_buf;
	v->sample_start_offset = dest_index;

	if(fd->phase_speed == 0) {
		int32_t source_index = 0;
		int32_t source_step = 1 << pi->playing_octave;
		int32_t length = 128 >> pi->playing_octave;
		for(int32_t i = 0; i < length; ++i) {
			v->synth_buf[dest_index++] = source_sample[source_index];
			source_index += source_step;
		}
	} else if(fd->phase_depth == 0) {
		int32_t source1_index = 0;
		int32_t source_step = 1 << pi->playing_octave;
		pi->phase_index = (int16_t)(pi->phase_index + (((int32_t)fd->phase_speed * (int32_t)s->calculated_tempo) >> 13));
		int32_t source2_index = (int32_t)((uint16_t)pi->phase_index >> 9);
		int32_t length2 = source2_index >> pi->playing_octave;
		int32_t length1 = (int32_t)v->sample_length_in_words * 2 - length2;
		for(int32_t i = 0; i < length1; ++i) {
			v->synth_buf[dest_index++] = (int8_t)((source_sample[source1_index] + source_sample[source2_index]) / 2);
			source1_index += source_step;
			source2_index += source_step;
		}
		if(length2 > 0) {
			source2_index -= 128;
			for(int32_t i = 0; i < length2; ++i) {
				v->synth_buf[dest_index++] = (int8_t)((source_sample[source1_index] + source_sample[source2_index]) / 2);
				source1_index += source_step;
				source2_index += source_step;
			}
		}
	} else {
		int16_t index = (int16_t)((((int32_t)fd->phase_speed * (int32_t)s->calculated_tempo) >> 11) * (int32_t)pi->phase_direction + (int32_t)pi->phase_index);
		if(index < 0) {
			if(index == -32768) {
				index = (int16_t)(index + pi->phase_direction);
			}
			pi->phase_direction = (int16_t)-pi->phase_direction;
			index = (int16_t)-index;
		}
		pi->phase_index = index;
		int32_t d = ((int32_t)fd->phase_depth * (int32_t)pi->phase_index) >> (17 + pi->playing_octave);
		int32_t length1 = (int32_t)v->sample_length_in_words + d;
		int32_t length2 = (int32_t)v->sample_length_in_words - d;
		if(length1 != 0) {
			int32_t increment = 64 / length1;
			int32_t increment_remainder = 64 % length1;
			int32_t source_index = 0;
			int32_t t = 0;
			for(int32_t i = 0; i < length1; ++i) {
				v->synth_buf[dest_index++] = source_sample[source_index];
				t -= increment_remainder;
				if(t < 0) {
					t += length1;
					source_index++;
				}
				source_index += increment;
			}
		}
		if(length2 != 0) {
			int32_t increment = 64 / length2;
			int32_t increment_remainder = 64 % length2;
			int32_t source_index = 64;
			int32_t t = 0;
			for(int32_t i = 0; i < length2; ++i) {
				v->synth_buf[dest_index++] = source_sample[source_index];
				t -= increment_remainder;
				if(t < 0) {
					t += length1;
					source_index++;
				}
				source_index += increment;
			}
		}
	}
	return 1;
}

// [=]===^=[ iffsmus_synth_play ]=================================================================[=]
static void iffsmus_synth_play(struct iffsmus_state *s, uint32_t ch) {
	struct iffsmus_voice *v = &s->voices[ch];
	if(v->set_sample_sequence == IFFSMUS_SS_START) {
		paula_queue_sample(&s->paula, (int32_t)ch, v->sample_data, v->sample_start_offset, (uint32_t)v->sample_length_in_words * 2u);
	} else {
		paula_queue_sample(&s->paula, (int32_t)ch, v->sample_data, v->sample_start_offset, (uint32_t)v->sample_length_in_words * 2u);
	}
	paula_set_loop(&s->paula, (int32_t)ch, v->sample_start_offset, (uint32_t)v->sample_length_in_words * 2u);
	paula_set_period(&s->paula, (int32_t)ch, v->period);
	paula_set_volume_256(&s->paula, (int32_t)ch, v->final_volume);
	v->set_sample_sequence = IFFSMUS_SS_NOTHING;
}

// [=]===^=[ iffsmus_form_setup ]=================================================================[=]
static int32_t iffsmus_form_setup(struct iffsmus_state *s, uint32_t ch, struct iffsmus_instrument *instr) {
	struct iffsmus_voice *v = &s->voices[ch];
	struct iffsmus_form_play *pi = &s->form_play[ch];
	struct iffsmus_form *fd = instr->form;

	switch(v->setup_sequence) {
	case IFFSMUS_IS_INITIALIZE: {
		int32_t octave = (int32_t)v->note / 12;
		int32_t note = (int32_t)v->note % 12;
		pi->mapped_note = (uint16_t)(((uint32_t)iffsmus_note_table[note] * 54728u) >> 15);
		pi->octave = (uint8_t)octave;
		pi->note = (uint8_t)note;
		int32_t octave_in_format = (-(octave - 10)) - (int32_t)fd->hi_octaves_to_skip;
		if(octave_in_format < 0 || octave_in_format >= (int32_t)fd->octaves) {
			v->setup_sequence = IFFSMUS_IS_NOTHING;
			if(v->status == IFFSMUS_VS_SILENCE) {
				return 0;
			}
			break;
		}
		uint32_t sample_length = fd->one_shot_hi_samples + fd->repeat_hi_samples;
		uint32_t start_offset = (sample_length << octave_in_format) - sample_length;
		uint32_t one_shot_length = fd->one_shot_hi_samples << octave_in_format;
		uint32_t loop_length = fd->repeat_hi_samples << octave_in_format;
		v->sample_data = fd->sample_data;
		v->sample_start_offset = start_offset;
		v->sample_length_in_words = (uint16_t)((one_shot_length != 0 ? one_shot_length : loop_length) / 2);
		if(loop_length != 0) {
			pi->loop_sample_data = v->sample_data;
			pi->loop_start = v->sample_start_offset + one_shot_length;
			pi->loop_length_in_words = (uint16_t)(loop_length / 2);
		} else {
			pi->loop_sample_data = 0;
			pi->loop_start = 0;
			pi->loop_length_in_words = 0;
		}
		v->set_sample_sequence = IFFSMUS_SS_START;
		paula_mute(&s->paula, (int32_t)ch);
		pi->volume_multiply = 1;
		break;
	}
	case IFFSMUS_IS_RELEASENOTE:
		pi->volume_multiply = 0;
		break;
	case IFFSMUS_IS_MUTE:
		paula_mute(&s->paula, (int32_t)ch);
		return 0;
	default:
		break;
	}
	v->period = (uint16_t)(((0x1080 - (int32_t)s->tune) * (int32_t)pi->mapped_note) >> 0x13);
	uint32_t fv = ((((((uint32_t)s->current_volume + 1) * (uint32_t)v->volume) / 256) + 1) * (fd->volume >> 1)) >> 0x11;
	v->final_volume = (uint16_t)(fv * pi->volume_multiply);
	return 1;
}

// [=]===^=[ iffsmus_form_play ]==================================================================[=]
static void iffsmus_form_play(struct iffsmus_state *s, uint32_t ch) {
	struct iffsmus_voice *v = &s->voices[ch];
	struct iffsmus_form_play *pi = &s->form_play[ch];
	if(v->set_sample_sequence == IFFSMUS_SS_START) {
		paula_queue_sample(&s->paula, (int32_t)ch, v->sample_data, v->sample_start_offset, (uint32_t)v->sample_length_in_words * 2u);
		v->set_sample_sequence = IFFSMUS_SS_SET_LOOP;
	}
	if(v->set_sample_sequence == IFFSMUS_SS_SET_LOOP) {
		if(pi->loop_sample_data != 0) {
			paula_set_loop(&s->paula, (int32_t)ch, pi->loop_start, (uint32_t)pi->loop_length_in_words * 2u);
		}
		v->set_sample_sequence = IFFSMUS_SS_NOTHING;
	}
	paula_set_period(&s->paula, (int32_t)ch, v->period);
	paula_set_volume_256(&s->paula, (int32_t)ch, v->final_volume);
}

// [=]===^=[ iffsmus_setup_and_play_instruments ]=================================================[=]
// Per-tick driver. Runs format-specific Setup (which may compute new period,
// new volume, regenerate synth waveform) followed by Play (which pushes the
// resulting state to Paula). Voices without a loaded format stay silent.
static void iffsmus_setup_and_play_instruments(struct iffsmus_state *s) {
	for(uint32_t i = 0; i < s->num_channels; ++i) {
		struct iffsmus_voice *v = &s->voices[i];
		int32_t instr_idx = s->current_instruments[i];
		struct iffsmus_instrument *instr = 0;
		if(instr_idx >= 0 && (uint32_t)instr_idx < s->num_instruments) {
			instr = &s->instruments[instr_idx];
		}
		uint8_t format = instr ? instr->format : IFFSMUS_FMT_NONE;

		if(format != IFFSMUS_FMT_NONE && (v->setup_sequence != IFFSMUS_IS_NOTHING || v->status != IFFSMUS_VS_SILENCE)) {
			if(format == IFFSMUS_FMT_SAMPLEDSOUND) {
				iffsmus_sampled_setup(s, i, instr);
			} else if(format == IFFSMUS_FMT_SYNTHESIS) {
				iffsmus_synth_setup(s, i, instr);
			} else if(format == IFFSMUS_FMT_FORM) {
				iffsmus_form_setup(s, i, instr);
			}
		}

		if(format != IFFSMUS_FMT_NONE) {
			if(v->setup_sequence == IFFSMUS_IS_INITIALIZE) {
				v->status = IFFSMUS_VS_PLAYING;
			} else if(v->setup_sequence == IFFSMUS_IS_RELEASENOTE) {
				v->status = IFFSMUS_VS_STOPPING;
			}
		} else {
			v->status = IFFSMUS_VS_SILENCE;
		}
		v->setup_sequence = IFFSMUS_IS_NOTHING;
	}

	for(uint32_t i = 0; i < s->num_channels; ++i) {
		struct iffsmus_voice *v = &s->voices[i];
		if(v->status == IFFSMUS_VS_SILENCE) {
			continue;
		}
		if(v->format == IFFSMUS_FMT_SAMPLEDSOUND) {
			iffsmus_sampled_play(s, i);
		} else if(v->format == IFFSMUS_FMT_SYNTHESIS) {
			iffsmus_synth_play(s, i);
		} else if(v->format == IFFSMUS_FMT_FORM) {
			iffsmus_form_play(s, i);
		}
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
	for(uint32_t i = 0; i < s->num_instruments; ++i) {
		struct iffsmus_instrument *instr = &s->instruments[i];
		if(instr->sampledsound) {
			if(instr->sampledsound->sample_data) {
				free(instr->sampledsound->sample_data);
			}
			free(instr->sampledsound);
			instr->sampledsound = 0;
		}
		if(instr->synthesis) {
			free(instr->synthesis);
			instr->synthesis = 0;
		}
		if(instr->form) {
			if(instr->form->sample_data) {
				free(instr->form->sample_data);
			}
			free(instr->form);
			instr->form = 0;
		}
	}
}

// [=]===^=[ iffsmus_init_internal ]==============================================================[=]
// Shared init body for both iffsmus_init and iffsmus_init_ex. Allocates state,
// parses the SMUS chunks (which reserves named instrument slots without
// loading their data), then initialises Paula and the sound engine. The
// caller is responsible for invoking the per-instrument loader if a host
// loader is available.
static struct iffsmus_state *iffsmus_init_internal(void *data, uint32_t len, int32_t sample_rate) {
	struct iffsmus_state *s = (struct iffsmus_state *)malloc(sizeof(*s));
	if(!s) {
		return 0;
	}
	memset(s, 0, sizeof(*s));
	s->sample_rate = sample_rate;
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
	// paula_init's tick_rate_hz is recomputed by iffsmus_set_cia_tempo before
	// the first tick fires, so the constant here is a placeholder.
	paula_init(&s->paula, sample_rate, 50);
	iffsmus_initialize_sound(s);
	return s;
}

// [=]===^=[ iffsmus_init ]=======================================================================[=]
static struct iffsmus_state *iffsmus_init(void *data, uint32_t len, int32_t sample_rate) {
	return iffsmus_init_internal(data, len, sample_rate);
}

// [=]===^=[ iffsmus_init_ex ]====================================================================[=]
// Like iffsmus_init, but uses the host-supplied loader to fetch instrument
// .instr (and .ss, for SampledSound) files from "Instruments/<name>.*" relative
// to the module's directory. Each unrecognised or missing instrument leaves
// its slot in IFFSMUS_FMT_NONE so the rest of the song still plays.
static struct iffsmus_state *iffsmus_init_ex(void *data, uint32_t len, int32_t sample_rate, struct player_loader *loader) {
	struct iffsmus_state *s = iffsmus_init_internal(data, len, sample_rate);
	if(!s) {
		return 0;
	}
	if(loader && loader->fetch) {
		for(uint32_t i = 0; i < s->num_instruments; ++i) {
			iffsmus_load_one_instrument(&s->instruments[i], loader);
		}
	}
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
static void iffsmus_get_audio(struct iffsmus_state *s, float *output, int32_t frames) {
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

// [=]===^=[ iffsmus_api_init_ex ]================================================================[=]
static void *iffsmus_api_init_ex(void *data, uint32_t len, int32_t sample_rate, struct player_loader *loader) {
	return iffsmus_init_ex(data, len, sample_rate, loader);
}

// [=]===^=[ iffsmus_api_free ]===================================================================[=]
static void iffsmus_api_free(void *state) {
	iffsmus_free((struct iffsmus_state *)state);
}

// [=]===^=[ iffsmus_api_get_audio ]==============================================================[=]
static void iffsmus_api_get_audio(void *state, float *output, int32_t frames) {
	iffsmus_get_audio((struct iffsmus_state *)state, output, frames);
}

static const char *iffsmus_extensions[] = { "smus", 0 };

static struct player_api iffsmus_api = {
	"IFF SMUS",
	iffsmus_extensions,
	iffsmus_api_init,
	iffsmus_api_free,
	iffsmus_api_get_audio,
	iffsmus_api_init_ex,
};
