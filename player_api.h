// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Common interface every ported replayer exports. Each player's header declares
// a `struct player_api <name>_api` global that the test player (or any host)
// can iterate to auto-detect the right replayer for a file.

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
	void  (*get_audio)(void *state, int16_t *output, int32_t frames);
	// Optional: when non-null and the host has a loader for sibling files,
	// the host should prefer this entry point. Players that don't need
	// external files leave this null and the host falls back to init().
	void *(*init_ex)(void *data, uint32_t len, int32_t sample_rate, struct player_loader *loader);
};
