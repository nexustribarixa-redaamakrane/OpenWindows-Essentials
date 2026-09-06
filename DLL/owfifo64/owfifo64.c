/*
 * owfifo64.c - OpenWindows FIFO Ring Buffer Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owfifo64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owfifo64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owfifo_init(void)
{
    (void)owfifo64_ready;
    /* TODO: implement owfifo_init */
}
void owfifo_push(void)
{
    (void)owfifo64_ready;
    /* TODO: implement owfifo_push */
}
void owfifo_pop(void)
{
    (void)owfifo64_ready;
    /* TODO: implement owfifo_pop */
}
void owfifo_peek(void)
{
    (void)owfifo64_ready;
    /* TODO: implement owfifo_peek */
}
void owfifo_count(void)
{
    (void)owfifo64_ready;
    /* TODO: implement owfifo_count */
}
void owfifo_is_empty(void)
{
    (void)owfifo64_ready;
    /* TODO: implement owfifo_is_empty */
}

