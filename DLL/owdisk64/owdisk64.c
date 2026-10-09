/*
 * owdisk64.c - OpenWindows Disk I/O Abstraction (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owdisk64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owdisk64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owdisk_init(void)
{
    (void)owdisk64_ready;
    /* TODO: implement owdisk_init */
}
void owdisk_read(void)
{
    (void)owdisk64_ready;
    /* TODO: implement owdisk_read */
}
void owdisk_write(void)
{
    (void)owdisk64_ready;
    /* TODO: implement owdisk_write */
}
void owdisk_flush(void)
{
    (void)owdisk64_ready;
    /* TODO: implement owdisk_flush */
}
void owdisk_get_info(void)
{
    (void)owdisk64_ready;
    /* TODO: implement owdisk_get_info */
}
void owdisk_enumerate(void)
{
    (void)owdisk64_ready;
    /* TODO: implement owdisk_enumerate */
}

