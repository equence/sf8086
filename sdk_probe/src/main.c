/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 *
 * Application entry for the 8086tiny bring-up probe firmware. */
#include <rtthread.h>

extern void reset_reason_report(void);

int main(void)
{
    reset_reason_report();
    rt_kprintf("\nsf8086 probe firmware running on SF32LB525 (黄山派)\n");
    rt_kprintf("type 'psram_smoke' in the shell to run the guest-RAM/console probe\n");
    return 0;
}
