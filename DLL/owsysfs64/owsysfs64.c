/*
 * owsysfs64.c - OpenWindows System Filesystem Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owsysfs64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owsysfs64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owsysfs_init(void)
{
    (void)owsysfs64_ready;
    /* TODO: implement owsysfs_init */
}
void owsysfs_read(void)
{
    (void)owsysfs64_ready;
    /* TODO: implement owsysfs_read */
}
void owsysfs_write(void)
{
    (void)owsysfs64_ready;
    /* TODO: implement owsysfs_write */
}
void owsysfs_enumerate(void)
{
    (void)owsysfs64_ready;
    /* TODO: implement owsysfs_enumerate */
}
void owsysfs_get_attr(void)
{
    (void)owsysfs64_ready;
    /* TODO: implement owsysfs_get_attr */
}

