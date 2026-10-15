/*
 * libc gaps of newlib/RT-Thread that 8086tiny relies on.
 *
 * 8086tiny calls ftime() for the emulated PC's real-time-clock opcode
 * (0F 01 / GET_RTC).  newlib does not provide ftime(), so the port layer
 * supplies it from the RT-Thread tick plus the libc clock.
 * This file is only built for the RT-Thread target.
 */
#include <sys/timeb.h>
#include <time.h>

#include <rtthread.h>

int ftime(struct timeb *tp)
{
    if (tp == RT_NULL)
        return -1;

    tp->time = (time_t)time(RT_NULL);
    tp->millitm = (unsigned short)(rt_tick_get() % 1000u);
    tp->timezone = 0;
    tp->dstflag = 0;
    return 0;
}
