#!/bin/bash
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT

set -e

CC=gcc
CFLAGS="-std=gnu99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -fwrapv"
LDFLAGS="-lasound -lpthread -lm"

$CC $CFLAGS test_player.c -o test_player $LDFLAGS
echo "built: test_player"
