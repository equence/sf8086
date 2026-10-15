/*
 * RAM-disk backend for the 8086tiny port (see emu_disk.h).
 *
 * Designed for the target: no malloc, no file system, no locking - the
 * emulator thread is the only user.  Read-ahead/seek semantics follow plain
 * POSIX file behaviour closely enough for 8086tiny:
 *   - lseek(fd, 0, SEEK_END) returns the image size (the core does >> 9)
 *   - reads and writes at the end of the image return a short count
 *   - the file position is reset on every open
 */
#include "emu_disk.h"

#define EMU_DISK_MAX 4
/* File descriptors that cannot collide with the C library's own descriptors. */
#define EMU_DISK_FD_BASE 0x400

struct emu_disk
{
    const char *name;
    unsigned char *image;
    unsigned int size;
    unsigned int position;
};

static struct emu_disk emu_disks[EMU_DISK_MAX];
static int emu_disk_count;

/* Accept "/bios", "bios", "./bios" and similar spellings. */
static int name_matches(const char *path, const char *name)
{
    if (path == 0 || name == 0)
        return 0;

    while (*path == '/' || (path[0] == '.' && path[1] == '/'))
        path++;

    while (*path && *name && *path == *name)
    {
        path++;
        name++;
    }
    return *path == 0 && *name == 0;
}

int emu_disk_register(const char *name, unsigned char *image, unsigned int size)
{
    if (emu_disk_count >= EMU_DISK_MAX || name == 0 || image == 0)
        return -1;

    emu_disks[emu_disk_count].name = name;
    emu_disks[emu_disk_count].image = image;
    emu_disks[emu_disk_count].size = size;
    emu_disks[emu_disk_count].position = 0;
    emu_disk_count++;
    return 0;
}

static struct emu_disk *disk_from_fd(int fd)
{
    int index = fd - EMU_DISK_FD_BASE;

    if (index < 0 || index >= emu_disk_count)
        return 0;
    return &emu_disks[index];
}

int emu_disk_open(const char *path, int flags, ...)
{
    int i;

    for (i = 0; i < emu_disk_count; i++)
    {
        if (name_matches(path, emu_disks[i].name))
        {
            emu_disks[i].position = 0;
            return EMU_DISK_FD_BASE + i;
        }
    }
    return -1;
}

int emu_disk_close(int fd)
{
    return disk_from_fd(fd) ? 0 : -1;
}

off_t emu_disk_lseek(int fd, off_t offset, int whence)
{
    struct emu_disk *disk = disk_from_fd(fd);
    off_t base;

    if (disk == 0)
        return -1;

    switch (whence)
    {
    case 0: /* SEEK_SET */
        base = 0;
        break;
    case 1: /* SEEK_CUR */
        base = (off_t)disk->position;
        break;
    case 2: /* SEEK_END */
        base = (off_t)disk->size;
        break;
    default:
        return -1;
    }

    if (whence == 2)
        return (off_t)disk->size; /* the core only asks for the size here */

    base += offset;
    if (base < 0)
        base = 0;
    if (base > (off_t)disk->size)
        base = (off_t)disk->size;
    disk->position = (unsigned int)base;
    return base;
}

ssize_t emu_disk_read(int fd, void *buffer, size_t length)
{
    struct emu_disk *disk = disk_from_fd(fd);
    size_t available;
    size_t i;

    if (disk == 0 || buffer == 0)
        return -1;

    available = disk->size - disk->position;
    if (length > available)
        length = available;

    for (i = 0; i < length; i++)
        ((unsigned char *)buffer)[i] = disk->image[disk->position + i];
    disk->position += (unsigned int)length;
    return (ssize_t)length;
}

ssize_t emu_disk_write(int fd, const void *buffer, size_t length)
{
    struct emu_disk *disk = disk_from_fd(fd);
    size_t available;
    size_t i;

    if (disk == 0 || buffer == 0)
        return -1;

    available = disk->size - disk->position;
    if (length > available)
        length = available;

    for (i = 0; i < length; i++)
        disk->image[disk->position + i] = ((const unsigned char *)buffer)[i];
    disk->position += (unsigned int)length;
    return (ssize_t)length;
}
