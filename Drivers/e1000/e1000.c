/*
 * e1000.c - OpenWindows Intel e1000 NIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "e1000.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool e1000_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void e1000_init(void)
{
    if (!e1000_initialized) { return; }
    /* TODO: implement e1000_init */
    (void)0;
}
void e1000_send(void)
{
    if (!e1000_initialized) { return; }
    /* TODO: implement e1000_send */
    (void)0;
}
void e1000_recv(void)
{
    if (!e1000_initialized) { return; }
    /* TODO: implement e1000_recv */
    (void)0;
}
void e1000_get_mac(void)
{
    if (!e1000_initialized) { return; }
    /* TODO: implement e1000_get_mac */
    (void)0;
}

