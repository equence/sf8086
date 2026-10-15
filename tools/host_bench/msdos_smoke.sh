#!/bin/sh
# Copyright (C) 2026 equence <equence@qq.com>
# Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
# Build a board floppy from a local MS-DOS 6.22 image and boot it in the host
# emulator, then compile and run the samples in MS-DOS.
#
# MS-DOS is not redistributable, so the source image is not in this repository:
# put your own copy at tools/floppy/local/dos622.img (git-ignored) or point
# DOS_IMAGE at it.  The script skips itself when that file is absent.
#
# Takes about two minutes.
set -e
cd "$(dirname "$0")/../.."
root=$(pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/sf8086-msdos.XXXXXX")

source_image="${DOS_IMAGE:-tools/floppy/local/dos622.img}"
if [ ! -f "$source_image" ]; then
  echo "msdos-smoke: SKIP (no MS-DOS image at $source_image)"
  exit 0
fi

SRC="$root/third_party/8086tiny"
EMU="$work/emu"
OUT="$work/console.out"
FIFO="$work/stdin"
IMAGE="$work/msdos.img"

echo "== building the image from $source_image =="
OUT_IMAGE="$IMAGE" sh tools/floppy/build_msdos.sh > "$work/build.log" 2>&1
tail -3 "$work/build.log" | sed 's/^/   /'

cc "$SRC/8086tiny.c" "$root/port/emu_console_posix.c" -I"$root/port" \
  -DSF8086_SERIAL_PORT -DNO_GRAPHICS -O2 -fsigned-char -std=c99 -o "$EMU"

mkfifo "$FIFO"
"$EMU" "$SRC/bios" "$IMAGE" < "$FIFO" > "$OUT" 2>&1 &
emu_pid=$!
exec 3>"$FIFO"

sleep 8
printf 'ver\r' >&3;                     sleep 4
printf 'cc tc\\samples\\hello.c\r' >&3; sleep 20
printf 'hello\r' >&3;                   sleep 8
printf 'mem\r' >&3;                     sleep 6

exec 3>&-
kill "$emu_pid" 2>/dev/null || true
wait "$emu_pid" 2>/dev/null || true

sed -e 's/\r//g' < "$OUT" > "$work/clean.txt"

status=0
check() {
  if grep -q -- "$1" "$work/clean.txt"; then
    echo "  ok: $1"
  else
    echo "  MISSING: $1"
    status=1
  fi
}

echo "msdos-smoke checks:"
check 'MS-DOS Version 6.22'
check 'Turbo C  Version 2.01'
check 'Turbo Link  Version 2.0'
check 'Hello from the SF32LB525.'
check 'Conventional         640K'

if [ "$status" -eq 0 ]; then
  echo "msdos-smoke: PASS"
else
  echo "msdos-smoke: FAIL (see $work/clean.txt)"
fi
exit "$status"
