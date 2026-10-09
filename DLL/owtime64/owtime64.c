/*
 * owtime64.c - OpenWindows Time Management Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owtime64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owtime64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owtime_init(void)
{
    (void)owtime64_ready;
    /* TODO: implement owtime_init */
}
void owtime_get_ticks(void)
{
    (void)owtime64_ready;
    /* TODO: implement owtime_get_ticks */
}
void owtime_sleep_ms(void)
{
    (void)owtime64_ready;
    /* TODO: implement owtime_sleep_ms */
}
void owtime_get_uptime(void)
{
    (void)owtime64_ready;
    /* TODO: implement owtime_get_uptime */
}
void owtime_set_epoch(void)
{
    (void)owtime64_ready;
    /* TODO: implement owtime_set_epoch */
}

