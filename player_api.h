// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Common interface every ported replayer exports. Each player's header declares
// a `struct player_api <name>_api` global that the test player (or any host)
// can iterate to auto-detect the right replayer for a file.
//
// Audio output is interleaved float stereo, nominal range [-1.0, 1.0]. Players
// ACCUMULATE into the caller's buffer (caller must pre-clear). They do NOT
// clip; the host is responsible for any final saturation, dithering, or
// conversion to the audio backend's native sample format.

#pragma once

#include <stdint.h>

// Optional file-loader callback used by players that read companion files
// (IFF SMUS instrument files, Face The Music external samples, etc.). Hosts
// that pass a loader implement `fetch` to resolve a logical name (e.g.
// "Instruments/Bass6.instr") to a heap-allocated byte buffer + length. The
// player calls free() on the returned pointer when done.
struct player_loader {
	void *ctx;
	uint8_t *(*fetch)(void *ctx, const char *name, uint32_t *out_len);
};

struct player_api {
	const char *name;
	const char **extensions;   /* null-terminated list of lowercase extensions, no dot */
	void *(*init)(void *data, uint32_t len, int32_t sample_rate);
	void  (*free)(void *state);
	void  (*get_audio)(void *state, float *output, int32_t frames);
	// Optional: when non-null and the host has a loader for sibling files,
	// the host should prefer this entry point. Players that don't need
	// external files leave this null and the host falls back to init().
	void *(*init_ex)(void *data, uint32_t len, int32_t sample_rate, struct player_loader *loader);
};

// [=]===^=[ player_get_audio_s16 ]===============================================================[=]
// Convenience wrapper for hosts that want signed-16 PCM out. Drives the
// player's float get_audio into the caller-supplied scratch buffer (must hold
// at least frames * 2 floats), clears it first, then converts with hard
// saturation into `output` (frames * 2 int16 stereo samples). The scratch is
// caller-owned so the hot path never allocates; reuse the same buffer across
// calls.
#include <string.h>
static void player_get_audio_s16(struct player_api *api, void *state, int16_t *output, float *scratch, int32_t frames) {
	int32_t samples = frames * 2;
	memset(scratch, 0, (size_t)samples * sizeof(float));
	api->get_audio(state, scratch, frames);
	for(int32_t i = 0; i < samples; ++i) {
		float v = scratch[i] * 32767.0f;
		if(v >  32767.0f) {
			v =  32767.0f;
		}
		if(v < -32768.0f) {
			v = -32768.0f;
		}
		output[i] = (int16_t)v;
	}
}
