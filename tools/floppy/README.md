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
