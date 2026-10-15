/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 *
 * SF32LB525 (黄山派 / sf32lb52-lchspi-ulp) bring-up probe for the 8086tiny port.
 *
 * Design constraint found while building this probe: the SDK PSRAM linker
 * sections (.RW_PSRAM1 / .RW_PSRAM_NON_RET) are PROGBITS with LMA 0x60000000,
 * so *statically placed* PSRAM data is emitted into main.bin and pads the flash
 * image up to that address (a 1 MiB array produced a 1.3 GB binary). Guest RAM
 * therefore must not be a static array; it is carved out of PSRAM at runtime,
 * which is what this probe verifies.
 */
#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <mem_map.h>

#include "emu_console.h"

/* 8086tiny needs RAM_SIZE (0x10FFF0) plus a little head room of guest memory. */
#define GUEST_RAM_SIZE (0x110000u)
#define FLOPPY_SIZE (1474560u) /* 1.44 MB fd.img, also kept in PSRAM */
#define BENCH_SIZE (64u * 1024u)
#define PATTERN_BASE 0xA5C3F00Du

/* Provided by the generated linker script; first unused PSRAM address. */
extern char __RW_PSRAM_NON_RET_end__[];

static struct rt_memheap psram_heap;
static unsigned char iram_probe[BENCH_SIZE] __attribute__((aligned(32)));

static void dwt_start(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static unsigned int fill_and_check(volatile unsigned int *base, unsigned int words)
{
    unsigned int i;
    unsigned int errors = 0;

    for (i = 0; i < words; i++)
        base[i] = PATTERN_BASE ^ i;
    for (i = 0; i < words; i++)
    {
        if (base[i] != (PATTERN_BASE ^ i))
            errors++;
    }
    return errors;
}

static int psram_smoke(int argc, char **argv)
{
    void *heap_start = (void *)(((unsigned int)__RW_PSRAM_NON_RET_end__ + 31u) & ~31u);
    unsigned int heap_size = (unsigned int)(PSRAM_BASE + PSRAM_SIZE) - (unsigned int)heap_start;
    unsigned char *guest_ram;
    unsigned char *floppy;
    unsigned int c0, c1, c2, c3;
    unsigned int words = GUEST_RAM_SIZE / 4u;
    unsigned int bench_words = BENCH_SIZE / 4u;
    unsigned int errors_psram, errors_iram, errors_floppy;
    unsigned int cycles_psram, cycles_iram;
    unsigned char rx = 0;
    int console_open;
    unsigned int i;
    int got_rx;

    rt_kprintf("\n=== sf8086 probe: SF32LB525 guest RAM / console bring-up ===\n");
    rt_kprintf("PSRAM window: 0x%08X..0x%08X (%u MB)\n",
               (unsigned int)PSRAM_BASE, (unsigned int)(PSRAM_BASE + PSRAM_SIZE),
               (unsigned int)(PSRAM_SIZE / (1024u * 1024u)));
    rt_kprintf("free PSRAM for the emulator: 0x%08X, %u bytes\n",
               (unsigned int)heap_start, heap_size);

    if (rt_memheap_init(&psram_heap, "psram", heap_start, heap_size) != RT_EOK)
    {
        rt_kprintf("probe: FAIL (psram heap init)\n");
        return -1;
    }

    guest_ram = (unsigned char *)rt_memheap_alloc(&psram_heap, GUEST_RAM_SIZE);
    floppy = (unsigned char *)rt_memheap_alloc(&psram_heap, FLOPPY_SIZE);
    rt_kprintf("guest_ram   : %p (%u bytes requested)\n", (void *)guest_ram,
               (unsigned int)GUEST_RAM_SIZE);
    rt_kprintf("floppy image: %p (%u bytes requested)\n", (void *)floppy,
               (unsigned int)FLOPPY_SIZE);
    if (!guest_ram || !floppy)
    {
        rt_kprintf("probe: FAIL (psram allocation)\n");
        return -1;
    }

    dwt_start();

    c0 = DWT->CYCCNT;
    errors_psram = fill_and_check((volatile unsigned int *)guest_ram, words);
    c1 = DWT->CYCCNT;

    c2 = DWT->CYCCNT;
    errors_iram = fill_and_check((volatile unsigned int *)iram_probe, bench_words);
    c3 = DWT->CYCCNT;

    cycles_psram = c1 - c0;
    cycles_iram = c3 - c2;
    errors_floppy = fill_and_check((volatile unsigned int *)floppy, FLOPPY_SIZE / 4u);

    rt_kprintf("guest_ram walk: errors=%u cycles=%u (%.1f cyc/word)\n",
               errors_psram, cycles_psram, (double)cycles_psram / (double)words);
    rt_kprintf("iram      walk: errors=%u cycles=%u (%.1f cyc/word)\n",
               errors_iram, cycles_iram, (double)cycles_iram / (double)bench_words);
    rt_kprintf("floppy image walk (%u bytes): errors=%u\n",
               (unsigned int)FLOPPY_SIZE, errors_floppy);
    rt_kprintf("sysclk=%u Hz\n", (unsigned int)SystemCoreClock);

    /* Console backend sanity check: this is the emulator's terminal path. */
    console_open = (emu_console_init() == 0);
    if (console_open)
    {
        static const char banner[] = "[emu_console] uart1 tx ok, send any byte within 3s\n";

        for (i = 0; banner[i]; i++)
            (void)emu_console_tx((unsigned char)banner[i]);

        got_rx = 0;
        for (i = 0; i < 300u; i++)
        {
            if (emu_console_rx(&rx) == 1)
            {
                got_rx = 1;
                break;
            }
            rt_thread_mdelay(10);
        }
        if (got_rx)
        {
            (void)emu_console_tx((unsigned char)'<');
            (void)emu_console_tx(rx);
            (void)emu_console_tx((unsigned char)'>');
            (void)emu_console_tx((unsigned char)'\n');
            rt_kprintf("emu_console rx ok: 0x%02X\n", (unsigned int)rx);
        }
        else
        {
            rt_kprintf("emu_console rx: no byte received (expected for an unattended run)\n");
        }
        emu_console_shutdown();
    }
    else
    {
        rt_kprintf("emu_console init FAILED\n");
    }

    rt_kprintf("probe: %s\n",
               (errors_psram || errors_iram || errors_floppy || !console_open)
                   ? "FAIL"
                   : "PASS");

    rt_memheap_free(guest_ram);
    rt_memheap_free(floppy);
    return (errors_psram || errors_iram || errors_floppy || !console_open) ? -1 : 0;
}
MSH_CMD_EXPORT(psram_smoke, SF32LB525 PSRAM + UART console bring-up probe);
