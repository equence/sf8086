/*
 * Flash-resident copies of the guest BIOS and the floppy image.
 *
 * Keeping them in flash (XIP) rather than in a file system is deliberate: the
 * port copies them into PSRAM at start-up with the RAM-disk backend from
 * port/emu_disk.c, so no file system needs to be mounted on the target.
 *
 * Paths are resolved relative to the directory scons is invoked from, which is
 * sdk_probe/project.
 *   image stamp: fd.img 1acf6420752227632fe7e5c11c216918 (1474560 bytes), bios 08766edbc54c12b20cdd0490a32cdec1
 */

    .section .rodata.sf8086_bios_image, "a"
    .balign 4
    .global sf8086_bios_image
sf8086_bios_image:
    .incbin "../../third_party/8086tiny/bios"
    .global sf8086_bios_image_end
sf8086_bios_image_end:

    .section .rodata.sf8086_fd_image, "a"
    .balign 4
    .global sf8086_fd_image
sf8086_fd_image:
    .incbin "../../third_party/8086tiny/fd.img"
    .global sf8086_fd_image_end
sf8086_fd_image_end:

    .end
