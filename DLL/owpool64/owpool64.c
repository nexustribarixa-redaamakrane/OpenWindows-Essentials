/*
 * owpool64.c - OpenWindows Memory Pool Allocator (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owpool64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owpool64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owpool_init(void)
{
    (void)owpool64_ready;
    /* TODO: implement owpool_init */
}
void owpool_alloc(void)
{
    (void)owpool64_ready;
    /* TODO: implement owpool_alloc */
}
void owpool_free(void)
{
    (void)owpool64_ready;
    /* TODO: implement owpool_free */
}
void owpool_reset(void)
{
    (void)owpool64_ready;
    /* TODO: implement owpool_reset */
}
void owpool_stats(void)
{
    (void)owpool64_ready;
    /* TODO: implement owpool_stats */
}

