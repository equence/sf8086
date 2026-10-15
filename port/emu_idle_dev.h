/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 *
 * "emu_idle": a character device with no input of its own.
 *
 * While the emulator owns UART1, the RT-Thread shell is re-pointed at this
 * device so that its blocking read parks there instead of stealing guest
 * keystrokes.  Handing UART1 back is done by waking that parked read.
 */
#ifndef SF8086_EMU_IDLE_DEV_H
#define SF8086_EMU_IDLE_DEV_H

#ifdef __cplusplus
extern "C" {
#endif

/* Register the device (idempotent). Returns 0 on success. */
int emu_idle_device_create(void);

/* Clear any queued text and stale wake-ups; called when the console is taken. */
void emu_idle_device_reset(void);

/*
 * Queue the internal command line and let a parked read deliver it to the
 * shell one byte at a time.  This is how the shell is woken after Ctrl-]
 * without touching stdio from the emulator thread.
 */
void emu_idle_device_wake(void);

#ifdef __cplusplus
}
#endif

#endif
