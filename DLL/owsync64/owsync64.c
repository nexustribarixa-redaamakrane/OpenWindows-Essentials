/*
 * owsync64.c - OpenWindows Synchronization Primitives (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owsync64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owsync64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owsync_spinlock_init(void)
{
    (void)owsync64_ready;
    /* TODO: implement owsync_spinlock_init */
}
void owsync_spinlock_acquire(void)
{
    (void)owsync64_ready;
    /* TODO: implement owsync_spinlock_acquire */
}
void owsync_spinlock_release(void)
{
    (void)owsync64_ready;
    /* TODO: implement owsync_spinlock_release */
}
void owsync_mutex_init(void)
{
    (void)owsync64_ready;
    /* TODO: implement owsync_mutex_init */
}
void owsync_mutex_lock(void)
{
    (void)owsync64_ready;
    /* TODO: implement owsync_mutex_lock */
}
void owsync_mutex_unlock(void)
{
    (void)owsync64_ready;
    /* TODO: implement owsync_mutex_unlock */
}
void owsync_barrier(void)
{
    (void)owsync64_ready;
    /* TODO: implement owsync_barrier */
}

