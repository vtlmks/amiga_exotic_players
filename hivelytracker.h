// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// HivelyTracker / AHX replayer, ported from NostalgicPlayer's C# implementation.
//
// HivelyTracker (HVL) and AHX (THX) use generated waveforms (saw / triangle /
// square / white noise) plus pre-baked low-pass and high-pass filter banks, an
// internal ring modulator that multiplies one voice waveform by another, and
// per-voice stereo panning. paula.h cannot do any of that, so this port
// includes its own internal mixer.
//
// Notes / known limitations:
//  - VisualizerChannel, ISnapshot, OnModuleInfoChanged, position-visit tracking
//    are intentionally dropped. End detection falls back to a position counter
//    that resets to song.Restart on wrap.
//  - Stereo separation is fixed at 100 (i.e. no narrowing applied).
//  - Module info / sample info APIs are dropped; the player only renders audio.
//
// Public API:
//   struct hivelytracker_state *hivelytracker_init(void *data, uint32_t len, int32_t sample_rate);
//   void hivelytracker_free(struct hivelytracker_state *s);
//   void hivelytracker_get_audio(struct hivelytracker_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "player_api.h"

#define HIVELY_MAX_CHANNELS 16
#define HIVELY_VOICE_BUFLEN 0x281
#define HIVELY_RING_BUFLEN  (0x282 * 4)
#define HIVELY_FILTER_SETS  63
#define HIVELY_WAVE_LEN     (0x04 + 0x08 + 0x10 + 0x20 + 0x40 + 0x80)
#define HIVELY_SQUARE_LEN   (0x80 * 0x20)
#define HIVELY_WN_LEN       (0x280 * 3)

enum {
	HIVELY_TYPE_UNKNOWN = 0,
	HIVELY_TYPE_AHX1    = 1,
	HIVELY_TYPE_AHX2    = 2,
	HIVELY_TYPE_HVL     = 3,
};

struct hively_envelope {
	int32_t a_frames;
	int32_t a_volume;
	int32_t d_frames;
	int32_t d_volume;
	int32_t s_frames;
	int32_t r_frames;
	int32_t r_volume;
};

struct hively_plist_entry {
	int32_t note;
	int32_t fixed_note;
	int32_t waveform;
	int32_t fx[2];
	int32_t fx_param[2];
};

struct hively_plist {
	int32_t speed;
	int32_t length;
	struct hively_plist_entry *entries;
};

struct hively_instrument {
	int32_t volume;
	int32_t wave_length;
	struct hively_envelope envelope;

	int32_t filter_lower_limit;
	int32_t filter_upper_limit;
	int32_t filter_speed;

	int32_t square_lower_limit;
	int32_t square_upper_limit;
	int32_t square_speed;

	int32_t vibrato_delay;
	int32_t vibrato_depth;
	int32_t vibrato_speed;

	int32_t hard_cut_release_frames;
	int32_t hard_cut_release;

	struct hively_plist play_list;
};

struct hively_step {
	int32_t note;
	int32_t instrument;
	int32_t fx;
	int32_t fx_param;
	int32_t fx_b;
	int32_t fx_b_param;
};

struct hively_position {
	int32_t track[HIVELY_MAX_CHANNELS];
	int32_t transpose[HIVELY_MAX_CHANNELS];
};

struct hively_song {
	int32_t restart;
	int32_t position_nr;
	int32_t track_length;
	int32_t track_nr;
	int32_t instrument_nr;
	int32_t speed_multiplier;
	int32_t channels;
	int32_t mix_gain;
	int32_t default_stereo;
	int32_t default_panning_left;
	int32_t default_panning_right;

	struct hively_position *positions;
	struct hively_step **tracks;     // tracks[track_idx][step_idx]
	struct hively_instrument *instruments;
};

struct hively_voice {
	int32_t voice_volume;
	int32_t voice_period;
	int8_t voice_buffer[HIVELY_VOICE_BUFLEN];

	int32_t track;
	int32_t transpose;
	int32_t next_track;
	int32_t next_transpose;
	int32_t override_transpose;

	int32_t adsr_volume;
	struct hively_envelope adsr;

	struct hively_instrument *instrument;
	int32_t instrument_number;
	int32_t sample_pos;
	int32_t delta;

	int32_t instr_period;
	int32_t track_period;
	int32_t vibrato_period;

	int32_t note_max_volume;
	int32_t perf_sub_volume;
	int32_t track_master_volume;

	int32_t new_waveform;
	int32_t waveform;
	int32_t plant_square;
	int32_t plant_period;
	int32_t kick_note;
	int32_t ignore_square;

	int32_t track_on;
	int32_t fixed_note;

	int32_t volume_slide_up;
	int32_t volume_slide_down;

	int32_t hard_cut;
	int32_t hard_cut_release;
	int32_t hard_cut_release_f;

	int32_t period_slide_speed;
	int32_t period_slide_period;
	int32_t period_slide_limit;
	int32_t period_slide_on;
	int32_t period_slide_with_limit;

	int32_t period_perf_slide_speed;
	int32_t period_perf_slide_period;
	int32_t period_perf_slide_on;

	int32_t vibrato_delay;
	int32_t vibrato_current;
	int32_t vibrato_depth;
	int32_t vibrato_speed;

	int32_t square_on;
	int32_t square_init;
	int32_t square_wait;
	int32_t square_lower_limit;
	int32_t square_upper_limit;
	int32_t square_pos;
	int32_t square_sign;
	int32_t square_sliding_in;
	int32_t square_reverse;

	int32_t filter_on;
	int32_t filter_init;
	int32_t filter_wait;
	int32_t filter_lower_limit;
	int32_t filter_upper_limit;
	int32_t filter_pos;
	int32_t filter_sign;
	int32_t filter_speed;
	int32_t filter_sliding_in;
	int32_t ignore_filter;

	int32_t perf_current;
	int32_t perf_speed;
	int32_t perf_wait;

	int32_t wave_length;

	struct hively_plist *perf_list;

	int32_t note_delay_wait;
	int32_t note_delay_on;
	int32_t note_cut_wait;
	int32_t note_cut_on;

	int32_t pan;
	int32_t set_pan;

	int32_t ring_sample_pos;
	int32_t ring_delta;
	int8_t *ring_mix_source;
	int32_t ring_plant_period;
	int32_t ring_base_period;
	int32_t ring_audio_period;
	int32_t ring_new_waveform;
	int32_t ring_waveform;
	int32_t ring_fixed_period;

	int8_t *audio_source;
	int32_t audio_offset;

	int8_t *ring_audio_source;
	int32_t ring_audio_offset;

	int32_t audio_period;
	int32_t audio_volume;

	uint32_t wn_random;
	int8_t *mix_source;

	int8_t square_temp_buffer[0x80];
	int8_t ring_voice_buffer[HIVELY_RING_BUFLEN];
};

struct hively_waves {
	int8_t sawtooths[HIVELY_FILTER_SETS][HIVELY_WAVE_LEN];
	int8_t triangles[HIVELY_FILTER_SETS][HIVELY_WAVE_LEN];
	int8_t squares  [HIVELY_FILTER_SETS][HIVELY_SQUARE_LEN];
	int8_t white_noise[HIVELY_FILTER_SETS][HIVELY_WN_LEN];
	int32_t panning_left[256];
	int32_t panning_right[256];
};

struct hivelytracker_state {
	int32_t module_type;
	int32_t sample_rate;

	struct hively_song song;
	struct hively_waves *waves;

	int32_t pos_nr;
	int32_t pos_jump;
	int32_t pattern_break;
	int32_t note_nr;
	int32_t pos_jump_note;
	int32_t tempo;
	int32_t step_wait_frames;
	int32_t get_new_position;
	int8_t *square_waveform;

	struct hively_voice voices[HIVELY_MAX_CHANNELS];

	int32_t tick_samples;
	int32_t tick_offset;
};

// [=]===^=[ hively_period_table ]================================================================[=]
static int32_t hively_period_table[] = {
	0x0000, 0x0d60, 0x0ca0, 0x0be8, 0x0b40, 0x0a98, 0x0a00, 0x0970,
	0x08e8, 0x0868, 0x07f0, 0x0780, 0x0714, 0x06b0, 0x0650, 0x05f4,
	0x05a0, 0x054c, 0x0500, 0x04b8, 0x0474, 0x0434, 0x03f8, 0x03c0,
	0x038a, 0x0358, 0x0328, 0x02fa, 0x02d0, 0x02a6, 0x0280, 0x025c,
	0x023a, 0x021a, 0x01fc, 0x01e0, 0x01c5, 0x01ac, 0x0194, 0x017d,
	0x0168, 0x0153, 0x0140, 0x012e, 0x011d, 0x010d, 0x00fe, 0x00f0,
	0x00e2, 0x00d6, 0x00ca, 0x00be, 0x00b4, 0x00aa, 0x00a0, 0x0097,
	0x008f, 0x0087, 0x007f, 0x0078, 0x0071
};

// [=]===^=[ hively_vibrato_table ]===============================================================[=]
static int32_t hively_vibrato_table[] = {
	   0,   24,   49,   74,   97,  120,  141,  161,  180,  197,  212,  224,  235,  244,  250,  253,  255,
	 253,  250,  244,  235,  224,  212,  197,  180,  161,  141,  120,   97,   74,   49,   24,
	   0,  -24,  -49,  -74,  -97, -120, -141, -161, -180, -197, -212, -224, -235, -244, -250, -253, -255,
	-253, -250, -244, -235, -224, -212, -197, -180, -161, -141, -120,  -97,  -74,  -49,  -24
};

// [=]===^=[ hively_offset_table ]================================================================[=]
static int32_t hively_offset_table[] = {
	0x00, 0x04, 0x04 + 0x08, 0x04 + 0x08 + 0x10, 0x04 + 0x08 + 0x10 + 0x20, 0x04 + 0x08 + 0x10 + 0x20 + 0x40
};

// [=]===^=[ hively_stereo_pan_left ]=============================================================[=]
static int32_t hively_stereo_pan_left[]  = { 128, 96, 64, 32, 0 };

// [=]===^=[ hively_stereo_pan_right ]============================================================[=]
static int32_t hively_stereo_pan_right[] = { 128, 160, 193, 225, 255 };

// Filter tables - large prebuilt coefficients used by the filter generator.
// Sizes: MidFilterTable and LowFilterTable are 31 * (sum of all wave widths) entries.

// [=]===^=[ hively_mid_filter_table ]============================================================[=]
static int16_t hively_mid_filter_table[] = {
	-1161, -4413, -7161, -13094, 635, 13255, 2189, 6401,
	9041, 16130, 13460, 5360, 6349, 12699, 19049, 25398,
	30464, 32512, 32512, 32515, 31625, 29756, 27158, 24060,
	20667, 17156, 13970, 11375, 9263, 7543, 6142, 5002,
	4074, 3318, 2702, 2178, 1755, 1415, 1141, 909,
	716, 563, 444, 331, -665, -2082, -6170, -9235,
	-13622, 12545, 9617, 3951, 8345, 11246, 18486, 6917,
	3848, 8635, 17271, 25907, 32163, 32512, 32455, 30734,
	27424, 23137, 18397, 13869, 10429, 7843, 5897, 4435,
	3335, 2507, 1885, 1389, 1023, 720, 530, 353,
	260, 173, 96, 32, -18, -55, -79, -92,
	-95, -838, -3229, -7298, -12386, -7107, 13946, 6501,
	5970, 9133, 14947, 16881, 6081, 3048, 10921, 21843,
	31371, 32512, 32068, 28864, 23686, 17672, 12233, 8469,
	5862, 4058, 2809, 1944, 1346, 900, 601, 371,
	223, 137, 64, 7, -34, -58, -69, -70,
	-63, -52, -39, -26, -14, -5, 4984, -4476,
	-8102, -14892, 2894, 12723, 4883, 8010, 9750, 17887,
	11790, 5099, 2520, 13207, 26415, 32512, 32457, 28690,
	22093, 14665, 9312, 5913, 3754, 2384, 1513, 911,
	548, 330, 143, 3, -86, -130, -139, -125,
	-97, -65, -35, -11, 6, 15, 19, 19,
	16, 12, 8, 6877, -5755, -9129, -15709, 9705,
	10893, 4157, 9882, 10897, 19236, 8153, 4285, 2149,
	15493, 30618, 32512, 30220, 22942, 14203, 8241, 4781,
	2774, 1609, 933, 501, 220, 81, 35, 2,
	-18, -26, -25, -20, -13, -7, -1, 2,
	4, 4, 3, 2, 1, 0, 0, -1,
	2431, -6956, -10698, -14594, 12720, 8980, 3714, 10892,
	12622, 19554, 6915, 3745, 1872, 17779, 32512, 32622,
	26286, 16302, 8605, 4542, 2397, 1265, 599, 283,
	45, -92, -141, -131, -93, -49, -14, 8,
	18, 18, 14, 8, 3, 0, -2, -3,
	-2, -2, -1, 0, 0, -3654, -8008, -12743,
	-11088, 13625, 7342, 3330, 11330, 14859, 18769, 6484,
	3319, 1660, 20065, 32512, 30699, 21108, 10616, 5075,
	2425, 1159, 477, 196, 1, -93, -109, -82,
	-44, -12, 7, 14, 13, 9, 4, 0,
	-2, -2, -1, -1, 0, 0, 0, 0,
	0, 0, -7765, -8867, -14957, -5862, 13550, 6139,
	2988, 11284, 17054, 16602, 6017, 2979, 1489, 22351,
	32512, 28083, 15576, 6708, 2888, 1243, 535, 188,
	32, -47, -64, -47, -22, -3, 7, 8,
	5, 3, 0, -1, -1, -1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, -9079,
	-9532, -16960, -335, 13001, 5333, 2704, 11192, 18742,
	13697, 5457, 2703, 1351, 24637, 32512, 24556, 10851,
	4185, 1614, 622, 184, 15, -57, -59, -34,
	-9, 5, 8, 6, 2, 0, -1, -1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, -8576, -10043, -18551, 4372,
	12190, 4809, 2472, 11230, 19803, 11170, 4953, 2473,
	1236, 26923, 32512, 20567, 7430, 2550, 875, 212,
	51, -30, -43, -25, -6, 3, 5, 3,
	1, 0, -1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, -6960, -10485, -19740, 7864, 11223, 4449, 2279,
	11623, 20380, 9488, 4553, 2280, 1140, 29209, 31829,
	16235, 4924, 1493, 452, 86, -7, -32, -20,
	-5, 2, 3, 2, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, -4739, -10974,
	-19831, 10240, 10190, 4169, 2114, 12524, 20649, 8531,
	4226, 2114, 1057, 31495, 29672, 11916, 3168, 841,
	121, 17, -22, -18, -5, 2, 2, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, -2333, -11641, -19288, 11765, 9175,
	3923, 1971, 13889, 20646, 8007, 3942, 1971, 985,
	32512, 27426, 8446, 1949, 449, 45, -11, -16,
	-5, 1, 1, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	29, -12616, -17971, 12690, 8247, 3693, 1846, 15662,
	20271, 7658, 3692, 1846, 923, 32512, 25132, 6284,
	1245, 246, -71, -78, -17, 8, 7, 1,
	-1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 2232, -14001, -15234,
	13198, 7447, 3478, 1736, 17409, 19411, 7332, 3472,
	1736, 868, 32512, 22545, 4352, 731, 18, -117,
	-40, 8, 9, 2, -1, -1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 4197, -15836, -11480, 13408, 6791, 3281,
	1639, 19224, 18074, 6978, 3276, 1639, 819, 32512,
	19657, 2706, 380, -148, -86, 2, 13, 3,
	-2, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 5863,
	-17878, -9460, 13389, 6270, 3104, 1551, 20996, 16431,
	6616, 3102, 1551, 776, 32512, 16633, 1921, 221,
	-95, -39, 5, 5, 0, -1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 7180, -20270, -6194, 13181,
	5866, 2946, 1473, 22548, 14746, 6273, 2946, 1473,
	737, 32512, 13621, 1263, 116, -53, -15, 4,
	2, -1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 8117, -21129, -2795, 12809, 5550, 2804, 1402,
	23717, 13326, 5962, 2804, 1402, 701, 32512, 10687,
	776, -56, -56, 4, 4, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 8560, -19953,
	508, 12299, 5295, 2675, 1337, 25109, 12263, 5684,
	2675, 1338, 669, 32512, 7905, 433, -36, -22,
	3, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 8488, -18731, 3672, 11679, 5080,
	2558, 1279, 26855, 11480, 5434, 2557, 1279, 639,
	32512, 5357, 212, -95, 0, 4, -1, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	7977, -24055, 6537, 10986, 4883, 2450, 1225, 28611,
	10918, 5206, 2450, 1225, 612, 32512, 3131, 83,
	-35, 2, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 7088, -30584, 9054,
	10265, 4696, 2351, 1176, 28707, 10494, 4996, 2351,
	1175, 588, 32512, 1920, -155, -13, 4, -1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 5952, -32627, 11249, 9564, 4519, 2260,
	1130, 28678, 10113, 4803, 2260, 1130, 565, 32512,
	1059, -73, -1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 4629,
	-32753, 13199, 8934, 4351, 2175, 1088, 28446, 9775,
	4623, 2175, 1087, 544, 32512, 434, -22, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 3132, -32768, 15225, 8430,
	4194, 2097, 1049, 30732, 9439, 4456, 2097, 1049,
	524, 32512, 75, -6, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 1345, -32768, 16765, 8107, 4048, 2025, 1012,
	32512, 9112, 4302, 2025, 1012, 506, 32385, 392,
	5, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, -706, -32768,
	17879, 8005, 3913, 1956, 978, 32512, 8843, 4157,
	1957, 978, 489, 31184, 1671, 122, 10, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, -3050, -32768, 18923, 8163, 3799,
	1893, 946, 32512, 8613, 4022, 1893, 945, 473,
	29903, 3074, 316, 52, 11, 3, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	-5812, -32768, 19851, 8626, 3739, 1833, 917, 32512,
	7982, 3889, 1833, 916, 459, 28541, 4567, 731,
	206, 66, 23, 8, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, -9235, -32768, 20587,
	9408, 3841, 1784, 889, 32512, 6486, 3688, 1776,
	889, 447, 27099, 6112, 1379, 313, 135, 65,
	33, 17, 7, 4, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, -12713
};

// [=]===^=[ hively_low_filter_table ]============================================================[=]
static int16_t hively_low_filter_table[] = {
	1188, 1318, -1178, -4304, -26320,
	-14931, -1716, -1486, 2494, 3611, 22275, 27450, -31839,
	-29668, -26258, -21608, -15880, -9560, -3211, 3138, 9369,
	15281, 20717, 25571, 29774, 32512, 32512, 32512, 32512,
	32512, 32512, 32512, 32512, 32512, 32512, 32512, 32512,
	32512, 32748, 32600, 32750, 32566, 32659, 32730, 8886,
	1762, 506, -1665, -12112, -24641, -8513, -2224, 247,
	3288, 9926, 25787, 28909, -31048, -27034, -20726, -12532,
	-3896, 4733, 13043, 20568, 27010, 32215, 32512, 32512,
	32512, 32512, 32512, 32512, 32512, 32762, 32696, 32647,
	32512, 32665, 32512, 32587, 32638, 32669, 32681, 32679,
	32667, 32648, 32624, 32598, 6183, 2141, -630, -2674,
	-21856, -18306, -5711, -2161, 2207, 4247, 17616, 26475,
	29719, -30017, -23596, -13741, -2819, 8029, 18049, 26470,
	32512, 32512, 32512, 32512, 32512, 32512, 32512, 32738,
	32663, 32612, 32756, 32549, 32602, 32629, 32636, 32628,
	32610, 32588, 32564, 32542, 32524, 32510, 32500, 32494,
	32492, 3604, 2248, -1495, -5612, -26800, -13545, -4745,
	-1390, 3443, 6973, 23495, 27724, 30246, -28745, -19355,
	-6335, 6861, 19001, 28690, 32512, 32512, 32512, 32512,
	32512, 32512, 32512, 32512, 32667, 32743, 32757, 32730,
	32681, 32624, 32572, 32529, 32500, 32482, 32476, 32477,
	32482, 32489, 32497, 32504, 32509, 32513, 7977, 1975,
	-1861, -9752, -25893, -10150, -4241, 86, 4190, 10643,
	25235, 28481, 30618, -27231, -14398, 1096, 15982, 27872,
	32512, 32512, 32512, 32512, 32512, 32734, 32631, 32767,
	32531, 32553, 32557, 32551, 32539, 32527, 32516, 32509,
	32505, 32504, 32505, 32506, 32508, 32510, 32511, 32512,
	32512, 32512, 32511, 14529, 1389, -2028, -14813, -22765,
	-7845, -3774, 1986, 4706, 14562, 25541, 29019, 30894,
	-25476, -9294, 8516, 23979, 32512, 32512, 32512, 32512,
	32512, 32512, 32708, 32762, 32727, 32654, 32579, 32522,
	32490, 32478, 32480, 32488, 32498, 32507, 32512, 32515,
	32515, 32514, 32513, 32512, 32510, 32510, 32510, 32510,
	17663, 557, -2504, -19988, -19501, -6436, -3340, 4135,
	5461, 18788, 26016, 29448, 31107, -23481, -4160, 15347,
	30045, 32512, 32512, 32512, 32512, 32512, 32674, 32700,
	32654, 32586, 32531, 32498, 32486, 32488, 32496, 32504,
	32510, 32513, 32514, 32513, 32512, 32511, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 16286, -402, -3522,
	-23951, -16641, -5631, -2983, 6251, 6837, 22781, 26712,
	29788, 31277, -21244, 1108, 21806, 32512, 32512, 32512,
	32512, 32695, 32576, 32622, 32600, 32557, 32520, 32501,
	32496, 32500, 32505, 32509, 32512, 32512, 32512, 32511,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 13436, -1351, -4793, -25948, -14224, -5151,
	-2702, 7687, 8805, 25705, 27348, 30064, 31415, -18766,
	5872, 26652, 32512, 32512, 32512, 32747, 32581, 32620,
	32586, 32540, 32508, 32497, 32499, 32505, 32510, 32512,
	32512, 32512, 32511, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 10427,
	-2162, -7136, -26147, -12195, -4810, -2474, 8723, 11098,
	27251, 27832, 30293, 31530, -16047, 10877, 30990, 32512,
	32512, 32512, 32512, 32584, 32571, 32536, 32511, 32502,
	32503, 32507, 32510, 32512, 32512, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 7797, -2748, -10188, -25174,
	-10519, -4515, -2281, 9397, 13473, 27937, 28213, 30487,
	31627, -13087, 15816, 32512, 32512, 32512, 32715, 32550,
	32560, 32534, 32512, 32505, 32506, 32508, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 5840, -3084, -13327, -23617, -9177, -4231, -2116,
	9892, 15843, 28292, 28538, 30652, 31710, -9886, 20235,
	32512, 32512, 32512, 32512, 32550, 32534, 32514, 32507,
	32507, 32510, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 4592, -3215,
	-15898, -21856, -8141, -3958, -1972, 10401, 18229, 28612,
	28824, 30796, 31781, -7103, 24037, 32512, 32512, 32745,
	32535, 32534, 32517, 32508, 32508, 32509, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 3964, -3262, -18721, -20087, -7368,
	-3705, -1847, 11014, 20634, 28996, 29075, 30920, 31843,
	-4732, 27243, 32512, 32512, 32648, 32627, 32530, 32495,
	32500, 32510, 32512, 32512, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	3858, -3404, -21965, -18398, -6801, -3479, -1738, 12009,
	22960, 29429, 29294, 31030, 31898, -2281, 30194, 32512,
	32512, 32699, 32569, 32496, 32496, 32509, 32513, 32512,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 4177, -3869, -24180,
	-16820, -6380, -3280, -1640, 13235, 25035, 29863, 29490,
	31128, 31947, 251, 32758, 32512, 32749, 32652, 32508,
	32490, 32507, 32513, 32512, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 4837, -4913, -26436, -15364, -6056, -3103,
	-1553, 14759, 26704, 30256, 29664, 31215, 31991, 2863,
	32512, 32512, 32657, 32580, 32503, 32501, 32510, 32512,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 5755,
	-6290, -27702, -14036, -5788, -2947, -1474, 16549, 27912,
	30602, 29821, 31294, 32030, 5555, 32512, 32512, 32592,
	32541, 32505, 32507, 32511, 32511, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 6898, -8911, -27788, -12841,
	-5550, -2805, -1403, 18509, 28687, 30906, 29963, 31364,
	32066, 8328, 32512, 32512, 32623, 32511, 32502, 32510,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 8107, -11465, -27077, -11789, -5325, -2676, -1339,
	19833, 29213, 31179, 30092, 31429, 32098, 11181, 32512,
	32512, 32561, 32508, 32508, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 9247, -13203,
	-25808, -10886, -5109, -2559, -1280, 21060, 29636, 31428,
	30209, 31488, 32127, 14114, 32512, 32681, 32529, 32502,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 10252, -16863, -24251, -10137, -4902,
	-2451, -1226, 21937, 30022, 31656, 30317, 31542, 32154,
	17128, 32512, 32581, 32514, 32508, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	11032, -22427, -22598, -9535, -4705, -2353, -1177, 20999,
	30406, 31867, 30415, 31591, 32179, 20222, 32512, 32591,
	32501, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 11539, -19778, -20962,
	-9060, -4522, -2261, -1131, 19486, 30789, 32061, 30507,
	31637, 32201, 23396, 32512, 32535, 32508, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 11803, -12759, -19353, -8690, -4353, -2177,
	-1089, 18499, 31165, 32240, 30591, 31678, 32222, 26651,
	32512, 32514, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 11826,
	-7586, -17510, -8384, -4196, -2099, -1050, 26861, 31521,
	32406, 30669, 31718, 32241, 29986, 32585, 32510, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 32511, 32511, 32511, 32511,
	32511, 32511, 32511, 32511, 11599, -2848, -15807, -8097,
	-4051, -2025, -1014, 30693, 31850, 32561, 30743, 31755,
	32261, 32512, 32524, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 11037, -5302, -14051, -7770, -3913, -1958, -980,
	28033, 32165, 32705, 30810, 31789, 32278, 32512, 32729,
	32536, 32513, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 10114, -7837,
	-12293, -7348, -3782, -1894, -948, 24926, 32473, 32512,
	30873, 31819, 32294, 32512, 32512, 32580, 32527, 32515,
	32512, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 8759, -10456, -10591, -6766, -3638,
	-1835, -917, 24058, 32600, 32512, 30934, 31850, 32309,
	32512, 32512, 32729, 32591, 32537, 32520, 32514, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	32510, 32510, 32510, 32510, 32510, 32510, 32510, 32510,
	6811, -13156, -9045, -5965, -3421, -1776, -890, 31582,
	32246, 32512, 30988, 31878, 32324, 32512, 32512, 32512,
	32628, 32573, 32541, 32526, 32518, 32514, 32513, 32512,
	32512, 32512, 32512, 32512, 32512, 32512, 32512, 32512,
	32512, 32512, 32512, 32512, 32512, 32512, 32512, 32512,
	32512, 32512, 32512, 32512, 32512, 4835
};

// [=]===^=[ hively_read_be16 ]===================================================================[=]
static uint16_t hively_read_be16(uint8_t *p) {
	return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

// [=]===^=[ hively_clip_shifted8 ]===============================================================[=]
static int32_t hively_clip_shifted8(int32_t in) {
	int16_t top = (int16_t)(in >> 16);
	if(top > 127) {
		in = 127 << 16;
	} else if(top < -128) {
		in = -(128 << 16);
	}
	return in;
}

// [=]===^=[ hively_gen_panning ]=================================================================[=]
static void hively_gen_panning(struct hively_waves *w) {
	double aa = (3.14159265 * 2.0) / 4.0;
	double ab = 0.0;
	for(int32_t i = 0; i < 256; ++i) {
		w->panning_left[i]  = (int32_t)(sin(aa) * 255.0);
		w->panning_right[i] = (int32_t)(sin(ab) * 255.0);
		aa += (3.14159265 * 2.0 / 4.0) / 256.0;
		ab += (3.14159265 * 2.0 / 4.0) / 256.0;
	}
	w->panning_left[255] = 0;
	w->panning_right[0]  = 0;
}

// [=]===^=[ hively_gen_sawtooth ]================================================================[=]
static void hively_gen_sawtooth(int8_t *buffer, int32_t offset, int32_t len) {
	int32_t add = 256 / (len - 1);
	int32_t val = -128;
	for(int32_t i = 0; i < len; ++i) {
		buffer[offset++] = (int8_t)val;
		val += add;
	}
}

// [=]===^=[ hively_gen_triangle ]================================================================[=]
static void hively_gen_triangle(int8_t *buffer, int32_t offset, int32_t len) {
	int32_t d2 = len;
	int32_t d5 = len >> 2;
	int32_t d1 = 128 / d5;
	int32_t d4 = -(d2 >> 1);
	int32_t val = 0;
	for(int32_t i = 0; i < d5; ++i) {
		buffer[offset++] = (int8_t)val;
		val += d1;
	}
	buffer[offset++] = 0x7f;
	if(d5 != 1) {
		val = 128;
		for(int32_t i = 0; i < d5 - 1; ++i) {
			val -= d1;
			buffer[offset++] = (int8_t)val;
		}
	}
	int32_t offset2 = offset + d4;
	for(int32_t i = 0; i < d5 * 2; ++i) {
		int8_t c = buffer[offset2++];
		if(c == 0x7f) {
			c = -128;
		} else {
			c = (int8_t)-c;
		}
		buffer[offset++] = c;
	}
}

// [=]===^=[ hively_gen_square ]==================================================================[=]
static void hively_gen_square(int8_t *buffer) {
	int32_t offset = 0;
	for(int32_t i = 1; i <= 0x20; ++i) {
		for(int32_t j = 0; j < (0x40 - i) * 2; ++j) {
			buffer[offset++] = -128;
		}
		for(int32_t j = 0; j < i * 2; ++j) {
			buffer[offset++] = 127;
		}
	}
}

// [=]===^=[ hively_gen_white_noise ]=============================================================[=]
static void hively_gen_white_noise(int8_t *buffer, int32_t len) {
	uint32_t ays = 0x41595321;
	int32_t offset = 0;
	do {
		uint8_t s = (uint8_t)ays;
		if((ays & 0x100) != 0) {
			s = 0x7f;
			if((ays & 0x8000) != 0) {
				s = 0x80;
			}
		}
		buffer[offset++] = (int8_t)s;
		--len;
		ays = (ays >> 5) | (ays << 27);
		ays = (ays & 0xffffff00) | ((ays & 0xff) ^ 0x9a);
		uint16_t bx = (uint16_t)ays;
		ays = (ays << 2) | (ays >> 30);
		uint16_t ax = (uint16_t)ays;
		bx = (uint16_t)(bx + ax);
		ax = (uint16_t)(ax ^ bx);
		ays = (ays & 0xffff0000) | ax;
		ays = (ays >> 3) | (ays << 29);
	} while(len != 0);
}

// [=]===^=[ hively_gen_filter ]==================================================================[=]
static void hively_gen_filter(int8_t *input, int32_t offset, int32_t len, int32_t freq, int32_t *filter_table_offset, int8_t *out_low, int8_t *out_high) {
	int32_t mid = (int32_t)hively_mid_filter_table[*filter_table_offset] << 8;
	int32_t low = (int32_t)hively_low_filter_table[*filter_table_offset] << 8;
	(*filter_table_offset)++;
	for(int32_t i = 0; i < len; ++i) {
		int32_t in = (int32_t)input[offset + i] << 16;
		int32_t high = hively_clip_shifted8(in - mid - low);
		int32_t fre = (high >> 8) * freq;
		mid = hively_clip_shifted8(mid + fre);
		fre = (mid >> 8) * freq;
		low = hively_clip_shifted8(low + fre);
		out_high[offset + i] = (int8_t)(high >> 16);
		out_low[offset + i]  = (int8_t)(low >> 16);
	}
}

// [=]===^=[ hively_gen_filter_waveforms ]========================================================[=]
static void hively_gen_filter_waveforms(struct hively_waves *w) {
	int32_t fto = 0;
	for(int32_t i = 0, freq = 25; i < 31; ++i, freq += 9) {
		hively_gen_filter(w->sawtooths[31], hively_offset_table[0], 0x04, freq, &fto, w->sawtooths[i], w->sawtooths[i + 32]);
		hively_gen_filter(w->sawtooths[31], hively_offset_table[1], 0x08, freq, &fto, w->sawtooths[i], w->sawtooths[i + 32]);
		hively_gen_filter(w->sawtooths[31], hively_offset_table[2], 0x10, freq, &fto, w->sawtooths[i], w->sawtooths[i + 32]);
		hively_gen_filter(w->sawtooths[31], hively_offset_table[3], 0x20, freq, &fto, w->sawtooths[i], w->sawtooths[i + 32]);
		hively_gen_filter(w->sawtooths[31], hively_offset_table[4], 0x40, freq, &fto, w->sawtooths[i], w->sawtooths[i + 32]);
		hively_gen_filter(w->sawtooths[31], hively_offset_table[5], 0x80, freq, &fto, w->sawtooths[i], w->sawtooths[i + 32]);

		hively_gen_filter(w->triangles[31], hively_offset_table[0], 0x04, freq, &fto, w->triangles[i], w->triangles[i + 32]);
		hively_gen_filter(w->triangles[31], hively_offset_table[1], 0x08, freq, &fto, w->triangles[i], w->triangles[i + 32]);
		hively_gen_filter(w->triangles[31], hively_offset_table[2], 0x10, freq, &fto, w->triangles[i], w->triangles[i + 32]);
		hively_gen_filter(w->triangles[31], hively_offset_table[3], 0x20, freq, &fto, w->triangles[i], w->triangles[i + 32]);
		hively_gen_filter(w->triangles[31], hively_offset_table[4], 0x40, freq, &fto, w->triangles[i], w->triangles[i + 32]);
		hively_gen_filter(w->triangles[31], hively_offset_table[5], 0x80, freq, &fto, w->triangles[i], w->triangles[i + 32]);

		hively_gen_filter(w->white_noise[31], 0, 0x280 * 3, freq, &fto, w->white_noise[i], w->white_noise[i + 32]);

		for(int32_t j = 0; j < 32; ++j) {
			hively_gen_filter(w->squares[31], j * 0x80, 0x80, freq, &fto, w->squares[i], w->squares[i + 32]);
		}
	}
}

// [=]===^=[ hively_gen_waves ]===================================================================[=]
static void hively_gen_waves(struct hively_waves *w) {
	memset(w, 0, sizeof(*w));
	hively_gen_panning(w);
	hively_gen_sawtooth(w->sawtooths[31], hively_offset_table[0], 0x04);
	hively_gen_sawtooth(w->sawtooths[31], hively_offset_table[1], 0x08);
	hively_gen_sawtooth(w->sawtooths[31], hively_offset_table[2], 0x10);
	hively_gen_sawtooth(w->sawtooths[31], hively_offset_table[3], 0x20);
	hively_gen_sawtooth(w->sawtooths[31], hively_offset_table[4], 0x40);
	hively_gen_sawtooth(w->sawtooths[31], hively_offset_table[5], 0x80);
	hively_gen_triangle(w->triangles[31], hively_offset_table[0], 0x04);
	hively_gen_triangle(w->triangles[31], hively_offset_table[1], 0x08);
	hively_gen_triangle(w->triangles[31], hively_offset_table[2], 0x10);
	hively_gen_triangle(w->triangles[31], hively_offset_table[3], 0x20);
	hively_gen_triangle(w->triangles[31], hively_offset_table[4], 0x40);
	hively_gen_triangle(w->triangles[31], hively_offset_table[5], 0x80);
	hively_gen_square(w->squares[31]);
	hively_gen_white_noise(w->white_noise[31], 0x280 * 3);
	hively_gen_filter_waveforms(w);
}

// [=]===^=[ hively_test_module ]=================================================================[=]
static int32_t hively_test_module(uint8_t *data, uint32_t len) {
	if(len < 14) {
		return HIVELY_TYPE_UNKNOWN;
	}
	uint8_t version = data[3];
	if(data[0] == 'T' && data[1] == 'H' && data[2] == 'X') {
		if(version == 0) { return HIVELY_TYPE_AHX1; }
		if(version == 1) { return HIVELY_TYPE_AHX2; }
	} else if(data[0] == 'H' && data[1] == 'V' && data[2] == 'L') {
		if(version == 0 || version == 1) { return HIVELY_TYPE_HVL; }
	}
	return HIVELY_TYPE_UNKNOWN;
}

// [=]===^=[ hively_voice_init ]==================================================================[=]
static void hively_voice_init(struct hively_voice *v, int32_t voice_num, struct hively_song *song) {
	memset(v, 0, sizeof(*v));
	v->override_transpose = 1000;
	v->delta = 1;
	v->track_master_volume = 0x40;
	v->filter_pos = 32;
	v->track_on = 1;
	v->wn_random = 0x280;
	v->mix_source = v->voice_buffer;

	if(((voice_num % 4) == 0) || ((voice_num % 4) == 3)) {
		v->pan = song->default_panning_left;
		v->set_pan = song->default_panning_left;
	} else {
		v->pan = song->default_panning_right;
		v->set_pan = song->default_panning_right;
	}
}

// [=]===^=[ hively_voice_calc_adsr ]=============================================================[=]
static void hively_voice_calc_adsr(struct hively_voice *v) {
	struct hively_envelope *src = &v->instrument->envelope;
	struct hively_envelope *dst = &v->adsr;
	dst->a_frames = src->a_frames;
	dst->a_volume = dst->a_frames != 0 ? src->a_volume * 256 / dst->a_frames : src->a_volume * 256;
	dst->d_frames = src->d_frames;
	dst->d_volume = dst->d_frames != 0 ? (src->d_volume - src->a_volume) * 256 / dst->d_frames : src->d_volume * 256;
	dst->s_frames = src->s_frames;
	dst->r_frames = src->r_frames;
	dst->r_volume = dst->r_frames != 0 ? (src->r_volume - src->d_volume) * 256 / dst->r_frames : src->r_volume * 256;
}

// [=]===^=[ hively_load ]========================================================================[=]
static int32_t hively_load(struct hivelytracker_state *s, uint8_t *data, uint32_t len) {
	struct hively_song *song = &s->song;
	uint32_t pos = 6;
	if(pos + 8 > len) { return 0; }

	uint8_t flag = data[pos++];
	song->speed_multiplier = s->module_type == HIVELY_TYPE_AHX1 ? 1 : ((flag >> 5) & 3) + 1;
	song->position_nr = ((flag & 0xf) << 8) | data[pos++];
	uint16_t restart = hively_read_be16(data + pos);
	pos += 2;

	if(s->module_type == HIVELY_TYPE_HVL) {
		song->channels = (restart >> 10) + 4;
		song->restart = restart & 0x03ff;
	} else {
		song->channels = 4;
		song->restart = restart;
	}

	if(song->channels < 1 || song->channels > HIVELY_MAX_CHANNELS) { return 0; }

	song->track_length = data[pos++];
	song->track_nr = data[pos++];
	song->instrument_nr = data[pos++];
	uint8_t sub_song_nr = data[pos++];

	if(s->module_type == HIVELY_TYPE_HVL) {
		if(pos + 2 > len) { return 0; }
		song->mix_gain = ((int32_t)data[pos++] << 8) / 100;
		song->default_stereo = data[pos++];
	} else {
		song->default_stereo = 4;
		song->mix_gain = (100 * 256) / 100;
	}

	if(song->default_stereo < 0 || song->default_stereo > 4) { song->default_stereo = 4; }
	song->default_panning_left  = hively_stereo_pan_left[song->default_stereo];
	song->default_panning_right = hively_stereo_pan_right[song->default_stereo];

	if(song->position_nr < 1 || song->position_nr > 999) { return 0; }
	if(song->restart < 0 || song->restart >= song->position_nr) { return 0; }
	if(song->track_length < 1 || song->track_length > 64) { return 0; }
	if(song->instrument_nr < 0 || song->instrument_nr > 63) { return 0; }

	pos += sub_song_nr * 2;
	if(pos > len) { return 0; }

	song->positions = (struct hively_position *)calloc((size_t)song->position_nr, sizeof(struct hively_position));
	if(!song->positions) { return 0; }

	for(int32_t i = 0; i < song->position_nr; ++i) {
		for(int32_t j = 0; j < song->channels; ++j) {
			if(pos + 2 > len) { return 0; }
			song->positions[i].track[j] = data[pos++];
			song->positions[i].transpose[j] = (int8_t)data[pos++];
		}
	}

	int32_t max_track = song->track_nr;
	song->tracks = (struct hively_step **)calloc((size_t)(max_track + 1), sizeof(struct hively_step *));
	if(!song->tracks) { return 0; }

	for(int32_t i = 0; i <= max_track; ++i) {
		song->tracks[i] = (struct hively_step *)calloc((size_t)song->track_length, sizeof(struct hively_step));
		if(!song->tracks[i]) { return 0; }
		if(((flag & 0x80) == 0x80) && (i == 0)) {
			continue;
		}
		for(int32_t j = 0; j < song->track_length; ++j) {
			if(s->module_type == HIVELY_TYPE_HVL) {
				if(pos + 1 > len) { return 0; }
				uint8_t b1 = data[pos++];
				if(b1 == 0x3f) {
					continue;
				}
				if(pos + 4 > len) { return 0; }
				uint8_t b2 = data[pos++];
				uint8_t b3 = data[pos++];
				uint8_t b4 = data[pos++];
				uint8_t b5 = data[pos++];
				song->tracks[i][j].note = b1;
				song->tracks[i][j].instrument = b2;
				song->tracks[i][j].fx = b3 >> 4;
				song->tracks[i][j].fx_param = b4;
				song->tracks[i][j].fx_b = b3 & 0xf;
				song->tracks[i][j].fx_b_param = b5;
			} else {
				if(pos + 3 > len) { return 0; }
				uint8_t b1 = data[pos++];
				uint8_t b2 = data[pos++];
				uint8_t b3 = data[pos++];
				song->tracks[i][j].note = (b1 >> 2) & 0x3f;
				song->tracks[i][j].instrument = ((b1 & 0x3) << 4) | (b2 >> 4);
				song->tracks[i][j].fx = b2 & 0xf;
				song->tracks[i][j].fx_param = b3;
				song->tracks[i][j].fx_b = 0;
				song->tracks[i][j].fx_b_param = 0;
			}
		}
	}

	song->instruments = (struct hively_instrument *)calloc((size_t)(song->instrument_nr + 1), sizeof(struct hively_instrument));
	if(!song->instruments) { return 0; }

	for(int32_t i = 1; i <= song->instrument_nr; ++i) {
		struct hively_instrument *ins = &song->instruments[i];
		if(pos + 22 > len) { return 0; }
		ins->volume = data[pos++];
		uint8_t b1 = data[pos++];
		ins->wave_length = b1 & 0x7;
		ins->envelope.a_frames = data[pos++];
		ins->envelope.a_volume = data[pos++];
		ins->envelope.d_frames = data[pos++];
		ins->envelope.d_volume = data[pos++];
		ins->envelope.s_frames = data[pos++];
		ins->envelope.r_frames = data[pos++];
		ins->envelope.r_volume = data[pos++];
		pos += 3;
		uint8_t b12 = data[pos++];
		if(s->module_type == HIVELY_TYPE_AHX1) {
			ins->filter_speed = 0;
			ins->filter_lower_limit = 0;
		} else {
			ins->filter_speed = ((b1 >> 3) & 0x1f) | ((b12 >> 2) & 0x20);
			ins->filter_lower_limit = b12 & 0x7f;
		}
		ins->vibrato_delay = data[pos++];
		uint8_t b14 = data[pos++];
		if(s->module_type == HIVELY_TYPE_AHX1) {
			ins->hard_cut_release_frames = 0;
			ins->hard_cut_release = 0;
		} else {
			ins->hard_cut_release_frames = (b14 >> 4) & 7;
			ins->hard_cut_release = (b14 & 0x80) != 0;
		}
		ins->vibrato_depth = b14 & 0xf;
		ins->vibrato_speed = data[pos++];
		ins->square_lower_limit = data[pos++];
		ins->square_upper_limit = data[pos++];
		ins->square_speed = data[pos++];
		uint8_t b19 = data[pos++];
		if(s->module_type == HIVELY_TYPE_AHX1) {
			ins->filter_upper_limit = 0;
		} else {
			ins->filter_speed |= ((b19 >> 1) & 0x40);
			ins->filter_upper_limit = b19 & 0x3f;
		}
		ins->play_list.speed = data[pos++];
		ins->play_list.length = data[pos++];

		ins->play_list.entries = (struct hively_plist_entry *)calloc((size_t)(ins->play_list.length + 1), sizeof(struct hively_plist_entry));
		if(!ins->play_list.entries) { return 0; }

		for(int32_t j = 0; j < ins->play_list.length; ++j) {
			if(pos + 4 > len) { return 0; }
			uint8_t pb1 = data[pos++];
			uint8_t pb2 = data[pos++];
			uint8_t pb3 = data[pos++];
			uint8_t pb4 = data[pos++];
			if(s->module_type == HIVELY_TYPE_HVL) {
				if(pos + 1 > len) { return 0; }
				uint8_t pb5 = data[pos++];
				ins->play_list.entries[j].fx[0] = pb1 & 0xf;
				ins->play_list.entries[j].fx[1] = (pb2 >> 3) & 0xf;
				ins->play_list.entries[j].waveform = pb2 & 7;
				ins->play_list.entries[j].fixed_note = ((pb3 >> 6) & 1) != 0;
				ins->play_list.entries[j].note = pb3 & 0x3f;
				ins->play_list.entries[j].fx_param[0] = pb4;
				ins->play_list.entries[j].fx_param[1] = pb5;
			} else {
				int32_t fx1 = (pb1 >> 2) & 7;
				fx1 = fx1 == 6 ? 12 : fx1 == 7 ? 15 : fx1;
				int32_t fx2 = (pb1 >> 5) & 7;
				fx2 = fx2 == 6 ? 12 : fx2 == 7 ? 15 : fx2;
				ins->play_list.entries[j].fx[0] = fx1;
				ins->play_list.entries[j].fx[1] = fx2;
				ins->play_list.entries[j].waveform = ((pb1 << 1) & 6) | (pb2 >> 7);
				ins->play_list.entries[j].fixed_note = ((pb2 >> 6) & 1) != 0;
				ins->play_list.entries[j].note = pb2 & 0x3f;
				ins->play_list.entries[j].fx_param[0] = pb3;
				ins->play_list.entries[j].fx_param[1] = pb4;
			}
		}
	}

	return 1;
}

// [=]===^=[ hively_initialize_sound ]============================================================[=]
static void hively_initialize_sound(struct hivelytracker_state *s, int32_t start_position) {
	s->pos_nr = start_position;
	s->pos_jump = 0;
	s->pattern_break = 0;
	s->note_nr = 0;
	s->pos_jump_note = 0;
	s->tempo = 6;
	s->step_wait_frames = 0;
	s->get_new_position = 1;
	s->square_waveform = 0;

	for(int32_t v = 0; v < s->song.channels; ++v) {
		hively_voice_init(&s->voices[v], v, &s->song);
	}
}

// [=]===^=[ hively_get_waveform ]================================================================[=]
static int8_t *hively_get_waveform(struct hivelytracker_state *s, int32_t waveform, int32_t filter) {
	if(waveform == 3 - 1) {
		return s->square_waveform;
	}
	int32_t fi = filter - 1;
	if(fi < 0) { fi = 0; }
	if(fi >= HIVELY_FILTER_SETS) { fi = HIVELY_FILTER_SETS - 1; }
	switch(waveform) {
		case 1 - 1: {
			return s->waves->triangles[fi];
		}

		case 2 - 1: {
			return s->waves->sawtooths[fi];
		}

		case 4 - 1: {
			return s->waves->white_noise[fi];
		}
	}
	return 0;
}

// [=]===^=[ hively_plist_command ]===============================================================[=]
static void hively_plist_command(struct hivelytracker_state *s, int32_t v_idx, int32_t fx, int32_t fx_param) {
	struct hively_voice *voice = &s->voices[v_idx];

	switch(fx) {
		case 0: {
			if((s->module_type != HIVELY_TYPE_AHX1) && (fx_param > 0) && (fx_param < 0x40)) {
				if(voice->ignore_filter != 0) {
					voice->filter_pos = voice->ignore_filter;
					voice->ignore_filter = 0;
				} else {
					voice->filter_pos = fx_param;
				}
				voice->new_waveform = 1;
			}
			break;
		}

		case 1: {
			voice->period_perf_slide_speed = fx_param;
			voice->period_perf_slide_on = 1;
			break;
		}

		case 2: {
			voice->period_perf_slide_speed = -fx_param;
			voice->period_perf_slide_on = 1;
			break;
		}

		case 3: {
			if(!voice->ignore_square) {
				voice->square_pos = fx_param >> (5 - voice->wave_length);
			} else {
				voice->ignore_square = 0;
			}
			break;
		}

		case 4: {
			if((s->module_type == HIVELY_TYPE_AHX1) || (fx_param == 0)) {
				voice->square_on = !voice->square_on;
				voice->square_init = voice->square_on;
				voice->square_sign = 1;
			} else {
				if((fx_param & 0x0f) != 0x00) {
					voice->square_on = !voice->square_on;
					voice->square_init = voice->square_on;
					voice->square_sign = 1;
					if((fx_param & 0x0f) == 0x0f) {
						voice->square_sign = -1;
					}
				}
				if((fx_param & 0xf0) != 0x00) {
					voice->filter_on = !voice->filter_on;
					voice->filter_init = voice->filter_on;
					voice->filter_sign = 1;
					if((fx_param & 0xf0) == 0xf0) {
						voice->filter_sign = -1;
					}
				}
			}
			break;
		}

		case 5: {
			voice->perf_current = fx_param;
			break;
		}

		case 7: {
			if(s->module_type == HIVELY_TYPE_HVL) {
				if((fx_param >= 1) && (fx_param <= 0x3c)) {
					voice->ring_base_period = fx_param;
					voice->ring_fixed_period = 1;
				} else if((fx_param >= 0x81) && (fx_param <= 0xbc)) {
					voice->ring_base_period = fx_param - 0x80;
					voice->ring_fixed_period = 0;
				} else {
					voice->ring_base_period = 0;
					voice->ring_fixed_period = 0;
					voice->ring_new_waveform = 0;
					voice->ring_audio_source = 0;
					voice->ring_audio_offset = 0;
					voice->ring_mix_source = 0;
					break;
				}
				voice->ring_waveform = 0;
				voice->ring_new_waveform = 1;
				voice->ring_plant_period = 1;
			}
			break;
		}

		case 8: {
			if(s->module_type == HIVELY_TYPE_HVL) {
				if((fx_param >= 1) && (fx_param <= 0x3c)) {
					voice->ring_base_period = fx_param;
					voice->ring_fixed_period = 1;
				} else if((fx_param >= 0x81) && (fx_param <= 0xbc)) {
					voice->ring_base_period = fx_param - 0x80;
					voice->ring_fixed_period = 0;
				} else {
					voice->ring_base_period = 0;
					voice->ring_fixed_period = 0;
					voice->ring_new_waveform = 0;
					voice->ring_audio_source = 0;
					voice->ring_audio_offset = 0;
					voice->ring_mix_source = 0;
					break;
				}
				voice->ring_waveform = 1;
				voice->ring_new_waveform = 1;
				voice->ring_plant_period = 1;
			}
			break;
		}

		case 9: {
			if(s->module_type == HIVELY_TYPE_HVL) {
				if(fx_param > 127) {
					fx_param -= 256;
				}
				voice->pan = fx_param + 128;
			}
			break;
		}

		case 12: {
			if(fx_param <= 0x40) {
				voice->note_max_volume = fx_param;
				break;
			}
			fx_param -= 0x50;
			if(fx_param < 0) { break; }
			if(fx_param <= 0x40) {
				voice->perf_sub_volume = fx_param;
				break;
			}
			fx_param -= 0xa0 - 0x50;
			if(fx_param < 0) { break; }
			if(fx_param <= 0x40) {
				voice->track_master_volume = fx_param;
			}
			break;
		}

		case 15: {
			voice->perf_speed = fx_param;
			voice->perf_wait = fx_param;
			break;
		}
	}
}

// [=]===^=[ hively_step_fx1 ]====================================================================[=]
static void hively_step_fx1(struct hivelytracker_state *s, struct hively_voice *voice, int32_t fx, int32_t fx_param) {
	switch(fx) {
		case 0x0: {
			if(((fx_param & 0x0f) > 0) && ((fx_param & 0x0f) <= 9)) {
				s->pos_jump = fx_param & 0x0f;
			}
			break;
		}

		case 0x5:
		case 0xa: {
			voice->volume_slide_down = fx_param & 0x0f;
			voice->volume_slide_up = fx_param >> 4;
			break;
		}

		case 0x7: {
			if(fx_param > 127) {
				fx_param -= 256;
			}
			voice->pan = fx_param + 128;
			voice->set_pan = fx_param + 128;
			break;
		}

		case 0xb: {
			s->pos_jump = s->pos_jump * 100 + (fx_param & 0x0f) + (fx_param >> 4) * 10;
			s->pattern_break = 1;
			break;
		}

		case 0xd: {
			s->pos_jump = s->pos_nr + 1;
			s->pos_jump_note = s->module_type == HIVELY_TYPE_AHX1 ? 0 : (fx_param & 0x0f) + (fx_param >> 4) * 10;
			s->pattern_break = 1;
			if(s->pos_jump_note > s->song.track_length) {
				s->pos_jump_note = 0;
			}
			break;
		}

		case 0xe: {
			switch(fx_param >> 4) {
				case 0xc: {
					if((fx_param & 0x0f) < s->tempo) {
						voice->note_cut_wait = fx_param & 0x0f;
						if(voice->note_cut_wait != 0) {
							voice->note_cut_on = 1;
							voice->hard_cut_release = 0;
						}
					}
					break;
				}
			}
			break;
		}

		case 0xf: {
			s->tempo = fx_param;
			break;
		}
	}
}

// [=]===^=[ hively_step_fx2 ]====================================================================[=]
static void hively_step_fx2(struct hivelytracker_state *s, struct hively_voice *voice, int32_t fx, int32_t fx_param, int32_t *note) {
	switch(fx) {
		case 0x9: {
			voice->square_pos = fx_param >> (5 - voice->wave_length);
			voice->ignore_square = 1;
			break;
		}

		case 0x3: {
			if(fx_param != 0) {
				voice->period_slide_speed = fx_param;
			}
			// fallthrough into case 0x5
		} /* fall through */
		case 0x5: {
			if(*note != 0) {
				int32_t newp = hively_period_table[*note];
				int32_t diff = hively_period_table[voice->track_period];
				diff -= newp;
				newp = diff + voice->period_slide_period;
				if(newp != 0) {
					voice->period_slide_limit = -diff;
				}
			}
			voice->period_slide_on = 1;
			voice->period_slide_with_limit = 1;
			*note = 0;
			break;
		}
	}
	(void)s;
}

// [=]===^=[ hively_step_fx3 ]====================================================================[=]
static void hively_step_fx3(struct hivelytracker_state *s, struct hively_voice *voice, int32_t fx, int32_t fx_param) {
	switch(fx) {
		case 0x1: {
			voice->period_slide_speed = -fx_param;
			voice->period_slide_on = 1;
			voice->period_slide_with_limit = 0;
			break;
		}

		case 0x2: {
			voice->period_slide_speed = fx_param;
			voice->period_slide_on = 1;
			voice->period_slide_with_limit = 0;
			break;
		}

		case 0x4: {
			if(s->module_type == HIVELY_TYPE_AHX1) {
				fx_param &= 0x0f;
			}
			if((fx_param == 0) || (fx_param == 0x40)) { break; }
			if(fx_param < 0x40) {
				voice->ignore_filter = fx_param;
				break;
			}
			if(fx_param > 0x7f) { break; }
			voice->filter_pos = fx_param - 0x40;
			break;
		}

		case 0xc: {
			if(fx_param <= 0x40) {
				voice->note_max_volume = fx_param;
				break;
			}
			fx_param -= 0x50;
			if(fx_param < 0) { break; }
			if(fx_param <= 0x40) {
				for(int32_t i = 0; i < s->song.channels; ++i) {
					s->voices[i].track_master_volume = fx_param;
				}
				break;
			}
			fx_param -= 0xa0 - 0x50;
			if(fx_param < 0) { break; }
			if(fx_param <= 0x40) {
				voice->track_master_volume = fx_param;
			}
			break;
		}

		case 0xe: {
			switch(fx_param >> 4) {
				case 0x1: {
					voice->period_slide_period -= (fx_param & 0x0f);
					voice->plant_period = 1;
					break;
				}

				case 0x2: {
					voice->period_slide_period += fx_param & 0x0f;
					voice->plant_period = 1;
					break;
				}

				case 0x4: {
					voice->vibrato_depth = fx_param & 0x0f;
					break;
				}

				case 0xa: {
					voice->note_max_volume += fx_param & 0x0f;
					if(voice->note_max_volume > 0x40) {
						voice->note_max_volume = 0x40;
					}
					break;
				}

				case 0xb: {
					voice->note_max_volume -= fx_param & 0x0f;
					if(voice->note_max_volume < 0) {
						voice->note_max_volume = 0;
					}
					break;
				}

				case 0xf: {
					if(s->module_type == HIVELY_TYPE_HVL) {
						switch(fx_param & 0xf) {
							case 1: {
								voice->override_transpose = voice->transpose;
								break;
							}
						}
					}
					break;
				}
			}
			break;
		}
	}
}

// [=]===^=[ hively_process_step ]================================================================[=]
static void hively_process_step(struct hivelytracker_state *s, int32_t v_idx) {
	struct hively_voice *voice = &s->voices[v_idx];
	if(!voice->track_on) {
		return;
	}

	voice->volume_slide_up = 0;
	voice->volume_slide_down = 0;

	struct hively_step *step = &s->song.tracks[s->song.positions[s->pos_nr].track[v_idx]][s->note_nr];
	int32_t note = step->note;
	int32_t instrument = step->instrument;
	int32_t fx = step->fx;
	int32_t fx_param = step->fx_param;
	int32_t fx_b = step->fx_b;
	int32_t fx_b_param = step->fx_b_param;

	int32_t done_note_del = 0;
	int32_t i;

	for(i = 0; i < 2; ++i) {
		int32_t cfx = i == 0 ? fx : fx_b;
		int32_t cparam = i == 0 ? fx_param : fx_b_param;
		if(((cfx & 0xf) == 0xe) && ((cparam & 0xf0) == 0xd0)) {
			if(voice->note_delay_on) {
				voice->note_delay_on = 0;
				done_note_del = 1;
			} else {
				if((cparam & 0x0f) < s->tempo) {
					voice->note_delay_wait = cparam & 0x0f;
					if(voice->note_delay_wait != 0) {
						voice->note_delay_on = 1;
						return;
					}
				}
			}
			if(i == 0 && done_note_del) {
				break;
			}
		}
	}

	if(note != 0) {
		voice->override_transpose = 1000;
	}

	hively_step_fx1(s, voice, fx & 0xf, fx_param);
	hively_step_fx1(s, voice, fx_b & 0xf, fx_b_param);

	if((instrument != 0) && (instrument <= s->song.instrument_nr)) {
		struct hively_instrument *ins = &s->song.instruments[instrument];

		voice->pan = voice->set_pan;
		voice->period_slide_speed = 0;
		voice->period_slide_period = 0;
		voice->period_slide_limit = 0;
		voice->perf_sub_volume = 0x40;
		voice->adsr_volume = 0;
		voice->instrument = ins;
		voice->instrument_number = instrument;
		voice->sample_pos = 0;

		hively_voice_calc_adsr(voice);

		voice->wave_length = ins->wave_length;
		voice->note_max_volume = ins->volume;

		voice->vibrato_current = 0;
		voice->vibrato_delay = ins->vibrato_delay;
		voice->vibrato_depth = ins->vibrato_depth;
		voice->vibrato_speed = ins->vibrato_speed;
		voice->vibrato_period = 0;

		voice->hard_cut_release = ins->hard_cut_release;
		voice->hard_cut = ins->hard_cut_release_frames;

		voice->ignore_square = 0;
		voice->square_sliding_in = 0;
		voice->square_wait = 0;
		voice->square_on = 0;

		int32_t sl = ins->square_lower_limit >> (5 - voice->wave_length);
		int32_t su = ins->square_upper_limit >> (5 - voice->wave_length);
		if(su < sl) {
			int32_t tmp = sl; sl = su; su = tmp;
		}
		voice->square_upper_limit = su;
		voice->square_lower_limit = sl;

		voice->ignore_filter = 0;
		voice->filter_wait = 0;
		voice->filter_on = 0;
		voice->filter_sliding_in = 0;

		int32_t d6 = ins->filter_speed;
		int32_t d3 = ins->filter_lower_limit;
		int32_t d4 = ins->filter_upper_limit;
		if((d3 & 0x80) != 0) { d6 |= 0x20; }
		if((d4 & 0x80) != 0) { d6 |= 0x40; }
		voice->filter_speed = d6;
		d3 &= ~0x80;
		d4 &= ~0x80;
		if(d3 > d4) {
			int32_t tmp = d3; d3 = d4; d4 = tmp;
		}
		voice->filter_upper_limit = d4;
		voice->filter_lower_limit = d3;
		voice->filter_pos = 32;

		voice->perf_wait = 0;
		voice->perf_current = 0;
		voice->perf_speed = ins->play_list.speed;
		voice->perf_list = &ins->play_list;

		voice->ring_mix_source = 0;
		voice->ring_sample_pos = 0;
		voice->ring_plant_period = 0;
		voice->ring_new_waveform = 0;
	}

	voice->period_slide_on = 0;

	hively_step_fx2(s, voice, fx & 0xf, fx_param, &note);
	hively_step_fx2(s, voice, fx_b & 0xf, fx_b_param, &note);

	if(note != 0) {
		voice->track_period = note;
		voice->plant_period = 1;
		voice->kick_note = 1;
	}

	hively_step_fx3(s, voice, fx & 0xf, fx_param);
	hively_step_fx3(s, voice, fx_b & 0xf, fx_b_param);
}

// [=]===^=[ hively_process_frame ]===============================================================[=]
static void hively_process_frame(struct hivelytracker_state *s, int32_t v_idx) {
	struct hively_voice *voice = &s->voices[v_idx];
	if(!voice->track_on) {
		return;
	}

	if(voice->note_delay_on) {
		if(voice->note_delay_wait <= 0) {
			hively_process_step(s, v_idx);
		} else {
			voice->note_delay_wait--;
		}
	}

	if(voice->hard_cut != 0) {
		int32_t next_instrument;
		if((s->note_nr + 1) < s->song.track_length) {
			next_instrument = s->song.tracks[voice->track][s->note_nr + 1].instrument;
		} else {
			next_instrument = s->song.tracks[voice->next_track][0].instrument;
		}
		if(next_instrument != 0) {
			int32_t d1 = s->tempo - voice->hard_cut;
			if(d1 < 0) { d1 = 0; }
			if(!voice->note_cut_on) {
				voice->note_cut_on = 1;
				voice->note_cut_wait = d1;
				voice->hard_cut_release_f = -(d1 - s->tempo);
			} else {
				voice->hard_cut = 0;
			}
		}
	}

	if(voice->note_cut_on) {
		if(voice->note_cut_wait <= 0) {
			voice->note_cut_on = 0;
			if(voice->hard_cut_release && voice->instrument) {
				voice->adsr.r_frames = voice->hard_cut_release_f;
				voice->adsr.r_volume = 0;
				if(voice->adsr.r_frames > 0) {
					voice->adsr.r_volume = -(voice->adsr_volume - (voice->instrument->envelope.r_volume << 8)) / voice->adsr.r_frames;
				}
				voice->adsr.a_frames = 0;
				voice->adsr.d_frames = 0;
				voice->adsr.s_frames = 0;
			} else {
				voice->note_max_volume = 0;
			}
		} else {
			voice->note_cut_wait--;
		}
	}

	if(voice->adsr.a_frames != 0 && voice->instrument) {
		voice->adsr_volume += voice->adsr.a_volume;
		if(--voice->adsr.a_frames <= 0) {
			voice->adsr_volume = voice->instrument->envelope.a_volume << 8;
		}
	} else if(voice->adsr.d_frames != 0 && voice->instrument) {
		voice->adsr_volume += voice->adsr.d_volume;
		if(--voice->adsr.d_frames <= 0) {
			voice->adsr_volume = voice->instrument->envelope.d_volume << 8;
		}
	} else if(voice->adsr.s_frames != 0) {
		voice->adsr.s_frames--;
	} else if(voice->adsr.r_frames != 0 && voice->instrument) {
		voice->adsr_volume += voice->adsr.r_volume;
		if(--voice->adsr.r_frames <= 0) {
			voice->adsr_volume = voice->instrument->envelope.r_volume << 8;
		}
	}

	voice->note_max_volume = voice->note_max_volume + voice->volume_slide_up - voice->volume_slide_down;
	if(voice->note_max_volume < 0) {
		voice->note_max_volume = 0;
	} else if(voice->note_max_volume > 0x40) {
		voice->note_max_volume = 0x40;
	}

	if(voice->period_slide_on) {
		if(voice->period_slide_with_limit) {
			int32_t d0 = voice->period_slide_period - voice->period_slide_limit;
			int32_t d2 = voice->period_slide_speed;
			if(d0 > 0) { d2 = -d2; }
			if(d0 != 0) {
				int32_t d3 = (d0 + d2) ^ d0;
				if(d3 >= 0) {
					d0 = voice->period_slide_period + d2;
				} else {
					d0 = voice->period_slide_limit;
				}
				voice->period_slide_period = d0;
				voice->plant_period = 1;
			}
		} else {
			voice->period_slide_period += voice->period_slide_speed;
			voice->plant_period = 1;
		}
	}

	if(voice->vibrato_depth != 0) {
		if(voice->vibrato_delay <= 0) {
			voice->vibrato_period = (hively_vibrato_table[voice->vibrato_current] * voice->vibrato_depth) >> 7;
			voice->plant_period = 1;
			voice->vibrato_current = (voice->vibrato_current + voice->vibrato_speed) & 0x3f;
		} else {
			voice->vibrato_delay--;
		}
	}

	if(voice->perf_list != 0) {
		if((voice->instrument != 0) && (voice->perf_current < voice->instrument->play_list.length)) {
			int32_t signed_overflow = voice->perf_wait == 128;
			voice->perf_wait--;
			if(signed_overflow || ((int8_t)voice->perf_wait <= 0)) {
				int32_t cur = voice->perf_current++;
				voice->perf_wait = voice->perf_speed;
				struct hively_plist_entry *entry = &voice->perf_list->entries[cur];

				if(entry->waveform != 0) {
					voice->waveform = entry->waveform - 1;
					voice->new_waveform = 1;
					voice->period_perf_slide_speed = 0;
					voice->period_perf_slide_period = 0;
				}

				voice->period_perf_slide_on = 0;

				for(int32_t i = 0; i < 2; ++i) {
					hively_plist_command(s, v_idx, entry->fx[i] & 0xff, entry->fx_param[i] & 0xff);
				}

				if(entry->note != 0) {
					voice->instr_period = entry->note;
					voice->plant_period = 1;
					voice->kick_note = 1;
					voice->fixed_note = entry->fixed_note;
				}
			}
		} else {
			if(voice->perf_wait != 0) {
				voice->perf_wait--;
			} else {
				voice->period_perf_slide_speed = 0;
			}
		}
	}

	if(voice->period_perf_slide_on) {
		voice->period_perf_slide_period -= voice->period_perf_slide_speed;
		if(voice->period_perf_slide_period != 0) {
			voice->plant_period = 1;
		}
	}

	if((voice->waveform == 3 - 1) && voice->square_on && voice->instrument) {
		if(--voice->square_wait <= 0) {
			int32_t d1 = voice->square_lower_limit;
			int32_t d2 = voice->square_upper_limit;
			int32_t d3 = voice->square_pos;
			if(voice->square_init) {
				voice->square_init = 0;
				if(d3 <= d1) {
					voice->square_sliding_in = 1;
					voice->square_sign = 1;
				} else if(d3 >= d2) {
					voice->square_sliding_in = 1;
					voice->square_sign = -1;
				}
			}
			if((d1 == d3) || (d2 == d3)) {
				if(voice->square_sliding_in) {
					voice->square_sliding_in = 0;
				} else {
					voice->square_sign = -voice->square_sign;
				}
			}
			d3 += voice->square_sign;
			voice->square_pos = d3;
			voice->plant_square = 1;
			voice->square_wait = voice->instrument->square_speed;
		}
	}

	if(voice->filter_on && (--voice->filter_wait <= 0)) {
		int32_t d1 = voice->filter_lower_limit;
		int32_t d2 = voice->filter_upper_limit;
		int32_t d3 = voice->filter_pos;
		if(voice->filter_init) {
			voice->filter_init = 0;
			if(d3 <= d1) {
				voice->filter_sliding_in = 1;
				voice->filter_sign = 1;
			} else if(d3 >= d2) {
				voice->filter_sliding_in = 1;
				voice->filter_sign = -1;
			}
		}
		int32_t f_max = (voice->filter_speed < 4) ? (5 - voice->filter_speed) : 1;
		for(int32_t i = 0; i < f_max; ++i) {
			if((d1 == d3) || (d2 == d3)) {
				if(voice->filter_sliding_in) {
					voice->filter_sliding_in = 0;
				} else {
					voice->filter_sign = -voice->filter_sign;
				}
			}
			d3 += voice->filter_sign;
		}
		if(d3 < 1) { d3 = 1; }
		if(d3 > 63) { d3 = 63; }
		voice->filter_pos = d3;
		voice->new_waveform = 1;
		voice->filter_wait = voice->filter_speed - 3;
		if(voice->filter_wait < 1) {
			voice->filter_wait = 1;
		}
	}

	if(((voice->waveform == 3 - 1) || voice->plant_square) && voice->instrument) {
		int8_t *square_ptr = s->waves->squares[voice->filter_pos - 1];
		int32_t x = voice->square_pos << (5 - voice->wave_length);
		if(x > 0x20) {
			x = 0x40 - x;
			voice->square_reverse = 1;
		}
		int32_t square_offset = x > 0 ? (x - 1) << 7 : 0;
		int32_t delta = 32 >> voice->wave_length;
		s->square_waveform = voice->square_temp_buffer;
		int32_t cnt = (1 << voice->wave_length) * 4;
		for(int32_t i = 0; i < cnt; ++i) {
			voice->square_temp_buffer[i] = square_ptr[square_offset];
			square_offset += delta;
		}
		voice->new_waveform = 1;
		voice->waveform = 3 - 1;
		voice->plant_square = 0;
	}

	if(voice->waveform == 4 - 1) {
		voice->new_waveform = 1;
	}

	if(voice->ring_new_waveform) {
		if(voice->ring_waveform > 1) {
			voice->ring_waveform = 1;
		}
		voice->ring_audio_source = hively_get_waveform(s, voice->ring_waveform, 32);
		voice->ring_audio_offset = hively_offset_table[voice->wave_length];
	}

	if(voice->new_waveform) {
		int8_t *audio_source = hively_get_waveform(s, voice->waveform, voice->filter_pos);
		int32_t audio_offset = 0;
		if(voice->waveform < 3 - 1) {
			audio_offset = hively_offset_table[voice->wave_length];
		}
		if(voice->waveform == 4 - 1) {
			audio_offset = (voice->wn_random & (2 * 0x280 - 1)) & ~1u;
			voice->wn_random += 2239384;
			voice->wn_random = ((((voice->wn_random >> 8) | (voice->wn_random << 24)) + 782323) ^ 75) - 6735;
		}
		voice->audio_source = audio_source;
		voice->audio_offset = audio_offset;
	}

	if(voice->ring_audio_source != 0) {
		voice->ring_audio_period = voice->ring_base_period;
		if(!voice->ring_fixed_period) {
			if(voice->override_transpose != 1000) {
				voice->ring_audio_period += voice->override_transpose + voice->track_period - 1;
			} else {
				voice->ring_audio_period += voice->transpose + voice->track_period - 1;
			}
		}
		if(voice->ring_audio_period > 5 * 12) { voice->ring_audio_period = 5 * 12; }
		if(voice->ring_audio_period < 0) { voice->ring_audio_period = 0; }
		voice->ring_audio_period = hively_period_table[voice->ring_audio_period];
		if(!voice->ring_fixed_period) {
			voice->ring_audio_period += voice->period_slide_period;
		}
		voice->ring_audio_period += voice->period_perf_slide_period + voice->vibrato_period;
		if(voice->ring_audio_period > 0x0d60) { voice->ring_audio_period = 0x0d60; }
		if(voice->ring_audio_period < 0x0071) { voice->ring_audio_period = 0x0071; }
	}

	voice->audio_period = voice->instr_period;
	if(!voice->fixed_note) {
		if(voice->override_transpose != 1000) {
			voice->audio_period += voice->override_transpose + voice->track_period - 1;
		} else {
			voice->audio_period += voice->transpose + voice->track_period - 1;
		}
	}
	if(voice->audio_period > 5 * 12) { voice->audio_period = 5 * 12; }
	if(voice->audio_period < 0) { voice->audio_period = 0; }
	voice->audio_period = hively_period_table[voice->audio_period];
	if(!voice->fixed_note) {
		voice->audio_period += voice->period_slide_period;
	}
	voice->audio_period += voice->period_perf_slide_period + voice->vibrato_period;
	if(voice->audio_period > 0x0d60) { voice->audio_period = 0x0d60; }
	if(voice->audio_period < 0x0071) { voice->audio_period = 0x0071; }

	voice->audio_volume = (((((((voice->adsr_volume >> 8) * voice->note_max_volume) >> 6) * voice->perf_sub_volume) >> 6) * voice->track_master_volume) >> 6);
}

// [=]===^=[ hively_set_audio ]===================================================================[=]
static void hively_set_audio(struct hivelytracker_state *s, int32_t v_idx) {
	struct hively_voice *voice = &s->voices[v_idx];
	if(!voice->track_on) {
		voice->voice_volume = 0;
		return;
	}
	voice->voice_volume = voice->audio_volume;

	if(voice->plant_period) {
		voice->plant_period = 0;
		voice->voice_period = voice->audio_period;
		double freq2 = (3546895.0 * 65536.0) / (double)voice->audio_period;
		int32_t delta = (int32_t)(freq2 / (double)s->sample_rate);
		if(delta > (0x280 << 16)) {
			delta -= (0x280 << 16);
		}
		if(delta == 0) { delta = 1; }
		voice->delta = delta;
	}

	if(voice->new_waveform) {
		if(voice->waveform == 4 - 1) {
			if(voice->audio_source) {
				memcpy(voice->voice_buffer, voice->audio_source + voice->audio_offset, 0x280);
			} else {
				memset(voice->voice_buffer, 0, 0x280);
			}
		} else {
			int32_t wave_loops = (1 << (5 - voice->wave_length)) * 5;
			int32_t loop_len = 4 * (1 << voice->wave_length);
			if(voice->audio_source != 0) {
				for(int32_t i = 0; i < wave_loops; ++i) {
					memcpy(voice->voice_buffer + i * loop_len, voice->audio_source + voice->audio_offset, (size_t)loop_len);
				}
			} else {
				for(int32_t i = 0; i < wave_loops; ++i) {
					memset(voice->voice_buffer + i * loop_len, 0, (size_t)loop_len);
				}
			}
		}
		voice->voice_buffer[0x280] = voice->voice_buffer[0];
		voice->mix_source = voice->voice_buffer;
		voice->new_waveform = 0;
	}

	if(voice->ring_plant_period) {
		voice->ring_plant_period = 0;
		double freq2 = (3546895.0 * 65536.0) / (double)voice->ring_audio_period;
		int32_t delta = (int32_t)(freq2 / (double)s->sample_rate);
		if(delta > (0x280 << 16)) {
			delta -= (0x280 << 16);
		}
		if(delta == 0) { delta = 1; }
		voice->ring_delta = delta;
	}

	if(voice->ring_new_waveform) {
		int32_t wave_loops = (1 << (5 - voice->wave_length)) * 5;
		int32_t loop_len = 4 * (1 << voice->wave_length);
		if(voice->ring_audio_source != 0) {
			for(int32_t i = 0; i < wave_loops; ++i) {
				memcpy(voice->ring_voice_buffer + i * loop_len, voice->ring_audio_source + voice->ring_audio_offset, (size_t)loop_len);
			}
		} else {
			for(int32_t i = 0; i < wave_loops; ++i) {
				memset(voice->ring_voice_buffer + i * loop_len, 0, (size_t)loop_len);
			}
		}
		voice->ring_voice_buffer[0x280] = voice->ring_voice_buffer[0];
		voice->ring_mix_source = voice->ring_voice_buffer;
		voice->ring_new_waveform = 0;
	}

	voice->kick_note = 0;
}

// [=]===^=[ hively_play_irq ]====================================================================[=]
static void hively_play_irq(struct hivelytracker_state *s) {
	if(s->step_wait_frames <= 0) {
		if(s->get_new_position) {
			int32_t next_pos = s->pos_nr + 1 == s->song.position_nr ? s->song.restart : s->pos_nr + 1;
			for(int32_t i = 0; i < s->song.channels; ++i) {
				s->voices[i].track = s->song.positions[s->pos_nr].track[i];
				s->voices[i].transpose = s->song.positions[s->pos_nr].transpose[i];
				s->voices[i].next_track = s->song.positions[next_pos].track[i];
				s->voices[i].next_transpose = s->song.positions[next_pos].transpose[i];
			}
			s->get_new_position = 0;
		}

		for(int32_t i = 0; i < s->song.channels; ++i) {
			hively_process_step(s, i);
		}
		s->step_wait_frames = s->tempo;
	}

	for(int32_t i = 0; i < s->song.channels; ++i) {
		hively_process_frame(s, i);
	}

	if((s->tempo > 0) && (--s->step_wait_frames <= 0)) {
		if(!s->pattern_break) {
			s->note_nr++;
			if(s->note_nr >= s->song.track_length) {
				s->pos_jump = s->pos_nr + 1;
				s->pos_jump_note = 0;
				s->pattern_break = 1;
			}
		}
		if(s->pattern_break) {
			s->pattern_break = 0;
			s->note_nr = s->pos_jump_note;
			s->pos_jump_note = 0;
			s->pos_nr = s->pos_jump;
			s->pos_jump = 0;
			if(s->pos_nr == s->song.position_nr) {
				s->pos_nr = s->song.restart;
			}
			s->get_new_position = 1;
		}
	}

	for(int32_t i = 0; i < s->song.channels; ++i) {
		hively_set_audio(s, i);
	}
}

// [=]===^=[ hively_mix_chunk ]===================================================================[=]
//
// Mixes one tick worth of samples and ACCUMULATES into the caller's int16 stereo
// output buffer (output is 2 * frames samples). Internal sample buffers in voice
// are 0x280 bytes (with a duplicate sample at index 0x280 to act as a wrap
// guard); positions are 16.16 fixed point indices into them.
static void hively_mix_chunk(struct hivelytracker_state *s, int16_t *output, int32_t frames) {
	int32_t chans = s->song.channels;
	int8_t *src[HIVELY_MAX_CHANNELS];
	int8_t *r_src[HIVELY_MAX_CHANNELS];
	int32_t delta[HIVELY_MAX_CHANNELS];
	int32_t r_delta[HIVELY_MAX_CHANNELS];
	int32_t vol[HIVELY_MAX_CHANNELS];
	int32_t pos[HIVELY_MAX_CHANNELS];
	int32_t r_pos[HIVELY_MAX_CHANNELS];
	int32_t pan_l[HIVELY_MAX_CHANNELS];
	int32_t pan_r[HIVELY_MAX_CHANNELS];

	for(int32_t i = 0; i < chans; ++i) {
		struct hively_voice *voice = &s->voices[i];
		int32_t pan = voice->pan;
		if(pan < 0) { pan = 0; }
		if(pan > 255) { pan = 255; }
		delta[i] = voice->delta != 0 ? voice->delta : 1;
		vol[i] = voice->voice_volume;
		pos[i] = voice->sample_pos;
		src[i] = voice->mix_source;
		pan_l[i] = s->waves->panning_left[pan];
		pan_r[i] = s->waves->panning_right[pan];
		r_delta[i] = voice->ring_delta != 0 ? voice->ring_delta : 1;
		r_pos[i] = voice->ring_sample_pos;
		r_src[i] = voice->ring_mix_source;
	}

	int32_t samples = frames;
	int32_t out_offset = 0;

	while(samples > 0) {
		int32_t loops = samples;
		for(int32_t i = 0; i < chans; ++i) {
			if(pos[i] >= (0x280 << 16)) {
				pos[i] -= 0x280 << 16;
			}
			int32_t cnt = ((0x280 << 16) - pos[i] - 1) / delta[i] + 1;
			if(cnt < loops) { loops = cnt; }
			if(r_src[i] != 0) {
				if(r_pos[i] >= (0x280 << 16)) {
					r_pos[i] -= 0x280 << 16;
				}
				cnt = ((0x280 << 16) - r_pos[i] - 1) / r_delta[i] + 1;
				if(cnt < loops) { loops = cnt; }
			}
		}
		samples -= loops;

		while(loops > 0) {
			int32_t a = 0;
			int32_t b = 0;
			for(int32_t i = 0; i < chans; ++i) {
				int32_t j;
				if(r_src[i] != 0 && src[i] != 0) {
					j = ((src[i][pos[i] >> 16] * r_src[i][r_pos[i] >> 16]) >> 7) * vol[i];
					r_pos[i] += r_delta[i];
				} else if(src[i] != 0) {
					j = src[i][pos[i] >> 16] * vol[i];
				} else {
					j = 0;
				}
				a += (j * pan_l[i]) >> 7;
				b += (j * pan_r[i]) >> 7;
				pos[i] += delta[i];
			}
			a = (a * s->song.mix_gain) >> 8;
			b = (b * s->song.mix_gain) >> 8;
			if(a < -0x8000) { a = -0x8000; }
			if(a >  0x7fff) { a =  0x7fff; }
			if(b < -0x8000) { b = -0x8000; }
			if(b >  0x7fff) { b =  0x7fff; }

			int32_t la = (int32_t)output[out_offset * 2 + 0] + a;
			int32_t lb = (int32_t)output[out_offset * 2 + 1] + b;
			if(la < -0x8000) { la = -0x8000; }
			if(la >  0x7fff) { la =  0x7fff; }
			if(lb < -0x8000) { lb = -0x8000; }
			if(lb >  0x7fff) { lb =  0x7fff; }
			output[out_offset * 2 + 0] = (int16_t)la;
			output[out_offset * 2 + 1] = (int16_t)lb;

			loops--;
			out_offset++;
		}
	}

	for(int32_t i = 0; i < chans; ++i) {
		s->voices[i].sample_pos = pos[i];
		s->voices[i].ring_sample_pos = r_pos[i];
	}
}

// [=]===^=[ hively_tick ]========================================================================[=]
static void hively_tick(struct hivelytracker_state *s) {
	hively_play_irq(s);
}

// [=]===^=[ hively_cleanup ]=====================================================================[=]
static void hively_cleanup(struct hivelytracker_state *s) {
	if(!s) { return; }
	if(s->song.positions) {
		free(s->song.positions);
		s->song.positions = 0;
	}
	if(s->song.tracks) {
		for(int32_t i = 0; i <= s->song.track_nr; ++i) {
			if(s->song.tracks[i]) {
				free(s->song.tracks[i]);
			}
		}
		free(s->song.tracks);
		s->song.tracks = 0;
	}
	if(s->song.instruments) {
		for(int32_t i = 0; i <= s->song.instrument_nr; ++i) {
			if(s->song.instruments[i].play_list.entries) {
				free(s->song.instruments[i].play_list.entries);
			}
		}
		free(s->song.instruments);
		s->song.instruments = 0;
	}
	if(s->waves) {
		free(s->waves);
		s->waves = 0;
	}
}

// [=]===^=[ hivelytracker_init ]=================================================================[=]
static struct hivelytracker_state *hivelytracker_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 14 || sample_rate < 8000) {
		return 0;
	}
	int32_t mt = hively_test_module((uint8_t *)data, len);
	if(mt == HIVELY_TYPE_UNKNOWN) {
		return 0;
	}

	struct hivelytracker_state *s = (struct hivelytracker_state *)calloc(1, sizeof(struct hivelytracker_state));
	if(!s) { return 0; }
	s->module_type = mt;
	s->sample_rate = sample_rate;

	if(!hively_load(s, (uint8_t *)data, len)) {
		hively_cleanup(s);
		free(s);
		return 0;
	}

	s->waves = (struct hively_waves *)calloc(1, sizeof(struct hively_waves));
	if(!s->waves) {
		hively_cleanup(s);
		free(s);
		return 0;
	}
	hively_gen_waves(s->waves);

	hively_initialize_sound(s, 0);

	int32_t tick_hz = 50 * (s->song.speed_multiplier > 0 ? s->song.speed_multiplier : 1);
	s->tick_samples = sample_rate / tick_hz;
	if(s->tick_samples < 1) { s->tick_samples = 1; }
	s->tick_offset = 0;

	return s;
}

// [=]===^=[ hivelytracker_free ]=================================================================[=]
static void hivelytracker_free(struct hivelytracker_state *s) {
	if(!s) { return; }
	hively_cleanup(s);
	free(s);
}

// [=]===^=[ hivelytracker_get_audio ]============================================================[=]
static void hivelytracker_get_audio(struct hivelytracker_state *s, int16_t *output, int32_t frames) {
	while(frames > 0) {
		int32_t remain = s->tick_samples - s->tick_offset;
		if(remain > frames) { remain = frames; }
		hively_mix_chunk(s, output, remain);
		output += remain * 2;
		s->tick_offset += remain;
		frames -= remain;
		if(s->tick_offset >= s->tick_samples) {
			s->tick_offset = 0;
			hively_tick(s);
		}
	}
}

// [=]===^=[ hivelytracker_api_init ]=============================================================[=]
static void *hivelytracker_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return hivelytracker_init(data, len, sample_rate);
}

// [=]===^=[ hivelytracker_api_free ]=============================================================[=]
static void hivelytracker_api_free(void *state) {
	hivelytracker_free((struct hivelytracker_state *)state);
}

// [=]===^=[ hivelytracker_api_get_audio ]========================================================[=]
static void hivelytracker_api_get_audio(void *state, int16_t *output, int32_t frames) {
	hivelytracker_get_audio((struct hivelytracker_state *)state, output, frames);
}

static const char *hivelytracker_extensions[] = { "hvl", "ahx", "thx", 0 };

static struct player_api hivelytracker_api = {
	"HivelyTracker",
	hivelytracker_extensions,
	hivelytracker_api_init,
	hivelytracker_api_free,
	hivelytracker_api_get_audio,
	0,
};
