#include "emu_console.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

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

    return n == 1 ? 0 : -1;
}

void emu_console_shutdown(void)
{
    if (restore_terminal)
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
    if (saved_flags >= 0)
        (void)fcntl(STDIN_FILENO, F_SETFL, saved_flags);
}
