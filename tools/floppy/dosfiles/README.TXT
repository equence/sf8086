sf8086: FreeDOS on the SF32LB525 (Huangshan Pi) board
=====================================================

What you are running
  FreeDOS (kernel build 2040, FreeCOM 0.82) on top of the 8086tiny emulator,
  which runs on the board's Cortex-M33 at 240 MHz.  Your terminal is the board
  UART1; the emulator maps it to the guest's keyboard and BIOS console.

Disks
  A: is a RAM disk inside the emulator.  It starts as a pristine copy of this
  floppy on every boot, and any file you write is lost when the emulator
  restarts.  Copy what you want to keep off the board (or rebuild the image
  with tools/floppy/build_fdimg.sh on your computer).

Going back to the board shell
  Press Ctrl-] to pause the emulator and return to the RT-Thread shell, then
  "sf8086run" to continue or "sf8086" to reboot DOS from a clean floppy.

Writing C
  CC hello.c            compile and link (Turbo C 2.01, small model)
  CC sieve.c            CC puts the .EXE next to the source
  SIEVE                 run it
  EDIT hello.c          full screen editor (F10 for the menu, Alt-F S to save)
  Examples are in \TC\SAMPLES.  The toolchain lives in \TC (TCC + TLINK);
  DEBUG is available for disassembly, and ZIP/UNZIP for archives.

Useful commands already installed
  EDIT  TREE  MOVE  XCOPY  FC  FIND  SORT  MORE  MEM  UNDELETE  ATTRIB
  CHOICE  DELTREE  FDAPM  MKEYB  DOSLFN  DISKCOPY  FDISK  FORMAT  SYS

Not installed on purpose
  Programs that need a 286/386 or protected mode (EMM386, HIMEM, XMS drivers,
  DOSFSCK, CWSDPMI) cannot run under 8086tiny and were removed.
