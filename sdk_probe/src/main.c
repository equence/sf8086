/* Application entry for the 8086tiny bring-up probe firmware. */
#include <rtthread.h>

extern void reset_reason_report(void);

int main(void)
{
    reset_reason_report();
    rt_kprintf("\nsf8086 probe firmware running on SF32LB525 (黄山派)\n");
    rt_kprintf("type 'psram_smoke' in the shell to run the guest-RAM/console probe\n");
    return 0;
}
