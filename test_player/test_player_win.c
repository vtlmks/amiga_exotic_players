// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Windows (mingw) port of test_player. Links against msvcrt.dll (a system DLL
// shipped with every Windows since 95), kernel32.dll, user32.dll, and
// winmm.dll. No installed runtime required.
//
// Build: ./build_win.sh

#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


#include "../player_api.h"
#include "../sidmon10.h"
#include "../sidmon20.h"
#include "../soundmon.h"
#include "../jamcracker.h"
#include "../futurecomposer.h"
#include "../amosmusicbank.h"
#include "../fred.h"
#include "../digitalmugician.h"
#include "../deltamusic20.h"
#include "../davidwhittaker.h"
#include "../bendaglish.h"
#include "../gamemusiccreator.h"
#include "../deltamusic10.h"
#include "../musicassembler.h"
#include "../ronklaren.h"
#include "../actionamics.h"
#include "../hippel.h"
#include "../soundfx.h"
#include "../oktalyzer.h"
#include "../instereo10.h"
#include "../instereo20.h"
#include "../pumatracker.h"
#include "../digitalsoundstudio.h"
#include "../quadracomposer.h"
#include "../med.h"
#include "../synthesis.h"
#include "../digibooster.h"
#include "../sonicarranger.h"
#include "../tfmx.h"
#include "../activisionpro.h"
#include "../iffsmus.h"
#include "../soundfactory.h"
#include "../soundcontrol.h"
#include "../voodoosupremesynthesizer.h"
#include "../artofnoise.h"
#include "../octamed.h"
#include "../facethemusic.h"
#include "../sample.h"
#include "../digiboosterpro.h"
#include "../hivelytracker.h"
#include "../fashiontracker.h"
#include "../soundtracker.h"

#define SAMPLE_RATE       48000
#define NUM_CHANNELS      2
#define FRAMES_PER_PERIOD 1024
#define NUM_WAVE_BUFFERS  4

static struct player_api *g_players[] = {
	&sidmon10_api,
	&sidmon20_api,
	&soundmon_api,
	&jamcracker_api,
	&futurecomposer_api,
	&amosmusicbank_api,
	&fred_api,
	&digitalmugician_api,
	&deltamusic20_api,
	&davidwhittaker_api,
	&bendaglish_api,
	&gamemusiccreator_api,
	&deltamusic10_api,
	&musicassembler_api,
	&ronklaren_api,
	&actionamics_api,
	&hippel_api,
	&soundfx_api,
	&oktalyzer_api,
	&instereo10_api,
	&instereo20_api,
	&pumatracker_api,
	&digitalsoundstudio_api,
	&quadracomposer_api,
	&med_api,
	&synthesis_api,
	&digibooster_api,
	&sonicarranger_api,
	&tfmx_api,
	&activisionpro_api,
	&iffsmus_api,
	&soundfactory_api,
	&soundcontrol_api,
	&voodoosupremesynthesizer_api,
	&artofnoise_api,
	&octamed_api,
	&facethemusic_api,
	&sample_api,
	&digiboosterpro_api,
	&hivelytracker_api,
	&fashiontracker_api,
	&soundtracker_api,
	0,
};

static volatile LONG g_running = 1;

// [=]===^=[ console_ctrl_handler ]===============================================================[=]
static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
	switch(ctrl_type) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			InterlockedExchange(&g_running, 0);
			return TRUE;
		default:
			return FALSE;
	}
}

// [=]===^=[ load_file ]==========================================================================[=]
static uint8_t *load_file(const char *path, uint32_t *out_len) {
	HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if(h == INVALID_HANDLE_VALUE) {
		return 0;
	}
	LARGE_INTEGER sz;
	if(!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 0x7FFFFFFF) {
		CloseHandle(h);
		return 0;
	}
	uint32_t len = (uint32_t)sz.QuadPart;
	uint8_t *buf = (uint8_t *)malloc(len);
	if(!buf) {
		CloseHandle(h);
		return 0;
	}
	DWORD got = 0;
	if(!ReadFile(h, buf, len, &got, 0) || got != len) {
		free(buf);
		CloseHandle(h);
		return 0;
	}
	CloseHandle(h);
	*out_len = len;
	return buf;
}

// [=]===^=[ basename_of ]========================================================================[=]
static const char *basename_of(const char *path) {
	const char *base = path;
	for(const char *p = path; *p; ++p) {
		if(*p == '/' || *p == '\\') {
			base = p + 1;
		}
	}
	return base;
}

// [=]===^=[ ascii_lower ]========================================================================[=]
static char ascii_lower(char c) {
	return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

// [=]===^=[ ascii_atoi ]=========================================================================[=]
static int32_t ascii_atoi(const char *s) {
	int32_t sign = 1;
	int32_t v = 0;
	while(*s == ' ' || *s == '\t') {
		++s;
	}
	if(*s == '-') {
		sign = -1;
		++s;
	} else if(*s == '+') {
		++s;
	}
	while(*s >= '0' && *s <= '9') {
		v = v * 10 + (*s - '0');
		++s;
	}
	return v * sign;
}

// [=]===^=[ try_tfmx_bundle ]====================================================================[=]
static uint8_t *try_tfmx_bundle(const char *path, uint8_t *mdat_data, uint32_t mdat_len, uint32_t *out_len) {
	const char *base = basename_of(path);
	size_t blen = strlen(base);
	if(blen < 6) {
		return 0;
	}
	int32_t is_mdat = (ascii_lower(base[0]) == 'm') &&
	                  (ascii_lower(base[1]) == 'd') &&
	                  (ascii_lower(base[2]) == 'a') &&
	                  (ascii_lower(base[3]) == 't') &&
	                  (base[4] == '.');
	if(!is_mdat) {
		return 0;
	}

	size_t prefix_len = (size_t)(base - path);
	size_t total = prefix_len + blen + 1;
	char *smpl_path = (char *)malloc(total);
	if(!smpl_path) {
		return 0;
	}
	memcpy(smpl_path, path, prefix_len);
	memcpy(smpl_path + prefix_len, "smpl", 4);
	memcpy(smpl_path + prefix_len + 4, base + 4, blen - 4);
	smpl_path[prefix_len + blen] = 0;

	uint32_t smpl_len = 0;
	uint8_t *smpl_data = load_file(smpl_path, &smpl_len);
	free(smpl_path);
	if(!smpl_data) {
		return 0;
	}

	uint32_t hdr_off = 18;
	uint32_t bundle_len = hdr_off + mdat_len + smpl_len;
	uint8_t *bundle = (uint8_t *)malloc(bundle_len);
	if(!bundle) {
		free(smpl_data);
		return 0;
	}
	memset(bundle, 0, hdr_off);
	bundle[0] = 'T'; bundle[1] = 'F'; bundle[2] = 'H'; bundle[3] = 'D';
	bundle[4] = 0; bundle[5] = 0;
	bundle[6] = (uint8_t)(hdr_off >> 8); bundle[7] = (uint8_t)hdr_off;
	bundle[8] = 0;
	bundle[9] = 0;
	bundle[10] = (uint8_t)(mdat_len >> 24); bundle[11] = (uint8_t)(mdat_len >> 16);
	bundle[12] = (uint8_t)(mdat_len >> 8);  bundle[13] = (uint8_t)mdat_len;
	bundle[14] = (uint8_t)(smpl_len >> 24); bundle[15] = (uint8_t)(smpl_len >> 16);
	bundle[16] = (uint8_t)(smpl_len >> 8);  bundle[17] = (uint8_t)smpl_len;
	memcpy(bundle + hdr_off, mdat_data, mdat_len);
	memcpy(bundle + hdr_off + mdat_len, smpl_data, smpl_len);

	free(smpl_data);
	*out_len = bundle_len;
	return bundle;
}

// [=]===^=[ get_extension ]======================================================================[=]
static const char *get_extension(const char *path, char *dst, size_t dst_size) {
	const char *base = path;
	for(const char *p = path; *p; ++p) {
		if(*p == '/' || *p == '\\') {
			base = p + 1;
		}
	}
	const char *dot = 0;
	for(const char *p = base; *p; ++p) {
		if(*p == '.') {
			dot = p;
		}
	}
	if(!dot || !dot[1]) {
		return 0;
	}
	size_t i = 0;
	for(const char *p = dot + 1; *p && i + 1 < dst_size; ++p, ++i) {
		dst[i] = ascii_lower(*p);
	}
	dst[i] = 0;
	return dst;
}

// [=]===^=[ extension_matches ]==================================================================[=]
static int32_t extension_matches(struct player_api *api, const char *ext) {
	if(!ext || !api->extensions) {
		return 0;
	}
	for(const char **e = api->extensions; *e; ++e) {
		if(strcmp(*e, ext) == 0) {
			return 1;
		}
	}
	return 0;
}

// [=]===^=[ fs_loader_fetch ]====================================================================[=]
static uint8_t *fs_loader_fetch(void *ctx, const char *name, uint32_t *out_len) {
	const char *base_dir = (const char *)ctx;
	if(!base_dir || !name || !out_len) {
		return 0;
	}
	size_t dlen = strlen(base_dir);
	size_t nlen = strlen(name);
	char *full = (char *)malloc(dlen + 1 + nlen + 1);
	if(!full) {
		return 0;
	}
	memcpy(full, base_dir, dlen);
	full[dlen] = '/';
	memcpy(full + dlen + 1, name, nlen);
	full[dlen + 1 + nlen] = 0;
	uint32_t flen = 0;
	uint8_t *buf = load_file(full, &flen);
	free(full);
	if(buf) {
		*out_len = flen;
	}
	return buf;
}

// [=]===^=[ try_init ]===========================================================================[=]
static void *try_init(struct player_api *api, uint8_t *data, uint32_t len, struct player_loader *loader) {
	if(api->init_ex) {
		void *state = api->init_ex(data, len, SAMPLE_RATE, loader);
		if(state) {
			return state;
		}
	}
	return api->init(data, len, SAMPLE_RATE);
}

// [=]===^=[ dispatch ]===========================================================================[=]
static void *dispatch(uint8_t *data, uint32_t len, const char *ext, struct player_api **out_api, struct player_loader *loader) {
	for(int32_t i = 0; g_players[i]; ++i) {
		if(extension_matches(g_players[i], ext)) {
			void *state = try_init(g_players[i], data, len, loader);
			if(state) {
				*out_api = g_players[i];
				return state;
			}
		}
	}
	for(int32_t i = 0; g_players[i]; ++i) {
		if(!extension_matches(g_players[i], ext)) {
			void *state = try_init(g_players[i], data, len, loader);
			if(state) {
				*out_api = g_players[i];
				return state;
			}
		}
	}
	return 0;
}

// [=]===^=[ open_waveout ]=======================================================================[=]
static int32_t open_waveout(HWAVEOUT *out_h) {
	WAVEFORMATEX wf;
	memset(&wf, 0, sizeof(wf));
	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = NUM_CHANNELS;
	wf.nSamplesPerSec = SAMPLE_RATE;
	wf.wBitsPerSample = 16;
	wf.nBlockAlign = wf.nChannels * (wf.wBitsPerSample / 8);
	wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
	MMRESULT r = waveOutOpen(out_h, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL);
	if(r != MMSYSERR_NOERROR) {
		fprintf(stderr, "waveOutOpen failed: %u\n", (unsigned)r);
		return 0;
	}
	return 1;
}

// [=]===^=[ apply_crossfeed ]====================================================================[=]
static void apply_crossfeed(int16_t *buf, int32_t frames, int32_t self_q15, int32_t xf_q15) {
	for(int32_t j = 0; j < frames; ++j) {
		int32_t l = buf[j * 2 + 0];
		int32_t r = buf[j * 2 + 1];
		int32_t nl = (l * self_q15 + r * xf_q15) >> 15;
		int32_t nr = (r * self_q15 + l * xf_q15) >> 15;
		if(nl > 32767) { nl = 32767; } else if(nl < -32768) { nl = -32768; }
		if(nr > 32767) { nr = 32767; } else if(nr < -32768) { nr = -32768; }
		buf[j * 2 + 0] = (int16_t)nl;
		buf[j * 2 + 1] = (int16_t)nr;
	}
}

// [=]===^=[ run_player ]=========================================================================[=]
static int32_t run_player(const char *path) {
	uint32_t len = 0;
	uint8_t *data = load_file(path, &len);
	if(!data) {
		fprintf(stderr, "failed to load: %s\n", path);
		return 1;
	}
	printf("loaded %u bytes\n", len);

	uint32_t bundle_len = 0;
	uint8_t *bundle = try_tfmx_bundle(path, data, len, &bundle_len);
	if(bundle) {
		printf("bundled mdat.+smpl. into %u bytes\n", bundle_len);
		free(data);
		data = bundle;
		len = bundle_len;
	}

	char ext_buf[16];
	const char *ext = get_extension(path, ext_buf, sizeof(ext_buf));
	if(ext) {
		printf("extension: .%s\n", ext);
	}

	char *base_dir = 0;
	{
		const char *base = basename_of(path);
		size_t prefix_len = (size_t)(base - path);
		if(prefix_len == 0) {
			base_dir = (char *)malloc(2);
			if(base_dir) {
				base_dir[0] = '.';
				base_dir[1] = 0;
			}
		} else {
			base_dir = (char *)malloc(prefix_len + 1);
			if(base_dir) {
				memcpy(base_dir, path, prefix_len);
				char last = base_dir[prefix_len - 1];
				if(prefix_len > 1 && (last == '/' || last == '\\')) {
					base_dir[prefix_len - 1] = 0;
				} else {
					base_dir[prefix_len] = 0;
				}
			}
		}
	}
	struct player_loader fs_loader = { base_dir, fs_loader_fetch };

	struct player_api *api = 0;
	void *state = dispatch(data, len, ext, &api, &fs_loader);
	if(!state) {
		fprintf(stderr, "no registered player recognised this file\n");
		free(base_dir);
		free(data);
		return 1;
	}
	printf("player: %s\n", api->name);

	HWAVEOUT wave;
	if(!open_waveout(&wave)) {
		api->free(state);
		free(base_dir);
		free(data);
		return 1;
	}

	WAVEHDR hdrs[NUM_WAVE_BUFFERS];
	int16_t *bufs[NUM_WAVE_BUFFERS];
	int32_t buf_bytes = FRAMES_PER_PERIOD * NUM_CHANNELS * (int32_t)sizeof(int16_t);
	for(int32_t i = 0; i < NUM_WAVE_BUFFERS; ++i) {
		bufs[i] = (int16_t *)calloc(FRAMES_PER_PERIOD * NUM_CHANNELS, sizeof(int16_t));
		memset(&hdrs[i], 0, sizeof(hdrs[i]));
		hdrs[i].lpData = (LPSTR)bufs[i];
		hdrs[i].dwBufferLength = (DWORD)buf_bytes;
		waveOutPrepareHeader(wave, &hdrs[i], sizeof(hdrs[i]));
	}

	printf("playing (ctrl+c to exit)\n");

	int32_t xf_q15;
	int32_t self_q15;
	{
		int32_t xf_pct = 40;
		char env[16];
		DWORD got = GetEnvironmentVariableA("CROSSFEED", env, sizeof(env));
		if(got > 0 && got < sizeof(env)) {
			xf_pct = ascii_atoi(env);
			if(xf_pct < 0) { xf_pct = 0; }
			if(xf_pct > 50) { xf_pct = 50; }
		}
		xf_q15 = (xf_pct * 32768 + 50) / 100;
		self_q15 = 32768 - xf_q15;
		printf("crossfeed: %d%% (each ear: %d%% own, %d%% other)\n",
			xf_pct, 100 - xf_pct, xf_pct);
	}

	for(int32_t i = 0; i < NUM_WAVE_BUFFERS; ++i) {
		memset(bufs[i], 0, (size_t)buf_bytes);
		api->get_audio(state, bufs[i], FRAMES_PER_PERIOD);
		if(xf_q15 != 0) {
			apply_crossfeed(bufs[i], FRAMES_PER_PERIOD, self_q15, xf_q15);
		}
		waveOutWrite(wave, &hdrs[i], sizeof(hdrs[i]));
	}

	while(InterlockedCompareExchange(&g_running, 0, 0)) {
		int32_t any_filled = 0;
		for(int32_t i = 0; i < NUM_WAVE_BUFFERS; ++i) {
			if((hdrs[i].dwFlags & WHDR_DONE) == 0) {
				continue;
			}
			memset(bufs[i], 0, (size_t)buf_bytes);
			api->get_audio(state, bufs[i], FRAMES_PER_PERIOD);
			if(xf_q15 != 0) {
				apply_crossfeed(bufs[i], FRAMES_PER_PERIOD, self_q15, xf_q15);
			}
			waveOutWrite(wave, &hdrs[i], sizeof(hdrs[i]));
			any_filled = 1;
		}
		if(!any_filled) {
			Sleep(2);
		}
	}

	printf("\nshutting down\n");
	waveOutReset(wave);
	for(int32_t i = 0; i < NUM_WAVE_BUFFERS; ++i) {
		waveOutUnprepareHeader(wave, &hdrs[i], sizeof(hdrs[i]));
		free(bufs[i]);
	}
	waveOutClose(wave);
	api->free(state);
	free(base_dir);
	free(data);
	return 0;
}

// [=]===^=[ print_usage ]========================================================================[=]
static void print_usage(const char *progname) {
	fprintf(stderr, "usage: %s <file>\n", progname);
	fprintf(stderr, "registered players:\n");
	for(int32_t i = 0; g_players[i]; ++i) {
		fprintf(stderr, "  %s (", g_players[i]->name);
		for(const char **e = g_players[i]->extensions; *e; ++e) {
			fprintf(stderr, "%s.%s", (e == g_players[i]->extensions) ? "" : " ", *e);
		}
		fprintf(stderr, ")\n");
	}
}

// [=]===^=[ main ]===============================================================================[=]
int main(int argc, char **argv) {
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
	SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

	if(argc < 2) {
		print_usage(argv[0]);
		return 1;
	}
	return run_player(argv[1]);
}
