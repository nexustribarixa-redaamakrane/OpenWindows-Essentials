/*
 * owmsg64.c - OpenWindows Message Queue Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owmsg64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owmsg64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owmsg_init(void)
{
    (void)owmsg64_ready;
    /* TODO: implement owmsg_init */
}
void owmsg_send(void)
{
    (void)owmsg64_ready;
    /* TODO: implement owmsg_send */
}
void owmsg_recv(void)
{
    (void)owmsg64_ready;
    /* TODO: implement owmsg_recv */
}
void owmsg_peek(void)
{
    (void)owmsg64_ready;
    /* TODO: implement owmsg_peek */
}
void owmsg_flush(void)
{
    (void)owmsg64_ready;
    /* TODO: implement owmsg_flush */
}

