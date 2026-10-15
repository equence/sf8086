/*
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

#ifdef __cplusplus
}
#endif

#endif
