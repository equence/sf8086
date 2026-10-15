/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 *
 * Host harness for the RAM-disk backend.
 *
 * Loads the guest BIOS and floppy image into plain memory buffers, registers
 * them with port/emu_disk.c, and runs the emulator core through exactly the
 * same redirection the target firmware uses.  Running it on the host is a
 * regression test for the backend semantics (open / lseek / read / write)
 * before flashing, and it reports how many guest instructions the boot takes.
 */
#include <stdio.h>
#include <stdlib.h>

#include "emu_disk.h"

extern unsigned char *mem; /* defined in 8086tiny.c with SF8086_PSRAM_MEM */
extern int sf8086_main(int argc, char **argv);

static unsigned char *load_image(const char *path, unsigned int *size)
{
    FILE *file = fopen(path, "rb");
    unsigned char *buffer;
    long length;

    if (file == 0)
    {
        fprintf(stderr, "ramdisk: cannot open %s\n", path);
        return 0;
    }
    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = (unsigned char *)malloc((size_t)length);
    if (buffer == 0 || fread(buffer, 1, (size_t)length, file) != (size_t)length)
    {
        fprintf(stderr, "ramdisk: cannot read %s\n", path);
        fclose(file);
        free(buffer);
        return 0;
    }
    fclose(file);
    *size = (unsigned int)length;
    return buffer;
}

int main(void)
{
    unsigned int bios_size = 0, fd_size = 0;
    unsigned char *bios = load_image("bios", &bios_size);
    unsigned char *floppy = load_image("fd.img", &fd_size);
    /* 8086tiny's argument loop walks argv until it reads the NULL terminator
     * after the last argument, so the array must be NULL terminated. */
    char *args[4];

    if (bios == 0 || floppy == 0)
        return 1;

    /* Same size the target reserves in PSRAM for guest RAM. */
    mem = (unsigned char *)calloc(1, 0x110000u);
    if (mem == 0)
        return 1;

    if (emu_disk_register("bios", bios, bios_size) != 0 ||
        emu_disk_register("fd.img", floppy, fd_size) != 0)
    {
        fprintf(stderr, "ramdisk: registration failed\n");
        return 1;
    }
    fprintf(stderr, "ramdisk: bios=%u B, fd.img=%u B (%u sectors)\n",
            bios_size, fd_size, fd_size >> 9);

    args[0] = "8086";
    args[1] = "bios";
    args[2] = "fd.img";
    args[3] = 0;
    return sf8086_main(3, args);
}
