// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// TFMX replayer (Chris Huelsbeck format), ported from NostalgicPlayer's
// LibTfmxAudioDecoder C# implementation. Drives an Amiga Paula (see paula.h)
// and supports the TFMX 1.5, TFMX Pro, and TFMX 7-Voice variants.
//
// Tick rate is module-defined (CIA-B timer): the player calls SetRate /
// SetBpm at runtime, expressed as a 24.8 fixed-point Hz value (default 50 Hz).
// Output mixing in get_audio is sample-rate driven; ticks are advanced from a
// fractional accumulator that converts (sample_rate / current_rate) into the
// next-tick boundary in output samples.
//
// Single-file formats (TFHD, TFMXPAK, TFMX-MOD) and inline sample data are
// supported by passing the entire file in init(). Two-file variants
// (mdat.X / smpl.X) are not supported through this single-buffer API: the
// host should merge the two files (mdat first, then smpl) before calling
// init() and the merged buffer will be detected as a non-tagged TFMX.
// In that mode, the boundary is auto-detected via offsets.MdatSize from the
// header offsets in the mdat data; for explicitly tagged TFMX files lacking
// inline samples, init() returns 0.
//
// Public API:
//   struct tfmx_state *tfmx_init(void *data, uint32_t len, int32_t sample_rate);
//   void tfmx_free(struct tfmx_state *s);
//   void tfmx_get_audio(struct tfmx_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define TFMX_TRACKS_MAX        8
#define TFMX_TRACK_STEPS_MAX   512
#define TFMX_VOICES_MAX        8
#define TFMX_RECURSE_LIMIT     16
#define TFMX_TRACK_CMD_MAX     4
#define TFMX_MACRO_CMD_COUNT   0x40
#define TFMX_PATT_CMD_COUNT    16
#define TFMX_DEFAULT_TICK_HZ   50

struct tfmx_module_offsets {
	uint32_t header;
	uint32_t track_table;
	uint32_t patterns;
	uint32_t macros;
	uint32_t sample_data;
	uint32_t silence;
};

struct tfmx_admin {
	int16_t speed;
	int16_t count;
	int32_t start_speed;
	int32_t start_song;
	uint8_t initialized;
	uint16_t random_word;
};

struct tfmx_cmd {
	uint8_t aa;
	uint8_t bb;
	uint8_t cd;
	uint8_t ee;
};

struct tfmx_pattern {
	uint32_t offset;
	uint32_t offset_saved;
	uint16_t step;
	uint16_t step_saved;
	uint8_t wait;
	int8_t loops;
	uint8_t eval_next;
	uint8_t infinite_loop;
};

struct tfmx_track {
	uint8_t pt;
	int8_t tr;
	uint8_t on;
	struct tfmx_pattern pattern;
};

struct tfmx_fade {
	uint8_t volume;
	uint8_t target;
	uint8_t speed;
	uint8_t count;
	int8_t delta;
	uint8_t active;
};

struct tfmx_seq_step {
	int32_t first;
	int32_t last;
	int32_t current;
	uint8_t size;
	uint8_t next;
};

struct tfmx_sequencer {
	uint8_t tracks;
	uint8_t eval_next;
	int16_t loops;
	struct tfmx_seq_step step;
	uint8_t step_seen_before[TFMX_TRACK_STEPS_MAX];
};

struct tfmx_player_info {
	struct tfmx_admin admin;
	struct tfmx_track track[TFMX_TRACKS_MAX];
	struct tfmx_cmd cmd;
	uint8_t macro_eval_again;
	struct tfmx_sequencer sequencer;
	struct tfmx_fade fade;
};

struct tfmx_macro_state {
	uint32_t offset;
	uint32_t offset_saved;
	uint32_t step;
	uint32_t step_saved;
	int16_t wait;
	uint8_t loop;
	uint8_t skip;
	uint8_t extra_wait;
};

struct tfmx_vibrato_state {
	uint8_t time;
	uint8_t count;
	int8_t intensity;
	int16_t delta;
};

struct tfmx_envelope_state {
	uint8_t flag;
	uint8_t count;
	uint8_t speed;
	uint8_t target;
};

struct tfmx_portamento_state {
	uint8_t count;
	uint8_t wait;
	uint16_t speed;
	uint16_t period;
};

struct tfmx_sample_range_state {
	uint32_t start;
	uint16_t length;
};

struct tfmx_paula_orig {
	uint32_t offset;
	uint16_t length;
};

struct tfmx_sid_op1 {
	uint16_t speed;
	uint16_t count;
	uint16_t inter_delta;
	int16_t inter_mod;
};

struct tfmx_sid_op23 {
	uint16_t speed;
	uint16_t count;
	uint32_t offset;
	int16_t delta;
};

struct tfmx_sid_state {
	uint32_t source_offset;
	uint16_t source_length;
	uint32_t target_offset;
	uint16_t target_length;
	int8_t last_sample;
	struct tfmx_sid_op1 op1;
	struct tfmx_sid_op23 op2;
	struct tfmx_sid_op23 op3;
};

struct tfmx_rnd_arp {
	uint32_t offset;
	uint16_t pos;
};

struct tfmx_rnd_state {
	uint8_t macro;
	int8_t count;
	int8_t speed;
	int8_t flag;
	uint8_t mode;
	uint8_t block_wait;
	uint8_t mask;
	struct tfmx_rnd_arp arp;
};

// Per-voice runtime state. Mirrors VoiceVars from the C# port plus the
// PaulaVoice "paula.start/length/period/volume" image needed to implement
// the On/Off/TakeNextBuf semantics on top of the local paula mixer.
struct tfmx_voice_state {
	uint16_t output_period;
	uint8_t voice_num;
	int8_t effects_mode;
	int8_t volume;
	uint8_t note;
	uint8_t note_previous;
	uint8_t note_volume;
	uint16_t period;
	int16_t detune;
	uint8_t key_up;

	struct tfmx_macro_state macro;

	int16_t wait_on_dma_count;
	uint16_t wait_on_dma_prev_loops;

	uint8_t add_begin_count;
	uint8_t add_begin_arg;
	int32_t add_begin_offset;

	struct tfmx_vibrato_state vibrato;
	struct tfmx_envelope_state envelope;
	struct tfmx_portamento_state portamento;
	struct tfmx_sample_range_state sample;
	struct tfmx_paula_orig paula_orig;
	struct tfmx_sid_state sid;
	struct tfmx_rnd_state rnd;

	// Mirror of the C# PaulaVoice state. paula_start_offset is an offset
	// into the unified module buffer (state->buf). length is in 16-bit
	// words to match the original Amiga Paula AUDxLEN convention.
	uint32_t paula_start_offset;
	uint16_t paula_length;
	uint16_t paula_period;
	uint16_t paula_volume;
	uint8_t paula_dma_on;
	uint16_t paula_loop_count;
	uint16_t paula_prev_loop_count;
};

struct tfmx_variant {
	uint8_t compressed;
	uint8_t finetune_unscaled;
	uint8_t vibrato_unscaled;
	uint8_t porta_unscaled;
	uint8_t porta_override;
	uint8_t no_note_detune;
	uint8_t bpm_speed5;
	uint8_t no_add_begin_count;
	uint8_t no_track_mute;
};

struct tfmx_input {
	uint8_t *buf;          // unified module buffer (mdat + smpl), allocated
	uint32_t buf_len;
	uint32_t len;
	uint32_t mdat_size;
	uint32_t smpl_size;
	int32_t version_hint;
	int32_t start_song_hint;
};

struct tfmx_state {
	struct paula paula;
	int32_t sample_rate;

	struct tfmx_input input;
	struct tfmx_module_offsets offsets;
	struct tfmx_variant variant;
	struct tfmx_player_info player_info;
	struct tfmx_voice_state voices[TFMX_VOICES_MAX];

	uint8_t channel_to_voice_map[TFMX_VOICES_MAX];

	uint8_t v_songs[64];
	uint32_t v_songs_count;

	int32_t voice_count;
	uint8_t song_end;
	uint8_t real_song_end;
	uint8_t loop_mode;
	uint8_t trigger_restart;

	uint32_t rate;             // 24.8 fixed-point Hz
	uint32_t tick_fp;          // ms*256 per tick (currently unused, kept for parity)
	uint32_t tick_fp_add;

	// Sample-rate driven tick cadence: number of output frames between
	// player ticks, in 16.16 fixed point, regenerated whenever rate changes.
	uint32_t frames_per_tick_fp;
	uint32_t frames_until_tick_fp;

	uint8_t track_cmd_used[TFMX_TRACK_CMD_MAX + 1];
	uint8_t pattern_cmd_used[TFMX_PATT_CMD_COUNT];
	uint8_t macro_cmd_used[TFMX_MACRO_CMD_COUNT];
};

static uint16_t tfmx_periods[] = {
	0x0d5c,
	0x0c9c, 0x0be8, 0x0b3c, 0x0a9a, 0x0a02, 0x0a02, 0x0972,
	0x08ea, 0x086a, 0x07f2, 0x0780, 0x0718,
	0x06ae, 0x064e, 0x05f4, 0x059e, 0x054d, 0x0501, 0x04b9, 0x0475,
	0x0435, 0x03f9, 0x03c0, 0x038c,
	0x0358, 0x032a, 0x02fc, 0x02d0, 0x02a8, 0x0282, 0x025e, 0x023b,
	0x021b, 0x01fd, 0x01e0, 0x01c6,
	0x01ac, 0x0194, 0x017d, 0x0168, 0x0154, 0x0140, 0x012f, 0x011e,
	0x010e, 0x00fe, 0x00f0, 0x00e3,
	0x00d6, 0x00ca, 0x00bf, 0x00b4, 0x00aa, 0x00a0, 0x0097, 0x008f,
	0x0087, 0x007f, 0x0078, 0x0071,
	0x00d6, 0x00ca, 0x00bf, 0x00b4, 0x00aa, 0x00a0, 0x0097, 0x008f,
	0x0087, 0x007f, 0x0078, 0x0071,
	0x00d6, 0x00ca, 0x00bf, 0x00b4
};

static uint32_t tfmx_crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
	0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
	0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
	0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
	0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
	0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
	0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
	0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
	0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
	0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
	0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
	0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
	0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
	0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
	0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
	0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
	0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
	0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
	0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
	0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static const char *tfmx_extensions[] = { "tfx", "mdat", "tfm", "tfmx", 0 };

// [=]===^=[ tfmx_read_be_u32 ]===================================================================[=]
static uint32_t tfmx_read_be_u32(uint8_t *p, uint32_t off) {
	return ((uint32_t)p[off] << 24) | ((uint32_t)p[off + 1] << 16) | ((uint32_t)p[off + 2] << 8) | (uint32_t)p[off + 3];
}

// [=]===^=[ tfmx_read_be_u16 ]===================================================================[=]
static uint16_t tfmx_read_be_u16(uint8_t *p, uint32_t off) {
	return (uint16_t)(((uint32_t)p[off] << 8) | (uint32_t)p[off + 1]);
}

// [=]===^=[ tfmx_make_word ]=====================================================================[=]
static uint16_t tfmx_make_word(uint8_t hi, uint8_t lo) {
	return (uint16_t)(((uint32_t)hi << 8) | (uint32_t)lo);
}

// [=]===^=[ tfmx_make_dword ]====================================================================[=]
static uint32_t tfmx_make_dword(uint8_t hihi, uint8_t hilo, uint8_t hi, uint8_t lo) {
	return ((uint32_t)hihi << 24) | ((uint32_t)hilo << 16) | ((uint32_t)hi << 8) | (uint32_t)lo;
}

// [=]===^=[ tfmx_byteswap_u32 ]==================================================================[=]
static uint32_t tfmx_byteswap_u32(uint32_t v) {
	return ((v & 0xff000000u) >> 24) | ((v & 0x00ff0000u) >> 8) | ((v & 0x0000ff00u) << 8) | ((v & 0x000000ffu) << 24);
}

// [=]===^=[ tfmx_crc32 ]=========================================================================[=]
static uint32_t tfmx_crc32(uint8_t *buf, uint32_t off, uint32_t len) {
	uint32_t crc = 0xffffffffu;
	uint32_t i;
	for(i = 0; i < len; ++i) {
		crc = tfmx_crc32_tab[(crc ^ buf[off + i]) & 0xff] ^ (crc >> 8);
	}
	return crc ^ 0xffffffffu;
}

// [=]===^=[ tfmx_random_word ]===================================================================[=]
static uint16_t tfmx_random_word(struct tfmx_state *s) {
	// Linear congruential, with the same general behaviour as the C# port.
	uint32_t r = (uint32_t)s->player_info.admin.random_word * 1103515245u + 12345u;
	s->player_info.admin.random_word = (uint16_t)(r >> 16);
	return s->player_info.admin.random_word;
}

// [=]===^=[ tfmx_get_macro_offset ]==============================================================[=]
static uint32_t tfmx_get_macro_offset(struct tfmx_state *s, uint8_t macro) {
	return s->offsets.header + tfmx_read_be_u32(s->input.buf, s->offsets.macros + ((uint32_t)(macro & 0x7f) << 2));
}

// [=]===^=[ tfmx_get_patt_offset ]===============================================================[=]
static uint32_t tfmx_get_patt_offset(struct tfmx_state *s, uint8_t pt) {
	return s->offsets.header + tfmx_read_be_u32(s->input.buf, s->offsets.patterns + ((uint32_t)pt << 2));
}

// [=]===^=[ tfmx_set_rate ]======================================================================[=]
// rate is in 24.8 fixed-point Hz (so default 50 Hz -> 50 << 8).
static void tfmx_set_rate(struct tfmx_state *s, uint32_t rate) {
	if(rate == 0) {
		rate = TFMX_DEFAULT_TICK_HZ << 8;
	}
	s->rate = rate;
	s->tick_fp = (1000u << 16) / rate;
	// frames per tick = sample_rate / (rate / 256)
	uint64_t fpt_fp = ((uint64_t)s->sample_rate << 24) / rate;
	s->frames_per_tick_fp = (uint32_t)fpt_fp;
}

// [=]===^=[ tfmx_set_bpm ]=======================================================================[=]
static void tfmx_set_bpm(struct tfmx_state *s, uint16_t bpm) {
	tfmx_set_rate(s, ((uint32_t)bpm << 8) * 2u / 5u);
}

// [=]===^=[ tfmx_get_track_mute ]================================================================[=]
static uint8_t tfmx_get_track_mute(struct tfmx_state *s, uint8_t t) {
	if(s->variant.no_track_mute) {
		return 1;
	}
	return (uint8_t)(0 == tfmx_read_be_u16(s->input.buf, s->offsets.header + 0x1c0u + ((uint32_t)t << 1)));
}

// [=]===^=[ tfmx_note_to_period ]================================================================[=]
static uint16_t tfmx_note_to_period(int32_t note) {
	if(note >= 0) {
		note &= 0x3f;
	} else if(note < -13) {
		note = -13;
	}
	return tfmx_periods[note + 13];
}

// Hardware shim helpers: thin layer between the TFMX engine and paula.h.
// These mirror the C# PaulaVoice / IChannel surface used by the port.

// [=]===^=[ tfmx_paula_apply_dma ]================================================================[=]
// Push the voice's mirrored Paula registers to the actual paula channel.
// Called when DMA goes from off to on (On()) or to update period/volume/
// length on each Run().
static void tfmx_paula_apply_dma(struct tfmx_state *s, struct tfmx_voice_state *v) {
	uint32_t length_bytes = (uint32_t)v->paula_length << 1;
	if(length_bytes == 0) {
		length_bytes = 2;
	}
	int8_t *sample_ptr = (int8_t *)s->input.buf;
	if(v->paula_dma_on) {
		paula_play_sample(&s->paula, (int32_t)v->voice_num, sample_ptr, v->paula_start_offset + length_bytes);
		s->paula.ch[v->voice_num].pos_fp = (uint64_t)v->paula_start_offset << PAULA_FP_SHIFT;
		paula_set_loop(&s->paula, (int32_t)v->voice_num, v->paula_start_offset, length_bytes);
		paula_set_period(&s->paula, (int32_t)v->voice_num, v->paula_period == 0 ? 0x100 : v->paula_period);
		paula_set_volume(&s->paula, (int32_t)v->voice_num, v->paula_volume > 64 ? 64 : v->paula_volume);
	}
}

// [=]===^=[ tfmx_paula_take_next_buf ]===========================================================[=]
// Equivalent to PaulaVoice.TakeNextBuf in the C# port: queues the current
// paula_start_offset / paula_length as the next-buffer pair so that when
// the currently playing one-shot wraps, the new bounds are picked up.
static void tfmx_paula_take_next_buf(struct tfmx_state *s, struct tfmx_voice_state *v) {
	uint32_t length_bytes = (uint32_t)v->paula_length << 1;
	if(length_bytes == 0) {
		length_bytes = 2;
	}
	int8_t *sample_ptr = (int8_t *)s->input.buf;
	if(!v->paula_dma_on) {
		return;
	}
	paula_queue_sample(&s->paula, (int32_t)v->voice_num, sample_ptr, v->paula_start_offset, length_bytes);
	paula_set_loop(&s->paula, (int32_t)v->voice_num, v->paula_start_offset, length_bytes);
}

// [=]===^=[ tfmx_paula_on ]======================================================================[=]
static void tfmx_paula_on(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->paula_dma_on = 1;
	tfmx_paula_apply_dma(s, v);
}

// [=]===^=[ tfmx_paula_off ]=====================================================================[=]
static void tfmx_paula_off(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->paula_dma_on = 0;
	v->paula_period = 0;
	v->paula_volume = 0;
	v->paula_loop_count = 0;
	paula_mute(&s->paula, (int32_t)v->voice_num);
}

// [=]===^=[ tfmx_take_next_buf_checked ]=========================================================[=]
// Equivalent to TakeNextBufChecked: clamps the start+length to lie within
// the unified buffer, falling back to the silence sample if necessary,
// then mirrors to paula via paula_queue_sample.
static void tfmx_take_next_buf_checked(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(v->paula_orig.offset >= s->input.len) {
		v->paula_start_offset = s->offsets.silence;
		v->paula_length = 1;
	} else if((v->paula_orig.offset + ((uint32_t)v->paula_orig.length << 1)) > s->input.len) {
		v->paula_length = (uint16_t)((s->input.len - v->paula_orig.offset) >> 1);
		v->paula_start_offset = v->paula_orig.offset;
	} else {
		v->paula_start_offset = v->paula_orig.offset;
		v->paula_length = v->paula_orig.length;
	}
	tfmx_paula_take_next_buf(s, v);
}

// [=]===^=[ tfmx_to_paula_start ]================================================================[=]
static void tfmx_to_paula_start(struct tfmx_state *s, struct tfmx_voice_state *v, uint32_t offset) {
	v->paula_orig.offset = offset;
	v->paula_start_offset = offset;
	tfmx_take_next_buf_checked(s, v);
}

// [=]===^=[ tfmx_to_paula_length ]===============================================================[=]
static void tfmx_to_paula_length(struct tfmx_state *s, struct tfmx_voice_state *v, uint16_t length) {
	v->paula_orig.length = length;
	v->paula_length = length;
	tfmx_take_next_buf_checked(s, v);
}

// Macro command handler signatures.
typedef void (*tfmx_macro_fn)(struct tfmx_state *, struct tfmx_voice_state *);
typedef void (*tfmx_patt_fn)(struct tfmx_state *, struct tfmx_track *);
typedef void (*tfmx_track_fn)(struct tfmx_state *, uint32_t);

// Forward macro/pattern/track function table indices via wrappers below.
// To preserve "no forward declarations", function order is: helpers first,
// then macro functions, then pattern functions, then track functions, then
// the dispatcher functions, then init/run, then API thunks.

// [=]===^=[ tfmx_note_cmd ]======================================================================[=]
// Implementation depends on macro state, but only modifies voice via NoteCmd
// pattern path. Defined here because both NoteCmd path and macro $21 invoke it.
static uint16_t tfmx_note_cmd_period(int32_t note) {
	return tfmx_note_to_period(note);
}

// [=]===^=[ tfmx_note_cmd_apply ]================================================================[=]
static void tfmx_note_cmd_apply(struct tfmx_state *s) {
	uint8_t v_num = s->channel_to_voice_map[s->player_info.cmd.cd & (TFMX_VOICES_MAX - 1)];
	struct tfmx_voice_state *v = &s->voices[v_num];
	struct tfmx_cmd *c = &s->player_info.cmd;
	if(c->aa == 0xfc) {
		// Lock note: nothing
	} else if(c->aa == 0xf7) {
		v->envelope.speed = c->bb;
		uint8_t tmp = (uint8_t)((c->cd >> 4) + 1);
		v->envelope.count = tmp;
		v->envelope.flag = tmp;
		v->envelope.target = c->ee;
	} else if(c->aa == 0xf6) {
		uint8_t tmp = (uint8_t)(c->bb & 0xfe);
		v->vibrato.time = tmp;
		v->vibrato.count = (uint8_t)(tmp >> 1);
		v->vibrato.intensity = (int8_t)c->ee;
		v->vibrato.delta = 0;
	} else if(c->aa == 0xf5) {
		v->key_up = 1;
	} else if(c->aa < 0xc0) {
		if(s->variant.no_note_detune) {
			v->detune = 0;
		} else {
			v->detune = (int8_t)c->ee;
		}
		v->note_volume = (uint8_t)(c->cd >> 4);
		v->note_previous = v->note;
		v->note = c->aa;
		v->key_up = 0;
		v->macro.offset = tfmx_get_macro_offset(s, (uint8_t)(c->bb & 0x7f));
		v->macro.step = 0;
		v->macro.wait = 0;
		v->macro.loop = 0xff;
		v->macro.skip = 0;
		v->effects_mode = 0;
		v->wait_on_dma_count = 0;
	} else {
		v->portamento.count = c->bb;
		v->portamento.wait = 1;
		if(v->portamento.speed == 0) {
			v->portamento.period = v->period;
		}
		v->portamento.speed = c->ee;
		v->note = (uint8_t)(c->aa & 0x3f);
		v->period = tfmx_note_to_period((int32_t)v->note);
	}
}

// [=]===^=[ tfmx_macro_extra_wait ]==============================================================[=]
static void tfmx_macro_extra_wait(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->macro.step++;
	if(!v->macro.extra_wait) {
		v->macro.extra_wait = 1;
		s->player_info.macro_eval_again = 1;
	}
}

// [=]===^=[ tfmx_macro_func_nop ]================================================================[=]
static void tfmx_macro_func_nop(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_stop_sample_sub ]====================================================[=]
static void tfmx_macro_func_stop_sample_sub(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->macro.step++;
	tfmx_paula_off(s, v);
	struct tfmx_cmd *c = &s->player_info.cmd;
	if(c->bb != 0) {
		v->macro.extra_wait = 0;
	} else {
		s->player_info.macro_eval_again = 1;
	}
	if(c->cd == 0) {
		if(c->ee == 0) {
			return;
		}
		uint8_t vol1 = (uint8_t)(v->note_volume * 3);
		v->volume = (int8_t)(vol1 + tfmx_make_word(c->cd, c->ee));
	} else {
		v->volume = (int8_t)c->ee;
	}
	int8_t vol = v->volume;
	if(s->player_info.fade.volume < 64) {
		vol = (int8_t)((4 * (int32_t)v->volume * (int32_t)s->player_info.fade.volume) / (4 * 0x40));
	}
	v->paula_volume = (uint16_t)vol;
}

// [=]===^=[ tfmx_macro_func_stop_sound ]=========================================================[=]
static void tfmx_macro_func_stop_sound(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->envelope.flag = 0;
	v->vibrato.time = 0;
	v->portamento.speed = 0;
	v->sid.target_length = 0;
	v->rnd.flag = 0;
	tfmx_macro_func_stop_sample_sub(s, v);
}

// [=]===^=[ tfmx_macro_func_start_sample ]=======================================================[=]
static void tfmx_macro_func_start_sample(struct tfmx_state *s, struct tfmx_voice_state *v) {
	tfmx_paula_on(s, v);
	v->effects_mode = (int8_t)s->player_info.cmd.bb;
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_set_begin_sub ]======================================================[=]
static void tfmx_macro_func_set_begin_sub(struct tfmx_state *s, struct tfmx_voice_state *v, uint32_t start) {
	v->sample.start = start;
	tfmx_to_paula_start(s, v, start);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_set_begin ]==========================================================[=]
static void tfmx_macro_func_set_begin(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->add_begin_count = 0;
	struct tfmx_cmd *c = &s->player_info.cmd;
	uint32_t start = s->offsets.sample_data + tfmx_make_dword(0, c->bb, c->cd, c->ee);
	tfmx_macro_func_set_begin_sub(s, v, start);
}

// [=]===^=[ tfmx_macro_func_set_len ]============================================================[=]
static void tfmx_macro_func_set_len(struct tfmx_state *s, struct tfmx_voice_state *v) {
	uint16_t len = tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee);
	v->sample.length = len;
	tfmx_to_paula_length(s, v, len);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_wait ]===============================================================[=]
static void tfmx_macro_func_wait(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if((s->player_info.cmd.bb & 1) != 0) {
		if(v->rnd.block_wait) {
			return;
		}
		v->rnd.block_wait = 1;
		v->macro.step++;
		s->player_info.macro_eval_again = 1;
	} else {
		v->macro.wait = (int16_t)tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee);
		tfmx_macro_extra_wait(s, v);
	}
}

// [=]===^=[ tfmx_macro_func_loop ]===============================================================[=]
static void tfmx_macro_func_loop(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(v->macro.loop == 0) {
		v->macro.loop = 0xff;
		v->macro.step++;
	} else {
		if(v->macro.loop == 0xff) {
			v->macro.loop = (uint8_t)(s->player_info.cmd.bb - 1);
		} else {
			v->macro.loop--;
		}
		v->macro.step = tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee);
	}
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_cont ]===============================================================[=]
static void tfmx_macro_func_cont(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->macro.offset = tfmx_get_macro_offset(s, (uint8_t)(s->player_info.cmd.bb & 0x7f));
	v->macro.step = tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee);
	v->macro.loop = 0xff;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_stop ]===============================================================[=]
static void tfmx_macro_func_stop(struct tfmx_state *s, struct tfmx_voice_state *v) {
	(void)s;
	v->macro.skip = 1;
}

// [=]===^=[ tfmx_macro_func_add_note_sub ]=======================================================[=]
static void tfmx_macro_func_add_note_sub(struct tfmx_state *s, struct tfmx_voice_state *v, uint8_t note_add) {
	int8_t n = (int8_t)((int32_t)note_add + (int8_t)s->player_info.cmd.bb);
	uint16_t p = tfmx_note_to_period((int32_t)n);
	int16_t finetune = (int16_t)((int32_t)v->detune + (int16_t)tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee));
	if(s->variant.finetune_unscaled) {
		p = (uint16_t)((int32_t)p + finetune);
	} else if(finetune != 0) {
		p = (uint16_t)((((int32_t)finetune + 0x100) * (int32_t)p) >> 8);
	}
	v->period = p;
	if(v->portamento.speed == 0) {
		v->output_period = p;
	}
}

// [=]===^=[ tfmx_macro_func_add_note ]===========================================================[=]
static void tfmx_macro_func_add_note(struct tfmx_state *s, struct tfmx_voice_state *v) {
	tfmx_macro_func_add_note_sub(s, v, v->note);
	tfmx_macro_extra_wait(s, v);
}

// [=]===^=[ tfmx_macro_func_set_note ]===========================================================[=]
static void tfmx_macro_func_set_note(struct tfmx_state *s, struct tfmx_voice_state *v) {
	tfmx_macro_func_add_note_sub(s, v, 0);
	tfmx_macro_extra_wait(s, v);
}

// [=]===^=[ tfmx_macro_func_reset ]==============================================================[=]
static void tfmx_macro_func_reset(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->add_begin_count = 0;
	v->envelope.flag = 0;
	v->vibrato.time = 0;
	v->portamento.speed = 0;
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_portamento ]=========================================================[=]
static void tfmx_macro_func_portamento(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	v->portamento.count = c->bb;
	v->portamento.wait = 1;
	if(s->variant.porta_override || (v->portamento.speed == 0)) {
		v->portamento.period = v->period;
	}
	v->portamento.speed = tfmx_make_word(c->cd, c->ee);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_vibrato ]============================================================[=]
static void tfmx_macro_func_vibrato(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	v->vibrato.time = c->bb;
	v->vibrato.count = (uint8_t)(c->bb >> 1);
	v->vibrato.intensity = (int8_t)c->ee;
	if(v->portamento.speed == 0) {
		v->output_period = v->period;
		v->vibrato.delta = 0;
	}
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_add_volume ]=========================================================[=]
static void tfmx_macro_func_add_volume(struct tfmx_state *s, struct tfmx_voice_state *v) {
	uint8_t vol = (uint8_t)(v->note_volume * 3);
	vol = (uint8_t)(vol + s->player_info.cmd.ee);
	v->volume = (int8_t)vol;
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_add_vol_note ]=======================================================[=]
static void tfmx_macro_func_add_vol_note(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	if(c->cd == 0xfe) {
		uint8_t ee = c->ee;
		c->cd = 0;
		c->ee = 0;
		tfmx_macro_func_add_note_sub(s, v, v->note);
		c->ee = ee;
	}
	uint8_t vol = (uint8_t)(v->note_volume * 3);
	vol = (uint8_t)(vol + c->ee);
	v->volume = (int8_t)vol;
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_set_volume ]=========================================================[=]
static void tfmx_macro_func_set_volume(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	if(c->cd == 0xfe) {
		uint8_t ee = c->ee;
		c->cd = 0;
		c->ee = 0;
		tfmx_macro_func_add_note_sub(s, v, v->note);
		c->ee = ee;
	}
	v->volume = (int8_t)c->ee;
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_envelope ]===========================================================[=]
static void tfmx_macro_func_envelope(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	v->envelope.speed = c->bb;
	v->envelope.flag = c->cd;
	v->envelope.count = c->cd;
	v->envelope.target = c->ee;
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_loop_keyup ]=========================================================[=]
static void tfmx_macro_func_loop_keyup(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(!v->key_up) {
		tfmx_macro_func_loop(s, v);
	} else {
		v->macro.step++;
		s->player_info.macro_eval_again = 1;
	}
}

// [=]===^=[ tfmx_macro_func_add_begin ]==========================================================[=]
static void tfmx_macro_func_add_begin(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	v->add_begin_count = c->bb;
	v->add_begin_arg = c->bb;
	int32_t offset = (int16_t)tfmx_make_word(c->cd, c->ee);
	v->add_begin_offset = offset;
	uint32_t begin = (uint32_t)((int64_t)v->sample.start + offset);
	if(begin < s->offsets.sample_data) {
		begin = s->offsets.sample_data;
	}
	if(v->sid.target_length == 0) {
		tfmx_macro_func_set_begin_sub(s, v, begin);
	} else {
		v->sample.start = begin;
		v->sid.source_offset = begin;
		v->macro.step++;
		s->player_info.macro_eval_again = 1;
	}
}

// [=]===^=[ tfmx_macro_func_add_len ]============================================================[=]
static void tfmx_macro_func_add_len(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
	int16_t len = (int16_t)tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee);
	v->sample.length = (uint16_t)((int32_t)v->sample.length + len);
	if(v->sid.target_length == 0) {
		tfmx_to_paula_length(s, v, v->sample.length);
	} else {
		v->sid.source_length = (uint16_t)len;
	}
}

// [=]===^=[ tfmx_macro_func_stop_sample ]========================================================[=]
static void tfmx_macro_func_stop_sample(struct tfmx_state *s, struct tfmx_voice_state *v) {
	tfmx_macro_func_stop_sample_sub(s, v);
}

// [=]===^=[ tfmx_macro_func_wait_keyup ]=========================================================[=]
static void tfmx_macro_func_wait_keyup(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(v->key_up) {
		v->macro.step++;
		s->player_info.macro_eval_again = 1;
	} else {
		if(v->macro.loop == 0) {
			v->macro.loop = 0xff;
			v->macro.step++;
			s->player_info.macro_eval_again = 1;
		} else if(v->macro.loop == 0xff) {
			v->macro.loop = (uint8_t)(s->player_info.cmd.ee - 1);
		} else {
			v->macro.loop--;
		}
	}
}

// [=]===^=[ tfmx_macro_func_goto ]===============================================================[=]
static void tfmx_macro_func_goto(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->macro.offset_saved = v->macro.offset;
	v->macro.step_saved = v->macro.step;
	tfmx_macro_func_cont(s, v);
}

// [=]===^=[ tfmx_macro_func_return ]=============================================================[=]
static void tfmx_macro_func_return(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->macro.offset = v->macro.offset_saved;
	v->macro.step = v->macro.step_saved;
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_set_period ]=========================================================[=]
static void tfmx_macro_func_set_period(struct tfmx_state *s, struct tfmx_voice_state *v) {
	uint16_t period = tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee);
	v->period = period;
	if(v->portamento.speed == 0) {
		v->output_period = period;
	}
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_sample_loop ]========================================================[=]
static void tfmx_macro_func_sample_loop(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	uint32_t offset = tfmx_make_dword(0, c->bb, c->cd, c->ee);
	v->sample.start += offset;
	v->sample.length = (uint16_t)((int32_t)v->sample.length - (int32_t)(offset >> 1));
	tfmx_to_paula_start(s, v, v->sample.start);
	tfmx_to_paula_length(s, v, v->sample.length);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_one_shot ]===========================================================[=]
static void tfmx_macro_func_one_shot(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->add_begin_count = 0;
	v->sample.start = s->offsets.sample_data;
	v->sample.length = 1;
	tfmx_to_paula_start(s, v, v->sample.start);
	tfmx_to_paula_length(s, v, v->sample.length);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_wait_on_dma ]========================================================[=]
static void tfmx_macro_func_wait_on_dma(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->wait_on_dma_count = (int16_t)tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee);
	v->macro.skip = 1;
	v->wait_on_dma_prev_loops = v->paula_loop_count;
	tfmx_macro_extra_wait(s, v);
}

// [=]===^=[ tfmx_random_play ]===================================================================[=]
// Forward-declared semantically: defined after macro_func_random_play.
// Implementation lives further down (mutual recursion would have us forward
// declare, so we inline both via dispatcher: random_play_run is called by
// modulation and by the macro). Define stubs here that call into a single
// consolidated routine.
//
// The C# code uses random play with sometimes intricate state. We keep the
// minimal but functionally complete implementation; modules using it are
// extremely rare.
static void tfmx_randomize(struct tfmx_state *s) {
	s->player_info.admin.random_word = (uint16_t)((s->player_info.admin.random_word ^ tfmx_random_word(s)) + 0x4335);
}

static void tfmx_random_play_mask(struct tfmx_state *s, struct tfmx_voice_state *v) {
	tfmx_randomize(s);
	v->rnd.arp.pos = (uint16_t)((s->player_info.admin.random_word & 0xff) & v->rnd.mask);
}

static void tfmx_random_play_check_wait(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if((s->player_info.cmd.aa & 0x80) != 0) {
		v->rnd.block_wait = 0;
	}
}

static void tfmx_random_play_reverb(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(((v->rnd.mode & 2) == 0) || ((((int32_t)v->rnd.speed * 3) / 8) != v->rnd.count)) {
		return;
	}
	struct tfmx_voice_state *v2 = &s->voices[(v->voice_num + 1) & 3];
	v2->volume = (int8_t)(((int32_t)v->volume * 5) / 8);
	if(v->period != v2->period) {
		v2->period = v->period;
		v2->output_period = v->period;
		tfmx_random_play_check_wait(s, v);
	}
}

static void tfmx_random_play_run(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(v->rnd.flag == 0) {
		return;
	} else if(v->rnd.flag > 0) {
		v->rnd.arp.offset = tfmx_get_macro_offset(s, v->rnd.macro);
		v->rnd.arp.pos = 0;
		v->rnd.flag = -1;
		if((v->rnd.mode & 1) != 0) {
			tfmx_random_play_mask(s, v);
		}
	}

	if(--v->rnd.count == 0) {
		v->rnd.count = v->rnd.speed;
		while(1) {
			uint8_t aa = s->input.buf[v->rnd.arp.offset + v->rnd.arp.pos];
			s->player_info.cmd.aa = aa;
			if(aa != 0) {
				break;
			}
			if(v->rnd.arp.pos == 0) {
				return;
			}
			v->rnd.arp.pos = 0;
		}
	} else {
		tfmx_random_play_reverb(s, v);
		return;
	}

	uint16_t n = (uint16_t)(((int8_t)s->player_info.cmd.aa + v->note) & 0x3f);
	if(n == 0) {
		tfmx_random_play_mask(s, v);
		return;
	}

	uint16_t p = tfmx_note_to_period((int32_t)n);
	int16_t finetune = v->detune;
	if(finetune != 0) {
		p = (uint16_t)((((int32_t)finetune + 0x100) * (int32_t)p) >> 8);
	}

	if((v->rnd.mode & 1) == 0) {
		v->period = p;
		if(v->portamento.speed != 0) {
			return;
		}
		v->output_period = p;
		tfmx_random_play_check_wait(s, v);
		v->rnd.arp.pos++;
		return;
	}

	tfmx_randomize(s);
	if(((v->rnd.mode & 4) != 0) || ((v->rnd.arp.pos & 3) != 0) || ((s->player_info.admin.random_word & 0xff) > 16)) {
		tfmx_random_play_check_wait(s, v);
		v->period = p;
		if(v->portamento.speed == 0) {
			v->output_period = p;
		}
	}
	v->rnd.arp.pos++;
	if((s->player_info.cmd.aa & 0x40) == 0) {
		return;
	}
	tfmx_randomize(s);
	if((s->player_info.admin.random_word >> 8) > 6) {
		tfmx_random_play_mask(s, v);
		return;
	}
}

// [=]===^=[ tfmx_macro_func_random_play ]========================================================[=]
static void tfmx_macro_func_random_play(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	v->rnd.macro = c->bb;
	v->rnd.speed = (int8_t)c->cd;
	v->rnd.mode = c->ee;
	v->rnd.count = 1;
	v->rnd.flag = 1;
	v->rnd.block_wait = 1;
	tfmx_random_play_run(s, v);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_split_key ]==========================================================[=]
static void tfmx_macro_func_split_key(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(s->player_info.cmd.bb >= v->note) {
		v->macro.step++;
	} else {
		v->macro.step = tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee);
	}
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_split_volume ]=======================================================[=]
static void tfmx_macro_func_split_volume(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if((int8_t)s->player_info.cmd.bb >= v->volume) {
		v->macro.step++;
	} else {
		v->macro.step = tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee);
	}
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_random_mask ]========================================================[=]
static void tfmx_macro_func_random_mask(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->rnd.mask = s->player_info.cmd.bb;
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_set_prev_note ]======================================================[=]
static void tfmx_macro_func_set_prev_note(struct tfmx_state *s, struct tfmx_voice_state *v) {
	tfmx_macro_func_add_note_sub(s, v, v->note_previous);
	tfmx_macro_extra_wait(s, v);
}

// [=]===^=[ tfmx_macro_func_play_macro ]=========================================================[=]
static void tfmx_macro_func_play_macro(struct tfmx_state *s, struct tfmx_voice_state *v) {
	s->player_info.cmd.aa = v->note;
	s->player_info.cmd.cd = (uint8_t)(s->player_info.cmd.cd | (v->note_volume << 4));
	tfmx_note_cmd_apply(s);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_22 ]=================================================================[=]
static void tfmx_macro_func_22(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	v->add_begin_count = 0;
	v->sid.source_offset = s->offsets.sample_data + tfmx_make_dword(0, c->bb, c->cd, c->ee);
	v->sample.start = v->sid.source_offset;
	tfmx_to_paula_start(s, v, s->offsets.sample_data + v->sid.target_offset);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_23 ]=================================================================[=]
static void tfmx_macro_func_23(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	uint16_t len = tfmx_make_word(c->aa, c->bb);
	if(len == 0) {
		len = 0x100;
	}
	tfmx_to_paula_length(s, v, (uint16_t)(len >> 1));
	v->sid.target_length = (uint16_t)((len - 1) & 0xff);
	uint16_t len2 = tfmx_make_word(c->cd, c->ee);
	v->sid.source_length = len2;
	v->sample.length = len2;
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_24 ]=================================================================[=]
static void tfmx_macro_func_24(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	v->sid.op3.offset = tfmx_make_dword(c->bb, c->cd, c->ee, 0);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_25 ]=================================================================[=]
static void tfmx_macro_func_25(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	v->sid.op3.speed = tfmx_make_word(c->aa, c->bb);
	v->sid.op3.count = v->sid.op3.speed;
	v->sid.op3.delta = (int16_t)tfmx_make_word(c->cd, c->ee);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_26 ]=================================================================[=]
static void tfmx_macro_func_26(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	v->sid.op2.offset = tfmx_make_dword(0, c->bb, c->cd, c->ee);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_27 ]=================================================================[=]
static void tfmx_macro_func_27(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	v->sid.op2.speed = tfmx_make_word(c->aa, c->bb);
	v->sid.op2.count = v->sid.op2.speed;
	v->sid.op2.delta = (int16_t)tfmx_make_word(c->cd, c->ee);
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_28 ]=================================================================[=]
static void tfmx_macro_func_28(struct tfmx_state *s, struct tfmx_voice_state *v) {
	struct tfmx_cmd *c = &s->player_info.cmd;
	v->sid.op1.inter_delta = tfmx_make_word(c->ee, (uint8_t)(v->sid.op1.inter_delta & 0xff));
	v->sid.op1.inter_mod = (int16_t)(16 * (int8_t)c->cd);
	v->sid.op1.speed = tfmx_make_word(c->aa, c->bb);
	v->sid.op1.count = v->sid.op1.speed;
	v->macro.step++;
	s->player_info.macro_eval_again = 1;
}

// [=]===^=[ tfmx_macro_func_29 ]=================================================================[=]
static void tfmx_macro_func_29(struct tfmx_state *s, struct tfmx_voice_state *v) {
	v->macro.step++;
	v->sid.target_length = 0;
	if(s->player_info.cmd.bb != 0) {
		v->sid.op1.speed = 0;
		v->sid.op1.count = 0;
		v->sid.op1.inter_delta = 0;
		v->sid.op1.inter_mod = 0;
		v->sid.op2.speed = 0;
		v->sid.op2.count = 0;
		v->sid.op2.offset = 0;
		v->sid.op2.delta = 0;
		v->sid.op3.speed = 0;
		v->sid.op3.count = 0;
		v->sid.op3.offset = 0;
		v->sid.op3.delta = 0;
	}
	s->player_info.macro_eval_again = 1;
}

// Modulation routines.

// [=]===^=[ tfmx_mod_add_begin ]=================================================================[=]
static void tfmx_mod_add_begin(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(s->variant.no_add_begin_count || (v->add_begin_count == 0)) {
		return;
	}
	v->sample.start = (uint32_t)((int64_t)v->sample.start + v->add_begin_offset);
	if(v->sample.start < s->offsets.sample_data) {
		v->sample.start = s->offsets.sample_data;
	}
	if(v->sid.target_length != 0) {
		v->sid.source_offset = v->sample.start;
	} else {
		tfmx_to_paula_start(s, v, v->sample.start);
	}
	if(--v->add_begin_count == 0) {
		v->add_begin_count = v->add_begin_arg;
		v->add_begin_offset = -v->add_begin_offset;
	}
}

// [=]===^=[ tfmx_mod_envelope ]==================================================================[=]
static void tfmx_mod_envelope(struct tfmx_state *s, struct tfmx_voice_state *v) {
	(void)s;
	if(v->envelope.flag == 0) {
		return;
	}
	if(v->envelope.count > 0) {
		v->envelope.count--;
		return;
	}
	v->envelope.count = v->envelope.flag;
	if(v->volume < (int8_t)v->envelope.target) {
		v->volume = (int8_t)((int32_t)v->volume + (int32_t)v->envelope.speed);
		if(v->volume >= (int8_t)v->envelope.target) {
			v->volume = (int8_t)v->envelope.target;
			v->envelope.flag = 0;
		}
	} else {
		v->volume = (int8_t)((int32_t)v->volume - (int32_t)v->envelope.speed);
		if(v->volume <= (int8_t)v->envelope.target) {
			v->volume = (int8_t)v->envelope.target;
			v->envelope.flag = 0;
		}
	}
}

// [=]===^=[ tfmx_mod_vibrato ]===================================================================[=]
static void tfmx_mod_vibrato(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(v->vibrato.time == 0) {
		return;
	}
	v->vibrato.delta = (int16_t)((int32_t)v->vibrato.delta + (int32_t)v->vibrato.intensity);
	uint16_t p;
	if(s->variant.vibrato_unscaled) {
		p = (uint16_t)((int32_t)v->period + (int32_t)v->vibrato.delta);
	} else {
		p = (uint16_t)(((0x800 + (int32_t)v->vibrato.delta) * (int32_t)v->period) >> 11);
	}
	if(v->portamento.speed == 0) {
		v->output_period = p;
	}
	if(--v->vibrato.count == 0) {
		v->vibrato.count = v->vibrato.time;
		v->vibrato.intensity = (int8_t)(-(int32_t)v->vibrato.intensity);
	}
}

// [=]===^=[ tfmx_mod_portamento ]================================================================[=]
static void tfmx_mod_portamento(struct tfmx_state *s, struct tfmx_voice_state *v) {
	uint16_t current;
	uint16_t target;
	uint8_t do_set;

	if((v->portamento.speed == 0) || (--v->portamento.wait != 0)) {
		return;
	}
	v->portamento.wait = v->portamento.count;
	current = v->portamento.period;
	target = v->period;
	do_set = 0;
	if(current == target) {
		v->portamento.speed = 0;
		current = target;
		do_set = 1;
	} else if(current < target) {
		if(s->variant.porta_unscaled) {
			current = (uint16_t)(current + v->portamento.speed);
		} else {
			current = (uint16_t)((((int32_t)0x100 + v->portamento.speed) * (int32_t)current) >> 8);
		}
		if(current < target) {
			do_set = 1;
		} else {
			v->portamento.speed = 0;
			current = target;
			do_set = 1;
		}
	} else {
		if(s->variant.porta_unscaled) {
			current = (uint16_t)(current - v->portamento.speed);
		}
		current = (uint16_t)((((int32_t)0x100 - v->portamento.speed) * (int32_t)current) >> 8);
		if(current > target) {
			do_set = 1;
		} else {
			v->portamento.speed = 0;
			current = target;
			do_set = 1;
		}
	}
	if(do_set) {
		current &= 0x7ff;
		v->portamento.period = current;
		v->output_period = current;
	}
}

// [=]===^=[ tfmx_mod_sid ]=======================================================================[=]
static void tfmx_mod_sid(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(v->sid.target_length == 0) {
		return;
	}
	uint8_t *p_target = s->input.buf + s->offsets.sample_data + v->sid.target_offset;
	uint32_t curr_source_offset = 0;
	uint32_t x = v->sid.op3.offset;
	uint8_t d = (uint8_t)(v->sid.op1.inter_delta >> 8);
	uint8_t dx = 0;
	int16_t cur = (int16_t)v->sid.last_sample;

	int32_t i;
	for(i = (int32_t)v->sid.target_length; i >= 0; --i) {
		x += v->sid.op2.offset;
		curr_source_offset += x;
		uint32_t src_idx = v->sid.source_offset + ((curr_source_offset >> 16) & v->sid.source_length);
		if(src_idx >= s->input.len) {
			break;
		}
		int16_t sval = (int8_t)s->input.buf[src_idx];
		uint32_t tgt_idx = (uint32_t)(p_target - s->input.buf);
		if(tgt_idx + 1 >= s->input.buf_len) {
			break;
		}
		if((d == 0) || (sval == cur)) {
			p_target[0] = (uint8_t)sval;
			p_target[1] = (uint8_t)sval;
		} else if(sval > cur) {
			cur = (int16_t)((int32_t)cur + (int32_t)d + (int32_t)dx);
			dx = (uint8_t)(cur > 0x7f ? 1 : 0);
			if((cur > 0x7f) || (cur >= sval)) {
				cur = sval;
				p_target[0] = (uint8_t)sval;
				p_target[1] = (uint8_t)sval;
			} else {
				p_target[0] = (uint8_t)cur;
				p_target[1] = (uint8_t)cur;
			}
		} else if(sval < cur) {
			cur = (int16_t)((int32_t)cur - (int32_t)d - (int32_t)dx);
			dx = (uint8_t)(cur < -128 ? 1 : 0);
			if((cur < -128) || (cur <= sval)) {
				cur = sval;
				p_target[0] = (uint8_t)sval;
				p_target[1] = (uint8_t)sval;
			} else {
				p_target[0] = (uint8_t)cur;
				p_target[1] = (uint8_t)cur;
			}
		}
	}

	v->sid.last_sample = (int8_t)cur;
	if(d != 0) {
		v->sid.op1.inter_delta = (uint16_t)((int32_t)v->sid.op1.inter_delta + (int32_t)v->sid.op1.inter_mod);
		if(--v->sid.op1.count == 0) {
			v->sid.op1.count = v->sid.op1.speed;
			v->sid.op1.inter_mod = (int16_t)(-(int32_t)v->sid.op1.inter_mod);
		}
	}
	v->sid.op3.offset = (uint32_t)((int64_t)v->sid.op3.offset + v->sid.op3.delta);
	if(--v->sid.op3.count == 0) {
		v->sid.op3.count = v->sid.op3.speed;
		if(v->sid.op3.count != 0) {
			v->sid.op3.delta = (int16_t)(-(int32_t)v->sid.op3.delta);
		}
	}
	v->sid.op2.offset = (uint32_t)((int64_t)v->sid.op2.offset + v->sid.op2.delta);
	if(--v->sid.op2.count == 0) {
		v->sid.op2.count = v->sid.op2.speed;
		if(v->sid.op2.count != 0) {
			v->sid.op2.delta = (int16_t)(-(int32_t)v->sid.op2.delta);
		}
	}
}

// [=]===^=[ tfmx_fade_init ]=====================================================================[=]
static void tfmx_fade_init(struct tfmx_state *s, uint8_t target, uint8_t speed) {
	if(s->player_info.fade.active) {
		return;
	}
	s->player_info.fade.active = 1;
	s->player_info.fade.target = target;
	s->player_info.fade.count = speed;
	s->player_info.fade.speed = speed;
	if((s->player_info.fade.speed == 0) || (s->player_info.fade.volume == s->player_info.fade.target)) {
		s->player_info.fade.volume = s->player_info.fade.target;
		s->player_info.fade.delta = 0;
		s->player_info.fade.active = 0;
		return;
	}
	if(s->player_info.fade.volume < s->player_info.fade.target) {
		s->player_info.fade.delta = 1;
	} else {
		s->player_info.fade.delta = -1;
	}
}

// [=]===^=[ tfmx_fade_apply ]====================================================================[=]
static void tfmx_fade_apply(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if((s->player_info.fade.delta != 0) && (--s->player_info.fade.count == 0)) {
		s->player_info.fade.count = s->player_info.fade.speed;
		s->player_info.fade.volume = (uint8_t)((int32_t)s->player_info.fade.volume + s->player_info.fade.delta);
		if(s->player_info.fade.volume == s->player_info.fade.target) {
			s->player_info.fade.delta = 0;
			s->player_info.fade.active = 0;
		}
	}
	int8_t vol = v->volume;
	if(s->player_info.fade.volume < 64) {
		vol = (int8_t)((4 * (int32_t)v->volume * (int32_t)s->player_info.fade.volume) / (4 * 0x40));
	}
	v->paula_volume = (uint16_t)vol;
}

// [=]===^=[ tfmx_process_modulation ]============================================================[=]
static void tfmx_process_modulation(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(v->effects_mode > 0) {
		tfmx_mod_add_begin(s, v);
		tfmx_mod_sid(s, v);
		tfmx_mod_vibrato(s, v);
		tfmx_mod_portamento(s, v);
		tfmx_mod_envelope(s, v);
	} else if(v->effects_mode == 0) {
		v->effects_mode = 1;
	}
	tfmx_random_play_run(s, v);
	tfmx_fade_apply(s, v);
}

// Macro dispatch table - constructed at runtime in tfmx_setup_dispatch().

// [=]===^=[ tfmx_macro_dispatch ]================================================================[=]
static void tfmx_macro_dispatch(struct tfmx_state *s, struct tfmx_voice_state *v, uint8_t cmd) {
	uint8_t c = (uint8_t)(cmd & 0x3f);
	switch(c) {
		case 0x00: { tfmx_macro_func_stop_sound(s, v); break; }
		case 0x01: { tfmx_macro_func_start_sample(s, v); break; }
		case 0x02: { tfmx_macro_func_set_begin(s, v); break; }
		case 0x03: { tfmx_macro_func_set_len(s, v); break; }
		case 0x04: { tfmx_macro_func_wait(s, v); break; }
		case 0x05: { tfmx_macro_func_loop(s, v); break; }
		case 0x06: { tfmx_macro_func_cont(s, v); break; }
		case 0x07: { tfmx_macro_func_stop(s, v); break; }
		case 0x08: { tfmx_macro_func_add_note(s, v); break; }
		case 0x09: { tfmx_macro_func_set_note(s, v); break; }
		case 0x0a: { tfmx_macro_func_reset(s, v); break; }
		case 0x0b: { tfmx_macro_func_portamento(s, v); break; }
		case 0x0c: { tfmx_macro_func_vibrato(s, v); break; }
		case 0x0d: {
			if(s->variant.compressed) {
				tfmx_macro_func_add_volume(s, v);
			} else {
				tfmx_macro_func_add_vol_note(s, v);
			}
			break;
		}
		case 0x0e: { tfmx_macro_func_set_volume(s, v); break; }
		case 0x0f: { tfmx_macro_func_envelope(s, v); break; }
		case 0x10: { tfmx_macro_func_loop_keyup(s, v); break; }
		case 0x11: { tfmx_macro_func_add_begin(s, v); break; }
		case 0x12: { tfmx_macro_func_add_len(s, v); break; }
		case 0x13: { tfmx_macro_func_stop_sample(s, v); break; }
		case 0x14: { tfmx_macro_func_wait_keyup(s, v); break; }
		case 0x15: { tfmx_macro_func_goto(s, v); break; }
		case 0x16: { tfmx_macro_func_return(s, v); break; }
		case 0x17: { tfmx_macro_func_set_period(s, v); break; }
		case 0x18: { tfmx_macro_func_sample_loop(s, v); break; }
		case 0x19: { tfmx_macro_func_one_shot(s, v); break; }
		case 0x1a: { tfmx_macro_func_wait_on_dma(s, v); break; }
		case 0x1b: { tfmx_macro_func_random_play(s, v); break; }
		case 0x1c: { tfmx_macro_func_split_key(s, v); break; }
		case 0x1d: { tfmx_macro_func_split_volume(s, v); break; }
		case 0x1e: { tfmx_macro_func_random_mask(s, v); break; }
		case 0x1f: { tfmx_macro_func_set_prev_note(s, v); break; }
		case 0x21: { tfmx_macro_func_play_macro(s, v); break; }
		case 0x22: { tfmx_macro_func_22(s, v); break; }
		case 0x23: { tfmx_macro_func_23(s, v); break; }
		case 0x24: { tfmx_macro_func_24(s, v); break; }
		case 0x25: { tfmx_macro_func_25(s, v); break; }
		case 0x26: { tfmx_macro_func_26(s, v); break; }
		case 0x27: { tfmx_macro_func_27(s, v); break; }
		case 0x28: { tfmx_macro_func_28(s, v); break; }
		case 0x29: { tfmx_macro_func_29(s, v); break; }

		default: { tfmx_macro_func_nop(s, v); break; }
	}
	s->macro_cmd_used[c] = 1;
}

// [=]===^=[ tfmx_process_macro_main ]============================================================[=]
static void tfmx_process_macro_main(struct tfmx_state *s, struct tfmx_voice_state *v) {
	if(v->macro.skip) {
		return;
	}
	if(v->macro.wait > 0) {
		v->macro.wait--;
		return;
	}

	int32_t macro_len = 0;
	do {
		s->player_info.macro_eval_again = 0;
		uint32_t p = v->macro.offset + (v->macro.step << 2);
		if(p + 3 >= s->input.len) {
			v->macro.skip = 1;
			return;
		}
		uint8_t command = s->input.buf[p];
		s->player_info.cmd.aa = 0;
		s->player_info.cmd.bb = s->input.buf[p + 1];
		s->player_info.cmd.cd = s->input.buf[p + 2];
		s->player_info.cmd.ee = s->input.buf[p + 3];
		tfmx_macro_dispatch(s, v, command);
	} while(s->player_info.macro_eval_again && (++macro_len < 32));
}

// Pattern command handlers.

// [=]===^=[ tfmx_patt_cmd_nop ]==================================================================[=]
static void tfmx_patt_cmd_nop(struct tfmx_state *s, struct tfmx_track *tr) {
	(void)s;
	tr->pattern.step++;
	tr->pattern.eval_next = 1;
}

// [=]===^=[ tfmx_process_track_step_dispatch ]===================================================[=]
// We call into ProcessTrackStep recursively, so define forward via the
// "dispatch" trampoline declared near the bottom; here we forward-declare
// only via the same trick: a function pointer set during init.
// To keep the "no forward declarations" rule, we avoid that and instead
// embed the small required logic inline through a re-entrant guard.
//
// In practice, PattCmd_End triggers a track-step advance which we set
// via player_info.sequencer.step.next; the main Run loop re-invokes
// ProcessTrackStep when that flag is set.

// [=]===^=[ tfmx_patt_cmd_end ]==================================================================[=]
static void tfmx_patt_cmd_end(struct tfmx_state *s, struct tfmx_track *tr) {
	tr->pt = 0xff;
	if(s->player_info.sequencer.step.current != s->player_info.sequencer.step.last) {
		s->player_info.sequencer.step.current++;
	}
	if(s->player_info.sequencer.step.current == s->player_info.sequencer.step.last) {
		s->song_end = 1;
		s->trigger_restart = 1;
		return;
	}
	s->player_info.sequencer.step.next = 1;
}

// [=]===^=[ tfmx_patt_cmd_loop ]=================================================================[=]
static void tfmx_patt_cmd_loop(struct tfmx_state *s, struct tfmx_track *tr) {
	if(tr->pattern.loops == 0) {
		tr->pattern.loops = -1;
		tr->pattern.step++;
		tr->pattern.eval_next = 1;
		return;
	} else if(tr->pattern.loops == -1) {
		tr->pattern.loops = (int8_t)(s->player_info.cmd.bb - 1);
		if(tr->pattern.loops == -1) {
			tr->pattern.infinite_loop = 1;
		}
	} else {
		tr->pattern.loops--;
	}
	tr->pattern.step = tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee);
	tr->pattern.eval_next = 1;
}

// [=]===^=[ tfmx_patt_cmd_goto ]=================================================================[=]
static void tfmx_patt_cmd_goto(struct tfmx_state *s, struct tfmx_track *tr) {
	tr->pt = s->player_info.cmd.bb;
	tr->pattern.offset = tfmx_get_patt_offset(s, tr->pt);
	tr->pattern.step = tfmx_make_word(s->player_info.cmd.cd, s->player_info.cmd.ee);
	tr->pattern.eval_next = 1;
}

// [=]===^=[ tfmx_patt_cmd_wait ]=================================================================[=]
static void tfmx_patt_cmd_wait(struct tfmx_state *s, struct tfmx_track *tr) {
	tr->pattern.wait = s->player_info.cmd.bb;
	tr->pattern.step++;
}

// [=]===^=[ tfmx_patt_cmd_stop ]=================================================================[=]
static void tfmx_patt_cmd_stop(struct tfmx_state *s, struct tfmx_track *tr) {
	(void)s;
	tr->pt = 0xff;
}

// [=]===^=[ tfmx_patt_cmd_note ]=================================================================[=]
static void tfmx_patt_cmd_note(struct tfmx_state *s, struct tfmx_track *tr) {
	tfmx_note_cmd_apply(s);
	tr->pattern.step++;
	tr->pattern.eval_next = 1;
}

// [=]===^=[ tfmx_patt_cmd_save_and_goto ]========================================================[=]
static void tfmx_patt_cmd_save_and_goto(struct tfmx_state *s, struct tfmx_track *tr) {
	tr->pattern.offset_saved = tr->pattern.offset;
	tr->pattern.step_saved = tr->pattern.step;
	tfmx_patt_cmd_goto(s, tr);
}

// [=]===^=[ tfmx_patt_cmd_return_from_goto ]=====================================================[=]
static void tfmx_patt_cmd_return_from_goto(struct tfmx_state *s, struct tfmx_track *tr) {
	(void)s;
	tr->pattern.offset = tr->pattern.offset_saved;
	tr->pattern.step = tr->pattern.step_saved;
	tr->pattern.step++;
	tr->pattern.eval_next = 1;
}

// [=]===^=[ tfmx_patt_cmd_fade ]=================================================================[=]
static void tfmx_patt_cmd_fade(struct tfmx_state *s, struct tfmx_track *tr) {
	tfmx_fade_init(s, s->player_info.cmd.ee, s->player_info.cmd.bb);
	tr->pattern.step++;
	tr->pattern.eval_next = 1;
}

// [=]===^=[ tfmx_patt_dispatch ]=================================================================[=]
static void tfmx_patt_dispatch(struct tfmx_state *s, struct tfmx_track *tr, uint8_t command) {
	switch(command) {
		case 0x0: { tfmx_patt_cmd_end(s, tr); break; }
		case 0x1: { tfmx_patt_cmd_loop(s, tr); break; }
		case 0x2: { tfmx_patt_cmd_goto(s, tr); break; }
		case 0x3: { tfmx_patt_cmd_wait(s, tr); break; }
		case 0x4: { tfmx_patt_cmd_stop(s, tr); break; }
		case 0x5: { tfmx_patt_cmd_note(s, tr); break; }
		case 0x6: { tfmx_patt_cmd_note(s, tr); break; }
		case 0x7: { tfmx_patt_cmd_note(s, tr); break; }
		case 0x8: { tfmx_patt_cmd_save_and_goto(s, tr); break; }
		case 0x9: { tfmx_patt_cmd_return_from_goto(s, tr); break; }
		case 0xa: { tfmx_patt_cmd_fade(s, tr); break; }
		case 0xc: { tfmx_patt_cmd_note(s, tr); break; }
		case 0xe: { tfmx_patt_cmd_stop(s, tr); break; }

		default: { tfmx_patt_cmd_nop(s, tr); break; }
	}
}

// [=]===^=[ tfmx_process_pattern ]===============================================================[=]
static void tfmx_process_pattern(struct tfmx_state *s, struct tfmx_track *tr) {
	int32_t eval_max_loops = TFMX_RECURSE_LIMIT;
	do {
		tr->pattern.eval_next = 0;
		uint32_t p = tr->pattern.offset + ((uint32_t)tr->pattern.step << 2);
		if(p + 3 >= s->input.len) {
			tr->pt = 0xff;
			return;
		}
		s->player_info.cmd.aa = s->input.buf[p];
		s->player_info.cmd.bb = s->input.buf[p + 1];
		s->player_info.cmd.cd = s->input.buf[p + 2];
		s->player_info.cmd.ee = s->input.buf[p + 3];
		uint8_t aa_bak = s->player_info.cmd.aa;
		if(s->player_info.cmd.aa < 0xf0) {
			if((s->player_info.cmd.aa < 0xc0) && (s->player_info.cmd.aa >= 0x7f)) {
				tr->pattern.wait = s->player_info.cmd.ee;
				s->player_info.cmd.ee = 0;
			}
			s->player_info.cmd.aa = (uint8_t)(s->player_info.cmd.aa + tr->tr);
			if(aa_bak < 0xc0) {
				s->player_info.cmd.aa &= 0x3f;
			}
			if(tr->on) {
				tfmx_note_cmd_apply(s);
			}
			if((aa_bak >= 0xc0) || (aa_bak < 0x7f)) {
				tr->pattern.step++;
				tr->pattern.eval_next = 1;
			} else {
				tr->pattern.step++;
			}
		} else {
			uint8_t command = (uint8_t)(s->player_info.cmd.aa & 0xf);
			s->pattern_cmd_used[command] = 1;
			tfmx_patt_dispatch(s, tr, command);
		}
	} while(tr->pattern.eval_next && (--eval_max_loops > 0));
}

// [=]===^=[ tfmx_process_pttr ]==================================================================[=]
static void tfmx_process_pttr(struct tfmx_state *s, struct tfmx_track *tr) {
	if(tr->pt < 0x90) {
		if(tr->pattern.offset == 0) {
			tr->pt = 0xff;
			return;
		}
		if(tr->pattern.wait == 0) {
			tfmx_process_pattern(s, tr);
		} else {
			tr->pattern.wait--;
		}
	} else {
		if(tr->pt == 0xfe) {
			tr->pt = 0xff;
			uint8_t v_num = s->channel_to_voice_map[tr->tr & (TFMX_VOICES_MAX - 1)];
			struct tfmx_voice_state *v = &s->voices[v_num];
			v->macro.skip = 1;
			tfmx_paula_off(s, v);
		}
	}
}

// Track command handlers.

// [=]===^=[ tfmx_track_cmd_stop ]================================================================[=]
static void tfmx_track_cmd_stop(struct tfmx_state *s, uint32_t step_offset) {
	(void)step_offset;
	s->song_end = 1;
	s->trigger_restart = 1;
}

// [=]===^=[ tfmx_track_cmd_loop ]================================================================[=]
static void tfmx_track_cmd_loop(struct tfmx_state *s, uint32_t step_offset) {
	if(s->player_info.sequencer.loops == 0) {
		s->player_info.sequencer.loops = -1;
		s->player_info.sequencer.step.current++;
	} else if(s->player_info.sequencer.loops < 0) {
		s->player_info.sequencer.step.current = (int32_t)tfmx_read_be_u16(s->input.buf, step_offset);
		s->player_info.sequencer.loops = (int16_t)(tfmx_read_be_u16(s->input.buf, step_offset + 2) - 1);
		if((s->player_info.sequencer.step.current > (TFMX_TRACK_STEPS_MAX - 1)) ||
			(s->player_info.sequencer.step.current > s->player_info.sequencer.step.last)) {
			s->song_end = 1;
			s->trigger_restart = 1;
		} else if(s->player_info.sequencer.step_seen_before[s->player_info.sequencer.step.current] && (s->player_info.sequencer.loops < 0)) {
			s->song_end = 1;
		}
		if((s->player_info.sequencer.loops == 0xeff) || (s->player_info.sequencer.loops > 0x100)) {
			s->player_info.sequencer.loops = 0;
		}
	} else {
		s->player_info.sequencer.loops--;
		s->player_info.sequencer.step.current = (int32_t)tfmx_read_be_u16(s->input.buf, step_offset);
	}
	s->player_info.sequencer.eval_next = 1;
}

// [=]===^=[ tfmx_track_cmd_speed ]===============================================================[=]
static void tfmx_track_cmd_speed(struct tfmx_state *s, uint32_t step_offset) {
	s->player_info.admin.speed = (int16_t)tfmx_read_be_u16(s->input.buf, step_offset);
	s->player_info.admin.count = s->player_info.admin.speed;
	uint16_t arg2 = (uint16_t)(0x81ff & tfmx_read_be_u16(s->input.buf, step_offset + 2));
	if((arg2 != 0) && (arg2 < 0x200)) {
		if(arg2 < 32) {
			arg2 = 125;
		}
		tfmx_set_rate(s, arg2);
	}
	s->player_info.sequencer.step.current++;
	s->player_info.sequencer.eval_next = 1;
}

// [=]===^=[ tfmx_track_cmd_7v ]==================================================================[=]
static void tfmx_track_cmd_7v(struct tfmx_state *s, uint32_t step_offset) {
	int16_t arg2 = (int16_t)tfmx_read_be_u16(s->input.buf, step_offset + 2);
	if(arg2 >= 0) {
		int8_t x = (int8_t)s->input.buf[step_offset + 3];
		if(x < -0x20) {
			x = -0x20;
		}
		tfmx_set_bpm(s, (uint16_t)((125 * 100) / (100 + x)));
	}
	s->track_cmd_used[3] = 1;
	s->player_info.sequencer.step.current++;
	s->player_info.sequencer.eval_next = 1;
}

// [=]===^=[ tfmx_track_cmd_fade ]================================================================[=]
static void tfmx_track_cmd_fade(struct tfmx_state *s, uint32_t step_offset) {
	tfmx_fade_init(s, s->input.buf[step_offset + 3], s->input.buf[step_offset + 1]);
	s->player_info.sequencer.step.current++;
	s->player_info.sequencer.eval_next = 1;
}

// [=]===^=[ tfmx_track_dispatch ]================================================================[=]
static void tfmx_track_dispatch(struct tfmx_state *s, uint8_t command, uint32_t step_offset) {
	switch(command) {
		case 0: { tfmx_track_cmd_stop(s, step_offset); break; }
		case 1: { tfmx_track_cmd_loop(s, step_offset); break; }
		case 2: { tfmx_track_cmd_speed(s, step_offset); break; }
		case 3: { tfmx_track_cmd_7v(s, step_offset); break; }
		case 4: { tfmx_track_cmd_fade(s, step_offset); break; }

		default: { tfmx_track_cmd_stop(s, step_offset); break; }
	}
	s->track_cmd_used[command] = 1;
}

// [=]===^=[ tfmx_process_track_step ]============================================================[=]
static void tfmx_process_track_step(struct tfmx_state *s) {
	int32_t eval_max_loops = TFMX_RECURSE_LIMIT;
	do {
		s->player_info.sequencer.eval_next = 0;
		if(s->player_info.sequencer.step.current > (TFMX_TRACK_STEPS_MAX - 1)) {
			s->player_info.sequencer.step.current = s->player_info.sequencer.step.first;
		}
		s->player_info.sequencer.step_seen_before[s->player_info.sequencer.step.current] = 1;
		uint32_t step_offset = s->offsets.track_table + ((uint32_t)s->player_info.sequencer.step.current << 4);
		if(step_offset + 16 > s->input.len) {
			s->song_end = 1;
			s->trigger_restart = 1;
			return;
		}
		if(0xeffe == tfmx_read_be_u16(s->input.buf, step_offset)) {
			step_offset += 2;
			uint16_t command = tfmx_read_be_u16(s->input.buf, step_offset);
			step_offset += 2;
			if(command > TFMX_TRACK_CMD_MAX) {
				command = 0;
			}
			tfmx_track_dispatch(s, (uint8_t)command, step_offset);
		} else {
			uint8_t t;
			for(t = 0; t < s->player_info.sequencer.tracks; ++t) {
				struct tfmx_track *tr = &s->player_info.track[t];
				tr->pt = s->input.buf[step_offset++];
				tr->tr = (int8_t)s->input.buf[step_offset++];
				if(tr->pt < 0x80) {
					tr->pattern.offset = tfmx_get_patt_offset(s, tr->pt);
					tr->pattern.step = 0;
					tr->pattern.wait = 0;
					tr->pattern.loops = -1;
					tr->pattern.infinite_loop = 0;
				}
			}
		}
	} while(s->player_info.sequencer.eval_next && (--eval_max_loops > 0));
}

// [=]===^=[ tfmx_set_tfmx_v1 ]===================================================================[=]
static void tfmx_set_tfmx_v1(struct tfmx_state *s) {
	s->variant.no_add_begin_count = 1;
	s->variant.vibrato_unscaled = 1;
	s->variant.finetune_unscaled = 1;
	s->variant.porta_unscaled = 0;
	s->variant.porta_override = 1;
	// In V1 mode, dispatch case 0x0d uses AddVolume directly.
	// We model this via a flag in dispatch (variant.compressed acts as the
	// flag in our implementation, but here we explicitly want AddVolume).
	// Re-use compressed flag for that switching.
	s->variant.compressed = 1;
}

// [=]===^=[ tfmx_traits_by_checksum ]============================================================[=]
static void tfmx_traits_by_checksum(struct tfmx_state *s) {
	uint32_t p0 = s->offsets.header + tfmx_read_be_u32(s->input.buf, s->offsets.patterns);
	if(p0 + 0x100 > s->input.len) {
		return;
	}
	uint32_t crc1 = tfmx_crc32(s->input.buf, p0, 0x100);
	if((crc1 == 0x48960d8c) || (crc1 == 0x5dcd624f) || (crc1 == 0x3f0b151f)) {
		tfmx_set_tfmx_v1(s);
		s->variant.no_note_detune = 1;
		s->variant.porta_unscaled = 0;
	} else if((crc1 == 0x27f8998c) || (crc1 == 0x26447707) || (crc1 == 0xd404651b) || (crc1 == 0xb5348633)) {
		tfmx_set_tfmx_v1(s);
		s->variant.porta_unscaled = 1;
	} else if(crc1 == 0x8ac70fc8) {
		tfmx_set_tfmx_v1(s);
	} else if(crc1 == 0xa8566760) {
		s->variant.no_track_mute = 1;
	} else if(crc1 == 0xab1a6c6e) {
		s->variant.no_track_mute = 1;
	} else if(crc1 == 0x76f8aa6e) {
		s->variant.bpm_speed5 = 1;
	}
}

// [=]===^=[ tfmx_reset_voices ]==================================================================[=]
static void tfmx_reset_voices(struct tfmx_state *s) {
	uint8_t t;
	uint8_t v;

	s->player_info.cmd.aa = 0;
	s->player_info.cmd.bb = 0;
	s->player_info.cmd.cd = 0;
	s->player_info.cmd.ee = 0;

	for(t = 0; t < s->player_info.sequencer.tracks; ++t) {
		struct tfmx_track *tr = &s->player_info.track[t];
		tr->on = tfmx_get_track_mute(s, t);
		tr->pt = 0xff;
		tr->tr = 0;
		tr->pattern.offset = 0;
		tr->pattern.step = 0;
		tr->pattern.wait = 0;
		tr->pattern.loops = -1;
		tr->pattern.infinite_loop = 0;
	}

	for(v = 0; v < (uint8_t)s->voice_count; ++v) {
		struct tfmx_voice_state *vs = &s->voices[v];
		memset(vs, 0, sizeof(*vs));
		vs->voice_num = v;
		vs->macro.wait = 1;
		vs->macro.skip = 1;
		vs->macro.loop = 0xff;
		vs->macro.extra_wait = 1;
		vs->key_up = 1;
		vs->wait_on_dma_count = -1;
		vs->sid.target_offset = (uint32_t)(0x100u * v) + 4u;
		tfmx_paula_off(s, vs);
		tfmx_to_paula_length(s, vs, 1);
		tfmx_to_paula_start(s, vs, s->offsets.silence);
		vs->paula_volume = 0;
		vs->paula_period = 0;
	}

	memset(s->track_cmd_used, 0, sizeof(s->track_cmd_used));
	memset(s->pattern_cmd_used, 0, sizeof(s->pattern_cmd_used));
	memset(s->macro_cmd_used, 0, sizeof(s->macro_cmd_used));
}

// [=]===^=[ tfmx_soft_restart ]==================================================================[=]
static void tfmx_soft_restart(struct tfmx_state *s) {
	uint8_t v;

	s->song_end = 0;
	s->tick_fp_add = 0;
	s->trigger_restart = 0;
	s->frames_until_tick_fp = 0;
	s->player_info.sequencer.step.next = 0;
	s->player_info.sequencer.loops = -1;
	memset(s->player_info.sequencer.step_seen_before, 0, sizeof(s->player_info.sequencer.step_seen_before));

	for(v = 0; v < (uint8_t)s->voice_count; ++v) {
		s->voices[v].key_up = 1;
		s->voices[v].volume = 0;
	}

	s->player_info.fade.active = 0;
	s->player_info.fade.volume = 64;
	s->player_info.fade.target = 64;
	s->player_info.fade.delta = 0;

	uint32_t so = (uint32_t)s->v_songs[s->player_info.admin.start_song] << 1;
	s->player_info.sequencer.step.first = (int32_t)tfmx_read_be_u16(s->input.buf, s->offsets.header + 0x100u + so);
	s->player_info.sequencer.step.current = s->player_info.sequencer.step.first;
	s->player_info.sequencer.step.last = (int32_t)tfmx_read_be_u16(s->input.buf, s->offsets.header + 0x140u + so);
	s->player_info.admin.speed = (int16_t)tfmx_read_be_u16(s->input.buf, s->offsets.header + 0x180u + so);

	if(s->player_info.admin.speed >= 0x10) {
		tfmx_set_bpm(s, (uint16_t)s->player_info.admin.speed);
		s->player_info.admin.speed = 0;
		if(s->variant.bpm_speed5) {
			s->player_info.admin.speed = 5;
		}
	}
	s->player_info.admin.start_speed = s->player_info.admin.speed;
	s->player_info.admin.count = 0;

	tfmx_process_track_step(s);
}

// [=]===^=[ tfmx_restart ]=======================================================================[=]
static void tfmx_restart(struct tfmx_state *s) {
	tfmx_reset_voices(s);
	tfmx_soft_restart(s);
}

// [=]===^=[ tfmx_run ]===========================================================================[=]
// One player tick. Mirrors TfmxDecoder.Run: advances macros, modulation,
// applies new period/volume to paula, advances the track sequencer when
// the speed counter hits zero.
static void tfmx_run(struct tfmx_state *s) {
	uint8_t v;

	if(!s->player_info.admin.initialized) {
		return;
	}

	s->real_song_end = 0;

	for(v = 0; v < (uint8_t)s->voice_count; ++v) {
		if(!s->song_end || s->loop_mode) {
			struct tfmx_voice_state *vs = &s->voices[v];
			if(vs->wait_on_dma_count >= 0) {
				uint16_t x = vs->paula_loop_count;
				uint16_t y = vs->wait_on_dma_prev_loops;
				int32_t d;
				if(x >= y) {
					d = (int32_t)(x - y);
				} else {
					d = (int32_t)x + (0x10000 - (int32_t)y);
				}
				if(d > vs->wait_on_dma_count) {
					vs->macro.skip = 0;
					vs->wait_on_dma_count = -1;
				} else {
					vs->wait_on_dma_count = (int16_t)((int32_t)vs->wait_on_dma_count - d);
					vs->wait_on_dma_prev_loops = vs->paula_loop_count;
				}
			}
			tfmx_process_macro_main(s, vs);
			tfmx_process_modulation(s, vs);
			vs->paula_period = vs->output_period;
		}
	}

	if(!s->song_end || s->loop_mode) {
		if(--s->player_info.admin.count < 0) {
			s->player_info.admin.count = s->player_info.admin.speed;
			do {
				s->player_info.sequencer.step.next = 0;
				int32_t count_inactive = 0;
				int32_t count_infinite = 0;
				uint8_t t;
				for(t = 0; t < s->player_info.sequencer.tracks; ++t) {
					struct tfmx_track *tr = &s->player_info.track[t];
					tr->on = tfmx_get_track_mute(s, t);
					if(tr->pt >= 0x90) {
						count_inactive++;
					} else if(tr->pattern.infinite_loop) {
						count_infinite++;
					}
					tfmx_process_pttr(s, tr);
					if(s->player_info.sequencer.step.next) {
						break;
					}
				}
				if(!s->player_info.sequencer.step.next) {
					if((count_inactive == s->player_info.sequencer.tracks) ||
						((count_inactive + count_infinite) == s->player_info.sequencer.tracks)) {
						s->song_end = 1;
						s->trigger_restart = 1;
					}
				}
				if(s->player_info.sequencer.step.next) {
					tfmx_process_track_step(s);
				}
			} while(s->player_info.sequencer.step.next);
		}
	}

	if(s->song_end && s->loop_mode) {
		s->song_end = 0;
		if(s->trigger_restart) {
			tfmx_soft_restart(s);
		}
		s->real_song_end = 1;
	}

	// Push period/volume to paula channels each tick.
	for(v = 0; v < (uint8_t)s->voice_count; ++v) {
		struct tfmx_voice_state *vs = &s->voices[v];
		if(vs->paula_dma_on) {
			paula_set_period(&s->paula, (int32_t)vs->voice_num, vs->paula_period == 0 ? 0x100 : vs->paula_period);
			uint16_t vol = vs->paula_volume > 64 ? 64 : vs->paula_volume;
			paula_set_volume(&s->paula, (int32_t)vs->voice_num, vol);
		}
		// Track Paula loop count for WaitOnDMA: increment when the channel
		// has wrapped at least one full length while DMA-on. We approximate
		// this by simply incrementing once per tick if the channel is on.
		if(vs->paula_dma_on) {
			vs->paula_loop_count++;
		}
	}
}

// [=]===^=[ tfmx_find_songs ]====================================================================[=]
// Mirrors FindSongs(): collects valid (sub-)song definitions from the song
// table at offset 0x100 / 0x140 / 0x180.
static void tfmx_find_songs(struct tfmx_state *s) {
	int32_t so;
	s->v_songs_count = 0;

	struct seen { int32_t s1; int32_t s2; int32_t s3; };
	struct seen seen[32];
	uint32_t seen_count = 0;

	for(so = 0; so < 32; ++so) {
		int32_t s1 = (int32_t)tfmx_read_be_u16(s->input.buf, s->offsets.header + 0x100u + ((uint32_t)so << 1));
		int32_t s2 = (int32_t)tfmx_read_be_u16(s->input.buf, s->offsets.header + 0x140u + ((uint32_t)so << 1));
		int32_t s3 = (int32_t)tfmx_read_be_u16(s->input.buf, s->offsets.header + 0x180u + ((uint32_t)so << 1));
		if((s1 > s2) || (s1 > 0x1ff) || (s2 > 0x1ff) || ((so > 1) && ((s1 == 0x1ff) || (s2 == 0x1ff)))) {
			continue;
		}
		uint8_t skip = 0;
		uint32_t i;
		for(i = 0; i < seen_count; ++i) {
			if((seen[i].s1 == s1) && (s2 == seen[i].s1)) {
				skip = 1;
				break;
			}
		}
		if(skip) {
			continue;
		}
		uint8_t dup = 0;
		for(i = 0; i < seen_count; ++i) {
			if((seen[i].s1 == s1) && (seen[i].s2 == s2) && (seen[i].s3 == s3)) {
				dup = 1;
				break;
			}
		}
		if(!dup && (s->v_songs_count < sizeof(s->v_songs))) {
			s->v_songs[s->v_songs_count++] = (uint8_t)so;
			if(seen_count < 32) {
				seen[seen_count].s1 = s1;
				seen[seen_count].s2 = s2;
				seen[seen_count].s3 = s3;
				seen_count++;
			}
		}
	}
}

// [=]===^=[ tfmx_detect_single_file ]============================================================[=]
// Returns header offset for single-file formats (TFHD/TFMXPAK/TFMX-MOD), or 0
// for "no single-file header" (in which case the data starts at offset 0
// with mdat content). Sets input.mdat_size / smpl_size / version_hint /
// start_song_hint when the format provides them.
static uint32_t tfmx_detect_single_file(struct tfmx_state *s) {
	uint8_t *buf = s->input.buf;
	uint32_t len = s->input.len;
	s->input.version_hint = 0;
	s->input.start_song_hint = -1;
	s->input.mdat_size = 0;
	s->input.smpl_size = 0;

	if((len >= 0x12) && (buf[0] == 'T') && (buf[1] == 'F') && (buf[2] == 'H') && (buf[3] == 'D')) {
		if(tfmx_read_be_u16(buf, 4) != 0) {
			return 0;
		}
		uint32_t hdr = tfmx_read_be_u32(buf, 4);
		s->input.version_hint = buf[8];
		s->input.mdat_size = tfmx_read_be_u32(buf, 10);
		s->input.smpl_size = tfmx_read_be_u32(buf, 14);
		if((s->input.mdat_size == 0) || (s->input.smpl_size == 0)) {
			return 0;
		}
		return hdr;
	}

	if((len >= 32) && (buf[0] == 'T') && (buf[1] == 'F') && (buf[2] == 'M') && (buf[3] == 'X') && (buf[4] == 'P') && (buf[5] == 'A') && (buf[6] == 'K')) {
		uint32_t i;
		uint32_t mdat = 0;
		uint32_t smpl = 0;
		for(i = 7; i < 32; ++i) {
			while((i < 32) && (buf[i] == ' ')) {
				++i;
			}
			uint32_t n = 0;
			while((i < 32) && (buf[i] >= '0') && (buf[i] <= '9')) {
				n = n * 10 + (uint32_t)(buf[i] - '0');
				++i;
			}
			if(mdat == 0) {
				mdat = n;
			} else if(smpl == 0) {
				smpl = n;
			} else {
				break;
			}
			if((i < 32) && (buf[i] == '>')) {
				break;
			}
		}
		for(i = 8; i < 32; ++i) {
			if((i + 2 < len) && (buf[i] == '>') && (buf[i + 1] == '>') && (buf[i + 2] == '>')) {
				if((mdat == 0) || (smpl == 0)) {
					return 0;
				}
				s->input.mdat_size = mdat;
				s->input.smpl_size = smpl;
				return i + 3;
			}
		}
		return 0;
	}

	if((len >= 16) && (buf[0] == 'T') && (buf[1] == 'F') && (buf[2] == 'M') && (buf[3] == 'X') && (buf[4] == '-') && (buf[5] == 'M') && (buf[6] == 'O') && (buf[7] == 'D')) {
		// TFMX-MOD uses LITTLE-endian offsets.
		if((buf[11] != 0) || (buf[15] != 0)) {
			return 0;
		}
		uint32_t sample_data = tfmx_make_dword(buf[11], buf[10], buf[9], buf[8]);
		uint32_t hdr = 8 + 12;
		s->input.mdat_size = sample_data - hdr;
		uint32_t offs = tfmx_make_dword(buf[15], buf[14], buf[13], buf[12]);
		s->input.smpl_size = offs - s->input.mdat_size;
		// We could parse the meta blocks here for start_song_hint; skipped.
		(void)offs;
		s->offsets.sample_data = sample_data;
		return hdr;
	}

	return 0;
}

// [=]===^=[ tfmx_detect_naked ]==================================================================[=]
// Detect a non-tagged TFMX (just starts with "TFMX " or "TFMX-SONG" /
// "TFMX_SONG" or lowercase "tfmxsong" tag).
static uint8_t tfmx_detect_naked(uint8_t *buf, uint32_t len) {
	if(len < 16) {
		return 0;
	}
	if((buf[0] == 'T') && (buf[1] == 'F') && (buf[2] == 'M') && (buf[3] == 'X') && (buf[4] == 0x20)) {
		return 1;
	}
	if((buf[0] == 'T') && (buf[1] == 'F') && (buf[2] == 'M') && (buf[3] == 'X') && (buf[4] == '-') &&
		(buf[5] == 'S') && (buf[6] == 'O') && (buf[7] == 'N') && (buf[8] == 'G')) {
		return 1;
	}
	if((buf[0] == 'T') && (buf[1] == 'F') && (buf[2] == 'M') && (buf[3] == 'X') && (buf[4] == '_') &&
		(buf[5] == 'S') && (buf[6] == 'O') && (buf[7] == 'N') && (buf[8] == 'G')) {
		return 1;
	}
	if((buf[0] == 't') && (buf[1] == 'f') && (buf[2] == 'm') && (buf[3] == 'x') &&
		(buf[4] == 's') && (buf[5] == 'o') && (buf[6] == 'n') && (buf[7] == 'g')) {
		return 1;
	}
	return 0;
}

// [=]===^=[ tfmx_init_decoder ]==================================================================[=]
// Build state from raw input buffer (already loaded into s->input.buf).
static int32_t tfmx_init_decoder(struct tfmx_state *s) {
	uint8_t v;
	uint32_t v_idx;

	for(v_idx = 0; v_idx < TFMX_VOICES_MAX; ++v_idx) {
		s->channel_to_voice_map[v_idx] = (uint8_t)(v_idx & 3);
	}

	s->offsets.header = tfmx_detect_single_file(s);
	if(s->offsets.header == 0) {
		if(!tfmx_detect_naked(s->input.buf, s->input.len)) {
			return 0;
		}
		// For a naked file, mdat_size is approximated by the embedded header
		// offsets. We assume the entire file is the mdat (samples either
		// inline as appended raw 8-bit data or absent entirely).
		s->input.mdat_size = s->input.len;
		s->input.smpl_size = 0;
	}

	uint32_t h = s->offsets.header;
	uint32_t o1 = tfmx_read_be_u32(s->input.buf, h + 0x1d0);
	uint32_t o2 = tfmx_read_be_u32(s->input.buf, h + 0x1d4);
	uint32_t o3 = tfmx_read_be_u32(s->input.buf, h + 0x1d8);

	if((o1 | o2 | o3) != 0) {
		s->variant.compressed = 1;
	} else {
		o1 = 0x800;
		o2 = 0x400;
		o3 = 0x600;
	}
	if((o1 >= s->input.len) || (o2 >= s->input.len) || (o3 >= s->input.len)) {
		o1 = tfmx_byteswap_u32(o1);
		o2 = tfmx_byteswap_u32(o2);
		o3 = tfmx_byteswap_u32(o3);
	}
	s->offsets.track_table = h + o1;
	s->offsets.patterns = h + o2;
	s->offsets.macros = h + o3;

	if((s->offsets.track_table >= s->input.len) || (s->offsets.patterns >= s->input.len) || (s->offsets.macros >= s->input.len)) {
		return 0;
	}

	s->offsets.sample_data = h + s->input.mdat_size;
	if(s->offsets.sample_data >= s->input.buf_len) {
		// No inline sample data; create a small zero-padded extension.
		uint32_t need = s->offsets.sample_data + 16;
		if(need > s->input.buf_len) {
			uint8_t *new_buf = (uint8_t *)realloc(s->input.buf, need);
			if(!new_buf) {
				return 0;
			}
			memset(new_buf + s->input.buf_len, 0, need - s->input.buf_len);
			s->input.buf = new_buf;
			s->input.buf_len = need;
			s->input.len = need;
		}
	}
	uint32_t o = s->offsets.sample_data;
	if(o + 4 <= s->input.buf_len) {
		s->input.buf[o] = 0;
		s->input.buf[o + 1] = 0;
		s->input.buf[o + 2] = 0;
		s->input.buf[o + 3] = 0;
	}
	s->offsets.silence = s->offsets.sample_data;

	tfmx_set_rate(s, (uint32_t)TFMX_DEFAULT_TICK_HZ << 8);
	s->voice_count = 4;
	s->player_info.sequencer.tracks = 8;
	s->player_info.sequencer.step.size = 16;

	memset(&s->variant, 0, sizeof(s->variant));

	if(((tfmx_read_be_u32(s->input.buf, s->offsets.header) == 0x54464d58u) && (s->input.buf[s->offsets.header + 4] == 0x20)) ||
		(s->input.version_hint == 1)) {
		tfmx_set_tfmx_v1(s);
	}

	tfmx_traits_by_checksum(s);
	tfmx_find_songs(s);

	if(s->v_songs_count == 0) {
		return 0;
	}

	if(s->input.start_song_hint >= 0) {
		s->v_songs_count = 1;
		s->v_songs[0] = (uint8_t)s->input.start_song_hint;
	}

	s->player_info.admin.initialized = 1;
	s->player_info.admin.start_song = 0;

	tfmx_restart(s);

	// Pre-scan for trackCmd 7V usage to enable 7-voice mapping.
	uint8_t loop_bak = s->loop_mode;
	s->loop_mode = 0;
	int32_t i;
	for(i = 0; i < TFMX_DEFAULT_TICK_HZ * 60; ++i) {
		tfmx_run(s);
		if(s->song_end) {
			break;
		}
	}
	s->loop_mode = loop_bak;

	if(s->track_cmd_used[3] || (s->input.version_hint == 3)) {
		s->voice_count = 8;
		s->channel_to_voice_map[3] = 7;
		s->channel_to_voice_map[4] = 3;
		s->channel_to_voice_map[5] = 4;
		s->channel_to_voice_map[6] = 5;
		s->channel_to_voice_map[7] = 6;
	}

	// Reset to start of song after the prescan.
	s->player_info.admin.start_song = 0;
	tfmx_restart(s);

	// Apply per-voice panning. Voices 0,3 -> left; 1,2 -> right (matches the
	// Pan4 table); for 7V, voices 4-6 also pan left.
	uint8_t pan_left[8] = { 0, 1, 1, 0, 0, 0, 0, 0 };
	for(v = 0; v < 8; ++v) {
		s->paula.ch[v].pan = pan_left[v] ? 127 : 0;
	}

	return 1;
}

// [=]===^=[ tfmx_init ]==========================================================================[=]
// Public API entry. The caller owns `data`; we copy it into our own buffer
// because the engine writes a 4-byte silence marker at offsets.SampleData,
// and may need to extend the buffer when no inline samples are present.
static struct tfmx_state *tfmx_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || (len < 512) || (sample_rate <= 0)) {
		return 0;
	}

	struct tfmx_state *s = (struct tfmx_state *)calloc(1, sizeof(*s));
	if(!s) {
		return 0;
	}

	s->sample_rate = sample_rate;
	paula_init(&s->paula, sample_rate, TFMX_DEFAULT_TICK_HZ);

	s->input.buf_len = len;
	s->input.len = len;
	s->input.buf = (uint8_t *)malloc(len);
	if(!s->input.buf) {
		free(s);
		return 0;
	}
	memcpy(s->input.buf, data, len);

	s->loop_mode = 1;

	if(!tfmx_init_decoder(s)) {
		free(s->input.buf);
		free(s);
		return 0;
	}

	return s;
}

// [=]===^=[ tfmx_free ]==========================================================================[=]
static void tfmx_free(struct tfmx_state *s) {
	if(!s) {
		return;
	}
	if(s->input.buf) {
		free(s->input.buf);
	}
	free(s);
}

// [=]===^=[ tfmx_get_audio ]=====================================================================[=]
// Sample-rate-driven mixing loop. We service ticks at the rate set by the
// engine (default 50 Hz, but track commands change it via SetRate/SetBpm).
static void tfmx_get_audio(struct tfmx_state *s, int16_t *output, int32_t frames) {
	if(!s || !output || (frames <= 0)) {
		return;
	}
	memset(output, 0, sizeof(int16_t) * 2 * (size_t)frames);

	int32_t produced = 0;
	while(produced < frames) {
		if(s->frames_until_tick_fp == 0) {
			tfmx_run(s);
			s->frames_until_tick_fp = s->frames_per_tick_fp;
		}
		uint32_t avail_ticks_fp = s->frames_until_tick_fp;
		int32_t avail_frames = (int32_t)(avail_ticks_fp >> 16);
		if(avail_frames <= 0) {
			avail_frames = 1;
		}
		int32_t need = frames - produced;
		int32_t chunk = avail_frames < need ? avail_frames : need;
		paula_mix_frames(&s->paula, output + produced * 2, chunk);
		produced += chunk;
		uint32_t consumed_fp = (uint32_t)chunk << 16;
		if(consumed_fp >= s->frames_until_tick_fp) {
			s->frames_until_tick_fp = 0;
		} else {
			s->frames_until_tick_fp -= consumed_fp;
		}
	}
}

// player_api thunks.

// [=]===^=[ tfmx_api_init ]======================================================================[=]
static void *tfmx_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return (void *)tfmx_init(data, len, sample_rate);
}

// [=]===^=[ tfmx_api_free ]======================================================================[=]
static void tfmx_api_free(void *state) {
	tfmx_free((struct tfmx_state *)state);
}

// [=]===^=[ tfmx_api_get_audio ]=================================================================[=]
static void tfmx_api_get_audio(void *state, int16_t *output, int32_t frames) {
	tfmx_get_audio((struct tfmx_state *)state, output, frames);
}

static struct player_api tfmx_api = {
	"TFMX",
	tfmx_extensions,
	tfmx_api_init,
	tfmx_api_free,
	tfmx_api_get_audio,
	0,
};
