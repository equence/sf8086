# tools/floppy - the DOS boot image that ships in the firmware

`third_party/8086tiny/fd.img` is the 1.44 MB FAT12 floppy that FreeDOS boots
from inside the emulator.  It is *generated*; the pristine upstream image is
kept as `fd-base.img` so every change is reproducible and reviewable.

## Rebuilding

```sh
sh tools/floppy/fetch_turbo_c.sh    # once: stages the C toolchain payload
sh tools/floppy/build_fdimg.sh      # fd-base.img + edits -> fd.img
```

`build_fdimg.sh` also stamps the new image checksums into
`sdk_probe/src/disk_image.S`, because the firmware pulls the image in with
`.incbin` and the build system cannot see that dependency.

After changing the image, reflash the firmware: the new floppy is what the board
serves to DOS (see the repository README for the guest console and the disks).

## Running MS-DOS instead of FreeDOS

The emulator needs no change for this. 8086tiny's BIOS already provides what
MS-DOS asks for (INT 13h read/write/parameters/type, INT 12h memory size, INT 11h
equipment, INT 1Ah clock, INT 1Ch tick, INT 16h keyboard, INT 10h video), and the
upstream documentation lists MS-DOS 6.22 among the tested systems.

Only the image changes. Put a 1.44 MB MS-DOS 6.22 boot floppy under
`tools/floppy/local/` (git-ignored, since MS-DOS is proprietary and must not be
committed) and run:

```sh
cp /path/to/dos622.img tools/floppy/local/dos622.img
sh tools/floppy/build_msdos.sh          # -> third_party/8086tiny/fd.img
cd sdk_probe/project && scons --board=sf32lb52-lchspi-ulp -j8
```

`build_msdos.sh` turns the stock rescue disk into something an 8086 can use: the
CD-ROM driver menu, the IDE/SCSI/DM driver trees, SMARTDRV, MSCDEX and the HIMEM
line in `CONFIG.SYS` are removed (none of them work here), `CONFIG.SYS` and
`AUTOEXEC.BAT` are replaced with a plain pair, and the Turbo C toolchain plus the
FreeDOS editor are added. `WITHOUT_TOOLS=1` leaves the toolchain out, and
`OUT_IMAGE=<path>` writes somewhere other than `fd.img`.

Result, as verified by `tools/host_bench/msdos_smoke.sh`: MS-DOS 6.22 reports
640 KB of conventional memory with 576 KB free, Turbo C compiles and links a
sample, and the program runs.

MS-DOS 7.1 (the Win9x-era DOS) does not work here: its boot sector already uses
386 instructions and its kernel expects a 386, while 8086tiny emulates an 8086.

Any other bootable 1.44 MB FAT12 floppy can be used with the plain builder:

```sh
DOS_IMAGE=/path/to/other-dos.img sh tools/floppy/build_fdimg.sh
```

That path checks the geometry, the boot signature and the presence of a DOS
system file, copies the image and edits the copy. Pruning is skipped for supplied
images since their layout is unknown (`PRUNE=1` applies the FreeDOS list), and
the toolchain is only added when the image has room for it.

## What is on the floppy

The upstream image already contained a full FreeDOS tool set (EDIT, DEBUG, ZIP/
UNZIP, XCOPY, MOVE, MEM, TREE, FDISK, FORMAT, DOSLFN, ...).  On top of that:

| Directory | Contents |
|-----------|----------|
| `\` | `CC.BAT` (compile + link), `README.TXT`, small samples |
| `\TC` | Turbo C 2.01: `TCC.EXE` (compiler), `TLINK.EXE` (linker) |
| `\TC\INCLUDE` | the 26 Turbo C headers |
| `\TC\LIB` | small model runtime: `C0S.OBJ`, `CS.LIB`, `MATHS.LIB`, `EMU.LIB` |
| `\TC\SAMPLES` | `HELLO.C`, `SIEVE.C`, `FILEIO.C`, `KEYTEST.C` |

`KEYTEST.C` prints the scancode/ASCII pair (INT 16h) and the BIOS shift flags for
every key press - handy when a key combination "does not work" in a DOS program.

Usage inside DOS:

```
CC hello.c        ->  HELLO.EXE   (Turbo C, small memory model)
EDIT hello.c      ->  full screen editor
```

## What was removed, and why

The floppy has 1.44 MB and was already ~98% full, so room for a compiler meant
removing things that cannot work on this machine:

* 286/386-only and DPMI programs: `EMM386`, `HIMEM`, the XMS drivers,
  `DOSFSCK`, `CWSDPMI`, `LFNXLAT.386`.  8086tiny implements neither protected
  mode nor XMS, so these could never run - keeping them only invited confusion.
* Hardware this emulator does not have: CD-ROM/SCSI/ISO tools and printer tools.
* The FreeDOS help database (230 KB) and its viewers; the documentation lives in
  this repository instead.  Set `KEEP_HELP=1` when rebuilding to keep it.

The boot configuration (`CONFIG.SYS`/`AUTOEXEC.BAT`) never loaded any of them.

## Licensing

The FreeDOS parts are GPL (see the FreeDOS project).  The Turbo C 2.01 files are
Borland's 1989 compiler, redistributed here as "antique software" for a private
build - see `payload/README.md` for the details and for how to build the image
without it.
