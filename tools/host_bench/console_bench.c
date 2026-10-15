/*
 * Copyright (C) 2026 equence <equence@qq.com>
 * Part of sf8086, a port of 8086tiny to the SiFli SF32LB525 board.
 *
 * Host-side benchmark console backend.
 *
 * Behaves like port/emu_console_posix.c but stops the emulator as soon as the
 * guest prints the DOS prompt ("A:\>") and reports how many guest instructions
 * were executed.  Combined with wall-clock time this gives
 *   - instructions needed to boot FreeDOS to a prompt
 *   - host emulated instructions per second
 * which is the anchor for estimating the same figure on the SF32LB525.
 */
#include "emu_console.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/* Defined in 8086tiny.c */
extern unsigned int inst_counter;

static const char prompt[] = "A:\\>";
static unsigned int prompt_pos;
static int saved_flags = -1;
static struct termios saved_termios;
static int restore_terminal;

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
    ssize_t n;

    do
        n = read(STDIN_FILENO, out, 1);
    while (n < 0 && errno == EINTR);

    return n == 1 ? 1 : 0;
}

int emu_console_tx(unsigned char ch)
{
    ssize_t n;

    do
        n = write(STDOUT_FILENO, &ch, 1);
    while (n < 0 && errno == EINTR);

    if (ch == (unsigned char)prompt[prompt_pos])
    {
        if (prompt[++prompt_pos] == 0)
        {
            emu_console_shutdown();
            fprintf(stderr, "bench: prompt reached after %u guest instructions\n", inst_counter);
            exit(0);
        }
    }
    else
    {
        prompt_pos = (ch == (unsigned char)prompt[0]) ? 1u : 0u;
    }

    return n == 1 ? 0 : -1;
}

void emu_console_shutdown(void)
{
    if (restore_terminal)
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
    if (saved_flags >= 0)
        (void)fcntl(STDIN_FILENO, F_SETFL, saved_flags);
}
