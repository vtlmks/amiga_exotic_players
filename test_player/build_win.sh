#!/bin/bash
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT
#
# Cross-compile test_player_win.c for Windows using mingw-w64.
# Defaults to 64-bit; pass `32` as first arg for 32-bit.
#
# CRT-free build: -nostartfiles -nodefaultlibs avoids pulling in mingw's
# UCRT-linked startup. We provide our own entry point (mainCRTStartup),
# malloc/memset/sin/etc in the source, and link only against the Win32
# import libs (kernel32, user32, winmm) plus libgcc for compiler helpers
# like __chkstk_ms / __divdi3 / __udivdi3.

set -e

ARCH="${1:-64}"
case "$ARCH" in
	64) CC=x86_64-w64-mingw32-gcc ;;
	32) CC=i686-w64-mingw32-gcc ;;
	*)  echo "usage: $0 [64|32]"; exit 1 ;;
esac

CFLAGS="-std=gnu99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -fwrapv"
# -ffreestanding tells gcc not to assume a hosted environment; safer for our
# replacement-libc scheme. -fno-builtin prevents gcc from reasoning about
# memcpy/memset semantics in a way that calls back into them recursively.
CFLAGS="$CFLAGS -ffreestanding -fno-builtin"
LDFLAGS="-nostartfiles -nodefaultlibs -Wl,--entry,mainCRTStartup -lgcc -lkernel32 -luser32 -lwinmm"

$CC $CFLAGS test_player_win.c -o test_player.exe $LDFLAGS
echo "built: test_player.exe ($ARCH-bit)"
