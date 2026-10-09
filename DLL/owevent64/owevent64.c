/*
 * owevent64.c - OpenWindows Event Bus Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owevent64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owevent64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owevent_init(void)
{
    (void)owevent64_ready;
    /* TODO: implement owevent_init */
}
void owevent_subscribe(void)
{
    (void)owevent64_ready;
    /* TODO: implement owevent_subscribe */
}
void owevent_unsubscribe(void)
{
    (void)owevent64_ready;
    /* TODO: implement owevent_unsubscribe */
}
void owevent_publish(void)
{
    (void)owevent64_ready;
    /* TODO: implement owevent_publish */
}
void owevent_poll(void)
{
    (void)owevent64_ready;
    /* TODO: implement owevent_poll */
}

