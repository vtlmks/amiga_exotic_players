// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Windows (mingw) port of test_player. CRT-free build: depends only on
// kernel32.dll, user32.dll, and winmm.dll (all shipped with every Windows
// since Win95). Replaces stdio with WriteFile, malloc/free with HeapAlloc/
// HeapFree, and provides minimal in-file implementations of the few math /
// memory / string functions the player headers reference.
//
// Build: ./build_win.sh

#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

// ============================================================================
// CRT shims. Function signatures must match <string.h> / <stdlib.h> / <math.h>
// so the player headers' declarations resolve to these. Linker uses our defs
// rather than pulling anything from libc.
// ============================================================================

// [=]===^=[ memset ]=============================================================================[=]
void *memset(void *dst, int v, size_t n) {
	uint8_t *d = (uint8_t *)dst;
	uint8_t b = (uint8_t)v;
	for(size_t i = 0; i < n; ++i) {
		d[i] = b;
	}
	return dst;
}

// [=]===^=[ memcpy ]=============================================================================[=]
void *memcpy(void *dst, const void *src, size_t n) {
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;
	for(size_t i = 0; i < n; ++i) {
		d[i] = s[i];
	}
	return dst;
}

// [=]===^=[ memmove ]============================================================================[=]
void *memmove(void *dst, const void *src, size_t n) {
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;
	if(d < s || d >= s + n) {
		for(size_t i = 0; i < n; ++i) {
			d[i] = s[i];
		}
	} else {
		for(size_t i = n; i--; ) {
			d[i] = s[i];
		}
	}
	return dst;
}

// [=]===^=[ memcmp ]=============================================================================[=]
int memcmp(const void *a, const void *b, size_t n) {
	const uint8_t *p = (const uint8_t *)a;
	const uint8_t *q = (const uint8_t *)b;
	for(size_t i = 0; i < n; ++i) {
		if(p[i] != q[i]) {
			return (int)p[i] - (int)q[i];
		}
	}
	return 0;
}

// [=]===^=[ strlen ]=============================================================================[=]
size_t strlen(const char *s) {
	size_t n = 0;
	while(s[n] != 0) {
		++n;
	}
	return n;
}

// [=]===^=[ strcmp ]=============================================================================[=]
int strcmp(const char *a, const char *b) {
	while(*a != 0 && *a == *b) {
		++a;
		++b;
	}
	return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

// [=]===^=[ strncmp ]============================================================================[=]
int strncmp(const char *a, const char *b, size_t n) {
	while(n--) {
		if(*a != *b) {
			return (int)(uint8_t)*a - (int)(uint8_t)*b;
		}
		if(*a == 0) {
			return 0;
		}
		++a;
		++b;
	}
	return 0;
}

// [=]===^=[ strncpy ]============================================================================[=]
char *strncpy(char *dst, const char *src, size_t n) {
	size_t i = 0;
	for(; i < n && src[i] != 0; ++i) {
		dst[i] = src[i];
	}
	for(; i < n; ++i) {
		dst[i] = 0;
	}
	return dst;
}

// [=]===^=[ strchr ]=============================================================================[=]
char *strchr(const char *s, int c) {
	while(*s != 0) {
		if(*s == (char)c) {
			return (char *)s;
		}
		++s;
	}
	return c == 0 ? (char *)s : 0;
}

// [=]===^=[ malloc ]=============================================================================[=]
void *malloc(size_t n) {
	if(n == 0) {
		n = 1;
	}
	return HeapAlloc(GetProcessHeap(), 0, n);
}

// [=]===^=[ calloc ]=============================================================================[=]
void *calloc(size_t n, size_t s) {
	size_t total = n * s;
	if(total == 0) {
		total = 1;
	}
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, total);
}

// [=]===^=[ realloc ]============================================================================[=]
void *realloc(void *p, size_t n) {
	if(p == 0) {
		return malloc(n);
	}
	if(n == 0) {
		HeapFree(GetProcessHeap(), 0, p);
		return 0;
	}
	return HeapReAlloc(GetProcessHeap(), 0, p, n);
}

// [=]===^=[ free ]===============================================================================[=]
void free(void *p) {
	if(p != 0) {
		HeapFree(GetProcessHeap(), 0, p);
	}
}

// [=]===^=[ qsort ]==============================================================================[=]
// Simple insertion sort -- not asymptotically great but qsort is only called
// from format-specific load paths on small arrays (sample lists, instrument
// tables) so O(n^2) is fine. Avoids needing a real recursive quicksort with
// pivot heuristics in our minimal-CRT build.
void qsort(void *base, size_t nmemb, size_t size, int (*cmp)(const void *, const void *)) {
	uint8_t *a = (uint8_t *)base;
	for(size_t i = 1; i < nmemb; ++i) {
		for(size_t j = i; j > 0; --j) {
			uint8_t *p = a + (j - 1) * size;
			uint8_t *q = a + j * size;
			if(cmp(p, q) <= 0) {
				break;
			}
			for(size_t k = 0; k < size; ++k) {
				uint8_t t = p[k];
				p[k] = q[k];
				q[k] = t;
			}
		}
	}
}

// [=]===^=[ ldexp ]==============================================================================[=]
// Standard C: v * 2^e. Implement via direct manipulation of the IEEE 754
// double's exponent field. Underflow returns +/-0; overflow returns +/-inf.
double ldexp(double v, int e) {
	union { double d; uint64_t u; } x;
	x.d = v;
	uint64_t mant = x.u & 0x000FFFFFFFFFFFFFu;
	uint64_t sign = x.u & 0x8000000000000000u;
	int32_t exp = (int32_t)((x.u >> 52) & 0x7FFu);
	if(exp == 0 && mant == 0) {
		return v;
	}
	if(exp == 0x7FF) {
		return v;
	}
	exp += e;
	if(exp <= 0) {
		x.u = sign;
		return x.d;
	}
	if(exp >= 0x7FF) {
		x.u = sign | 0x7FF0000000000000u;
		return x.d;
	}
	x.u = sign | ((uint64_t)exp << 52) | mant;
	return x.d;
}

// [=]===^=[ sin ]================================================================================[=]
// Taylor-series sin to ~1e-12 precision after argument reduction to [-pi, pi].
// Adequate for the player headers' panning/lookup-table init paths; not in
// any audio hot loop.
double sin(double x) {
	static const double TWO_PI = 6.28318530717958647692;
	static const double PI     = 3.14159265358979323846;
	while(x >  PI) { x -= TWO_PI; }
	while(x < -PI) { x += TWO_PI; }
	double x2 = x * x;
	double term = x;
	double sum = x;
	int32_t sign = -1;
	for(int32_t i = 1; i < 10; ++i) {
		double n = (double)(2 * i);
		double divisor = n * (n + 1.0);
		term = term * x2 / divisor;
		sum += sign * term;
		sign = -sign;
	}
	return sum;
}

// [=]===^=[ pow ]================================================================================[=]
// pow(b, e). Player code uses pow(2.0, x) only; we implement the general
// case via exp(e * log(b)) but optimise the b == 2.0 path.
static double exp2_local(double e) {
	int32_t int_e = (int32_t)e;
	if((double)int_e > e) {
		--int_e;
	}
	double frac = e - (double)int_e;
	static const double LN2 = 0.69314718055994530942;
	double f = frac * LN2;
	double term = 1.0;
	double sum = 1.0;
	for(int32_t k = 1; k < 16; ++k) {
		term *= f / (double)k;
		sum += term;
	}
	return ldexp(sum, int_e);
}

double pow(double b, double e) {
	if(b == 2.0) {
		return exp2_local(e);
	}
	// Generic path is unused by the registered players; provide a sensible
	// fallback rather than risk silent bugs.
	if(b <= 0.0) {
		return 0.0;
	}
	// log(b): use ldexp split (b = m * 2^k where m in [1, 2)).
	union { double d; uint64_t u; } x;
	x.d = b;
	int32_t k = (int32_t)((x.u >> 52) & 0x7FFu) - 1023;
	x.u = (x.u & 0x800FFFFFFFFFFFFFu) | ((uint64_t)1023 << 52);
	double m = x.d;
	// log(m) via Taylor at m near 1: log(1+y) where y = m - 1
	double y = m - 1.0;
	double term = y;
	double log_m = 0.0;
	int32_t sign = 1;
	for(int32_t i = 1; i < 30; ++i) {
		log_m += sign * term / (double)i;
		term *= y;
		sign = -sign;
	}
	static const double LN2 = 0.69314718055994530942;
	double log_b = log_m + (double)k * LN2;
	return exp2_local(e * log_b / LN2);
}

// ============================================================================
// Player headers. Their <stdio.h> / <stdlib.h> / <string.h> / <math.h>
// includes contribute declarations only; none of the active code paths in the
// registered players call functions outside of what we've defined above.
// ============================================================================

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

// ============================================================================
// Win32-based replacements for stdio / file I/O / signal handling.
// ============================================================================

static volatile LONG g_running = 1;
static HANDLE g_stdout = 0;
static HANDLE g_stderr = 0;

// [=]===^=[ wprint ]=============================================================================[=]
// Format-and-write to a Win32 console handle. wvsprintfA (user32.dll) handles
// %s / %d / %u / %x / %c / %% etc -- enough for everything this program prints.
// Fixed 1024-byte buffer; longer messages would truncate, but none exceed it
// in this code path.
static void wprint(HANDLE h, const char *fmt, ...) {
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	int n = wvsprintfA(buf, fmt, ap);
	va_end(ap);
	if(n <= 0) {
		return;
	}
	DWORD wrote = 0;
	WriteFile(h, buf, (DWORD)n, &wrote, 0);
}

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
		wprint(g_stderr, "waveOutOpen failed: %u\n", (unsigned)r);
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
		wprint(g_stderr, "failed to load: %s\n", path);
		return 1;
	}
	wprint(g_stdout, "loaded %u bytes\n", len);

	uint32_t bundle_len = 0;
	uint8_t *bundle = try_tfmx_bundle(path, data, len, &bundle_len);
	if(bundle) {
		wprint(g_stdout, "bundled mdat.+smpl. into %u bytes\n", bundle_len);
		free(data);
		data = bundle;
		len = bundle_len;
	}

	char ext_buf[16];
	const char *ext = get_extension(path, ext_buf, sizeof(ext_buf));
	if(ext) {
		wprint(g_stdout, "extension: .%s\n", ext);
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
		wprint(g_stderr, "no registered player recognised this file\n");
		free(base_dir);
		free(data);
		return 1;
	}
	wprint(g_stdout, "player: %s\n", api->name);

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

	wprint(g_stdout, "playing (ctrl+c to exit)\n");

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
		wprint(g_stdout, "crossfeed: %d%% (each ear: %d%% own, %d%% other)\n",
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

	wprint(g_stdout, "\nshutting down\n");
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
	wprint(g_stderr, "usage: %s <file>\n", progname);
	wprint(g_stderr, "registered players:\n");
	for(int32_t i = 0; g_players[i]; ++i) {
		wprint(g_stderr, "  %s (", g_players[i]->name);
		for(const char **e = g_players[i]->extensions; *e; ++e) {
			wprint(g_stderr, "%s.%s", (e == g_players[i]->extensions) ? "" : " ", *e);
		}
		wprint(g_stderr, ")\n");
	}
}

// [=]===^=[ parse_cmdline ]======================================================================[=]
// Tokenise a Win32 command line into argc/argv with simple quote handling
// (a single pair of double-quotes around each argument). Sufficient for
// "test_player.exe path/to/file.mod" with paths containing spaces; not a full
// implementation of CommandLineToArgvW's escape rules.
static int32_t parse_cmdline(char *cmdline, char **argv, int32_t max_args) {
	int32_t argc = 0;
	char *p = cmdline;
	while(*p && argc < max_args) {
		while(*p == ' ' || *p == '\t') {
			++p;
		}
		if(*p == 0) {
			break;
		}
		char *start;
		if(*p == '"') {
			++p;
			start = p;
			while(*p && *p != '"') {
				++p;
			}
		} else {
			start = p;
			while(*p && *p != ' ' && *p != '\t') {
				++p;
			}
		}
		if(*p) {
			*p = 0;
			++p;
		}
		argv[argc++] = start;
	}
	return argc;
}

// [=]===^=[ mainCRTStartup ]=====================================================================[=]
// Custom entry point. mingw-w64's default mainCRTStartup pulls in UCRT init
// (api-ms-win-crt-*.dll), which Windows 7 doesn't ship. Replacing it gives
// us a binary that depends only on kernel32.dll, user32.dll, and winmm.dll.
void __stdcall mainCRTStartup(void) {
	g_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	g_stderr = GetStdHandle(STD_ERROR_HANDLE);

	SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

	char *argv[8];
	char *cmdline = GetCommandLineA();
	int32_t argc = parse_cmdline(cmdline, argv, 8);

	int32_t rc;
	if(argc < 2) {
		print_usage(argc > 0 ? argv[0] : "test_player.exe");
		rc = 1;
	} else {
		rc = run_player(argv[1]);
	}
	ExitProcess((UINT)rc);
}
