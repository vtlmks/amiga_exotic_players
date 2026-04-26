#!/bin/bash
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT
#
# Cross-compile test_player_win.c for Windows using mingw-w64.
# Defaults to 64-bit; pass `32` as first arg for 32-bit.
#
# Links against msvcrt.dll (legacy C runtime, present in System32 on every
# Windows since 95), kernel32, user32, and winmm. Standard C startup; no
# installed runtime required.

set -e

ARCH="${1:-64}"
case "$ARCH" in
	64) CC=x86_64-w64-mingw32-gcc;       WINDRES=x86_64-w64-mingw32-windres ;;
	32) CC=i686-w64-mingw32-gcc;         WINDRES=i686-w64-mingw32-windres ;;
	*)  echo "usage: $0 [64|32]"; exit 1 ;;
esac

CFLAGS="-std=gnu99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -fwrapv"

# Force linkage against legacy msvcrt.dll (in System32 since Win95/NT4) rather
# than UCRT (api-ms-win-crt-*.dll). UCRT requires KB2999226 on Win7 and ships
# only with Win10+; msvcrt.dll has been a system DLL on every Windows for 30
# years.
CFLAGS="$CFLAGS -mcrtdll=msvcrt-os"

# DLLCHARACTERISTICS bits (ASLR / DEP / NX). Their absence contributes to AV
# heuristic scoring; setting them costs nothing.
LDFLAGS_HARDENING="-Wl,--nxcompat -Wl,--dynamicbase"
if [ "$ARCH" = "64" ]; then
	LDFLAGS_HARDENING="$LDFLAGS_HARDENING -Wl,--high-entropy-va"
fi

LDFLAGS="$LDFLAGS_HARDENING -lwinmm"

# Compile the resource (VERSIONINFO + embedded application manifest).
$WINDRES test_player.rc -O coff -o test_player.res.o

$CC $CFLAGS test_player_win.c test_player.res.o -o test_player.exe $LDFLAGS
rm -f test_player.res.o
echo "built: test_player.exe ($ARCH-bit)"
