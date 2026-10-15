/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 *
 * Build unit for the upstream 8086tiny core on the SF32LB525.
 *
 * The upstream tree is included verbatim; everything the target needs is done
 * here so that third_party/8086tiny/8086tiny.c stays close to upstream:
 *   - SF8086_SERIAL_PORT: console I/O goes through port/emu_console.h (UART1)
 *   - SF8086_PSRAM_MEM:   guest RAM is a pointer supplied by the port layer
 *   - NO_GRAPHICS:        no SDL, text console only
 *   - the emulator entry point is renamed so it can live under RT-Thread
 *   - the three warning classes the dense upstream macros trigger are muted
 *     locally (the SDK build treats warnings as errors)
 *
 * NOTE: this file must be compiled with -fsigned-char (see src/SConscript).
 * The SDK project default is -funsigned-char, and with that setting the
 * emulator produces no guest output at all.
 */
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wlogical-not-parentheses"

#define SF8086_SERIAL_PORT
#define SF8086_PSRAM_MEM
#define NO_GRAPHICS

/*
 * Disk I/O goes to the RAM-disk backend instead of the C library: the port
 * keeps the BIOS and floppy images in PSRAM (see port/emu_disk.c), so no file
 * system has to be mounted.  The macros must be in place before the emulator
 * includes <fcntl.h>/<unistd.h> so the declarations get redirected as well.
 */
#define open emu_disk_open
#define close emu_disk_close
#define lseek emu_disk_lseek
#define read emu_disk_read
#define write emu_disk_write
#include "emu_disk.h"

#define main sf8086_main
#include "../../third_party/8086tiny/8086tiny.c"
