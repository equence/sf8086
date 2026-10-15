#!/bin/sh
# Copyright (C) 2026 equence <equence@qq.com>
# Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
# Keyboard regression: what the guest receives for the key sequences a terminal
# sends.  Runs KEYTEST.C (built with the floppy's own Turbo C) inside the host
# emulator and pins down the behaviour:
#
#   Ctrl+A then x  -> Alt+X (scancode 2D, ASCII 0, ALT flag)   [BIOS convention]
#   ESC then x     -> Alt+X as well                            [terminal Meta]
#   ESC alone      -> plain ESC key (scancode 01, ASCII 1B)
#   ESC [ A        -> up arrow (scancode 48), i.e. CSI is untouched
#
# Takes about two minutes.
set -e
cd "$(dirname "$0")/../.."
root=$(pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/sf8086-keys.XXXXXX")

SRC="$root/third_party/8086tiny"
EMU="$work/emu"
OUT="$work/console.out"
FIFO="$work/stdin"

cc "$SRC/8086tiny.c" "$root/port/emu_console_posix.c" -I"$root/port" \
  -DSF8086_SERIAL_PORT -DNO_GRAPHICS -O2 -fsigned-char -std=c99 -o "$EMU"

cp "$SRC/fd.img" "$work/fd-test.img"
mkfifo "$FIFO"
"$EMU" "$SRC/bios" "$work/fd-test.img" < "$FIFO" > "$OUT" 2>&1 &
emu_pid=$!
exec 3>"$FIFO"

sleep 8
printf 'cc tc\\samples\\keytest.c\r' >&3; sleep 22
printf 'keytest\r' >&3;                sleep 12
printf '\001x' >&3; sleep 4            # Ctrl+A then x
printf '\033x' >&3; sleep 4            # ESC then x (terminal Meta)
printf '\033'  >&3; sleep 1            # lone ESC (filter has to time out)
printf 'z'     >&3; sleep 4
printf '\033[A' >&3; sleep 4           # up arrow
printf '\003'  >&3; sleep 6            # quit

exec 3>&-
kill "$emu_pid" 2>/dev/null || true
wait "$emu_pid" 2>/dev/null || true

sed -e 's/\r//g' < "$OUT" > "$work/clean.txt"

status=0
expect_count() {
  count=$(grep -c -- "$1" "$work/clean.txt" || true)
  if [ "$count" -ge "$2" ]; then
    echo "  ok: \"$1\" x$count"
  else
    echo "  MISSING: \"$1\" (found $count, want >= $2)"
    status=1
  fi
}

echo "keys-smoke checks:"
expect_count 'scancode 2D  ascii 00' 2   # Ctrl+A x and ESC x both yield Alt+X
expect_count 'scancode 01  ascii 1B' 1   # lone ESC still arrives
expect_count 'scancode 48  ascii 00' 1   # up arrow via ESC [ A
expect_count 'scancode 2C  ascii 7A' 1   # plain letter 'z'
expect_count 'KEYTEST: done' 1

if [ "$status" -eq 0 ]; then
  echo "keys-smoke: PASS"
else
  echo "keys-smoke: FAIL (see $work/clean.txt)"
fi
exit "$status"
