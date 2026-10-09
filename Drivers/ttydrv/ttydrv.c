/*
 * ttydrv.c - OpenWindows TTY Console Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ttydrv.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool ttydrv_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void tty_init(void)
{
    if (!ttydrv_initialized) { return; }
    /* TODO: implement tty_init */
    (void)0;
}
void tty_write_char(void)
{
    if (!ttydrv_initialized) { return; }
    /* TODO: implement tty_write_char */
    (void)0;
}
void tty_write_string(void)
{
    if (!ttydrv_initialized) { return; }
    /* TODO: implement tty_write_string */
    (void)0;
}
void tty_clear(void)
{
    if (!ttydrv_initialized) { return; }
    /* TODO: implement tty_clear */
    (void)0;
}
void tty_scroll(void)
{
    if (!ttydrv_initialized) { return; }
    /* TODO: implement tty_scroll */
    (void)0;
}

