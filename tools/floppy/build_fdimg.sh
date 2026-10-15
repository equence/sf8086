#!/bin/sh
# Copyright (C) 2026 equence <equence@qq.com>
# Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
# Build the DOS boot floppy that ships inside the firmware.
#
# By default it starts from the vendored FreeDOS image and applies the recipe:
#   1. prune what cannot run on an 8086 or cannot be used on this board
#   2. add Turbo C 2.01 (small model) for C development
#   3. add a short README and the sample programs
#
# A different DOS, including MS-DOS, can be used instead by pointing DOS_IMAGE at
# a bootable 1.44 MB floppy image.  That image is copied first and then edited, so
# the original is never touched.
#
# Usage (from the repository root):
#   sh tools/floppy/build_fdimg.sh
#   DOS_IMAGE=/path/to/msdos622.img sh tools/floppy/build_fdimg.sh
#   DOS_IMAGE=... WITHOUT_TOOLS=1 sh tools/floppy/build_fdimg.sh
#   KEEP_HELP=1 sh tools/floppy/build_fdimg.sh        (keep the FreeDOS help data)
#
# The result always keeps the 1.44 MB geometry: the emulated BIOS and both DOSes
# depend on it.
set -e

cd "$(dirname "$0")/../.."
root=$(pwd)
fat12="python3 $root/tools/floppy/fat12.py"
default_base="$root/third_party/8086tiny/fd-base.img"
base="${DOS_IMAGE:-$default_base}"
image="$root/third_party/8086tiny/fd.img"
payload="$root/tools/floppy/payload/tc201"

# Pruning is specific to the FreeDOS image; a supplied image is left alone unless
# PRUNE=1 is set explicitly.
prune=0
[ -z "${DOS_IMAGE:-}" ] && prune=1
[ "${PRUNE:-}" = "1" ] && prune=1

# ---------------------------------------------------------------------------
# 0. Check the image we start from
# ---------------------------------------------------------------------------
if [ ! -f "$base" ]; then
  echo "missing base image: $base" >&2
  exit 1
fi

python3 "$root/tools/floppy/check_image.py" "$base"

cp "$base" "$image"
echo
$fat12 "$image" free

if [ "$prune" = "1" ]; then
  # -------------------------------------------------------------------------
  # 1. Prune (FreeDOS image only)
  # -------------------------------------------------------------------------
  # Programs that need a 286/386, protected mode or a DPMI host: they cannot run
  # under 8086tiny and only mislead anyone who tries.  Everything the boot
  # configuration loads is already commented out, so removing them changes nothing.
  DEAD_ON_8086="
FREEDOS/BIN/CWSDPMI.EXE
FREEDOS/BIN/DOSFSCK.EXE
FREEDOS/BIN/EMM386.EXE
FREEDOS/BIN/HIMEM.EXE
FREEDOS/BIN/FDXMS.SYS
FREEDOS/BIN/FDXMS286.SYS
FREEDOS/BIN/FDXXMS.SYS
FREEDOS/BIN/LFNXLAT.386
"

  # CD-ROM / SCSI / ISO tooling and printer tooling: 8086tiny has neither.
  NO_HARDWARE="
FREEDOS/BIN/SHSUCDHD.EXE
FREEDOS/BIN/SHSUCDRD.EXE
FREEDOS/BIN/SHSUCDRI.EXE
FREEDOS/BIN/SHSUCDX.COM
FREEDOS/BIN/SHSUDVHD.EXE
FREEDOS/BIN/SHSURDRV.EXE
FREEDOS/BIN/SHSUFDRV.EXE
FREEDOS/BIN/XCDROM.SYS
FREEDOS/BIN/XDMA.SYS
FREEDOS/BIN/CDRCACHE.SYS
FREEDOS/BIN/CD2ISO86.EXE
FREEDOS/BIN/ISOBAR.EXE
FREEDOS/BIN/OMI.EXE
FREEDOS/BIN/PRINT.COM
FREEDOS/BIN/PRINTQ.EXE
FREEDOS/BIN/GRAPHICS.BAT
FREEDOS/BIN/GRAPH-HP.COM
FREEDOS/BIN/GRAPH-PS.COM
FREEDOS/BIN/GRAPHPIN.COM
FREEDOS/BIN/CTMOUSE.EXE
FREEDOS/BIN/MVALUES.EXE
FREEDOS/BIN/PCISLEEP.COM
FREEDOS/BIN/LOWDMA.COM
FREEDOS/BIN/LOWDMA.SYS
FREEDOS/BIN/FMLFN.DRV
FREEDOS/BIN/DRVON.COM
FREEDOS/BIN/TICKLE.COM
FREEDOS/BIN/DOSLFNMS.COM
FREEDOS/BIN/FDCOM.ICO
APPINFO.TGZ
"

  # The FreeDOS help database and its viewers.  The documentation lives in this
  # repository instead; on an 8086 guest the 230 KB buys more as C workspace.
  echo "== pruning =="
  for path in $DEAD_ON_8086 $NO_HARDWARE; do
    $fat12 "$image" del "$path" 2>/dev/null || echo "  (absent) $path"
  done
  if [ "$KEEP_HELP" != "1" ]; then
    $fat12 "$image" del "FREEDOS/HELP/EN/HHSTNDRD/HHSTNDRD.ZIP" 2>/dev/null \
      || echo "  (absent) help database"
    for path in FREEDOS/HELP/EN/INDEX.HTM FREEDOS/HELP/EN/HELP.HTM \
                FREEDOS/HELP/EN/HHSTNDRD/H2CPYING.TXT \
                FREEDOS/HELP/EN/HHSTNDRD/README.1ST \
                FREEDOS/HELP/INDEX.HTM FREEDOS/HELP/FDL.TXT \
                FREEDOS/BIN/HELP.EXE FREEDOS/BIN/PG.EXE FREEDOS/BIN/PG.MAN; do
      $fat12 "$image" del "$path" 2>/dev/null || echo "  (absent) $path"
    done
  fi
  $fat12 "$image" free
else
  echo "== pruning skipped (image supplied through DOS_IMAGE) =="
fi

# ---------------------------------------------------------------------------
# 2. Tools, if they fit
# ---------------------------------------------------------------------------
if [ "${WITHOUT_TOOLS:-}" = "1" ]; then
  echo
  echo "== WITHOUT_TOOLS=1: image used as it is =="
else
  need=$(python3 - "$payload" "$root/tools/floppy/dosfiles" <<'PY'
import os
import sys

total = 0
for directory in sys.argv[1:]:
    for folder, _dirs, files in os.walk(directory):
        for name in files:
            size = os.path.getsize(os.path.join(folder, name))
            total += (size + 511) // 512 * 512
print(total + 16 * 512)          # directory entries and slack
PY
)
  free=$(python3 - "$image" "$root" <<'PY'
import sys

sys.path.insert(0, sys.argv[2] + "/tools/floppy")
from fat12 import Fat12                                     # noqa: E402

fs = Fat12(sys.argv[1])
print(sum(1 for c in range(2, fs.cluster_count + 2) if fs.is_free(c)) * fs.bytes_per_sector)
PY
)
  echo
  echo "== toolchain needs $need B, image has $free B free =="

  if [ ! -f "$payload/BIN/TCC.EXE" ]; then
    echo "== Turbo C payload not staged: skipping the compiler =="
    echo "   run: sh tools/floppy/fetch_turbo_c.sh"
  elif [ "$free" -lt "$need" ]; then
    echo "== not enough room for the toolchain: skipping it =="
    echo "   the image works as it is; WITHOUT_TOOLS=1 suppresses this message"
  else
    echo "== adding Turbo C 2.01 (small model) =="
    $fat12 "$image" mkdir TC
    $fat12 "$image" mkdir TC/INCLUDE
    $fat12 "$image" mkdir TC/LIB
    for file in "$payload"/BIN/*; do
      $fat12 "$image" add "$file" TC
    done
    for file in "$payload"/INCLUDE/*; do
      $fat12 "$image" add "$file" TC/INCLUDE
    done
    for file in "$payload"/LIB/*; do
      $fat12 "$image" add "$file" TC/LIB
    done

    # -----------------------------------------------------------------------
    # 3. Samples and notes
    # -----------------------------------------------------------------------
    echo "== adding samples =="
    $fat12 "$image" mkdir TC/SAMPLES

    # DOS text files want CRLF endings; keep the repository copies as plain ASCII.
    stage=$(mktemp -d "${TMPDIR:-/tmp}/sf8086-dosfiles.XXXXXX")
    for file in "$root"/tools/floppy/dosfiles/*; do
      name=$(basename "$file")
      awk 'BEGIN { ORS = "\r\n" } { print }' "$file" > "$stage/$name"
    done

    for file in "$stage"/*; do
      case "$file" in
        *.C) $fat12 "$image" add "$file" TC/SAMPLES ;;
        *)   $fat12 "$image" add "$file" ;;
      esac
    done
  fi
fi

$fat12 "$image" check
echo
$fat12 "$image" free
echo
echo "built $image"

# ---------------------------------------------------------------------------
# 4. Tell the firmware build that the images changed
# ---------------------------------------------------------------------------
# disk_image.S pulls these files in with .incbin, which the SCons graph cannot
# see, so a rebuilt floppy would silently be left out of the firmware.  Stamping
# the checksums into the assembly source makes the change visible to the build
# system's content signatures.
stamp_file="$root/sdk_probe/src/disk_image.S"
fd_sum=$(md5 -q "$image" 2>/dev/null || md5sum "$image" | cut -d' ' -f1)
bios_sum=$(md5 -q "$root/third_party/8086tiny/bios" 2>/dev/null \
           || md5sum "$root/third_party/8086tiny/bios" | cut -d' ' -f1)
fd_size=$(wc -c < "$image" | tr -d ' ')

python3 - "$stamp_file" "$fd_sum" "$fd_size" "$bios_sum" <<'PY'
import re
import sys

path, fd_sum, fd_size, bios_sum = sys.argv[1:5]
line = (f" *   image stamp: fd.img {fd_sum} ({fd_size} bytes), "
        f"bios {bios_sum}\n")
text = open(path).read()
if re.search(r"^ \*   image stamp: .*$", text, re.M):
    text = re.sub(r"^ \*   image stamp: .*$", line.rstrip("\n"), text, flags=re.M)
else:
    marker = " * sdk_probe/project.\n"
    text = text.replace(marker, marker + line, 1)
open(path, "w").write(text)
print("stamped disk_image.S with the new image checksums")
PY
