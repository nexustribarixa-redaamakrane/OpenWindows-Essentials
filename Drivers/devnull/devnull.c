/*
 * devnull.c - OpenWindows Null/Zero Device Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "devnull.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool devnull_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void devnull_init(void)
{
    if (!devnull_initialized) { return; }
    /* TODO: implement devnull_init */
    (void)0;
}
void devnull_read(void)
{
    if (!devnull_initialized) { return; }
    /* TODO: implement devnull_read */
    (void)0;
}
void devnull_write(void)
{
    if (!devnull_initialized) { return; }
    /* TODO: implement devnull_write */
    (void)0;
}

