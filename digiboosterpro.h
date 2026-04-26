// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// DigiBooster Pro 2.x / 3.x (.dbm) replayer, ported from NostalgicPlayer's
// C# implementation. Drives an 8-channel Amiga Paula (see paula.h).
//
// Public API:
//   struct digiboosterpro_state *digiboosterpro_init(void *data, uint32_t len, int32_t sample_rate);
//   void digiboosterpro_free(struct digiboosterpro_state *s);
//   void digiboosterpro_get_audio(struct digiboosterpro_state *s, int16_t *output, int32_t frames);
//
// Scope and intentional exclusions:
//   - Standard sequencer (orders, patterns, rows, ticks, BPM/speed) is implemented.
//   - Standard tracker effects implemented: 0xx arpeggio, 1xx/2xx portamentos,
//     3xx/5xx porta-to-note (+ vol slide), 4xx/6xx vibrato (+ vol slide), 8xx
//     panning, 9xx sample offset, Axx volume slide, Bxx position jump, Cxx set
//     volume, Dxx pattern break, Exx extras (E1/E2 fine porta, E4 mute, E6 loop,
//     E7 sample offset hi, E8 panning, E9 retrig, EA/EB fine vol, EC cut,
//     ED note delay, EE pattern delay), Fxx set speed/tempo, Gxx global volume,
//     Hxx global volume slide, Pxx panning slide.
//   - Per-track DSP echo (V/W/X/Y/Z) is intentionally NOT implemented; those
//     effects are parsed and ignored. The DSPE chunk and global echo defaults
//     are skipped.
//   - Volume / panning envelopes (VENV/PENV) are parsed and applied per-tick
//     via EnvelopeInterpolator (mirrors C# MSynth_Do_Envelopes 1:1).
//   - Backwards playback (E3) is parsed but plays forward (paula.h has no
//     backwards mode).
//   - 32-bit samples are rejected (matches the original).
//   - 16-bit samples are converted to 8-bit signed at load time (high byte).

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#if defined(DBPRO_DEBUG_TEMPO) || defined(DBPRO_DEBUG_FREQ)
#include <stdio.h>
#endif

#define DBPRO_MAX_TRACKS    254
#define DBPRO_MAX_INSTR     255
#define DBPRO_MAX_SAMPLES   256
#define DBPRO_MAX_PATTERNS  65535

// Effect command opcodes (Effect.cs).
enum {
	DBPRO_FX_ARPEGGIO       = 0x00,
	DBPRO_FX_PORTA_UP       = 0x01,
	DBPRO_FX_PORTA_DOWN     = 0x02,
	DBPRO_FX_PORTA_TO_NOTE  = 0x03,
	DBPRO_FX_VIBRATO        = 0x04,
	DBPRO_FX_PORTA_VS       = 0x05,
	DBPRO_FX_VIBRATO_VS     = 0x06,
	DBPRO_FX_SET_PANNING    = 0x08,
	DBPRO_FX_SAMPLE_OFFSET  = 0x09,
	DBPRO_FX_VOLUME_SLIDE   = 0x0a,
	DBPRO_FX_POSITION_JUMP  = 0x0b,
	DBPRO_FX_SET_VOLUME     = 0x0c,
	DBPRO_FX_PATTERN_BREAK  = 0x0d,
	DBPRO_FX_EXTRA          = 0x0e,
	DBPRO_FX_SET_TEMPO      = 0x0f,
	DBPRO_FX_GLOBAL_VOL     = 0x10,
	DBPRO_FX_GLOBAL_VOL_SL  = 0x11,
	DBPRO_FX_PANNING_SLIDE  = 0x19,
	DBPRO_FX_ECHO_SWITCH    = 0x1f,
	DBPRO_FX_ECHO_DELAY     = 0x20,
	DBPRO_FX_ECHO_FEEDBACK  = 0x21,
	DBPRO_FX_ECHO_MIX       = 0x22,
	DBPRO_FX_ECHO_CROSS     = 0x23,
};

// Extended (E) opcodes, identified by the high nibble of the parameter.
enum {
	DBPRO_EX_FINE_PORTA_UP   = 0x1,
	DBPRO_EX_FINE_PORTA_DOWN = 0x2,
	DBPRO_EX_PLAY_BACKWARDS  = 0x3,
	DBPRO_EX_CHAN_CTRL_A     = 0x4,
	DBPRO_EX_SET_LOOP        = 0x6,
	DBPRO_EX_SET_SMP_OFFSET  = 0x7,
	DBPRO_EX_SET_PANNING     = 0x8,
	DBPRO_EX_RETRIG_NOTE     = 0x9,
	DBPRO_EX_FINE_VOL_UP     = 0xa,
	DBPRO_EX_FINE_VOL_DOWN   = 0xb,
	DBPRO_EX_NOTE_CUT        = 0xc,
	DBPRO_EX_NOTE_DELAY      = 0xd,
	DBPRO_EX_PATTERN_DELAY   = 0xe,
};

// Loop flags from instrument (InstrumentFlag).
enum {
	DBPRO_LOOP_NONE     = 0,
	DBPRO_LOOP_FORWARD  = 1,
	DBPRO_LOOP_PINGPONG = 2,
	DBPRO_LOOP_MASK     = 3,
};

// Envelope constants (Constants.cs).
#define DBPRO_ENV_MAX_POINTS        32
#define DBPRO_ENV_DISABLED          0xffff
#define DBPRO_ENV_LOOP_DISABLED     0xffff
#define DBPRO_ENV_SUSTAIN_DISABLED  0xffff

// Envelope flag bits (b[2] in the on-disk envelope record).
#define DBPRO_ENVF_ENABLED          0x01
#define DBPRO_ENVF_SUSTAIN_A        0x02
#define DBPRO_ENVF_LOOP             0x04
#define DBPRO_ENVF_SUSTAIN_B        0x08

#define DBPRO_CREATOR_DB2           2
#define DBPRO_CREATOR_DB3           3

struct dbpro_entry {
	uint8_t note;            // 0..11; 0 means no note in the row
	uint8_t octave;          // 0..7; 0 means no note in the row
	uint8_t instrument;      // 1-based; 0 means none
	uint8_t command1;
	uint8_t parameter1;
	uint8_t command2;
	uint8_t parameter2;
};

struct dbpro_pattern {
	uint16_t num_rows;
	struct dbpro_entry *entries;   // num_rows * num_tracks
};

struct dbpro_song {
	uint16_t num_orders;
	uint16_t *play_list;
};

struct dbpro_sample {
	int8_t *data;            // owned, malloc'd; null when empty
	uint32_t frames;         // sample frame count
};

struct dbpro_instrument {
	uint16_t volume;         // 0..64
	int16_t panning;         // -128..+128
	uint16_t sample_number;  // 0-based into samples
	uint32_t c3_frequency;   // in Hz
	uint32_t loop_start;
	uint32_t loop_length;
	uint16_t flags;          // InstrumentFlag bits
	uint16_t volume_envelope;  // index into volume_envelopes, 0xffff if none
	uint16_t panning_envelope; // index into panning_envelopes, 0xffff if none
};

struct dbpro_env_point {
	uint16_t position;
	int16_t value;
};

struct dbpro_envelope {
	uint16_t instrument_number;   // 1-based; 0xffff means no link
	uint16_t num_sections;
	uint16_t loop_first;
	uint16_t loop_last;            // 0xffff if no loop
	uint16_t sustain_a;            // 0xffff if disabled
	uint16_t sustain_b;            // 0xffff if disabled
	struct dbpro_env_point points[DBPRO_ENV_MAX_POINTS];
};

struct dbpro_env_interp {
	uint16_t index;        // index into env arrays, 0xffff if disabled
	uint16_t tick_counter;
	uint16_t section;
	int16_t  x_delta;
	int32_t  y_delta;
	int32_t  y_start;
	int16_t  previous_value;
	uint16_t sustain_a;    // mirror of envelope's, cleared to 0xffff on key-off
	uint16_t sustain_b;
	uint16_t loop_end;     // mirror; cleared on key-off when below sustain
};

struct dbpro_old_values {
	uint8_t volume_slide;
	uint8_t panning_slide;
	uint8_t portamento_up;
	uint8_t portamento_down;
	uint8_t portamento_speed;
	uint8_t volume_slide_5;
	uint8_t vibrato;
	uint8_t vibrato_6;
};

struct dbpro_track {
	uint16_t track_number;
	int32_t instrument;       // 1-based; 0 means none
	uint8_t is_on;
	uint8_t play_backwards;

	int32_t volume;           // speed-prescaled, 0..64*speed
	int32_t panning;          // speed-prescaled, -128*speed..+128*speed
	int32_t note;             // dbm2: absolute note index
	int32_t pitch;            // dbm2: period in 16ths; dbm3: speed*ftnote
	int16_t volume_delta;
	int16_t panning_delta;
	int16_t pitch_delta;
	int16_t porta3_delta;
	int16_t arp_table[3];
	int16_t porta3_target;
	int16_t vibrato_speed;
	int16_t vibrato_depth;
	int16_t vibrato_counter;

	int32_t trigger_counter;
	int32_t cut_counter;
	int32_t retrigger;
	int32_t trigger_offset;

	struct dbpro_old_values old;

	int32_t loop_counter;
	int32_t loop_order;
	int32_t loop_row;

	// Cached current sample binding for triggers.
	int8_t *sample_data;
	uint32_t sample_length;
	uint32_t sample_loop_start;
	uint32_t sample_loop_length;
	uint8_t sample_loop_type;  // 0 none, 1 forward, 2 pingpong (treated as forward)

	// Per-track envelope interpolators.
	struct dbpro_env_interp volume_env;
	struct dbpro_env_interp panning_env;
	int32_t volume_env_current;   // 0..16384 (1.0 == 16384). Set to 16384 on trigger.
	int32_t panning_env_current;  // -16384..+16384. Set to 0 on trigger.
};

struct dbpro_state {
	struct paula paula;

	uint8_t *module_data;       // caller-owned
	uint32_t module_len;

	uint16_t creator_version;   // 2 or 3
	uint16_t creator_revision;

	uint16_t num_instruments;
	uint16_t num_samples;
	uint16_t num_songs;
	uint16_t num_patterns;
	uint16_t num_tracks;

	struct dbpro_instrument *instruments;
	struct dbpro_sample *samples;
	struct dbpro_song *songs;
	struct dbpro_pattern *patterns;

	uint16_t num_volume_envelopes;
	uint16_t num_panning_envelopes;
	struct dbpro_envelope *volume_envelopes;
	struct dbpro_envelope *panning_envelopes;

	struct dbpro_track *tracks;

	int32_t song;
	int32_t order;
	int32_t pattern;
	int32_t row;
	int32_t tick;
	int32_t speed;
	int32_t tempo;
	int32_t pattern_delay;
	uint8_t delay_module_end;
	int32_t delay_pattern_break;
	int32_t delay_pattern_jump;
	int32_t delay_loop;
	int16_t global_volume;
	int16_t global_volume_slide;
	uint8_t old_global_volume_slide;
	int16_t min_volume;
	int16_t max_volume;
	int16_t min_panning;
	int16_t max_panning;
	int16_t min_pitch;
	int16_t max_pitch;
	uint8_t arp_counter;
	uint8_t end_reached;
	uint8_t restart_song;
};

// Periods used by version 2 modules (DBM2). 8 octaves x 12 notes.
static int16_t dbpro_periods[96] = {
	13696, 12928, 12192, 11520, 10848, 10240,  9664,  9120,  8608,  8128,  7680,  7248,
	 6848,  6464,  6096,  5760,  5424,  5120,  4832,  4560,  4304,  4064,  3840,  3624,
	 3424,  3232,  3048,  2880,  2712,  2560,  2416,  2280,  2152,  2032,  1920,  1812,
	 1712,  1616,  1524,  1440,  1356,  1280,  1208,  1140,  1076,  1016,   960,   904,
	  856,   808,   760,   720,   680,   640,   604,   572,   540,   508,   480,   452,
	  428,   404,   380,   360,   340,   320,   302,   286,   270,   254,   240,   226,
	  214,   202,   190,   180,   170,   160,   151,   143,   135,   127,   120,   113,
	  107,   101,    95,    90,    85,    80,    76,    72,    68,    64,    60,    57,
};

// Vibrato sine table (Tables.Vibrato).
static int16_t dbpro_vibrato[64] = {
	   0,   25,   50,   74,   98,  121,  142,  162,
	 181,  197,  213,  226,  237,  245,  251,  255,
	 256,  255,  251,  245,  237,  226,  213,  197,
	 181,  162,  142,  121,   98,   74,   50,   25,
	   0,  -25,  -50,  -74,  -98, -121, -142, -162,
	-181, -197, -213, -226, -237, -245, -251, -255,
	-256, -255, -251, -245, -237, -226, -213, -197,
	-181, -162, -142, -121,  -98,  -74,  -50,  -25,
};

// MusicScale: 12 semitones x 8 fine-tune steps (Tables.MusicScale).
static uint32_t dbpro_music_scale[96] = {
	 65536,  66011,  66489,  66971,  67456,  67945,  68438,  68933,
	 69433,  69936,  70443,  70953,  71468,  71985,  72507,  73032,
	 73562,  74095,  74632,  75172,  75717,  76266,  76819,  77375,
	 77936,  78501,  79069,  79642,  80220,  80801,  81386,  81976,
	 82570,  83169,  83771,  84378,  84990,  85606,  86226,  86851,
	 87480,  88114,  88752,  89396,  90043,  90696,  91353,  92015,
	 92682,  93354,  94030,  94711,  95398,  96089,  96785,  97487,
	 98193,  98905,  99621, 100340, 101070, 101800, 102540, 103280,
	104032, 104786, 105545, 106310, 107080, 107856, 108638, 109425,
	110218, 111017, 111821, 112631, 113448, 114270, 115098, 115932,
	116772, 117618, 118470, 119329, 120194, 121065, 121942, 122825,
	123715, 124612, 125515, 126425, 127341, 128263, 129193, 130129,
};

// Smooth porta tables - each row is 0..speed-1 entries; 0 = unused.
// Row indexed by speed (1..31). Column indexed by remainder (0..speed-1).
static uint32_t dbpro_smooth_porta[32][32] = {
	{ 0 },
	{ 65536 },
	{ 65536, 65773 },
	{ 65536, 65694, 65852 },
	{ 65536, 65654, 65773, 65892 },
	{ 65536, 65631, 65726, 65821, 65916 },
	{ 65536, 65615, 65694, 65773, 65852, 65932 },
	{ 65536, 65604, 65671, 65739, 65807, 65875, 65943 },
	{ 65536, 65595, 65654, 65714, 65773, 65832, 65892, 65951 },
	{ 65536, 65589, 65641, 65694, 65747, 65799, 65852, 65905, 65958 },
	{ 65536, 65583, 65631, 65678, 65726, 65773, 65821, 65868, 65916, 65963 },
	{ 65536, 65579, 65622, 65665, 65708, 65751, 65795, 65838, 65881, 65924, 65968 },
	{ 65536, 65575, 65615, 65654, 65694, 65733, 65773, 65813, 65852, 65892, 65932, 65971 },
	{ 65536, 65572, 65609, 65645, 65682, 65718, 65755, 65791, 65828, 65864, 65901, 65938, 65974 },
	{ 65536, 65570, 65604, 65637, 65671, 65705, 65739, 65773, 65807, 65841, 65875, 65909, 65943, 65977 },
	{ 65536, 65568, 65599, 65631, 65662, 65694, 65726, 65757, 65789, 65821, 65852, 65884, 65916, 65947, 65979 },
	{ 65536, 65566, 65595, 65625, 65654, 65684, 65714, 65743, 65773, 65803, 65832, 65862, 65892, 65922, 65951, 65981 },
	{ 65536, 65564, 65592, 65620, 65647, 65675, 65703, 65731, 65759, 65787, 65815, 65843, 65871, 65899, 65927, 65955, 65983 },
	{ 65536, 65562, 65589, 65615, 65641, 65668, 65694, 65720, 65747, 65773, 65799, 65826, 65852, 65879, 65905, 65932, 65958, 65984 },
	{ 65536, 65561, 65586, 65611, 65636, 65661, 65686, 65711, 65736, 65761, 65786, 65811, 65836, 65861, 65886, 65911, 65936, 65961, 65986 },
	{ 65536, 65560, 65583, 65607, 65631, 65654, 65678, 65702, 65726, 65749, 65773, 65797, 65821, 65844, 65868, 65892, 65916, 65939, 65963, 65987 },
	{ 65536, 65559, 65581, 65604, 65626, 65649, 65671, 65694, 65717, 65739, 65762, 65784, 65807, 65830, 65852, 65875, 65898, 65920, 65943, 65966, 65988 },
	{ 65536, 65558, 65579, 65601, 65622, 65644, 65665, 65687, 65708, 65730, 65751, 65773, 65795, 65816, 65838, 65859, 65881, 65903, 65924, 65946, 65968, 65989 },
	{ 65536, 65557, 65577, 65598, 65618, 65639, 65660, 65680, 65701, 65721, 65742, 65763, 65783, 65804, 65825, 65845, 65866, 65887, 65907, 65928, 65949, 65969, 65990 },
	{ 65536, 65556, 65575, 65595, 65615, 65635, 65654, 65674, 65694, 65714, 65733, 65753, 65773, 65793, 65813, 65832, 65852, 65872, 65892, 65912, 65932, 65951, 65971, 65991 },
	{ 65536, 65555, 65574, 65593, 65612, 65631, 65650, 65669, 65688, 65707, 65726, 65745, 65764, 65783, 65802, 65821, 65840, 65859, 65878, 65897, 65916, 65935, 65954, 65973, 65992 },
	{ 65536, 65554, 65572, 65591, 65609, 65627, 65645, 65664, 65682, 65700, 65718, 65737, 65755, 65773, 65791, 65810, 65828, 65846, 65864, 65883, 65901, 65919, 65938, 65956, 65974, 65993 },
	{ 65536, 65554, 65571, 65589, 65606, 65624, 65641, 65659, 65676, 65694, 65711, 65729, 65747, 65764, 65782, 65799, 65817, 65835, 65852, 65870, 65887, 65905, 65923, 65940, 65958, 65976, 65993 },
	{ 65536, 65553, 65570, 65587, 65604, 65621, 65637, 65654, 65671, 65688, 65705, 65722, 65739, 65756, 65773, 65790, 65807, 65824, 65841, 65858, 65875, 65892, 65909, 65926, 65943, 65960, 65977, 65994 },
	{ 65536, 65552, 65569, 65585, 65601, 65618, 65634, 65650, 65667, 65683, 65699, 65716, 65732, 65748, 65765, 65781, 65798, 65814, 65830, 65847, 65863, 65880, 65896, 65912, 65929, 65945, 65962, 65978, 65994 },
	{ 65536, 65552, 65568, 65583, 65599, 65615, 65631, 65647, 65662, 65678, 65694, 65710, 65726, 65741, 65757, 65773, 65789, 65805, 65821, 65836, 65852, 65868, 65884, 65900, 65916, 65932, 65947, 65963, 65979, 65995 },
	{ 65536, 65551, 65567, 65582, 65597, 65612, 65628, 65643, 65658, 65674, 65689, 65704, 65719, 65735, 65750, 65765, 65781, 65796, 65811, 65827, 65842, 65857, 65873, 65888, 65903, 65919, 65934, 65949, 65965, 65980, 65996 },
};

// [=]===^=[ dbpro_read_u16_be ]==================================================================[=]
static uint16_t dbpro_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ dbpro_read_u32_be ]==================================================================[=]
static uint32_t dbpro_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ dbpro_check_mark ]===================================================================[=]
static int32_t dbpro_check_mark(uint8_t *p, uint32_t len, uint32_t off, const char *mark, uint32_t mlen) {
	if(off + mlen > len) {
		return 0;
	}
	for(uint32_t i = 0; i < mlen; ++i) {
		if(p[off + i] != (uint8_t)mark[i]) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ dbpro_bcd2bin ]======================================================================[=]
static uint8_t dbpro_bcd2bin(uint8_t x) {
	uint8_t hi = (uint8_t)(x >> 4);
	uint8_t lo = (uint8_t)(x & 0x0f);
	uint8_t r = 0;
	if(hi < 10) {
		r = (uint8_t)(hi * 10);
	} else {
		return 0;
	}
	if(lo < 10) {
		r = (uint8_t)(r + lo);
	} else {
		return 0;
	}
	return r;
}

// [=]===^=[ dbpro_set_bpm_tempo ]================================================================[=]
// ProTracker BPM scale: samples_per_tick = sample_rate * 5 / (2 * bpm).
static void dbpro_set_bpm_tempo(struct dbpro_state *s, uint32_t bpm) {
	if(bpm < 28) {
		bpm = 28;
	}
	int32_t spt = (s->paula.sample_rate * 5) / ((int32_t)bpm * 2);
	if(spt < 1) {
		spt = 1;
	}
	s->paula.samples_per_tick = spt;
	if(s->paula.tick_offset >= spt) {
		s->paula.tick_offset = 0;
	}
#ifdef DBPRO_DEBUG_TEMPO
	fprintf(stderr, "[dbpro] bpm=%u spt=%d tick_hz=%.2f\n",
		(unsigned)bpm, spt, (double)s->paula.sample_rate / spt);
#endif
}

// [=]===^=[ dbpro_free_module ]==================================================================[=]
static void dbpro_free_module(struct dbpro_state *s) {
	if(s->instruments) {
		free(s->instruments);
		s->instruments = 0;
	}
	if(s->samples) {
		for(uint32_t i = 0; i < s->num_samples; ++i) {
			if(s->samples[i].data) {
				free(s->samples[i].data);
			}
		}
		free(s->samples);
		s->samples = 0;
	}
	if(s->songs) {
		for(uint32_t i = 0; i < s->num_songs; ++i) {
			if(s->songs[i].play_list) {
				free(s->songs[i].play_list);
			}
		}
		free(s->songs);
		s->songs = 0;
	}
	if(s->patterns) {
		for(uint32_t i = 0; i < s->num_patterns; ++i) {
			if(s->patterns[i].entries) {
				free(s->patterns[i].entries);
			}
		}
		free(s->patterns);
		s->patterns = 0;
	}
	if(s->tracks) {
		free(s->tracks);
		s->tracks = 0;
	}
	if(s->volume_envelopes) {
		free(s->volume_envelopes);
		s->volume_envelopes = 0;
	}
	if(s->panning_envelopes) {
		free(s->panning_envelopes);
		s->panning_envelopes = 0;
	}
}

// [=]===^=[ dbpro_load_chunk_info ]==============================================================[=]
static int32_t dbpro_load_chunk_info(struct dbpro_state *s, uint8_t *cdata, uint32_t clen) {
	if(clen < 10) {
		return 0;
	}
	s->num_instruments = dbpro_read_u16_be(&cdata[0]);
	s->num_samples = dbpro_read_u16_be(&cdata[2]);
	s->num_songs = dbpro_read_u16_be(&cdata[4]);
	s->num_patterns = dbpro_read_u16_be(&cdata[6]);
	s->num_tracks = dbpro_read_u16_be(&cdata[8]);
	if(s->num_instruments == 0 || s->num_instruments > 255) {
		return 0;
	}
	if(s->num_samples == 0 || s->num_samples > 255) {
		return 0;
	}
	if(s->num_tracks == 0 || s->num_tracks > DBPRO_MAX_TRACKS || (s->num_tracks & 1) != 0) {
		return 0;
	}
	if(s->num_songs == 0 || s->num_songs > 255) {
		return 0;
	}
	if(s->num_patterns == 0) {
		return 0;
	}
	s->instruments = (struct dbpro_instrument *)calloc(s->num_instruments, sizeof(struct dbpro_instrument));
	s->samples = (struct dbpro_sample *)calloc(s->num_samples, sizeof(struct dbpro_sample));
	s->songs = (struct dbpro_song *)calloc(s->num_songs, sizeof(struct dbpro_song));
	s->patterns = (struct dbpro_pattern *)calloc(s->num_patterns, sizeof(struct dbpro_pattern));
	s->tracks = (struct dbpro_track *)calloc(s->num_tracks, sizeof(struct dbpro_track));
	if(!s->instruments || !s->samples || !s->songs || !s->patterns || !s->tracks) {
		return 0;
	}
	return 1;
}

// [=]===^=[ dbpro_load_chunk_song ]==============================================================[=]
static int32_t dbpro_load_chunk_song(struct dbpro_state *s, uint8_t *cdata, uint32_t clen) {
	uint32_t pos = 0;
	for(uint32_t i = 0; i < s->num_songs; ++i) {
		if(pos + 46 > clen) {
			return 0;
		}
		uint16_t num_orders = dbpro_read_u16_be(&cdata[pos + 44]);
		pos += 46;
		uint32_t list_bytes = (uint32_t)num_orders * 2;
		if(pos + list_bytes > clen) {
			return 0;
		}
		s->songs[i].num_orders = num_orders;
		s->songs[i].play_list = (uint16_t *)calloc(num_orders ? num_orders : 1, sizeof(uint16_t));
		if(!s->songs[i].play_list) {
			return 0;
		}
		for(uint32_t j = 0; j < num_orders; ++j) {
			s->songs[i].play_list[j] = dbpro_read_u16_be(&cdata[pos + j * 2]);
		}
		pos += list_bytes;
	}
	return 1;
}

// [=]===^=[ dbpro_load_chunk_inst ]==============================================================[=]
static int32_t dbpro_load_chunk_inst(struct dbpro_state *s, uint8_t *cdata, uint32_t clen) {
	uint32_t pos = 0;
	for(uint32_t i = 0; i < s->num_instruments; ++i) {
		if(pos + 50 > clen) {
			return 0;
		}
		uint8_t *b = &cdata[pos];
		struct dbpro_instrument *mi = &s->instruments[i];
		uint16_t sn = dbpro_read_u16_be(&b[30]);
		mi->sample_number = (uint16_t)(sn ? sn - 1 : 0);
		mi->volume = dbpro_read_u16_be(&b[32]);
		mi->c3_frequency = dbpro_read_u32_be(&b[34]);
		mi->loop_start = dbpro_read_u32_be(&b[38]);
		mi->loop_length = dbpro_read_u32_be(&b[42]);
		mi->panning = (int16_t)dbpro_read_u16_be(&b[46]);
		mi->flags = dbpro_read_u16_be(&b[48]);
		if(mi->loop_length == 0) {
			mi->flags = (uint16_t)(mi->flags & ~DBPRO_LOOP_MASK);
		}
		if((mi->flags & DBPRO_LOOP_MASK) == DBPRO_LOOP_NONE) {
			mi->loop_start = 0;
			mi->loop_length = 0;
		}
		mi->volume_envelope = DBPRO_ENV_DISABLED;
		mi->panning_envelope = DBPRO_ENV_DISABLED;
		pos += 50;
	}
	return 1;
}

// [=]===^=[ dbpro_load_envelope ]================================================================[=]
// Reads one 136-byte envelope record from cdata at *pos. Mirrors C#
// Loader.Read_Envelope. is_panning controls panning-specific value validation
// (and the DigiBooster-2 *4-128 conversion) ; on validation failure returns 0
// after writing default-disabled fields. On success returns 1 with mEnv
// populated.
static int32_t dbpro_load_envelope(struct dbpro_state *s, struct dbpro_envelope *me, uint8_t *cdata, uint32_t clen, uint32_t *pos, int32_t is_panning) {
	uint32_t p = *pos;
	if(p + 136 > clen) {
		return 0;
	}
	uint8_t *b = cdata + p;
	uint8_t flags = b[2];
	uint16_t sections = b[3];
	if(sections > 31) {
		sections = 31;
	}
	me->num_sections = sections;
	me->sustain_a = DBPRO_ENV_SUSTAIN_DISABLED;
	me->sustain_b = DBPRO_ENV_SUSTAIN_DISABLED;
	me->loop_first = DBPRO_ENV_LOOP_DISABLED;
	me->loop_last = DBPRO_ENV_LOOP_DISABLED;

	uint16_t instrument = (uint16_t)((b[0] << 8) | b[1]);
	if(instrument == 0 || instrument > 255) {
		me->instrument_number = DBPRO_ENV_DISABLED;
	} else {
		me->instrument_number = instrument;
	}

	if((flags & DBPRO_ENVF_ENABLED) != 0) {
		if((flags & DBPRO_ENVF_SUSTAIN_B) != 0) {
			if(b[7] <= me->num_sections) {
				me->sustain_b = b[7];
			} else {
				return 0;
			}
		}
		if((flags & DBPRO_ENVF_SUSTAIN_A) != 0) {
			if(b[4] <= me->num_sections) {
				me->sustain_a = b[4];
			} else {
				return 0;
			}
		}
		if((flags & DBPRO_ENVF_LOOP) != 0) {
			// TNE bug-fix from C#: b[5] <= b[6] (equality permitted) so Dead
			// Ahead - Part M H Variations.dbm can be loaded.
			if((b[6] <= me->num_sections) && (b[5] <= b[6])) {
				me->loop_first = b[5];
				me->loop_last = b[6];
			} else {
				return 0;
			}
		}
		if(me->sustain_a != DBPRO_ENV_SUSTAIN_DISABLED && me->sustain_b != DBPRO_ENV_SUSTAIN_DISABLED) {
			if(me->sustain_a > me->sustain_b) {
				uint16_t tmp = me->sustain_a;
				me->sustain_a = me->sustain_b;
				me->sustain_b = tmp;
			}
		}
	}

	uint32_t pp = 8;
	uint32_t total_pts = (uint32_t)sections + 1u;
	for(uint32_t pt = 0; pt < total_pts; ++pt) {
		int16_t ppos = (int16_t)((b[pp] << 8) | b[pp + 1]);
		int16_t pval = (int16_t)((b[pp + 2] << 8) | b[pp + 3]);
		pp += 4;
		if((flags & DBPRO_ENVF_ENABLED) != 0) {
			if(is_panning) {
				if(s->creator_version == DBPRO_CREATOR_DB2) {
					pval = (int16_t)((pval << 2) - 128);
				}
				if(ppos < 0 || ppos > 2048) {
					return 0;
				}
				if(pval < -128 || pval > 128) {
					return 0;
				}
			} else {
				if(ppos < 0 || ppos > 2048) {
					return 0;
				}
				if(pval < 0 || pval > 64) {
					return 0;
				}
			}
		} else {
			ppos = 0;
			pval = 0;
		}
		me->points[pt].position = (uint16_t)ppos;
		me->points[pt].value = pval;
	}
	*pos = p + 136;
	return 1;
}

// [=]===^=[ dbpro_load_chunk_venv ]===============================================================[=]
static int32_t dbpro_load_chunk_venv(struct dbpro_state *s, uint8_t *cdata, uint32_t clen) {
	if(clen < 2) {
		return 0;
	}
	uint16_t n = (uint16_t)((cdata[0] << 8) | cdata[1]);
	if(n > 255) {
		return 0;
	}
	s->num_volume_envelopes = n;
	if(n == 0) {
		return 1;
	}
	s->volume_envelopes = (struct dbpro_envelope *)calloc(n, sizeof(struct dbpro_envelope));
	if(!s->volume_envelopes) {
		return 0;
	}
	uint32_t pos = 2;
	for(uint32_t i = 0; i < n; ++i) {
		if(!dbpro_load_envelope(s, &s->volume_envelopes[i], cdata, clen, &pos, 0)) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ dbpro_load_chunk_penv ]===============================================================[=]
static int32_t dbpro_load_chunk_penv(struct dbpro_state *s, uint8_t *cdata, uint32_t clen) {
	if(clen < 2) {
		return 0;
	}
	uint16_t n = (uint16_t)((cdata[0] << 8) | cdata[1]);
	if(n > 255) {
		return 0;
	}
	s->num_panning_envelopes = n;
	if(n == 0) {
		return 1;
	}
	s->panning_envelopes = (struct dbpro_envelope *)calloc(n, sizeof(struct dbpro_envelope));
	if(!s->panning_envelopes) {
		return 0;
	}
	uint32_t pos = 2;
	for(uint32_t i = 0; i < n; ++i) {
		if(!dbpro_load_envelope(s, &s->panning_envelopes[i], cdata, clen, &pos, 1)) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ dbpro_assign_envelopes ]=============================================================[=]
// Mirrors C# Loader.Assign_Envelopes: link each envelope's instrumentNumber
// back into the instrument's volume_envelope / panning_envelope index.
// Returns 0 on out-of-range instrument numbers (treated as data corruption).
static int32_t dbpro_assign_envelopes(struct dbpro_state *s) {
	for(uint32_t i = 0; i < s->num_volume_envelopes; ++i) {
		uint16_t instr = s->volume_envelopes[i].instrument_number;
		if(instr == DBPRO_ENV_DISABLED) {
			continue;
		}
		if(instr == 0 || instr > s->num_instruments) {
			return 0;
		}
		s->instruments[instr - 1].volume_envelope = (uint16_t)i;
	}
	for(uint32_t i = 0; i < s->num_panning_envelopes; ++i) {
		uint16_t instr = s->panning_envelopes[i].instrument_number;
		if(instr == DBPRO_ENV_DISABLED) {
			continue;
		}
		if(instr == 0 || instr > s->num_instruments) {
			return 0;
		}
		s->instruments[instr - 1].panning_envelope = (uint16_t)i;
	}
	return 1;
}

// [=]===^=[ dbpro_load_pattern ]=================================================================[=]
static int32_t dbpro_load_pattern(struct dbpro_state *s, struct dbpro_pattern *mp, uint8_t *cdata, uint32_t clen, uint32_t *cursor) {
	uint32_t pos = *cursor;
	if(pos + 6 > clen) {
		return 0;
	}
	uint16_t rows = dbpro_read_u16_be(&cdata[pos]);
	int32_t pack_size = (int32_t)dbpro_read_u32_be(&cdata[pos + 2]);
	pos += 6;
	if(rows == 0 || pack_size <= 0 || pos + (uint32_t)pack_size > clen) {
		return 0;
	}
	mp->num_rows = rows;
	mp->entries = (struct dbpro_entry *)calloc((uint32_t)rows * s->num_tracks, sizeof(struct dbpro_entry));
	if(!mp->entries) {
		return 0;
	}
	uint8_t *pd = &cdata[pos];
	int32_t pack_counter = pack_size;
	int32_t state = 0; // 0=track, 1=bitfield, 2=note, 3=instr, 4=cmd1, 5=par1, 6=cmd2, 7=par2
	uint32_t row = 0;
	uint8_t bit_field = 0;
	struct dbpro_entry *me = 0;
	int32_t off = 0;
	while(pack_counter-- > 0 && row < rows) {
		uint8_t byte = pd[off++];
		switch(state) {
			case 0: {
				if(byte == 0) {
					row++;
				} else {
					if(byte <= s->num_tracks) {
						me = &mp->entries[row * s->num_tracks + (byte - 1)];
					} else {
						me = 0;
						return 0;
					}
					state = 1;
				}
				break;
			}

			case 1: {
				bit_field = byte;
				if(bit_field & 0x01) {
					state = 2;
				} else if(bit_field & 0x02) {
					state = 3;
				} else if(bit_field & 0x04) {
					state = 4;
				} else if(bit_field & 0x08) {
					state = 5;
				} else if(bit_field & 0x10) {
					state = 6;
				} else if(bit_field & 0x20) {
					state = 7;
				} else {
					state = 0;
				}
				break;
			}

			case 2: {
				if(me) {
					me->octave = (uint8_t)(byte >> 4);
					me->note = (uint8_t)(byte & 0x0f);
				}
				if(bit_field & 0x02) {
					state = 3;
				} else if(bit_field & 0x04) {
					state = 4;
				} else if(bit_field & 0x08) {
					state = 5;
				} else if(bit_field & 0x10) {
					state = 6;
				} else if(bit_field & 0x20) {
					state = 7;
				} else {
					state = 0;
				}
				break;
			}

			case 3: {
				if(me) {
					me->instrument = byte;
				}
				if(bit_field & 0x04) {
					state = 4;
				} else if(bit_field & 0x08) {
					state = 5;
				} else if(bit_field & 0x10) {
					state = 6;
				} else if(bit_field & 0x20) {
					state = 7;
				} else {
					state = 0;
				}
				break;
			}

			case 4: {
				if(me) {
					me->command1 = byte;
				}
				if(bit_field & 0x08) {
					state = 5;
				} else if(bit_field & 0x10) {
					state = 6;
				} else if(bit_field & 0x20) {
					state = 7;
				} else {
					state = 0;
				}
				break;
			}

			case 5: {
				if(me) {
					me->parameter1 = byte;
				}
				if(bit_field & 0x10) {
					state = 6;
				} else if(bit_field & 0x20) {
					state = 7;
				} else {
					state = 0;
				}
				break;
			}

			case 6: {
				if(me) {
					me->command2 = byte;
				}
				if(bit_field & 0x20) {
					state = 7;
				} else {
					state = 0;
				}
				break;
			}

			case 7: {
				if(me) {
					me->parameter2 = byte;
				}
				state = 0;
				break;
			}
		}
	}
	*cursor = pos + (uint32_t)pack_size;
	return 1;
}

// [=]===^=[ dbpro_load_chunk_patt ]==============================================================[=]
static int32_t dbpro_load_chunk_patt(struct dbpro_state *s, uint8_t *cdata, uint32_t clen) {
	uint32_t cursor = 0;
	for(uint32_t i = 0; i < s->num_patterns; ++i) {
		if(!dbpro_load_pattern(s, &s->patterns[i], cdata, clen, &cursor)) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ dbpro_load_chunk_smpl ]==============================================================[=]
// Each sample: 8 bytes header (flags+frames), then frame data. 16-bit data is
// big-endian. We downconvert 16-bit to 8-bit signed by taking the high byte.
static int32_t dbpro_load_chunk_smpl(struct dbpro_state *s, uint8_t *cdata, uint32_t clen) {
	uint32_t pos = 0;
	for(uint32_t i = 0; i < s->num_samples; ++i) {
		if(pos + 8 > clen) {
			return 0;
		}
		uint8_t *b = &cdata[pos];
		uint8_t bps = (uint8_t)(b[3] & 0x07);
		uint32_t frames = dbpro_read_u32_be(&b[4]);
		pos += 8;
		s->samples[i].frames = frames;
		if(frames == 0 || frames >= 0x40000000u) {
			s->samples[i].data = 0;
			continue;
		}
		switch(bps) {
			case 1: {
				if(pos + frames > clen) {
					return 0;
				}
				s->samples[i].data = (int8_t *)malloc(frames);
				if(!s->samples[i].data) {
					return 0;
				}
				for(uint32_t j = 0; j < frames; ++j) {
					s->samples[i].data[j] = (int8_t)cdata[pos + j];
				}
				pos += frames;
				break;
			}

			case 2: {
				uint32_t bytes = frames * 2;
				if(pos + bytes > clen) {
					return 0;
				}
				s->samples[i].data = (int8_t *)malloc(frames);
				if(!s->samples[i].data) {
					return 0;
				}
				for(uint32_t j = 0; j < frames; ++j) {
					s->samples[i].data[j] = (int8_t)cdata[pos + j * 2];
				}
				pos += bytes;
				break;
			}

			default: {
				return 0;
			}
		}
	}
	return 1;
}

// [=]===^=[ dbpro_verify ]=======================================================================[=]
static int32_t dbpro_verify(struct dbpro_state *s) {
	for(uint32_t i = 0; i < s->num_instruments; ++i) {
		struct dbpro_instrument *mi = &s->instruments[i];
		if(mi->sample_number >= s->num_samples) {
			mi->sample_number = 0;
		}
		struct dbpro_sample *sm = &s->samples[mi->sample_number];
		if(sm && sm->frames > 0) {
			if((mi->flags & DBPRO_LOOP_MASK) != 0) {
				if(mi->loop_start >= sm->frames) {
					mi->loop_start = 0;
					mi->loop_length = 0;
					mi->flags = (uint16_t)(mi->flags & ~DBPRO_LOOP_MASK);
				}
				if(mi->loop_start + mi->loop_length > sm->frames) {
					mi->loop_length = sm->frames - mi->loop_start;
				}
			}
		} else {
			mi->loop_start = 0;
			mi->loop_length = 0;
			mi->flags = (uint16_t)(mi->flags & ~DBPRO_LOOP_MASK);
		}
		if(mi->c3_frequency < 1000 || mi->c3_frequency > 192000) {
			return 0;
		}
	}
	for(uint32_t s_i = 0; s_i < s->num_songs; ++s_i) {
		struct dbpro_song *sng = &s->songs[s_i];
		for(uint32_t o = 0; o < sng->num_orders; ++o) {
			if(sng->play_list[o] >= s->num_patterns) {
				return 0;
			}
		}
	}
	return 1;
}

// [=]===^=[ dbpro_load ]=========================================================================[=]
static int32_t dbpro_load(struct dbpro_state *s) {
	uint8_t *d = s->module_data;
	uint32_t len = s->module_len;
	if(len < 16) {
		return 0;
	}
	if(!dbpro_check_mark(d, len, 0, "DBM0", 4)) {
		return 0;
	}
	uint8_t version = d[4];
	if(version == 2) {
		s->creator_version = 2;
	} else if(version == 3) {
		s->creator_version = 3;
	} else {
		return 0;
	}
	s->creator_revision = dbpro_bcd2bin(d[5]);
	uint32_t pos = 8;
	uint8_t have_info = 0;
	while(pos + 8 <= len) {
		char mark[5];
		mark[0] = (char)d[pos + 0];
		mark[1] = (char)d[pos + 1];
		mark[2] = (char)d[pos + 2];
		mark[3] = (char)d[pos + 3];
		mark[4] = 0;
		uint32_t size = dbpro_read_u32_be(&d[pos + 4]);
		pos += 8;
		if(pos + size > len) {
			return 0;
		}
		uint8_t *cdata = &d[pos];
		if(memcmp(mark, "INFO", 4) == 0) {
			if(!dbpro_load_chunk_info(s, cdata, size)) {
				return 0;
			}
			have_info = 1;
		} else if(memcmp(mark, "SONG", 4) == 0) {
			if(!have_info || !dbpro_load_chunk_song(s, cdata, size)) {
				return 0;
			}
		} else if(memcmp(mark, "INST", 4) == 0) {
			if(!have_info || !dbpro_load_chunk_inst(s, cdata, size)) {
				return 0;
			}
		} else if(memcmp(mark, "PATT", 4) == 0) {
			if(!have_info || !dbpro_load_chunk_patt(s, cdata, size)) {
				return 0;
			}
		} else if(memcmp(mark, "SMPL", 4) == 0) {
			if(!have_info || !dbpro_load_chunk_smpl(s, cdata, size)) {
				return 0;
			}
		} else if(memcmp(mark, "VENV", 4) == 0) {
			if(!have_info || !dbpro_load_chunk_venv(s, cdata, size)) {
				return 0;
			}
		} else if(memcmp(mark, "PENV", 4) == 0) {
			if(!have_info || !dbpro_load_chunk_penv(s, cdata, size)) {
				return 0;
			}
		}
		// NAME / DSPE intentionally skipped (DSP echo not implemented).
		pos += size;
	}
	if(!have_info) {
		return 0;
	}
	if(!dbpro_assign_envelopes(s)) {
		return 0;
	}
	if(!dbpro_verify(s)) {
		return 0;
	}
	return 1;
}

// [=]===^=[ dbpro_porta_to_note_p ]===============================================================[=]
static int32_t dbpro_porta_to_note_p(struct dbpro_entry *me) {
	uint8_t c1 = me->command1;
	uint8_t c2 = me->command2;
	if(c1 == DBPRO_FX_PORTA_TO_NOTE || c1 == DBPRO_FX_PORTA_VS) {
		return 1;
	}
	if(c2 == DBPRO_FX_PORTA_TO_NOTE || c2 == DBPRO_FX_PORTA_VS) {
		return 1;
	}
	return 0;
}

// [=]===^=[ dbpro_init_tracks ]==================================================================[=]
static void dbpro_init_tracks(struct dbpro_state *s) {
	for(uint32_t i = 0; i < s->num_tracks; ++i) {
		struct dbpro_track *mt = &s->tracks[i];
		memset(mt, 0, sizeof(*mt));
		mt->track_number = (uint16_t)i;
		mt->trigger_counter = 0x7fffffff;
		mt->cut_counter = 0x7fffffff;
		mt->porta3_target = 576;       // C-4
		mt->volume_env.index = DBPRO_ENV_DISABLED;
		mt->panning_env.index = DBPRO_ENV_DISABLED;
		mt->volume_env_current = 16384;
		mt->panning_env_current = 0;
		// LRRL panning, mirroring NostalgicPlayer's default Amiga pan.
		s->paula.ch[i & 7].pan = (((i & 2) >> 1) ^ (i & 1)) ? 127 : 0;
	}
}

// [=]===^=[ dbpro_reset ]========================================================================[=]
static void dbpro_reset(struct dbpro_state *s) {
	s->tick = 0;
	s->speed = 6;
	s->tempo = 125;
	s->pattern_delay = 0;
	s->pattern = 0;
	s->row = 0;
	s->order = 0;
	s->song = 0;
	s->global_volume = 64;
	s->global_volume_slide = 0;
	s->arp_counter = 0;
	s->min_volume = 0;
	s->min_panning = -768;
	s->min_pitch = 576;
	s->max_volume = 384;
	s->max_panning = 768;
	s->max_pitch = 4608;
	s->delay_module_end = 0;
	s->delay_pattern_break = -1;
	s->delay_pattern_jump = -1;
	s->delay_loop = -1;
	s->end_reached = 0;
	s->restart_song = 0;
	dbpro_init_tracks(s);
	if(s->num_songs > 0 && s->songs[0].num_orders > 0) {
		s->pattern = s->songs[0].play_list[0];
	}
	dbpro_set_bpm_tempo(s, (uint32_t)s->tempo);
}

// [=]===^=[ dbpro_set_freq ]=====================================================================[=]
// DBP's pitch math produces playback frequencies up to ~150 kHz which exceed
// what the period model in paula.h can represent (PAULA_MIN_PERIOD clamps to
// ~28.6 kHz). NostalgicPlayer's mixer takes raw Hz, so we do the same.
static void dbpro_set_freq(struct dbpro_state *s, int32_t channel, uint32_t freq) {
	if(freq == 0) {
		paula_mute(&s->paula, channel);
		return;
	}
#ifdef DBPRO_DEBUG_FREQ
	{
		static int32_t s_count = 0;
		if(s_count < 16) {
			fprintf(stderr, "[dbpro] ch=%d freq=%u\n", channel, (unsigned)freq);
			s_count++;
		}
	}
#endif
	paula_set_freq_hz(&s->paula, channel, freq);
}

// [=]===^=[ dbpro_msynth_pitch ]=================================================================[=]
// Compute and apply the playback frequency for the track. Updates Paula period
// for the channel. Mirrors MSynth_Pitch in the C# source.
static void dbpro_msynth_pitch(struct dbpro_state *s, struct dbpro_track *mt, int32_t channel) {
	int32_t pitch = mt->pitch;
	if(mt->instrument == 0) {
		return;
	}
	struct dbpro_instrument *mi = &s->instruments[mt->instrument - 1];
	if(s->creator_version == 2) {
		int16_t arp = mt->arp_table[s->arp_counter];
		int32_t note_idx = mt->note + arp;
		if(note_idx >= 0 && note_idx < 96) {
			pitch = dbpro_periods[note_idx];
		}
		int32_t frequency = 3579545 / pitch;
		pitch = (int32_t)(3579545 / ((((int64_t)mi->c3_frequency * 256) / 8363 * frequency) / 256));
		int32_t vib_val = ((dbpro_vibrato[mt->vibrato_counter & 0x3f] * 5 / 3) * mt->vibrato_depth) / 128;
		pitch += vib_val;
		if(pitch < 1) {
			pitch = 1;
		}
		uint32_t freq = (uint32_t)((3546895 / pitch) * 256 / 64);
		dbpro_set_freq(s, channel, freq);
	} else {
		pitch += mt->arp_table[s->arp_counter];
		int32_t vib_val = (int32_t)dbpro_vibrato[mt->vibrato_counter & 0x3f] * mt->vibrato_depth;
		pitch += vib_val >> 8;
		if(pitch < s->min_pitch) {
			pitch = s->min_pitch;
		}
		if(pitch > s->max_pitch) {
			pitch = s->max_pitch;
		}
		uint32_t octave = 0;
		uint32_t s_porta = (uint32_t)(pitch % s->speed);
		int32_t f_tune = pitch / s->speed;
		uint32_t alpha = dbpro_smooth_porta[s->speed][s_porta];
		f_tune -= 96;
		while(f_tune >= 96) {
			f_tune -= 96;
			octave++;
		}
		if(f_tune < 0) {
			f_tune = 0;
		}
		uint32_t beta = dbpro_music_scale[f_tune];
		uint64_t freq64 = (uint64_t)mi->c3_frequency * beta * alpha;
		uint32_t shift = 19;
		if(octave > 19) {
			octave = 19;
		}
		shift -= octave;
		freq64 >>= shift;
		uint32_t freq = (uint32_t)(freq64 / 65536);
		dbpro_set_freq(s, channel, freq);
	}
	mt->vibrato_counter = (int16_t)((mt->vibrato_counter + mt->vibrato_speed) & 0x3f);
}

// [=]===^=[ dbpro_msynth_instrument ]============================================================[=]
static void dbpro_msynth_instrument(struct dbpro_state *s, struct dbpro_track *mt, int32_t instr) {
	mt->instrument = 0;
	mt->is_on = 0;
	if(instr <= 0 || instr > s->num_instruments) {
		return;
	}
	struct dbpro_instrument *mi = &s->instruments[instr - 1];
	// Wire up the envelope interpolator indices regardless of sample
	// availability (matches C# which assigns mt.VolumeEnvelope.Index /
	// mt.PanningEnvelope.Index unconditionally before the sample check).
	mt->volume_env.index = mi->volume_envelope;
	mt->panning_env.index = mi->panning_envelope;
	struct dbpro_sample *sm = &s->samples[mi->sample_number];
	if(!sm || !sm->data || sm->frames == 0) {
		return;
	}
	mt->sample_data = sm->data;
	mt->sample_length = sm->frames;
	uint16_t loop = (uint16_t)(mi->flags & DBPRO_LOOP_MASK);
	if(loop == DBPRO_LOOP_NONE) {
		mt->sample_loop_start = 0;
		mt->sample_loop_length = 0;
		mt->sample_loop_type = 0;
	} else {
		mt->sample_loop_start = mi->loop_start;
		mt->sample_loop_length = mi->loop_length;
		mt->sample_loop_type = (uint8_t)loop;
	}
	mt->instrument = instr;
}

// [=]===^=[ dbpro_msynth_default_volume ]========================================================[=]
static void dbpro_msynth_default_volume(struct dbpro_state *s, struct dbpro_track *mt) {
	if(mt->instrument == 0) {
		return;
	}
	struct dbpro_instrument *mi = &s->instruments[mt->instrument - 1];
	mt->volume = (int32_t)mi->volume * s->speed;
	mt->panning = (int32_t)mi->panning * s->speed;
	// Volume reset restarts both envelopes (C# MSynth_DefaultVolume).
	mt->volume_env.section = 0;
	mt->volume_env.tick_counter = 0;
	mt->panning_env.section = 0;
	mt->panning_env.tick_counter = 0;
}

// [=]===^=[ dbpro_msynth_trigger ]===============================================================[=]
static void dbpro_msynth_trigger(struct dbpro_state *s, struct dbpro_track *mt, int32_t channel) {
	// Reset envelope outputs and interpolator state. Mirrors C# MSynth_Trigger.
	mt->volume_env_current = 16384;
	mt->panning_env_current = 0;
	mt->volume_env.section = 0;
	mt->volume_env.tick_counter = 0;
	if(mt->volume_env.index != DBPRO_ENV_DISABLED && mt->volume_env.index < s->num_volume_envelopes) {
		struct dbpro_envelope *me = &s->volume_envelopes[mt->volume_env.index];
		mt->volume_env.sustain_a = me->sustain_a;
		mt->volume_env.sustain_b = me->sustain_b;
		mt->volume_env.loop_end = me->loop_last;
	}
	mt->panning_env.section = 0;
	mt->panning_env.tick_counter = 0;
	if(mt->panning_env.index != DBPRO_ENV_DISABLED && mt->panning_env.index < s->num_panning_envelopes) {
		struct dbpro_envelope *me = &s->panning_envelopes[mt->panning_env.index];
		mt->panning_env.sustain_a = me->sustain_a;
		mt->panning_env.sustain_b = me->sustain_b;
		mt->panning_env.loop_end = me->loop_last;
	}

	if(mt->trigger_offset > (int32_t)mt->sample_length) {
		mt->trigger_offset = (int32_t)mt->sample_length;
	}
	if(mt->trigger_offset < 0) {
		mt->trigger_offset = 0;
	}
	uint32_t length = mt->sample_length - (uint32_t)mt->trigger_offset;
	if(length > 0 && mt->sample_data) {
		// DBP effect E3 toggles backwards playback per-track. Apply before
		// paula_play_sample so the channel seeds pos_fp at the high end of
		// the buffer.
		paula_set_backwards(&s->paula, channel, mt->play_backwards ? 1 : 0);
		paula_play_sample(&s->paula, channel, mt->sample_data + mt->trigger_offset, length);
		if(mt->sample_loop_length > 0) {
			paula_set_loop(&s->paula, channel, mt->sample_loop_start, mt->sample_loop_length);
		} else {
			paula_set_loop(&s->paula, channel, 0, 0);
		}
	} else {
		paula_mute(&s->paula, channel);
	}
	mt->vibrato_counter = 0;
	mt->is_on = 1;
}

// [=]===^=[ dbpro_msynth_volume_env_interp ]=====================================================[=]
// Per-tick volume envelope interpolator; mirrors MSynth_VolumeEnvelope_Interpolator
// 1:1 (including the post-decrement on tick_counter).
static int16_t dbpro_msynth_volume_env_interp(struct dbpro_env_interp *evi, struct dbpro_envelope *me) {
	if(evi->tick_counter == 0) {
		if(evi->section >= evi->loop_end) {
			evi->section = me->loop_first;
		}
		evi->y_start = (int16_t)(me->points[evi->section & (DBPRO_ENV_MAX_POINTS - 1)].value << 8);
		if(evi->section == evi->sustain_a) {
			return (int16_t)evi->y_start;
		}
		if(evi->section == evi->sustain_b) {
			return (int16_t)evi->y_start;
		}
		if(evi->section >= me->num_sections) {
			return (int16_t)evi->y_start;
		}
		uint32_t s_idx = evi->section & (DBPRO_ENV_MAX_POINTS - 1);
		uint32_t s1_idx = (evi->section + 1u) & (DBPRO_ENV_MAX_POINTS - 1);
		evi->x_delta = (int16_t)(me->points[s1_idx].position - me->points[s_idx].position);
		evi->tick_counter = (uint16_t)evi->x_delta;
		evi->y_delta = ((int32_t)me->points[s1_idx].value << 8) - evi->y_start;
		evi->section = (uint16_t)(evi->section + 1);
	}
	int16_t result;
	if(evi->x_delta != 0) {
		int32_t consumed = (int32_t)evi->x_delta - (int32_t)evi->tick_counter;
		result = (int16_t)(evi->y_start + (evi->y_delta * consumed) / evi->x_delta);
	} else {
		result = (int16_t)evi->y_start;
	}
	evi->tick_counter--;
	evi->previous_value = result;
	return result;
}

// [=]===^=[ dbpro_msynth_panning_env_interp ]====================================================[=]
// Per-tick panning envelope interpolator; mirrors MSynth_PanningEnvelope_Interpolator.
// Returned value is in range -16384..+16384 (centered around zero).
static int16_t dbpro_msynth_panning_env_interp(struct dbpro_env_interp *evi, struct dbpro_envelope *me) {
	if(evi->tick_counter == 0) {
		if(evi->section >= evi->loop_end) {
			evi->section = me->loop_first;
		}
		uint32_t s_idx = evi->section & (DBPRO_ENV_MAX_POINTS - 1);
		evi->y_start = ((int32_t)me->points[s_idx].value + 128) << 7;
		if(evi->section == evi->sustain_a) {
			return (int16_t)(evi->y_start - 16384);
		}
		if(evi->section == evi->sustain_b) {
			return (int16_t)(evi->y_start - 16384);
		}
		if(evi->section >= me->num_sections) {
			return (int16_t)(evi->y_start - 16384);
		}
		uint32_t s1_idx = (evi->section + 1u) & (DBPRO_ENV_MAX_POINTS - 1);
		evi->x_delta = (int16_t)(me->points[s1_idx].position - me->points[s_idx].position);
		evi->tick_counter = (uint16_t)evi->x_delta;
		evi->y_delta = (((int32_t)me->points[s1_idx].value + 128) << 7) - evi->y_start;
		evi->section = (uint16_t)(evi->section + 1);
	}
	int16_t result;
	if(evi->x_delta != 0) {
		int32_t consumed = (int32_t)evi->x_delta - (int32_t)evi->tick_counter;
		result = (int16_t)((evi->y_start + (evi->y_delta * consumed) / evi->x_delta) - 16384);
	} else {
		result = (int16_t)(evi->y_start - 16384);
	}
	evi->tick_counter--;
	evi->previous_value = result;
	return result;
}

// [=]===^=[ dbpro_msynth_do_envelopes ]==========================================================[=]
// Per-tick envelope advance for all active tracks. Mirrors MSynth_Do_Envelopes.
static void dbpro_msynth_do_envelopes(struct dbpro_state *s) {
	for(uint32_t track = 0; track < s->num_tracks; ++track) {
		struct dbpro_track *mt = &s->tracks[track];
		if(mt->is_on && mt->volume_env.index != DBPRO_ENV_DISABLED && mt->volume_env.index < s->num_volume_envelopes) {
			mt->volume_env_current = dbpro_msynth_volume_env_interp(&mt->volume_env, &s->volume_envelopes[mt->volume_env.index]);
		}
		if(mt->is_on && mt->panning_env.index != DBPRO_ENV_DISABLED && mt->panning_env.index < s->num_panning_envelopes) {
			mt->panning_env_current = dbpro_msynth_panning_env_interp(&mt->panning_env, &s->panning_envelopes[mt->panning_env.index]);
		}
	}
}

// [=]===^=[ dbpro_reset_loop ]===================================================================[=]
static void dbpro_reset_loop(struct dbpro_track *mt) {
	mt->loop_counter = 0;
	mt->loop_order = 0;
	mt->loop_row = 0;
}

// [=]===^=[ dbpro_apply_delayed ]================================================================[=]
static void dbpro_apply_delayed(struct dbpro_state *s) {
	struct dbpro_song *song = &s->songs[s->song];
	if(s->delay_pattern_jump != -1) {
		if(s->delay_pattern_jump < song->num_orders) {
			if(s->delay_pattern_jump < s->order) {
				s->delay_module_end = 1;
			}
			s->order = s->delay_pattern_jump;
		} else {
			s->order = 0;
			s->delay_module_end = 1;
		}
		s->pattern = song->play_list[s->order];
		s->row = 0;
	}
	if(s->delay_pattern_break != -1) {
		if(s->delay_pattern_jump == -1 && s->row > 0) {
			s->order++;
			if(s->order >= song->num_orders) {
				s->order = 0;
				s->delay_module_end = 1;
			}
		}
		s->pattern = song->play_list[s->order];
		struct dbpro_pattern *mp = &s->patterns[s->pattern];
		if(s->delay_pattern_break < mp->num_rows) {
			s->row = s->delay_pattern_break;
		} else {
			s->row = mp->num_rows - 1;
		}
	}
	if(s->delay_loop != -1) {
		s->order = s->tracks[s->delay_loop].loop_order;
		s->pattern = song->play_list[s->order];
		s->row = s->tracks[s->delay_loop].loop_row;
	} else {
		if(s->delay_module_end) {
			s->end_reached = 1;
		}
	}
	s->delay_pattern_break = -1;
	s->delay_pattern_jump = -1;
	s->delay_loop = -1;
	s->delay_module_end = 0;
}

// [=]===^=[ dbpro_effect_exx ]===================================================================[=]
static void dbpro_effect_exx(struct dbpro_state *s, struct dbpro_track *mt, int32_t channel, uint8_t parameter) {
	uint8_t op = (uint8_t)(parameter >> 4);
	uint8_t arg = (uint8_t)(parameter & 0x0f);
	switch(op) {
		case DBPRO_EX_FINE_PORTA_UP: {
			if(s->creator_version == 2) {
				mt->pitch_delta = (int16_t)(mt->pitch_delta - arg);
				if(mt->pitch < s->min_pitch) {
					mt->pitch = s->min_pitch;
				}
			} else {
				mt->pitch += (int32_t)arg * s->speed;
				if(mt->pitch > s->max_pitch) {
					mt->pitch = s->max_pitch;
				}
			}
			break;
		}

		case DBPRO_EX_FINE_PORTA_DOWN: {
			if(s->creator_version == 2) {
				mt->pitch_delta = (int16_t)(mt->pitch_delta + arg);
				if(mt->pitch > s->max_pitch) {
					mt->pitch = s->max_pitch;
				}
			} else {
				mt->pitch -= (int32_t)arg * s->speed;
				if(mt->pitch < s->min_pitch) {
					mt->pitch = s->min_pitch;
				}
			}
			break;
		}

		case DBPRO_EX_PLAY_BACKWARDS: {
			if(mt->trigger_counter != 0x7fff) {
				mt->play_backwards = 1;   // honoured by paula_set_backwards on next trigger
			}
			break;
		}

		case DBPRO_EX_CHAN_CTRL_A: {
			if(parameter == 0x40) {
				mt->is_on = 0;
			}
			break;
		}

		case DBPRO_EX_SET_LOOP: {
			if(arg != 0) {
				if(mt->loop_counter == 0) {
					mt->loop_counter = arg;
					s->delay_loop = channel;
				} else {
					mt->loop_counter--;
					if(mt->loop_counter > 0) {
						s->delay_loop = channel;
					} else {
						dbpro_reset_loop(mt);
						s->delay_loop = -1;
					}
				}
			} else {
				if(mt->loop_counter == 0) {
					mt->loop_order = s->order;
					mt->loop_row = s->row;
				}
			}
			break;
		}

		case DBPRO_EX_SET_SMP_OFFSET: {
			mt->trigger_offset += (int32_t)arg << 16;
			break;
		}

		case DBPRO_EX_SET_PANNING: {
			mt->panning = ((int32_t)(arg << 4) - 128) * s->speed;
			break;
		}

		case DBPRO_EX_RETRIG_NOTE: {
			mt->retrigger = arg;
			break;
		}

		case DBPRO_EX_FINE_VOL_UP: {
			mt->volume += (int32_t)arg * s->speed;
			if(mt->volume > s->max_volume) {
				mt->volume = s->max_volume;
			}
			break;
		}

		case DBPRO_EX_FINE_VOL_DOWN: {
			mt->volume -= (int32_t)arg * s->speed;
			if(mt->volume < s->min_volume) {
				mt->volume = s->min_volume;
			}
			break;
		}

		case DBPRO_EX_NOTE_CUT: {
			mt->cut_counter = arg;
			break;
		}

		case DBPRO_EX_NOTE_DELAY: {
			mt->trigger_counter = arg;
			break;
		}

		case DBPRO_EX_PATTERN_DELAY: {
			s->pattern_delay = arg;
			break;
		}
	}
}

// [=]===^=[ dbpro_effect ]=======================================================================[=]
static void dbpro_effect(struct dbpro_state *s, struct dbpro_track *mt, int32_t channel, uint8_t command, uint8_t parameter) {
	switch(command) {
		case DBPRO_FX_ARPEGGIO: {
			if(s->creator_version == 2) {
				mt->arp_table[1] = (int16_t)(parameter >> 4);
				mt->arp_table[2] = (int16_t)(parameter & 0x0f);
			} else {
				mt->arp_table[1] = (int16_t)(((parameter >> 4) << 3) * s->speed);
				mt->arp_table[2] = (int16_t)(((parameter & 0x0f) << 3) * s->speed);
			}
			break;
		}

		case DBPRO_FX_PORTA_UP: {
			if(parameter == 0) {
				parameter = mt->old.portamento_up;
			} else {
				mt->old.portamento_up = parameter;
			}
			if(s->creator_version == 2) {
				if(parameter < 0xf0) {
					mt->pitch_delta = (int16_t)(mt->pitch_delta - (parameter * 4));
				} else {
					mt->pitch_delta = (int16_t)(mt->pitch_delta - (parameter & 0x0f));
				}
			} else {
				if(parameter < 0xf0) {
					mt->pitch_delta = (int16_t)(mt->pitch_delta + (parameter * s->speed));
				} else {
					mt->pitch_delta = (int16_t)(mt->pitch_delta + (parameter & 0x0f));
				}
			}
			break;
		}

		case DBPRO_FX_PORTA_DOWN: {
			if(parameter == 0) {
				parameter = mt->old.portamento_down;
			} else {
				mt->old.portamento_down = parameter;
			}
			if(s->creator_version == 2) {
				if(parameter < 0xf0) {
					mt->pitch_delta = (int16_t)(mt->pitch_delta + (parameter * 4));
				} else {
					mt->pitch_delta = (int16_t)(mt->pitch_delta + (parameter & 0x0f));
				}
			} else {
				if(parameter < 0xf0) {
					mt->pitch_delta = (int16_t)(mt->pitch_delta - (parameter * s->speed));
				} else {
					mt->pitch_delta = (int16_t)(mt->pitch_delta - (parameter & 0x0f));
				}
			}
			break;
		}

		case DBPRO_FX_PORTA_TO_NOTE: {
			if(parameter == 0) {
				parameter = mt->old.portamento_speed;
			} else {
				mt->old.portamento_speed = parameter;
			}
			int32_t porta_target;
			if(s->creator_version == 2) {
				porta_target = mt->porta3_target * 4;
				if(porta_target >= mt->pitch) {
					mt->porta3_delta = (int16_t)(mt->porta3_delta + parameter * 4);
				} else {
					mt->porta3_delta = (int16_t)(mt->porta3_delta - parameter * 4);
				}
			} else {
				porta_target = mt->porta3_target * s->speed;
				if(porta_target >= mt->pitch) {
					mt->porta3_delta = (int16_t)(mt->porta3_delta + parameter * s->speed);
				} else {
					mt->porta3_delta = (int16_t)(mt->porta3_delta - parameter * s->speed);
				}
			}
			break;
		}

		case DBPRO_FX_VIBRATO: {
			if(s->creator_version == 2) {
				if((parameter & 0xf0) == 0) {
					parameter = (uint8_t)((parameter & 0x0f) | (mt->old.vibrato & 0xf0));
				}
				if((parameter & 0x0f) == 0) {
					parameter = (uint8_t)((parameter & 0xf0) | (mt->old.vibrato & 0x0f));
				}
				mt->old.vibrato = parameter;
			} else {
				if(parameter == 0) {
					parameter = mt->old.vibrato;
				} else {
					mt->old.vibrato = parameter;
				}
			}
			mt->vibrato_speed = (int16_t)(parameter >> 4);
			mt->vibrato_depth = (int16_t)(parameter & 0x0f);
			if(s->creator_version == 3) {
				mt->vibrato_depth = (int16_t)(mt->vibrato_depth * s->speed);
			}
			break;
		}

		case DBPRO_FX_PORTA_VS: {
			if(parameter == 0) {
				parameter = mt->old.volume_slide_5;
			} else {
				mt->old.volume_slide_5 = parameter;
			}
			int16_t porta_speed = mt->old.portamento_speed;
			if(s->creator_version == 2) {
				if(mt->porta3_target >= mt->pitch) {
					mt->porta3_delta = (int16_t)(mt->porta3_delta + porta_speed * 4);
				} else {
					mt->porta3_delta = (int16_t)(mt->porta3_delta - porta_speed * 4);
				}
			} else {
				int32_t porta_target = mt->porta3_target * s->speed;
				if(porta_target >= mt->pitch) {
					mt->porta3_delta = (int16_t)(mt->porta3_delta + porta_speed * s->speed);
				} else {
					mt->porta3_delta = (int16_t)(mt->porta3_delta - porta_speed * s->speed);
				}
			}
			int16_t p0 = (int16_t)(parameter >> 4);
			int16_t p1 = (int16_t)(parameter & 0x0f);
			if(p0 == 0 || p1 == 0) {
				if(p0 != 0) {
					mt->volume_delta = (int16_t)(mt->volume_delta + p0 * s->speed);
				}
				if(p1 != 0) {
					mt->volume_delta = (int16_t)(mt->volume_delta - p1 * s->speed);
				}
			} else {
				if(p1 == 0x0f) {
					mt->volume_delta = (int16_t)(mt->volume_delta + p0);
				} else if(p0 == 0x0f) {
					mt->volume_delta = (int16_t)(mt->volume_delta - p1);
				}
			}
			break;
		}

		case DBPRO_FX_VIBRATO_VS: {
			if(parameter == 0) {
				parameter = mt->old.vibrato_6;
			} else {
				mt->old.vibrato_6 = parameter;
			}
			mt->vibrato_speed = (int16_t)(mt->old.vibrato >> 4);
			mt->vibrato_depth = (int16_t)(mt->old.vibrato & 0x0f);
			if(s->creator_version == 3) {
				mt->vibrato_depth = (int16_t)(mt->vibrato_depth * s->speed);
			}
			int16_t p0 = (int16_t)(parameter >> 4);
			int16_t p1 = (int16_t)(parameter & 0x0f);
			if(p0 == 0 || p1 == 0) {
				if(p0 != 0) {
					mt->volume_delta = (int16_t)(mt->volume_delta + p0 * s->speed);
				}
				if(p1 != 0) {
					mt->volume_delta = (int16_t)(mt->volume_delta - p1 * s->speed);
				}
			} else {
				if(p1 == 0x0f) {
					mt->volume_delta = (int16_t)(mt->volume_delta + p0);
				} else if(p0 == 0x0f) {
					mt->volume_delta = (int16_t)(mt->volume_delta - p1);
				}
			}
			break;
		}

		case DBPRO_FX_SET_PANNING: {
			mt->panning = ((int32_t)parameter - 128) * s->speed;
			break;
		}

		case DBPRO_FX_SAMPLE_OFFSET: {
			mt->trigger_offset += (int32_t)parameter << 8;
			break;
		}

		case DBPRO_FX_VOLUME_SLIDE: {
			if(parameter == 0) {
				parameter = mt->old.volume_slide;
			} else {
				mt->old.volume_slide = parameter;
			}
			int16_t p0 = (int16_t)(parameter >> 4);
			int16_t p1 = (int16_t)(parameter & 0x0f);
			if(p0 == 0 || p1 == 0) {
				if(p0 != 0) {
					mt->volume_delta = (int16_t)(mt->volume_delta + p0 * s->speed);
				}
				if(p1 != 0) {
					mt->volume_delta = (int16_t)(mt->volume_delta - p1 * s->speed);
				}
			} else {
				if(p1 == 0x0f) {
					mt->volume_delta = (int16_t)(mt->volume_delta + p0);
				} else if(p0 == 0x0f) {
					mt->volume_delta = (int16_t)(mt->volume_delta - p1);
				}
			}
			break;
		}

		case DBPRO_FX_POSITION_JUMP: {
			s->delay_pattern_jump = parameter;
			break;
		}

		case DBPRO_FX_SET_VOLUME: {
			if(parameter <= 0x40) {
				mt->volume = (int32_t)parameter * s->speed;
			}
			break;
		}

		case DBPRO_FX_PATTERN_BREAK: {
			s->delay_pattern_break = dbpro_bcd2bin(parameter);
			break;
		}

		case DBPRO_FX_EXTRA: {
			dbpro_effect_exx(s, mt, channel, parameter);
			break;
		}

		case DBPRO_FX_GLOBAL_VOL: {
			s->global_volume = parameter;
			if(s->global_volume > 0x40) {
				s->global_volume = 0x40;
			}
			break;
		}

		case DBPRO_FX_GLOBAL_VOL_SL: {
			if(parameter == 0) {
				parameter = s->old_global_volume_slide;
			} else {
				s->old_global_volume_slide = parameter;
			}
			int16_t p0 = (int16_t)(parameter >> 4);
			int16_t p1 = (int16_t)(parameter & 0x0f);
			if(p0 == 0 || p1 == 0) {
				if(p0 != 0) {
					s->global_volume_slide = (int16_t)(s->global_volume_slide + p0);
				}
				if(p1 != 0) {
					s->global_volume_slide = (int16_t)(s->global_volume_slide - p1);
				}
			}
			break;
		}

		case DBPRO_FX_PANNING_SLIDE: {
			if(parameter == 0) {
				parameter = mt->old.panning_slide;
			} else {
				mt->old.panning_slide = parameter;
			}
			int16_t p0 = (int16_t)(parameter >> 4);
			int16_t p1 = (int16_t)(parameter & 0x0f);
			if(p0 == 0 || p1 == 0) {
				if(p0 != 0) {
					mt->panning_delta = (int16_t)(mt->panning_delta + p0 * s->speed);
				}
				if(p1 != 0) {
					mt->panning_delta = (int16_t)(mt->panning_delta - p1 * s->speed);
				}
			}
			break;
		}

		// V/W/X/Y/Z (echo): intentionally ignored. Echo DSP is excluded.
	}
}

// [=]===^=[ dbpro_change_speed ]=================================================================[=]
static void dbpro_change_speed(struct dbpro_state *s, int32_t speed) {
	if(speed < 1) {
		speed = 1;
	}
	if(speed > 31) {
		speed = 31;
	}
	s->speed = speed;
}

// [=]===^=[ dbpro_change_tempo ]=================================================================[=]
static void dbpro_change_tempo(struct dbpro_state *s, int32_t tempo) {
	s->tempo = tempo;
	dbpro_set_bpm_tempo(s, (uint32_t)tempo);
}

// [=]===^=[ dbpro_scan_for_speed ]===============================================================[=]
static void dbpro_scan_for_speed(struct dbpro_state *s) {
	struct dbpro_pattern *mp = &s->patterns[s->pattern];
	uint32_t row_off = (uint32_t)s->row * s->num_tracks;
	for(uint32_t track = 0; track < s->num_tracks; ++track) {
		struct dbpro_entry *me = &mp->entries[row_off + track];
		if(me->command1 == DBPRO_FX_SET_TEMPO) {
			if(me->parameter1 == 0) {
				s->pattern_delay = 0x7fffffff;
			} else if(me->parameter1 < 0x20) {
				dbpro_change_speed(s, me->parameter1);
			} else {
				dbpro_change_tempo(s, me->parameter1);
			}
		}
		if(me->command2 == DBPRO_FX_SET_TEMPO) {
			if(me->parameter2 == 0) {
				s->pattern_delay = 0x7fffffff;
			} else if(me->parameter2 < 0x20) {
				dbpro_change_speed(s, me->parameter2);
			} else {
				dbpro_change_tempo(s, me->parameter2);
			}
		}
	}
}

// [=]===^=[ dbpro_next_pattern ]=================================================================[=]
static void dbpro_next_pattern(struct dbpro_state *s) {
	struct dbpro_song *song = &s->songs[s->song];
	s->order++;
	if(s->order >= song->num_orders) {
		s->order = 0;
		s->delay_module_end = 1;
	}
	s->pattern = song->play_list[s->order];
	s->row = 0;
}

// [=]===^=[ dbpro_next_row ]=====================================================================[=]
static void dbpro_next_row(struct dbpro_state *s) {
	struct dbpro_pattern *mp = &s->patterns[s->pattern];
	uint32_t row_off = (uint32_t)s->row * s->num_tracks;
	for(uint32_t track = 0; track < s->num_tracks; ++track) {
		struct dbpro_entry *me = &mp->entries[row_off + track];
		struct dbpro_track *mt = &s->tracks[track];
		mt->trigger_counter = 0x7fffffff;
		mt->cut_counter = 0x7fffffff;
		mt->retrigger = 0;
		mt->play_backwards = 0;

		if(me->instrument != 0 && me->instrument != mt->instrument) {
			dbpro_msynth_instrument(s, mt, me->instrument);
		}

		if(me->octave != 0) {
			if(me->note < 12) {
				if(s->creator_version == 2) {
					int32_t note = (me->octave - 1) * 12 + me->note;
					if(note < 0) {
						note = 0;
					}
					if(note >= 96) {
						note = 95;
					}
					int16_t period = dbpro_periods[note];
					if(!dbpro_porta_to_note_p(me)) {
						mt->note = note;
						mt->pitch = period;
						mt->trigger_counter = 0;
					} else {
						mt->porta3_target = period;
					}
				} else {
					int16_t ft_note = (int16_t)(((me->octave << 3) + (me->octave << 2) + me->note) << 3);
					if(!dbpro_porta_to_note_p(me)) {
						mt->pitch = ft_note;
						mt->pitch *= s->speed;
						mt->trigger_counter = 0;
					} else {
						mt->porta3_target = ft_note;
					}
				}
			} else {
				// Key off. Allow each envelope to release toward its final
				// point. C# Player.cs:1502-1531: if both envelopes are absent,
				// hard channel-off; otherwise per envelope, either clear
				// LoopEnd (letting it run past the loop) or clear one of
				// SustainA/SustainB (whichever is smaller; SustainA on tie).
				if(mt->volume_env.index == DBPRO_ENV_DISABLED && mt->panning_env.index == DBPRO_ENV_DISABLED) {
					mt->is_on = 0;
				}
				if(mt->volume_env.index != DBPRO_ENV_DISABLED) {
					if(mt->volume_env.loop_end <= mt->volume_env.sustain_a && mt->volume_env.loop_end <= mt->volume_env.sustain_b) {
						mt->volume_env.loop_end = DBPRO_ENV_LOOP_DISABLED;
					} else if(mt->volume_env.sustain_a <= mt->volume_env.sustain_b) {
						mt->volume_env.sustain_a = DBPRO_ENV_SUSTAIN_DISABLED;
					} else {
						mt->volume_env.sustain_b = DBPRO_ENV_SUSTAIN_DISABLED;
					}
				}
				if(mt->panning_env.index != DBPRO_ENV_DISABLED) {
					if(mt->panning_env.loop_end <= mt->panning_env.sustain_a && mt->panning_env.loop_end <= mt->panning_env.sustain_b) {
						mt->panning_env.loop_end = DBPRO_ENV_LOOP_DISABLED;
					} else if(mt->panning_env.sustain_a <= mt->panning_env.sustain_b) {
						mt->panning_env.sustain_a = DBPRO_ENV_SUSTAIN_DISABLED;
					} else {
						mt->panning_env.sustain_b = DBPRO_ENV_SUSTAIN_DISABLED;
					}
				}
			}
		}

		if(me->instrument != 0) {
			dbpro_msynth_default_volume(s, mt);
			mt->trigger_offset = 0;
		}

		if(me->command1 != DBPRO_FX_ARPEGGIO || me->parameter1 != 0) {
			dbpro_effect(s, mt, (int32_t)track, me->command1, me->parameter1);
		}
		if(me->command2 != DBPRO_FX_ARPEGGIO || me->parameter2 != 0) {
			dbpro_effect(s, mt, (int32_t)track, me->command2, me->parameter2);
		}
	}
	s->row++;
	if(s->row >= mp->num_rows) {
		dbpro_next_pattern(s);
	}
}

// [=]===^=[ dbpro_post_tick ]====================================================================[=]
static void dbpro_post_tick(struct dbpro_state *s) {
	for(uint32_t track = 0; track < s->num_tracks; ++track) {
		struct dbpro_track *mt = &s->tracks[track];
		mt->volume += mt->volume_delta;
		if(mt->volume > s->max_volume) {
			mt->volume = s->max_volume;
		}
		if(mt->volume < s->min_volume) {
			mt->volume = s->min_volume;
		}
		mt->panning += mt->panning_delta;
		if(mt->panning > s->max_panning) {
			mt->panning = s->max_panning;
		}
		if(mt->panning < s->min_panning) {
			mt->panning = s->min_panning;
		}
		if(mt->porta3_delta != 0) {
			int32_t porta_target = mt->porta3_target;
			if(s->creator_version == 3) {
				porta_target *= s->speed;
			}
			mt->pitch += mt->porta3_delta;
			if(mt->porta3_delta > 0) {
				if(mt->pitch > porta_target) {
					mt->pitch = porta_target;
				}
			} else {
				if(mt->pitch < porta_target) {
					mt->pitch = porta_target;
				}
			}
		}
		mt->pitch += mt->pitch_delta;
		if(mt->pitch > s->max_pitch) {
			mt->pitch = s->max_pitch;
		}
		if(mt->pitch < s->min_pitch) {
			mt->pitch = s->min_pitch;
		}
	}
	s->global_volume += s->global_volume_slide;
	if(s->global_volume > 64) {
		s->global_volume = 64;
	}
	if(s->global_volume < 0) {
		s->global_volume = 0;
	}
}

// [=]===^=[ dbpro_do_triggers ]==================================================================[=]
static void dbpro_do_triggers(struct dbpro_state *s) {
	for(uint32_t track = 0; track < s->num_tracks; ++track) {
		struct dbpro_track *mt = &s->tracks[track];
		if(mt->instrument != 0) {
			if(mt->trigger_counter == 0) {
				dbpro_msynth_trigger(s, mt, (int32_t)track);
				mt->trigger_counter = mt->retrigger;
			}
			mt->trigger_counter--;
			if(mt->cut_counter-- <= 0) {
				mt->volume = 0;
			}
		}
	}
}

// [=]===^=[ dbpro_clear_slides ]=================================================================[=]
static void dbpro_clear_slides(struct dbpro_state *s) {
	if(s->speed < 1) {
		s->speed = 1;
	}
	for(uint32_t track = 0; track < s->num_tracks; ++track) {
		struct dbpro_track *mt = &s->tracks[track];
		mt->volume_delta = 0;
		mt->panning_delta = 0;
		mt->pitch_delta = 0;
		mt->porta3_delta = 0;
		mt->volume /= s->speed;
		mt->panning /= s->speed;
		if(s->creator_version == 3) {
			mt->pitch /= s->speed;
		}
		mt->arp_table[1] = 0;
		mt->arp_table[2] = 0;
		mt->vibrato_speed = 0;
		mt->vibrato_depth = 0;
	}
	s->global_volume_slide = 0;
	s->arp_counter = 0;
}

// [=]===^=[ dbpro_setup_slides ]=================================================================[=]
static void dbpro_setup_slides(struct dbpro_state *s) {
	for(uint32_t track = 0; track < s->num_tracks; ++track) {
		struct dbpro_track *mt = &s->tracks[track];
		mt->volume *= s->speed;
		mt->panning *= s->speed;
		if(s->creator_version == 3) {
			mt->pitch *= s->speed;
		}
	}
	s->min_volume = 0;
	s->min_panning = (int16_t)(-(s->speed << 7));
	s->max_volume = (int16_t)(s->speed << 6);
	s->max_panning = (int16_t)(s->speed << 7);
	if(s->creator_version == 2) {
		s->min_pitch = 57;
		s->max_pitch = 13696;
	} else {
		s->min_pitch = (int16_t)(s->speed * 96);
		s->max_pitch = (int16_t)(s->speed * 864);
	}
}

// [=]===^=[ dbpro_tick_gains_and_pitch ]=========================================================[=]
static void dbpro_tick_gains_and_pitch(struct dbpro_state *s) {
	if(s->speed < 1) {
		s->speed = 1;
	}
	for(uint32_t track = 0; track < s->num_tracks; ++track) {
		struct dbpro_track *mt = &s->tracks[track];
		dbpro_msynth_pitch(s, mt, (int32_t)track);
		// volc <0, +64*speed> -> 0..16384, then envelope, then global volume scale 0..64.
		int32_t volc = (mt->volume << 8) / s->speed;          // 0..16384
		int32_t pan = (mt->panning << 7) / s->speed;          // -16384..+16384
#ifndef DBPRO_NO_ENVELOPES
		// Volume envelope is multiplied with the gain and renormalized
		// (gain 0..16384, env 0..16384). C# Player.cs:1837-1838.
		volc = (volc * mt->volume_env_current) >> 14;
#endif
		volc *= s->global_volume;
		volc >>= 6;                                            // 0..16384
#ifndef DBPRO_NO_ENVELOPES
		// Panning envelope: p = p0 + (1 - |p0|) * ev. C# Player.cs:1842-1845.
		{
			int32_t p_abs = pan >= 0 ? pan : -pan;
			int32_t p1 = 16384 - p_abs;
			int32_t p2 = (p1 * mt->panning_env_current) >> 14;
			pan += p2;
		}
#endif
		// Map 0..16384 -> 0..64 paula scale (>>8).
		int32_t paula_vol = volc >> 8;
		if(paula_vol > 64) {
			paula_vol = 64;
		}
		if(paula_vol < 0) {
			paula_vol = 0;
		}
		paula_set_volume(&s->paula, (int32_t)track, (uint16_t)paula_vol);
		// Map pan -16384..+16384 to 0..127.
		int32_t pan_127 = (pan + 16384) >> 8;                  // 0..128
		if(pan_127 > 127) {
			pan_127 = 127;
		}
		if(pan_127 < 0) {
			pan_127 = 0;
		}
		s->paula.ch[track & 7].pan = (uint8_t)pan_127;
		if(!mt->is_on) {
			paula_mute(&s->paula, (int32_t)track);
		}
	}
}

// [=]===^=[ dbpro_next_tick ]====================================================================[=]
static void dbpro_next_tick(struct dbpro_state *s) {
	dbpro_post_tick(s);
	if(s->tick == 0) {
		if(s->pattern_delay > 0) {
			if(s->pattern_delay-- > 15) {
				s->end_reached = 1;
				s->restart_song = 1;
			}
		} else {
			dbpro_apply_delayed(s);
			dbpro_clear_slides(s);
			dbpro_scan_for_speed(s);
			dbpro_setup_slides(s);
			dbpro_next_row(s);
		}
	}
	dbpro_do_triggers(s);
#ifndef DBPRO_NO_ENVELOPES
	dbpro_msynth_do_envelopes(s);
#endif
	dbpro_tick_gains_and_pitch(s);
	s->arp_counter++;
	if(s->arp_counter > 2) {
		s->arp_counter = 0;
	}
	s->tick++;
	if(s->tick >= s->speed) {
		s->tick = 0;
	}
}

// [=]===^=[ dbpro_handle_end ]===================================================================[=]
static void dbpro_handle_end(struct dbpro_state *s) {
	if(!s->end_reached) {
		return;
	}
	s->end_reached = 0;
	if(s->restart_song) {
		s->restart_song = 0;
		dbpro_reset(s);
	} else {
		if(s->global_volume == 0) {
			for(uint32_t track = 0; track < s->num_tracks; ++track) {
				paula_mute(&s->paula, (int32_t)track);
			}
		}
		s->global_volume = 64;
	}
}

// [=]===^=[ dbpro_tick ]=========================================================================[=]
static void dbpro_tick(struct dbpro_state *s) {
	dbpro_next_tick(s);
	for(uint32_t track = 0; track < s->num_tracks; ++track) {
		if(!s->tracks[track].is_on) {
			paula_mute(&s->paula, (int32_t)track);
		}
	}
	dbpro_handle_end(s);
}

// [=]===^=[ digiboosterpro_init ]================================================================[=]
static struct dbpro_state *digiboosterpro_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 16 || sample_rate < 8000) {
		return 0;
	}
	struct dbpro_state *s = (struct dbpro_state *)calloc(1, sizeof(struct dbpro_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;
	if(!dbpro_load(s)) {
		dbpro_free_module(s);
		free(s);
		return 0;
	}
	// DBM uses BPM-driven ticks, so use a placeholder tick rate; tempo set
	// directly afterwards.
	paula_init(&s->paula, sample_rate, 50);
	dbpro_reset(s);
	return s;
}

// [=]===^=[ digiboosterpro_free ]================================================================[=]
static void digiboosterpro_free(struct dbpro_state *s) {
	if(!s) {
		return;
	}
	dbpro_free_module(s);
	free(s);
}

// [=]===^=[ digiboosterpro_get_audio ]===========================================================[=]
static void digiboosterpro_get_audio(struct dbpro_state *s, int16_t *output, int32_t frames) {
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
			dbpro_tick(s);
		}
	}
}

// [=]===^=[ digiboosterpro_api_init ]============================================================[=]
static void *digiboosterpro_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return digiboosterpro_init(data, len, sample_rate);
}

// [=]===^=[ digiboosterpro_api_free ]============================================================[=]
static void digiboosterpro_api_free(void *state) {
	digiboosterpro_free((struct dbpro_state *)state);
}

// [=]===^=[ digiboosterpro_api_get_audio ]=======================================================[=]
static void digiboosterpro_api_get_audio(void *state, int16_t *output, int32_t frames) {
	digiboosterpro_get_audio((struct dbpro_state *)state, output, frames);
}

static const char *digiboosterpro_extensions[] = { "dbm", 0 };

static struct player_api digiboosterpro_api = {
	"DigiBooster Pro 2.x/3.x",
	digiboosterpro_extensions,
	digiboosterpro_api_init,
	digiboosterpro_api_free,
	digiboosterpro_api_get_audio,
	0,
};
