// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Voodoo Supreme Synthesizer replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct voodoosupremesynthesizer_state *voodoosupremesynthesizer_init(void *data, uint32_t len, int32_t sample_rate);
//   void voodoosupremesynthesizer_free(struct voodoosupremesynthesizer_state *s);
//   void voodoosupremesynthesizer_get_audio(struct voodoosupremesynthesizer_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define VSS_TICK_HZ                50
#define VSS_MAX_OBJECTS            256
#define VSS_STACK_SIZE             32

// Track effect opcodes
#define VSS_EFF_GOSUB              0x81
#define VSS_EFF_RETURN             0x82
#define VSS_EFF_START_LOOP         0x83
#define VSS_EFF_DO_LOOP            0x84
#define VSS_EFF_SET_SAMPLE         0x85
#define VSS_EFF_SET_VOL_ENV        0x86
#define VSS_EFF_SET_PERIOD_TABLE   0x87
#define VSS_EFF_SET_WAVEFORM_TABLE 0x88
#define VSS_EFF_PORTAMENTO         0x89
#define VSS_EFF_SET_TRANSPOSE      0x8a
#define VSS_EFF_GOTO               0x8b
#define VSS_EFF_SET_RESET_FLAGS    0x8c
#define VSS_EFF_SET_WAVEFORM_MASK  0x8d
#define VSS_EFF_NOTE_CUT           0xff

// Synthesis flags
#define VSS_SYN_FREQUENCY_BASED_LENGTH (1u << 0)
#define VSS_SYN_STOP_SAMPLE            (1u << 1)
#define VSS_SYN_FREQUENCY_MAPPED       (1u << 5)
#define VSS_SYN_XOR_RING_MODULATION    (1u << 6)
#define VSS_SYN_MORPHING               (1u << 7)

// Reset flags
#define VSS_RESET_WAVEFORM_TABLE   (1u << 5)
#define VSS_RESET_PERIOD_TABLE     (1u << 6)
#define VSS_RESET_VOLUME_ENVELOPE  (1u << 7)

// Module object types
#define VSS_OBJ_UNKNOWN            0
#define VSS_OBJ_TRACK              1
#define VSS_OBJ_VOLUME_ENVELOPE    2
#define VSS_OBJ_PERIOD_TABLE       3
#define VSS_OBJ_WAVEFORM_TABLE     4
#define VSS_OBJ_WAVEFORM           5
#define VSS_OBJ_SAMPLE             6

struct vss_module_obj {
	uint8_t type;
	int32_t file_offset;          // original file offset, used for sample/waveform identification
	uint8_t *bytes;               // owned for track/envelope/period table; for waveform/sample points into module buffer
	uint32_t length;
	uint8_t owns_bytes;
};

struct vss_song {
	struct vss_module_obj objects[VSS_MAX_OBJECTS];
	uint32_t num_objects;
	uint32_t track_indices[4];
};

struct vss_voice {
	int32_t channel_number;

	int32_t sample1_index;        // index into song->objects, or -1
	uint32_t sample1_offset;      // used in frequency mapped mode
	int16_t sample1_number;       // index into all-samples list (for paula identity)
	int32_t sample2_index;
	int16_t sample2_number;

	int8_t audio_buffer[2][32];
	uint8_t use_audio_buffer;

	uint32_t stack[VSS_STACK_SIZE];
	uint32_t stack_top;

	uint32_t track_index;         // index into song->objects pointing to current track
	int32_t track_position;
	uint8_t tick_counter;

	uint8_t new_note;
	int8_t transpose;
	uint16_t note_period;
	uint16_t target_period;
	uint8_t current_volume;
	uint8_t final_volume;
	uint8_t master_volume;
	uint8_t reset_flags;

	uint8_t portamento_tick_counter;
	uint8_t portamento_increment;
	uint8_t portamento_direction;
	uint8_t portamento_delay;
	uint8_t portamento_duration;

	int32_t volume_envelope_index;
	uint8_t volume_envelope_position;
	uint8_t volume_envelope_tick_counter;
	int8_t volume_envelope_delta;

	int32_t period_table_index;
	uint8_t period_table_position;
	uint8_t period_table_tick_counter;
	uint8_t period_table_command;

	int32_t waveform_table_index;
	uint8_t waveform_table_position;

	uint8_t waveform_start_position;
	uint8_t waveform_position;
	uint8_t waveform_tick_counter;
	uint8_t waveform_increment;
	uint8_t waveform_mask;
	uint32_t synthesis_mode;
	uint8_t morph_speed;
};

struct voodoosupremesynthesizer_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	int32_t footer_offset;

	struct vss_song *sub_songs;
	uint32_t num_sub_songs;

	struct vss_song *current_song;
	struct vss_voice voices[4];

	int8_t empty_sample[256];

	// All-samples list across all subsongs, sorted by file offset.
	// Used to keep stable paula sample numbers (matches NostalgicPlayer logic).
	int32_t *all_sample_offsets;
	uint32_t num_all_samples;
};

// [=]===^=[ vss_periods ]========================================================================[=]
static uint16_t vss_periods[] = {
	                                                           32256, 30464, 28672,
	27136, 25600, 24192, 22784, 21504, 20352, 19200, 18048, 17689, 16128, 15232, 14336,
	13568, 12800, 12096, 11392, 10752, 10176,  9600,  9024,  8512,  8064,  7616,  7168,
	 6784,  6400,  6048,  5696,  5376,  5088,  4800,  4512,  4256,  4032,  3808,  3584,
	 3392,  3200,  3024,  2848,  2688,  2544,  2400,  2256,  2128,  2016,  1904,  1792,
	 1696,  1600,  1512,  1424,  1344,  1272,  1200,  1128,  1064,  1008,   952,   896,
	  848,   800,   756,   712,   672,   636,   600,   564,   532,   504,   476,   448,
	  424,   400,   378,   356,   336,   318,   300,   282,   266,   252,   238,   224,
	  212,   200,   189,   178,   168,   159,   150,   141,   133
};

// [=]===^=[ vss_frequency_ratio ]================================================================[=]
static uint16_t vss_frequency_ratio[] = {
	  1,   1,
	107, 101,
	 55,  49,
	 44,  37,
	160, 127,
	  4,   3,
	140,  99,
	218, 146,
	100,  63,
	111,  66,
	 98,  55,
	168,  89,
	  2,   1
};

// [=]===^=[ vss_read_u16_be ]====================================================================[=]
static uint16_t vss_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ vss_read_i32_be ]====================================================================[=]
static int32_t vss_read_i32_be(uint8_t *p) {
	return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

// [=]===^=[ vss_check_mark ]=====================================================================[=]
static int32_t vss_check_mark(uint8_t *data, uint32_t pos, uint32_t len, const char *mark) {
	uint32_t mlen = (uint32_t)strlen(mark);
	if(pos + mlen > len) {
		return 0;
	}
	for(uint32_t i = 0; i < mlen; ++i) {
		if((char)data[pos + i] != mark[i]) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ vss_identify ]=======================================================================[=]
static int32_t vss_identify(uint8_t *data, uint32_t len, int32_t *out_footer) {
	if(len < 64) {
		return 0;
	}
	uint32_t scan_start = (uint32_t)((len - 64 + 1) & ~(uint32_t)1);
	for(uint32_t i = 0; i + 8 <= 64; i += 2) {
		uint32_t pos = scan_start + i;
		if(pos + 8 > len) {
			break;
		}
		if(vss_check_mark(data, pos, len, "VSS0")) {
			uint32_t songs = ((uint32_t)data[pos + 4] << 24) | ((uint32_t)data[pos + 5] << 16) | ((uint32_t)data[pos + 6] << 8) | (uint32_t)data[pos + 7];
			if(songs < 0x100) {
				*out_footer = (int32_t)pos;
				return 1;
			}
			return 0;
		}
	}
	return 0;
}

// [=]===^=[ vss_read_offset_table_size ]=========================================================[=]
// Determines how many sub-song offsets follow the VSS0 header.
static int32_t vss_read_offset_table_size(uint8_t *data, uint32_t len, int32_t footer_offset) {
	if((uint32_t)(footer_offset + 8) > len) {
		return 0;
	}
	return vss_read_i32_be(data + footer_offset + 4);
}

// [=]===^=[ vss_load_all_offsets ]===============================================================[=]
// Reads the offset list at songOffset until the read position equals the smallest
// referenced offset seen so far. Marks the first 4 entries as VSS_OBJ_TRACK.
static int32_t vss_load_all_offsets(uint8_t *data, uint32_t len, int32_t footer_offset, int32_t song_offset, struct vss_song *song) {
	if((uint32_t)song_offset >= len) {
		return 0;
	}
	int32_t min_offset = footer_offset;
	int32_t pos = song_offset;
	uint32_t count = 0;

	for(;;) {
		if((uint32_t)(pos + 4) > len) {
			return 0;
		}
		int32_t new_offset = vss_read_i32_be(data + pos);
		pos += 4;

		if(new_offset >= 0) {
			int32_t abs_off = song_offset + new_offset;
			if(abs_off < min_offset) {
				min_offset = abs_off;
			}
		}

		if(count >= VSS_MAX_OBJECTS) {
			return 0;
		}
		song->objects[count].type = VSS_OBJ_UNKNOWN;
		song->objects[count].file_offset = song_offset + new_offset;
		song->objects[count].bytes = 0;
		song->objects[count].length = 0;
		song->objects[count].owns_bytes = 0;
		count++;

		if(pos == min_offset) {
			break;
		}
		if(pos > min_offset) {
			return 0;
		}
	}

	song->num_objects = count;
	if(count < 4) {
		return 0;
	}
	song->objects[0].type = VSS_OBJ_TRACK;
	song->objects[1].type = VSS_OBJ_TRACK;
	song->objects[2].type = VSS_OBJ_TRACK;
	song->objects[3].type = VSS_OBJ_TRACK;
	song->track_indices[0] = 0;
	song->track_indices[1] = 1;
	song->track_indices[2] = 2;
	song->track_indices[3] = 3;
	return 1;
}

// [=]===^=[ vss_append_byte ]====================================================================[=]
static int32_t vss_append_byte(uint8_t **buf, uint32_t *count, uint32_t *cap, uint8_t value) {
	if(*count == *cap) {
		uint32_t new_cap = (*cap == 0) ? 64 : (*cap * 2);
		uint8_t *nb = (uint8_t *)realloc(*buf, new_cap);
		if(!nb) {
			return 0;
		}
		*buf = nb;
		*cap = new_cap;
	}
	(*buf)[(*count)++] = value;
	return 1;
}

// [=]===^=[ vss_load_single_track ]==============================================================[=]
// Walks one track, marking referenced object indices in song->objects[*].type.
static int32_t vss_load_single_track(uint8_t *data, uint32_t len, struct vss_song *song, int32_t track_offset) {
	if((uint32_t)track_offset >= len) {
		return 0;
	}
	uint8_t *track_bytes = 0;
	uint32_t track_len = 0;
	uint32_t track_cap = 0;
	int32_t pos = track_offset;
	uint8_t sample_mode = 0;
	int32_t sample_number1 = -1;
	int32_t sample_number2 = -1;
	uint8_t done = 0;

	while(!done) {
		if((uint32_t)(pos + 1) > len) {
			free(track_bytes);
			return 0;
		}
		uint8_t cmd = data[pos++];
		if(!vss_append_byte(&track_bytes, &track_len, &track_cap, cmd)) {
			free(track_bytes);
			return 0;
		}

		if(cmd < 0x80) {
			if(sample_number1 != -1) {
				if((uint32_t)sample_number1 < song->num_objects) {
					song->objects[sample_number1].type = sample_mode ? VSS_OBJ_SAMPLE : VSS_OBJ_WAVEFORM;
				}
				sample_number1 = -1;
			}
			if(sample_number2 != -1) {
				if((uint32_t)sample_number2 < song->num_objects) {
					song->objects[sample_number2].type = sample_mode ? VSS_OBJ_SAMPLE : VSS_OBJ_WAVEFORM;
				}
				sample_number2 = -1;
			}
			if((uint32_t)(pos + 1) > len) {
				free(track_bytes);
				return 0;
			}
			if(!vss_append_byte(&track_bytes, &track_len, &track_cap, data[pos++])) {
				free(track_bytes);
				return 0;
			}
			continue;
		}

		switch(cmd) {
			case VSS_EFF_GOSUB: {
				if((uint32_t)(pos + 1) > len) {
					free(track_bytes);
					return 0;
				}
				uint8_t arg = data[pos++];
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, arg)) {
					free(track_bytes);
					return 0;
				}
				if(arg < song->num_objects) {
					song->objects[arg].type = VSS_OBJ_TRACK;
				}
				break;
			}

			case VSS_EFF_RETURN: {
				done = 1;
				break;
			}

			case VSS_EFF_DO_LOOP: {
				break;
			}

			case VSS_EFF_START_LOOP: {
				if((uint32_t)(pos + 2) > len) {
					free(track_bytes);
					return 0;
				}
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, data[pos++])) {
					free(track_bytes);
					return 0;
				}
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, data[pos++])) {
					free(track_bytes);
					return 0;
				}
				break;
			}

			case VSS_EFF_SET_SAMPLE: {
				if((uint32_t)(pos + 2) > len) {
					free(track_bytes);
					return 0;
				}
				uint8_t arg1 = data[pos++];
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, arg1)) {
					free(track_bytes);
					return 0;
				}
				sample_number1 = arg1;
				uint8_t arg2 = data[pos++];
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, arg2)) {
					free(track_bytes);
					return 0;
				}
				sample_number2 = arg2;
				break;
			}

			case VSS_EFF_SET_VOL_ENV: {
				if((uint32_t)(pos + 1) > len) {
					free(track_bytes);
					return 0;
				}
				uint8_t arg = data[pos++];
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, arg)) {
					free(track_bytes);
					return 0;
				}
				if(arg < song->num_objects) {
					song->objects[arg].type = VSS_OBJ_VOLUME_ENVELOPE;
				}
				break;
			}

			case VSS_EFF_SET_PERIOD_TABLE: {
				if((uint32_t)(pos + 1) > len) {
					free(track_bytes);
					return 0;
				}
				uint8_t arg = data[pos++];
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, arg)) {
					free(track_bytes);
					return 0;
				}
				if(arg < song->num_objects) {
					song->objects[arg].type = VSS_OBJ_PERIOD_TABLE;
				}
				break;
			}

			case VSS_EFF_SET_WAVEFORM_TABLE: {
				if((uint32_t)(pos + 2) > len) {
					free(track_bytes);
					return 0;
				}
				uint8_t arg1 = data[pos++];
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, arg1)) {
					free(track_bytes);
					return 0;
				}
				if(arg1 < song->num_objects) {
					song->objects[arg1].type = VSS_OBJ_WAVEFORM_TABLE;
				}
				uint8_t arg2 = data[pos++];
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, arg2)) {
					free(track_bytes);
					return 0;
				}
				sample_mode = (uint8_t)((arg2 & 0x20) != 0);
				break;
			}

			case VSS_EFF_PORTAMENTO: {
				if((uint32_t)(pos + 3) > len) {
					free(track_bytes);
					return 0;
				}
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, data[pos++])) {
					free(track_bytes);
					return 0;
				}
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, data[pos++])) {
					free(track_bytes);
					return 0;
				}
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, data[pos++])) {
					free(track_bytes);
					return 0;
				}
				break;
			}

			case VSS_EFF_SET_TRANSPOSE:
			case VSS_EFF_SET_RESET_FLAGS:
			case VSS_EFF_SET_WAVEFORM_MASK:
			case VSS_EFF_NOTE_CUT: {
				if((uint32_t)(pos + 1) > len) {
					free(track_bytes);
					return 0;
				}
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, data[pos++])) {
					free(track_bytes);
					return 0;
				}
				break;
			}

			case VSS_EFF_GOTO: {
				if((uint32_t)(pos + 1) > len) {
					free(track_bytes);
					return 0;
				}
				uint8_t arg = data[pos++];
				int32_t new_track_offset = (arg < song->num_objects) ? song->objects[arg].file_offset : 0;
				uint16_t new_pos = 0;
				if((new_track_offset >= track_offset) && (new_track_offset < (track_offset + (int32_t)track_len))) {
					new_pos = (uint16_t)(new_track_offset - track_offset);
				}
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, (uint8_t)(new_pos >> 8))) {
					free(track_bytes);
					return 0;
				}
				if(!vss_append_byte(&track_bytes, &track_len, &track_cap, (uint8_t)(new_pos & 0xff))) {
					free(track_bytes);
					return 0;
				}
				done = 1;
				break;
			}

			default: {
				free(track_bytes);
				return 0;
			}
		}
	}

	// Find the matching object slot and store the parsed track bytes.
	for(uint32_t i = 0; i < song->num_objects; ++i) {
		if((song->objects[i].type == VSS_OBJ_TRACK) && (song->objects[i].file_offset == track_offset) && (song->objects[i].bytes == 0)) {
			song->objects[i].bytes = track_bytes;
			song->objects[i].length = track_len;
			song->objects[i].owns_bytes = 1;
			return 1;
		}
	}
	free(track_bytes);
	return 0;
}

// [=]===^=[ vss_load_tracks ]====================================================================[=]
static int32_t vss_load_tracks(uint8_t *data, uint32_t len, struct vss_song *song) {
	// Iterate until no new tracks appear (Gosub from a track may mark new ones).
	uint8_t progress = 1;
	while(progress) {
		progress = 0;
		for(uint32_t i = 0; i < song->num_objects; ++i) {
			if((song->objects[i].type == VSS_OBJ_TRACK) && (song->objects[i].bytes == 0)) {
				if(!vss_load_single_track(data, len, song, song->objects[i].file_offset)) {
					return 0;
				}
				progress = 1;
			}
		}
	}
	return 1;
}

// [=]===^=[ vss_load_simple_table ]==============================================================[=]
// Loads a sequence of (byte, byte) pairs until first byte equals `terminator`.
static int32_t vss_load_simple_table(uint8_t *data, uint32_t len, int32_t offset, uint8_t terminator, uint8_t **out_bytes, uint32_t *out_len) {
	if((uint32_t)offset >= len) {
		return 0;
	}
	uint8_t *buf = 0;
	uint32_t count = 0;
	uint32_t cap = 0;
	int32_t pos = offset;
	uint8_t done = 0;

	while(!done) {
		if((uint32_t)(pos + 2) > len) {
			free(buf);
			return 0;
		}
		uint8_t b0 = data[pos++];
		uint8_t b1 = data[pos++];
		if(!vss_append_byte(&buf, &count, &cap, b0)) {
			free(buf);
			return 0;
		}
		if(!vss_append_byte(&buf, &count, &cap, b1)) {
			free(buf);
			return 0;
		}
		if(b0 == terminator) {
			done = 1;
		}
	}
	*out_bytes = buf;
	*out_len = count;
	return 1;
}

// [=]===^=[ vss_load_volume_envelopes ]==========================================================[=]
static int32_t vss_load_volume_envelopes(uint8_t *data, uint32_t len, struct vss_song *song) {
	for(uint32_t i = 0; i < song->num_objects; ++i) {
		if(song->objects[i].type == VSS_OBJ_VOLUME_ENVELOPE) {
			uint8_t *buf = 0;
			uint32_t blen = 0;
			if(!vss_load_simple_table(data, len, song->objects[i].file_offset, 0x88, &buf, &blen)) {
				return 0;
			}
			song->objects[i].bytes = buf;
			song->objects[i].length = blen;
			song->objects[i].owns_bytes = 1;
		}
	}
	return 1;
}

// [=]===^=[ vss_load_period_tables ]=============================================================[=]
static int32_t vss_load_period_tables(uint8_t *data, uint32_t len, struct vss_song *song) {
	for(uint32_t i = 0; i < song->num_objects; ++i) {
		if(song->objects[i].type == VSS_OBJ_PERIOD_TABLE) {
			uint8_t *buf = 0;
			uint32_t blen = 0;
			if(!vss_load_simple_table(data, len, song->objects[i].file_offset, 0xff, &buf, &blen)) {
				return 0;
			}
			song->objects[i].bytes = buf;
			song->objects[i].length = blen;
			song->objects[i].owns_bytes = 1;
		}
	}
	return 1;
}

// [=]===^=[ vss_load_waveform_tables ]===========================================================[=]
static int32_t vss_load_waveform_tables(uint8_t *data, uint32_t len, struct vss_song *song) {
	for(uint32_t i = 0; i < song->num_objects; ++i) {
		if(song->objects[i].type == VSS_OBJ_WAVEFORM_TABLE) {
			int32_t off = song->objects[i].file_offset;
			if((uint32_t)(off + 28) > len) {
				return 0;
			}
			uint8_t *buf = (uint8_t *)malloc(28);
			if(!buf) {
				return 0;
			}
			memcpy(buf, data + off, 28);
			song->objects[i].bytes = buf;
			song->objects[i].length = 28;
			song->objects[i].owns_bytes = 1;
		}
	}
	return 1;
}

// [=]===^=[ vss_load_waveforms ]=================================================================[=]
static int32_t vss_load_waveforms(uint8_t *data, uint32_t len, struct vss_song *song) {
	for(uint32_t i = 0; i < song->num_objects; ++i) {
		if(song->objects[i].type == VSS_OBJ_WAVEFORM) {
			int32_t off = song->objects[i].file_offset;
			if((uint32_t)(off + 32) > len) {
				return 0;
			}
			song->objects[i].bytes = data + off;
			song->objects[i].length = 32;
			song->objects[i].owns_bytes = 0;
		}
	}
	return 1;
}

// [=]===^=[ vss_load_samples ]===================================================================[=]
static int32_t vss_load_samples(uint8_t *data, uint32_t len, struct vss_song *song) {
	for(uint32_t i = 0; i < song->num_objects; ++i) {
		if(song->objects[i].type == VSS_OBJ_SAMPLE) {
			int32_t off = song->objects[i].file_offset;
			if((off < 2) || ((uint32_t)off > len)) {
				return 0;
			}
			uint16_t slen = vss_read_u16_be(data + off - 2);
			if((uint32_t)(off + slen) > len) {
				return 0;
			}
			song->objects[i].bytes = data + off;
			song->objects[i].length = slen;
			song->objects[i].owns_bytes = 0;
		}
	}
	return 1;
}

// [=]===^=[ vss_load_sub_song ]==================================================================[=]
static int32_t vss_load_sub_song(uint8_t *data, uint32_t len, int32_t footer_offset, int32_t song_offset, struct vss_song *song) {
	if(!vss_load_all_offsets(data, len, footer_offset, song_offset, song)) {
		return 0;
	}
	if(!vss_load_tracks(data, len, song)) {
		return 0;
	}
	if(!vss_load_volume_envelopes(data, len, song)) {
		return 0;
	}
	if(!vss_load_period_tables(data, len, song)) {
		return 0;
	}
	if(!vss_load_waveform_tables(data, len, song)) {
		return 0;
	}
	if(!vss_load_waveforms(data, len, song)) {
		return 0;
	}
	if(!vss_load_samples(data, len, song)) {
		return 0;
	}
	return 1;
}

// [=]===^=[ vss_free_song ]======================================================================[=]
static void vss_free_song(struct vss_song *song) {
	for(uint32_t i = 0; i < song->num_objects; ++i) {
		if(song->objects[i].owns_bytes && song->objects[i].bytes) {
			free(song->objects[i].bytes);
		}
		song->objects[i].bytes = 0;
		song->objects[i].owns_bytes = 0;
	}
}

// [=]===^=[ vss_build_all_samples ]==============================================================[=]
// Build a sorted array of all sample/waveform file offsets across all subsongs.
// Provides stable paula identifiers.
static int32_t vss_build_all_samples(struct voodoosupremesynthesizer_state *s) {
	uint32_t cap = 64;
	int32_t *list = (int32_t *)malloc(cap * sizeof(int32_t));
	if(!list) {
		return 0;
	}
	uint32_t count = 0;

	for(uint32_t si = 0; si < s->num_sub_songs; ++si) {
		struct vss_song *song = &s->sub_songs[si];
		for(uint32_t i = 0; i < song->num_objects; ++i) {
			if((song->objects[i].type == VSS_OBJ_WAVEFORM) || (song->objects[i].type == VSS_OBJ_SAMPLE)) {
				int32_t off = song->objects[i].file_offset;
				uint32_t j;
				for(j = 0; j < count; ++j) {
					if(list[j] == off) {
						break;
					}
				}
				if(j == count) {
					if(count == cap) {
						cap *= 2;
						int32_t *nl = (int32_t *)realloc(list, cap * sizeof(int32_t));
						if(!nl) {
							free(list);
							return 0;
						}
						list = nl;
					}
					list[count++] = off;
				}
			}
		}
	}

	// Insertion sort by ascending offset.
	for(uint32_t i = 1; i < count; ++i) {
		int32_t key = list[i];
		int32_t j = (int32_t)i - 1;
		while((j >= 0) && (list[j] > key)) {
			list[j + 1] = list[j];
			--j;
		}
		list[j + 1] = key;
	}

	s->all_sample_offsets = list;
	s->num_all_samples = count;
	return 1;
}

// [=]===^=[ vss_find_sample_number ]=============================================================[=]
static int16_t vss_find_sample_number(struct voodoosupremesynthesizer_state *s, int32_t file_offset) {
	for(uint32_t i = 0; i < s->num_all_samples; ++i) {
		if(s->all_sample_offsets[i] == file_offset) {
			return (int16_t)i;
		}
	}
	return -1;
}

// [=]===^=[ vss_obj_data_i8 ]====================================================================[=]
// Returns the bytes pointer of an object reinterpreted as int8_t (sample data).
static int8_t *vss_obj_data_i8(struct vss_song *song, int32_t index) {
	if((index < 0) || ((uint32_t)index >= song->num_objects)) {
		return 0;
	}
	return (int8_t *)song->objects[index].bytes;
}

// [=]===^=[ vss_obj_length ]=====================================================================[=]
static uint32_t vss_obj_length(struct vss_song *song, int32_t index) {
	if((index < 0) || ((uint32_t)index >= song->num_objects)) {
		return 0;
	}
	return song->objects[index].length;
}

// [=]===^=[ vss_initialize_sound ]===============================================================[=]
static void vss_initialize_sound(struct voodoosupremesynthesizer_state *s, uint32_t sub_song) {
	if(sub_song >= s->num_sub_songs) {
		sub_song = 0;
	}
	s->current_song = &s->sub_songs[sub_song];

	for(int32_t i = 0; i < 4; ++i) {
		struct vss_voice *v = &s->voices[i];
		memset(v, 0, sizeof(*v));
		v->channel_number = i;
		v->sample1_index = -1;
		v->sample1_number = -1;
		v->sample2_index = -1;
		v->sample2_number = -1;
		v->track_index = s->current_song->track_indices[i];
		v->track_position = 0;
		v->tick_counter = 1;
		v->master_volume = 64;
		v->waveform_increment = 1;

		paula_play_sample(&s->paula, i, s->empty_sample, 16);
		paula_set_loop(&s->paula, i, 0, 16);
		paula_set_volume(&s->paula, i, 0);
	}
}

// [=]===^=[ vss_get_period ]=====================================================================[=]
static uint16_t vss_get_period(struct vss_voice *v, uint8_t note) {
	int32_t idx = (int32_t)note + (int32_t)v->transpose;
	if((idx < 0) || (idx >= (int32_t)(sizeof(vss_periods) / sizeof(vss_periods[0])))) {
		return 0;
	}
	return vss_periods[idx];
}

// [=]===^=[ vss_do_volume_envelope ]=============================================================[=]
static void vss_do_volume_envelope(struct voodoosupremesynthesizer_state *s, struct vss_voice *v) {
	v->volume_envelope_tick_counter--;
	if(v->volume_envelope_tick_counter == 0) {
		uint8_t *envelope = vss_obj_data_i8(s->current_song, v->volume_envelope_index) ? (uint8_t *)s->current_song->objects[v->volume_envelope_index].bytes : 0;
		if(!envelope) {
			return;
		}
		uint32_t elen = vss_obj_length(s->current_song, v->volume_envelope_index);
		// C# original is `while (envelope[pos] == 0x88) pos = envelope[pos+1];` --
		// unbounded. envelope_position is uint8_t (0..255), so a 256-byte visited
		// set deterministically catches any jump cycle. Replaces the previous
		// 256-hop cap and is strictly more correct: terminates on either a
		// non-0x88 byte, out-of-bounds, or a revisited position.
		uint8_t volenv_visited[256] = { 0 };
		while((v->volume_envelope_position + 1u < elen) && (envelope[v->volume_envelope_position] == 0x88)) {
			if(volenv_visited[v->volume_envelope_position]) {
				break;
			}
			volenv_visited[v->volume_envelope_position] = 1;
			v->volume_envelope_position = envelope[v->volume_envelope_position + 1];
		}
		if((v->volume_envelope_position + 1u >= elen)) {
			return;
		}
		if(envelope[v->volume_envelope_position + 1] == 0) {
			v->current_volume = envelope[v->volume_envelope_position];
			v->volume_envelope_tick_counter = 1;
		} else {
			v->volume_envelope_delta = (int8_t)envelope[v->volume_envelope_position];
			v->volume_envelope_tick_counter = envelope[v->volume_envelope_position + 1];
			v->current_volume = (uint8_t)(v->current_volume + v->volume_envelope_delta);
		}
		v->volume_envelope_position = (uint8_t)(v->volume_envelope_position + 2);
	} else {
		v->current_volume = (uint8_t)(v->current_volume + v->volume_envelope_delta);
	}
}

// [=]===^=[ vss_do_period_table_part1 ]==========================================================[=]
static void vss_do_period_table_part1(struct voodoosupremesynthesizer_state *s, struct vss_voice *v) {
	v->period_table_tick_counter--;
	if(v->period_table_tick_counter == 0) {
		if(v->period_table_index < 0) {
			return;
		}
		uint8_t *table = (uint8_t *)s->current_song->objects[v->period_table_index].bytes;
		if(!table) {
			return;
		}
		uint32_t tlen = s->current_song->objects[v->period_table_index].length;
		// Cycle-safe period-table 0xff jump-chase. C# is unbounded; position is
		// uint8_t so a 256-byte visited set fully replaces the old hop cap.
		uint8_t pertbl_visited[256] = { 0 };
		while((v->period_table_position + 1u < tlen) && (table[v->period_table_position] == 0xff)) {
			if(pertbl_visited[v->period_table_position]) {
				break;
			}
			pertbl_visited[v->period_table_position] = 1;
			v->period_table_position = table[v->period_table_position + 1];
		}
		if((v->period_table_position + 1u >= tlen)) {
			return;
		}
		v->period_table_command = table[v->period_table_position++];
		v->period_table_tick_counter = table[v->period_table_position++];
	}
}

// [=]===^=[ vss_do_period_table_part2 ]==========================================================[=]
static void vss_do_period_table_part2(struct vss_voice *v) {
	if(v->period_table_command == 0xfe) {
		v->note_period = (uint16_t)(v->note_period << 1);
	} else if(v->period_table_command == 0x7f) {
		v->note_period = (uint16_t)(v->note_period >> 1);
	} else if(v->period_table_command == 0x7e) {
		uint8_t counter = v->period_table_tick_counter;
		v->period_table_tick_counter = 1;

		uint16_t numerator;
		uint16_t denominator;
		uint8_t round;

		if((counter & 0x80) != 0) {
			counter = (uint8_t)(counter & 0x7f);
			if(counter >= 13) {
				int32_t octave = (counter / 12) & 7;
				v->note_period = (uint16_t)(v->note_period << octave);
				counter = (uint8_t)(counter % 12);
			}
			counter = (uint8_t)(counter * 2);
			numerator = vss_frequency_ratio[counter];
			denominator = vss_frequency_ratio[counter + 1];
			round = 1;
		} else {
			if(counter >= 13) {
				int32_t octave = (counter / 12) & 7;
				v->note_period = (uint16_t)(v->note_period >> octave);
				counter = (uint8_t)(counter % 12);
			}
			counter = (uint8_t)(counter * 2);
			denominator = vss_frequency_ratio[counter];
			numerator = vss_frequency_ratio[counter + 1];
			round = 0;
		}

		int32_t temp = (int32_t)v->note_period * (int32_t)numerator;
		v->note_period = (uint16_t)(temp / denominator);
		if(round && ((temp % denominator) != 0)) {
			v->note_period++;
		}
	} else if(v->period_table_command < 0x7f) {
		v->note_period = (uint16_t)(v->note_period + v->period_table_command);
	} else {
		v->note_period = (uint16_t)(v->note_period - (v->period_table_command & 0x7f));
	}
}

// [=]===^=[ vss_do_portamento ]==================================================================[=]
static void vss_do_portamento(struct vss_voice *v) {
	v->portamento_duration--;
	if(v->portamento_duration == 0) {
		v->portamento_increment = 0;
		v->portamento_tick_counter = 0;
	} else {
		v->portamento_tick_counter--;
		if(v->portamento_tick_counter == 0) {
			v->portamento_tick_counter = v->portamento_delay;
			if(v->portamento_direction) {
				v->note_period = (uint16_t)(v->note_period - v->portamento_increment);
			} else {
				v->note_period = (uint16_t)(v->note_period + v->portamento_increment);
			}
		}
	}
}

// [=]===^=[ vss_set_hardware ]===================================================================[=]
static void vss_set_hardware(struct voodoosupremesynthesizer_state *s, struct vss_voice *v) {
	int32_t ch = v->channel_number;
	if(v->target_period > 0) {
		paula_set_period(&s->paula, ch, v->target_period);
	}
	paula_set_volume(&s->paula, ch, v->final_volume);

	v->target_period = v->note_period;
	v->final_volume = (uint8_t)(((uint32_t)v->current_volume * (uint32_t)v->master_volume) / 64);
}

// [=]===^=[ vss_do_ring_modulation ]=============================================================[=]
static void vss_do_ring_modulation(struct voodoosupremesynthesizer_state *s, struct vss_voice *v) {
	int32_t ch = v->channel_number;
	v->waveform_tick_counter--;
	if(v->waveform_tick_counter == 0) {
		if(v->waveform_table_index < 0) {
			return;
		}
		uint8_t *table = (uint8_t *)s->current_song->objects[v->waveform_table_index].bytes;
		if(!table) {
			return;
		}
		uint32_t tlen = s->current_song->objects[v->waveform_table_index].length;
		// Cycle-safe waveform-table jump chase (high-bit-set opcodes).
		uint8_t wftbl_visited[256] = { 0 };
		while((v->waveform_table_position + 1u < tlen) && ((table[v->waveform_table_position] & 0x80) != 0)) {
			if(wftbl_visited[v->waveform_table_position]) {
				break;
			}
			wftbl_visited[v->waveform_table_position] = 1;
			v->waveform_table_position = table[v->waveform_table_position + 1];
		}
		if((v->waveform_table_position + 1u >= tlen)) {
			return;
		}
		v->waveform_increment = table[v->waveform_table_position++];
		v->waveform_tick_counter = table[v->waveform_table_position++];
	}

	v->waveform_position = (uint8_t)((v->waveform_position + v->waveform_increment) & 0x1f);

	int8_t *playing = v->audio_buffer[v->use_audio_buffer];
	paula_queue_sample(&s->paula, ch, playing, 0, 32);
	paula_set_loop(&s->paula, ch, 0, 32);

	v->new_note = 0;
	v->use_audio_buffer ^= 1;

	int8_t *sample1 = vss_obj_data_i8(s->current_song, v->sample1_index);
	int8_t *sample2 = vss_obj_data_i8(s->current_song, v->sample2_index);
	if(sample1 && sample2) {
		int8_t *fill = v->audio_buffer[v->use_audio_buffer];
		uint32_t fill_offset = 0;
		uint32_t s1_off = 0;
		uint32_t s2_off = v->waveform_position;
		uint8_t mask = v->waveform_mask;

		for(int32_t i = 0; i < 32; ++i) {
			int16_t s1d = sample1[s1_off++];
			int16_t s2d = sample2[s2_off++];
			uint8_t sample = (uint8_t)((int32_t)(s1d + s2d) >> 1);
			if((sample & 0x80) != 0) {
				sample = (uint8_t)(-(int8_t)((-(int8_t)sample) | mask));
			} else {
				sample = (uint8_t)(sample | mask);
			}
			fill[fill_offset++] = (int8_t)sample;
			if(s2_off == 32) {
				s2_off = 0;
			}
		}
	}
}

// [=]===^=[ vss_do_xor_ring_modulation ]=========================================================[=]
static void vss_do_xor_ring_modulation(struct voodoosupremesynthesizer_state *s, struct vss_voice *v) {
	int32_t ch = v->channel_number;
	v->waveform_tick_counter--;
	if(v->waveform_tick_counter == 0) {
		if(v->waveform_table_index < 0) {
			return;
		}
		uint8_t *table = (uint8_t *)s->current_song->objects[v->waveform_table_index].bytes;
		if(!table) {
			return;
		}
		uint32_t tlen = s->current_song->objects[v->waveform_table_index].length;
		// Cycle-safe waveform-table 0xff jump chase.
		uint8_t wftbl_visited[256] = { 0 };
		while((v->waveform_table_position + 1u < tlen) && (table[v->waveform_table_position] == 0xff)) {
			if(wftbl_visited[v->waveform_table_position]) {
				break;
			}
			wftbl_visited[v->waveform_table_position] = 1;
			v->waveform_table_position = table[v->waveform_table_position + 1];
		}
		if((v->waveform_table_position + 1u >= tlen)) {
			return;
		}
		v->waveform_increment = table[v->waveform_table_position++];
		v->waveform_tick_counter = table[v->waveform_table_position++];
	}

	int8_t *playing = v->audio_buffer[v->use_audio_buffer];
	paula_queue_sample(&s->paula, ch, playing, 0, 32);
	paula_set_loop(&s->paula, ch, 0, 32);

	v->new_note = 0;
	v->use_audio_buffer ^= 1;

	int8_t *sample1 = vss_obj_data_i8(s->current_song, v->sample1_index);
	int8_t *sample2 = vss_obj_data_i8(s->current_song, v->sample2_index);
	if(sample1 && sample2) {
		int8_t *fill = v->audio_buffer[v->use_audio_buffer];
		if((v->waveform_increment & 0x80) != 0) {
			for(int32_t i = 0; i < 32; ++i) {
				fill[i] = sample1[i];
			}
		} else {
			uint8_t position = v->waveform_position;
			uint8_t switch_pos = (uint8_t)((v->waveform_increment & 0x1f) + position);
			uint8_t flag = 0;
			if((switch_pos & 0x20) != 0) {
				flag = 1;
				switch_pos = (uint8_t)(switch_pos & 0x1f);
			}
			uint32_t play_off = 0;
			uint32_t fill_off = 0;
			for(int32_t i = 0; i < 32; ++i) {
				int8_t sd = playing[play_off++];
				if(i == position) {
					flag ^= 1;
				}
				if(i == switch_pos) {
					flag ^= 1;
				}
				if(flag) {
					sd = (int8_t)(sd ^ (int8_t)(sample2[i] & v->waveform_mask));
				}
				fill[fill_off++] = sd;
			}
			v->waveform_position = (uint8_t)((v->waveform_increment + v->waveform_position) & 0x1f);
		}
	}
}

// [=]===^=[ vss_do_morphing ]====================================================================[=]
static void vss_do_morphing(struct voodoosupremesynthesizer_state *s, struct vss_voice *v) {
	int32_t ch = v->channel_number;
	v->waveform_tick_counter--;
	if(v->waveform_tick_counter == 0) {
		if(v->waveform_table_index < 0) {
			return;
		}
		uint8_t *table = (uint8_t *)s->current_song->objects[v->waveform_table_index].bytes;
		if(!table) {
			return;
		}
		uint32_t tlen = s->current_song->objects[v->waveform_table_index].length;
		// Cycle-safe waveform-table 0xff jump chase.
		uint8_t wftbl_visited[256] = { 0 };
		while((v->waveform_table_position + 1u < tlen) && (table[v->waveform_table_position] == 0xff)) {
			if(wftbl_visited[v->waveform_table_position]) {
				break;
			}
			wftbl_visited[v->waveform_table_position] = 1;
			v->waveform_table_position = table[v->waveform_table_position + 1];
		}
		if((v->waveform_table_position + 2u >= tlen)) {
			return;
		}
		v->waveform_increment = table[v->waveform_table_position++];
		v->morph_speed = table[v->waveform_table_position++];
		v->waveform_tick_counter = table[v->waveform_table_position++];
	}

	int8_t *playing = v->audio_buffer[v->use_audio_buffer];
	paula_queue_sample(&s->paula, ch, playing, 0, 32);
	paula_set_loop(&s->paula, ch, 0, 32);

	v->new_note = 0;
	v->use_audio_buffer ^= 1;

	int8_t *sample1 = vss_obj_data_i8(s->current_song, v->sample1_index);
	int8_t *sample2 = vss_obj_data_i8(s->current_song, v->sample2_index);
	if(sample1 && sample2) {
		int8_t *fill = v->audio_buffer[v->use_audio_buffer];
		if(v->waveform_increment == 0x80) {
			for(int32_t i = 0; i < 32; ++i) {
				fill[i] = sample1[i];
			}
		} else {
			uint8_t speed = v->morph_speed;
			int8_t *sample = ((v->waveform_increment & 0xc0) == 0x40) ? sample1 : sample2;
			uint8_t position = v->waveform_position;
			uint8_t switch_pos = (uint8_t)((v->waveform_increment & 0x1f) + position);
			uint8_t flag = 0;
			if((switch_pos & 0x20) != 0) {
				flag = 1;
				switch_pos = (uint8_t)(switch_pos & 0x1f);
			}
			uint32_t play_off = 0;
			uint32_t fill_off = 0;
			for(int32_t i = 0; i < 32; ++i) {
				int8_t sd = playing[play_off++];
				if(i == position) {
					flag ^= 1;
				}
				if(i == switch_pos) {
					flag ^= 1;
				}
				if(flag) {
					uint8_t sd1 = (uint8_t)((uint8_t)sd - 0x80);
					uint8_t sd2 = (uint8_t)((uint8_t)sample[i] - 0x80);
					uint8_t diff = (uint8_t)(sd2 - sd1);
					if(sd2 < sd1) {
						diff = (uint8_t)-diff;
						if(speed >= diff) {
							sd = sample[i];
						} else {
							sd = (int8_t)(sd1 + 0x80 - speed);
						}
					} else {
						if(speed >= diff) {
							sd = sample[i];
						} else {
							sd = (int8_t)(sd1 + 0x80 + speed);
						}
					}
				}
				fill[fill_off++] = sd;
			}
			if((v->waveform_increment & 0xc0) != 0xc0) {
				v->waveform_position = (uint8_t)((v->waveform_increment + v->waveform_position) & 0x1f);
			}
		}
	}
}

// [=]===^=[ vss_do_frequency_mapped ]============================================================[=]
static void vss_do_frequency_mapped(struct voodoosupremesynthesizer_state *s, struct vss_voice *v) {
	v->waveform_tick_counter--;
	if(v->waveform_tick_counter == 0) {
		if(v->waveform_table_index < 0) {
			return;
		}
		uint8_t *table = (uint8_t *)s->current_song->objects[v->waveform_table_index].bytes;
		if(!table) {
			return;
		}
		uint32_t tlen = s->current_song->objects[v->waveform_table_index].length;
		// Cycle-safe waveform-table 0xff jump chase.
		uint8_t wftbl_visited[256] = { 0 };
		while((v->waveform_table_position + 1u < tlen) && (table[v->waveform_table_position] == 0xff)) {
			if(wftbl_visited[v->waveform_table_position]) {
				break;
			}
			wftbl_visited[v->waveform_table_position] = 1;
			v->waveform_table_position = table[v->waveform_table_position + 1];
		}
		if((v->waveform_table_position + 2u >= tlen)) {
			return;
		}
		v->sample1_index = v->sample2_index;
		v->sample1_offset = (uint32_t)(((uint32_t)table[v->waveform_table_position] << 8) + (uint32_t)table[v->waveform_table_position + 1]);
		v->sample1_number = v->sample2_number;
		v->synthesis_mode &= ~VSS_SYN_STOP_SAMPLE;
		v->waveform_tick_counter = table[v->waveform_table_position + 2];
		v->waveform_table_position = (uint8_t)(v->waveform_table_position + 3);
	}

	if(v->synthesis_mode & VSS_SYN_FREQUENCY_BASED_LENGTH) {
		uint16_t period = vss_periods[v->waveform_mask];
		if(period == 0) {
			period = 1;
		}
		int32_t delta = v->note_period / period;
		int32_t delta_remainder = v->note_period % period;
		uint16_t result = (uint16_t)(((delta_remainder * 128) / period) + (delta * 128));
		v->waveform_increment = (uint8_t)(result >> 8);
		v->waveform_start_position = (uint8_t)(result & 0xff);
	} else {
		v->waveform_increment = 0;
		v->waveform_start_position = 128;
	}
}

// [=]===^=[ vss_waveform_generator ]=============================================================[=]
static void vss_waveform_generator(struct voodoosupremesynthesizer_state *s, struct vss_voice *v) {
	if(v->synthesis_mode & VSS_SYN_XOR_RING_MODULATION) {
		vss_do_xor_ring_modulation(s, v);
	} else if(v->synthesis_mode & VSS_SYN_MORPHING) {
		vss_do_morphing(s, v);
	} else if(v->synthesis_mode & VSS_SYN_FREQUENCY_MAPPED) {
		vss_do_frequency_mapped(s, v);
	} else {
		vss_do_ring_modulation(s, v);
	}
}

// [=]===^=[ vss_audio_interrupt ]================================================================[=]
// Called by the frequency-mapped renderer between sample chunks. The C# version is
// driven by Paula's mixer reaching the end of the queued sample; here we run it once
// per tick when in frequency-mapped mode and use paula_queue_sample to chain segments.
static void vss_audio_interrupt(struct voodoosupremesynthesizer_state *s, struct vss_voice *v) {
	if((v->synthesis_mode & VSS_SYN_FREQUENCY_MAPPED) == 0) {
		return;
	}
	int32_t ch = v->channel_number;
	uint32_t prev_offset = v->sample1_offset;
	v->sample1_offset = (uint32_t)(v->sample1_offset + (((uint32_t)v->waveform_increment << 8) | (uint32_t)v->waveform_start_position));

	if(v->synthesis_mode & VSS_SYN_STOP_SAMPLE) {
		paula_queue_sample(&s->paula, ch, s->empty_sample, 0, 16);
		paula_set_loop(&s->paula, ch, 0, 16);
		v->sample1_index = -1;
		v->sample1_offset = 0;
		return;
	}

	int8_t *sample2 = vss_obj_data_i8(s->current_song, v->sample2_index);
	uint32_t s2_len = vss_obj_length(s->current_song, v->sample2_index);
	if(!sample2) {
		return;
	}
	if(v->sample1_offset >= s2_len) {
		paula_queue_sample(&s->paula, ch, s->empty_sample, 0, 16);
		paula_set_loop(&s->paula, ch, 0, 16);
		v->sample1_index = -1;
		v->sample1_offset = 0;
		v->synthesis_mode |= VSS_SYN_STOP_SAMPLE;
	} else {
		int8_t *s1 = vss_obj_data_i8(s->current_song, v->sample1_index);
		if(s1) {
			uint32_t play_len = (s2_len > prev_offset) ? (s2_len - prev_offset) : 16;
			if(play_len > 0x40) {
				play_len = 0x40;
			}
			paula_queue_sample(&s->paula, ch, s1, prev_offset, play_len);
			paula_set_loop(&s->paula, ch, prev_offset, play_len);
			v->new_note = 0;
		}
	}
}

// [=]===^=[ vss_cmd_note_cut ]===================================================================[=]
static void vss_cmd_note_cut(struct vss_voice *v, uint8_t *track) {
	v->current_volume = 0;
	v->volume_envelope_tick_counter = 0;
	v->tick_counter = track[v->track_position++];
	v->final_volume = 0;
	v->volume_envelope_delta = 0;
}

// [=]===^=[ vss_cmd_gosub ]======================================================================[=]
static void vss_cmd_gosub(struct vss_voice *v, uint8_t *track) {
	uint8_t track_num = track[v->track_position++];
	if(v->stack_top + 2 > VSS_STACK_SIZE) {
		return;
	}
	v->stack[v->stack_top++] = (uint32_t)v->track_position;
	v->stack[v->stack_top++] = v->track_index;
	v->track_index = track_num;
	v->track_position = 0;
}

// [=]===^=[ vss_cmd_return ]=====================================================================[=]
static void vss_cmd_return(struct vss_voice *v) {
	if(v->stack_top < 2) {
		return;
	}
	v->track_index = v->stack[--v->stack_top];
	v->track_position = (int32_t)v->stack[--v->stack_top];
}

// [=]===^=[ vss_cmd_start_loop ]=================================================================[=]
static void vss_cmd_start_loop(struct vss_voice *v, uint8_t *track) {
	uint8_t loop_count = track[v->track_position];
	v->track_position += 2;
	if(v->stack_top + 2 > VSS_STACK_SIZE) {
		return;
	}
	v->stack[v->stack_top++] = (uint32_t)v->track_position;
	v->stack[v->stack_top++] = (uint32_t)loop_count;
}

// [=]===^=[ vss_cmd_do_loop ]====================================================================[=]
static void vss_cmd_do_loop(struct vss_voice *v) {
	if(v->stack_top < 2) {
		return;
	}
	uint8_t loop_count = (uint8_t)v->stack[--v->stack_top];
	int32_t loop_position = (int32_t)v->stack[--v->stack_top];
	loop_count--;
	if(loop_count != 0) {
		if(v->stack_top + 2 > VSS_STACK_SIZE) {
			return;
		}
		v->stack[v->stack_top++] = (uint32_t)loop_position;
		v->stack[v->stack_top++] = (uint32_t)loop_count;
		v->track_position = loop_position;
	}
}

// [=]===^=[ vss_cmd_set_sample ]=================================================================[=]
static void vss_cmd_set_sample(struct voodoosupremesynthesizer_state *s, struct vss_voice *v, uint8_t *track) {
	uint8_t n1 = track[v->track_position++];
	v->sample1_index = (int32_t)n1;
	v->sample1_offset = 0;
	if((uint32_t)v->sample1_index < s->current_song->num_objects) {
		v->sample1_number = vss_find_sample_number(s, s->current_song->objects[v->sample1_index].file_offset);
	} else {
		v->sample1_number = -1;
	}

	uint8_t n2 = track[v->track_position++];
	v->sample2_index = (int32_t)n2;
	if((uint32_t)v->sample2_index < s->current_song->num_objects) {
		v->sample2_number = vss_find_sample_number(s, s->current_song->objects[v->sample2_index].file_offset);
	} else {
		v->sample2_number = -1;
	}
}

// [=]===^=[ vss_cmd_set_volume_envelope ]========================================================[=]
static void vss_cmd_set_volume_envelope(struct vss_voice *v, uint8_t *track) {
	v->volume_envelope_index = (int32_t)track[v->track_position++];
	v->volume_envelope_position = 0;
	v->volume_envelope_tick_counter = 1;
}

// [=]===^=[ vss_cmd_set_period_table ]===========================================================[=]
static void vss_cmd_set_period_table(struct vss_voice *v, uint8_t *track) {
	v->period_table_index = (int32_t)track[v->track_position++];
	v->period_table_position = 0;
	v->period_table_tick_counter = 1;
}

// [=]===^=[ vss_cmd_set_waveform_table ]=========================================================[=]
static void vss_cmd_set_waveform_table(struct voodoosupremesynthesizer_state *s, struct vss_voice *v, uint8_t *track) {
	v->waveform_table_index = (int32_t)track[v->track_position++];
	v->waveform_table_position = 0;
	v->waveform_tick_counter = 1;

	uint32_t mode = track[v->track_position++];
	uint32_t old_mode = v->synthesis_mode;
	int32_t ch = v->channel_number;

	if(mode & VSS_SYN_FREQUENCY_MAPPED) {
		v->synthesis_mode = mode;
	} else {
		v->waveform_position = (uint8_t)(mode & 0x1f);
		v->synthesis_mode = mode;
		v->waveform_start_position = v->waveform_position;
		if(old_mode & VSS_SYN_FREQUENCY_MAPPED) {
			paula_queue_sample(&s->paula, ch, v->audio_buffer[0], 0, 32);
			paula_set_loop(&s->paula, ch, 0, 32);
			v->waveform_increment = 0;
			v->waveform_start_position = 0;
		}
	}
}

// [=]===^=[ vss_cmd_portamento ]=================================================================[=]
static void vss_cmd_portamento(struct vss_voice *v, uint8_t *track) {
	uint8_t start_note = track[v->track_position++];
	uint16_t start_period = vss_get_period(v, start_note);
	v->note_period = start_period;
	v->new_note = 1;

	uint8_t stop_note = track[v->track_position++];
	uint16_t stop_period = vss_get_period(v, stop_note);
	int32_t delta = (int32_t)stop_period - (int32_t)start_period;
	if(delta < 0) {
		v->portamento_direction = 1;
		delta = -delta;
	} else {
		v->portamento_direction = 0;
	}

	uint8_t ticks = track[v->track_position];
	if(ticks == 0) {
		ticks = 1;
	}
	int32_t increment = (delta == 0) ? 1 : (delta / ticks);
	if(increment == 0) {
		increment = 1;
	}
	v->portamento_increment = (uint8_t)increment;
	v->portamento_delay = (delta == 0) ? 1 : (uint8_t)(ticks / delta);
	v->portamento_tick_counter = 1;
	if(v->portamento_delay == 0) {
		v->portamento_delay = 1;
	}
	v->portamento_duration = ticks;
}

// [=]===^=[ vss_cmd_set_transpose ]==============================================================[=]
static void vss_cmd_set_transpose(struct vss_voice *v, uint8_t *track) {
	v->transpose = (int8_t)track[v->track_position++];
}

// [=]===^=[ vss_cmd_goto ]=======================================================================[=]
static void vss_cmd_goto(struct vss_voice *v, uint8_t *track) {
	v->track_position = ((int32_t)track[v->track_position] << 8) | (int32_t)track[v->track_position + 1];
}

// [=]===^=[ vss_cmd_set_reset_flags ]============================================================[=]
static void vss_cmd_set_reset_flags(struct vss_voice *v, uint8_t *track) {
	v->reset_flags = track[v->track_position++];
}

// [=]===^=[ vss_cmd_set_waveform_mask ]==========================================================[=]
static void vss_cmd_set_waveform_mask(struct vss_voice *v, uint8_t *track) {
	v->waveform_mask = track[v->track_position++];
}

// [=]===^=[ vss_parse_track_command ]============================================================[=]
// Returns 0=continue, 1=exit (consume one tick), 2=set wait (treat like normal note end)
static int32_t vss_parse_track_command(struct voodoosupremesynthesizer_state *s, struct vss_voice *v, uint8_t *track, uint8_t cmd) {
	switch(cmd) {
		case VSS_EFF_NOTE_CUT: {
			vss_cmd_note_cut(v, track);
			return 1;
		}
		case VSS_EFF_GOSUB: {
			vss_cmd_gosub(v, track);
			return 0;
		}
		case VSS_EFF_RETURN: {
			vss_cmd_return(v);
			return 0;
		}
		case VSS_EFF_START_LOOP: {
			vss_cmd_start_loop(v, track);
			return 0;
		}
		case VSS_EFF_DO_LOOP: {
			vss_cmd_do_loop(v);
			return 0;
		}
		case VSS_EFF_SET_SAMPLE: {
			vss_cmd_set_sample(s, v, track);
			return 0;
		}
		case VSS_EFF_SET_VOL_ENV: {
			vss_cmd_set_volume_envelope(v, track);
			return 0;
		}
		case VSS_EFF_SET_PERIOD_TABLE: {
			vss_cmd_set_period_table(v, track);
			return 0;
		}
		case VSS_EFF_SET_WAVEFORM_TABLE: {
			vss_cmd_set_waveform_table(s, v, track);
			return 0;
		}
		case VSS_EFF_PORTAMENTO: {
			vss_cmd_portamento(v, track);
			return 2;
		}
		case VSS_EFF_SET_TRANSPOSE: {
			vss_cmd_set_transpose(v, track);
			return 0;
		}
		case VSS_EFF_GOTO: {
			vss_cmd_goto(v, track);
			return 0;
		}
		case VSS_EFF_SET_RESET_FLAGS: {
			vss_cmd_set_reset_flags(v, track);
			return 0;
		}
		case VSS_EFF_SET_WAVEFORM_MASK: {
			vss_cmd_set_waveform_mask(v, track);
			return 0;
		}
		default: {
			return 1;
		}
	}
}

// [=]===^=[ vss_parse_track_data ]===============================================================[=]
static void vss_parse_track_data(struct voodoosupremesynthesizer_state *s, struct vss_voice *v) {
	// C# original is `for(;;)` and continues whenever ParseTrackCommand returns
	// NextCommand. Most commands advance track_position then return NextCommand,
	// so the loop terminates naturally on the first non-command byte (period set)
	// or on NoteCut/default (Exit). A 65536-iteration guard catches a corrupt
	// or pathologically-built track that creates a Goto/Gosub cycle without ever
	// hitting a period-set byte. Not a known-bug masker -- keep.
	for(int32_t guard = 0; guard < 65536; ++guard) {
		uint8_t *track = (uint8_t *)s->current_song->objects[v->track_index].bytes;
		uint32_t tlen = s->current_song->objects[v->track_index].length;
		if(!track || (uint32_t)v->track_position >= tlen) {
			v->tick_counter = 1;
			break;
		}

		uint8_t cmd = track[v->track_position++];

		if(cmd >= 0x80) {
			int32_t result = vss_parse_track_command(s, v, track, cmd);
			if(result == 0) {
				continue;
			}
			if(result == 1) {
				break;
			}
		} else {
			v->note_period = vss_get_period(v, cmd);
			v->new_note = 1;
		}

		v->synthesis_mode &= ~VSS_SYN_STOP_SAMPLE;
		if((uint32_t)v->track_position >= tlen) {
			v->tick_counter = 1;
			break;
		}
		v->tick_counter = track[v->track_position++];

		if((v->reset_flags & VSS_RESET_WAVEFORM_TABLE) == 0) {
			v->waveform_table_position = 0;
			v->waveform_tick_counter = 1;
			v->waveform_position = v->waveform_start_position;
		}
		if((v->reset_flags & VSS_RESET_VOLUME_ENVELOPE) == 0) {
			v->volume_envelope_position = 0;
			v->volume_envelope_tick_counter = 1;
		}
		if((v->reset_flags & VSS_RESET_PERIOD_TABLE) == 0) {
			v->period_table_position = 0;
			v->period_table_tick_counter = 1;
		}
		break;
	}
}

// [=]===^=[ vss_process_voice ]==================================================================[=]
static void vss_process_voice(struct voodoosupremesynthesizer_state *s, struct vss_voice *v) {
	v->tick_counter--;
	if(v->tick_counter == 0) {
		vss_parse_track_data(s, v);
	}
	vss_do_volume_envelope(s, v);
	vss_do_period_table_part1(s, v);
	vss_do_portamento(v);
	vss_do_period_table_part2(v);
	vss_set_hardware(s, v);
	vss_waveform_generator(s, v);
	vss_audio_interrupt(s, v);
}

// [=]===^=[ vss_tick ]===========================================================================[=]
static void vss_tick(struct voodoosupremesynthesizer_state *s) {
	for(int32_t i = 0; i < 4; ++i) {
		vss_process_voice(s, &s->voices[i]);
	}
}

// [=]===^=[ vss_cleanup ]========================================================================[=]
static void vss_cleanup(struct voodoosupremesynthesizer_state *s) {
	if(!s) {
		return;
	}
	if(s->sub_songs) {
		for(uint32_t i = 0; i < s->num_sub_songs; ++i) {
			vss_free_song(&s->sub_songs[i]);
		}
		free(s->sub_songs);
		s->sub_songs = 0;
	}
	free(s->all_sample_offsets);
	s->all_sample_offsets = 0;
}

// [=]===^=[ voodoosupremesynthesizer_init ]======================================================[=]
static struct voodoosupremesynthesizer_state *voodoosupremesynthesizer_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 64 || sample_rate < 8000) {
		return 0;
	}

	int32_t footer_offset = 0;
	if(!vss_identify((uint8_t *)data, len, &footer_offset)) {
		return 0;
	}

	struct voodoosupremesynthesizer_state *s = (struct voodoosupremesynthesizer_state *)calloc(1, sizeof(struct voodoosupremesynthesizer_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;
	s->footer_offset = footer_offset;

	int32_t num_songs = vss_read_offset_table_size(s->module_data, s->module_len, footer_offset);
	if((num_songs <= 0) || (num_songs >= 0x100)) {
		free(s);
		return 0;
	}

	if((uint32_t)(footer_offset + 8 + num_songs * 4) > s->module_len) {
		free(s);
		return 0;
	}

	s->sub_songs = (struct vss_song *)calloc((size_t)num_songs, sizeof(struct vss_song));
	if(!s->sub_songs) {
		free(s);
		return 0;
	}
	s->num_sub_songs = (uint32_t)num_songs;

	for(int32_t i = 0; i < num_songs; ++i) {
		int32_t rel = vss_read_i32_be(s->module_data + footer_offset + 8 + i * 4);
		int32_t song_off = footer_offset + rel;
		if(!vss_load_sub_song(s->module_data, s->module_len, footer_offset, song_off, &s->sub_songs[i])) {
			vss_cleanup(s);
			free(s);
			return 0;
		}
	}

	if(!vss_build_all_samples(s)) {
		vss_cleanup(s);
		free(s);
		return 0;
	}

	memset(s->empty_sample, 0, sizeof(s->empty_sample));

	paula_init(&s->paula, sample_rate, VSS_TICK_HZ);
	vss_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ voodoosupremesynthesizer_free ]======================================================[=]
static void voodoosupremesynthesizer_free(struct voodoosupremesynthesizer_state *s) {
	if(!s) {
		return;
	}
	vss_cleanup(s);
	free(s);
}

// [=]===^=[ voodoosupremesynthesizer_get_audio ]=================================================[=]
static void voodoosupremesynthesizer_get_audio(struct voodoosupremesynthesizer_state *s, int16_t *output, int32_t frames) {
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
			vss_tick(s);
		}
	}
}

// [=]===^=[ voodoosupremesynthesizer_api_init ]==================================================[=]
static void *voodoosupremesynthesizer_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return voodoosupremesynthesizer_init(data, len, sample_rate);
}

// [=]===^=[ voodoosupremesynthesizer_api_free ]==================================================[=]
static void voodoosupremesynthesizer_api_free(void *state) {
	voodoosupremesynthesizer_free((struct voodoosupremesynthesizer_state *)state);
}

// [=]===^=[ voodoosupremesynthesizer_api_get_audio ]=============================================[=]
static void voodoosupremesynthesizer_api_get_audio(void *state, int16_t *output, int32_t frames) {
	voodoosupremesynthesizer_get_audio((struct voodoosupremesynthesizer_state *)state, output, frames);
}

static const char *voodoosupremesynthesizer_extensions[] = { "vss", 0 };

static struct player_api voodoosupremesynthesizer_api = {
	"Voodoo Supreme Synthesizer",
	voodoosupremesynthesizer_extensions,
	voodoosupremesynthesizer_api_init,
	voodoosupremesynthesizer_api_free,
	voodoosupremesynthesizer_api_get_audio,
	0,
};
