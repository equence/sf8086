/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 *
 * Console boundary for 8086tiny.
 *
 * The emulator only needs byte-oriented terminal I/O.  The target implementation
 * maps these calls to the board UART; the host implementation maps them to a
 * terminal so the same CPU/BIOS path can be regression-tested.
 */
#ifndef SF8086_EMU_CONSOLE_H
#define SF8086_EMU_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Return 0 when no byte is available, 1 after storing one byte in *out. */
int emu_console_rx(unsigned char *out);

/* Transmit one byte. Return 0 on success, negative on failure. */
int emu_console_tx(unsigned char ch);

/* Initialize/shutdown the byte stream. */
int emu_console_init(void);
void emu_console_shutdown(void);

/*
 * Console ownership, implemented by the RT-Thread backend only (host builds
 * use the POSIX backend and never call these).
 *
 * UART1 is shared: the RT-Thread shell reads it through stdio (fd 0 is bound to
 * /dev/uart1 at boot) and the emulator reads the same device directly, so
 * without arbitration the two readers split the keystrokes.
 *
 *   emu_console_take_over()  - stdio -> /dev/emu_idle (a device with no input),
 *                              so only the emulator reads UART1
 *   emu_console_hand_back()  - stdio -> /dev/uart1 again
 *
 * Both must be called from the shell thread, because switching stdio closes the
 * FILE the shell reads from.  The emulator thread only wakes the shell with an
 * internal command (see emu_idle_dev.h) and then parks on a semaphore.
 *
 * The escape byte (Ctrl-], 0x1D) is consumed by the backend and never reaches
 * the guest.
 */
int emu_console_take_over(void);
int emu_console_hand_back(void);
int emu_console_is_paused(void);
int emu_console_resume(void);
int emu_console_reboot(void);

/* Called (in the emulator thread) when a reboot was requested. */
void emu_console_set_reboot_hook(void (*hook)(void));

/* Restart the "DOS prompt reached" measurement (call before each guest boot). */
void emu_console_reset_prompt_report(void);

#ifdef __cplusplus
}
#endif

#endif
