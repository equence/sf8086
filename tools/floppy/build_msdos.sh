#!/bin/sh
# Build a board floppy from an MS-DOS 6.22 boot disk.
#
# The stock 6.22 rescue disk boots into a CD-ROM driver menu and loads HIMEM,
# which an 8086 guest cannot use.  This script replaces the boot configuration
# with a plain one, drops the CD-ROM/SCSI driver trees, keeps the DOS utilities,
# and adds an editor plus the Turbo C toolchain.
#
# Usage (from the repository root):
#   DOS_IMAGE=/path/to/dos622.img sh tools/floppy/build_msdos.sh
#   DOS_IMAGE=... WITHOUT_TOOLS=1 sh tools/floppy/build_msdos.sh
#
# MS-DOS is not redistributable: keep the source image under tools/floppy/local/,
# which is ignored by git.
set -e

cd "$(dirname "$0")/../.."
root=$(pwd)
fat12="python3 $root/tools/floppy/fat12.py"
image="${OUT_IMAGE:-$root/third_party/8086tiny/fd.img}"
payload="$root/tools/floppy/payload/tc201"
base="${DOS_IMAGE:-$root/tools/floppy/local/dos622.img}"

if [ ! -f "$base" ]; then
  echo "missing MS-DOS image: $base" >&2
  echo "set DOS_IMAGE=<path to your 1.44 MB MS-DOS 6.22 boot floppy>" >&2
  exit 1
fi

python3 "$root/tools/floppy/check_image.py" "$base"

cp "$base" "$image"
echo
$fat12 "$image" free

# ---------------------------------------------------------------------------
# 1. Remove what an 8086 XT with no CD-ROM drive cannot use
# ---------------------------------------------------------------------------
echo "== pruning =="
for path in SMARTDRV.EXE MSCDEX.EXE FINDDRIV.EXE HDTYPE.COM CTLOAD.COM \
            BOOT_CD-.ROM SD.INI EDIT.COM; do
  $fat12 "$image" del "$path" 2>/dev/null || echo "  (absent) $path"
done
for directory in IDE SCSI DM; do
  for name in $(python3 - "$image" "$directory" <<'PY'
import sys

sys.path.insert(0, "tools/floppy")
from fat12 import Fat12                                     # noqa: E402

image, directory = sys.argv[1], sys.argv[2]
fs = Fat12(image)
try:
    cluster = fs.find_dir(directory)
except SystemExit:
    raise SystemExit(0)
print(" ".join(row[0] for row in fs.list_dir(cluster)
               if not (row[1] & 0x10)))
PY
  ); do
    $fat12 "$image" del "$directory/$name" >/dev/null 2>&1 || true
  done
done

# ---------------------------------------------------------------------------
# 2. Plain boot configuration
# ---------------------------------------------------------------------------
echo "== replacing CONFIG.SYS and AUTOEXEC.BAT =="
stage=$(mktemp -d "${TMPDIR:-/tmp}/sf8086-msdos.XXXXXX")
for name in CONFIG.SYS AUTOEXEC.BAT; do
  awk 'BEGIN { ORS = "\r\n" } { print }' "$root/tools/floppy/msdos/$name" > "$stage/$name"
done
$fat12 "$image" del CONFIG.SYS
$fat12 "$image" del AUTOEXEC.BAT
$fat12 "$image" add "$stage/CONFIG.SYS"
$fat12 "$image" add "$stage/AUTOEXEC.BAT"

# ---------------------------------------------------------------------------
# 3. Editor and C toolchain
# ---------------------------------------------------------------------------
if [ "${WITHOUT_TOOLS:-}" != "1" ]; then
  # MS-DOS 6.22 ships EDIT.COM, which needs QBASIC.EXE that this rescue disk does
  # not carry.  The FreeDOS editor is a plain 16-bit DOS program, so it is taken
  # from the vendored FreeDOS image instead.
  if [ -f "$root/third_party/8086tiny/fd-base.img" ]; then
    echo "== adding the FreeDOS editor =="
    $fat12 "$image" import "$root/third_party/8086tiny/fd-base.img" \
      "FREEDOS/BIN/EDIT.EXE" ""
    $fat12 "$image" import "$root/third_party/8086tiny/fd-base.img" \
      "FREEDOS/BIN/EDIT.HLP" ""
  fi

  if [ -f "$payload/BIN/TCC.EXE" ]; then
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

    echo "== adding samples =="
    $fat12 "$image" mkdir TC/SAMPLES
    stage2=$(mktemp -d "${TMPDIR:-/tmp}/sf8086-samples.XXXXXX")
    for file in "$root"/tools/floppy/dosfiles/*; do
      name=$(basename "$file")
      awk 'BEGIN { ORS = "\r\n" } { print }' "$file" > "$stage2/$name"
    done
    for file in "$stage2"/*; do
      case "$file" in
        *.C) $fat12 "$image" add "$file" TC/SAMPLES ;;
        *)   $fat12 "$image" add "$file" ;;
      esac
    done
  else
    echo "== Turbo C payload not staged: skipping the compiler =="
    echo "   run: sh tools/floppy/fetch_turbo_c.sh"
  fi
fi

$fat12 "$image" check
echo
$fat12 "$image" free
echo
echo "built $image"

# ---------------------------------------------------------------------------
# 4. Tell the firmware build that the image changed
# ---------------------------------------------------------------------------
if [ -n "${OUT_IMAGE:-}" ]; then
  echo "built $image (OUT_IMAGE set, firmware stamp left alone)"
  exit 0
fi

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
