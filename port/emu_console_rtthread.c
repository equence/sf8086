/* SiFli/RT-Thread UART1 console backend for SF32LB525. */
#include "emu_console.h"

#include <rtdevice.h>
#include <rtthread.h>

static rt_device_t console_uart;

/*
 * Boot-time report: the first time the guest prints the DOS prompt, log how
 * long the emulator has been running and how many guest instructions that took.
 * This is the measurement that answers the "is it fast enough" question on
 * hardware, without needing an external stopwatch.
 */
extern unsigned int inst_counter; /* defined in 8086tiny.c */

static const char prompt_sequence[] = "A:\\>";
static unsigned char prompt_match;
static unsigned char prompt_reported;
static rt_tick_t emulator_start_tick;

int emu_console_init(void)
{
    console_uart = rt_device_find("uart1");
    if (console_uart == RT_NULL)
        return -RT_ENOSYS;

    prompt_match = 0;
    prompt_reported = 0;
    emulator_start_tick = rt_tick_get();

    /* The board Kconfig already enables UART1 RX DMA on PA18/PA19. */
    return rt_device_open(console_uart,
                          RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_DMA_RX);
}

int emu_console_rx(unsigned char *out)
{
    return console_uart != RT_NULL && rt_device_read(console_uart, -1, out, 1) == 1;
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
}
