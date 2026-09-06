/*
 * owthread64.c - OpenWindows Thread Management Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owthread64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owthread64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owthread_create(void)
{
    (void)owthread64_ready;
    /* TODO: implement owthread_create */
}
void owthread_join(void)
{
    (void)owthread64_ready;
    /* TODO: implement owthread_join */
}
void owthread_detach(void)
{
    (void)owthread64_ready;
    /* TODO: implement owthread_detach */
}
void owthread_yield(void)
{
    (void)owthread64_ready;
    /* TODO: implement owthread_yield */
}
void owthread_exit(void)
{
    (void)owthread64_ready;
    /* TODO: implement owthread_exit */
}
void owthread_self(void)
{
    (void)owthread64_ready;
    /* TODO: implement owthread_self */
}

