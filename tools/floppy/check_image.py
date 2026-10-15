#!/usr/bin/env python3
"""Check that a file is a 1.44 MB FAT12 boot floppy we can build from.

Used by the FreeDOS and MS-DOS image builders.  Prints what it found and exits
non-zero if the file could not be booted by the emulated BIOS at all.
"""

import re
import struct
import sys

SYSTEM_FILES = (b"IO      SYS", b"MSDOS   SYS", b"IBMBIO  COM",
                b"IBMDOS  COM", b"KERNEL  SYS")


def check(path):
    data = open(path, "rb").read()
    problems = []

    if len(data) != 1474560:
        problems.append(f"size is {len(data)}, expected 1474560 (1.44 MB)")
    if data[510:512] != b"\x55\xAA":
        problems.append("no 55 AA boot signature")

    print(f"base image: {path}")
    try:
        bps, spc, resv, nfat, ndir, nsect, media = struct.unpack_from("<HBHBHHB", data, 11)
        spf = struct.unpack_from("<H", data, 22)[0]
    except Exception as exc:                                # noqa: BLE001
        problems.append(f"unreadable BPB: {exc}")
    else:
        print(f"  geometry : {bps} B/sector, {spc} sector/cluster, {nsect} sectors, "
              f"media 0x{media:02X}")
        print(f"  layout   : {nfat} FAT of {spf} sectors, {ndir} root entries, "
              f"{resv} reserved")
        if bps != 512 or spc != 1 or nsect != 2880:
            problems.append(f"geometry is {bps} B sectors, {spc} per cluster, {nsect} total")

        root_dir = data[(resv + nfat * spf) * bps:(resv + nfat * spf) * bps + ndir * 32]
        names = b"".join(re.findall(rb"[A-Z0-9_ ]{11}", root_dir))
        if not any(name in names for name in SYSTEM_FILES):
            print("  warning  : no IO.SYS / MSDOS.SYS / KERNEL.SYS in the root directory")

    if problems:
        for problem in problems:
            print(f"  problem  : {problem}", file=sys.stderr)
        print("not a 1.44 MB FAT12 boot floppy; refusing to build from it",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(check(sys.argv[1]))
