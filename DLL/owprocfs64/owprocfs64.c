/*
 * owprocfs64.c - OpenWindows Process Filesystem Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owprocfs64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owprocfs64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owprocfs_init(void)
{
    (void)owprocfs64_ready;
    /* TODO: implement owprocfs_init */
}
void owprocfs_read_pid(void)
{
    (void)owprocfs64_ready;
    /* TODO: implement owprocfs_read_pid */
}
void owprocfs_list(void)
{
    (void)owprocfs64_ready;
    /* TODO: implement owprocfs_list */
}
void owprocfs_get_stat(void)
{
    (void)owprocfs64_ready;
    /* TODO: implement owprocfs_get_stat */
}
void owprocfs_get_mem(void)
{
    (void)owprocfs64_ready;
    /* TODO: implement owprocfs_get_mem */
}

