/*
 * owheap64.c - OpenWindows Managed Heap Allocator (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owheap64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owheap64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owheap_init(void)
{
    (void)owheap64_ready;
    /* TODO: implement owheap_init */
}
void owheap_alloc(void)
{
    (void)owheap64_ready;
    /* TODO: implement owheap_alloc */
}
void owheap_free(void)
{
    (void)owheap64_ready;
    /* TODO: implement owheap_free */
}
void owheap_realloc(void)
{
    (void)owheap64_ready;
    /* TODO: implement owheap_realloc */
}
void owheap_stats(void)
{
    (void)owheap64_ready;
    /* TODO: implement owheap_stats */
}

