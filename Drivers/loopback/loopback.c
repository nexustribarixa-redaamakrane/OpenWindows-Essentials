/*
 * loopback.c - OpenWindows Loopback Block Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "loopback.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool loopback_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void loop_init(void)
{
    if (!loopback_initialized) { return; }
    /* TODO: implement loop_init */
    (void)0;
}
void loop_attach(void)
{
    if (!loopback_initialized) { return; }
    /* TODO: implement loop_attach */
    (void)0;
}
void loop_detach(void)
{
    if (!loopback_initialized) { return; }
    /* TODO: implement loop_detach */
    (void)0;
}
void loop_read(void)
{
    if (!loopback_initialized) { return; }
    /* TODO: implement loop_read */
    (void)0;
}
void loop_write(void)
{
    if (!loopback_initialized) { return; }
    /* TODO: implement loop_write */
    (void)0;
}

