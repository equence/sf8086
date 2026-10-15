#!/bin/sh
# Copyright (C) 2026 equence <equence@qq.com>
# Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
# Host benchmark: how many guest instructions does a FreeDOS boot need, and how
# fast does the host execute them?
set -e
cd "$(dirname "$0")/../.."

SRC=third_party/8086tiny
BIN=${TMPDIR:-/tmp}/sf8086-host-bench

cc "$SRC/8086tiny.c" tools/host_bench/console_bench.c \
  -Iport -DSF8086_SERIAL_PORT -DNO_GRAPHICS \
  -O2 -fsigned-char -std=c99 -o "$BIN"

start=$(date +%s.%N)
"$BIN" "$SRC/bios" "$SRC/fd.img" </dev/null >/dev/null
end=$(date +%s.%N)

awk -v s="$start" -v e="$end" 'BEGIN { printf "bench: wall clock %.2f s\n", e - s }'
