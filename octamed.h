// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// OctaMED replayer, ported from NostalgicPlayer's C# implementation.
// Accepts MMD0, MMD1, MMD2 and MMD3 modules (4-byte mark "MMD0".."MMD3").
// MMDC (the packed wrapper) magic is also accepted at identification time
// because TestModule treats markVersion 'C' as MedPacker; the actual block
// data of an unpacked MMDC is laid out the same as MMD0, so we treat 'C'
// like '0' during loading.
//
// Drives Paula (see paula.h). 8 virtual channels are available; OctaMED 8-ch
// modules software-mixed two voices per Paula channel on real hardware, but
// here we simply use channels 0..7 directly.
//
// Notes on coverage:
//   - Synth and hybrid instruments: fully supported (per-track SynthHandler with
//     volume- and waveform-table command interpretation, vibrato modulation,
//     arpeggio, envelope waveforms, hybrid sample modulation).
//   - MIDI instruments: not supported.
//   - Multi-octave (5/3/2/4/6/7-octave IFF) samples: stored as a single buffer
//     using the lowest octave; OctaMED's per-octave switching is not modelled.
//     Most real modules use plain mono 8-bit samples.
//   - 16-bit / stereo / packed / delta-coded samples: rejected at load time.
//   - The MMD0..MMD3 effect set is implemented per the original player; effects
//     0x10 (custom MIDI), 0x1c (MIDI preset), 0x2d (ARexx), and the filter sweep
//     family (0x23..0x25) are no-ops since they require MIDI/Amiga hardware.
//   - Echo / stereo separation effect group state is parsed but not rendered.
//   - MMD2/MMD3 sections and multiple play sequences are supported, including
//     the in-pseq Stop and PosJump commands.
//   - MMD1+ block extra command pages and 0x0c00xx kludge tables are read but
//     only page 0 is interpreted by the player (subsequent pages mostly carry
//     MIDI/synth controls that we cannot render).
//   - MMD3 per-instrument LongRepeat/LongRepLen and InstrFlag.Loop/PingPong/
//     Disabled are honoured.
//
// Public API:
//   struct octamed_state *octamed_init(void *data, uint32_t len, int32_t sample_rate);
//   void octamed_free(struct octamed_state *s);
//   void octamed_get_audio(struct octamed_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#ifdef OCTAMED_DEBUG_NOTES
#include <stdio.h>
#endif

#define OCTAMED_MAX_TRACKS    8
#define OCTAMED_MAX_INSTR     63
#define OCTAMED_OCTAVES       6
#define OCTAMED_MAX_BLOCKS    1024
#define OCTAMED_MAX_PSEQ      1024
#define OCTAMED_MAX_SECTIONS  256
#define OCTAMED_MAX_PLAYSEQS  256

// Special note numbers from the MMD0..MMD3 grammar. Only NOTE_STP is meaningful
// for the player's note column.
#define OCTAMED_NOTE_STP      0x80

// Instrument flags derived during loading.
#define OCTAMED_IFLAG_LOOP        0x01
#define OCTAMED_IFLAG_PINGPONG    0x08
#define OCTAMED_IFLAG_DISABLED    0x04

// Track misc flags during play.
#define OCTAMED_MISC_BACKWARDS    0x01
#define OCTAMED_MISC_STOPNOTE     0x04

// Break enum values.
#define OCTAMED_BREAK_NORMAL    0
#define OCTAMED_BREAK_PATTERN   1
#define OCTAMED_BREAK_LOOP      2
#define OCTAMED_BREAK_POSJUMP   3

// MmdFlag bits.
#define OCTAMED_FLAG_FILTERON     0x01
#define OCTAMED_FLAG_VOLHEX       0x10
#define OCTAMED_FLAG_STSLIDE      0x20
#define OCTAMED_FLAG_8CHAN        0x40

// MmdFlag2 bits.
#define OCTAMED_FLAG2_BMASK       0x1f
#define OCTAMED_FLAG2_BPM         0x20
#define OCTAMED_FLAG2_MIX         0x80

// PlaySeq command codes (matches PSeqCmd enum).
#define OCTAMED_PSCMD_NONE      0
#define OCTAMED_PSCMD_STOP      1
#define OCTAMED_PSCMD_POSJUMP   2

// Synth waveform/volume table command bytes. Common to vol and wf tables.
#define OCTAMED_SYN_CMD_SPD     0xf0
#define OCTAMED_SYN_CMD_WAI     0xf1
#define OCTAMED_SYN_CMD_CHD     0xf2
#define OCTAMED_SYN_CMD_CHU     0xf3
#define OCTAMED_SYN_CMD_RES     0xf6
#define OCTAMED_SYN_CMD_HLT     0xfb
#define OCTAMED_SYN_CMD_JMP     0xfe
#define OCTAMED_SYN_CMD_END     0xff

// Volume-table-only commands.
#define OCTAMED_SYN_VOL_EN1     0xf4
#define OCTAMED_SYN_VOL_EN2     0xf5
#define OCTAMED_SYN_VOL_JWS     0xfa

// Waveform-table-only commands.
#define OCTAMED_SYN_WF_VBD      0xf4
#define OCTAMED_SYN_WF_VBS      0xf5
#define OCTAMED_SYN_WF_VWF      0xf7
#define OCTAMED_SYN_WF_JVS      0xfa
#define OCTAMED_SYN_WF_ARP      0xfc
#define OCTAMED_SYN_WF_ARE      0xfd

// Per-track synth playback type. Matches C# SynthData.SyType.
#define OCTAMED_SYTYPE_NONE     0
#define OCTAMED_SYTYPE_SYNTH    1
#define OCTAMED_SYTYPE_HYBRID   2

// MiscFlag.NoSynthWfPtrReset
#define OCTAMED_MISC_NOSYNTHRST 0x02

struct octamed_synth_wf {
	int8_t *data;                  // signed 8-bit waveform samples
	uint16_t length;               // length in 16-bit WORDS, matching C# SyWfLength
};

struct octamed_synth_sound {
	uint32_t vol_table_len;
	uint32_t wf_table_len;
	uint32_t vol_speed;
	uint32_t wf_speed;
	uint8_t vol_table[128];
	uint8_t wf_table[128];
	struct octamed_synth_wf *waveforms;
	uint32_t num_waveforms;
};

struct octamed_note {
	uint8_t note;
	uint8_t instr;
	uint8_t cmd;
	uint8_t data0;
	uint8_t data1;
};

struct octamed_block {
	uint8_t tracks;
	uint16_t lines;
	struct octamed_note *grid;     // [lines * tracks]
};

struct octamed_instr {
	int8_t *sample_data;           // base allocation (covers all octaves)
	uint32_t sample_length;        // length of octave 0 (default selection)
	uint32_t octave_offset[7];     // byte offset of each octave within sample_data
	uint32_t octave_length[7];     // length of each octave's buffer in bytes
	uint8_t num_octaves;           // 1..7 (multiOctaveCount[type & 0x07])
	uint8_t sample_type;           // type & 0x07 (multi-octave layout selector)
	uint32_t loop_start;           // base loop_start (octave 0 reference)
	uint32_t loop_length;          // base loop_length
	uint8_t init_vol;              // 0..128
	uint8_t cur_vol;
	int8_t s_trans;                // sample transpose
	int8_t fine_tune;              // -8..7
	uint8_t hold;
	uint8_t decay;
	uint8_t flags;
	uint8_t valid;                 // 1 if this slot has any data (sample present)
	struct octamed_synth_sound *synth;	// non-NULL for synth/hybrid instruments
};

struct octamed_track {
	uint8_t prev_note;
	uint8_t prev_inum;             // 0..MAX_INSTR-1
	uint8_t prev_vol;              // 0..128
	uint8_t misc_flags;
	int32_t note_off_cnt;          // -1 = none
	uint32_t init_hold;
	uint32_t init_decay;
	uint32_t decay;
	uint32_t fade_speed;
	int32_t s_transp;
	int32_t fine_tune;
	int32_t arp_adjust;
	int32_t vibr_adjust;
	uint32_t s_offset;
	uint8_t curr_note;
	uint8_t fx_type;               // 0=normal,1=none/noplay
	int32_t frequency;
	int32_t port_target_freq;
	uint16_t port_speed;
	uint8_t vib_shift;
	uint8_t vib_speed;
	uint8_t vib_size;
	uint16_t vib_offs;
	uint8_t temp_vol;              // +1; 0 = none

	// Per-track synth playback state. Mirrors C# SynthData verbatim.
	uint8_t sy_type;                       // OCTAMED_SYTYPE_*
	int32_t sy_period_change;
	uint32_t sy_vib_offs;
	uint32_t sy_vib_speed;
	int32_t sy_vib_dep;
	uint32_t sy_vib_wf_num;                // 0 = default sine
	uint32_t sy_arp_start;
	uint32_t sy_arp_offs;
	uint32_t sy_vol_cmd_pos;
	uint32_t sy_wf_cmd_pos;
	uint32_t sy_vol_wait;
	uint32_t sy_wf_wait;
	uint32_t sy_vol_chg_speed;
	uint32_t sy_wf_chg_speed;
	uint32_t sy_vol_x_speed;
	uint32_t sy_wf_x_speed;
	int32_t sy_vol_x_cnt;
	int32_t sy_wf_x_cnt;
	uint32_t sy_env_wf_num;                // 0 = none
	uint8_t sy_env_loop;
	uint16_t sy_env_count;
	int32_t sy_vol;
	uint8_t sy_note_number;
	uint8_t sy_start;                      // C# Mixer.startSyn[chNum] flag
	uint32_t sy_decay;                     // mirrors TrkDecay (used for synth decay JMP target)
};

struct octamed_pseq_entry {
	uint16_t value;                // block number, or jump target if cmd is PosJump
	uint8_t cmd;                   // OCTAMED_PSCMD_*
};

struct octamed_pseq {
	struct octamed_pseq_entry *entries;
	uint32_t count;
};

struct octamed_state {
	struct paula paula;

	int32_t sample_rate;

	// Format marker: '0', '1', '2', '3', or 'C'.
	uint8_t mark_version;

	// Module data (heap, owned by state).
	struct octamed_block blocks[OCTAMED_MAX_BLOCKS];
	uint32_t num_blocks;
	struct octamed_instr instr[OCTAMED_MAX_INSTR];

	// Sections + multiple play sequences (MMD2/MMD3). MMD0/MMD1 collapses to
	// 1 section with 1 pseq.
	struct octamed_pseq pseqs[OCTAMED_MAX_PLAYSEQS];
	uint32_t num_pseqs;
	uint16_t sections[OCTAMED_MAX_SECTIONS];     // each is a pseq index
	uint32_t num_sections;

	uint8_t num_samples;

	// Tempo state.
	uint16_t tempo_bpm;
	uint16_t ticks_per_line;
	uint16_t lines_per_beat;
	uint8_t bpm_mode;

	int8_t play_transp;
	uint8_t master_vol;
	uint8_t track_vol[16];
	uint8_t num_channels;          // 4 or 8
	uint8_t slide_first;
	uint8_t mix_conv;              // needs old-to-mix transpose conversion?
	uint8_t eight_ch_conv;         // had 8-channel flag?
	uint8_t vol_hex;

	// Per-track state.
	struct octamed_track td[OCTAMED_MAX_TRACKS];

	// Player position.
	uint32_t cur_section;          // index into sections[]
	uint32_t cur_pseq;             // current pseq index (mirrors sections[cur_section])
	uint32_t pseq_pos;             // index into pseqs[cur_pseq].entries[]
	uint16_t cur_block;
	uint16_t cur_line;
	int32_t pulse_ctr;
	uint32_t block_delay;
	uint16_t fx_line;
	uint16_t fx_block;
	uint16_t next_line;
	uint16_t repeat_line;
	uint32_t repeat_counter;
	uint8_t breaktype;
	uint8_t delayed_stop;
	uint8_t end_reached;

	// Frequency table: [16 fine tune][72 notes].
	uint16_t freq_table[16][72];

#ifdef OCTAMED_DEBUG_NOTES
	uint64_t frames_played;
#endif
};

// Sine table for vibrato (matches the original 32-entry sineTable).
static int8_t octamed_sine_table[32] = {
	0, 25, 49, 71, 90, 106, 117, 125, 127, 125, 117, 106, 90, 71, 49, 25,
	0, -25, -49, -71, -90, -106, -117, -125, -127, -125, -117, -106, -90, -71, -49, -25
};

// Compatibility table for SoundTracker-style numeric tempo values <=10.
static uint16_t octamed_bpm_comp_vals[10] = {
	195, 97, 65, 49, 39, 32, 28, 24, 22, 20
};

// MmdFlag2.Mix conversion uses these for the +24 transposition kludge for MMD0
// when modules were originally non-mix mode. For 5..8 channel tempo conversion.
static uint8_t octamed_bpm_vals[9] = {
	179, 164, 152, 141, 131, 123, 116, 110, 104
};

// [=]===^=[ octamed_read_u16_be ]================================================================[=]
static uint16_t octamed_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ octamed_read_u32_be ]================================================================[=]
static uint32_t octamed_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ octamed_read_i16_be ]================================================================[=]
static int16_t octamed_read_i16_be(uint8_t *p) {
	return (int16_t)octamed_read_u16_be(p);
}

// [=]===^=[ octamed_identify ]===================================================================[=]
// Accepts MMD0, MMD1, MMD2, MMD3 and MMDC.
static int32_t octamed_identify(uint8_t *data, uint32_t len) {
	if(len < 840) {
		return 0;
	}
	if(data[0] != 'M' || data[1] != 'M' || data[2] != 'D') {
		return 0;
	}
	uint8_t v = data[3];
	if((v < '0' || v > '3') && (v != 'C')) {
		return 0;
	}
	return 1;
}

// [=]===^=[ octamed_build_freq_table ]===========================================================[=]
// Replicates the C# Mixer constructor. Frequencies are sample-playback rates in
// Hz used by the NostalgicPlayer mixer; we feed them to Paula via period.
static void octamed_build_freq_table(struct octamed_state *s) {
	for(int32_t ft = 0; ft < 16; ++ft) {
		double v = 1046.502261 + (ft - 8) * 7.778532836;
		for(int32_t n = 0; n < 72; ++n) {
			uint32_t iv = (uint32_t)v;
			if(iv > 65535) {
				iv = 65535;
			}
			s->freq_table[ft][n] = (uint16_t)iv;
			v *= 1.059463094;
		}
	}
}

// [=]===^=[ octamed_get_note_freq ]==============================================================[=]
static uint32_t octamed_get_note_freq(struct octamed_state *s, uint8_t note, int32_t fine_tune) {
	int32_t ft = fine_tune + 8;
	if(ft < 0) {
		ft = 0;
	}
	if(ft > 15) {
		ft = 15;
	}
	if(note > 71) {
		note = 71;
	}
	return s->freq_table[ft][note];
}

// Multi-octave buffer-selection table. Indexed by [sampleType - 1][octave],
// where octave = note / 12 clamped to 0..5. Mirrors C# Sample.multiOctaveBufferIndex.
static int8_t octamed_multi_oct_buf_idx[6][6] = {
	{ 4, 4, 3, 2, 2, 1 },		// type 1 -> 5 octaves
	{ 2, 2, 2, 2, 2, 1 },		// type 2 -> 3 octaves
	{ 1, 1, 0, 0, 0, 0 },		// type 3 -> 2 octaves
	{ 3, 3, 2, 2, 2, 1 },		// type 4 -> 4 octaves
	{ 5, 5, 5, 4, 3, 2 },		// type 5 -> 6 octaves
	{ 6, 6, 6, 5, 4, 3 },		// type 6 -> 7 octaves
};

// Mirrors C# Sample.multiOctaveStart. Per-octave note-difference applied to
// the requested note when picking the corresponding buffer.
static int8_t octamed_multi_oct_start[6][6] = {
	{  12,  12,   0, -12, -12, -24 },	// 5
	{   0,   0,   0,   0,   0, -12 },	// 3
	{   0,   0, -12, -12, -12, -12 },	// 2
	{   0,   0, -12, -12, -12, -24 },	// 4
	{   0,   0,   0, -12, -24, -36 },	// 6
	{   0,   0,   0, -12, -24, -36 },	// 7
};

// [=]===^=[ octamed_get_instr_note_freq ]========================================================[=]
// Mirrors C# Mixer.GetInstrNoteFreq. Synth/hybrid instruments take the special
// path: clamp note to <= 60 then look up directly. Regular samples apply the
// multi-octave note difference for the chosen buffer first.
static uint32_t octamed_get_instr_note_freq(struct octamed_state *s, uint8_t note, struct octamed_instr *ci) {
	if(note <= 0x7f) {
		if(ci->synth != 0) {
			while(note > 60) {
				note -= 12;
			}
			if(note == 0) {
				return 0;
			}
			return octamed_get_note_freq(s, (uint8_t)(note - 1), ci->fine_tune);
		}
		if(ci->sample_type != 0 && ci->num_octaves > 1) {
			int32_t oct = (int32_t)(note - 1) / 12;
			if(oct < 0) {
				oct = 0;
			}
			if(oct > 5) {
				oct = 5;
			}
			int32_t adjusted = (int32_t)note + octamed_multi_oct_start[ci->sample_type - 1][oct];
			if(adjusted < 1) {
				adjusted = 1;
			}
			if(adjusted > 0x7f) {
				adjusted = 0x7f;
			}
			note = (uint8_t)adjusted;
		}
		while(note > 60) {
			note -= 12;
		}
		if(note == 0) {
			return 0;
		}
		return octamed_get_note_freq(s, (uint8_t)(note - 1), ci->fine_tune);
	}
	return 0;
}

// [=]===^=[ octamed_set_paula_freq ]=============================================================[=]
// Set channel playback rate in Hz directly. C# OctaMed Mixer.SetChannelFreq
// calls VirtualChannels[chNum].SetFrequency(freq), not SetAmigaPeriod, so we
// match it via paula_set_freq_hz -- this avoids the PAULA_MIN_PERIOD clamp on
// notes whose intended rate exceeds the Amiga hardware limit.
static void octamed_set_paula_freq(struct octamed_state *s, int32_t idx, int32_t freq) {
	if(freq <= 0) {
		paula_mute(&s->paula, idx);
		return;
	}
	paula_set_freq_hz(&s->paula, idx, (uint32_t)freq);
}

// [=]===^=[ octamed_set_mix_tempo ]==============================================================[=]
// Compute the player tick rate from current tempo settings and update Paula's
// samples-per-tick. BPM mode: hz = tempo * lpb / 10. Else SoundTracker-style.
static void octamed_set_mix_tempo(struct octamed_state *s) {
	float hz;
	if(s->bpm_mode) {
		hz = (float)s->tempo_bpm * (float)s->lines_per_beat / 10.0f;
	} else {
		uint16_t t = s->tempo_bpm;
		if(t == 0) {
			t = 1;
		}
		if(t <= 10) {
			t = octamed_bpm_comp_vals[t - 1];
		}
		hz = 1.0f / (0.474326f / (float)t * 1.3968255f);
	}
	if(hz < 1.0f) {
		hz = 50.0f;
	}
	int32_t spt = (int32_t)((float)s->sample_rate / hz);
	if(spt < 1) {
		spt = 1;
	}
	s->paula.samples_per_tick = spt;
	if(s->paula.tick_offset > spt) {
		s->paula.tick_offset = 0;
	}
}

// [=]===^=[ octamed_cleanup ]====================================================================[=]
static void octamed_cleanup(struct octamed_state *s) {
	for(uint32_t i = 0; i < s->num_blocks; ++i) {
		if(s->blocks[i].grid) {
			free(s->blocks[i].grid);
			s->blocks[i].grid = 0;
		}
	}
	s->num_blocks = 0;
	for(uint32_t i = 0; i < s->num_pseqs; ++i) {
		if(s->pseqs[i].entries) {
			free(s->pseqs[i].entries);
			s->pseqs[i].entries = 0;
		}
		s->pseqs[i].count = 0;
	}
	s->num_pseqs = 0;
	for(int32_t i = 0; i < OCTAMED_MAX_INSTR; ++i) {
		if(s->instr[i].sample_data) {
			free(s->instr[i].sample_data);
			s->instr[i].sample_data = 0;
		}
		if(s->instr[i].synth) {
			struct octamed_synth_sound *ss = s->instr[i].synth;
			if(ss->waveforms) {
				for(uint32_t w = 0; w < ss->num_waveforms; ++w) {
					if(ss->waveforms[w].data) {
						free(ss->waveforms[w].data);
					}
				}
				free(ss->waveforms);
			}
			free(ss);
			s->instr[i].synth = 0;
		}
	}
}

// [=]===^=[ octamed_apply_old_vol ]==============================================================[=]
// Replicates ScanConvertOldVolToNew for the volume command (0x0c).
// data1 holds the second arg byte (filled by the MMD1+ extended cmd table or
// by the MMD0 single-byte form). When data1 != 0 we promote it; otherwise on
// non-vol_hex modules apply the hex-to-decimal correction.
static void octamed_apply_old_vol(struct octamed_state *s) {
	for(uint32_t b = 0; b < s->num_blocks; ++b) {
		struct octamed_block *blk = &s->blocks[b];
		uint32_t total = (uint32_t)blk->lines * (uint32_t)blk->tracks;
		for(uint32_t i = 0; i < total; ++i) {
			struct octamed_note *n = &blk->grid[i];
			if(n->cmd != 0x0c) {
				continue;
			}
			uint8_t d2 = n->data1;
			if(d2 != 0) {
				n->data0 = d2;
				n->data1 = 0;
			} else {
				if(!s->vol_hex) {
					uint8_t d = n->data0;
					if((d != 0) && (d < 128)) {
						d = (uint8_t)((d >> 4) * 10 + (d & 0x0f));
						n->data0 = d;
					}
				}
			}
		}
	}
}

// [=]===^=[ octamed_apply_mix_conv ]=============================================================[=]
// Replicates C# ScanSongConvertToMixMode.NoteOperation for non-mix-mode
// modules (Flags2.Mix==0). For each note:
//   - if instrument is unused or has a real sample: transpose (+24).
//   - non-multi-octave samples get a pre-clamp: while note+iTrans > 36, note -= 12.
//   - synth-only instruments (no sample data) are skipped; their note is a
//     synth control, not pitch.
// We don't track MIDI separately and don't model synth-only sounds here, so
// only `valid` (real PCM sample) instruments are transposed. type0 (MMD0/MMDC)
// MIDI handling and the multi-octave skip are implemented faithfully.
static void octamed_apply_mix_conv(struct octamed_state *s) {
	uint8_t transp_instr[OCTAMED_MAX_INSTR + 1];
	uint8_t is_multi[OCTAMED_MAX_INSTR + 1];
	int8_t i_trans[OCTAMED_MAX_INSTR + 1];
	transp_instr[0] = 1;
	is_multi[0] = 0;
	i_trans[0] = 0;
	for(uint32_t i = 1; i <= OCTAMED_MAX_INSTR; ++i) {
		struct octamed_instr *ci = &s->instr[i - 1];
		// Treat as transposable when the slot is actually a sample we can
		// play (valid=1). Synth-only / unsupported slots stay untransposed.
		transp_instr[i] = ci->valid ? 1 : 0;
		is_multi[i] = (ci->num_octaves > 1) ? 1 : 0;
		i_trans[i] = ci->s_trans;
	}
	uint8_t last_inum = 0;
	for(uint32_t b = 0; b < s->num_blocks; ++b) {
		struct octamed_block *blk = &s->blocks[b];
		for(int32_t row = 0; row < blk->lines; ++row) {
			for(int32_t t = 0; t < blk->tracks; ++t) {
				struct octamed_note *n = &blk->grid[row * blk->tracks + t];
				if(n->instr != 0) {
					last_inum = n->instr;
				}
				if((n->note != 0) && (n->note <= (0x7f - 24)) && transp_instr[last_inum]) {
					if(!is_multi[last_inum]) {
						// Kludge for broken 4-ch modules that use x-4..x-6
						// instead of x-3: clamp note range pre-transpose.
						while(((int32_t)(int8_t)n->note + (int32_t)i_trans[last_inum]) > 3 * 12) {
							n->note = (uint8_t)(n->note - 12);
						}
					}
					n->note = (uint8_t)(n->note + 24);
				}
			}
		}
	}
}

// [=]===^=[ octamed_apply_8ch_tempo ]============================================================[=]
// Replicates ScanSongConvertTempo for 5..8 channel modules.
static void octamed_apply_8ch_tempo(struct octamed_state *s) {
	for(uint32_t b = 0; b < s->num_blocks; ++b) {
		struct octamed_block *blk = &s->blocks[b];
		uint32_t total = (uint32_t)blk->lines * (uint32_t)blk->tracks;
		for(uint32_t i = 0; i < total; ++i) {
			struct octamed_note *n = &blk->grid[i];
			if(n->cmd != 0x0f) {
				continue;
			}
			uint8_t d = n->data0;
			if((d == 0) || (d > 240)) {
				continue;
			}
			n->data0 = (d >= 10) ? (uint8_t)99 : octamed_bpm_vals[d - 1];
		}
	}
	s->bpm_mode = 1;
	s->lines_per_beat = 4;
	uint16_t old = s->tempo_bpm;
	if(old == 0) {
		old = 1;
	}
	s->tempo_bpm = (old >= 10) ? (uint16_t)99 : octamed_bpm_vals[old - 1];
}

// [=]===^=[ octamed_install_pcm ]================================================================[=]
// Installs PCM data for an instrument slot, building the multi-octave layout
// from the type byte. Used both by regular sample loading and by hybrid
// loading (where the PCM data follows the synth-header MmdSampleHdr).
static int32_t octamed_install_pcm(struct octamed_state *s, uint32_t inum, uint8_t *src, uint32_t num_bytes, uint8_t mtype) {
	struct octamed_instr *ci = &s->instr[inum];
	int8_t *buf;
	static uint8_t multi_octave_count[8] = { 1, 5, 3, 2, 4, 6, 7, 1 };
	uint8_t noct;
	uint32_t base_len;
	uint32_t boff;
	uint32_t olen;

	if(num_bytes == 0) {
		return 1;
	}
	buf = (int8_t *)malloc(num_bytes);
	if(!buf) {
		return 0;
	}
	memcpy(buf, src, num_bytes);
	ci->sample_data = buf;
	noct = multi_octave_count[mtype & 7];
	ci->sample_type = (uint8_t)(mtype & 7);
	ci->num_octaves = noct;
	if(noct == 1) {
		ci->octave_offset[0] = 0;
		ci->octave_length[0] = num_bytes;
	} else {
		base_len = num_bytes / ((1u << noct) - 1u);
		boff = 0;
		olen = base_len;
		for(uint32_t i = 0; i < noct; ++i) {
			ci->octave_offset[i] = boff;
			ci->octave_length[i] = olen;
			boff += olen;
			olen <<= 1;
		}
	}
	ci->sample_length = ci->octave_length[0];
	if(ci->loop_start >= ci->sample_length) {
		ci->loop_start = 0;
		ci->loop_length = 0;
	}
	if(ci->loop_start + ci->loop_length > ci->sample_length) {
		ci->loop_length = ci->sample_length - ci->loop_start;
	}
	if(ci->loop_length > 2) {
		ci->flags |= OCTAMED_IFLAG_LOOP;
	}
	ci->valid = 1;
	return 1;
}

// [=]===^=[ octamed_free_synth ]=================================================================[=]
// Releases a synth sound and all of its waveform data buffers.
static void octamed_free_synth(struct octamed_synth_sound *sy) {
	uint32_t k;
	if(!sy) {
		return;
	}
	if(sy->waveforms) {
		for(k = 0; k < sy->num_waveforms; ++k) {
			if(sy->waveforms[k].data) {
				free(sy->waveforms[k].data);
			}
		}
		free(sy->waveforms);
	}
	free(sy);
}

// [=]===^=[ octamed_load_synth_sound ]===========================================================[=]
// Parses an MMD synth/hybrid instrument starting at `off` (the start of the
// 4-byte length + 2-byte type header). For hybrid (type==-2), the very first
// waveform in the waveform list is actually a complete MmdSampleHdr+PCM sample
// instead of a synth waveform; that PCM is installed as the instrument's
// sample data so that PlrPlayNote's sample trigger path runs in parallel with
// SynthHandler. Returns 1 on success, 0 on hard error.
//
// NOTE(peter): file offsets inside the synth payload (the per-waveform table
// of u32 pointers) are relative to `off` (== C# `startOffs = position - 6`).
static int32_t octamed_load_synth_sound(struct octamed_state *s, uint8_t *data, uint32_t len, uint32_t off, uint32_t inum, uint8_t is_hybrid) {
	struct octamed_synth_sound *sy;
	struct octamed_instr *ci;
	uint32_t hpos;
	uint32_t vol_tbl_len;
	uint32_t wf_tbl_len;
	uint32_t num_wfs;
	uint32_t cnt2;
	uint32_t i;
	uint8_t vol_speed;
	uint8_t wf_speed;
	uint8_t d;
	uint32_t *wf_ptrs;
	uint32_t wpos;
	uint32_t hyb_bytes;
	int16_t hyb_type;
	uint32_t hyb_pos;
	uint32_t avail;
	uint16_t wflen;
	uint32_t wbytes;
	int8_t *wd;

	hpos = off + 6;
	// MmdSynthSound: Decay(1), 3 reserved, Rpt(2), RptLen(2), VolTblLen(2),
	// WfTblLen(2), VolSpeed(1), WfSpeed(1), NumWfs(2). Total 16 bytes.
	if(hpos + 16 > len) {
		return 0;
	}
	// Decay(1) + 3 skipped + Rpt(2) + RptLen(2). All currently informational.
	hpos += 1 + 3 + 2 + 2;
	vol_tbl_len = octamed_read_u16_be(data + hpos); hpos += 2;
	wf_tbl_len = octamed_read_u16_be(data + hpos); hpos += 2;
	vol_speed = data[hpos++];
	wf_speed = data[hpos++];
	num_wfs = octamed_read_u16_be(data + hpos); hpos += 2;

	if(vol_tbl_len > 128 || wf_tbl_len > 128 || num_wfs > 256) {
		return 0;
	}

	sy = (struct octamed_synth_sound *)calloc(1, sizeof(struct octamed_synth_sound));
	if(!sy) {
		return 0;
	}
	sy->vol_speed = vol_speed;
	sy->wf_speed = wf_speed;

	// Volume table (read up to vol_tbl_len, stopping early on END).
	cnt2 = 0;
	while(cnt2 < vol_tbl_len) {
		if(hpos + 1 > len) {
			octamed_free_synth(sy);
			return 0;
		}
		d = data[hpos++];
		sy->vol_table[cnt2++] = d;
		if(d == OCTAMED_SYN_CMD_END) {
			sy->vol_table_len = cnt2;
			hpos += vol_tbl_len - cnt2;
			break;
		}
	}
	if(sy->vol_table_len == 0) {
		sy->vol_table_len = cnt2;
	}
	if(hpos > len) {
		octamed_free_synth(sy);
		return 0;
	}

	// Waveform table.
	cnt2 = 0;
	while(cnt2 < wf_tbl_len) {
		if(hpos + 1 > len) {
			octamed_free_synth(sy);
			return 0;
		}
		d = data[hpos++];
		sy->wf_table[cnt2++] = d;
		if(d == OCTAMED_SYN_CMD_END) {
			sy->wf_table_len = cnt2;
			hpos += wf_tbl_len - cnt2;
			break;
		}
	}
	if(sy->wf_table_len == 0) {
		sy->wf_table_len = cnt2;
	}
	if(hpos > len) {
		octamed_free_synth(sy);
		return 0;
	}

	// NumWfs * u32 pointer table (relative to `off`).
	if(hpos + num_wfs * 4u > len) {
		octamed_free_synth(sy);
		return 0;
	}
	wf_ptrs = (uint32_t *)calloc(num_wfs > 0 ? num_wfs : 1, sizeof(uint32_t));
	if(!wf_ptrs) {
		octamed_free_synth(sy);
		return 0;
	}
	for(i = 0; i < num_wfs; ++i) {
		wf_ptrs[i] = octamed_read_u32_be(data + hpos);
		hpos += 4;
	}

	if(num_wfs > 0) {
		sy->waveforms = (struct octamed_synth_wf *)calloc(num_wfs, sizeof(struct octamed_synth_wf));
		if(!sy->waveforms) {
			free(wf_ptrs);
			octamed_free_synth(sy);
			return 0;
		}
	}
	sy->num_waveforms = num_wfs;

	for(i = 0; i < num_wfs; ++i) {
		wpos = off + wf_ptrs[i];
		if(wpos >= len) {
			free(wf_ptrs);
			octamed_free_synth(sy);
			return 0;
		}
		if(i == 0 && is_hybrid) {
			// First waveform slot for hybrid is a complete MmdSampleHdr
			// (length u32, type i16) followed by raw PCM.
			if(wpos + 6 > len) {
				free(wf_ptrs);
				octamed_free_synth(sy);
				return 0;
			}
			hyb_bytes = octamed_read_u32_be(data + wpos);
			hyb_type = octamed_read_i16_be(data + wpos + 4);
			hyb_pos = wpos + 6;
			if(hyb_type < 0) {
				sy->waveforms[i].data = 0;
				sy->waveforms[i].length = 0;
				continue;
			}
			if((hyb_type & 0x00f0) != 0 || (hyb_type & 0x0f) > 7) {
				// 16-bit/stereo/packed/delta hybrid samples are unsupported.
				sy->waveforms[i].data = 0;
				sy->waveforms[i].length = 0;
				continue;
			}
			if(hyb_pos + hyb_bytes > len) {
				avail = (hyb_pos < len) ? (len - hyb_pos) : 0;
				if(hyb_bytes > avail + 512) {
					free(wf_ptrs);
					octamed_free_synth(sy);
					return 0;
				}
				hyb_bytes = avail;
			}
			if(hyb_bytes != 0) {
				if(!octamed_install_pcm(s, inum, data + hyb_pos, hyb_bytes, (uint8_t)(hyb_type & 7))) {
					free(wf_ptrs);
					octamed_free_synth(sy);
					return 0;
				}
			}
			sy->waveforms[i].data = 0;
			sy->waveforms[i].length = 0;
		} else {
			if(wpos + 2 > len) {
				free(wf_ptrs);
				octamed_free_synth(sy);
				return 0;
			}
			wflen = octamed_read_u16_be(data + wpos);
			wbytes = (uint32_t)wflen * 2u;
			if(wpos + 2 + wbytes > len) {
				avail = (wpos + 2 < len) ? (len - (wpos + 2)) : 0;
				if(wbytes > avail) {
					wbytes = avail;
					wflen = (uint16_t)(wbytes / 2u);
				}
			}
			sy->waveforms[i].length = wflen;
			// C# SynthWf has a fixed 128-byte SyWfData; allocate at least 128
			// (zero-padded) so envelope/vibrato paths can read up to index 127
			// without bounds violations even on short waveforms.
			{
				uint32_t alloc = wbytes > 128u ? wbytes : 128u;
				wd = (int8_t *)calloc(alloc, 1);
				if(!wd) {
					free(wf_ptrs);
					octamed_free_synth(sy);
					return 0;
				}
				if(wbytes > 0) {
					memcpy(wd, data + wpos + 2, wbytes);
				}
				sy->waveforms[i].data = wd;
			}
		}
	}

	free(wf_ptrs);

	ci = &s->instr[inum];
	ci->synth = sy;
	// TN bug-fix from C# OctaMedWorker: synth/hybrid sounds always run at full
	// volume, ignoring the saved instrument volume. Parasol Stars depends on it.
	ci->init_vol = 128;
	ci->cur_vol = 128;
	return 1;
}

// [=]===^=[ octamed_load_sample ]================================================================[=]
// Reads one sample at file offset `off`. Returns 0 on hard error (truncation),
// 1 on success or skipped (16-bit/stereo/packed -> empty slot).
// type < 0 enters the synth/hybrid loader.
static int32_t octamed_load_sample(struct octamed_state *s, uint8_t *data, uint32_t len, uint32_t off, uint32_t inum) {
	if(off + 6 > len) {
		return 0;
	}
	uint32_t num_bytes = octamed_read_u32_be(data + off);
	int16_t type = octamed_read_i16_be(data + off + 4);
	uint32_t hpos = off + 6;
	if(type < 0) {
		// type == -1: pure synth, type == -2: hybrid. type < -2 is rejected by
		// the C# loader.
		if(type < -2) {
			return 1;
		}
		uint8_t is_hybrid = (type == -2) ? 1 : 0;
		if(!octamed_load_synth_sound(s, data, len, off, inum, is_hybrid)) {
			return 0;
		}
		return 1;
	}
	if((type & 0x0f) > 6) {
		return 1;
	}
	if((type & 0x00f0) != 0) {
		// 16-bit / stereo / delta / packed: not supported, leave slot silent.
		return 1;
	}
	if(hpos + num_bytes > len) {
		// C# loader tolerates up to ~512 bytes truncation on the trailing
		// sample; clamp to what we have rather than failing.
		uint32_t avail = (hpos < len) ? (len - hpos) : 0;
		if(num_bytes > avail + 512) {
			return 0;
		}
		num_bytes = avail;
	}
	if(num_bytes == 0) {
		return 1;
	}
	return octamed_install_pcm(s, inum, data + hpos, num_bytes, (uint8_t)(type & 0x07));
}

// [=]===^=[ octamed_alloc_pseq ]=================================================================[=]
// Allocates one pseq slot and returns its index, or 0xffffffff on overflow.
static uint32_t octamed_alloc_pseq(struct octamed_state *s, uint32_t count) {
	if(s->num_pseqs >= OCTAMED_MAX_PLAYSEQS) {
		return 0xffffffffu;
	}
	uint32_t idx = s->num_pseqs;
	struct octamed_pseq *p = &s->pseqs[idx];
	p->count = count;
	if(count == 0) {
		p->entries = 0;
	} else {
		p->entries = (struct octamed_pseq_entry *)calloc(count, sizeof(struct octamed_pseq_entry));
		if(!p->entries) {
			return 0xffffffffu;
		}
	}
	s->num_pseqs++;
	return idx;
}

// [=]===^=[ octamed_parse_block_packed ]=========================================================[=]
// MMD1+ block packed cell format: each cell is 4 bytes (note, instr, cmd, arg).
// libxmp-style RLE prefix is applied per source byte stream.
static int32_t octamed_parse_block_packed(uint8_t *data, uint32_t len, uint32_t pos, uint32_t size, uint8_t *dst) {
	uint32_t j = 0;
	while(j < size) {
		if(pos >= len) {
			return 0;
		}
		uint32_t pack = data[pos++];
		if((pack & 0x80) != 0) {
			uint32_t run = 256 - pack;
			if(run > size - j) {
				run = size - j;
			}
			memset(dst + j, 0, run);
			j += run;
			continue;
		}
		pack++;
		if(pack > size - j) {
			pack = size - j;
		}
		if(pos + pack > len) {
			return 0;
		}
		memcpy(dst + j, data + pos, pack);
		pos += pack;
		j += pack;
	}
	return 1;
}

// [=]===^=[ octamed_load ]=======================================================================[=]
// Parse a complete MMD0..MMD3 module. Returns 1 on success, 0 on error.
static int32_t octamed_load(struct octamed_state *s, uint8_t *data, uint32_t len) {
	uint32_t locals_song_offs;
	uint32_t locals_blocks_offs;
	uint32_t locals_samples_offs;
	uint32_t locals_exp_offs;
	uint32_t pos;
	uint8_t markv;
	uint8_t flags;
	uint8_t flags2;
	uint16_t num_blocks_w;
	uint16_t def_tempo;
	int8_t play_transp;
	uint8_t tempo2;
	uint8_t master_vol;
	uint8_t num_samples;
	uint8_t max_chans;
	uint8_t is_legacy_song;            // MMD0/MMD1 song format
	uint8_t mmd1plus_block;            // MMD1/2/3 packed block format

	if(len < 52) {
		return 0;
	}

	markv = data[3];
	s->mark_version = markv;
	is_legacy_song = ((markv == '0') || (markv == '1') || (markv == 'C')) ? 1 : 0;
	mmd1plus_block = ((markv == '1') || (markv == '2') || (markv == '3')) ? 1 : 0;

	// MMD header (offset 0): id(4) modlen(4) songoffs(4) psecnum(2) pseq(2)
	// blocksoffs(4) flags(1) reserved(3) samplesoffs(4) reserved(4) expoffs(4)
	locals_song_offs = octamed_read_u32_be(data + 8);
	locals_blocks_offs = octamed_read_u32_be(data + 16);
	locals_samples_offs = octamed_read_u32_be(data + 24);
	locals_exp_offs = octamed_read_u32_be(data + 32);

	if(locals_song_offs == 0 || locals_song_offs >= len) {
		return 0;
	}

	// Read 63 sample structures (Mmd0Sample is 8 bytes each).
	pos = locals_song_offs;
	if(pos + 63 * 8 > len) {
		return 0;
	}
	for(uint32_t i = 0; i < 63; ++i) {
		uint16_t rep = octamed_read_u16_be(data + pos);
		uint16_t replen = octamed_read_u16_be(data + pos + 2);
		uint8_t vol = data[pos + 6];
		int8_t strans = (int8_t)data[pos + 7];
		struct octamed_instr *ci = &s->instr[i];
		ci->loop_start = (uint32_t)rep << 1;
		ci->loop_length = (uint32_t)replen << 1;
		ci->s_trans = strans;
		ci->init_vol = (uint8_t)((uint32_t)vol * 2);
		if(ci->init_vol > 128) {
			ci->init_vol = 128;
		}
		ci->cur_vol = ci->init_vol;
		ci->fine_tune = 0;
		ci->hold = 0;
		ci->decay = 0;
		ci->flags = 0;
		ci->valid = 0;
		pos += 8;
	}

	max_chans = 4;

	if(is_legacy_song) {
		// MMD0/MMD1/MMDC: Mmd0SongData starts here.
		if(pos + 2 + 2 + 256 + 2 + 1 + 1 + 1 + 1 + 16 + 1 + 1 > len) {
			return 0;
		}
		num_blocks_w = octamed_read_u16_be(data + pos); pos += 2;
		uint16_t song_len = octamed_read_u16_be(data + pos); pos += 2;
		if(song_len > 256) {
			return 0;
		}
		uint8_t legacy_pseq[256];
		memcpy(legacy_pseq, data + pos, 256);
		pos += 256;
		def_tempo = octamed_read_u16_be(data + pos); pos += 2;
		play_transp = (int8_t)data[pos++];
		flags = data[pos++];
		flags2 = data[pos++];
		tempo2 = data[pos++];
		memcpy(s->track_vol, data + pos, 16);
		pos += 16;
		master_vol = data[pos++];
		num_samples = data[pos++];

		// Build a single section + single play sequence.
		uint32_t pidx = octamed_alloc_pseq(s, song_len);
		if(pidx == 0xffffffffu) {
			return 0;
		}
		for(uint32_t i = 0; i < song_len; ++i) {
			s->pseqs[pidx].entries[i].value = legacy_pseq[i];
			s->pseqs[pidx].entries[i].cmd = OCTAMED_PSCMD_NONE;
		}
		s->sections[0] = (uint16_t)pidx;
		s->num_sections = 1;

		s->num_blocks = num_blocks_w;
		s->num_samples = num_samples;
		s->tempo_bpm = def_tempo;
		s->ticks_per_line = tempo2 ? tempo2 : 6;
		s->lines_per_beat = (uint16_t)((flags2 & OCTAMED_FLAG2_BMASK) + 1);
		s->bpm_mode = (flags2 & OCTAMED_FLAG2_BPM) ? 1 : 0;
		s->play_transp = play_transp;
		s->master_vol = master_vol ? master_vol : 64;
		s->slide_first = (flags & OCTAMED_FLAG_STSLIDE) ? 1 : 0;
		s->vol_hex = (flags & OCTAMED_FLAG_VOLHEX) ? 1 : 0;
		s->mix_conv = (flags2 & OCTAMED_FLAG2_MIX) ? 0 : 1;
		s->eight_ch_conv = (flags & OCTAMED_FLAG_8CHAN) ? 1 : 0;
		s->num_channels = s->eight_ch_conv ? 8 : 4;
		paula_set_lp_filter(&s->paula, (flags & OCTAMED_FLAG_FILTERON) ? 1 : 0);
	} else {
		// MMD2/MMD3: Mmd2SongData has section/playseq pointer table.
		uint32_t song_pos = pos;
		if(song_pos + 2 + 2 + 4 + 4 + 4 + 2 + 2 + 4 + 4 + 2 + 2 + 1 + 1 + 2 + 1 + 223 + 2 + 1 + 1 + 1 + 1 + 16 + 1 + 1 > len) {
			return 0;
		}
		num_blocks_w = octamed_read_u16_be(data + song_pos); song_pos += 2;
		uint16_t num_sections = octamed_read_u16_be(data + song_pos); song_pos += 2;
		uint32_t pseq_table_offs = octamed_read_u32_be(data + song_pos); song_pos += 4;
		uint32_t section_table_offs = octamed_read_u32_be(data + song_pos); song_pos += 4;
		uint32_t track_vols_offs = octamed_read_u32_be(data + song_pos); song_pos += 4;
		uint16_t num_tracks = octamed_read_u16_be(data + song_pos); song_pos += 2;
		uint16_t num_play_seqs = octamed_read_u16_be(data + song_pos); song_pos += 2;
		// trackPansOffs(4) flags3(4) volAdj(2) channels(2) mixEchoType(1)
		// mixEchoDepth(1) mixEchoLen(2) mixStereoSep(1)
		song_pos += 4;                       // trackPansOffs (unused: no panning here)
		song_pos += 4;                       // flags3
		song_pos += 2;                       // volAdj
		uint16_t channels_field = octamed_read_u16_be(data + song_pos); song_pos += 2;
		song_pos += 1 + 1 + 2 + 1;           // echo + stereo sep
		song_pos += 223;                     // reserved
		def_tempo = octamed_read_u16_be(data + song_pos); song_pos += 2;
		play_transp = (int8_t)data[song_pos++];
		flags = data[song_pos++];
		flags2 = data[song_pos++];
		tempo2 = data[song_pos++];
		song_pos += 16;                      // unused trk fields here in MMD2/3
		master_vol = data[song_pos++];
		num_samples = data[song_pos++];
		(void)song_pos;

		s->num_blocks = num_blocks_w;
		s->num_samples = num_samples;
		s->tempo_bpm = def_tempo;
		s->ticks_per_line = tempo2 ? tempo2 : 6;
		s->lines_per_beat = (uint16_t)((flags2 & OCTAMED_FLAG2_BMASK) + 1);
		s->bpm_mode = (flags2 & OCTAMED_FLAG2_BPM) ? 1 : 0;
		s->play_transp = play_transp;
		s->master_vol = master_vol ? master_vol : 64;
		s->slide_first = (flags & OCTAMED_FLAG_STSLIDE) ? 1 : 0;
		s->vol_hex = (flags & OCTAMED_FLAG_VOLHEX) ? 1 : 0;
		s->mix_conv = (flags2 & OCTAMED_FLAG2_MIX) ? 0 : 1;
		s->eight_ch_conv = (flags & OCTAMED_FLAG_8CHAN) ? 1 : 0;
		s->num_channels = (uint8_t)((channels_field != 0) ? channels_field : 4);
		if(s->num_channels > OCTAMED_MAX_TRACKS) {
			s->num_channels = OCTAMED_MAX_TRACKS;
		}
		paula_set_lp_filter(&s->paula, (flags & OCTAMED_FLAG_FILTERON) ? 1 : 0);

		// Track volumes.
		uint16_t song_tracks = num_tracks;
		if(song_tracks > 16) {
			song_tracks = 16;
		}
		if(track_vols_offs != 0 && track_vols_offs + song_tracks <= len) {
			memcpy(s->track_vol, data + track_vols_offs, song_tracks);
		}

		// Section table: NumSections u16 entries.
		if(num_sections > OCTAMED_MAX_SECTIONS) {
			return 0;
		}
		if(num_sections == 0) {
			return 0;
		}
		if(section_table_offs == 0 || section_table_offs + (uint32_t)num_sections * 2u > len) {
			return 0;
		}
		for(uint32_t i = 0; i < num_sections; ++i) {
			s->sections[i] = octamed_read_u16_be(data + section_table_offs + i * 2);
		}
		s->num_sections = num_sections;

		// PlaySeq table.
		if(num_play_seqs > OCTAMED_MAX_PLAYSEQS) {
			return 0;
		}
		if(pseq_table_offs == 0 || pseq_table_offs + (uint32_t)num_play_seqs * 4u > len) {
			return 0;
		}
		uint32_t cmd_ptrs[OCTAMED_MAX_PLAYSEQS];
		for(uint32_t i = 0; i < num_play_seqs; ++i) {
			uint32_t pseq_off = octamed_read_u32_be(data + pseq_table_offs + i * 4);
			if(pseq_off == 0 || pseq_off + 32 + 4 + 4 + 2 > len) {
				return 0;
			}
			// Skip 32-byte name.
			uint32_t cur = pseq_off + 32;
			uint32_t cmd_ptr = octamed_read_u32_be(data + cur); cur += 4;
			cur += 4;                            // reserved
			uint16_t seq_len = octamed_read_u16_be(data + cur); cur += 2;
			if(seq_len > OCTAMED_MAX_PSEQ) {
				seq_len = OCTAMED_MAX_PSEQ;
			}
			if(cur + (uint32_t)seq_len * 2u > len) {
				return 0;
			}
			uint32_t pidx = octamed_alloc_pseq(s, seq_len);
			if(pidx == 0xffffffffu) {
				return 0;
			}
			for(uint32_t j = 0; j < seq_len; ++j) {
				uint16_t v = octamed_read_u16_be(data + cur + j * 2);
				if(v < 0x8000) {
					s->pseqs[pidx].entries[j].value = v;
					s->pseqs[pidx].entries[j].cmd = OCTAMED_PSCMD_NONE;
				} else {
					// Sentinel; mark as no-op (block 0 with no cmd).
					s->pseqs[pidx].entries[j].value = 0;
					s->pseqs[pidx].entries[j].cmd = OCTAMED_PSCMD_NONE;
				}
			}
			cmd_ptrs[i] = cmd_ptr;
		}

		// Pseq commands.
		for(uint32_t i = 0; i < num_play_seqs; ++i) {
			uint32_t cmd_ptr = cmd_ptrs[i];
			if(cmd_ptr == 0) {
				continue;
			}
			uint32_t cur = cmd_ptr;
			// NOTE(peter): C# original unbounded; cap at 4096 records to avoid
			// rogue files spinning here. Each record is at least 4 bytes.
			uint32_t guard = 0;
			while(cur + 4 <= len) {
				if(++guard > 4096) {
					break;
				}
				uint16_t offs = octamed_read_u16_be(data + cur); cur += 2;
				uint8_t cmd_num = data[cur++];
				uint8_t extra = data[cur++];
				if(offs == 0xffff && cmd_num == 0 && extra == 0) {
					break;
				}
				if(offs >= s->pseqs[i].count) {
					if(extra != 0) {
						cur += extra;
					}
					continue;
				}
				if(cmd_num == OCTAMED_PSCMD_STOP) {
					s->pseqs[i].entries[offs].cmd = OCTAMED_PSCMD_STOP;
					if(extra != 0) {
						cur += extra;
					}
				} else if(cmd_num == OCTAMED_PSCMD_POSJUMP) {
					if(cur + 2 > len) {
						break;
					}
					uint16_t target = octamed_read_u16_be(data + cur); cur += 2;
					s->pseqs[i].entries[offs].cmd = OCTAMED_PSCMD_POSJUMP;
					s->pseqs[i].entries[offs].value = target;
					if(extra > 2) {
						cur += extra - 2;
					}
				} else {
					if(extra != 0) {
						cur += extra;
					}
				}
			}
		}

		// Validate sections refer to existing pseqs.
		for(uint32_t i = 0; i < s->num_sections; ++i) {
			if(s->sections[i] >= s->num_pseqs) {
				s->sections[i] = 0;
			}
		}
	}

	if(num_samples > OCTAMED_MAX_INSTR) {
		num_samples = OCTAMED_MAX_INSTR;
	}
	s->num_samples = num_samples;

	if(s->num_blocks > OCTAMED_MAX_BLOCKS) {
		return 0;
	}

	// Read block pointer table.
	if(locals_blocks_offs == 0 || locals_blocks_offs + s->num_blocks * 4 > len) {
		return 0;
	}
	for(uint32_t b = 0; b < s->num_blocks; ++b) {
		uint32_t blk_off = octamed_read_u32_be(data + locals_blocks_offs + b * 4);
		if(blk_off == 0) {
			return 0;
		}
		if(!mmd1plus_block) {
			// MMD0/MMDC: tracks(1) lines(1) then block bytes.
			// MMD0 has tracks*lines*3 raw bytes; MMDC compresses them with a
			// simple RLE: each control byte b is either (b & 0x80 != 0) -> run
			// of (256 - b) zero bytes, or (b + 1) uncompressed bytes follow.
			if(blk_off + 2 > len) {
				return 0;
			}
			uint8_t bt = data[blk_off];
			uint16_t bl = (uint16_t)(data[blk_off + 1] + 1);
			if(bt == 0 || bl == 0) {
				return 0;
			}
			if(bt > OCTAMED_MAX_TRACKS) {
				bt = OCTAMED_MAX_TRACKS;
			}
			if(bt > max_chans) {
				max_chans = bt;
			}
			uint32_t need = (uint32_t)bt * (uint32_t)bl * 3;
			s->blocks[b].tracks = bt;
			s->blocks[b].lines = bl;
			s->blocks[b].grid = (struct octamed_note *)calloc((size_t)bt * (size_t)bl, sizeof(struct octamed_note));
			if(!s->blocks[b].grid) {
				return 0;
			}
			uint8_t *raw = 0;
			uint8_t raw_owns = 0;
			if(s->mark_version == 'C') {
				raw = (uint8_t *)calloc(need, 1);
				if(!raw) {
					return 0;
				}
				raw_owns = 1;
				uint32_t pos2 = blk_off + 2;
				uint32_t j = 0;
				while(j < need) {
					if(pos2 >= len) {
						free(raw);
						return 0;
					}
					uint32_t pack = data[pos2++];
					if((pack & 0x80) != 0) {
						uint32_t run = 256 - pack;
						if(run > need - j) {
							run = need - j;
						}
						j += run;          // already zeroed by calloc
					} else {
						pack++;
						if(pack > need - j) {
							pack = need - j;
						}
						if(pos2 + pack > len) {
							free(raw);
							return 0;
						}
						memcpy(raw + j, data + pos2, pack);
						pos2 += pack;
						j += pack;
					}
				}
			} else {
				if(blk_off + 2 + need > len) {
					return 0;
				}
			}
			uint8_t *src = raw_owns ? raw : (data + blk_off + 2);
			for(int32_t row = 0; row < bl; ++row) {
				for(int32_t t = 0; t < bt; ++t) {
					struct octamed_note *n = &s->blocks[b].grid[row * bt + t];
					uint8_t b0 = src[0];
					uint8_t b1 = src[1];
					uint8_t b2 = src[2];
					n->note = (uint8_t)(b0 & 0x3f);
					uint8_t inum = (uint8_t)((b1 >> 4) | ((b0 & 0x80) ? 0x10 : 0) | ((b0 & 0x40) ? 0x20 : 0));
					n->instr = inum;
					uint8_t cmd_nibble = (uint8_t)(b1 & 0x0f);
					if(cmd_nibble == 0) {
						n->cmd = 0;
						n->data0 = 0;
						n->data1 = b2;
					} else {
						n->cmd = cmd_nibble;
						n->data0 = b2;
						n->data1 = 0;
					}
					src += 3;
				}
			}
			if(raw_owns) {
				free(raw);
			}
		} else {
			// MMD1/2/3: tracks(2) lines(2) blockInfoOffs(4), then packed cells.
			if(blk_off + 8 > len) {
				return 0;
			}
			uint32_t bt32 = octamed_read_u16_be(data + blk_off);
			uint16_t bl = (uint16_t)(octamed_read_u16_be(data + blk_off + 2) + 1);
			uint32_t blk_info_offs = octamed_read_u32_be(data + blk_off + 4);
			uint32_t skip_tracks = 0;
			if(bt32 > OCTAMED_MAX_TRACKS) {
				skip_tracks = bt32 - OCTAMED_MAX_TRACKS;
				bt32 = OCTAMED_MAX_TRACKS;
			}
			uint8_t bt = (uint8_t)bt32;
			if(bt == 0 || bl == 0) {
				return 0;
			}
			if(bt > max_chans) {
				max_chans = bt;
			}
			s->blocks[b].tracks = bt;
			s->blocks[b].lines = bl;
			s->blocks[b].grid = (struct octamed_note *)calloc((size_t)bt * (size_t)bl, sizeof(struct octamed_note));
			if(!s->blocks[b].grid) {
				return 0;
			}
			// MMD1/2/3 cell data is uncompressed: (bt + skip_tracks) * bl * 4 bytes.
			uint32_t raw_size = (uint32_t)(bt32 + skip_tracks) * (uint32_t)bl * 4u;
			if(blk_off + 8 + raw_size > len) {
				return 0;
			}
			uint8_t *raw = (uint8_t *)malloc(raw_size);
			if(!raw) {
				return 0;
			}
			memcpy(raw, data + blk_off + 8, raw_size);
			// Translate cells into grid (skipping high tracks).
			uint32_t in_stride = (uint32_t)(bt32 + skip_tracks) * 4u;
			for(uint32_t row = 0; row < bl; ++row) {
				uint8_t *src = raw + row * in_stride;
				for(uint32_t t = 0; t < bt; ++t) {
					struct octamed_note *n = &s->blocks[b].grid[row * bt + t];
					uint8_t nb = src[0];
					uint8_t ib = src[1];
					uint8_t cb = src[2];
					uint8_t ab = src[3];
					if(nb <= 0x80) {
						n->note = nb;
					}
					n->instr = ib;
					if(cb == 0x19 || cb == 0x00) {
						n->cmd = cb;
						n->data0 = 0;
						n->data1 = ab;
					} else {
						n->cmd = cb;
						n->data0 = ab;
						n->data1 = 0;
					}
					src += 4;
				}
			}
			free(raw);

			// Read block info: page table (additional command pages: we only
			// keep page 0) and CmdExtTable (extension byte per cell).
			if(blk_info_offs != 0 && blk_info_offs + 20 <= len) {
				// Mmd1BlockInfo: hl(4) name(4) namelen(4) pageTable(4) cmdExtTable(4)
				uint32_t page_table = octamed_read_u32_be(data + blk_info_offs + 12);
				uint32_t cmd_ext_table = octamed_read_u32_be(data + blk_info_offs + 16);
				uint32_t num_pages = 1;        // page 0 only is supported in playback
				(void)page_table;
				if(cmd_ext_table != 0 && cmd_ext_table + num_pages * 4u <= len) {
					// Only page 0's extension page is honoured, matching the
					// 0c00xx kludge handling for the volume command.
					uint32_t ext_off = octamed_read_u32_be(data + cmd_ext_table);
					if(ext_off != 0) {
						uint32_t need = (uint32_t)(bt32 + skip_tracks) * (uint32_t)bl;
						if(ext_off + need <= len) {
							uint8_t *src = data + ext_off;
							for(uint32_t row = 0; row < bl; ++row) {
								for(uint32_t t = 0; t < bt32 + skip_tracks; ++t) {
									if(t < bt) {
										struct octamed_note *n = &s->blocks[b].grid[row * bt + t];
										uint8_t arg2 = src[0];
										if(n->cmd == 0x00 || n->cmd == 0x19) {
											n->data0 = arg2;
										} else {
											n->data1 = arg2;
										}
									}
									src++;
								}
							}
						}
					}
				}
			}
		}
	}

	if(max_chans > s->num_channels) {
		s->num_channels = max_chans;
	}

	octamed_apply_old_vol(s);

	// Read samples.
	if(locals_samples_offs != 0 && s->num_samples != 0) {
		if(locals_samples_offs + s->num_samples * 4 > len) {
			return 0;
		}
		for(uint32_t i = 0; i < s->num_samples; ++i) {
			uint32_t soff = octamed_read_u32_be(data + locals_samples_offs + i * 4);
			if(soff == 0) {
				continue;
			}
			if(!octamed_load_sample(s, data, len, soff, i)) {
				return 0;
			}
		}
	}

	// Read MMD ExpData. The InsText block may carry hold/decay/finetune/
	// LongRepeat/LongRepLen/InitVol overrides for MMD1+ modules.
	uint8_t loop_flag_set = 0;
	if(locals_exp_offs != 0 && locals_exp_offs + 64 <= len) {
		uint32_t e = locals_exp_offs;
		// expData layout: nextHdr(4) insTextOffs(4) insTextEntries(2)
		// insTextEntrySize(2) annoTextOffs(4) annoLen(4) instInfoOffs(4)
		// instInfoEntries(2) instInfoEntrySize(2) ...
		uint32_t ins_text_offs = octamed_read_u32_be(data + e + 4);
		uint16_t ins_text_entries = octamed_read_u16_be(data + e + 8);
		uint16_t ins_text_size = octamed_read_u16_be(data + e + 10);
		if(ins_text_offs != 0 && ins_text_size >= 2) {
			uint32_t lim = ins_text_entries;
			if(lim > OCTAMED_MAX_INSTR) {
				lim = OCTAMED_MAX_INSTR;
			}
			for(uint32_t i = 0; i < lim; ++i) {
				uint32_t off = ins_text_offs + i * (uint32_t)ins_text_size;
				if(off + ins_text_size > len) {
					break;
				}
				struct octamed_instr *ci = &s->instr[i];
				if(ins_text_size >= 2) {
					ci->hold = data[off + 0];
					ci->decay = data[off + 1];
				}
				if(ins_text_size >= 4) {
					// off+2 = midi suppress (skip), off+3 = fine tune.
					int8_t ft = (int8_t)data[off + 3];
					ci->fine_tune = ft;
				}
				if(ins_text_size >= 8) {
					// off+4 default pitch, off+5 instr flags, off+6..7 midi preset.
					uint8_t iflags = data[off + 5];
					if(iflags & 0x04) {        // InstrFlag.Disabled
						ci->flags |= OCTAMED_IFLAG_DISABLED;
					}
					if(iflags & 0x01) {        // InstrFlag.Loop
						ci->flags |= OCTAMED_IFLAG_LOOP;
						if(iflags & 0x08) {    // InstrFlag.PingPong
							ci->flags |= OCTAMED_IFLAG_PINGPONG;
						}
					}
					loop_flag_set = 1;
				}
				if(ins_text_size >= 18) {
					// off+8 outputDevice, off+9 reserved, off+10..13 longRepeat,
					// off+14..17 longRepLen.
					uint32_t lrep = octamed_read_u32_be(data + off + 10);
					uint32_t lrlen = octamed_read_u32_be(data + off + 14);
					ci->loop_start = lrep;
					ci->loop_length = lrlen;
				}
				if(ins_text_size >= 19) {
					uint8_t v = data[off + 18];
					if(v > 128) {
						v = 128;
					}
					ci->init_vol = v;
					ci->cur_vol = v;
				}
				// Skip remaining (port number, midi bank).
			}
		}
	}

	if(!loop_flag_set) {
		// Pre-MMD3 modules: derive loop flag from RepLen > 2.
		for(uint32_t i = 0; i < OCTAMED_MAX_INSTR; ++i) {
			struct octamed_instr *ci = &s->instr[i];
			if(ci->loop_length > 2) {
				ci->flags |= OCTAMED_IFLAG_LOOP;
			}
		}
	}

	// Re-clamp loop bounds against actual sample length.
	for(uint32_t i = 0; i < OCTAMED_MAX_INSTR; ++i) {
		struct octamed_instr *ci = &s->instr[i];
		if(ci->sample_length == 0) {
			continue;
		}
		if(ci->loop_start >= ci->sample_length) {
			ci->loop_start = 0;
			ci->loop_length = 0;
			ci->flags &= (uint8_t)~OCTAMED_IFLAG_LOOP;
		} else if(ci->loop_start + ci->loop_length > ci->sample_length) {
			ci->loop_length = ci->sample_length - ci->loop_start;
		}
	}

	// Conversions for old non-mix-mode modules.
	if(s->mix_conv) {
		octamed_apply_mix_conv(s);
	}
	if(s->eight_ch_conv) {
		octamed_apply_8ch_tempo(s);
	}

	return 1;
}

// [=]===^=[ octamed_init_track_data ]============================================================[=]
static void octamed_init_track_data(struct octamed_state *s) {
	for(int32_t i = 0; i < OCTAMED_MAX_TRACKS; ++i) {
		struct octamed_track *t = &s->td[i];
		t->prev_note = 0;
		t->prev_inum = 0;
		t->prev_vol = 0;
		t->misc_flags = 0;
		t->note_off_cnt = -1;
		t->init_hold = 0;
		t->init_decay = 0;
		t->decay = 0;
		t->fade_speed = 0;
		t->s_transp = 0;
		t->fine_tune = 0;
		t->arp_adjust = 0;
		t->vibr_adjust = 0;
		t->s_offset = 0;
		t->curr_note = 0;
		t->fx_type = 0;
		t->frequency = 0;
		t->port_target_freq = 0;
		t->port_speed = 0;
		t->vib_shift = 6;
		t->vib_speed = 0;
		t->vib_size = 0;
		t->vib_offs = 0;
		t->temp_vol = 0;
		t->sy_type = OCTAMED_SYTYPE_NONE;
		t->sy_period_change = 0;
		t->sy_vib_offs = 0;
		t->sy_vib_speed = 0;
		t->sy_vib_dep = 0;
		t->sy_vib_wf_num = 0;
		t->sy_arp_start = 0;
		t->sy_arp_offs = 0;
		t->sy_vol_cmd_pos = 0;
		t->sy_wf_cmd_pos = 0;
		t->sy_vol_wait = 0;
		t->sy_wf_wait = 0;
		t->sy_vol_chg_speed = 0;
		t->sy_wf_chg_speed = 0;
		t->sy_vol_x_speed = 0;
		t->sy_wf_x_speed = 0;
		t->sy_vol_x_cnt = 0;
		t->sy_wf_x_cnt = 0;
		t->sy_env_wf_num = 0;
		t->sy_env_loop = 0;
		t->sy_env_count = 0;
		t->sy_vol = 0;
		t->sy_note_number = 0;
		t->sy_start = 0;
		t->sy_decay = 0;
	}
}

// [=]===^=[ octamed_pick_first_block ]===========================================================[=]
static uint16_t octamed_pick_first_block(struct octamed_state *s) {
	if(s->num_sections == 0 || s->num_pseqs == 0) {
		return 0;
	}
	uint16_t pidx = s->sections[0];
	if(pidx >= s->num_pseqs) {
		return 0;
	}
	if(s->pseqs[pidx].count == 0) {
		return 0;
	}
	uint16_t blk = s->pseqs[pidx].entries[0].value;
	if(blk >= s->num_blocks) {
		blk = 0;
	}
	return blk;
}

// [=]===^=[ octamed_initialize_sound ]===========================================================[=]
static void octamed_initialize_sound(struct octamed_state *s) {
	s->cur_section = 0;
	s->cur_pseq = (s->num_sections > 0) ? s->sections[0] : 0;
	s->pseq_pos = 0;
	s->cur_block = octamed_pick_first_block(s);
	s->cur_line = 0;
	s->fx_line = 0;
	s->fx_block = s->cur_block;
	s->next_line = 0;
	s->breaktype = OCTAMED_BREAK_NORMAL;
	s->repeat_line = 0;
	s->repeat_counter = 0;
	s->delayed_stop = 0;
	s->end_reached = 0;
	s->block_delay = 0;
	s->pulse_ctr = s->ticks_per_line;        // trigger immediately

	octamed_init_track_data(s);

	for(int32_t i = 0; i < OCTAMED_MAX_INSTR; ++i) {
		s->instr[i].cur_vol = s->instr[i].init_vol;
	}

	octamed_set_mix_tempo(s);
}

// [=]===^=[ octamed_extract_instr_data ]=========================================================[=]
static void octamed_extract_instr_data(struct octamed_state *s, struct octamed_track *t, uint32_t inum) {
	struct octamed_instr *ci = &s->instr[inum];
	t->prev_vol = ci->cur_vol;
	t->init_hold = ci->hold;
	t->init_decay = ci->decay;
	t->s_transp = ci->s_trans;
	t->fine_tune = ci->fine_tune;
	t->s_offset = 0;
	t->misc_flags = 0;
}

// [=]===^=[ octamed_play_paula_sample ]==========================================================[=]
// Triggers a sample on a Paula channel. For multi-octave samples this picks
// the appropriate buffer based on `note` and shifts loop_start/length.
static void octamed_play_paula_sample(struct octamed_state *s, int32_t idx, struct octamed_instr *ci, uint32_t s_offset, uint8_t note) {
	if(!ci->valid || ci->sample_data == 0 || ci->sample_length == 0) {
		paula_mute(&s->paula, idx);
		return;
	}

	uint32_t buf_off = ci->octave_offset[0];
	uint32_t buf_len = ci->octave_length[0];
	uint32_t loop_s = ci->loop_start;
	uint32_t loop_l = ci->loop_length;

	if(ci->sample_type != 0 && ci->num_octaves > 1) {
		int32_t oct = note / 12;
		if(oct < 0) {
			oct = 0;
		}
		if(oct > 5) {
			oct = 5;
		}
		uint8_t type_idx = (uint8_t)(ci->sample_type - 1);
		int8_t buf_num = octamed_multi_oct_buf_idx[type_idx][oct];
		if(buf_num >= ci->num_octaves) {
			buf_num = (int8_t)(ci->num_octaves - 1);
		}
		buf_off = ci->octave_offset[buf_num];
		buf_len = ci->octave_length[buf_num];
		// Loop points scale by 2^buf_num (octaves double in size).
		loop_s = ci->loop_start << buf_num;
		loop_l = ci->loop_length << buf_num;
	}

	uint32_t off = s_offset;
	if(off >= buf_len) {
		off = 0;
	}
	paula_play_sample(&s->paula, idx, ci->sample_data + buf_off, buf_len);
	if((ci->flags & OCTAMED_IFLAG_LOOP) && loop_l > 2) {
		paula_set_loop(&s->paula, idx, loop_s, loop_l);
	} else {
		paula_set_loop(&s->paula, idx, 0, 0);
	}
	if(off > 0) {
		s->paula.ch[idx].pos_fp = off << PAULA_FP_SHIFT;
	}
}

// [=]===^=[ octamed_mute_track ]=================================================================[=]
// Mirrors C# Player.MuteChannel override: clear the track's synth state then
// silence the Paula channel.
static void octamed_mute_track(struct octamed_state *s, int32_t trk) {
	if(trk < 0 || trk >= OCTAMED_MAX_TRACKS) {
		return;
	}
	s->td[trk].sy_type = OCTAMED_SYTYPE_NONE;
	paula_mute(&s->paula, trk);
}

// [=]===^=[ octamed_clear_synth ]================================================================[=]
// Mirrors C# Player.ClearSynth: zero all per-track synth playback variables
// (note number, arpeggio offsets, command pointers, vibrato/envelope state)
// and seed them with the per-instrument speeds. Honours MiscFlag.NoSynthWfPtrReset.
static void octamed_clear_synth(struct octamed_state *s, int32_t trk, uint32_t inum) {
	struct octamed_track *t = &s->td[trk];
	struct octamed_synth_sound *snd = (inum < OCTAMED_MAX_INSTR) ? s->instr[inum].synth : 0;
	t->sy_note_number = 0;
	t->sy_arp_offs = 0;
	t->sy_arp_start = 0;
	t->sy_vol_x_cnt = 0;
	t->sy_wf_x_cnt = 0;
	t->sy_vol_cmd_pos = 0;
	if((t->misc_flags & OCTAMED_MISC_NOSYNTHRST) == 0) {
		t->sy_wf_cmd_pos = 0;
	}
	t->sy_vol_wait = 0;
	t->sy_wf_wait = 0;
	t->sy_vib_speed = 0;
	t->sy_vib_dep = 0;
	t->sy_vib_wf_num = 0;
	t->sy_vib_offs = 0;
	t->sy_period_change = 0;
	t->sy_vol_chg_speed = 0;
	t->sy_wf_chg_speed = 0;
	t->sy_vol_x_speed = snd ? snd->vol_speed : 0;
	t->sy_wf_x_speed = snd ? snd->wf_speed : 0;
	t->sy_env_wf_num = 0;
}

// [=]===^=[ octamed_prepare_synth_sound ]========================================================[=]
// Mirrors C# Mixer.PrepareSynthSound: arms the channel so the next
// SetSynthWaveform call triggers fresh playback rather than swapping mid-DMA.
static void octamed_prepare_synth_sound(struct octamed_state *s, int32_t trk) {
	if(trk < 0 || trk >= OCTAMED_MAX_TRACKS) {
		return;
	}
	s->td[trk].sy_start = 1;
}

// [=]===^=[ octamed_set_synth_waveform ]=========================================================[=]
// Mirrors C# Mixer.SetSynthWaveform: trigger or hot-swap the named synth
// waveform and set up its loop. `length` is in bytes.
static void octamed_set_synth_waveform(struct octamed_state *s, int32_t trk, int8_t *data, uint32_t length) {
	struct octamed_track *t;
	if(trk < 0 || trk >= OCTAMED_MAX_TRACKS || length == 0 || data == 0) {
		return;
	}
	t = &s->td[trk];
	if(t->sy_start) {
		paula_play_sample(&s->paula, trk, data, length);
		t->sy_start = 0;
	} else {
		// Hot-swap the underlying buffer without restarting position.
		paula_queue_sample(&s->paula, trk, data, 0, length);
	}
	paula_set_loop(&s->paula, trk, 0, length);
}

// [=]===^=[ octamed_synth_handler ]==============================================================[=]
// Per-tick synth/hybrid playback. Returns the new frequency to feed into
// SetChannelFreq. Mirrors C# Player.SynthHandler 1:1, including the
// instCount<128 deadlock guards on both inner command-decode loops.
static int32_t octamed_synth_handler(struct octamed_state *s, int32_t trk, uint32_t inum, struct octamed_track *t) {
	struct octamed_synth_sound *snd;
	int32_t curr_freq;
	int32_t curr_period_change;

	snd = (inum < OCTAMED_MAX_INSTR) ? s->instr[inum].synth : 0;
	if(snd == 0) {
		t->sy_type = OCTAMED_SYTYPE_NONE;
		octamed_mute_track(s, trk);
		return 0;
	}

	// ---- Volume table ------------------------------------------------------
	if(t->sy_vol_x_cnt-- <= 1) {
		t->sy_vol_x_cnt = (int32_t)t->sy_vol_x_speed;
		if(t->sy_vol_chg_speed != 0) {
			t->sy_vol += (int32_t)t->sy_vol_chg_speed;
			if(t->sy_vol < 0) {
				t->sy_vol = 0;
			} else if(t->sy_vol > 64) {
				t->sy_vol = 64;
			}
		}
		if(t->sy_env_wf_num != 0) {
			// Envelope wf #1 on a hybrid sample is silently skipped (the
			// sample itself is the modulation source). Otherwise read the
			// 128-byte envelope shape.
			if(t->sy_env_wf_num == 1 && s->instr[inum].sample_length != 0) {
				// nop
			} else if(t->sy_env_wf_num <= snd->num_waveforms) {
				struct octamed_synth_wf *ewf = &snd->waveforms[t->sy_env_wf_num - 1];
				if(ewf->data) {
					t->sy_vol = ((int32_t)ewf->data[t->sy_env_count & 0x7f] + 128) >> 2;
				}
				if(++t->sy_env_count >= 128) {
					t->sy_env_count = 0;
					if(!t->sy_env_loop) {
						t->sy_env_wf_num = 0;
					}
				}
			}
		}
		if(t->sy_vol_wait == 0 || --t->sy_vol_wait == 0) {
			uint8_t end_cycle = 0;
			int32_t inst_count = 0;
			while(!end_cycle && (++inst_count < 128)) {
				uint8_t cmd = snd->vol_table[t->sy_vol_cmd_pos++ & 0x7f];
				if(cmd < 0x80) {
					t->sy_vol = cmd;
					if(t->sy_vol > 64) {
						t->sy_vol = 64;
					}
					break;
				}
				switch(cmd) {
					case OCTAMED_SYN_CMD_JMP: {
						t->sy_vol_cmd_pos = snd->vol_table[t->sy_vol_cmd_pos & 0x7f];
						if(t->sy_vol_cmd_pos > 127) {
							t->sy_vol_cmd_pos = 0;
						}
						break;
					}
					case OCTAMED_SYN_CMD_SPD: {
						t->sy_vol_x_speed = snd->vol_table[t->sy_vol_cmd_pos++ & 0x7f];
						break;
					}
					case OCTAMED_SYN_CMD_WAI: {
						t->sy_vol_wait = snd->vol_table[t->sy_vol_cmd_pos++ & 0x7f];
						end_cycle = 1;
						break;
					}
					case OCTAMED_SYN_CMD_CHU: {
						t->sy_vol_chg_speed = snd->vol_table[t->sy_vol_cmd_pos++ & 0x7f];
						break;
					}
					case OCTAMED_SYN_CMD_CHD: {
						uint8_t v = snd->vol_table[t->sy_vol_cmd_pos++ & 0x7f];
						t->sy_vol_chg_speed = (uint32_t)(-(int32_t)v);
						break;
					}
					case OCTAMED_SYN_VOL_JWS: {
						t->sy_wf_wait = 0;
						t->sy_wf_cmd_pos = snd->vol_table[t->sy_vol_cmd_pos++ & 0x7f];
						break;
					}
					case OCTAMED_SYN_VOL_EN1: {
						t->sy_env_wf_num = (uint32_t)(snd->vol_table[t->sy_vol_cmd_pos++ & 0x7f]) + 1u;
						t->sy_env_loop = 0;
						t->sy_env_count = 0;
						break;
					}
					case OCTAMED_SYN_VOL_EN2: {
						t->sy_env_wf_num = (uint32_t)(snd->vol_table[t->sy_vol_cmd_pos++ & 0x7f]) + 1u;
						t->sy_env_loop = 1;
						t->sy_env_count = 0;
						break;
					}
					case OCTAMED_SYN_CMD_RES: {
						t->sy_env_wf_num = 0;
						break;
					}
					case OCTAMED_SYN_CMD_END:
					case OCTAMED_SYN_CMD_HLT: {
						t->sy_vol_cmd_pos--;
						end_cycle = 1;
						break;
					}
					default: {
						end_cycle = 1;
						break;
					}
				}
			}
		}
	}

	t->temp_vol = (uint8_t)((((uint32_t)t->sy_vol * (uint32_t)t->prev_vol) >> 6) + 1u);

	// ---- Waveform table ----------------------------------------------------
	if(t->sy_wf_x_cnt-- <= 1) {
		t->sy_wf_x_cnt = (int32_t)t->sy_wf_x_speed;
		if(t->sy_wf_chg_speed != 0) {
			t->sy_period_change += (int32_t)t->sy_wf_chg_speed;
		}
		if(t->sy_wf_wait == 0 || --t->sy_wf_wait == 0) {
			uint8_t end_cycle = 0;
			int32_t inst_count = 0;
			while(!end_cycle && (++inst_count < 128)) {
				uint8_t cmd = snd->wf_table[t->sy_wf_cmd_pos++ & 0x7f];
				if(cmd < 0x80) {
					if(cmd < snd->num_waveforms) {
						struct octamed_synth_wf *swf = &snd->waveforms[cmd];
						if(swf->data && swf->length > 0) {
							octamed_set_synth_waveform(s, trk, swf->data, (uint32_t)swf->length * 2u);
						}
					}
					break;
				}
				switch(cmd) {
					case OCTAMED_SYN_WF_VWF: {
						t->sy_vib_wf_num = (uint32_t)(snd->wf_table[t->sy_wf_cmd_pos++ & 0x7f]) + 1u;
						break;
					}
					case OCTAMED_SYN_CMD_JMP: {
						t->sy_wf_cmd_pos = snd->wf_table[t->sy_wf_cmd_pos & 0x7f];
						if(t->sy_wf_cmd_pos > 127) {
							t->sy_wf_cmd_pos = 0;
						}
						break;
					}
					case OCTAMED_SYN_WF_ARP: {
						t->sy_arp_offs = t->sy_wf_cmd_pos;
						t->sy_arp_start = t->sy_wf_cmd_pos;
						// Scan to next command byte (>=0x80, ideally ARE).
						while(snd->wf_table[t->sy_wf_cmd_pos & 0x7f] < 0x80) {
							t->sy_wf_cmd_pos++;
						}
						break;
					}
					case OCTAMED_SYN_CMD_SPD: {
						t->sy_wf_x_speed = snd->wf_table[t->sy_wf_cmd_pos++ & 0x7f];
						break;
					}
					case OCTAMED_SYN_CMD_WAI: {
						t->sy_wf_wait = snd->wf_table[t->sy_wf_cmd_pos++ & 0x7f];
						end_cycle = 1;
						break;
					}
					case OCTAMED_SYN_WF_VBD: {
						t->sy_vib_dep = snd->wf_table[t->sy_wf_cmd_pos++ & 0x7f];
						break;
					}
					case OCTAMED_SYN_WF_VBS: {
						t->sy_vib_speed = snd->wf_table[t->sy_wf_cmd_pos++ & 0x7f];
						break;
					}
					case OCTAMED_SYN_CMD_CHD: {
						t->sy_wf_chg_speed = snd->wf_table[t->sy_wf_cmd_pos++ & 0x7f];
						break;
					}
					case OCTAMED_SYN_CMD_CHU: {
						uint8_t v = snd->wf_table[t->sy_wf_cmd_pos++ & 0x7f];
						t->sy_wf_chg_speed = (uint32_t)(-(int32_t)v);
						break;
					}
					case OCTAMED_SYN_CMD_RES: {
						t->sy_period_change = 0;
						break;
					}
					case OCTAMED_SYN_WF_JVS: {
						t->sy_vol_cmd_pos = snd->wf_table[t->sy_wf_cmd_pos++ & 0x7f];
						if(t->sy_vol_cmd_pos > 127) {
							t->sy_vol_cmd_pos = 0;
						}
						t->sy_vol_wait = 0;
						break;
					}
					case OCTAMED_SYN_CMD_END:
					case OCTAMED_SYN_CMD_HLT: {
						t->sy_wf_cmd_pos--;
						end_cycle = 1;
						break;
					}
					default: {
						end_cycle = 1;
						break;
					}
				}
			}
		}
	}

	curr_freq = t->frequency;

	// Arpeggio (transposes the playing note by the byte at ArpOffs). The C#
	// code adds raw bytes (mod 256) and casts back to byte before frequency
	// lookup, so a 0xff entry wraps high rather than being a -1 transpose.
	if(t->sy_arp_offs != 0) {
		uint8_t step = snd->wf_table[t->sy_arp_offs & 0x7f];
		uint8_t nn = (uint8_t)(t->sy_note_number + step);
		curr_freq = (int32_t)octamed_get_note_freq(s, nn, t->fine_tune);
		t->sy_arp_offs++;
		if(snd->wf_table[t->sy_arp_offs & 0x7f] >= 0x80) {
			t->sy_arp_offs = t->sy_arp_start;
		}
	}

	// Vibrato (modulates Paula period).
	curr_period_change = t->sy_period_change;
	if(t->sy_vib_dep != 0) {
		// Hybrid + VibWfNum==1 means "use the sample's own waveform"; the
		// C# falls through silently here. Match that behaviour.
		if(t->sy_vib_wf_num == 1 && s->instr[inum].sample_length != 0) {
			// nop
		} else if(t->sy_vib_wf_num <= snd->num_waveforms) {
			int8_t *vib_wave;
			if(t->sy_vib_wf_num != 0 && t->sy_vib_wf_num <= snd->num_waveforms) {
				struct octamed_synth_wf *vw = &snd->waveforms[t->sy_vib_wf_num - 1];
				vib_wave = vw->data ? vw->data : octamed_sine_table;
			} else {
				vib_wave = octamed_sine_table;
			}
			int32_t idx = (int32_t)((t->sy_vib_offs >> 4) & 0x1f);
			curr_period_change += ((int32_t)vib_wave[idx] * t->sy_vib_dep) / 256;
			t->sy_vib_offs += t->sy_vib_speed;
		}
	}

	if(curr_period_change != 0 && curr_freq != 0) {
		int32_t new_per = 3579545 / curr_freq + curr_period_change;
		if(new_per < 113) {
			new_per = 113;
		}
		curr_freq = 3579545 / new_per;
	}

	return curr_freq;
}

// [=]===^=[ octamed_plr_play_note ]==============================================================[=]
static void octamed_plr_play_note(struct octamed_state *s, int32_t trk, uint8_t note, uint32_t inum) {
	struct octamed_track *t = &s->td[trk];
	if(inum >= OCTAMED_MAX_INSTR) {
		return;
	}
	struct octamed_instr *ci = &s->instr[inum];
#ifdef OCTAMED_DEBUG_NOTES
	{
		double t_sec = (double)s->frames_played / (double)s->sample_rate;
		const char *kind = "----";
		if(ci->synth && ci->sample_length != 0) {
			kind = "HYB ";
		} else if(ci->synth) {
			kind = "SYN ";
		} else if(ci->valid) {
			kind = "PCM ";
		} else {
			kind = "EMPT";
		}
		fprintf(stderr, "[%7.3fs] blk=%3u line=%3u trk=%d inst=%2u %s note=%3u flags=0x%02x len=%u rep=%u/%u\n",
			t_sec, s->cur_block, s->cur_line, trk, (unsigned)(inum + 1), kind, note,
			ci->flags, ci->sample_length, ci->loop_start, ci->loop_length);
	}
#endif
	if((ci->flags & OCTAMED_IFLAG_DISABLED) != 0) {
		octamed_mute_track(s, trk);
		return;
	}
	if(!ci->valid && ci->synth == 0) {
		return;
	}
	if(note < 0x80) {
		// Mirrors C# PlrPlayNote: note += play_transpose + sample_transpose.
		int32_t nt = note + s->play_transp + ci->s_trans;
		while(nt >= 0x80) {
			nt -= 12;
		}
		while(nt < 0) {
			nt += 12;
		}
		note = (uint8_t)nt;
		while(note > 71) {
			note -= 12;
		}
	}
	t->decay = t->init_decay;
	t->fade_speed = 0;
	t->vib_offs = 0;
	t->frequency = (int32_t)octamed_get_instr_note_freq(s, (uint8_t)(note + 1), ci);

	if(ci->synth != 0) {
		// Mirrors C# PlrPlayNote synth branch. Always re-arms the channel for
		// fresh trigger (the original code intentionally retriggers on every
		// note for Parasol Stars compatibility).
		octamed_prepare_synth_sound(s, trk);
		if(ci->sample_length != 0) {
			t->sy_type = OCTAMED_SYTYPE_HYBRID;
		} else {
			t->sy_type = OCTAMED_SYTYPE_SYNTH;
		}
	} else {
		t->sy_type = OCTAMED_SYTYPE_NONE;
	}

	if(t->sy_type != OCTAMED_SYTYPE_SYNTH) {
		// Pure sample or hybrid: trigger the PCM sample on Paula.
		octamed_play_paula_sample(s, trk, ci, t->s_offset, note);
	}

	if(t->sy_type != OCTAMED_SYTYPE_NONE) {
		octamed_clear_synth(s, trk, inum);
		t->sy_note_number = note;
		// Track decay is the synth-decay JMP target (used in StopNote handling).
		t->sy_decay = t->decay;
	}
}

// [=]===^=[ octamed_play_fx_note ]===============================================================[=]
static void octamed_play_fx_note(struct octamed_state *s, int32_t trk, struct octamed_track *t) {
	if(t->curr_note == 0) {
		return;
	}
	if(t->note_off_cnt >= 0) {
		t->note_off_cnt += s->pulse_ctr;
	} else if(t->init_hold != 0) {
		t->note_off_cnt = (int32_t)t->init_hold;
	} else {
		t->note_off_cnt = -1;
	}
	octamed_plr_play_note(s, trk, (uint8_t)(t->curr_note - 1), t->prev_inum);
}

// [=]===^=[ octamed_do_cmd1 ]====================================================================[=]
// Slide up: period -= data ; freq = clk*256 / period.
static void octamed_do_cmd1(struct octamed_track *t, uint16_t data) {
	if(t->frequency > 0) {
		int32_t div = 3579545 * 256 / t->frequency - (int32_t)data;
		if(div > 0) {
			t->frequency = 3579545 * 256 / div;
		}
	}
}

// [=]===^=[ octamed_do_cmd2 ]====================================================================[=]
// Slide down: period += data.
static void octamed_do_cmd2(struct octamed_track *t, uint16_t data) {
	if(t->frequency > 0) {
		int32_t div = 3579545 * 256 / t->frequency + (int32_t)data;
		if(div > 0) {
			t->frequency = 3579545 * 256 / div;
		}
	}
}

// [=]===^=[ octamed_do_portamento ]==============================================================[=]
static void octamed_do_portamento(struct octamed_track *t) {
	if(t->port_target_freq == 0 || t->frequency <= 0) {
		return;
	}
	int32_t new_freq = t->frequency;
	int32_t div = 3579545 * 256 / new_freq;
	if(t->frequency > t->port_target_freq) {
		div += t->port_speed;
		if(div != 0) {
			new_freq = 3579545 * 256 / div;
			if(new_freq <= t->port_target_freq) {
				new_freq = t->port_target_freq;
				t->port_target_freq = 0;
			}
		}
	} else {
		if(div > t->port_speed) {
			div -= t->port_speed;
			new_freq = 3579545 * 256 / div;
		} else {
			new_freq = t->port_target_freq;
		}
		if(new_freq >= t->port_target_freq) {
			new_freq = t->port_target_freq;
			t->port_target_freq = 0;
		}
	}
	t->frequency = new_freq;
}

// [=]===^=[ octamed_do_vibrato ]=================================================================[=]
static void octamed_do_vibrato(struct octamed_track *t) {
	if(t->frequency > 0) {
		int32_t per = 3579545 / t->frequency;
		per += (octamed_sine_table[(t->vib_offs >> 2) & 0x1f] * t->vib_size) >> t->vib_shift;
		if(per > 0) {
			t->vibr_adjust = 3579545 / per - t->frequency;
		}
	}
	t->vib_offs += t->vib_speed;
}

// [=]===^=[ octamed_vib_cont ]===================================================================[=]
static void octamed_vib_cont(struct octamed_state *s, struct octamed_track *t, uint8_t data) {
	if(s->pulse_ctr == 0 && data != 0) {
		if((data & 0x0f) != 0) {
			t->vib_size = (uint8_t)(data & 0x0f);
		}
		if((data >> 4) != 0) {
			t->vib_speed = (uint8_t)((data >> 4) * 2);
		}
	}
	octamed_do_vibrato(t);
}

// [=]===^=[ octamed_do_delay_retrig ]============================================================[=]
static void octamed_do_delay_retrig(struct octamed_track *t) {
	t->note_off_cnt = (int32_t)t->init_hold;
	if(t->note_off_cnt == 0) {
		t->note_off_cnt = -1;
	}
	t->fx_type = 1;                          // NoPlay
}

// [=]===^=[ octamed_advance_song_pos ]===========================================================[=]
// Advances to a new pseq position, walking past PSeqCmd Stop/PosJump entries
// and across section boundaries. Mirrors PlayPosition.AdvanceSongPosition.
static void octamed_advance_song_pos(struct octamed_state *s, uint32_t new_pos) {
	uint32_t jump_count = 0;
	uint32_t roll_over = 0;
	uint32_t guard = 0;
	s->pseq_pos = new_pos;
	// NOTE(peter): C# original unbounded; cap at 4096 hops to defend against
	// bad data that creates infinite jump cycles even after the jumpCount<10
	// check.
	while(1) {
		if(++guard > 4096) {
			s->cur_block = 0;
			return;
		}
		if(s->cur_pseq >= s->num_pseqs) {
			s->cur_pseq = 0;
		}
		struct octamed_pseq *p = &s->pseqs[s->cur_pseq];
		if(s->pseq_pos >= p->count) {
			if(roll_over++ > 3) {
				s->cur_block = 0;
				return;
			}
			s->pseq_pos = 0;
			s->cur_section++;
			if(s->cur_section >= s->num_sections) {
				s->cur_section = 0;
				s->end_reached = 1;
			}
			s->cur_pseq = (s->num_sections > 0) ? s->sections[s->cur_section] : 0;
			continue;
		}
		struct octamed_pseq_entry *pse = &p->entries[s->pseq_pos];
		if(pse->cmd != OCTAMED_PSCMD_NONE) {
			if(pse->cmd == OCTAMED_PSCMD_POSJUMP) {
				if(jump_count++ < 10) {
					s->pseq_pos = pse->value;
					continue;
				}
				// Fall through: treat as normal entry (block num).
			} else if(pse->cmd == OCTAMED_PSCMD_STOP) {
				s->delayed_stop = 1;
				// Match C# behaviour: cmdHandler returns true so we break out
				// and use this entry's block.
			}
		}
		uint16_t new_blk = pse->value;
		if(new_blk < s->num_blocks) {
			s->cur_block = new_blk;
			return;
		}
		s->pseq_pos++;
	}
}

// [=]===^=[ octamed_advance_pos ]================================================================[=]
static void octamed_advance_pos(struct octamed_state *s) {
	s->cur_line++;
	if(s->cur_line >= s->blocks[s->cur_block].lines) {
		s->cur_line = 0;
		octamed_advance_song_pos(s, s->pseq_pos + 1);
	}
}

// [=]===^=[ octamed_pattern_break ]==============================================================[=]
static void octamed_pattern_break(struct octamed_state *s, uint16_t new_line) {
	octamed_advance_song_pos(s, s->pseq_pos + 1);
	s->cur_line = new_line;
	if(s->cur_line >= s->blocks[s->cur_block].lines) {
		s->cur_line = 0;
	}
}

// [=]===^=[ octamed_position_jump ]==============================================================[=]
static void octamed_position_jump(struct octamed_state *s, uint32_t new_pseq) {
	if(new_pseq <= s->pseq_pos) {
		s->end_reached = 1;
	}
	octamed_advance_song_pos(s, new_pseq);
	s->cur_line = 0;
}

// [=]===^=[ octamed_update_freq_vol ]============================================================[=]
static void octamed_update_freq_vol(struct octamed_state *s, int32_t trk) {
	struct octamed_track *t = &s->td[trk];
	if(t->fx_type != 0) {
		return;
	}
	int32_t base_freq;
	if(t->sy_type != OCTAMED_SYTYPE_NONE) {
		base_freq = octamed_synth_handler(s, trk, t->prev_inum, t);
	} else {
		base_freq = t->frequency;
	}
	int32_t freq = base_freq + t->arp_adjust + t->vibr_adjust;
	if(freq <= 0 || freq > 65535) {
		paula_mute(&s->paula, trk);
	} else {
		octamed_set_paula_freq(s, trk, freq);
	}
	uint8_t used_vol = t->prev_vol;
	if(t->temp_vol != 0) {
		used_vol = (uint8_t)(t->temp_vol - 1);
		t->temp_vol = 0;
	}
	uint16_t track_vol = (trk < 16) ? s->track_vol[trk] : 64;
	uint32_t scaled = ((uint32_t)used_vol * track_vol * s->master_vol) / (64u * 64u);
	if(scaled > 128) {
		scaled = 128;
	}
	paula_set_volume_256(&s->paula, trk, (uint16_t)(scaled * 2));
	t->arp_adjust = 0;
	t->vibr_adjust = 0;
}

// [=]===^=[ octamed_handle_pre_fx ]==============================================================[=]
// Pre-fx commands run once when a new line is read (before notes are played).
static void octamed_handle_pre_fx(struct octamed_state *s, struct octamed_block *blk, uint16_t line) {
	for(int32_t trk = 0; trk < blk->tracks && trk < OCTAMED_MAX_TRACKS; ++trk) {
		struct octamed_track *t = &s->td[trk];
		if(t->fx_type == 1) {
			continue;
		}
		struct octamed_note *n = &blk->grid[line * blk->tracks + trk];
		uint8_t cmd = n->cmd;
		uint8_t data = n->data0;
		uint16_t data_w = (uint16_t)((uint16_t)n->data0 * 256 + n->data1);
		switch(cmd) {
			case 0x03: {
				if(t->curr_note != 0) {
					uint8_t dest = t->curr_note;
					if(dest < 0x80) {
						int32_t dn = dest + s->play_transp + t->s_transp;
						while(dn >= 0x80) {
							dn -= 12;
						}
						while(dn < 1) {
							dn += 12;
						}
						dest = (uint8_t)dn;
					}
					t->port_target_freq = (int32_t)octamed_get_instr_note_freq(s, dest, &s->instr[t->prev_inum]);
				}
				if(data_w != 0) {
					t->port_speed = data_w;
				}
				t->fx_type = 1;
				break;
			}
			case 0x08: {
				t->init_hold = (uint32_t)(data & 0x0f);
				t->init_decay = (uint32_t)(data >> 4);
				break;
			}
			case 0x09: {
				uint8_t v = (uint8_t)(data & 0x1f);
				if(v != 0) {
					s->ticks_per_line = v;
					octamed_set_mix_tempo(s);
				}
				break;
			}
			case 0x0b: {
				s->breaktype = OCTAMED_BREAK_POSJUMP;
				s->next_line = data;
				break;
			}
			case 0x0c: {
				if(data < 0x80) {
					if(data <= 64) {
						t->prev_vol = (uint8_t)(data * 2);
					}
				} else {
					uint8_t d = (uint8_t)(data & 0x7f);
					if(d <= 64) {
						d = (uint8_t)(d * 2);
						t->prev_vol = d;
						s->instr[t->prev_inum].cur_vol = d;
					}
				}
				break;
			}
			case 0x0e: {
				// Set synth waveform sequence position; arms NoSynthWfPtrReset
				// so the next ClearSynth call preserves the WfCmdPos.
				if(data < 128) {
					t->sy_wf_cmd_pos = data;
					t->misc_flags |= OCTAMED_MISC_NOSYNTHRST;
				}
				break;
			}
			case 0x0f: {
				switch(data) {
					case 0x00: {
						s->breaktype = OCTAMED_BREAK_PATTERN;
						s->next_line = 0;
						break;
					}
					case 0xf2: case 0xf4: case 0xf5: {
						octamed_do_delay_retrig(t);
						break;
					}
					case 0xf8: {
						paula_set_lp_filter(&s->paula, 0);
						break;
					}
					case 0xf9: {
						paula_set_lp_filter(&s->paula, 1);
						break;
					}
					case 0xfd: {
						if(t->curr_note != 0) {
							uint8_t dest = t->curr_note;
							if(dest < 0x80) {
								int32_t dn = dest + s->play_transp + t->s_transp;
								while(dn >= 0x80) {
									dn -= 12;
								}
								while(dn < 1) {
									dn += 12;
								}
								dest = (uint8_t)dn;
							}
							t->frequency = (int32_t)octamed_get_instr_note_freq(s, dest, &s->instr[t->prev_inum]);
						}
						break;
					}
					case 0xfe: {
						s->delayed_stop = 1;
						s->end_reached = 1;
						break;
					}
					case 0xff: {
						octamed_mute_track(s, trk);
						break;
					}
					default: {
						if(data <= 240) {
							s->tempo_bpm = data;
							octamed_set_mix_tempo(s);
						}
						break;
					}
				}
				break;
			}
			case 0x15: {
				int8_t sd = (int8_t)data;
				if(sd >= -8 && sd <= 7) {
					t->fine_tune = sd;
				}
				break;
			}
			case 0x16: {
				if(data != 0) {
					if(s->repeat_counter == 0) {
						s->repeat_counter = data;
					} else {
						if(--s->repeat_counter == 0) {
							break;
						}
					}
					s->next_line = s->repeat_line;
					s->breaktype = OCTAMED_BREAK_LOOP;
				} else {
					s->repeat_line = line;
				}
				break;
			}
			case 0x19: {
				t->s_offset = (uint32_t)data_w << 8;
				break;
			}
			case 0x1d: {
				s->breaktype = OCTAMED_BREAK_PATTERN;
				s->next_line = data;
				break;
			}
			case 0x1e: {
				if(s->block_delay == 0) {
					s->block_delay = (uint32_t)data + 1;
				}
				break;
			}
			case 0x1f: {
				octamed_do_delay_retrig(t);
				break;
			}
			case 0x20: {
				if(data_w == 0) {
					t->misc_flags |= OCTAMED_MISC_BACKWARDS;
				}
				break;
			}
			case 0x2e: {
				int8_t sd = (int8_t)data;
				if(sd >= -16 && sd <= 16) {
					// Track panning is not modelled by Paula; ignore.
				}
				break;
			}
			default:
				break;
		}
	}
}

// [=]===^=[ octamed_handle_per_tick_fx ]=========================================================[=]
static void octamed_handle_per_tick_fx(struct octamed_state *s, struct octamed_block *blk, uint16_t fx_line) {
	for(int32_t trk = 0; trk < blk->tracks && trk < OCTAMED_MAX_TRACKS; ++trk) {
		struct octamed_track *t = &s->td[trk];
		if(t->fx_type == 1) {
			continue;
		}
		struct octamed_note *n = &blk->grid[fx_line * blk->tracks + trk];
		uint8_t cmd = n->cmd;
		uint8_t data = n->data0;
		uint16_t data_w = (uint16_t)((uint16_t)n->data0 * 256 + n->data1);
		switch(cmd) {
			case 0x00: {
				uint8_t a = n->data1;
				if(a != 0) {
					uint8_t bas = t->prev_note;
					if(bas > 0x80) {
						break;
					}
					switch(s->pulse_ctr % 3) {
						case 0: bas += (uint8_t)(a >> 4); break;
						case 1: bas += (uint8_t)(a & 0x0f); break;
						default: break;
					}
					int32_t bn = (int32_t)bas + s->play_transp - 1 + t->s_transp;
					while(bn < 0) {
						bn += 12;
					}
					while(bn >= 0x80) {
						bn -= 12;
					}
					int32_t freq = (int32_t)octamed_get_note_freq(s, (uint8_t)bn, t->fine_tune);
					t->arp_adjust = freq - t->frequency;
				}
				break;
			}
			case 0x11: {
				if(s->pulse_ctr == 0) {
					octamed_do_cmd1(t, data_w);
				}
				break;
			}
			case 0x01: {
				if(s->pulse_ctr == 0 && s->slide_first) {
					break;
				}
				octamed_do_cmd1(t, data_w);
				break;
			}
			case 0x12: {
				if(s->pulse_ctr == 0) {
					octamed_do_cmd2(t, data_w);
				}
				break;
			}
			case 0x02: {
				if(s->pulse_ctr == 0 && s->slide_first) {
					break;
				}
				octamed_do_cmd2(t, data_w);
				break;
			}
			case 0x03: {
				if(s->pulse_ctr == 0 && s->slide_first) {
					break;
				}
				octamed_do_portamento(t);
				break;
			}
			case 0x0d:
			case 0x0a:
			case 0x06:
			case 0x05: {
				if(s->pulse_ctr == 0 && s->slide_first) {
					break;
				}
				if((data & 0xf0) != 0) {
					int32_t v = t->prev_vol + (int32_t)((data >> 4) * 2);
					if(v > 128) {
						v = 128;
					}
					t->prev_vol = (uint8_t)v;
				} else {
					int32_t dec = (data & 0x0f) * 2;
					if(dec > t->prev_vol) {
						t->prev_vol = 0;
					} else {
						t->prev_vol = (uint8_t)(t->prev_vol - dec);
					}
				}
				if(cmd == 0x06) {
					octamed_do_vibrato(t);
				} else if(cmd == 0x05) {
					octamed_do_portamento(t);
				}
				break;
			}
			case 0x04: {
				t->vib_shift = 5;
				octamed_vib_cont(s, t, data);
				break;
			}
			case 0x14: {
				t->vib_shift = 6;
				octamed_vib_cont(s, t, data);
				break;
			}
			case 0x13: {
				if(s->pulse_ctr < 3) {
					t->vibr_adjust = -(int32_t)data;
				}
				break;
			}
			case 0x18: {
				if(s->pulse_ctr == data) {
					t->prev_vol = 0;
				}
				break;
			}
			case 0x1a: {
				if(s->pulse_ctr == 0) {
					uint8_t incr = (uint8_t)(data + ((n->data1 >= 0x80) ? 1 : 0));
					if((int32_t)t->prev_vol + incr < 128) {
						t->prev_vol = (uint8_t)(t->prev_vol + incr);
					} else {
						t->prev_vol = 128;
					}
				}
				break;
			}
			case 0x1b: {
				if(s->pulse_ctr == 0) {
					uint8_t decr = (uint8_t)(data - ((n->data1 >= 0x80) ? 1 : 0));
					if(t->prev_vol > decr) {
						t->prev_vol = (uint8_t)(t->prev_vol - decr);
					} else {
						t->prev_vol = 0;
					}
				}
				break;
			}
			case 0x0f: {
				switch(data) {
					case 0xf1:
					case 0xf2: {
						if(s->pulse_ctr == 3) {
							octamed_play_fx_note(s, trk, t);
						}
						break;
					}
					case 0xf3: {
						if(s->pulse_ctr == 2 || s->pulse_ctr == 4) {
							octamed_play_fx_note(s, trk, t);
						}
						break;
					}
					case 0xf4: {
						if((s->ticks_per_line / 3) == s->pulse_ctr) {
							octamed_play_fx_note(s, trk, t);
						}
						break;
					}
					case 0xf5: {
						if(((s->ticks_per_line * 2) / 3) == s->pulse_ctr) {
							octamed_play_fx_note(s, trk, t);
						}
						break;
					}
					default:
						break;
				}
				break;
			}
			case 0x1f: {
				if((data >> 4) != 0) {
					if(s->pulse_ctr < (data >> 4)) {
						break;
					}
					if(s->pulse_ctr == (data >> 4)) {
						octamed_play_fx_note(s, trk, t);
						break;
					}
				}
				if((data & 0x0f) != 0 && (s->pulse_ctr % (data & 0x0f)) == 0) {
					octamed_play_fx_note(s, trk, t);
				}
				break;
			}
			case 0x20: {
				if(s->pulse_ctr == 0 && data != 0) {
					uint32_t adv = (uint32_t)data_w << PAULA_FP_SHIFT;
					s->paula.ch[trk].pos_fp += adv;
				}
				break;
			}
			case 0x21: {
				int32_t f = t->frequency + ((t->frequency * (int32_t)data) >> 11);
				if(f > 65535) {
					f = 65535;
				}
				t->frequency = f;
				break;
			}
			case 0x22: {
				int32_t shift = (t->frequency * (int32_t)data) >> 11;
				if(shift + 1 < t->frequency) {
					t->frequency -= shift;
				} else {
					t->frequency = 1;
				}
				break;
			}
			case 0x29: {
				if(s->pulse_ctr == 0) {
					struct octamed_instr *ci = &s->instr[t->prev_inum];
					uint8_t div = n->data1;
					if(div == 0) {
						div = 0x10;
					}
					// Mirrors C# !smp.IsSynthSound() check: skip on synth-only.
					if(ci->valid && ci->sample_data && ci->synth == 0 && data < div) {
						uint32_t pos = ((uint32_t)data * ci->sample_length) / div;
						s->paula.ch[trk].pos_fp = pos << PAULA_FP_SHIFT;
					}
				}
				break;
			}
			default:
				break;
		}
	}
}

// [=]===^=[ octamed_tick ]=======================================================================[=]
// One playback tick. Mirrors PlrCallBack().
static void octamed_tick(struct octamed_state *s) {
	struct octamed_block *blk;

	s->pulse_ctr++;
	if(s->pulse_ctr >= s->ticks_per_line) {
		blk = &s->blocks[s->cur_block];
		s->pulse_ctr = 0;

		if(s->delayed_stop) {
			s->end_reached = 1;
		}

		if(s->block_delay != 0 && --s->block_delay != 0) {
			// keep current line
		} else {
			uint16_t line = s->cur_line;

			for(int32_t trk = 0; trk < blk->tracks && trk < OCTAMED_MAX_TRACKS; ++trk) {
				struct octamed_track *t = &s->td[trk];
				t->fx_type = 0;
				struct octamed_note *n = &blk->grid[line * blk->tracks + trk];
				uint8_t nn = n->note;
				if(nn != 0 && nn < 0x80) {
					t->curr_note = nn;
				} else if(nn == OCTAMED_NOTE_STP) {
					t->curr_note = 0;
				} else {
					t->curr_note = (nn > 0x7f) ? 0 : nn;
				}
				if(t->curr_note != 0) {
					t->prev_note = t->curr_note;
				}
				if(n->instr != 0 && n->instr <= OCTAMED_MAX_INSTR) {
					t->prev_inum = (uint8_t)(n->instr - 1);
					octamed_extract_instr_data(s, t, t->prev_inum);
					// C# Vitale.med fix: synth-instr restate without note must
					// clear synth state so portamento doesn't drift internals.
					if(nn != 0 && s->instr[t->prev_inum].synth != 0) {
						octamed_clear_synth(s, trk, t->prev_inum);
					}
				}
				if(nn == OCTAMED_NOTE_STP) {
					t->misc_flags |= OCTAMED_MISC_STOPNOTE;
				}
			}

			octamed_handle_pre_fx(s, blk, line);

			for(int32_t trk = 0; trk < blk->tracks && trk < OCTAMED_MAX_TRACKS; ++trk) {
				struct octamed_track *t = &s->td[trk];
				if(t->fx_type == 1) {
					continue;
				}
				if(t->curr_note != 0) {
					t->note_off_cnt = (t->init_hold != 0) ? (int32_t)t->init_hold : -1;
					octamed_plr_play_note(s, trk, (uint8_t)(t->curr_note - 1), t->prev_inum);
				}
			}

			s->fx_line = line;
			s->fx_block = s->cur_block;

			if(!s->delayed_stop) {
				switch(s->breaktype) {
					case OCTAMED_BREAK_LOOP: {
						s->cur_line = s->next_line;
						break;
					}
					case OCTAMED_BREAK_PATTERN: {
						octamed_pattern_break(s, s->next_line);
						break;
					}
					case OCTAMED_BREAK_POSJUMP: {
						octamed_position_jump(s, s->next_line);
						break;
					}
					default: {
						octamed_advance_pos(s);
						break;
					}
				}
				s->breaktype = OCTAMED_BREAK_NORMAL;
			}

			blk = &s->blocks[s->cur_block];
		}

		uint16_t curr_line = s->cur_line;
		if(curr_line >= blk->lines) {
			curr_line = blk->lines - 1;
		}
		for(int32_t trk = 0; trk < blk->tracks && trk < OCTAMED_MAX_TRACKS; ++trk) {
			struct octamed_track *t = &s->td[trk];
			if(t->note_off_cnt >= 0) {
				struct octamed_note *n = &blk->grid[curr_line * blk->tracks + trk];
				if((n->note == 0 && n->instr != 0) || (n->note != 0 && n->cmd == 0x03)) {
					t->note_off_cnt += s->ticks_per_line;
				}
			}
		}
	}

	if(s->fx_block >= s->num_blocks) {
		s->fx_block = (uint16_t)(s->num_blocks - 1);
	}
	blk = &s->blocks[s->fx_block];

	for(int32_t trk = 0; trk < blk->tracks && trk < OCTAMED_MAX_TRACKS; ++trk) {
		struct octamed_track *t = &s->td[trk];
		if((t->misc_flags & OCTAMED_MISC_STOPNOTE) || (t->note_off_cnt >= 0 && --t->note_off_cnt < 0)) {
			t->misc_flags &= (uint8_t)~OCTAMED_MISC_STOPNOTE;
			if(t->sy_type != OCTAMED_SYTYPE_NONE) {
				// Synth/hybrid note-off: jump the volume command pointer to
				// TrkDecay so the synth's release section runs. See C# Player
				// line ~799.
				t->sy_vol_cmd_pos = t->sy_decay;
				t->sy_vol_wait = 0;
			} else {
				uint32_t fs = t->decay * 2;
				t->fade_speed = fs;
				if(fs == 0) {
					octamed_mute_track(s, trk);
				}
			}
		}
		if(t->fade_speed != 0) {
			if(t->prev_vol > t->fade_speed) {
				t->prev_vol = (uint8_t)(t->prev_vol - t->fade_speed);
			} else {
				t->prev_vol = 0;
				t->fade_speed = 0;
			}
		}
		t->fx_type = 0;
	}

	if(s->fx_line >= blk->lines) {
		s->fx_line = (uint16_t)(blk->lines - 1);
	}
	octamed_handle_per_tick_fx(s, blk, s->fx_line);

	for(int32_t trk = 0; trk < blk->tracks && trk < OCTAMED_MAX_TRACKS; ++trk) {
		octamed_update_freq_vol(s, trk);
	}
}

// [=]===^=[ octamed_init ]=======================================================================[=]
static struct octamed_state *octamed_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || sample_rate < 8000) {
		return 0;
	}
	if(!octamed_identify((uint8_t *)data, len)) {
		return 0;
	}
	struct octamed_state *s = (struct octamed_state *)calloc(1, sizeof(struct octamed_state));
	if(!s) {
		return 0;
	}
	s->sample_rate = sample_rate;
	// paula_init must run before octamed_load: the loader calls
	// paula_set_lp_filter for songs whose flags request the LED filter.
	paula_init(&s->paula, sample_rate, 50);
	octamed_build_freq_table(s);
	if(!octamed_load(s, (uint8_t *)data, len)) {
		octamed_cleanup(s);
		free(s);
		return 0;
	}
	octamed_initialize_sound(s);
	return s;
}

// [=]===^=[ octamed_free ]=======================================================================[=]
static void octamed_free(struct octamed_state *s) {
	if(!s) {
		return;
	}
	octamed_cleanup(s);
	free(s);
}

// [=]===^=[ octamed_get_audio ]==================================================================[=]
static void octamed_get_audio(struct octamed_state *s, int16_t *output, int32_t frames) {
	while(frames > 0) {
		int32_t remain = s->paula.samples_per_tick - s->paula.tick_offset;
		if(remain > frames) {
			remain = frames;
		}
		paula_mix_frames(&s->paula, output, remain);
		output += remain * 2;
		s->paula.tick_offset += remain;
		frames -= remain;
#ifdef OCTAMED_DEBUG_NOTES
		s->frames_played += (uint64_t)remain;
#endif
		if(s->paula.tick_offset >= s->paula.samples_per_tick) {
			s->paula.tick_offset = 0;
			octamed_tick(s);
		}
	}
}

// [=]===^=[ octamed_api_init ]===================================================================[=]
static void *octamed_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return octamed_init(data, len, sample_rate);
}

// [=]===^=[ octamed_api_free ]===================================================================[=]
static void octamed_api_free(void *state) {
	octamed_free((struct octamed_state *)state);
}

// [=]===^=[ octamed_api_get_audio ]==============================================================[=]
static void octamed_api_get_audio(void *state, int16_t *output, int32_t frames) {
	octamed_get_audio((struct octamed_state *)state, output, frames);
}

static const char *octamed_extensions[] = { "med", "mmd0", "mmd1", "mmd2", "mmd3", "mmdc", "omed", "ocss", "md0", "md1", "md2", "md3", 0 };

static struct player_api octamed_api = {
	"OctaMED MMD0/1/2/3",
	octamed_extensions,
	octamed_api_init,
	octamed_api_free,
	octamed_api_get_audio,
	0,
};
