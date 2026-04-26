// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// One-shot sample player, ported from NostalgicPlayer's Sample agent.
// Decodes IFF-8SVX (PCM and Fibonacci-Delta), IFF-16SV, AIFF, and RIFF-WAVE
// (PCM and IEEE float), then plays the decoded buffer back at the requested
// host sample rate using a linear-interpolating resampler.
//
// Public API:
//   struct sample_state *sample_init(void *data, uint32_t len, int32_t sample_rate);
//   void sample_free(struct sample_state *s);
//   void sample_get_audio(struct sample_state *s, int16_t *output, int32_t frames);

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "player_api.h"

#define SAMPLE_FP_SHIFT  16
#define SAMPLE_FP_ONE    (1u << SAMPLE_FP_SHIFT)

enum {
	SAMPLE_FMT_NONE = 0,
	SAMPLE_FMT_IFF_8SVX_PCM,
	SAMPLE_FMT_IFF_8SVX_FIB,
	SAMPLE_FMT_IFF_16SV,
	SAMPLE_FMT_AIFF,
	SAMPLE_FMT_WAVE_PCM,
	SAMPLE_FMT_WAVE_FLOAT,
};

struct sample_state {
	uint8_t *module_data;
	uint32_t module_len;

	int32_t host_sample_rate;
	int32_t source_frequency;
	int32_t channels;
	int32_t bits;

	int16_t *pcm;
	uint32_t pcm_frames;
	uint32_t loop_start;
	uint32_t loop_length;
	uint8_t has_loop;

	uint64_t pos_fp;
	uint64_t step_fp;
};

// [=]===^=[ sample_read_u16_be ]=================================================================[=]
static uint16_t sample_read_u16_be(uint8_t *p) {
	return (uint16_t)((p[0] << 8) | p[1]);
}

// [=]===^=[ sample_read_u32_be ]=================================================================[=]
static uint32_t sample_read_u32_be(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// [=]===^=[ sample_read_u16_le ]=================================================================[=]
static uint16_t sample_read_u16_le(uint8_t *p) {
	return (uint16_t)(p[0] | (p[1] << 8));
}

// [=]===^=[ sample_read_u32_le ]=================================================================[=]
static uint32_t sample_read_u32_le(uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// [=]===^=[ sample_check_mark ]==================================================================[=]
static int32_t sample_check_mark(uint8_t *p, const char *mark) {
	return (p[0] == (uint8_t)mark[0]) && (p[1] == (uint8_t)mark[1]) &&
	       (p[2] == (uint8_t)mark[2]) && (p[3] == (uint8_t)mark[3]);
}

// IEEE-754 80-bit big-endian extended precision used by AIFF COMM chunk.
// [=]===^=[ sample_ieee_extended_to_double ]=====================================================[=]
static double sample_ieee_extended_to_double(uint8_t *bytes) {
	int32_t expon = ((bytes[0] & 0x7f) << 8) | (bytes[1] & 0xff);
	uint64_t hi_mant = ((uint64_t)bytes[2] << 24) | ((uint64_t)bytes[3] << 16) |
	                   ((uint64_t)bytes[4] << 8)  | ((uint64_t)bytes[5]);
	uint64_t lo_mant = ((uint64_t)bytes[6] << 24) | ((uint64_t)bytes[7] << 16) |
	                   ((uint64_t)bytes[8] << 8)  | ((uint64_t)bytes[9]);
	double f;
	if((expon == 0) && (hi_mant == 0) && (lo_mant == 0)) {
		f = 0.0;
	} else if(expon == 0x7fff) {
		f = 0.0;
	} else {
		expon -= 16383;
		f  = ldexp((double)hi_mant, expon - 31);
		f += ldexp((double)lo_mant, expon - 31 - 32);
	}
	if(bytes[0] & 0x80) {
		return -f;
	}
	return f;
}

static int8_t sample_fib_table[16] = {
	-34, -21, -13, -8, -5, -3, -2, -1, 0, 1, 2, 3, 5, 8, 13, 21
};

// Walks the next IFF/RIFF chunk header at *cursor, advances cursor past it, and returns
// a pointer to the chunk body. Returns 0 when fewer than 8 bytes remain.
// [=]===^=[ sample_iff_walk_chunk ]==============================================================[=]
static uint8_t *sample_iff_walk_chunk(uint8_t *data, uint32_t len, uint32_t *cursor, char *out_mark, int32_t big_endian, uint32_t *chunk_size) {
	uint32_t offset = *cursor;
	if(offset + 8 > len) {
		return 0;
	}
	memcpy(out_mark, data + offset, 4);
	out_mark[4] = 0;
	uint32_t size = big_endian ? sample_read_u32_be(data + offset + 4) : sample_read_u32_le(data + offset + 4);
	*chunk_size = size;
	uint8_t *payload = data + offset + 8;
	uint32_t advance = (size + 1) & 0xfffffffe;
	if(offset + 8 + advance > len) {
		*cursor = len;
	} else {
		*cursor = offset + 8 + advance;
	}
	return payload;
}

// [=]===^=[ sample_decode_iff_8svx_pcm ]=========================================================[=]
static int32_t sample_decode_iff_8svx_pcm(struct sample_state *s, uint8_t *body, uint32_t body_size, uint32_t one_shot, uint32_t repeat) {
	uint32_t total = one_shot + repeat;
	if(total == 0) {
		return 0;
	}
	if(s->channels == 1) {
		if(total > body_size) {
			total = body_size;
		}
		s->pcm = (int16_t *)malloc(sizeof(int16_t) * total);
		if(!s->pcm) {
			return 0;
		}
		for(uint32_t i = 0; i < total; ++i) {
			s->pcm[i] = (int16_t)((int8_t)body[i] << 8);
		}
		s->pcm_frames = total;
		if(repeat > 0) {
			s->has_loop = 1;
			s->loop_start = one_shot;
			s->loop_length = repeat;
		}
	} else {
		uint32_t per_channel = total;
		if(per_channel * 2 > body_size) {
			per_channel = body_size / 2;
		}
		s->pcm = (int16_t *)malloc(sizeof(int16_t) * per_channel * 2);
		if(!s->pcm) {
			return 0;
		}
		for(uint32_t i = 0; i < per_channel; ++i) {
			s->pcm[i * 2 + 0] = (int16_t)((int8_t)body[i] << 8);
			s->pcm[i * 2 + 1] = (int16_t)((int8_t)body[per_channel + i] << 8);
		}
		s->pcm_frames = per_channel;
		if(repeat > 0) {
			s->has_loop = 1;
			s->loop_start = one_shot;
			s->loop_length = repeat;
		}
	}
	return 1;
}

// [=]===^=[ sample_decode_iff_8svx_fib ]=========================================================[=]
static int32_t sample_decode_iff_8svx_fib(struct sample_state *s, uint8_t *body, uint32_t body_size) {
	if(body_size < 2) {
		return 0;
	}
	if(s->channels == 1) {
		uint32_t pairs = body_size - 2;
		uint32_t total = pairs * 2;
		s->pcm = (int16_t *)malloc(sizeof(int16_t) * total);
		if(!s->pcm) {
			return 0;
		}
		int8_t val = (int8_t)body[1];
		uint32_t out_idx = 0;
		for(uint32_t i = 0; i < pairs; ++i) {
			val += sample_fib_table[(body[2 + i] >> 4) & 0x0f];
			s->pcm[out_idx++] = (int16_t)(val << 8);
			val += sample_fib_table[body[2 + i] & 0x0f];
			s->pcm[out_idx++] = (int16_t)(val << 8);
		}
		s->pcm_frames = total;
	} else {
		uint32_t half = body_size / 2;
		if(half < 2) {
			return 0;
		}
		uint32_t pairs = half - 2;
		uint32_t total = pairs * 2;
		s->pcm = (int16_t *)malloc(sizeof(int16_t) * total * 2);
		if(!s->pcm) {
			return 0;
		}
		int8_t l = (int8_t)body[1];
		int8_t r = (int8_t)body[half + 1];
		uint32_t out_idx = 0;
		for(uint32_t i = 0; i < pairs; ++i) {
			l += sample_fib_table[(body[2 + i] >> 4) & 0x0f];
			r += sample_fib_table[(body[half + 2 + i] >> 4) & 0x0f];
			s->pcm[out_idx++] = (int16_t)(l << 8);
			s->pcm[out_idx++] = (int16_t)(r << 8);
			l += sample_fib_table[body[2 + i] & 0x0f];
			r += sample_fib_table[body[half + 2 + i] & 0x0f];
			s->pcm[out_idx++] = (int16_t)(l << 8);
			s->pcm[out_idx++] = (int16_t)(r << 8);
		}
		s->pcm_frames = total;
	}
	return 1;
}

// [=]===^=[ sample_decode_iff_16sv ]=============================================================[=]
static int32_t sample_decode_iff_16sv(struct sample_state *s, uint8_t *body, uint32_t body_size, uint32_t one_shot, uint32_t repeat) {
	uint32_t total_samples = one_shot + repeat;
	if(s->channels == 2) {
		total_samples *= 2;
	}
	uint32_t avail = body_size / 2;
	if(total_samples > avail) {
		total_samples = avail;
	}
	if(total_samples == 0) {
		return 0;
	}
	if(s->channels == 1) {
		s->pcm = (int16_t *)malloc(sizeof(int16_t) * total_samples);
		if(!s->pcm) {
			return 0;
		}
		for(uint32_t i = 0; i < total_samples; ++i) {
			s->pcm[i] = (int16_t)sample_read_u16_be(body + i * 2);
		}
		s->pcm_frames = total_samples;
		if(repeat > 0) {
			s->has_loop = 1;
			s->loop_start = one_shot;
			s->loop_length = repeat;
		}
	} else {
		uint32_t per_channel = total_samples / 2;
		s->pcm = (int16_t *)malloc(sizeof(int16_t) * per_channel * 2);
		if(!s->pcm) {
			return 0;
		}
		for(uint32_t i = 0; i < per_channel; ++i) {
			s->pcm[i * 2 + 0] = (int16_t)sample_read_u16_be(body + i * 2);
			s->pcm[i * 2 + 1] = (int16_t)sample_read_u16_be(body + (per_channel + i) * 2);
		}
		s->pcm_frames = per_channel;
		if(repeat > 0) {
			s->has_loop = 1;
			s->loop_start = one_shot;
			s->loop_length = repeat;
		}
	}
	return 1;
}

// AIFF SSND payload always begins with offset and block-size fields, then big-endian PCM frames.
// [=]===^=[ sample_decode_aiff ]=================================================================[=]
static int32_t sample_decode_aiff(struct sample_state *s, uint8_t *ssnd, uint32_t ssnd_size) {
	if(ssnd_size < 8) {
		return 0;
	}
	uint32_t offset = sample_read_u32_be(ssnd);
	uint8_t *data = ssnd + 8 + offset;
	uint32_t avail = ssnd_size - 8 - offset;
	int32_t sample_size = (s->bits + 7) / 8;
	int32_t shift = sample_size * 8 - s->bits;
	uint32_t frames = avail / (sample_size * (uint32_t)s->channels);
	if(frames == 0) {
		return 0;
	}
	uint32_t total_samples = frames * (uint32_t)s->channels;
	s->pcm = (int16_t *)malloc(sizeof(int16_t) * total_samples);
	if(!s->pcm) {
		return 0;
	}
	for(uint32_t i = 0; i < total_samples; ++i) {
		int32_t v = 0;
		uint8_t *p = data + i * sample_size;
		switch(sample_size) {
			case 1: {
				v = ((int32_t)(int8_t)p[0]) << 8;
				v >>= shift;
				break;
			}

			case 2: {
				v = (int16_t)sample_read_u16_be(p);
				v >>= shift;
				break;
			}

			case 3: {
				int32_t raw = (p[0] << 24) | (p[1] << 16) | (p[2] << 8);
				raw >>= shift;
				v = raw >> 16;
				break;
			}

			case 4: {
				int32_t raw = (int32_t)sample_read_u32_be(p);
				raw >>= shift;
				v = raw >> 16;
				break;
			}
		}
		if(v > 32767) {
			v = 32767;
		}
		if(v < -32768) {
			v = -32768;
		}
		s->pcm[i] = (int16_t)v;
	}
	s->pcm_frames = frames;
	return 1;
}

// [=]===^=[ sample_decode_wave_pcm ]=============================================================[=]
static int32_t sample_decode_wave_pcm(struct sample_state *s, uint8_t *data, uint32_t data_size) {
	int32_t sample_size = (s->bits + 7) / 8;
	int32_t shift = sample_size * 8 - s->bits;
	uint32_t frames = data_size / (sample_size * (uint32_t)s->channels);
	if(frames == 0) {
		return 0;
	}
	uint32_t total_samples = frames * (uint32_t)s->channels;
	s->pcm = (int16_t *)malloc(sizeof(int16_t) * total_samples);
	if(!s->pcm) {
		return 0;
	}
	for(uint32_t i = 0; i < total_samples; ++i) {
		int32_t v = 0;
		uint8_t *p = data + i * sample_size;
		switch(sample_size) {
			case 1: {
				int32_t raw = (int32_t)p[0] - 128;
				v = raw << 8;
				v >>= shift;
				break;
			}

			case 2: {
				v = (int16_t)sample_read_u16_le(p);
				v >>= shift;
				break;
			}

			case 3: {
				int32_t raw = (p[2] << 24) | (p[1] << 16) | (p[0] << 8);
				raw >>= shift;
				v = raw >> 16;
				break;
			}

			case 4: {
				int32_t raw = (int32_t)sample_read_u32_le(p);
				raw >>= shift;
				v = raw >> 16;
				break;
			}
		}
		if(v > 32767) {
			v = 32767;
		}
		if(v < -32768) {
			v = -32768;
		}
		s->pcm[i] = (int16_t)v;
	}
	s->pcm_frames = frames;
	return 1;
}

// [=]===^=[ sample_decode_wave_float ]===========================================================[=]
static int32_t sample_decode_wave_float(struct sample_state *s, uint8_t *data, uint32_t data_size) {
	int32_t sample_size = s->bits / 8;
	if((sample_size != 4) && (sample_size != 8)) {
		return 0;
	}
	uint32_t frames = data_size / (sample_size * (uint32_t)s->channels);
	if(frames == 0) {
		return 0;
	}
	uint32_t total_samples = frames * (uint32_t)s->channels;
	s->pcm = (int16_t *)malloc(sizeof(int16_t) * total_samples);
	if(!s->pcm) {
		return 0;
	}
	for(uint32_t i = 0; i < total_samples; ++i) {
		uint8_t *p = data + i * sample_size;
		double f = 0.0;
		if(sample_size == 4) {
			union { uint32_t u; float f; } u;
			u.u = sample_read_u32_le(p);
			f = (double)u.f;
		} else {
			union { uint64_t u; double d; } u;
			u.u = (uint64_t)sample_read_u32_le(p) | ((uint64_t)sample_read_u32_le(p + 4) << 32);
			f = u.d;
		}
		if(f > 1.0) {
			f = 1.0;
		}
		if(f < -1.0) {
			f = -1.0;
		}
		s->pcm[i] = (int16_t)(f * 32767.0);
	}
	s->pcm_frames = frames;
	return 1;
}

// [=]===^=[ sample_load_iff_8svx ]===============================================================[=]
static int32_t sample_load_iff_8svx(struct sample_state *s, uint8_t *data, uint32_t len) {
	if(len < 12 || !sample_check_mark(data, "FORM") || !sample_check_mark(data + 8, "8SVX")) {
		return 0;
	}
	uint32_t cursor = 12;
	uint32_t one_shot = 0;
	uint32_t repeat = 0;
	uint8_t compression = 0;
	uint8_t got_vhdr = 0;
	uint8_t got_body = 0;
	uint8_t *body = 0;
	uint32_t body_size = 0;
	s->channels = 1;
	s->bits = 8;
	while(cursor < len) {
		char mark[5];
		uint32_t size = 0;
		uint8_t *payload = sample_iff_walk_chunk(data, len, &cursor, mark, 1, &size);
		if(!payload) {
			break;
		}
		if(memcmp(mark, "VHDR", 4) == 0 && size >= 20) {
			one_shot = sample_read_u32_be(payload);
			repeat = sample_read_u32_be(payload + 4);
			s->source_frequency = sample_read_u16_be(payload + 12);
			compression = payload[15];
			got_vhdr = 1;
		} else if(memcmp(mark, "CHAN", 4) == 0 && size >= 4) {
			if(sample_read_u32_be(payload) == 6) {
				s->channels = 2;
			}
		} else if(memcmp(mark, "BODY", 4) == 0) {
			body = payload;
			body_size = size;
			got_body = 1;
		}
	}
	if(!got_vhdr || !got_body) {
		return 0;
	}
	if(compression == 0) {
		return sample_decode_iff_8svx_pcm(s, body, body_size, one_shot, repeat);
	}
	if(compression == 1) {
		return sample_decode_iff_8svx_fib(s, body, body_size);
	}
	return 0;
}

// [=]===^=[ sample_load_iff_16sv ]===============================================================[=]
static int32_t sample_load_iff_16sv(struct sample_state *s, uint8_t *data, uint32_t len) {
	if(len < 12 || !sample_check_mark(data, "FORM") || !sample_check_mark(data + 8, "16SV")) {
		return 0;
	}
	uint32_t cursor = 12;
	uint32_t one_shot = 0;
	uint32_t repeat = 0;
	uint8_t got_vhdr = 0;
	uint8_t got_body = 0;
	uint8_t *body = 0;
	uint32_t body_size = 0;
	s->channels = 1;
	s->bits = 16;
	while(cursor < len) {
		char mark[5];
		uint32_t size = 0;
		uint8_t *payload = sample_iff_walk_chunk(data, len, &cursor, mark, 1, &size);
		if(!payload) {
			break;
		}
		if(memcmp(mark, "VHDR", 4) == 0 && size >= 20) {
			one_shot = sample_read_u32_be(payload);
			repeat = sample_read_u32_be(payload + 4);
			s->source_frequency = sample_read_u16_be(payload + 12);
			got_vhdr = 1;
		} else if(memcmp(mark, "CHAN", 4) == 0 && size >= 4) {
			if(sample_read_u32_be(payload) == 6) {
				s->channels = 2;
			}
		} else if(memcmp(mark, "BODY", 4) == 0) {
			body = payload;
			body_size = size;
			got_body = 1;
		}
	}
	if(!got_vhdr || !got_body) {
		return 0;
	}
	return sample_decode_iff_16sv(s, body, body_size, one_shot, repeat);
}

// [=]===^=[ sample_load_aiff ]===================================================================[=]
static int32_t sample_load_aiff(struct sample_state *s, uint8_t *data, uint32_t len) {
	if(len < 12 || !sample_check_mark(data, "FORM") || !sample_check_mark(data + 8, "AIFF")) {
		return 0;
	}
	uint32_t cursor = 12;
	uint8_t got_comm = 0;
	uint8_t got_ssnd = 0;
	uint8_t *ssnd = 0;
	uint32_t ssnd_size = 0;
	while(cursor < len) {
		char mark[5];
		uint32_t size = 0;
		uint8_t *payload = sample_iff_walk_chunk(data, len, &cursor, mark, 1, &size);
		if(!payload) {
			break;
		}
		if(memcmp(mark, "COMM", 4) == 0 && size >= 18) {
			s->channels = (int32_t)sample_read_u16_be(payload);
			s->bits = (int32_t)sample_read_u16_be(payload + 6);
			s->source_frequency = (int32_t)sample_ieee_extended_to_double(payload + 8);
			got_comm = 1;
		} else if(memcmp(mark, "SSND", 4) == 0) {
			ssnd = payload;
			ssnd_size = size;
			got_ssnd = 1;
		}
	}
	if(!got_comm || !got_ssnd || s->channels < 1 || s->channels > 2) {
		return 0;
	}
	return sample_decode_aiff(s, ssnd, ssnd_size);
}

// [=]===^=[ sample_load_riff_wave ]==============================================================[=]
static int32_t sample_load_riff_wave(struct sample_state *s, uint8_t *data, uint32_t len) {
	if(len < 12 || !sample_check_mark(data, "RIFF") || !sample_check_mark(data + 8, "WAVE")) {
		return 0;
	}
	uint32_t cursor = 12;
	uint8_t got_fmt = 0;
	uint8_t got_data = 0;
	uint16_t format_tag = 0;
	uint8_t *audio = 0;
	uint32_t audio_size = 0;
	while(cursor < len) {
		char mark[5];
		uint32_t size = 0;
		uint8_t *payload = sample_iff_walk_chunk(data, len, &cursor, mark, 0, &size);
		if(!payload) {
			break;
		}
		if(memcmp(mark, "fmt ", 4) == 0 && size >= 16) {
			format_tag = sample_read_u16_le(payload);
			s->channels = (int32_t)sample_read_u16_le(payload + 2);
			s->source_frequency = (int32_t)sample_read_u32_le(payload + 4);
			s->bits = (int32_t)sample_read_u16_le(payload + 14);
			if(format_tag == 0xfffe && size >= 16 + 2 + 22) {
				// WAVE_FORMAT_EXTENSIBLE: real format lives in the SubFormat GUID's first uint16.
				format_tag = sample_read_u16_le(payload + 16 + 2 + 6);
			}
			got_fmt = 1;
		} else if(memcmp(mark, "data", 4) == 0) {
			audio = payload;
			audio_size = size;
			got_data = 1;
		}
	}
	if(!got_fmt || !got_data || s->channels < 1 || s->channels > 2) {
		return 0;
	}
	if(format_tag == 0x0001) {
		return sample_decode_wave_pcm(s, audio, audio_size);
	}
	if(format_tag == 0x0003) {
		return sample_decode_wave_float(s, audio, audio_size);
	}
	return 0;
}

// [=]===^=[ sample_init ]========================================================================[=]
static struct sample_state *sample_init(void *data, uint32_t len, int32_t sample_rate) {
	if(!data || len < 12 || sample_rate < 8000) {
		return 0;
	}

	struct sample_state *s = (struct sample_state *)calloc(1, sizeof(struct sample_state));
	if(!s) {
		return 0;
	}
	s->module_data = (uint8_t *)data;
	s->module_len = len;
	s->host_sample_rate = sample_rate;

	uint8_t *d = s->module_data;
	int32_t ok = 0;
	if(sample_check_mark(d, "FORM")) {
		if(sample_check_mark(d + 8, "8SVX")) {
			ok = sample_load_iff_8svx(s, d, len);
		} else if(sample_check_mark(d + 8, "16SV")) {
			ok = sample_load_iff_16sv(s, d, len);
		} else if(sample_check_mark(d + 8, "AIFF")) {
			ok = sample_load_aiff(s, d, len);
		}
	} else if(sample_check_mark(d, "RIFF") && sample_check_mark(d + 8, "WAVE")) {
		ok = sample_load_riff_wave(s, d, len);
	}

	if(!ok || !s->pcm || s->pcm_frames == 0 || s->source_frequency <= 0) {
		if(s->pcm) {
			free(s->pcm);
		}
		free(s);
		return 0;
	}

	s->step_fp = ((uint64_t)s->source_frequency << SAMPLE_FP_SHIFT) / (uint64_t)s->host_sample_rate;
	s->pos_fp = 0;
	return s;
}

// [=]===^=[ sample_free ]========================================================================[=]
static void sample_free(struct sample_state *s) {
	if(!s) {
		return;
	}
	if(s->pcm) {
		free(s->pcm);
	}
	free(s);
}

// [=]===^=[ sample_get_audio ]===================================================================[=]
static void sample_get_audio(struct sample_state *s, int16_t *output, int32_t frames) {
	uint64_t length_fp = (uint64_t)s->pcm_frames << SAMPLE_FP_SHIFT;
	uint64_t loop_start_fp = (uint64_t)s->loop_start << SAMPLE_FP_SHIFT;
	uint64_t loop_length_fp = (uint64_t)s->loop_length << SAMPLE_FP_SHIFT;

	for(int32_t i = 0; i < frames; ++i) {
		if(s->pos_fp >= length_fp) {
			if(s->has_loop && loop_length_fp > 0) {
				uint64_t over = s->pos_fp - length_fp;
				s->pos_fp = loop_start_fp + (over % loop_length_fp);
			} else {
				output[0] += 0;
				output[1] += 0;
				output += 2;
				continue;
			}
		}
		uint32_t idx = (uint32_t)(s->pos_fp >> SAMPLE_FP_SHIFT);
		uint32_t frac = (uint32_t)(s->pos_fp & (SAMPLE_FP_ONE - 1));
		uint32_t next = idx + 1;
		if(next >= s->pcm_frames) {
			if(s->has_loop && loop_length_fp > 0) {
				next = s->loop_start;
			} else {
				next = idx;
			}
		}
		int32_t left;
		int32_t right;
		if(s->channels == 1) {
			int32_t a = s->pcm[idx];
			int32_t b = s->pcm[next];
			int32_t v = a + (((b - a) * (int32_t)frac) >> SAMPLE_FP_SHIFT);
			left = v;
			right = v;
		} else {
			int32_t la = s->pcm[idx * 2 + 0];
			int32_t lb = s->pcm[next * 2 + 0];
			int32_t ra = s->pcm[idx * 2 + 1];
			int32_t rb = s->pcm[next * 2 + 1];
			left  = la + (((lb - la) * (int32_t)frac) >> SAMPLE_FP_SHIFT);
			right = ra + (((rb - ra) * (int32_t)frac) >> SAMPLE_FP_SHIFT);
		}
		int32_t mixed_l = (int32_t)output[0] + left;
		int32_t mixed_r = (int32_t)output[1] + right;
		if(mixed_l > 32767) {
			mixed_l = 32767;
		}
		if(mixed_l < -32768) {
			mixed_l = -32768;
		}
		if(mixed_r > 32767) {
			mixed_r = 32767;
		}
		if(mixed_r < -32768) {
			mixed_r = -32768;
		}
		output[0] = (int16_t)mixed_l;
		output[1] = (int16_t)mixed_r;
		output += 2;
		s->pos_fp += s->step_fp;
	}
}

// [=]===^=[ sample_api_init ]====================================================================[=]
static void *sample_api_init(void *data, uint32_t len, int32_t sample_rate) {
	return sample_init(data, len, sample_rate);
}

// [=]===^=[ sample_api_free ]====================================================================[=]
static void sample_api_free(void *state) {
	sample_free((struct sample_state *)state);
}

// [=]===^=[ sample_api_get_audio ]===============================================================[=]
static void sample_api_get_audio(void *state, int16_t *output, int32_t frames) {
	sample_get_audio((struct sample_state *)state, output, frames);
}

static const char *sample_extensions[] = { "iff", "8svx", "16sv", "iff16", "aiff", "aif", "wav", 0 };

static struct player_api sample_api = {
	"Sample",
	sample_extensions,
	sample_api_init,
	sample_api_free,
	sample_api_get_audio,
	0,
};
