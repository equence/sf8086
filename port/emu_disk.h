/*
 * RAM-disk backend for the 8086tiny port.
 *
 * The emulator only needs a tiny POSIX-like file interface for its BIOS and
 * floppy images (open / lseek / read / write / close).  Instead of mounting a
 * file system on the target, the port keeps the images in PSRAM and serves
 * those four calls from memory.  The emulator core is redirected to these
 * entry points at compile time (see sdk_probe/src/sf8086_core.c), so no
 * upstream source changes are needed for disk I/O.
 */
#ifndef SF8086_EMU_DISK_H
#define SF8086_EMU_DISK_H

#include <sys/types.h> /* ssize_t / off_t so the prototypes stay ABI-correct */

#ifdef __cplusplus
extern "C" {
#endif

/* Hand a RAM-resident image to the backend under `name` (e.g. "bios"). */
int emu_disk_register(const char *name, unsigned char *image, unsigned int size);

/* POSIX-like entry points; signatures match newlib's declarations. */
int emu_disk_open(const char *path, int flags, ...);
int emu_disk_close(int fd);
off_t emu_disk_lseek(int fd, off_t offset, int whence);
ssize_t emu_disk_read(int fd, void *buffer, size_t length);
ssize_t emu_disk_write(int fd, const void *buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif
