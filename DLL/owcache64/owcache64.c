/*
 * owcache64.c - OpenWindows Block Cache Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owcache64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owcache64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owcache_init(void)
{
    (void)owcache64_ready;
    /* TODO: implement owcache_init */
}
void owcache_lookup(void)
{
    (void)owcache64_ready;
    /* TODO: implement owcache_lookup */
}
void owcache_insert(void)
{
    (void)owcache64_ready;
    /* TODO: implement owcache_insert */
}
void owcache_evict(void)
{
    (void)owcache64_ready;
    /* TODO: implement owcache_evict */
}
void owcache_flush(void)
{
    (void)owcache64_ready;
    /* TODO: implement owcache_flush */
}

