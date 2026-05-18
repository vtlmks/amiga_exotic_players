// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// ProTracker 2.3d replayer (31-sample .mod).
//
// A faithful port of the ProTracker 2.3d play routine -- the de-facto
// reference replayer for the Amiga .mod catalogue. Behaviour is locked to
// PT2.3d, including its quirks: the signed-arpeggio period-table overflow,
// the portamento-up sign-removed clamp bug, the EFx funk/invert-loop sample
// mutation, the one-tick-delayed CIA tempo change, the Dxx/Bxx ordering, the
// 9xx offset-past-end truncation, and the vibrato/tremolo waveform tables
// with the E4x/E7x no-retrigger continue bit. The 15-sample pre-ProTracker
// family is handled by soundtracker.h; this header only accepts the genuine
// 4-channel ProTracker/NoiseTracker lineage.
//
// Accepted tags at offset 1080: M.K., M!K!, M&K!, FLT4, N.T.; also an
// untagged-but-structurally-31-sample 4-channel module. Multichannel
// (6CHN/8CHN/FLT8/xxCH) is a different tracker family and not handled here.
//
// On-disk layout (31-sample, all multi-byte fields big-endian):
//   0..19      song title (20 bytes)
//   20..950    31 sample headers, 30 bytes each:
//                 0..21   name (22 bytes)
//                 22..23  length in words
//                 24      finetune (low nibble, signed -8..+7)
//                 25      volume (0..64)
//                 26..27  loop start in words
//                 28..29  loop length in words
//   950        song length (positions used; 1..128)
//   951        restart byte (ProTracker writes 0x7f; NoiseTracker writes a
//              real restart position -- used as a timing-mode signal)
//   952..1079  pattern (order) table, 128 bytes
//   1080..1083 4-byte format tag
//   1084..     pattern data, 1024 bytes per pattern, 64 rows of 4 voices x
//                 4 bytes:
//                 byte 0: sample number high nibble (bits 4..7) + period hi
//                 byte 1: period low 8 bits
//                 byte 2: sample number low nibble (bits 4..7) + effect cmd
//                 byte 3: effect argument
//   ...        sample data (signed 8-bit PCM), one block per sample in order
//
// Timing mode (CIA vs VBLANK): ProTracker 2.3d uses CIA timing, where Fxx
// with arg >= 0x20 is a BPM (tempo) change. NoiseTracker modules use VBLANK
// timing, where every Fxx is a raw ticks-per-row value at a fixed 50 Hz and
// arg >= 0x20 must NOT be read as BPM. There is no authoring-tool marker in
// the file; the same content-only signals every faithful player converges on
// are used (see pt_detect_vblank). A host that knows better may force the
// choice by defining PROTRACKER_TIMING before including this header.
//
// Public API:
//   struct protracker_state *protracker_init(void *data, uint32_t len, int32_t sample_rate);
//   void protracker_free(struct protracker_state *s);
//   void protracker_get_audio(struct protracker_state *s, float *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

enum {
	PROTRACKER_TIMING_CIA    = 0,   // ProTracker: Fxx >= 0x20 is BPM
	PROTRACKER_TIMING_VBLANK = 1,   // NoiseTracker: Fxx is always ticks/row at 50 Hz
};

// PROTRACKER_TIMING, when defined by the host before including this header,
// forces the timing mode. When left undefined it is detected from content
// (pt_detect_vblank).

#define PT_VOICES        4
#define PT_NUM_SAMPLES   31
#define PT_SAMPLE_HDR    30
#define PT_SAMPLES_OFF   20
#define PT_SONGLEN_OFF   950
#define PT_RESTART_OFF   951
#define PT_ORDER_OFF     952
#define PT_ORDER_LEN     128
#define PT_TAG_OFF       1080
#define PT_PAT_DATA_OFF  1084
#define PT_PAT_BYTES     1024
#define PT_ROWS_PER_PAT  64
#define PT_BYTES_PER_ROW 16
#define PT_DEF_SPEED     6
#define PT_DEF_BPM       125
#define PT_MIN_BPM       32

// PT truncates 1773447 / bpm to integer; the CIA timer fires on underflow so
// the divisor is (period + 1). 709379 is the PAL CIA clock.
#define PT_CIA_CLK       709379
#define PT_CIA_BPM_NUM   1773447

// PAL vertical-blank rate used for VBLANK-timed (NoiseTracker) modules.
#define PT_VBLANK_HZ     50.0

// ProTracker period table: 16 finetune sections (finetune 0..+7 then -8..-1),
// each 36 periods followed by a 0 terminator (37-word stride). The trailing
// 15 words reproduce the out-of-bounds read PT's arpeggio performs on
// finetune -1 top notes (these are the bytes that follow the period table in
// the original PT binary; the values are part of the audible result).
static int16_t pt_period_table[(37 * 16) + 15] = {
	856,808,762,720,678,640,604,570,538,508,480,453,428,404,381,360,339,320,302,285,269,254,240,226,214,202,190,180,170,160,151,143,135,127,120,113,0,
	850,802,757,715,674,637,601,567,535,505,477,450,425,401,379,357,337,318,300,284,268,253,239,225,213,201,189,179,169,159,150,142,134,126,119,113,0,
	844,796,752,709,670,632,597,563,532,502,474,447,422,398,376,355,335,316,298,282,266,251,237,224,211,199,188,177,167,158,149,141,133,125,118,112,0,
	838,791,746,704,665,628,592,559,528,498,470,444,419,395,373,352,332,314,296,280,264,249,235,222,209,198,187,176,166,157,148,140,132,125,118,111,0,
	832,785,741,699,660,623,588,555,524,495,467,441,416,392,370,350,330,312,294,278,262,247,233,220,208,196,185,175,165,156,147,139,131,124,117,110,0,
	826,779,736,694,655,619,584,551,520,491,463,437,413,390,368,347,328,309,292,276,260,245,232,219,206,195,184,174,164,155,146,138,130,123,116,109,0,
	820,774,730,689,651,614,580,547,516,487,460,434,410,387,365,345,325,307,290,274,258,244,230,217,205,193,183,172,163,154,145,137,129,122,115,109,0,
	814,768,725,684,646,610,575,543,513,484,457,431,407,384,363,342,323,305,288,272,256,242,228,216,204,192,181,171,161,152,144,136,128,121,114,108,0,
	907,856,808,762,720,678,640,604,570,538,508,480,453,428,404,381,360,339,320,302,285,269,254,240,226,214,202,190,180,170,160,151,143,135,127,120,0,
	900,850,802,757,715,675,636,601,567,535,505,477,450,425,401,379,357,337,318,300,284,268,253,238,225,212,200,189,179,169,159,150,142,134,126,119,0,
	894,844,796,752,709,670,632,597,563,532,502,474,447,422,398,376,355,335,316,298,282,266,251,237,223,211,199,188,177,167,158,149,141,133,125,118,0,
	887,838,791,746,704,665,628,592,559,528,498,470,444,419,395,373,352,332,314,296,280,264,249,235,222,209,198,187,176,166,157,148,140,132,125,118,0,
	881,832,785,741,699,660,623,588,555,524,494,467,441,416,392,370,350,330,312,294,278,262,247,233,220,208,196,185,175,165,156,147,139,131,123,117,0,
	875,826,779,736,694,655,619,584,551,520,491,463,437,413,390,368,347,328,309,292,276,260,245,232,219,206,195,184,174,164,155,146,138,130,123,116,0,
	868,820,774,730,689,651,614,580,547,516,487,460,434,410,387,365,345,325,307,290,274,258,244,230,217,205,193,183,172,163,154,145,137,129,122,115,0,
	862,814,768,725,684,646,610,575,543,513,484,457,431,407,384,363,342,323,305,288,272,256,242,228,216,203,192,181,171,161,152,144,136,128,121,114,0,
	774,1800,2314,3087,4113,4627,5400,6426,6940,7713,8739,9253,24625,12851,13365,
};

// PT vibrato/tremolo sine quarter-period lookup (32 entries, one half-cycle).
static uint8_t pt_vibrato_table[32] = {
	0x00,0x18,0x31,0x4a,0x61,0x78,0x8d,0xa1,
	0xb4,0xc5,0xd4,0xe0,0xeb,0xf4,0xfa,0xfd,
	0xff,0xfd,0xfa,0xf4,0xeb,0xe0,0xd4,0xc5,
	0xb4,0xa1,0x8d,0x78,0x61,0x4a,0x31,0x18,
};

// EFx (FunkRepeat / InvertLoop) speed table.
static uint8_t pt_funk_table[16] = {
	0x00,0x05,0x06,0x07,0x08,0x0a,0x0b,0x0d,
	0x10,0x13,0x16,0x1a,0x20,0x2b,0x40,0x80,
};

struct pt_sample {
	uint32_t data_off;        // byte offset of sample data in module_data
	uint32_t length;          // bytes
	uint32_t loop_start;      // bytes
	uint32_t loop_length;     // bytes
	uint8_t  volume;          // 0..64
	uint8_t  finetune;        // 0..15 (0..7 = +0..+7, 8..15 = -8..-1)
};

struct pt_channel {
	int32_t  index;           // 0..3
	uint16_t n_note;          // raw period from the cell (0..0xfff)
	uint16_t n_cmd;           // (command << 8) | argument
	uint8_t  n_samplenum;     // 0..30 (sample number - 1)

	// Sample geometry, all byte offsets relative to the sample's data start
	// except n_base_off which is the absolute offset in module_data.
	uint32_t n_base_off;      // sample data start in module_data
	uint32_t n_play_len;      // first-pass length from base (wrap point)
	uint32_t n_loop_start;    // loop start, from base
	uint32_t n_loop_len;      // loop length (0 => one-shot)
	uint32_t n_start_off;     // read start within sample (9xx), from base
	uint32_t n_wavestart;     // EFx invert-loop write cursor, from base
	uint8_t  n_has_sample;    // a valid sample is loaded on this channel

	int16_t  n_period;
	int16_t  n_wantedperiod;
	uint8_t  n_finetune;
	int16_t  n_volume;        // 0..64 (kept signed; PT slides can go negative pre-clamp)
	uint8_t  n_toneportdirec;
	uint8_t  n_toneportspeed;
	uint8_t  n_vibratocmd;
	uint8_t  n_vibratopos;
	uint8_t  n_tremolocmd;
	uint8_t  n_tremolopos;
	uint8_t  n_wavecontrol;
	uint8_t  n_glissfunk;
	uint8_t  n_loopcount;
	uint8_t  n_pattpos;
	uint8_t  n_sampleoffset;
	uint8_t  n_funkoffset;
};

struct protracker_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	struct pt_sample samples[PT_NUM_SAMPLES];
	uint8_t  order[PT_ORDER_LEN];
	uint8_t  song_length;
	uint8_t  restart;
	uint32_t num_patterns;
	uint8_t  timing_mode;

	struct pt_channel ch[PT_VOICES];

	int32_t  song_tick;
	int32_t  song_speed;
	int32_t  song_bpm;
	int32_t  cia_set_bpm;     // pending CIA BPM change (-1 = none; applied next tick)

	int32_t  mod_pos;
	int32_t  mod_pattern;
	int32_t  song_row;

	int32_t  pbreak_position;
	uint8_t  pbreak_flag;
	uint8_t  posjump_assert;
	uint8_t  patt_del_time;
	uint8_t  patt_del_time2;
	uint8_t  stop_song;       // F00: restart from position 0 (loop)
	uint8_t  low_mask;        // fine-portamento nibble mask (transient)
	uint8_t  end_reached;
};

// [=]===^=[ pt_read_u16_be ]=====================================================================[=]
static uint16_t pt_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ pt_read_u32_be ]=====================================================================[=]
static uint32_t pt_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ pt_is_pt_tag ]=======================================================================[=]
// The genuine 4-channel ProTracker/NoiseTracker lineage. Multichannel tags
// (6CHN/8CHN/FLT8/xxCH) belong to other trackers and are intentionally not
// accepted here.
static int32_t pt_is_pt_tag(uint32_t v) {
	switch(v) {
		case 0x4d2e4b2eu:  // "M.K."
		case 0x4d214b21u:  // "M!K!"
		case 0x4d264b21u:  // "M&K!"
		case 0x464c5434u:  // "FLT4"
		case 0x4e2e542eu:  // "N.T."
			return 1;
		default:
			break;
	}
	return 0;
}

// [=]===^=[ pt_count_patterns ]==================================================================[=]
// PT sizes pattern storage from the highest index that appears anywhere in
// the 128-byte order table, not just within song_length: authoring tools
// leave referenced-but-unplayed patterns past the end of the song.
static uint32_t pt_count_patterns(uint8_t *order) {
	uint32_t max_pat = 0;
	for(uint32_t i = 0; i < PT_ORDER_LEN; ++i) {
		if(order[i] > max_pat) {
			max_pat = order[i];
		}
	}
	return max_pat + 1u;
}

// [=]===^=[ pt_identify ]========================================================================[=]
// A 31-sample 4-channel module. The tag at 1080 is the primary signal; a
// missing/garbled tag is still accepted when the structure checks out
// (header + patterns + sample bytes fit the file with only minor trailing
// slack, every cell's sample-number bits are sane).
static int32_t pt_identify(uint8_t *data, uint32_t len) {
	if(len < PT_PAT_DATA_OFF) {
		return 0;
	}

	uint32_t tag = pt_read_u32_be(data + PT_TAG_OFF);
	int32_t tagged = pt_is_pt_tag(tag);

	uint8_t song_length = data[PT_SONGLEN_OFF];
	if(song_length == 0 || song_length > PT_ORDER_LEN) {
		return 0;
	}

	uint32_t num_patterns = pt_count_patterns(data + PT_ORDER_OFF);
	if(num_patterns > PT_ORDER_LEN) {
		return 0;
	}

	uint32_t total_sample_bytes = 0;
	for(uint32_t i = 0; i < PT_NUM_SAMPLES; ++i) {
		uint8_t *h = data + PT_SAMPLES_OFF + i * PT_SAMPLE_HDR;
		uint32_t length_w = pt_read_u16_be(h + 22);
		uint8_t  volume   = h[25];
		if(volume > 64) {
			return 0;
		}
		total_sample_bytes += length_w * 2u;
	}

	uint32_t pat_bytes = num_patterns * PT_PAT_BYTES;
	if(len < PT_PAT_DATA_OFF + pat_bytes) {
		return 0;
	}

	if(!tagged) {
		// Untagged: demand the full layout fits with only a little trailing
		// slack, and that no cell carries an impossible sample number.
		uint32_t need = PT_PAT_DATA_OFF + pat_bytes + total_sample_bytes;
		if(len + 256u < need || len > need + 65536u) {
			return 0;
		}
		for(uint32_t p = 0; p < num_patterns; ++p) {
			uint32_t base = PT_PAT_DATA_OFF + p * PT_PAT_BYTES;
			for(uint32_t off = 0; off < PT_PAT_BYTES; off += 4) {
				uint8_t *c = data + base + off;
				uint32_t sample_no = ((uint32_t)(c[0] & 0xf0)) | ((uint32_t)c[2] >> 4);
				if(sample_no > PT_NUM_SAMPLES) {
					return 0;
				}
			}
		}
	}
	return 1;
}

// [=]===^=[ pt_detect_vblank ]===================================================================[=]
// ProTracker 2.3d is CIA-timed; NoiseTracker is VBLANK-timed. No file field
// records the authoring tool, so this uses the content signals every faithful
// player converges on. A module is VBLANK iff it (a) uses some Fxx with arg
// >= 0x20 -- the only case where the two interpretations differ at all --
// while (b) carrying no extended Exy command anywhere (NoiseTracker has none)
// and (c) having a restart byte other than the 0x7f ProTracker always writes.
// Verified against the canonical hard case: "klisje paa klisje" (restart 0,
// no E-commands, F20/F30 as raw speed) classifies VBLANK; a real PT module
// like "physical presence" (restart 0x7f, E6/EA/EB present, F8C as BPM 140)
// stays CIA. If a module never uses Fxx >= 0x20 the choice is moot (every
// speed < 0x20 plays identically in both modes) and it stays CIA.
static uint8_t pt_detect_vblank(struct protracker_state *s) {
	if(s->restart == 0x7f) {
		return PROTRACKER_TIMING_CIA;
	}
	uint8_t has_fhi = 0;
	for(uint32_t p = 0; p < s->num_patterns; ++p) {
		uint32_t base = PT_PAT_DATA_OFF + p * PT_PAT_BYTES;
		for(uint32_t off = 0; off < PT_PAT_BYTES; off += 4) {
			uint8_t *c = s->module_data + base + off;
			uint8_t cmd = (uint8_t)(c[2] & 0x0f);
			uint8_t arg = c[3];
			if(cmd == 0xe) {
				return PROTRACKER_TIMING_CIA;
			}
			if(cmd == 0xf && arg >= 0x20) {
				has_fhi = 1;
			}
		}
	}
	return has_fhi ? PROTRACKER_TIMING_VBLANK : PROTRACKER_TIMING_CIA;
}

// [=]===^=[ pt_set_tempo ]=======================================================================[=]
// Recompute the Paula tick window from the current BPM. CIA mode uses PT's
// truncating divider exactly; VBLANK mode is a fixed 50 Hz regardless of BPM.
static void pt_set_tempo(struct protracker_state *s, int32_t bpm) {
	s->song_bpm = bpm;
	double hz;
	if(s->timing_mode == PROTRACKER_TIMING_VBLANK) {
		hz = PT_VBLANK_HZ;
	} else {
		uint32_t cia_period = (uint32_t)(PT_CIA_BPM_NUM / bpm);
		hz = (double)PT_CIA_CLK / (double)(cia_period + 1u);
	}
	int32_t spt = (int32_t)((double)s->paula.sample_rate / hz + 0.5);
	if(spt < 1) {
		spt = 1;
	}
	s->paula.samples_per_tick = spt;
}

// [=]===^=[ pt_set_speed ]=======================================================================[=]
static void pt_set_speed(struct protracker_state *s, int32_t speed) {
	s->song_speed = speed;
	s->song_tick = 0;
}

// [=]===^=[ pt_paula_period ]====================================================================[=]
static void pt_paula_period(struct protracker_state *s, struct pt_channel *ch, int32_t period) {
	paula_set_period(&s->paula, ch->index, (uint16_t)(period & 0xfff));
}

// [=]===^=[ pt_paula_volume ]====================================================================[=]
static void pt_paula_volume(struct protracker_state *s, struct pt_channel *ch, int32_t vol) {
	if(vol < 0) {
		vol = 0;
	}
	if(vol > 64) {
		vol = 64;
	}
	paula_set_volume(&s->paula, ch->index, (uint16_t)vol);
}

// [=]===^=[ pt_do_retrg ]========================================================================[=]
// Restart the channel's DMA from n_start_off for n_play_len bytes, then hand
// Paula the loop region for the cycle after that (the Amiga "write AUDxLC/LEN
// after starting DMA" sequence). 9xx only moves the read cursor; the wrap
// point stays at the sample base + original length.
static void pt_do_retrg(struct protracker_state *s, struct pt_channel *ch) {
	if(!ch->n_has_sample || ch->n_play_len == 0) {
		paula_play_sample(&s->paula, ch->index, 0, 0);
		return;
	}
	int8_t *base = (int8_t *)(s->module_data + ch->n_base_off);
	paula_play_sample(&s->paula, ch->index, base, ch->n_play_len);
	if(ch->n_loop_len > 0) {
		paula_set_loop(&s->paula, ch->index, ch->n_loop_start, ch->n_loop_len);
	} else {
		paula_set_loop(&s->paula, ch->index, 0, 0);
	}
	if(ch->n_start_off > 0) {
		paula_set_pos(&s->paula, ch->index, ch->n_start_off);
	}
	pt_paula_period(s, ch, ch->n_period);
	pt_paula_volume(s, ch, ch->n_volume);
}

// [=]===^=[ pt_update_funk ]=====================================================================[=]
// EFx FunkRepeat / InvertLoop: accumulate the funk offset and, on wrap, step
// the invert cursor through the loop and bit-invert one sample byte in place.
// PT mutates its own sample RAM here; the port mutates the loaded module the
// same way (this is observable behaviour, not an implementation detail).
static void pt_update_funk(struct protracker_state *s, struct pt_channel *ch) {
	uint8_t funk_speed = (uint8_t)(ch->n_glissfunk >> 4);
	if(funk_speed == 0) {
		return;
	}
	ch->n_funkoffset += pt_funk_table[funk_speed];
	if(ch->n_funkoffset >= 128) {
		ch->n_funkoffset = 0;
		if(ch->n_has_sample && ch->n_loop_len > 0) {
			uint32_t ws = ch->n_wavestart + 1u;
			if(ws >= ch->n_loop_start + ch->n_loop_len) {
				ws = ch->n_loop_start;
			}
			ch->n_wavestart = ws;
			int8_t *p = (int8_t *)(s->module_data + ch->n_base_off + ws);
			*p = (int8_t)(-1 - *p);
		}
	}
}

// [=]===^=[ pt_set_gliss_control ]===============================================================[=]
static void pt_set_gliss_control(struct pt_channel *ch) {
	ch->n_glissfunk = (uint8_t)((ch->n_glissfunk & 0xf0) | (ch->n_cmd & 0x0f));
}

// [=]===^=[ pt_set_vibrato_control ]=============================================================[=]
static void pt_set_vibrato_control(struct pt_channel *ch) {
	ch->n_wavecontrol = (uint8_t)((ch->n_wavecontrol & 0xf0) | (ch->n_cmd & 0x0f));
}

// [=]===^=[ pt_set_finetune ]====================================================================[=]
static void pt_set_finetune(struct pt_channel *ch) {
	ch->n_finetune = (uint8_t)(ch->n_cmd & 0xf);
}

// [=]===^=[ pt_jump_loop ]=======================================================================[=]
static void pt_jump_loop(struct protracker_state *s, struct pt_channel *ch) {
	if(s->song_tick != 0) {
		return;
	}
	if((ch->n_cmd & 0xf) == 0) {
		ch->n_pattpos = (uint8_t)s->song_row;
	} else {
		if(ch->n_loopcount == 0) {
			ch->n_loopcount = (uint8_t)(ch->n_cmd & 0xf);
		} else if(--ch->n_loopcount == 0) {
			return;
		}
		s->pbreak_position = ch->n_pattpos;
		s->pbreak_flag = 1;
	}
}

// [=]===^=[ pt_set_tremolo_control ]=============================================================[=]
static void pt_set_tremolo_control(struct pt_channel *ch) {
	ch->n_wavecontrol = (uint8_t)(((ch->n_cmd & 0xf) << 4) | (ch->n_wavecontrol & 0xf));
}

// [=]===^=[ pt_do_retrig_note ]==================================================================[=]
static void pt_do_retrig_note(struct protracker_state *s, struct pt_channel *ch) {
	if((ch->n_cmd & 0xf) > 0) {
		if(s->song_tick == 0 && (ch->n_note & 0xfff) > 0) {
			return;
		}
		if(s->song_tick % (int32_t)(ch->n_cmd & 0xf) == 0) {
			pt_do_retrg(s, ch);
		}
	}
}

// [=]===^=[ pt_volume_slide ]====================================================================[=]
static void pt_volume_slide(struct pt_channel *ch) {
	uint8_t param = (uint8_t)(ch->n_cmd & 0xff);
	if((param & 0xf0) == 0) {
		ch->n_volume -= (int16_t)(param & 0x0f);
		if(ch->n_volume < 0) {
			ch->n_volume = 0;
		}
	} else {
		ch->n_volume += (int16_t)(param >> 4);
		if(ch->n_volume > 64) {
			ch->n_volume = 64;
		}
	}
}

// [=]===^=[ pt_volume_fine_up ]==================================================================[=]
static void pt_volume_fine_up(struct protracker_state *s, struct pt_channel *ch) {
	if(s->song_tick == 0) {
		ch->n_volume += (int16_t)(ch->n_cmd & 0xf);
		if(ch->n_volume > 64) {
			ch->n_volume = 64;
		}
	}
}

// [=]===^=[ pt_volume_fine_down ]================================================================[=]
static void pt_volume_fine_down(struct protracker_state *s, struct pt_channel *ch) {
	if(s->song_tick == 0) {
		ch->n_volume -= (int16_t)(ch->n_cmd & 0xf);
		if(ch->n_volume < 0) {
			ch->n_volume = 0;
		}
	}
}

// [=]===^=[ pt_note_cut ]========================================================================[=]
static void pt_note_cut(struct protracker_state *s, struct pt_channel *ch) {
	if(s->song_tick == (int32_t)(ch->n_cmd & 0xf)) {
		ch->n_volume = 0;
	}
}

// [=]===^=[ pt_note_delay ]======================================================================[=]
static void pt_note_delay(struct protracker_state *s, struct pt_channel *ch) {
	if(s->song_tick == (int32_t)(ch->n_cmd & 0xf) && (ch->n_note & 0xfff) > 0) {
		pt_do_retrg(s, ch);
	}
}

// [=]===^=[ pt_pattern_delay ]===================================================================[=]
static void pt_pattern_delay(struct protracker_state *s, struct pt_channel *ch) {
	if(s->song_tick == 0 && s->patt_del_time2 == 0) {
		s->patt_del_time = (uint8_t)((ch->n_cmd & 0xf) + 1);
	}
}

// [=]===^=[ pt_funk_it ]=========================================================================[=]
static void pt_funk_it(struct protracker_state *s, struct pt_channel *ch) {
	if(s->song_tick == 0) {
		ch->n_glissfunk = (uint8_t)(((ch->n_cmd & 0xf) << 4) | (ch->n_glissfunk & 0xf));
		if((ch->n_glissfunk & 0xf0) > 0) {
			pt_update_funk(s, ch);
		}
	}
}

// [=]===^=[ pt_position_jump ]===================================================================[=]
static void pt_position_jump(struct protracker_state *s, struct pt_channel *ch) {
	s->mod_pos = (int32_t)(ch->n_cmd & 0xff) - 1;   // +1 in pt_next_position lands on the target
	s->pbreak_position = 0;
	s->posjump_assert = 1;
}

// [=]===^=[ pt_volume_change ]===================================================================[=]
static void pt_volume_change(struct pt_channel *ch) {
	ch->n_volume = (int16_t)(ch->n_cmd & 0xff);
	if(ch->n_volume > 64) {
		ch->n_volume = 64;
	}
}

// [=]===^=[ pt_pattern_break ]===================================================================[=]
static void pt_pattern_break(struct protracker_state *s, struct pt_channel *ch) {
	int32_t pos = (int32_t)(((ch->n_cmd & 0xf0) >> 4) * 10u) + (int32_t)(ch->n_cmd & 0x0f);
	if(pos > 63) {
		pos = 0;
	}
	s->pbreak_position = pos;
	s->posjump_assert = 1;
}

// [=]===^=[ pt_set_speed_cmd ]===================================================================[=]
// Fxx: arg 0 stops the song (treated here as a loop back to position 0).
// VBLANK modules read every arg as ticks-per-row; CIA modules read arg < 0x20
// as speed and arg >= 0x20 as a BPM change (the latter delayed one tick, a
// PT/CIA timing quirk reproduced via cia_set_bpm).
static void pt_set_speed_cmd(struct protracker_state *s, struct pt_channel *ch) {
	int32_t arg = (int32_t)(ch->n_cmd & 0xff);
	if(arg > 0) {
		if(s->timing_mode == PROTRACKER_TIMING_VBLANK || arg < 32) {
			pt_set_speed(s, arg);
		} else {
			s->cia_set_bpm = arg;
		}
	} else {
		s->stop_song = 1;
	}
}

// [=]===^=[ pt_arpeggio ]========================================================================[=]
// 0xy. Tick 0 restores the base period; ticks 1/2 add the high/low nibble as
// a table-index offset. The base-note search and the deliberate read past the
// 36-entry section (into the next finetune row or the trailing pad) reproduce
// PT2.3d exactly, including the finetune -1 overflow.
static void pt_arpeggio(struct protracker_state *s, struct pt_channel *ch) {
	int32_t arp_tick = s->song_tick % 3;
	int32_t arp_note;
	if(arp_tick == 1) {
		arp_note = ch->n_cmd >> 4;
	} else if(arp_tick == 2) {
		arp_note = ch->n_cmd & 0xf;
	} else {
		pt_paula_period(s, ch, ch->n_period);
		return;
	}
	int16_t *periods = &pt_period_table[ch->n_finetune * 37];
	for(int32_t base_note = 0; base_note < 37; ++base_note) {
		if(ch->n_period >= periods[base_note]) {
			pt_paula_period(s, ch, periods[base_note + arp_note]);
			break;
		}
	}
}

// [=]===^=[ pt_porta_up ]========================================================================[=]
// PT bug preserved: the sign is removed (& 0xfff) before the < 113 clamp, so
// an underflowing period is NOT clamped and plays as a very low pitch.
static void pt_porta_up(struct protracker_state *s, struct pt_channel *ch) {
	ch->n_period -= (int16_t)((ch->n_cmd & 0xff) & s->low_mask);
	s->low_mask = 0xff;
	if((ch->n_period & 0xfff) < 113) {
		ch->n_period = (int16_t)((ch->n_period & 0xf000) | 113);
	}
	pt_paula_period(s, ch, ch->n_period & 0xfff);
}

// [=]===^=[ pt_porta_down ]======================================================================[=]
static void pt_porta_down(struct protracker_state *s, struct pt_channel *ch) {
	ch->n_period += (int16_t)((ch->n_cmd & 0xff) & s->low_mask);
	s->low_mask = 0xff;
	if((ch->n_period & 0xfff) > 856) {
		ch->n_period = (int16_t)((ch->n_period & 0xf000) | 856);
	}
	pt_paula_period(s, ch, ch->n_period & 0xfff);
}

// [=]===^=[ pt_filter_on_off ]===================================================================[=]
// E0x: bit 0 clear => LED low-pass on.
static void pt_filter_on_off(struct protracker_state *s, struct pt_channel *ch) {
	if(s->song_tick == 0) {
		paula_set_lp_filter(&s->paula, ((ch->n_cmd & 1) ^ 1) ? 1 : 0);
	}
}

// [=]===^=[ pt_fine_porta_up ]===================================================================[=]
static void pt_fine_porta_up(struct protracker_state *s, struct pt_channel *ch) {
	if(s->song_tick == 0) {
		s->low_mask = 0xf;
		pt_porta_up(s, ch);
	}
}

// [=]===^=[ pt_fine_porta_down ]=================================================================[=]
static void pt_fine_porta_down(struct protracker_state *s, struct pt_channel *ch) {
	if(s->song_tick == 0) {
		s->low_mask = 0xf;
		pt_porta_down(s, ch);
	}
}

// [=]===^=[ pt_set_tone_porta ]==================================================================[=]
static void pt_set_tone_porta(struct pt_channel *ch) {
	uint16_t note = (uint16_t)(ch->n_note & 0xfff);
	int16_t *pp = &pt_period_table[ch->n_finetune * 37];
	int32_t i = 0;
	while(1) {
		if(note >= pp[i]) {
			break;
		}
		if(++i >= 37) {
			i = 35;
			break;
		}
	}
	if((ch->n_finetune & 8) && i > 0) {
		--i;
	}
	ch->n_wantedperiod = pp[i];
	ch->n_toneportdirec = 0;
	if(ch->n_period == ch->n_wantedperiod) {
		ch->n_wantedperiod = 0;
	} else if(ch->n_period > ch->n_wantedperiod) {
		ch->n_toneportdirec = 1;
	}
}

// [=]===^=[ pt_tone_port_no_change ]=============================================================[=]
static void pt_tone_port_no_change(struct protracker_state *s, struct pt_channel *ch) {
	if(ch->n_wantedperiod <= 0) {
		return;
	}
	if(ch->n_toneportdirec > 0) {
		ch->n_period -= (int16_t)ch->n_toneportspeed;
		if(ch->n_period <= ch->n_wantedperiod) {
			ch->n_period = ch->n_wantedperiod;
			ch->n_wantedperiod = 0;
		}
	} else {
		ch->n_period += (int16_t)ch->n_toneportspeed;
		if(ch->n_period >= ch->n_wantedperiod) {
			ch->n_period = ch->n_wantedperiod;
			ch->n_wantedperiod = 0;
		}
	}
	if((ch->n_glissfunk & 0xf) == 0) {
		pt_paula_period(s, ch, ch->n_period);
	} else {
		int16_t *pp = &pt_period_table[ch->n_finetune * 37];
		int32_t i = 0;
		while(1) {
			if(ch->n_period >= pp[i]) {
				break;
			}
			if(++i >= 37) {
				i = 35;
				break;
			}
		}
		pt_paula_period(s, ch, pp[i]);
	}
}

// [=]===^=[ pt_tone_portamento ]=================================================================[=]
static void pt_tone_portamento(struct protracker_state *s, struct pt_channel *ch) {
	if((ch->n_cmd & 0xff) > 0) {
		ch->n_toneportspeed = (uint8_t)(ch->n_cmd & 0xff);
		ch->n_cmd &= 0xff00;
	}
	pt_tone_port_no_change(s, ch);
}

// [=]===^=[ pt_vibrato2 ]========================================================================[=]
static void pt_vibrato2(struct protracker_state *s, struct pt_channel *ch) {
	uint16_t vibrato_data;
	uint8_t pos = (uint8_t)((ch->n_vibratopos >> 2) & 0x1f);
	uint8_t type = (uint8_t)(ch->n_wavecontrol & 3);
	if(type == 0) {
		vibrato_data = pt_vibrato_table[pos];
	} else if(type == 1) {
		if(ch->n_vibratopos < 128) {
			vibrato_data = (uint16_t)(pos << 3);
		} else {
			vibrato_data = (uint16_t)(255 - (pos << 3));
		}
	} else {
		vibrato_data = 255;
	}
	vibrato_data = (uint16_t)((vibrato_data * (ch->n_vibratocmd & 0xf)) >> 7);
	if(ch->n_vibratopos < 128) {
		vibrato_data = (uint16_t)(ch->n_period + vibrato_data);
	} else {
		vibrato_data = (uint16_t)(ch->n_period - vibrato_data);
	}
	pt_paula_period(s, ch, vibrato_data);
	ch->n_vibratopos += (uint8_t)((ch->n_vibratocmd >> 2) & 0x3c);
}

// [=]===^=[ pt_vibrato ]=========================================================================[=]
static void pt_vibrato(struct protracker_state *s, struct pt_channel *ch) {
	if((ch->n_cmd & 0x0f) > 0) {
		ch->n_vibratocmd = (uint8_t)((ch->n_vibratocmd & 0xf0) | (ch->n_cmd & 0x0f));
	}
	if((ch->n_cmd & 0xf0) > 0) {
		ch->n_vibratocmd = (uint8_t)((ch->n_cmd & 0xf0) | (ch->n_vibratocmd & 0x0f));
	}
	pt_vibrato2(s, ch);
}

// [=]===^=[ pt_tone_plus_vol_slide ]=============================================================[=]
static void pt_tone_plus_vol_slide(struct protracker_state *s, struct pt_channel *ch) {
	pt_tone_port_no_change(s, ch);
	pt_volume_slide(ch);
}

// [=]===^=[ pt_vibrato_plus_vol_slide ]==========================================================[=]
static void pt_vibrato_plus_vol_slide(struct protracker_state *s, struct pt_channel *ch) {
	pt_vibrato2(s, ch);
	pt_volume_slide(ch);
}

// [=]===^=[ pt_tremolo ]=========================================================================[=]
// The ramp branch reads n_vibratopos instead of n_tremolopos: that is a PT2.3d
// bug, kept intentionally.
static void pt_tremolo(struct protracker_state *s, struct pt_channel *ch) {
	int16_t tremolo_data;
	if((ch->n_cmd & 0x0f) > 0) {
		ch->n_tremolocmd = (uint8_t)((ch->n_tremolocmd & 0xf0) | (ch->n_cmd & 0x0f));
	}
	if((ch->n_cmd & 0xf0) > 0) {
		ch->n_tremolocmd = (uint8_t)((ch->n_cmd & 0xf0) | (ch->n_tremolocmd & 0x0f));
	}
	uint8_t pos = (uint8_t)((ch->n_tremolopos >> 2) & 0x1f);
	uint8_t type = (uint8_t)((ch->n_wavecontrol >> 4) & 3);
	if(type == 0) {
		tremolo_data = (int16_t)pt_vibrato_table[pos];
	} else if(type == 1) {
		if(ch->n_vibratopos < 128) {
			tremolo_data = (int16_t)(pos << 3);
		} else {
			tremolo_data = (int16_t)(255 - (pos << 3));
		}
	} else {
		tremolo_data = 255;
	}
	tremolo_data = (int16_t)(((uint16_t)tremolo_data * (ch->n_tremolocmd & 0xf)) >> 6);
	if(ch->n_tremolopos < 128) {
		tremolo_data = (int16_t)(ch->n_volume + tremolo_data);
		if(tremolo_data > 64) {
			tremolo_data = 64;
		}
	} else {
		tremolo_data = (int16_t)(ch->n_volume - tremolo_data);
		if(tremolo_data < 0) {
			tremolo_data = 0;
		}
	}
	pt_paula_volume(s, ch, tremolo_data);
	ch->n_tremolopos += (uint8_t)((ch->n_tremolocmd >> 2) & 0x3c);
}

// [=]===^=[ pt_sample_offset ]===================================================================[=]
// 9xx with memory. PT moves the start cursor and shortens the play length;
// past-end truncates to a single word. Here the wrap point is unchanged
// (base + original length) and only the read cursor moves, which is the
// equivalent observable Paula behaviour.
static void pt_sample_offset(struct pt_channel *ch) {
	if((ch->n_cmd & 0xff) > 0) {
		ch->n_sampleoffset = (uint8_t)(ch->n_cmd & 0xff);
	}
	uint32_t new_offset = (uint32_t)ch->n_sampleoffset << 8;   // bytes (xx * 256)
	if(new_offset < ch->n_play_len) {
		ch->n_start_off = new_offset;
	} else {
		ch->n_start_off = 0;
		ch->n_play_len = 2;
		ch->n_loop_len = 0;
	}
}

// [=]===^=[ pt_e_commands ]======================================================================[=]
static void pt_e_commands(struct protracker_state *s, struct pt_channel *ch) {
	uint8_t ecmd = (uint8_t)((ch->n_cmd & 0x00f0) >> 4);
	switch(ecmd) {
		case 0x0:
			pt_filter_on_off(s, ch);
			return;

		case 0x1:
			pt_fine_porta_up(s, ch);
			return;

		case 0x2:
			pt_fine_porta_down(s, ch);
			return;

		case 0x3:
			pt_set_gliss_control(ch);
			return;

		case 0x4:
			pt_set_vibrato_control(ch);
			return;

		case 0x5:
			pt_set_finetune(ch);
			return;

		case 0x6:
			pt_jump_loop(s, ch);
			return;

		case 0x7:
			pt_set_tremolo_control(ch);
			return;

		case 0x8:
			// E8x (Karplus-Strong) is, in >95% of real modules, demo sync;
			// PT2.3d disables it by default and so does this port.
			return;

		case 0x9:
			pt_do_retrig_note(s, ch);
			return;

		case 0xa:
			pt_volume_fine_up(s, ch);
			return;

		case 0xb:
			pt_volume_fine_down(s, ch);
			return;

		case 0xc:
			pt_note_cut(s, ch);
			return;

		case 0xd:
			pt_note_delay(s, ch);
			return;

		case 0xe:
			pt_pattern_delay(s, ch);
			return;

		case 0xf:
			pt_funk_it(s, ch);
			return;

		default:
			break;
	}
}

// [=]===^=[ pt_check_more_effects ]==============================================================[=]
// The tick-0 effect path: position/structure effects, sample offset, set
// speed, the E-commands, set volume, otherwise just (re)assert the period.
static void pt_check_more_effects(struct protracker_state *s, struct pt_channel *ch) {
	uint8_t cmd = (uint8_t)((ch->n_cmd & 0x0f00) >> 8);
	switch(cmd) {
		case 0x9:
			pt_sample_offset(ch);
			return;

		case 0xb:
			pt_position_jump(s, ch);
			return;

		case 0xd:
			pt_pattern_break(s, ch);
			return;

		case 0xe:
			pt_e_commands(s, ch);
			return;

		case 0xf:
			pt_set_speed_cmd(s, ch);
			return;

		case 0xc:
			pt_volume_change(ch);
			return;

		default:
			break;
	}
	pt_paula_period(s, ch, ch->n_period);
}

// [=]===^=[ pt_chkefx2 ]=========================================================================[=]
// The tick-> 0 effect path: funk update, then the continuous effects.
static void pt_chkefx2(struct protracker_state *s, struct pt_channel *ch) {
	pt_update_funk(s, ch);
	if((ch->n_cmd & 0xfff) == 0) {
		return;
	}
	uint8_t cmd = (uint8_t)((ch->n_cmd & 0x0f00) >> 8);
	switch(cmd) {
		case 0x0:
			pt_arpeggio(s, ch);
			return;

		case 0x1:
			pt_porta_up(s, ch);
			return;

		case 0x2:
			pt_porta_down(s, ch);
			return;

		case 0x3:
			pt_tone_portamento(s, ch);
			return;

		case 0x4:
			pt_vibrato(s, ch);
			return;

		case 0x5:
			pt_tone_plus_vol_slide(s, ch);
			return;

		case 0x6:
			pt_vibrato_plus_vol_slide(s, ch);
			return;

		case 0xe:
			pt_e_commands(s, ch);
			return;

		default:
			break;
	}
	pt_paula_period(s, ch, ch->n_period);
	if(cmd == 0x7) {
		pt_tremolo(s, ch);
	} else if(cmd == 0xa) {
		pt_volume_slide(ch);
	}
}

// [=]===^=[ pt_check_effects ]===================================================================[=]
// Per-tick effect entry. After everything except tremolo (which writes the
// volume itself), the channel volume is re-asserted -- the same control flow
// PT's stack trick produces.
static void pt_check_effects(struct protracker_state *s, struct pt_channel *ch) {
	pt_chkefx2(s, ch);
	uint8_t cmd = (uint8_t)((ch->n_cmd & 0x0f00) >> 8);
	if(cmd != 0x7) {
		pt_paula_volume(s, ch, ch->n_volume);
	}
}

// [=]===^=[ pt_set_period ]======================================================================[=]
// Canonicalise the cell's raw period through the finetune-0 table, apply this
// channel's finetune, and (unless a note delay is pending) retrigger the
// sample. The vibrato/tremolo positions reset unless their E4x/E7x continue
// bit is set.
static void pt_set_period(struct protracker_state *s, struct pt_channel *ch) {
	uint16_t note = (uint16_t)(ch->n_note & 0xfff);
	int32_t i;
	for(i = 0; i < 37; ++i) {
		if(note >= pt_period_table[i]) {
			break;
		}
	}
	ch->n_period = pt_period_table[(ch->n_finetune * 37) + i];

	if((ch->n_cmd & 0xff0) != 0xed0) {
		if((ch->n_wavecontrol & 0x04) == 0) {
			ch->n_vibratopos = 0;
		}
		if((ch->n_wavecontrol & 0x40) == 0) {
			ch->n_tremolopos = 0;
		}
		pt_do_retrg(s, ch);
	}
	pt_check_more_effects(s, ch);
}

// [=]===^=[ pt_load_sample_to_channel ]==========================================================[=]
// Translate a sample header into the channel's Paula geometry. PT's loop
// model: with a loop, the first pass spans only [0, loopstart+looplen) and
// the tail past the loop is never played; without a loop the whole sample
// plays once, then DMA continues from the (often silent) 2-byte head.
static void pt_load_sample_to_channel(struct protracker_state *s, struct pt_channel *ch, uint8_t sample_no) {
	ch->n_samplenum = (uint8_t)(sample_no - 1);
	struct pt_sample *sm = &s->samples[ch->n_samplenum];
	ch->n_base_off = sm->data_off;
	ch->n_finetune = (uint8_t)(sm->finetune & 0xf);
	ch->n_volume = (int16_t)sm->volume;
	ch->n_start_off = 0;
	if(sm->loop_start > 0 && sm->loop_length >= 2) {
		ch->n_loop_start = sm->loop_start;
		ch->n_loop_len = sm->loop_length;
		ch->n_play_len = sm->loop_start + sm->loop_length;
	} else if(sm->loop_length >= 4) {
		ch->n_loop_start = 0;
		ch->n_loop_len = sm->loop_length;
		ch->n_play_len = sm->length;
	} else {
		ch->n_loop_start = 0;
		ch->n_loop_len = 0;
		ch->n_play_len = sm->length;
	}
	ch->n_wavestart = ch->n_loop_start;
	if(ch->n_play_len > sm->length) {
		ch->n_play_len = sm->length;
	}
	ch->n_has_sample = (sm->length > 0) ? 1 : 0;
}

// [=]===^=[ pt_play_voice ]======================================================================[=]
// Tick-0 per-channel row processing: read the cell, latch sample/finetune/
// volume, then branch exactly as PT (E5x set-finetune, tone-porta 3/5,
// sample-offset 9, plain note, or effect-only continuation).
static void pt_play_voice(struct protracker_state *s, struct pt_channel *ch) {
	uint8_t pat = (uint8_t)s->mod_pattern;
	uint32_t row_off = PT_PAT_DATA_OFF + (uint32_t)pat * PT_PAT_BYTES + (uint32_t)s->song_row * PT_BYTES_PER_ROW;
	uint8_t *cell = s->module_data + row_off + (uint32_t)ch->index * 4;

	uint16_t period = (uint16_t)((((uint16_t)cell[0] & 0x0f) << 8) | (uint16_t)cell[1]);
	uint8_t  sample_no = (uint8_t)((cell[0] & 0xf0) | (cell[2] >> 4));
	uint8_t  command = (uint8_t)(cell[2] & 0x0f);
	uint8_t  param = cell[3];

	if(ch->n_note == 0 && ch->n_cmd == 0) {
		pt_paula_period(s, ch, ch->n_period);
	}

	ch->n_note = period;
	ch->n_cmd = (uint16_t)((command << 8) | param);

	if(sample_no >= 1 && sample_no <= PT_NUM_SAMPLES) {
		pt_load_sample_to_channel(s, ch, sample_no);
	}

	if((ch->n_note & 0xfff) > 0) {
		if((ch->n_cmd & 0xff0) == 0xe50) {
			pt_set_finetune(ch);
			pt_set_period(s, ch);
		} else {
			uint8_t cmd = (uint8_t)((ch->n_cmd & 0x0f00) >> 8);
			if(cmd == 3 || cmd == 5) {
				pt_set_tone_porta(ch);
				pt_check_more_effects(s, ch);
			} else if(cmd == 9) {
				pt_check_more_effects(s, ch);
				pt_set_period(s, ch);
			} else {
				pt_set_period(s, ch);
			}
		}
	} else {
		pt_check_more_effects(s, ch);
	}
}

// [=]===^=[ pt_next_position ]===================================================================[=]
static void pt_next_position(struct protracker_state *s) {
	s->song_row = s->pbreak_position;
	s->pbreak_position = 0;
	s->posjump_assert = 0;
	s->mod_pos = (s->mod_pos + 1) & 127;
	if(s->mod_pos >= s->song_length) {
		s->mod_pos = 0;
		s->end_reached = 1;
	}
	s->mod_pattern = s->order[s->mod_pos];
}

// [=]===^=[ pt_tick ]============================================================================[=]
// One replayer tick, structured exactly as PT2.3d's tickReplayer: the CIA BPM
// change is applied a tick late, tick wraps to 0 to read a new row (or, under
// pattern delay, just re-runs effects), the row advances with the pattern-
// delay rewind, then pbreak/posjump are resolved.
static void pt_tick(struct protracker_state *s) {
	if(s->cia_set_bpm != -1) {
		pt_set_tempo(s, s->cia_set_bpm);
		s->cia_set_bpm = -1;
	}

	s->song_tick++;
	int32_t read_new = 0;
	if(s->song_tick >= s->song_speed) {
		s->song_tick = 0;
		read_new = 1;
	}

	if(read_new) {
		if(s->patt_del_time2 == 0) {
			for(int32_t i = 0; i < PT_VOICES; ++i) {
				pt_play_voice(s, &s->ch[i]);
				pt_paula_volume(s, &s->ch[i], s->ch[i].n_volume);
			}
		} else {
			for(int32_t i = 0; i < PT_VOICES; ++i) {
				pt_check_effects(s, &s->ch[i]);
			}
		}

		s->song_row++;

		if(s->patt_del_time > 0) {
			s->patt_del_time2 = s->patt_del_time;
			s->patt_del_time = 0;
		}
		if(s->patt_del_time2 > 0) {
			s->patt_del_time2--;
			if(s->patt_del_time2 > 0) {
				s->song_row--;
			}
		}

		if(s->pbreak_flag) {
			s->song_row = s->pbreak_position;
			s->pbreak_position = 0;
			s->pbreak_flag = 0;
		}

		if(s->song_row >= PT_ROWS_PER_PAT || s->posjump_assert) {
			pt_next_position(s);
		}
	} else {
		for(int32_t i = 0; i < PT_VOICES; ++i) {
			pt_check_effects(s, &s->ch[i]);
		}
		if(s->posjump_assert) {
			pt_next_position(s);
		}
	}

	if(s->stop_song) {
		s->stop_song = 0;
		s->mod_pos = 0;
		s->song_row = 0;
		s->song_tick = 0;
		s->mod_pattern = s->order[0];
		s->end_reached = 1;
	}
}

// [=]===^=[ pt_load ]============================================================================[=]
static int32_t pt_load(struct protracker_state *s) {
	uint8_t *d = s->module_data;
	uint32_t len = s->module_len;

	s->song_length = d[PT_SONGLEN_OFF];
	s->restart = d[PT_RESTART_OFF];
	memcpy(s->order, d + PT_ORDER_OFF, PT_ORDER_LEN);
	s->num_patterns = pt_count_patterns(s->order);

	uint32_t cursor = PT_PAT_DATA_OFF + s->num_patterns * PT_PAT_BYTES;
	if(cursor > len) {
		return 0;
	}

	for(uint32_t i = 0; i < PT_NUM_SAMPLES; ++i) {
		uint8_t *h = d + PT_SAMPLES_OFF + i * PT_SAMPLE_HDR;
		uint32_t length_b   = (uint32_t)pt_read_u16_be(h + 22) * 2u;
		uint8_t  finetune   = (uint8_t)(h[24] & 0x0f);
		uint8_t  volume     = h[25];
		uint32_t loop_b     = (uint32_t)pt_read_u16_be(h + 26) * 2u;
		uint32_t loop_len_b = (uint32_t)pt_read_u16_be(h + 28) * 2u;

		struct pt_sample *sm = &s->samples[i];
		sm->data_off = cursor;
		sm->length = length_b;
		sm->finetune = finetune;
		sm->volume = (volume > 64) ? 64 : volume;
		sm->loop_start = loop_b;
		sm->loop_length = loop_len_b;
		// Clamp a loop that runs past the sample so Paula never reads beyond it.
		if(sm->loop_start + sm->loop_length > length_b) {
			if(sm->loop_start >= length_b) {
				sm->loop_start = 0;
				sm->loop_length = 0;
			} else {
				sm->loop_length = length_b - sm->loop_start;
			}
		}
		cursor += length_b;
		// Real-world rips pad or truncate the final sample; tolerate it
		// rather than reject an otherwise valid module.
		if(cursor > len) {
			if(sm->data_off >= len) {
				sm->data_off = 0;
				sm->length = 0;
				sm->loop_start = 0;
				sm->loop_length = 0;
			} else {
				sm->length = len - sm->data_off;
				if(sm->loop_start + sm->loop_length > sm->length) {
					sm->loop_start = 0;
					sm->loop_length = 0;
				}
			}
			cursor = len;
		}
	}
	return 1;
}

// [=]===^=[ protracker_init ]====================================================================[=]
static struct protracker_state *protracker_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < PT_PAT_DATA_OFF || sample_rate < 8000) {
		return 0;
	}
	if(!pt_identify((uint8_t *)data, len)) {
		return 0;
	}
	struct protracker_state *s = (struct protracker_state *)calloc(1, sizeof(struct protracker_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;
	if(!pt_load(s)) {
		free(s);
		return 0;
	}

#ifdef PROTRACKER_TIMING
	s->timing_mode = PROTRACKER_TIMING;
#else
	s->timing_mode = pt_detect_vblank(s);
#endif

	paula_init(&s->paula, sample_rate, (int32_t)PT_VBLANK_HZ);
	for(int32_t i = 0; i < PT_VOICES; ++i) {
		s->ch[i].index = i;
	}
	s->low_mask = 0xff;
	s->cia_set_bpm = -1;
	s->song_speed = PT_DEF_SPEED;
	s->mod_pos = 0;
	s->mod_pattern = s->order[0];
	s->song_row = 0;
	s->song_tick = 0;
	pt_set_tempo(s, PT_DEF_BPM);
	return s;
}

// [=]===^=[ protracker_free ]====================================================================[=]
static void protracker_free(struct protracker_state *s) {
	if(s) {
		free(s);
	}
}

// [=]===^=[ protracker_get_audio ]===============================================================[=]
static void protracker_get_audio(struct protracker_state *s, float *output, int32_t frames) {
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
			pt_tick(s);
		}
	}
}

// [=]===^=[ protracker_api_init ]================================================================[=]
static void *protracker_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return protracker_init(data, len, sample_rate);
}

// [=]===^=[ protracker_api_free ]================================================================[=]
static void protracker_api_free(void *state) {
	protracker_free((struct protracker_state *)state);
}

// [=]===^=[ protracker_api_get_audio ]===========================================================[=]
static void protracker_api_get_audio(void *state, float *output, int32_t frames) {
	protracker_get_audio((struct protracker_state *)state, output, frames);
}

static const char *protracker_extensions[] = { "mod", 0 };

static struct player_api protracker_api = {
	"ProTracker 2.3d",
	protracker_extensions,
	protracker_api_init,
	protracker_api_free,
	protracker_api_get_audio,
	0,
};
