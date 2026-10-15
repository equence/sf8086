#!/bin/sh
# Fetch the Turbo C 2.01 pieces used on the floppy and stage them in payload/.
#
# Turbo C 2.01 (Borland, 1989) is the practical C toolchain for an 8086-class
# machine: its compiler and linker are 16-bit real mode, so they run under
# 8086tiny, and it needs no 386 protected mode or DPMI host.  See payload/README.md
# for the licensing note - the files are staged here for a private build.
#
# The archive is cached in the work directory, so re-running is cheap.
set -e
cd "$(dirname "$0")"

URL="https://archive.org/download/tc201_202505/tc201.zip"
CACHE_DIR="${TMPDIR:-/tmp}/sf8086-tc201"
ZIP="$CACHE_DIR/tc201.zip"
PAYLOAD="payload/tc201"

mkdir -p "$CACHE_DIR" "$PAYLOAD/INCLUDE" "$PAYLOAD/LIB" "$PAYLOAD/BIN"

if [ ! -f "$ZIP" ]; then
  echo "downloading $URL"
  curl -L --http1.1 --retry 3 --retry-delay 2 --max-time 300 -o "$ZIP" "$URL"
fi

EXTRACT="$CACHE_DIR/extract"
mkdir -p "$EXTRACT"
unzip -q -o "$ZIP" -d "$EXTRACT"

DISK2="$EXTRACT/Disk2"
DISK3="$EXTRACT/Disk3"
if [ ! -f "$DISK2/TCC.EXE" ] || [ ! -f "$DISK3/CS.LIB" ]; then
  echo "unexpected archive layout: expected Disk2/TCC.EXE and Disk3/CS.LIB" >&2
  exit 1
fi

# Compiler driver and linker.
cp "$DISK2/TCC.EXE" "$DISK2/TLINK.EXE" "$PAYLOAD/BIN/"

# Headers (the whole set is small) and the small model library.
cp "$DISK3"/*.H "$PAYLOAD/INCLUDE/"
cp "$DISK3/C0S.OBJ" "$DISK3/CS.LIB" "$DISK3/MATHS.LIB" "$DISK3/EMU.LIB" "$PAYLOAD/LIB/"

echo "payload staged in $PAYLOAD:"
du -sk "$PAYLOAD"/* | sed 's/^/  /'
