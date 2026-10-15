/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 *
 * "emu_idle" console sink for the 8086tiny port (see emu_idle_dev.h).
 *
 * Behaviour:
 *   read()  - blocks until emu_idle_device_wake() is called, then delivers the
 *             internal command line one byte at a time ("sf8086_paused").  No
 *             other input ever arrives, so a shell reading this device costs no
 *             CPU while the emulator runs.
 *   write() - accepted and discarded (the emulator's own output goes to UART1
 *             through emu_console_tx, not through stdio).
 *
 * Registered lazily by the console backend; devfs resolves /dev/<name> at open
 * time, so a device registered after boot is usable immediately.
 */
#include "emu_idle_dev.h"

#include <rtdevice.h>
#include <rtthread.h>

static struct rt_device emu_idle_device;
static struct rt_semaphore emu_idle_sem;
static int emu_idle_created;

/*
 * Executed by the shell when the console is handed back.  The leading
 * backspaces discard any partial line left in the shell's buffer, so the
 * command itself is always what gets parsed.
 */
static const char emu_idle_line[] = "\b\b\b\b\b\b\b\b" "sf8086_paused\n";
static const char *emu_idle_next;

static rt_err_t emu_idle_init(rt_device_t dev)
{
    (void)dev;
    return RT_EOK;
}

static rt_err_t emu_idle_open(rt_device_t dev, rt_uint16_t oflag)
{
    (void)dev;
    (void)oflag;
    return RT_EOK;
}

static rt_err_t emu_idle_close(rt_device_t dev)
{
    (void)dev;
    return RT_EOK;
}

static rt_size_t emu_idle_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    (void)dev;
    (void)pos;

    if (size == 0 || buffer == RT_NULL)
        return 0;

    for (;;)
    {
        if (emu_idle_next != RT_NULL && *emu_idle_next != 0)
        {
            ((char *)buffer)[0] = *emu_idle_next++;
            return 1;
        }

        emu_idle_next = RT_NULL;

        /* Park the reader until the console is handed back. */
        if (rt_sem_take(&emu_idle_sem, RT_WAITING_FOREVER) != RT_EOK)
            return 0;
    }
}

static rt_size_t emu_idle_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    (void)dev;
    (void)pos;
    (void)buffer;
    return size; /* discarded */
}

static rt_err_t emu_idle_control(rt_device_t dev, int cmd, void *args)
{
    (void)dev;
    (void)cmd;
    (void)args;
    return RT_EOK;
}

int emu_idle_device_create(void)
{
    if (emu_idle_created)
        return 0;

    if (rt_sem_init(&emu_idle_sem, "emu_idle", 0, RT_IPC_FLAG_FIFO) != RT_EOK)
        return -1;

    emu_idle_device.type = RT_Device_Class_Char;
    emu_idle_device.init = emu_idle_init;
    emu_idle_device.open = emu_idle_open;
    emu_idle_device.close = emu_idle_close;
    emu_idle_device.read = emu_idle_read;
    emu_idle_device.write = emu_idle_write;
    emu_idle_device.control = emu_idle_control;

    if (rt_device_register(&emu_idle_device, "emu_idle", RT_DEVICE_FLAG_RDWR) != RT_EOK)
        return -1;

    emu_idle_created = 1;
    return 0;
}

void emu_idle_device_reset(void)
{
    if (!emu_idle_created)
        return;

    emu_idle_next = RT_NULL;

    /* Drop a wake-up that nobody consumed, so the shell does not execute a
     * spurious empty command later on. */
    (void)rt_sem_control(&emu_idle_sem, RT_IPC_CMD_RESET, RT_NULL);
}

void emu_idle_device_wake(void)
{
    if (!emu_idle_created)
        return;

    emu_idle_next = emu_idle_line;
    (void)rt_sem_release(&emu_idle_sem);
}
