#!/bin/sh
# Copyright (C) 2026 equence <equence@qq.com>
# Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
# Host regression: the POSIX console backend behaves like a UART byte stream.
# Expected evidence: FreeDOS kernel banner appears without relying on direct
# write(1,...) inside the emulator core.
#
# The emulator has no exit path (the guest runs forever), so the run is bounded
# by a timer instead of relying on EOF.
set -e
cd "$(dirname "$0")"
cc 8086tiny.c ../../port/emu_console_posix.c \
  -I../../port -DSF8086_SERIAL_PORT -DNO_GRAPHICS \
  -O2 -fsigned-char -std=c99 -o /tmp/sf8086-serial-smoke

/tmp/sf8086-serial-smoke bios fd.img </dev/null > /tmp/sf8086-serial-smoke.out 2>&1 &
smoke_pid=$!
sleep 15
kill "$smoke_pid" 2>/dev/null || true
wait "$smoke_pid" 2>/dev/null || true

if grep -q 'FreeDOS kernel' /tmp/sf8086-serial-smoke.out; then
  echo 'serial-smoke: PASS'
else
  echo 'serial-smoke: FAIL (no FreeDOS kernel banner)'
  exit 1
fi
