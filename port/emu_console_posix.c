/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 */

#include "emu_console.h"
#include "emu_meta.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

static int saved_flags = -1;
static struct termios saved_termios;
static int restore_terminal;
static unsigned char pending_byte;
static int has_pending_byte;

int emu_console_init(void)
{
    struct termios raw;

    saved_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (saved_flags >= 0)
        (void)fcntl(STDIN_FILENO, F_SETFL, saved_flags | O_NONBLOCK);

    if (tcgetattr(STDIN_FILENO, &saved_termios) == 0)
    {
        raw = saved_termios;
        cfmakeraw(&raw);
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        restore_terminal = 1;
    }
    return 0;
}

int emu_console_rx(unsigned char *out)
{
    unsigned char next;
    int waited;
    ssize_t n;

    if (has_pending_byte)
    {
        *out = pending_byte;
        has_pending_byte = 0;
        return 1;
    }

    do
        n = read(STDIN_FILENO, out, 1);
    while (n < 0 && errno == EINTR);

    if (n != 1)
        return 0;

    if (*out != EMU_META_ESC)
        return 1;

    /* Wait briefly for the key a terminal sends after ESC (Alt+key). */
    for (waited = 0; waited < EMU_META_TIMEOUT_MS; waited++)
    {
        do
            n = read(STDIN_FILENO, &next, 1);
        while (n < 0 && errno == EINTR);

        if (n == 1)
        {
            pending_byte = next;
            has_pending_byte = 1;
            if (emu_meta_is_alt_pair(next))
            {
                *out = EMU_META_ALT_PREFIX;
                return 1;
            }
            return 1; /* plain ESC; the queued byte follows next call */
        }
        usleep(1000);
    }

    return 1; /* lone ESC */
}

int emu_console_tx(unsigned char ch)
{
    ssize_t n;

    do
        n = write(STDOUT_FILENO, &ch, 1);
    while (n < 0 && errno == EINTR);

    return n == 1 ? 0 : -1;
}

void emu_console_shutdown(void)
{
    if (restore_terminal)
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
    if (saved_flags >= 0)
        (void)fcntl(STDIN_FILENO, F_SETFL, saved_flags);
}
