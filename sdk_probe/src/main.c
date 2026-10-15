/* Application entry for the 8086tiny bring-up probe firmware. */
#include <rtthread.h>

int main(void)
{
    rt_kprintf("\nsf8086 probe firmware running on SF32LB525 (黄山派)\n");
    rt_kprintf("type 'psram_smoke' in the shell to run the guest-RAM/console probe\n");
    return 0;
}
