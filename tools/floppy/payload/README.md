# payload/ - third-party binaries staged for the floppy image

These files are not built from this repository; they are staged here so that
`build_fdimg.sh` can produce the boot image offline.  `fetch_turbo_c.sh`
re-downloads them from a public archive.

## tc201/ - Turbo C 2.01 (Borland, 1989)

`tc201/BIN/{TCC.EXE,TLINK.EXE}`, `tc201/INCLUDE/*.H` and
`tc201/LIB/{C0S.OBJ,CS.LIB,MATHS.LIB,EMU.LIB}` come from the Turbo C 2.01
distribution disks (archive.org item `tc201_202505`).

Why this compiler: it is the practical C toolchain for an 8086-class machine.
TCC.EXE and TLINK.EXE are 16-bit real-mode DOS programs, so they run under
8086tiny, which does not implement 386 protected mode.  Toolchains that need a
386 or a DPMI host (OpenWatcom, DJGPP, SmallerC's prebuilt DOS binaries) cannot.

Borland released Turbo C 2.01 as free "antique software" in 2006; the release
permits free download and use, but does not explicitly grant redistribution
rights.  These files are staged for a private build of this board image.  If
this repository is going to be published, either drop `tc201/` (and skip the
compiler in `build_fdimg.sh`) or confirm the redistribution terms first.

Deliberately *not* staged: TC.EXE (the 290 KB IDE, which needs more memory than
an 8086tiny guest has free), TCHELP.TCH, the graphics/BGI libraries and the
medium/compact/large/huge model libraries.
