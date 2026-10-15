#!/bin/sh
# Host regression for the RAM-disk backend used on the target.
# The emulator core is compiled with -Dopen=emu_disk_open and friends (the same
# redirection sdk_probe/src/sf8086_core.c applies) and must still boot FreeDOS.
set -e
cd "$(dirname "$0")/../.."
root=$(pwd)
work=$(mktemp -d)

CFLAGS="-I$root/port -std=c99 -O2 -fsigned-char"
REDIRECT="-Dopen=emu_disk_open -Dclose=emu_disk_close -Dlseek=emu_disk_lseek \
          -Dread=emu_disk_read -Dwrite=emu_disk_write"

cc -c "$root/third_party/8086tiny/8086tiny.c" -o "$work/core.o" $CFLAGS \
  -DSF8086_SERIAL_PORT -DNO_GRAPHICS -DSF8086_PSRAM_MEM -Dmain=sf8086_main \
  $REDIRECT
cc -c "$root/port/emu_disk.c" -o "$work/disk.o" $CFLAGS
cc -c "$root/tools/host_bench/console_bench.c" -o "$work/console.o" $CFLAGS
cc -c "$root/tools/host_bench/ramdisk_main.c" -o "$work/main.o" $CFLAGS
cc "$work/core.o" "$work/disk.o" "$work/console.o" "$work/main.o" -o "$work/ramdisk-smoke"

# main() loads the images from the current directory.
cd "$root/third_party/8086tiny"
if "$work/ramdisk-smoke" </dev/null >/dev/null; then
  echo "ramdisk-smoke: PASS"
else
  echo "ramdisk-smoke: FAIL"
  exit 1
fi
