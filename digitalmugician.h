// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Digital Mugician / Digital Mugician 2 replayer, ported from NostalgicPlayer's
// C# implementation. Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
// DMU2 modules use 7 logical voices that are folded back onto 4 Paula channels
// (the original NostalgicPlayer also routes them through 4 hard panned channels).
//
// Public API:
//   struct digitalmugician_state *digitalmugician_init(void *data, uint32_t len, int32_t sample_rate);
//   void digitalmugician_free(struct digitalmugician_state *s);
//   void digitalmugician_get_audio(struct digitalmugician_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define DMU_TICK_HZ           50
#define DMU_PERIOD_START      7
#define DMU_NUM_PERIODS       (16 * 64 + 7)
#define DMU_WAVEFORM_SIZE     128

enum {
	DMU_TYPE_UNKNOWN = 0,
	DMU_TYPE_DMU1    = 1,
	DMU_TYPE_DMU2    = 2,
};

enum {
	DMU_EFF_NONE = 0,
	DMU_EFF_PITCH_BEND,
	DMU_EFF_NO_INSTRUMENT_EFFECT,
	DMU_EFF_NO_INSTRUMENT_VOLUME,
	DMU_EFF_NO_INSTRUMENT_EFFECT_AND_VOLUME,
	DMU_EFF_PATTERN_LENGTH,
	DMU_EFF_SONG_SPEED,
	DMU_EFF_FILTER_ON,
	DMU_EFF_FILTER_OFF,
	DMU_EFF_SWITCH_FILTER,
	DMU_EFF_NO_DMA,
	DMU_EFF_ARPEGGIO,
	DMU_EFF_NO_WANDER,
	DMU_EFF_SHUFFLE,
};

enum {
	DMU_INSTEFF_NONE = 0,
	DMU_INSTEFF_FILTER,
	DMU_INSTEFF_MIXING,
	DMU_INSTEFF_SCRLEFT,
	DMU_INSTEFF_SCRRIGHT,
	DMU_INSTEFF_UPSAMPLE,
	DMU_INSTEFF_DOWNSAMPLE,
	DMU_INSTEFF_NEGATE,
	DMU_INSTEFF_MADMIX1,
	DMU_INSTEFF_ADDITION,
	DMU_INSTEFF_FILTER2,
	DMU_INSTEFF_MORPHING,
	DMU_INSTEFF_MORPHF,
	DMU_INSTEFF_FILTER3,
	DMU_INSTEFF_POLYGATE,
	DMU_INSTEFF_COLGATE,
};

struct dmu_instrument {
	uint8_t waveform_number;
	uint16_t loop_length;
	uint8_t volume;
	uint8_t volume_speed;
	uint8_t arpeggio_number;
	uint8_t pitch;
	uint8_t effect_index;
	uint8_t delay;
	uint8_t finetune;
	uint8_t pitch_loop;
	uint8_t pitch_speed;
	uint8_t effect;
	uint8_t source_wave1;
	uint8_t source_wave2;
	uint8_t effect_speed;
	uint8_t volume_loop;
};

struct dmu_sample {
	int8_t *data;
	uint32_t length;
	int32_t loop_start;
};

struct dmu_track_row {
	uint8_t note;
	uint8_t instrument;
	uint8_t effect;
	uint8_t effect_param;
};

struct dmu_track {
	struct dmu_track_row rows[64];
};

struct dmu_sequence {
	uint8_t track_number;
	int8_t transpose;
};

struct dmu_subsong {
	uint8_t loop_song;
	uint8_t loop_position;
	uint8_t song_speed;
	uint8_t number_of_sequences;
};

struct dmu_voice_info {
	uint16_t track;
	int16_t transpose;
	uint16_t last_note;
	uint16_t last_instrument;
	uint8_t last_effect;
	uint16_t last_effect_param;
	uint16_t finetune;
	uint16_t note_period;
	uint16_t pitch_bend_end_note;
	uint16_t pitch_bend_end_period;
	int16_t current_pitch_bend_period;
	uint16_t pitch_index;
	uint16_t arpeggio_index;
	uint16_t volume_index;
	uint16_t volume_speed_counter;
	uint8_t instrument_delay;
	uint8_t instrument_effect_speed;
};

struct digitalmugician_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;
	int8_t *sample_pool;          // copies of sample data, owned by us
	uint32_t sample_pool_len;

	int32_t module_type;
	int32_t number_of_channels;

	uint16_t number_of_tracks;
	uint32_t number_of_instruments;
	uint32_t number_of_waveforms;
	uint32_t number_of_samples;

	uint32_t subsong_sequence_length[8];
	struct dmu_subsong subsongs[8];
	struct dmu_sequence *subsong_sequences[8]; // [4 * length], indexed [k*length + j]

	struct dmu_track *tracks;
	uint8_t arpeggios[8][32];

	struct dmu_instrument *instruments;
	int8_t (*waveforms)[DMU_WAVEFORM_SIZE]; // numberOfWaveforms entries
	struct dmu_sample *samples;

	int32_t subsong_mapping[8];
	int32_t subsong_mapping_count;

	int32_t subsong_number;
	int32_t real_subsong;
	struct dmu_subsong *current_subsong;
	struct dmu_sequence *current_sequence;  // 4 * length
	struct dmu_sequence *current_sequence2;
	uint32_t current_sequence_length;

	struct dmu_voice_info voice_info[7];

	uint16_t speed;
	uint16_t current_speed;
	uint16_t last_shown_speed;
	uint8_t new_pattern;
	uint8_t new_row;
	uint16_t current_position;
	uint16_t song_length;
	uint16_t current_row;
	uint16_t pattern_length;

	uint8_t ch_tab[4];
	int32_t ch_tab_index;

	uint8_t end_reached;
};

// [=]===^=[ dmu_periods ]========================================================================[=]
static uint16_t dmu_periods[DMU_NUM_PERIODS] = {
	4825, 4554, 4299, 4057, 3820, 3615, 3412,
	3220, 3040, 2869, 2708, 2556, 2412, 2277, 2149, 2029, 1915, 1807,
	1706, 1610, 1520, 1434, 1354, 1278, 1206, 1139, 1075, 1014,  957,  904,
	 854,  805,  760,  717,  677,  639,  603,  569,  537,  507,  479,  452,
	 426,  403,  380,  359,  338,  319,  302,  285,  269,  254,  239,  226,
	 213,  201,  190,  179,  169,  160,  151,  142,  134,  127,
	4842, 4571, 4314, 4072, 3843, 3628,
	3424, 3232, 3051, 2879, 2718, 2565, 2421, 2285, 2157, 2036, 1922, 1814,
	1712, 1616, 1525, 1440, 1359, 1283, 1211, 1143, 1079, 1018,  961,  907,
	 856,  808,  763,  720,  679,  641,  605,  571,  539,  509,  480,  453,
	 428,  404,  381,  360,  340,  321,  303,  286,  270,  254,  240,  227,
	 214,  202,  191,  180,  170,  160,  151,  143,  135,  127,
	4860, 4587, 4330, 4087, 3857, 3641,
	3437, 3244, 3062, 2890, 2728, 2574, 2430, 2294, 2165, 2043, 1929, 1820,
	1718, 1622, 1531, 1445, 1364, 1287, 1215, 1147, 1082, 1022,  964,  910,
	 859,  811,  765,  722,  682,  644,  607,  573,  541,  511,  482,  455,
	 430,  405,  383,  361,  341,  322,  304,  287,  271,  255,  241,  228,
	 215,  203,  191,  181,  170,  161,  152,  143,  135,  128,
	4878, 4604, 4345, 4102, 3871, 3654,
	3449, 3255, 3073, 2900, 2737, 2584, 2439, 2302, 2173, 2051, 1936, 1827,
	1724, 1628, 1536, 1450, 1369, 1292, 1219, 1151, 1086, 1025,  968,  914,
	 862,  814,  768,  725,  684,  646,  610,  575,  543,  513,  484,  457,
	 431,  407,  384,  363,  342,  323,  305,  288,  272,  256,  242,  228,
	 216,  203,  192,  181,  171,  161,  152,  144,  136,  128,
	4895, 4620, 4361, 4116, 3885, 3667,
	3461, 3267, 3084, 2911, 2747, 2593, 2448, 2310, 2181, 2058, 1943, 1834,
	1731, 1634, 1542, 1455, 1374, 1297, 1224, 1155, 1090, 1029,  971,  917,
	 865,  817,  771,  728,  687,  648,  612,  578,  545,  515,  486,  458,
	 433,  408,  385,  364,  343,  324,  306,  289,  273,  257,  243,  229,
	 216,  204,  193,  182,  172,  162,  153,  144,  136,  129,
	4913, 4637, 4377, 4131, 3899, 3681,
	3474, 3279, 3095, 2921, 2757, 2603, 2456, 2319, 2188, 2066, 1950, 1840,
	1737, 1639, 1547, 1461, 1379, 1301, 1228, 1159, 1094, 1033,  975,  920,
	 868,  820,  774,  730,  689,  651,  614,  580,  547,  516,  487,  460,
	 434,  410,  387,  365,  345,  325,  307,  290,  274,  258,  244,  230,
	 217,  205,  193,  183,  172,  163,  154,  145,  137,  129,
	4931, 4654, 4393, 4146, 3913, 3694,
	3486, 3291, 3106, 2932, 2767, 2612, 2465, 2327, 2196, 2073, 1957, 1847,
	1743, 1645, 1553, 1466, 1384, 1306, 1233, 1163, 1098, 1037,  978,  923,
	 872,  823,  777,  733,  692,  653,  616,  582,  549,  518,  489,  462,
	 436,  411,  388,  366,  346,  326,  308,  291,  275,  259,  245,  231,
	 218,  206,  194,  183,  173,  163,  154,  145,  137,  130,
	4948, 4671, 4409, 4161, 3928, 3707,
	3499, 3303, 3117, 2942, 2777, 2621, 2474, 2335, 2204, 2081, 1964, 1854,
	1750, 1651, 1559, 1471, 1389, 1311, 1237, 1168, 1102, 1040,  982,  927,
	 875,  826,  779,  736,  694,  655,  619,  584,  551,  520,  491,  463,
	 437,  413,  390,  368,  347,  328,  309,  292,  276,  260,  245,  232,
	 219,  206,  195,  184,  174,  164,  155,  146,  138,  130,
	4966, 4688, 4425, 4176, 3942, 3721,
	3512, 3315, 3129, 2953, 2787, 2631, 2483, 2344, 2212, 2088, 1971, 1860,
	1756, 1657, 1564, 1477, 1394, 1315, 1242, 1172, 1106, 1044,  985,  930,
	 878,  829,  782,  738,  697,  658,  621,  586,  553,  522,  493,  465,
	 439,  414,  391,  369,  348,  329,  310,  293,  277,  261,  246,  233,
	 219,  207,  196,  185,  174,  164,  155,  146,  138,  131,
	4984, 4705, 4441, 4191, 3956, 3734,
	3524, 3327, 3140, 2964, 2797, 2640, 2492, 2352, 2220, 2096, 1978, 1867,
	1762, 1663, 1570, 1482, 1399, 1320, 1246, 1176, 1110, 1048,  989,  934,
	 881,  832,  785,  741,  699,  660,  623,  588,  555,  524,  495,  467,
	 441,  416,  392,  370,  350,  330,  312,  294,  278,  262,  247,  233,
	 220,  208,  196,  185,  175,  165,  156,  147,  139,  131,
	5002, 4722, 4457, 4206, 3970, 3748,
	3537, 3339, 3151, 2974, 2807, 2650, 2501, 2361, 2228, 2103, 1985, 1874,
	1769, 1669, 1576, 1487, 1404, 1325, 1251, 1180, 1114, 1052,  993,  937,
	 884,  835,  788,  744,  702,  662,  625,  590,  557,  526,  496,  468,
	 442,  417,  394,  372,  351,  331,  313,  295,  279,  263,  248,  234,
	 221,  209,  197,  186,  175,  166,  156,  148,  139,  131,
	5020, 4739, 4473, 4222, 3985, 3761,
	3550, 3351, 3163, 2985, 2818, 2659, 2510, 2369, 2236, 2111, 1992, 1881,
	1775, 1675, 1581, 1493, 1409, 1330, 1255, 1185, 1118, 1055,  996,  940,
	 887,  838,  791,  746,  704,  665,  628,  592,  559,  528,  498,  470,
	 444,  419,  395,  373,  352,  332,  314,  296,  280,  264,  249,  235,
	 222,  209,  198,  187,  176,  166,  157,  148,  140,  132,
	5039, 4756, 4489, 4237, 3999, 3775,
	3563, 3363, 3174, 2996, 2828, 2669, 2519, 2378, 2244, 2118, 2000, 1887,
	1781, 1681, 1587, 1498, 1414, 1335, 1260, 1189, 1122, 1059, 1000,  944,
	 891,  841,  794,  749,  707,  667,  630,  594,  561,  530,  500,  472,
	 445,  420,  397,  374,  353,  334,  315,  297,  281,  265,  250,  236,
	 223,  210,  198,  187,  177,  167,  157,  149,  140,  132,
	5057, 4773, 4505, 4252, 4014, 3788,
	3576, 3375, 3186, 3007, 2838, 2679, 2528, 2387, 2253, 2126, 2007, 1894,
	1788, 1688, 1593, 1503, 1419, 1339, 1264, 1193, 1126, 1063, 1003,  947,
	 894,  844,  796,  752,  710,  670,  632,  597,  563,  532,  502,  474,
	 447,  422,  398,  376,  355,  335,  316,  298,  282,  266,  251,  237,
	 223,  211,  199,  188,  177,  167,  158,  149,  141,  133,
	5075, 4790, 4521, 4268, 4028, 3802,
	3589, 3387, 3197, 3018, 2848, 2688, 2538, 2395, 2261, 2134, 2014, 1901,
	1794, 1694, 1599, 1509, 1424, 1344, 1269, 1198, 1130, 1067, 1007,  951,
	 897,  847,  799,  754,  712,  672,  634,  599,  565,  533,  504,  475,
	 449,  423,  400,  377,  356,  336,  317,  299,  283,  267,  252,  238,
	 224,  212,  200,  189,  178,  168,  159,  150,  141,  133,
	5093, 4808, 4538, 4283, 4043, 3816,
	3602, 3399, 3209, 3029, 2859, 2698, 2547, 2404, 2269, 2142, 2021, 1908,
	1801, 1700, 1604, 1514, 1429, 1349, 1273, 1202, 1134, 1071, 1011,  954,
	 900,  850,  802,  757,  715,  675,  637,  601,  567,  535,  505,  477,
	 450,  425,  401,  379,  357,  337,  318,  300,  284,  268,  253,  238,
	 225,  212,  201,  189,  179,  169,  159,  150,  142,  134
};

// [=]===^=[ dmu_read_u32_be ]====================================================================[=]
static uint32_t dmu_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ dmu_read_i32_be ]====================================================================[=]
static int32_t dmu_read_i32_be(uint8_t *p) {
	return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

// [=]===^=[ dmu_read_u16_be ]====================================================================[=]
static uint16_t dmu_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

// [=]===^=[ dmu_cleanup ]========================================================================[=]
static void dmu_cleanup(struct digitalmugician_state *s) {
	if(!s) {
		return;
	}
	for(int32_t i = 0; i < 8; ++i) {
		free(s->subsong_sequences[i]);
		s->subsong_sequences[i] = 0;
	}
	free(s->tracks); s->tracks = 0;
	free(s->instruments); s->instruments = 0;
	free(s->waveforms); s->waveforms = 0;
	free(s->samples); s->samples = 0;
	free(s->sample_pool); s->sample_pool = 0;
}

// [=]===^=[ dmu_identify ]=======================================================================[=]
static int32_t dmu_identify(uint8_t *data, uint32_t len) {
	if(len < 204) {
		return DMU_TYPE_UNKNOWN;
	}
	if(memcmp(data, " MUGICIAN/SOFTEYES 1990 ", 24) == 0) {
		return DMU_TYPE_DMU1;
	}
	if(memcmp(data, " MUGICIAN2/SOFTEYES 1990", 24) == 0) {
		return DMU_TYPE_DMU2;
	}
	return DMU_TYPE_UNKNOWN;
}

// [=]===^=[ dmu_get_period ]=====================================================================[=]
static uint16_t dmu_get_period(uint16_t note, int16_t transpose, uint16_t finetune) {
	int32_t index = (int32_t)DMU_PERIOD_START + (int32_t)note + (int32_t)transpose + (int32_t)finetune * 64;
	if(index < 0 || index >= DMU_NUM_PERIODS) {
		return 0;
	}
	return dmu_periods[index];
}

// [=]===^=[ dmu_change_speed ]===================================================================[=]
static void dmu_change_speed(struct digitalmugician_state *s, uint16_t new_speed) {
	if(new_speed != s->last_shown_speed) {
		s->last_shown_speed = new_speed;
		s->current_speed = new_speed;
	}
}

// [=]===^=[ dmu_inst_effect_filter ]=============================================================[=]
static void dmu_inst_effect_filter(int8_t *wave) {
	for(int32_t i = 0; i < 127; ++i) {
		wave[i] = (int8_t)(((int32_t)wave[i] + (int32_t)wave[i + 1]) / 2);
	}
}

// [=]===^=[ dmu_inst_effect_mixing ]=============================================================[=]
static void dmu_inst_effect_mixing(struct digitalmugician_state *s, int8_t *wave, struct dmu_instrument *inst) {
	int8_t *src1 = s->waveforms[inst->source_wave1];
	int8_t *src2 = s->waveforms[inst->source_wave2];
	int32_t index = inst->effect_index;
	inst->effect_index = (uint8_t)((inst->effect_index + 1) & 0x7f);
	for(uint32_t i = 0; i < inst->loop_length; ++i) {
		wave[i] = (int8_t)(((int32_t)src1[i] + (int32_t)src2[index]) / 2);
		index = (index + 1) & 0x7f;
	}
}

// [=]===^=[ dmu_inst_effect_scrleft ]============================================================[=]
static void dmu_inst_effect_scrleft(int8_t *wave) {
	int8_t first = wave[0];
	for(int32_t i = 0; i < 127; ++i) {
		wave[i] = wave[i + 1];
	}
	wave[127] = first;
}

// [=]===^=[ dmu_inst_effect_scrright ]===========================================================[=]
static void dmu_inst_effect_scrright(int8_t *wave) {
	int8_t last = wave[127];
	for(int32_t i = 126; i >= 0; --i) {
		wave[i + 1] = wave[i];
	}
	wave[0] = last;
}

// [=]===^=[ dmu_inst_effect_upsampling ]=========================================================[=]
static void dmu_inst_effect_upsampling(int8_t *wave) {
	int32_t src = 0;
	int32_t dst = 0;
	for(int32_t i = 0; i < 64; ++i) {
		wave[dst++] = wave[src];
		src += 2;
	}
	src = 0;
	for(int32_t i = 0; i < 64; ++i) {
		wave[dst++] = wave[src++];
	}
}

// [=]===^=[ dmu_inst_effect_downsampling ]=======================================================[=]
static void dmu_inst_effect_downsampling(int8_t *wave) {
	int32_t src = 64;
	int32_t dst = 128;
	for(int32_t i = 0; i < 64; ++i) {
		--dst;
		--src;
		wave[dst] = wave[src];
		--dst;
		wave[dst] = wave[src];
	}
}

// [=]===^=[ dmu_inst_effect_negate ]=============================================================[=]
static void dmu_inst_effect_negate(int8_t *wave, struct dmu_instrument *inst) {
	int32_t index = inst->effect_index;
	wave[index] = (int8_t)-wave[index];
	inst->effect_index++;
	if(inst->effect_index >= inst->loop_length) {
		inst->effect_index = 0;
	}
}

// [=]===^=[ dmu_inst_effect_madmix1 ]============================================================[=]
static void dmu_inst_effect_madmix1(struct digitalmugician_state *s, int8_t *wave, struct dmu_instrument *inst) {
	inst->effect_index = (uint8_t)((inst->effect_index + 1) & 0x7f);
	int8_t *src2 = s->waveforms[inst->source_wave2];
	int8_t increment = src2[inst->effect_index];
	int8_t add = 3;
	for(uint32_t i = 0; i < inst->loop_length; ++i) {
		wave[i] = (int8_t)(wave[i] + add);
		add = (int8_t)(add + increment);
	}
}

// [=]===^=[ dmu_inst_effect_addition ]===========================================================[=]
static void dmu_inst_effect_addition(struct digitalmugician_state *s, int8_t *wave, struct dmu_instrument *inst) {
	int8_t *src2 = s->waveforms[inst->source_wave2];
	for(uint32_t i = 0; i < inst->loop_length; ++i) {
		wave[i] = (int8_t)((int32_t)src2[i] + (int32_t)wave[i]);
	}
}

// [=]===^=[ dmu_inst_effect_filter2 ]============================================================[=]
static void dmu_inst_effect_filter2(int8_t *wave) {
	for(int32_t i = 0; i < 126; ++i) {
		wave[i + 1] = (int8_t)(((int32_t)wave[i] * 3 + (int32_t)wave[i + 2]) / 4);
	}
}

// [=]===^=[ dmu_inst_effect_morphing ]===========================================================[=]
static void dmu_inst_effect_morphing(struct digitalmugician_state *s, int8_t *wave, struct dmu_instrument *inst) {
	int8_t *src1 = s->waveforms[inst->source_wave1];
	int8_t *src2 = s->waveforms[inst->source_wave2];
	inst->effect_index = (uint8_t)((inst->effect_index + 1) & 0x7f);
	int32_t mul1, mul2;
	if(inst->effect_index < 64) {
		mul1 = inst->effect_index;
		mul2 = (inst->effect_index ^ 0xff) & 0x3f;
	} else {
		mul1 = 127 - inst->effect_index;
		mul2 = (mul1 ^ 0xff) & 0x3f;
	}
	for(uint32_t i = 0; i < inst->loop_length; ++i) {
		wave[i] = (int8_t)(((int32_t)src1[i] * mul1 + (int32_t)src2[i] * mul2) / 64);
	}
}

// [=]===^=[ dmu_inst_effect_morphf ]=============================================================[=]
static void dmu_inst_effect_morphf(struct digitalmugician_state *s, int8_t *wave, struct dmu_instrument *inst) {
	int8_t *src1 = s->waveforms[inst->source_wave1];
	int8_t *src2 = s->waveforms[inst->source_wave2];
	inst->effect_index = (uint8_t)((inst->effect_index + 1) & 0x1f);
	int32_t mul1, mul2;
	if(inst->effect_index < 16) {
		mul1 = inst->effect_index;
		mul2 = (inst->effect_index ^ 0xff) & 0x0f;
	} else {
		mul1 = 31 - inst->effect_index;
		mul2 = (mul1 ^ 0xff) & 0x0f;
	}
	for(uint32_t i = 0; i < inst->loop_length; ++i) {
		wave[i] = (int8_t)(((int32_t)src1[i] * mul1 + (int32_t)src2[i] * mul2) / 16);
	}
}

// [=]===^=[ dmu_inst_effect_filter3 ]============================================================[=]
static void dmu_inst_effect_filter3(int8_t *wave) {
	for(int32_t i = 0; i < 126; ++i) {
		wave[i + 1] = (int8_t)(((int32_t)wave[i] + (int32_t)wave[i + 2]) / 2);
	}
}

// [=]===^=[ dmu_inst_effect_polygate ]===========================================================[=]
static void dmu_inst_effect_polygate(int8_t *wave, struct dmu_instrument *inst) {
	int32_t index = inst->effect_index;
	wave[index] = (int8_t)-wave[index];
	index = (index + inst->source_wave2) & ((int32_t)inst->loop_length - 1);
	wave[index] = (int8_t)-wave[index];
	inst->effect_index++;
	if(inst->effect_index >= inst->loop_length) {
		inst->effect_index = 0;
	}
}

// [=]===^=[ dmu_inst_effect_colgate ]============================================================[=]
static void dmu_inst_effect_colgate(int8_t *wave, struct dmu_instrument *inst) {
	dmu_inst_effect_filter(wave);
	inst->effect_index++;
	if(inst->effect_index == inst->source_wave2) {
		inst->effect_index = 0;
		dmu_inst_effect_upsampling(wave);
	}
}

// [=]===^=[ dmu_dispatch_inst_effect ]===========================================================[=]
static void dmu_dispatch_inst_effect(struct digitalmugician_state *s, int8_t *wave, struct dmu_instrument *inst) {
	switch(inst->effect) {
		case DMU_INSTEFF_FILTER: {
			dmu_inst_effect_filter(wave);
			break;
		}

		case DMU_INSTEFF_MIXING: {
			dmu_inst_effect_mixing(s, wave, inst);
			break;
		}

		case DMU_INSTEFF_SCRLEFT: {
			dmu_inst_effect_scrleft(wave);
			break;
		}

		case DMU_INSTEFF_SCRRIGHT: {
			dmu_inst_effect_scrright(wave);
			break;
		}

		case DMU_INSTEFF_UPSAMPLE: {
			dmu_inst_effect_upsampling(wave);
			break;
		}

		case DMU_INSTEFF_DOWNSAMPLE: {
			dmu_inst_effect_downsampling(wave);
			break;
		}

		case DMU_INSTEFF_NEGATE: {
			dmu_inst_effect_negate(wave, inst);
			break;
		}

		case DMU_INSTEFF_MADMIX1: {
			dmu_inst_effect_madmix1(s, wave, inst);
			break;
		}

		case DMU_INSTEFF_ADDITION: {
			dmu_inst_effect_addition(s, wave, inst);
			break;
		}

		case DMU_INSTEFF_FILTER2: {
			dmu_inst_effect_filter2(wave);
			break;
		}

		case DMU_INSTEFF_MORPHING: {
			dmu_inst_effect_morphing(s, wave, inst);
			break;
		}

		case DMU_INSTEFF_MORPHF: {
			dmu_inst_effect_morphf(s, wave, inst);
			break;
		}

		case DMU_INSTEFF_FILTER3: {
			dmu_inst_effect_filter3(wave);
			break;
		}

		case DMU_INSTEFF_POLYGATE: {
			dmu_inst_effect_polygate(wave, inst);
			break;
		}

		case DMU_INSTEFF_COLGATE: {
			dmu_inst_effect_colgate(wave, inst);
			break;
		}

		default: {
			break;
		}
	}
}

// [=]===^=[ dmu_play_note ]======================================================================[=]
static void dmu_play_note(struct digitalmugician_state *s, int32_t channel_number) {
	struct dmu_voice_info *voice = &s->voice_info[channel_number];
	int32_t paula_ch = channel_number;
	if(paula_ch >= PAULA_NUM_CHANNELS) {
		paula_ch = PAULA_NUM_CHANNELS - 1;
	}

	if(s->new_pattern) {
		struct dmu_sequence *seq;
		if((s->module_type == DMU_TYPE_DMU1) || (channel_number < 3)) {
			seq = &s->current_sequence[channel_number * (int32_t)s->current_sequence_length + s->current_position];
		} else {
			seq = &s->current_sequence2[(channel_number - 3) * (int32_t)s->current_sequence_length + s->current_position];
		}
		voice->track = seq->track_number;
		voice->transpose = seq->transpose;
	}

	if(s->new_row) {
		struct dmu_track *track = &s->tracks[voice->track];
		struct dmu_track_row *row = &track->rows[s->current_row];

		if(row->note != 0) {
			if(row->effect != (DMU_EFF_NO_WANDER + 62)) {
				voice->last_note = row->note;
				if(row->instrument != 0) {
					voice->last_instrument = (uint16_t)(row->instrument - 1);
				}
			}
			voice->last_instrument &= 0x3f;
			voice->last_effect = (row->effect < 64) ? (uint8_t)DMU_EFF_PITCH_BEND : (uint8_t)(row->effect - 62);
			voice->last_effect_param = row->effect_param;

			if(voice->last_instrument < s->number_of_instruments) {
				struct dmu_instrument *inst = &s->instruments[voice->last_instrument];
				voice->finetune = inst->finetune;

				if(voice->last_effect == DMU_EFF_NO_WANDER) {
					voice->pitch_bend_end_note = row->note;
					voice->pitch_bend_end_period = dmu_get_period(voice->pitch_bend_end_note, voice->transpose, voice->finetune);
				} else {
					voice->pitch_bend_end_note = row->effect;
					if(voice->last_effect == DMU_EFF_PITCH_BEND) {
						voice->pitch_bend_end_period = dmu_get_period(voice->pitch_bend_end_note, voice->transpose, voice->finetune);
					}
				}

				if(voice->last_effect == DMU_EFF_ARPEGGIO) {
					inst->arpeggio_number = (uint8_t)(voice->last_effect_param & 7);
				}

				uint8_t waveform = inst->waveform_number;

				if(voice->last_effect != DMU_EFF_NO_WANDER) {
					if(waveform >= 32) {
						struct dmu_sample *sample = &s->samples[waveform - 32];
						paula_play_sample(&s->paula, paula_ch, sample->data, sample->length);
						if(sample->loop_start >= 0) {
							paula_set_loop(&s->paula, paula_ch, (uint32_t)sample->loop_start, sample->length - (uint32_t)sample->loop_start);
						}
					} else {
						int8_t *wavedata = s->waveforms[waveform];
						if(voice->last_effect != DMU_EFF_NO_DMA) {
							paula_play_sample(&s->paula, paula_ch, wavedata, inst->loop_length);
						} else {
							paula_queue_sample(&s->paula, paula_ch, wavedata, 0, inst->loop_length);
						}
						paula_set_loop(&s->paula, paula_ch, 0, inst->loop_length);

						if(s->module_type == DMU_TYPE_DMU1) {
							if((inst->effect != DMU_INSTEFF_NONE) && (voice->last_effect != DMU_EFF_NO_INSTRUMENT_EFFECT) && (voice->last_effect != DMU_EFF_NO_INSTRUMENT_EFFECT_AND_VOLUME)) {
								int8_t *source = s->waveforms[inst->source_wave1];
								memcpy(wavedata, source, DMU_WAVEFORM_SIZE);
								inst->effect_index = 0;
								voice->instrument_effect_speed = inst->effect_speed;
							}
						}
					}
				}

				if((voice->last_effect != DMU_EFF_NO_INSTRUMENT_VOLUME) && (voice->last_effect != DMU_EFF_NO_INSTRUMENT_EFFECT_AND_VOLUME) && (voice->last_effect != DMU_EFF_NO_WANDER)) {
					voice->volume_speed_counter = 1;
					voice->volume_index = 0;
				}

				voice->current_pitch_bend_period = 0;
				voice->instrument_delay = inst->delay;
				voice->pitch_index = 0;
				voice->arpeggio_index = 0;
			}
		}
	}

	switch(voice->last_effect) {
		case DMU_EFF_PATTERN_LENGTH: {
			if((voice->last_effect_param != 0) && (voice->last_effect_param <= 64)) {
				s->pattern_length = voice->last_effect_param;
			}
			break;
		}

		case DMU_EFF_SONG_SPEED: {
			uint8_t parm = (uint8_t)(voice->last_effect_param & 0x0f);
			uint8_t new_spd = (uint8_t)((parm << 4) | parm);
			if((parm != 0) && (parm <= 15)) {
				dmu_change_speed(s, new_spd);
			}
			break;
		}

		case DMU_EFF_FILTER_ON: {
			break;
		}

		case DMU_EFF_FILTER_OFF: {
			break;
		}

		case DMU_EFF_SHUFFLE: {
			voice->last_effect = DMU_EFF_NONE;
			if(((voice->last_effect_param & 0x0f) != 0) && ((voice->last_effect_param & 0xf0) != 0)) {
				dmu_change_speed(s, voice->last_effect_param);
			}
			break;
		}

		default: {
			break;
		}
	}
}

// [=]===^=[ dmu_do_effects ]=====================================================================[=]
static void dmu_do_effects(struct digitalmugician_state *s, int32_t channel_number) {
	struct dmu_voice_info *voice = &s->voice_info[channel_number];
	int32_t paula_ch = channel_number;
	if(paula_ch >= PAULA_NUM_CHANNELS) {
		paula_ch = PAULA_NUM_CHANNELS - 1;
	}

	if(voice->last_instrument < s->number_of_instruments) {
		struct dmu_instrument *inst = &s->instruments[voice->last_instrument];

		if((inst->effect != DMU_INSTEFF_NONE) && (inst->waveform_number < 32)) {
			uint8_t inst_num = (uint8_t)(voice->last_instrument + 1);
			if((s->ch_tab[0] != inst_num) && (s->ch_tab[1] != inst_num) && (s->ch_tab[2] != inst_num) && (s->ch_tab[3] != inst_num)) {
				if(s->ch_tab_index < 4) {
					s->ch_tab[s->ch_tab_index++] = inst_num;
				}
				if(voice->instrument_effect_speed == 0) {
					voice->instrument_effect_speed = inst->effect_speed;
					int8_t *wavedata = s->waveforms[inst->waveform_number];
					dmu_dispatch_inst_effect(s, wavedata, inst);
				} else {
					voice->instrument_effect_speed--;
				}
			}
		}

		if(voice->volume_speed_counter != 0) {
			voice->volume_speed_counter--;
			if(voice->volume_speed_counter == 0) {
				voice->volume_speed_counter = inst->volume_speed;
				voice->volume_index++;
				voice->volume_index &= 0x7f;
				if((voice->volume_index == 0) && !inst->volume_loop) {
					voice->volume_speed_counter = 0;
				} else {
					int32_t raw = (int32_t)s->waveforms[inst->volume][voice->volume_index];
					int32_t volume = -(int32_t)(int8_t)(raw + 0x81);
					volume = (volume & 0xff) / 4;
					paula_set_volume(&s->paula, paula_ch, (uint16_t)volume);
				}
			}
		}

		int32_t note = voice->last_note;

		if(inst->arpeggio_number != 0) {
			uint8_t *arp = s->arpeggios[inst->arpeggio_number];
			note += (int8_t)arp[voice->arpeggio_index];
			voice->arpeggio_index++;
			voice->arpeggio_index &= 0x1f;
		}

		uint16_t period = dmu_get_period((uint16_t)note, voice->transpose, voice->finetune);
		voice->note_period = period;

		if((voice->last_effect == DMU_EFF_NO_WANDER) || (voice->last_effect == DMU_EFF_PITCH_BEND)) {
			int32_t period_increment = -(int32_t)(int8_t)voice->last_effect_param;
			voice->current_pitch_bend_period = (int16_t)(voice->current_pitch_bend_period + period_increment);
			voice->note_period = (uint16_t)(voice->note_period + voice->current_pitch_bend_period);

			if(voice->last_effect_param != 0) {
				if(period_increment < 0) {
					if(voice->note_period <= voice->pitch_bend_end_period) {
						voice->current_pitch_bend_period = (int16_t)(voice->pitch_bend_end_period - period);
						voice->last_effect_param = 0;
					}
				} else {
					if(voice->note_period >= voice->pitch_bend_end_period) {
						voice->current_pitch_bend_period = (int16_t)(voice->pitch_bend_end_period - period);
						voice->last_effect_param = 0;
					}
				}
			}
		}

		if(inst->pitch != 0) {
			if(voice->instrument_delay == 0) {
				int8_t pitch_data = s->waveforms[inst->pitch][voice->pitch_index];
				voice->pitch_index++;
				voice->pitch_index &= 0x7f;
				if(voice->pitch_index == 0) {
					voice->pitch_index = inst->pitch_loop;
				}
				voice->note_period = (uint16_t)(voice->note_period - pitch_data);
			} else {
				voice->instrument_delay--;
			}
		}

		paula_set_period(&s->paula, paula_ch, voice->note_period);
	}
}

// [=]===^=[ dmu_initialize_sound ]===============================================================[=]
static void dmu_initialize_sound(struct digitalmugician_state *s, int32_t song_number) {
	s->subsong_number = song_number;
	s->real_subsong = s->subsong_mapping[song_number];
	s->current_subsong = &s->subsongs[s->real_subsong];

	memset(s->voice_info, 0, sizeof(s->voice_info));

	s->speed = s->current_subsong->song_speed;
	s->current_speed = (uint16_t)(((s->current_subsong->song_speed & 0x0f) << 4) | (s->current_subsong->song_speed & 0x0f));
	s->last_shown_speed = s->current_speed;
	s->new_pattern = 1;
	s->new_row = 1;
	s->current_position = 0;
	s->song_length = s->current_subsong->number_of_sequences;
	s->current_row = 0;
	s->pattern_length = 64;

	s->current_sequence = s->subsong_sequences[s->real_subsong];
	s->current_sequence_length = s->subsong_sequence_length[s->real_subsong];

	if(s->module_type == DMU_TYPE_DMU2) {
		s->current_sequence2 = s->subsong_sequences[s->real_subsong + 1];
	} else {
		s->current_sequence2 = 0;
	}

	memset(s->ch_tab, 0, sizeof(s->ch_tab));
	s->ch_tab_index = 0;

	s->end_reached = 0;
}

// [=]===^=[ dmu_play_it ]========================================================================[=]
static void dmu_play_it(struct digitalmugician_state *s) {
	s->ch_tab[0] = 0x80;
	s->ch_tab[1] = 0x80;
	s->ch_tab[2] = 0x80;
	s->ch_tab[3] = 0x80;
	s->ch_tab_index = 0;

	for(int32_t i = 0; i < s->number_of_channels; ++i) {
		dmu_play_note(s, i);
	}
	for(int32_t i = 0; i < s->number_of_channels; ++i) {
		dmu_do_effects(s, i);
	}

	s->new_pattern = 0;
	s->new_row = 0;

	s->speed--;
	if(s->speed == 0) {
		s->speed = (uint16_t)(s->current_speed & 0x0f);
		dmu_change_speed(s, (uint16_t)(((s->current_speed & 0x0f) << 4) | ((s->current_speed & 0xf0) >> 4)));
		s->new_row = 1;
		s->current_row++;
		if((s->current_row == 64) || (s->current_row == s->pattern_length)) {
			s->current_row = 0;
			s->new_pattern = 1;
			s->current_position++;
			if(s->current_position == s->song_length) {
				if(s->current_subsong->loop_song) {
					s->current_position = s->current_subsong->loop_position;
				} else {
					dmu_initialize_sound(s, s->subsong_number);
				}
				s->end_reached = 1;
			}
		}
	}
}

// [=]===^=[ dmu_load ]===========================================================================[=]
static int32_t dmu_load(struct digitalmugician_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;

	if(len < 204) {
		return 0;
	}

	uint32_t pos = 24;
	uint8_t arpeggios_enabled = (dmu_read_u16_be(data + pos) != 0) ? 1 : 0;
	pos += 2;
	s->number_of_tracks = dmu_read_u16_be(data + pos);
	pos += 2;

	for(int32_t i = 0; i < 8; ++i) {
		s->subsong_sequence_length[i] = dmu_read_u32_be(data + pos);
		pos += 4;
	}

	s->number_of_instruments = dmu_read_u32_be(data + pos); pos += 4;
	s->number_of_waveforms   = dmu_read_u32_be(data + pos); pos += 4;
	s->number_of_samples     = dmu_read_u32_be(data + pos); pos += 4;
	uint32_t samples_size    = dmu_read_u32_be(data + pos); pos += 4;

	if(pos > len) {
		return 0;
	}

	// Sub-songs: 16 bytes each
	if(pos + 8 * 16 > len) {
		return 0;
	}
	for(int32_t i = 0; i < 8; ++i) {
		s->subsongs[i].loop_song           = data[pos++] ? 1 : 0;
		s->subsongs[i].loop_position       = data[pos++];
		s->subsongs[i].song_speed          = data[pos++];
		s->subsongs[i].number_of_sequences = data[pos++];
		pos += 12; // skip name
	}

	// Sequences
	for(int32_t i = 0; i < 8; ++i) {
		uint32_t seq_len = s->subsong_sequence_length[i];
		if(seq_len == 0) {
			s->subsong_sequences[i] = 0;
			continue;
		}
		if(pos + seq_len * 4 * 2 > len) {
			return 0;
		}
		s->subsong_sequences[i] = (struct dmu_sequence *)calloc(4 * seq_len, sizeof(struct dmu_sequence));
		if(!s->subsong_sequences[i]) {
			return 0;
		}
		for(uint32_t j = 0; j < seq_len; ++j) {
			for(int32_t k = 0; k < 4; ++k) {
				struct dmu_sequence *seq = &s->subsong_sequences[i][k * (int32_t)seq_len + j];
				seq->track_number = data[pos++];
				seq->transpose = (int8_t)data[pos++];
			}
		}
	}

	// Instruments: 16 bytes each
	if(s->number_of_instruments > 0) {
		if(pos + s->number_of_instruments * 16 > len) {
			return 0;
		}
		s->instruments = (struct dmu_instrument *)calloc(s->number_of_instruments, sizeof(struct dmu_instrument));
		if(!s->instruments) {
			return 0;
		}
		for(uint32_t i = 0; i < s->number_of_instruments; ++i) {
			struct dmu_instrument *inst = &s->instruments[i];
			inst->waveform_number  = data[pos++];
			inst->loop_length      = (uint16_t)(data[pos++] * 2);
			inst->volume           = data[pos++];
			inst->volume_speed     = data[pos++];
			inst->arpeggio_number  = data[pos++];
			inst->pitch            = data[pos++];
			inst->effect_index     = data[pos++];
			inst->delay            = data[pos++];
			inst->finetune         = data[pos++];
			inst->pitch_loop       = data[pos++];
			inst->pitch_speed      = data[pos++];
			inst->effect           = data[pos++];
			inst->source_wave1     = data[pos++];
			inst->source_wave2     = data[pos++];
			inst->effect_speed     = data[pos++];
			inst->volume_loop      = data[pos++] ? 1 : 0;
		}
	}

	// Waveforms: 128 bytes each
	if(s->number_of_waveforms > 0) {
		if(pos + s->number_of_waveforms * DMU_WAVEFORM_SIZE > len) {
			return 0;
		}
		s->waveforms = (int8_t (*)[DMU_WAVEFORM_SIZE])calloc(s->number_of_waveforms, DMU_WAVEFORM_SIZE);
		if(!s->waveforms) {
			return 0;
		}
		for(uint32_t i = 0; i < s->number_of_waveforms; ++i) {
			memcpy(s->waveforms[i], data + pos, DMU_WAVEFORM_SIZE);
			pos += DMU_WAVEFORM_SIZE;
		}
	}

	// Sample headers: 32 bytes each (12 of meta + 20 reserved)
	if(s->number_of_samples > 0) {
		if(pos + s->number_of_samples * 32 > len) {
			return 0;
		}
		s->samples = (struct dmu_sample *)calloc(s->number_of_samples, sizeof(struct dmu_sample));
		if(!s->samples) {
			return 0;
		}
		for(uint32_t i = 0; i < s->number_of_samples; ++i) {
			uint32_t start_offset = dmu_read_u32_be(data + pos); pos += 4;
			uint32_t end_offset   = dmu_read_u32_be(data + pos); pos += 4;
			int32_t loop_start    = dmu_read_i32_be(data + pos); pos += 4;
			pos += 20;

			s->samples[i].length = end_offset - start_offset;
			if(loop_start != 0) {
				s->samples[i].loop_start = (int32_t)(loop_start - (int32_t)start_offset);
			} else {
				s->samples[i].loop_start = -1;
			}
			// Defer setting data pointer until tracks/sample-data offsets are known.
			s->samples[i].data = (int8_t *)(uintptr_t)start_offset; // temporary stash
		}
	}

	// Tracks: 64 rows * 4 bytes = 256 bytes each
	if(s->number_of_tracks > 0) {
		if(pos + (uint32_t)s->number_of_tracks * 256 > len) {
			return 0;
		}
		s->tracks = (struct dmu_track *)calloc(s->number_of_tracks, sizeof(struct dmu_track));
		if(!s->tracks) {
			return 0;
		}
		for(uint32_t i = 0; i < s->number_of_tracks; ++i) {
			for(int32_t j = 0; j < 64; ++j) {
				struct dmu_track_row *row = &s->tracks[i].rows[j];
				row->note         = data[pos++];
				row->instrument   = data[pos++];
				row->effect       = data[pos++];
				row->effect_param = data[pos++];
			}
		}
	}

	// Sample data follows. Resolve sample data pointers using the stashed start_offset.
	uint32_t sample_data_start = pos;
	if(s->number_of_samples > 0) {
		// Compute total length needed and copy samples into our owned pool.
		uint32_t total = 0;
		for(uint32_t i = 0; i < s->number_of_samples; ++i) {
			total += s->samples[i].length;
		}
		if(total > 0) {
			if(sample_data_start + samples_size > len) {
				// Try to be permissive: clamp to remaining bytes
				if(sample_data_start > len) {
					return 0;
				}
			}
			s->sample_pool = (int8_t *)calloc(total > 0 ? total : 1, 1);
			if(!s->sample_pool) {
				return 0;
			}
			s->sample_pool_len = total;
			uint32_t cursor = 0;
			for(uint32_t i = 0; i < s->number_of_samples; ++i) {
				uint32_t start = (uint32_t)(uintptr_t)s->samples[i].data;
				uint32_t length = s->samples[i].length;
				if(sample_data_start + start + length > len) {
					length = (sample_data_start + start <= len) ? (len - (sample_data_start + start)) : 0;
					s->samples[i].length = length;
				}
				if(length > 0) {
					memcpy(s->sample_pool + cursor, data + sample_data_start + start, length);
				}
				s->samples[i].data = s->sample_pool + cursor;
				cursor += length;
			}
		} else {
			for(uint32_t i = 0; i < s->number_of_samples; ++i) {
				s->samples[i].data = 0;
			}
		}
	}

	// Arpeggios
	memset(s->arpeggios, 0, sizeof(s->arpeggios));
	if(arpeggios_enabled) {
		uint32_t arp_pos = sample_data_start + samples_size;
		if(arp_pos + 8 * 32 <= len) {
			for(int32_t i = 0; i < 8; ++i) {
				memcpy(s->arpeggios[i], data + arp_pos, 32);
				arp_pos += 32;
			}
		}
	}

	return 1;
}

// [=]===^=[ dmu_build_subsong_mapping ]==========================================================[=]
static void dmu_build_subsong_mapping(struct digitalmugician_state *s) {
	int32_t increment = (s->module_type == DMU_TYPE_DMU1) ? 1 : 2;
	s->subsong_mapping_count = 0;
	for(int32_t i = 0; i < 8; i += increment) {
		if(s->subsong_sequence_length[i] != s->subsongs[i].number_of_sequences) {
			continue;
		}
		struct dmu_sequence *sequences = s->subsong_sequences[i];
		if(!sequences) {
			continue;
		}
		uint32_t seq_len = s->subsong_sequence_length[i];
		int32_t found = 0;
		for(uint32_t j = 0; j < seq_len && !found; ++j) {
			for(int32_t k = 0; k < 4 && !found; ++k) {
				struct dmu_sequence *seq = &sequences[k * (int32_t)seq_len + j];
				if((seq->track_number != 0) || (seq->transpose != 0)) {
					s->subsong_mapping[s->subsong_mapping_count++] = i;
					found = 1;
				}
			}
		}
	}
	if(s->subsong_mapping_count == 0) {
		// fallback: at least map subsong 0 so we don't crash
		s->subsong_mapping[0] = 0;
		s->subsong_mapping_count = 1;
	}
}

// [=]===^=[ digitalmugician_init ]===============================================================[=]
static struct digitalmugician_state *digitalmugician_init(void *data, uint32_t len, int32_t sample_rate) {
	int32_t type = dmu_identify((uint8_t *)data, len);
	if(type == DMU_TYPE_UNKNOWN) {
		return 0;
	}

	struct digitalmugician_state *s = (struct digitalmugician_state *)calloc(1, sizeof(struct digitalmugician_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;
	s->module_type = type;
	s->number_of_channels = (type == DMU_TYPE_DMU1) ? 4 : 7;

	if(!dmu_load(s)) {
		dmu_cleanup(s);
		free(s);
		return 0;
	}

	dmu_build_subsong_mapping(s);

	paula_init(&s->paula, sample_rate, DMU_TICK_HZ);

	if(s->module_type == DMU_TYPE_DMU2) {
		// Hard-pan the four mix channels (DMU2 folds 7 voices into 4 Paula channels).
		s->paula.ch[0].pan = 0;
		s->paula.ch[1].pan = 127;
		s->paula.ch[2].pan = 127;
		s->paula.ch[3].pan = 0;
	}

	dmu_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ digitalmugician_free ]===============================================================[=]
static void digitalmugician_free(struct digitalmugician_state *s) {
	if(!s) {
		return;
	}
	dmu_cleanup(s);
	free(s);
}

// [=]===^=[ digitalmugician_get_audio ]==========================================================[=]
static void digitalmugician_get_audio(struct digitalmugician_state *s, int16_t *output, int32_t frames) {
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
			dmu_play_it(s);
		}
	}
}

// [=]===^=[ digitalmugician_api_init ]===========================================================[=]
static void *digitalmugician_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return digitalmugician_init(data, len, sample_rate);
}

// [=]===^=[ digitalmugician_api_free ]===========================================================[=]
static void digitalmugician_api_free(void *state) {
	digitalmugician_free((struct digitalmugician_state *)state);
}

// [=]===^=[ digitalmugician_api_get_audio ]======================================================[=]
static void digitalmugician_api_get_audio(void *state, int16_t *output, int32_t frames) {
	digitalmugician_get_audio((struct digitalmugician_state *)state, output, frames);
}

static const char *digitalmugician_extensions[] = { "dmu", "mug", 0 };

static struct player_api digitalmugician_api = {
	"Digital Mugician",
	digitalmugician_extensions,
	digitalmugician_api_init,
	digitalmugician_api_free,
	digitalmugician_api_get_audio,
	0,
};
