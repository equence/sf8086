/*
 * KEYTEST - show what the guest actually receives for each key press.
 *
 * Useful when a key combination "does not work" in a DOS program: it prints the
 * BIOS scancode/ASCII pair from INT 16h and the BIOS shift flags, which is
 * exactly what a DOS application sees.
 *
 * Note on Alt: 8086tiny is fed ASCII characters from a serial terminal, which
 * cannot say "Alt is held".  Its BIOS therefore accepts Ctrl+A as an Alt prefix
 * (Ctrl+A then X means Alt+X), and the port translates a terminal's ESC+X
 * (the usual Meta encoding) into that prefix.  Alt+key presses arrive with an
 * ASCII code of 0, as they do on a real PC.
 *
 * Ctrl+C quits.
 */
#include <stdio.h>
#include <bios.h>
#include <dos.h>

int main(void)
{
    unsigned key;
    unsigned char far *flags = (unsigned char far *)MK_FP(0x40, 0x17);

    printf("KEYTEST: press keys (Ctrl+C quits)\n");
    printf("Alt over the serial line is: Ctrl+A then the key\n\n");

    for (;;)
    {
        key = bioskey(0);
        printf("scancode %02X  ascii %02X%s  flags[40:17]=%02X%s%s%s\n",
               key >> 8, key & 0xFF,
               (key & 0xFF) ? "" : " (extended)",
               *flags,
               (*flags & 0x08) ? " ALT" : "",
               (*flags & 0x04) ? " CTRL" : "",
               (*flags & 0x03) ? " SHIFT" : "");

        if ((key & 0xFF) == 3)
            break;
    }
    printf("KEYTEST: done\n");
    return 0;
}
