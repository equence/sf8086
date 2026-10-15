/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 *
 * Terminal Meta (Alt) translation for the 8086tiny port.
 *
 * 8086tiny's BIOS emulates an XT keyboard by converting ASCII characters from
 * the console into scancodes.  A serial line cannot express "Alt is held down",
 * so the BIOS instead accepts a prefix: ASCII 0x01 (Ctrl+A) means "the next key
 * has Alt held" (see bios.asm and the Keyboard Support section of doc.html).
 *
 * Terminals, on the other hand, send Alt+X as ESC followed by 'x' ("Meta sends
 * escape").  8086tiny's BIOS reads that ESC with its escape-sequence parser,
 * which is meant for cursor keys (ESC [ A ...): the ESC is swallowed, the
 * following key is not delivered as text, and what reaches the guest is not
 * Alt+X at all.
 *
 * This filter bridges the two conventions: a printable key arriving immediately
 * after ESC is reported as "Ctrl+A then key", i.e. Alt+key for the BIOS.  A lone
 * ESC (nothing follows within EMU_META_TIMEOUT_MS) still reaches the guest as a
 * plain ESC key, and ESC [ / ESC O sequences are passed through untouched so
 * cursor and function keys keep working.
 */
#ifndef SF8086_EMU_META_H
#define SF8086_EMU_META_H

#ifdef __cplusplus
extern "C" {
#endif

#define EMU_META_ESC 0x1B
#define EMU_META_ALT_PREFIX 0x01

/* How long to wait for the key that follows ESC (milliseconds). */
#define EMU_META_TIMEOUT_MS 40

/* True when ESC + `key` should be reported to the guest as Alt+key. */
static inline int emu_meta_is_alt_pair(unsigned char key)
{
    if (key < 0x20 || key > 0x7E)
        return 0; /* only printable ASCII carries a Meta meaning here */
    if (key == '[' || key == 'O')
        return 0; /* CSI and SS3 introducers: cursor keys, function keys */
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif
