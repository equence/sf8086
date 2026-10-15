#!/bin/sh
# End-to-end check of the DOS boot floppy: boot it in the host emulator, compile
# the sample programs with the bundled Turbo C 2.01, run them, and look for the
# expected output.  This exercises the same image the firmware embeds.
#
# The emulator writes guest files straight into the image it is given, so the
# test runs against a throwaway copy and never touches third_party/8086tiny/fd.img.
#
# Takes about two minutes: the guest is an 8086 running at roughly a million
# instructions per second per instruction-counting emulator... i.e. a C compiler
# from 1989 doing real work in a 640 KB machine.
set -e
cd "$(dirname "$0")/../.."
root=$(pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/sf8086-floppy.XXXXXX")

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

send() { printf '%s\r' "$1" >&3; }

sleep 8                       # FreeDOS boots to A:\>
send 'cc tc\samples\hello.c'; sleep 20
send 'hello';                 sleep 8
send 'cc tc\samples\sieve.c'; sleep 25
send 'sieve';                 sleep 25
send 'cc tc\samples\fileio.c'; sleep 25
send 'fileio';                sleep 15

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

echo "floppy-smoke checks:"
check 'Turbo C  Version 2.01'
check 'Turbo Link  Version 2.0'
check 'Hello from the SF32LB525.'
check 'primes below 2000'
check 'line 5 written by FILEIO.EXE'

if [ "$status" -eq 0 ]; then
  echo "floppy-smoke: PASS"
else
  echo "floppy-smoke: FAIL (see $work/clean.txt)"
fi
exit "$status"
