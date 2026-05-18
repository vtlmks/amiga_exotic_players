#!/bin/bash
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT

set -e

CC=gcc
# -DPAULA_PROFILE compiles in the mixer CPU-time probe (negligible: two vDSO
# clock reads per ALSA period). The realtime-factor report is printed at exit
# only when the PAULA_PROFILE environment variable is also set at run time.
CFLAGS="-std=gnu99 -O2 -march=x86-64-v2 -ffast-math -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -fwrapv -DPAULA_PROFILE"
LDFLAGS="-lasound -lpthread -lm"

$CC $CFLAGS test_player.c -o test_player $LDFLAGS
echo "built: test_player"
