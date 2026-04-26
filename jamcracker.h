// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// JamCracker replayer, ported from NostalgicPlayer's C# implementation.
// Drives a 4-channel Amiga Paula (see paula.h). 50Hz tick rate.
//
// Public API:
//   struct jamcracker_state *jamcracker_init(void *data, uint32_t len, int32_t sample_rate);
//   void jamcracker_free(struct jamcracker_state *s);
//   void jamcracker_get_audio(struct jamcracker_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "paula.h"
#include "player_api.h"

#define JAMCRACKER_TICK_HZ      50
#define JAMCRACKER_NUM_VOICES   4

struct jamcracker_note {
	uint8_t period;
	int8_t instr;
	uint8_t speed;
	uint8_t arpeggio;
	uint8_t vibrato;
	uint8_t phase;
	uint8_t volume;
	uint8_t porta;
};

struct jamcracker_inst {
	uint8_t name[32];
	uint8_t flags;
	uint32_t size;
	int8_t *address;       // points into module_data buffer (not owned)
};

struct jamcracker_patt {
	uint16_t size;
	struct jamcracker_note *address;   // owned, allocated array of size*4 entries
};

struct jamcracker_voice {
	uint16_t wave_offset;
	uint16_t dmacon;
	int16_t ins_num;
	uint16_t ins_len;
	int8_t *ins_address;
	int8_t *real_ins_address;
	int8_t wave_buffer[0x40];
	int32_t per_index;
	uint16_t pers[3];
	int16_t por;
	int16_t delta_por;
	int16_t por_level;
	int16_t vib;
	int16_t delta_vib;
	int16_t vol;
	int16_t delta_vol;
	uint16_t vol_level;
	uint16_t phase;
	int16_t delta_phase;
	uint8_t vib_cnt;
	uint8_t vib_max;
	uint8_t flags;
};

struct jamcracker_state {
	struct paula paula;

	uint8_t *module_data;
	uint32_t module_len;

	uint16_t samples_num;
	uint16_t pattern_num;
	uint16_t song_len;

	struct jamcracker_inst *inst_table;
	struct jamcracker_patt *patt_table;
	uint16_t *song_table;

	// Global playing info
	struct jamcracker_note *cur_address;   // current pattern note array
	int32_t address_index;
	uint16_t tmp_dmacon;
	uint16_t song_pos;
	uint16_t note_cnt;
	uint8_t wait;
	uint8_t wait_cnt;

	struct jamcracker_voice voices[JAMCRACKER_NUM_VOICES];
};

// [=]===^=[ jamcracker_periods ]=================================================================[=]
static uint16_t jamcracker_periods[] = {
	1019, 962, 908,
	 857, 809, 763, 720, 680, 642, 606, 572, 540, 509, 481, 454,
	 428, 404, 381, 360, 340, 321, 303, 286, 270, 254, 240, 227,
	 214, 202, 190, 180, 170, 160, 151, 143, 135, 135, 135, 135,
	 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135
};

// [=]===^=[ jamcracker_read_u16_be ]=============================================================[=]
static uint16_t jamcracker_read_u16_be(uint8_t *p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// [=]===^=[ jamcracker_read_u32_be ]=============================================================[=]
static uint32_t jamcracker_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ jamcracker_test_module ]=============================================================[=]
static int32_t jamcracker_test_module(uint8_t *buf, uint32_t len) {
	if(len < 6) {
		return 0;
	}
	if((buf[0] != 'B') || (buf[1] != 'e') || (buf[2] != 'E') || (buf[3] != 'p')) {
		return 0;
	}
	return 1;
}

// [=]===^=[ jamcracker_cleanup ]=================================================================[=]
static void jamcracker_cleanup(struct jamcracker_state *s) {
	if(!s) {
		return;
	}
	if(s->patt_table) {
		for(uint32_t i = 0; i < s->pattern_num; ++i) {
			free(s->patt_table[i].address);
		}
		free(s->patt_table);
		s->patt_table = 0;
	}
	free(s->inst_table); s->inst_table = 0;
	free(s->song_table); s->song_table = 0;
}

// [=]===^=[ jamcracker_load ]====================================================================[=]
static int32_t jamcracker_load(struct jamcracker_state *s) {
	uint8_t *data = s->module_data;
	uint32_t len = s->module_len;
	uint32_t pos = 4;   // skip "BeEp" mark

	if(pos + 2 > len) {
		return 0;
	}
	s->samples_num = jamcracker_read_u16_be(data + pos);
	pos += 2;

	if(s->samples_num > 0) {
		s->inst_table = (struct jamcracker_inst *)calloc(s->samples_num, sizeof(struct jamcracker_inst));
		if(!s->inst_table) {
			return 0;
		}
	}

	for(uint32_t i = 0; i < s->samples_num; ++i) {
		if(pos + 31 + 1 + 4 + 4 > len) {
			return 0;
		}
		struct jamcracker_inst *ins = &s->inst_table[i];
		memcpy(ins->name, data + pos, 31);
		ins->name[31] = 0;
		pos += 31;
		ins->flags = data[pos];
		pos += 1;
		ins->size = jamcracker_read_u32_be(data + pos);
		pos += 4;
		pos += 4;   // skip address field
	}

	if(pos + 2 > len) {
		return 0;
	}
	s->pattern_num = jamcracker_read_u16_be(data + pos);
	pos += 2;

	if(s->pattern_num > 0) {
		s->patt_table = (struct jamcracker_patt *)calloc(s->pattern_num, sizeof(struct jamcracker_patt));
		if(!s->patt_table) {
			return 0;
		}
	}

	for(uint32_t i = 0; i < s->pattern_num; ++i) {
		if(pos + 2 + 4 > len) {
			return 0;
		}
		s->patt_table[i].size = jamcracker_read_u16_be(data + pos);
		pos += 2;
		pos += 4;   // skip address field
	}

	if(pos + 2 > len) {
		return 0;
	}
	s->song_len = jamcracker_read_u16_be(data + pos);
	pos += 2;

	if(s->song_len > 0) {
		s->song_table = (uint16_t *)calloc(s->song_len, sizeof(uint16_t));
		if(!s->song_table) {
			return 0;
		}
		if(pos + (uint32_t)s->song_len * 2 > len) {
			return 0;
		}
		for(uint32_t i = 0; i < s->song_len; ++i) {
			s->song_table[i] = jamcracker_read_u16_be(data + pos);
			pos += 2;
		}
	}

	for(uint32_t i = 0; i < s->pattern_num; ++i) {
		uint32_t entries = (uint32_t)s->patt_table[i].size * 4;
		if(entries == 0) {
			continue;
		}
		s->patt_table[i].address = (struct jamcracker_note *)calloc(entries, sizeof(struct jamcracker_note));
		if(!s->patt_table[i].address) {
			return 0;
		}
		if(pos + entries * 8 > len) {
			return 0;
		}
		for(uint32_t j = 0; j < entries; ++j) {
			struct jamcracker_note *n = &s->patt_table[i].address[j];
			n->period   = data[pos + 0];
			n->instr    = (int8_t)data[pos + 1];
			n->speed    = data[pos + 2];
			n->arpeggio = data[pos + 3];
			n->vibrato  = data[pos + 4];
			n->phase    = data[pos + 5];
			n->volume   = data[pos + 6];
			n->porta    = data[pos + 7];
			pos += 8;
		}
	}

	for(uint32_t i = 0; i < s->samples_num; ++i) {
		struct jamcracker_inst *ins = &s->inst_table[i];
		if(ins->size != 0) {
			if(pos + ins->size > len) {
				// truncated final sample is allowed in the C# loader for the last sample only,
				// but here we must clamp instead to keep playback safe.
				if(i != (uint32_t)s->samples_num - 1) {
					return 0;
				}
				ins->size = len - pos;
			}
			ins->address = (int8_t *)(data + pos);
			pos += ins->size;
		}
	}

	return 1;
}

// [=]===^=[ jamcracker_initialize_sound ]========================================================[=]
static void jamcracker_initialize_sound(struct jamcracker_state *s, int32_t start_position) {
	struct jamcracker_patt *pi = &s->patt_table[s->song_table[start_position]];

	s->song_pos = (uint16_t)start_position;
	s->note_cnt = pi->size;
	s->cur_address = pi->address;
	s->address_index = 0;
	s->wait = 6;
	s->wait_cnt = 1;
	s->tmp_dmacon = 0;

	uint16_t wave_off = 0x80;
	for(int32_t i = 0; i < JAMCRACKER_NUM_VOICES; ++i) {
		struct jamcracker_voice *v = &s->voices[i];
		memset(v, 0, sizeof(*v));
		v->wave_offset = wave_off;
		v->dmacon = (uint16_t)(1 << i);
		v->ins_num = -1;
		v->ins_len = 0;
		v->ins_address = 0;
		v->real_ins_address = 0;
		v->per_index = 0;
		v->pers[0] = 1019;
		v->pers[1] = 0;
		v->pers[2] = 0;
		v->por = 0;
		v->delta_por = 0;
		v->por_level = 0;
		v->vib = 0;
		v->delta_vib = 0;
		v->vol = 0;
		v->delta_vol = 0;
		v->vol_level = 0x40;
		v->phase = 0;
		v->delta_phase = 0;
		v->vib_cnt = 0;
		v->vib_max = 0;
		v->flags = 0;

		wave_off += 0x40;
	}
}

// [=]===^=[ jamcracker_nw_note ]=================================================================[=]
static void jamcracker_nw_note(struct jamcracker_state *s, struct jamcracker_note *adr, struct jamcracker_voice *voice) {
	int32_t per_index;

	if(adr->period != 0) {
		per_index = (int32_t)adr->period - 1;

		if((adr->speed & 64) != 0) {
			voice->por_level = (int16_t)jamcracker_periods[per_index];
		} else {
			s->tmp_dmacon |= voice->dmacon;

			voice->per_index = per_index;
			voice->pers[0] = jamcracker_periods[per_index];
			voice->pers[1] = jamcracker_periods[per_index];
			voice->pers[2] = jamcracker_periods[per_index];

			voice->por = 0;

			if((int32_t)adr->instr > (int32_t)s->samples_num) {
				voice->ins_address = 0;
				voice->real_ins_address = 0;
				voice->ins_len = 0;
				voice->ins_num = -1;
				voice->flags = 0;
			} else {
				int32_t ins_idx = (int32_t)adr->instr;
				if(ins_idx < 0 || ins_idx >= (int32_t)s->samples_num) {
					voice->ins_address = 0;
					voice->real_ins_address = 0;
					voice->ins_len = 0;
					voice->ins_num = -1;
					voice->flags = 0;
				} else {
					struct jamcracker_inst *ins_info = &s->inst_table[ins_idx];
					if(ins_info->address == 0) {
						voice->ins_address = 0;
						voice->real_ins_address = 0;
						voice->ins_len = 0;
						voice->ins_num = -1;
						voice->flags = 0;
					} else {
						if((ins_info->flags & 2) == 0) {
							voice->ins_address = ins_info->address;
							voice->real_ins_address = ins_info->address;
							voice->ins_len = (uint16_t)(ins_info->size / 2);
						} else {
							voice->real_ins_address = ins_info->address;
							voice->ins_address = voice->wave_buffer;
							memcpy(voice->wave_buffer, ins_info->address + voice->wave_offset, 0x40);
							voice->ins_len = 0x20;
						}

						voice->flags = ins_info->flags;
						voice->vol = (int16_t)voice->vol_level;
						voice->ins_num = adr->instr;
					}
				}
			}
		}
	}

	if((adr->speed & 15) != 0) {
		s->wait = (uint8_t)(adr->speed & 15);
	}

	// Do arpeggio
	per_index = voice->per_index;

	if(adr->arpeggio != 0) {
		if(adr->arpeggio == 255) {
			voice->pers[0] = jamcracker_periods[per_index];
			voice->pers[1] = jamcracker_periods[per_index];
			voice->pers[2] = jamcracker_periods[per_index];
		} else {
			voice->pers[2] = jamcracker_periods[per_index + (adr->arpeggio & 15)];
			voice->pers[1] = jamcracker_periods[per_index + (adr->arpeggio >> 4)];
			voice->pers[0] = jamcracker_periods[per_index];
		}
	}

	// Do vibrato
	if(adr->vibrato != 0) {
		if(adr->vibrato == 255) {
			voice->vib = 0;
			voice->delta_vib = 0;
			voice->vib_cnt = 0;
		} else {
			voice->vib = 0;
			voice->delta_vib = (int16_t)(adr->vibrato & 15);
			voice->vib_max = (uint8_t)(adr->vibrato >> 4);
			voice->vib_cnt = (uint8_t)(adr->vibrato >> 5);
		}
	}

	// Do phase
	if(adr->phase != 0) {
		if(adr->phase == 255) {
			voice->phase = 0;
			voice->delta_phase = -1;
		} else {
			voice->phase = 0;
			voice->delta_phase = (int16_t)(adr->phase & 15);
		}
	}

	// Do volume
	int16_t temp = (int16_t)adr->volume;
	if(temp == 0) {
		if((adr->speed & 128) != 0) {
			voice->vol = temp;
			voice->vol_level = (uint16_t)temp;
			voice->delta_vol = 0;
		}
	} else {
		if(temp == 255) {
			voice->delta_vol = 0;
		} else {
			if((adr->speed & 128) != 0) {
				voice->vol = temp;
				voice->vol_level = (uint16_t)temp;
				voice->delta_vol = 0;
			} else {
				temp &= 0x7f;
				if((adr->volume & 128) != 0) {
					temp = (int16_t)-temp;
				}
				voice->delta_vol = temp;
			}
		}
	}

	// Do portamento
	temp = (int16_t)adr->porta;
	if(temp != 0) {
		if(temp == 255) {
			voice->por = 0;
			voice->delta_por = 0;
		} else {
			voice->por = 0;
			if((adr->speed & 64) != 0) {
				if(voice->por_level <= (int16_t)voice->pers[0]) {
					temp = (int16_t)-temp;
				}
			} else {
				temp &= 0x7f;
				if((adr->porta & 128) == 0) {
					temp = (int16_t)-temp;
					voice->por_level = 135;
				} else {
					voice->por_level = 1019;
				}
			}

			voice->delta_por = temp;
		}
	}
}

// [=]===^=[ jamcracker_set_voice ]===============================================================[=]
static void jamcracker_set_voice(struct jamcracker_state *s, struct jamcracker_voice *voice, int32_t voice_idx) {
	if((s->tmp_dmacon & voice->dmacon) != 0) {
		// Setup the start sample
		if(voice->ins_address == 0) {
			paula_mute(&s->paula, voice_idx);
		} else {
			paula_play_sample(&s->paula, voice_idx, voice->ins_address, (uint32_t)voice->ins_len * 2);
			paula_set_period(&s->paula, voice_idx, voice->pers[0]);
		}

		// Check to see if sample loops
		if((voice->flags & 1) == 0) {
			voice->ins_address = 0;
			voice->real_ins_address = 0;
			voice->ins_len = 0;
			voice->ins_num = -1;
		}

		// Setup loop
		if(voice->ins_address != 0) {
			paula_set_loop(&s->paula, voice_idx, 0, (uint32_t)voice->ins_len * 2);
		}
	}
}

// [=]===^=[ jamcracker_rotate_periods ]==========================================================[=]
static void jamcracker_rotate_periods(struct jamcracker_voice *voice) {
	uint16_t temp1 = voice->pers[0];
	voice->pers[0] = voice->pers[1];
	voice->pers[1] = voice->pers[2];
	voice->pers[2] = temp1;
}

// [=]===^=[ jamcracker_set_channel ]=============================================================[=]
static void jamcracker_set_channel(struct jamcracker_state *s, struct jamcracker_voice *voice, int32_t voice_idx) {
	while(voice->pers[0] == 0) {
		jamcracker_rotate_periods(voice);
	}

	int16_t per = (int16_t)((int16_t)voice->pers[0] + voice->por);
	if(voice->por < 0) {
		if(per < voice->por_level) {
			per = voice->por_level;
		}
	} else {
		if((voice->por != 0) && (per > voice->por_level)) {
			per = voice->por_level;
		}
	}

	// Add vibrato
	per = (int16_t)(per + voice->vib);

	if(per < 135) {
		per = 135;
	} else if(per > 1019) {
		per = 1019;
	}

	paula_set_period(&s->paula, voice_idx, (uint16_t)per);
	jamcracker_rotate_periods(voice);

	voice->por = (int16_t)(voice->por + voice->delta_por);
	if(voice->por < -1019) {
		voice->por = -1019;
	} else if(voice->por > 1019) {
		voice->por = 1019;
	}

	if(voice->vib_cnt != 0) {
		voice->vib = (int16_t)(voice->vib + voice->delta_vib);
		voice->vib_cnt--;
		if(voice->vib_cnt == 0) {
			voice->delta_vib = (int16_t)-voice->delta_vib;
			voice->vib_cnt = voice->vib_max;
		}
	}

	// Volume is 0..64 Amiga scale here; use the 0..64 setter via *4 to 0..256.
	uint16_t vol = (uint16_t)voice->vol;
	if(vol > 64) {
		vol = 64;
	}
	paula_set_volume_256(&s->paula, voice_idx, (uint16_t)(vol * 4));

	voice->vol = (int16_t)(voice->vol + voice->delta_vol);
	if(voice->vol < 0) {
		voice->vol = 0;
	} else if(voice->vol > 64) {
		voice->vol = 64;
	}

	if(((voice->flags & 1) != 0) && (voice->delta_phase != 0)) {
		if(voice->delta_phase < 0) {
			voice->delta_phase = 0;
		}

		int8_t *ins_data = voice->ins_address;
		int8_t *wave = voice->real_ins_address;
		if(ins_data != 0 && wave != 0) {
			int32_t wave_phase = (int32_t)voice->phase / 4;

			for(int32_t i = 0; i < 64; ++i) {
				int16_t t = wave[i];
				t = (int16_t)(t + wave[wave_phase++]);
				t = (int16_t)(t >> 1);
				ins_data[i] = (int8_t)t;
			}
		}

		voice->phase = (uint16_t)(voice->phase + voice->delta_phase);
		if(voice->phase >= 256) {
			voice->phase = (uint16_t)(voice->phase - 256);
		}
	}
}

// [=]===^=[ jamcracker_new_note ]================================================================[=]
static void jamcracker_new_note(struct jamcracker_state *s) {
	struct jamcracker_note *adr = s->cur_address;
	int32_t adr_index = s->address_index;
	s->address_index += 4;

	s->note_cnt--;
	if(s->note_cnt == 0) {
		s->song_pos++;
		if(s->song_pos >= s->song_len) {
			s->song_pos = 0;
		}

		struct jamcracker_patt *pi = &s->patt_table[s->song_table[s->song_pos]];
		s->note_cnt = pi->size;
		s->cur_address = pi->address;
		adr = s->cur_address;

		s->address_index = 0;
		adr_index = 0;
	}

	s->tmp_dmacon = 0;

	jamcracker_nw_note(s, &adr[adr_index + 0], &s->voices[0]);
	jamcracker_nw_note(s, &adr[adr_index + 1], &s->voices[1]);
	jamcracker_nw_note(s, &adr[adr_index + 2], &s->voices[2]);
	jamcracker_nw_note(s, &adr[adr_index + 3], &s->voices[3]);

	jamcracker_set_voice(s, &s->voices[0], 0);
	jamcracker_set_voice(s, &s->voices[1], 1);
	jamcracker_set_voice(s, &s->voices[2], 2);
	jamcracker_set_voice(s, &s->voices[3], 3);
}

// [=]===^=[ jamcracker_tick ]====================================================================[=]
static void jamcracker_tick(struct jamcracker_state *s) {
	s->wait_cnt--;
	if(s->wait_cnt == 0) {
		jamcracker_new_note(s);
		s->wait_cnt = s->wait;
	}

	for(int32_t i = 0; i < JAMCRACKER_NUM_VOICES; ++i) {
		jamcracker_set_channel(s, &s->voices[i], i);
	}
}

// [=]===^=[ jamcracker_init ]====================================================================[=]
static struct jamcracker_state *jamcracker_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 6 || sample_rate < 8000) {
		return 0;
	}

	if(!jamcracker_test_module((uint8_t *)data, len)) {
		return 0;
	}

	struct jamcracker_state *s = (struct jamcracker_state *)calloc(1, sizeof(struct jamcracker_state));
	if(!s) {
		return 0;
	}

	s->module_data = (uint8_t *)data;
	s->module_len = len;

	if(!jamcracker_load(s)) {
		jamcracker_cleanup(s);
		free(s);
		return 0;
	}

	if(s->song_len == 0 || s->pattern_num == 0 || s->song_table == 0 || s->patt_table == 0) {
		jamcracker_cleanup(s);
		free(s);
		return 0;
	}

	if(s->song_table[0] >= s->pattern_num) {
		jamcracker_cleanup(s);
		free(s);
		return 0;
	}

	paula_init(&s->paula, sample_rate, JAMCRACKER_TICK_HZ);
	jamcracker_initialize_sound(s, 0);
	return s;
}

// [=]===^=[ jamcracker_free ]====================================================================[=]
static void jamcracker_free(struct jamcracker_state *s) {
	if(!s) {
		return;
	}
	jamcracker_cleanup(s);
	free(s);
}

// [=]===^=[ jamcracker_get_audio ]===============================================================[=]
static void jamcracker_get_audio(struct jamcracker_state *s, int16_t *output, int32_t frames) {
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
			jamcracker_tick(s);
		}
	}
}

// [=]===^=[ jamcracker_api_init ]================================================================[=]
static void *jamcracker_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return jamcracker_init(data, len, sample_rate);
}

// [=]===^=[ jamcracker_api_free ]================================================================[=]
static void jamcracker_api_free(void *state) {
	jamcracker_free((struct jamcracker_state *)state);
}

// [=]===^=[ jamcracker_api_get_audio ]===========================================================[=]
static void jamcracker_api_get_audio(void *state, int16_t *output, int32_t frames) {
	jamcracker_get_audio((struct jamcracker_state *)state, output, frames);
}

static const char *jamcracker_extensions[] = { "jam", 0 };

static struct player_api jamcracker_api = {
	"JamCracker",
	jamcracker_extensions,
	jamcracker_api_init,
	jamcracker_api_free,
	jamcracker_api_get_audio,
	0,
};
