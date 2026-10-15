/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 *
 * SiFli/RT-Thread UART1 console backend for SF32LB525.
 *
 * Console ownership
 * -----------------
 * UART1 is the RT-Thread console.  With RT_USING_POSIX the shell reads it
 * through newlib's getchar(), i.e. fd 0, which libc_system_init() bound to
 * /dev/uart1 at boot.  The emulator reads the same device directly, so without
 * arbitration the two readers split the keystrokes (guest input lands in the
 * shell and vice versa).
 *
 * The fix is to make sure only one reader exists at a time:
 *   take_over():  bind stdio to /dev/emu_idle, a device that never delivers
 *                 input, so the shell's blocking read parks there instead of
 *                 touching UART1.  UART1 is then read by the emulator alone.
 *   hand_back():  bind stdio back to UART1 so the shell is usable again.
 *
 * Both run in the shell thread (switching stdio closes the FILE the shell reads
 * from, which must not happen while that thread is inside a read).  The shell
 * reaches the hand-back command because Ctrl-] makes this backend queue
 * "sf8086_paused" on the idle device and wake the parked read.
 *
 * The escape byte (Ctrl-], 0x1D) is consumed here and never reaches the guest:
 * it parks this thread until the shell resumes or reboots the emulator.  Note
 * that a thread cannot be suspended from outside on this RT-Thread version
 * (rt_thread_suspend only accepts a READY thread), so parking on a semaphore is
 * the portable way.
 */
#include "emu_console.h"
#include "emu_idle_dev.h"
#include "emu_meta.h"

#include <fcntl.h>
#include <rtdevice.h>
#include <rtthread.h>

/* Provided by the newlib glue (components/libc/compilers/newlib/stdio.c). */
extern int libc_stdio_set_console(const char *device_name, int mode);

/* Ctrl-] returns the console to the RT-Thread shell. */
#define EMU_CONSOLE_ESCAPE 0x1D
#define EMU_IDLE_DEVICE_NAME "emu_idle"

extern unsigned int inst_counter; /* defined in 8086tiny.c */

static rt_device_t console_uart;
static int console_opened;
static unsigned char pending_byte;
static int has_pending_byte;

/*
 * The emulator never blocks, and RT-Thread only preempts a running thread for
 * one with a numerically lower priority.  Anything at a higher number - the
 * idle thread, a watchdog feeder, USB/PM work - would therefore never run while
 * DOS is running.  This poll runs once per 20000 guest instructions, so yielding
 * here costs well under 1% of the guest speed and keeps the rest of the system
 * alive.
 */
#define EMU_CONSOLE_POLLS_PER_YIELD 10
static unsigned int poll_count;

static struct rt_semaphore emu_pause_sem;
static int emu_pause_sem_ready;
static volatile int emu_paused;          /* emulator parked, shell owns UART1 */
static volatile int emu_reboot_request;  /* shell asked for a fresh boot */
static void (*emu_reboot_hook)(void);    /* unwinds into the emulator loop */

/*
 * Boot-time report: the first time the guest prints the DOS prompt, log how
 * long the emulator has been running and how many guest instructions that took.
 */
static const char prompt_sequence[] = "A:\\>";
static unsigned char prompt_match;
static unsigned char prompt_reported;
static rt_tick_t emulator_start_tick;

static void emu_pause_sem_init(void)
{
    if (!emu_pause_sem_ready)
    {
        (void)rt_sem_init(&emu_pause_sem, "emu_pause", 0, RT_IPC_FLAG_FIFO);
        emu_pause_sem_ready = 1;
    }
}

int emu_console_init(void)
{
    if (console_opened)
        return 0;

    console_uart = rt_device_find("uart1");
    if (console_uart == RT_NULL)
        return -RT_ENOSYS;

    emu_console_reset_prompt_report();

    if (rt_device_open(console_uart,
                       RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_DMA_RX) != RT_EOK)
    {
        console_uart = RT_NULL;
        return -RT_EIO;
    }

    console_opened = 1;
    return 0;
}

int emu_console_take_over(void)
{
    emu_pause_sem_init();

    /* Open UART1 for the emulator before the shell's stdio handle on it goes
     * away, so the device never drops to zero references. */
    if (emu_console_init() < 0)
        return -1;
    if (emu_idle_device_create() != 0)
        return -1;

    emu_idle_device_reset();

    if (libc_stdio_set_console(EMU_IDLE_DEVICE_NAME, O_RDWR) < 0)
        return -1;

    return 0;
}

int emu_console_hand_back(void)
{
    return libc_stdio_set_console(RT_CONSOLE_DEVICE_NAME, O_RDWR) < 0 ? -1 : 0;
}

int emu_console_is_paused(void)
{
    return emu_paused;
}

int emu_console_resume(void)
{
    if (!emu_paused)
        return -1;
    if (emu_console_take_over() != 0)
        return -1;

    emu_reboot_request = 0;
    emu_paused = 0;
    (void)rt_sem_release(&emu_pause_sem);
    return 0;
}

int emu_console_reboot(void)
{
    if (!emu_paused)
        return -1;
    if (emu_console_take_over() != 0)
        return -1;

    emu_reboot_request = 1;
    emu_paused = 0;
    (void)rt_sem_release(&emu_pause_sem);
    return 0;
}

void emu_console_set_reboot_hook(void (*hook)(void))
{
    emu_reboot_hook = hook;
}

void emu_console_reset_prompt_report(void)
{
    prompt_match = 0;
    prompt_reported = 0;
    emulator_start_tick = rt_tick_get();
}

int emu_console_rx(unsigned char *out)
{
    unsigned char ch;
    unsigned char next;
    int waited;

    if (console_uart == RT_NULL)
        return 0;

    if ((++poll_count % EMU_CONSOLE_POLLS_PER_YIELD) == 0)
        rt_thread_mdelay(1);

    if (has_pending_byte)
    {
        *out = pending_byte;
        has_pending_byte = 0;
        return 1;
    }

    if (rt_device_read(console_uart, -1, &ch, 1) != 1)
        return 0;

    if (ch == EMU_META_ESC)
    {
        /*
         * A terminal sends Alt+key as ESC followed by the key.  Wait a moment
         * for that key: if it is printable, report it as the BIOS's Alt prefix
         * (Ctrl+A) followed by the key, otherwise pass the ESC through so
         * escape sequences and the plain ESC key keep working.
         */
        for (waited = 0; waited < EMU_META_TIMEOUT_MS; waited++)
        {
            if (rt_device_read(console_uart, -1, &next, 1) == 1)
            {
                pending_byte = next;
                has_pending_byte = 1;
                *out = emu_meta_is_alt_pair(next) ? EMU_META_ALT_PREFIX : EMU_META_ESC;
                return 1;
            }
            rt_thread_mdelay(1);
        }

        *out = EMU_META_ESC; /* lone ESC */
        return 1;
    }

    if (ch == EMU_CONSOLE_ESCAPE)
    {
        emu_paused = 1; /* set before the shell can act on the handover */

        /* Wake the shell through the idle device; it will run sf8086_paused,
         * which rebinds stdio to UART1 from its own thread. */
        emu_idle_device_wake();

        (void)rt_sem_take(&emu_pause_sem, RT_WAITING_FOREVER);

        emu_paused = 0;
        if (emu_reboot_request)
        {
            emu_reboot_request = 0;
            if (emu_reboot_hook != RT_NULL)
                emu_reboot_hook(); /* does not return: restarts the guest */
        }
        return 0; /* the escape byte never reaches the guest */
    }

    *out = ch;
    return 1;
}

int emu_console_tx(unsigned char ch)
{
    int result;

    if (console_uart == RT_NULL)
        return -RT_EIO;

    result = rt_device_write(console_uart, 0, &ch, 1) == 1 ? 0 : -RT_EIO;

    if (!prompt_reported)
    {
        if (ch == (unsigned char)prompt_sequence[prompt_match])
        {
            if (prompt_sequence[++prompt_match] == 0)
            {
                prompt_reported = 1;
                rt_kprintf("\n[8086] DOS prompt reached after %u ms, %u guest instructions\n",
                           (unsigned int)(rt_tick_get() - emulator_start_tick),
                           inst_counter);
            }
        }
        else
        {
            prompt_match = (ch == (unsigned char)prompt_sequence[0]) ? 1 : 0;
        }
    }

    return result;
}

void emu_console_shutdown(void)
{
    if (console_uart != RT_NULL)
        (void)rt_device_close(console_uart);
    console_uart = RT_NULL;
    console_opened = 0;
}
