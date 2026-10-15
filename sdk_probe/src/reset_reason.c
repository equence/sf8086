/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 *
 * Report why the board last reset.
 *
 * The SF32LB52x PMU keeps the reset/wakeup source in PMUC_WSR, and the bits are
 * sticky, so the value read at boot says what caused *this* boot: watchdog,
 * low battery, power key, the reset/wakeup pins, charger events or the RTC.
 * Nothing has to be armed beforehand, which makes this the cheapest way to turn
 * "it reboots sometimes" into a concrete answer.
 *
 * The value is printed once at boot (first line in the log, so it is easy to
 * find after an unexplained reset) and kept in RAM for the `resetinfo` command.
 * Reading it does not clear it; the bits are cleared after reporting so the next
 * boot reports only its own cause.
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>

#include "bf0_hal_pmu.h"

#define RESET_SOURCE_MAX 112

static char reset_source[RESET_SOURCE_MAX] = "unknown";
static rt_uint32_t reset_source_raw;
static int reset_source_mode;

#ifndef PMUC_WSR_CHG
#define PMUC_WSR_CHG 0
#endif

static void append(char *buffer, const char *text)
{
    rt_size_t used = strlen(buffer);
    rt_size_t room = RESET_SOURCE_MAX - used;

    if (room > 1)
        strncat(buffer, text, room - 1);
}

void reset_reason_report(void)
{
    struct
    {
        rt_uint32_t mask;
        const char *text;
    } sources[] = {
        {PMUC_WSR_LOWBAT, "LOWBAT(battery/power)"},
        {PMUC_WSR_PWRKEY, "PWRKEY(power button)"},
        {PMUC_WSR_CHG, "CHG(charger/IPM)"},
        {PMUC_WSR_PIN0, "PIN0(reset/wakeup pin)"},
        {PMUC_WSR_PIN1, "PIN1(reset/wakeup pin)"},
        {PMUC_WSR_IWDT, "IWDT(independent watchdog)"},
        {PMUC_WSR_WDT1, "WDT1(watchdog)"},
        {PMUC_WSR_WDT2, "WDT2(watchdog)"},
        {PMUC_WSR_RTC, "RTC"},
    };
    rt_uint32_t wsr = hwp_pmuc->WSR;
    rt_size_t i;

    reset_source_raw = wsr;
    reset_source_mode = (int)SystemPowerOnModeGet();

    if (wsr == 0)
    {
        strncpy(reset_source, "cold boot (no PMU source latched)", sizeof(reset_source) - 1);
    }
    else
    {
        reset_source[0] = 0;
        for (i = 0; i < sizeof(sources) / sizeof(sources[0]); i++)
        {
            if (wsr & sources[i].mask)
            {
                if (reset_source[0] != 0)
                    append(reset_source, " + ");
                append(reset_source, sources[i].text);
            }
        }
        if (reset_source[0] == 0)
            strncpy(reset_source, "unrecognised PMUC_WSR bits", sizeof(reset_source) - 1);
    }

    rt_kprintf("\n[boot] reset source: %s\n", reset_source);
    rt_kprintf("[boot]   PMUC_WSR=0x%08X  power-on mode=%d\n", wsr, reset_source_mode);

    /* Clear so the next reset reports only its own cause. */
    HAL_PMU_CLEAR_WSR(wsr);
}

static int resetinfo(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    rt_kprintf("[reset] this boot: %s\n", reset_source);
    rt_kprintf("[reset]   PMUC_WSR at boot=0x%08X, now=0x%08X, mode=%d, uptime=%u ms\n",
               reset_source_raw, hwp_pmuc->WSR, reset_source_mode, rt_tick_get());
    rt_kprintf("[reset] note: a hard fault does NOT reset this board - it prints a\n"
               "        register dump and stops.  A reboot is always a PMU source.\n");
    return 0;
}
MSH_CMD_EXPORT(resetinfo, show why the board last reset);
