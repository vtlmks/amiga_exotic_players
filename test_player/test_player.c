// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Command-line test harness. Iterates the registered player list, prefers
// players whose extension matches the file name, falls back to content-based
// identification via each player's init() function.
// Usage: ./test_player <file>
// Ctrl+C to exit.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <alsa/asoundlib.h>

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
#define FRAMES_PER_PERIOD 256
#define PERIOD_COUNT      2

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

static volatile int32_t g_running = 1;

// [=]===^=[ on_sigint ]==========================================================================[=]
static void on_sigint(int sig) {
	(void)sig;
	g_running = 0;
}

// [=]===^=[ load_file ]==========================================================================[=]
static uint8_t *load_file(const char *path, uint32_t *out_len) {
	FILE *f = fopen(path, "rb");
	if(!f) {
		return 0;
	}
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	if(len <= 0) {
		fclose(f);
		return 0;
	}
	fseek(f, 0, SEEK_SET);
	uint8_t *buf = (uint8_t *)malloc((size_t)len);
	if(!buf) {
		fclose(f);
		return 0;
	}
	if(fread(buf, 1, (size_t)len, f) != (size_t)len) {
		free(buf);
		fclose(f);
		return 0;
	}
	fclose(f);
	*out_len = (uint32_t)len;
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

// [=]===^=[ try_tfmx_bundle ]====================================================================[=]
// If `path` is a TFMX `mdat.<name>` (case-insensitive), look for the matching
// `smpl.<name>` sibling in the same directory, build a TFHD bundle in memory,
// and return the bundle (caller must free). On any miss, returns 0 and the
// caller falls back to single-file dispatch.
static uint8_t *try_tfmx_bundle(const char *path, uint8_t *mdat_data, uint32_t mdat_len, uint32_t *out_len) {
	const char *base = basename_of(path);
	size_t blen = strlen(base);
	if(blen < 6) {
		return 0;
	}
	int32_t is_mdat = ((tolower((unsigned char)base[0]) == 'm') &&
	                   (tolower((unsigned char)base[1]) == 'd') &&
	                   (tolower((unsigned char)base[2]) == 'a') &&
	                   (tolower((unsigned char)base[3]) == 't') &&
	                   (base[4] == '.'));
	if(!is_mdat) {
		return 0;
	}

	// Build sibling path with prefix replaced.
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

	// Build TFHD bundle. Header layout (matches tfmx_detect_single_file):
	//   bytes  0..3  "TFHD"
	//   bytes  4..7  header offset (BE u32; bytes 4..5 must be 0)
	//   byte   8     version_hint (0)
	//   byte   9     pad
	//   bytes 10..13 mdat_size (BE u32)
	//   bytes 14..17 smpl_size (BE u32)
	//   bytes 18..   mdat data, then smpl data.
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
// Returns a pointer to the lowercase extension (without dot), or 0. Writes up to
// `dst_size` chars into `dst`, including null terminator. Works with UNIX or Windows paths.
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
		dst[i] = (char)tolower((unsigned char)*p);
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
// Filesystem-backed implementation of player_loader.fetch. Resolves `name`
// relative to the directory of the input file, supporting subpaths like
// "Instruments/Bass6.instr" used by IFF SMUS modules. Returns a malloc'd
// buffer + size on success, or 0 on miss / I/O error.
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
// Prefers init_ex when the player exposes one (so external-file loading via
// the loader callback can succeed); falls back to plain init otherwise.
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
// Picks the first player whose init() accepts the buffer. Tries extension-matched
// players first, then all others.
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

// [=]===^=[ open_alsa ]==========================================================================[=]
static snd_pcm_t *open_alsa(void) {
	snd_pcm_t *pcm = 0;
	int32_t err;

	const char *names[] = { "default", "pulse", "plughw:0,0", 0 };
	for(int32_t i = 0; names[i]; ++i) {
		err = snd_pcm_open(&pcm, names[i], SND_PCM_STREAM_PLAYBACK, 0);
		if(err >= 0) {
			fprintf(stdout, "alsa device: %s\n", names[i]);
			break;
		}
		fprintf(stderr, "snd_pcm_open(%s): %s\n", names[i], snd_strerror(err));
		pcm = 0;
	}
	if(!pcm) {
		return 0;
	}

	err = snd_pcm_set_params(pcm,
	    SND_PCM_FORMAT_S16_LE,
	    SND_PCM_ACCESS_RW_INTERLEAVED,
	    NUM_CHANNELS,
	    SAMPLE_RATE,
	    1,
	    100 * 1000);
	if(err < 0) {
		fprintf(stderr, "snd_pcm_set_params: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 0;
	}
	return pcm;
}

// [=]===^=[ main ]===============================================================================[=]
int main(int argc, char **argv) {
	if(argc < 2) {
		fprintf(stderr, "usage: %s <file>\n", argv[0]);
		fprintf(stderr, "registered players:\n");
		for(int32_t i = 0; g_players[i]; ++i) {
			fprintf(stderr, "  %s (", g_players[i]->name);
			for(const char **e = g_players[i]->extensions; *e; ++e) {
				fprintf(stderr, "%s.%s", (e == g_players[i]->extensions) ? "" : " ", *e);
			}
			fprintf(stderr, ")\n");
		}
		return 1;
	}

	uint32_t len = 0;
	uint8_t *data = load_file(argv[1], &len);
	if(!data) {
		fprintf(stderr, "failed to load: %s\n", argv[1]);
		return 1;
	}
	fprintf(stdout, "loaded %u bytes\n", len);

	// TFMX modules typically come as mdat.<name> + smpl.<name>; merge into
	// a TFHD bundle the player can identify.
	uint32_t bundle_len = 0;
	uint8_t *bundle = try_tfmx_bundle(argv[1], data, len, &bundle_len);
	if(bundle) {
		fprintf(stdout, "bundled mdat.+smpl. into %u bytes\n", bundle_len);
		free(data);
		data = bundle;
		len = bundle_len;
	}

	char ext_buf[16];
	const char *ext = get_extension(argv[1], ext_buf, sizeof(ext_buf));
	if(ext) {
		fprintf(stdout, "extension: .%s\n", ext);
	}

	// Build a filesystem loader rooted at the input file's directory so
	// players that need companion files (IFF SMUS Instruments/, FaceTheMusic
	// external samples) can fetch them by name.
	char *base_dir = 0;
	{
		const char *base = basename_of(argv[1]);
		size_t prefix_len = (size_t)(base - argv[1]);
		if(prefix_len == 0) {
			base_dir = strdup(".");
		} else {
			base_dir = (char *)malloc(prefix_len + 1);
			if(base_dir) {
				memcpy(base_dir, argv[1], prefix_len);
				// Strip trailing '/'.
				if(prefix_len > 1 && base_dir[prefix_len - 1] == '/') {
					base_dir[prefix_len - 1] = 0;
				} else {
					base_dir[prefix_len] = 0;
				}
			}
		}
	}
	struct player_loader fs_loader = {
		.ctx = base_dir,
		.fetch = fs_loader_fetch,
	};

	struct player_api *api = 0;
	void *state = dispatch(data, len, ext, &api, &fs_loader);
	if(!state) {
		fprintf(stderr, "no registered player recognised this file\n");
		free(base_dir);
		free(data);
		return 1;
	}
	fprintf(stdout, "player: %s\n", api->name);

	snd_pcm_t *pcm = open_alsa();
	if(!pcm) {
		fprintf(stderr, "failed to open audio device\n");
		api->free(state);
		free(data);
		free(base_dir);
		return 1;
	}

	signal(SIGINT, on_sigint);

	int16_t *buffer = (int16_t *)calloc(FRAMES_PER_PERIOD * NUM_CHANNELS, sizeof(int16_t));
	fprintf(stdout, "playing (ctrl+c to exit)\n");

	// Headphone crossfeed: hard-panned Amiga stereo is brutal in headphones.
	// We reduce L/R separation by mixing a fraction of each channel into the
	// other before output. CROSSFEED env var controls the fraction in
	// percent (0..50, default 40). 0 = pure stereo passthrough; 50 = mono.
	// At 40 each ear keeps 60% of its own signal and picks up 40% of the other.
	int32_t xf_q15;       // crossfeed amount as Q15 fixed-point (0..16384)
	int32_t self_q15;     // self-amount = 32768 - 2*xf_q15  (so L+R sum is preserved)
	{
		int32_t xf_pct = 40;
		const char *env = getenv("CROSSFEED");
		if(env && *env) {
			xf_pct = atoi(env);
			if(xf_pct < 0) {
				xf_pct = 0;
			}
			if(xf_pct > 50) {
				xf_pct = 50;
			}
		}
		xf_q15 = (xf_pct * 32768 + 50) / 100;
		self_q15 = 32768 - xf_q15;
		fprintf(stdout, "crossfeed: %d%% (each ear: %d%% own, %d%% other)\n",
			xf_pct, 100 - xf_pct, xf_pct);
	}

	while(g_running) {
		memset(buffer, 0, FRAMES_PER_PERIOD * NUM_CHANNELS * sizeof(int16_t));
		api->get_audio(state, buffer, FRAMES_PER_PERIOD);

		if(xf_q15 != 0) {
			for(int32_t i = 0; i < FRAMES_PER_PERIOD; ++i) {
				int32_t l = buffer[i * 2 + 0];
				int32_t r = buffer[i * 2 + 1];
				int32_t nl = (l * self_q15 + r * xf_q15) >> 15;
				int32_t nr = (r * self_q15 + l * xf_q15) >> 15;
				if(nl > 32767) {
					nl = 32767;
				} else if(nl < -32768) {
					nl = -32768;
				}
				if(nr > 32767) {
					nr = 32767;
				} else if(nr < -32768) {
					nr = -32768;
				}
				buffer[i * 2 + 0] = (int16_t)nl;
				buffer[i * 2 + 1] = (int16_t)nr;
			}
		}

		snd_pcm_sframes_t written = snd_pcm_writei(pcm, buffer, FRAMES_PER_PERIOD);
		if(written < 0) {
			if(snd_pcm_recover(pcm, (int)written, 1) < 0) {
				fprintf(stderr, "alsa unrecoverable\n");
				break;
			}
		}
	}

	fprintf(stdout, "\nshutting down\n");
	snd_pcm_drop(pcm);
	snd_pcm_close(pcm);
	free(buffer);
	api->free(state);
	free(data);
	free(base_dir);
	return 0;
}
