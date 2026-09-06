/*
 * owdev64.c - OpenWindows Device Manager Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owdev64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owdev64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owdev_init(void)
{
    (void)owdev64_ready;
    /* TODO: implement owdev_init */
}
void owdev_register(void)
{
    (void)owdev64_ready;
    /* TODO: implement owdev_register */
}
void owdev_unregister(void)
{
    (void)owdev64_ready;
    /* TODO: implement owdev_unregister */
}
void owdev_find(void)
{
    (void)owdev64_ready;
    /* TODO: implement owdev_find */
}
void owdev_enumerate(void)
{
    (void)owdev64_ready;
    /* TODO: implement owdev_enumerate */
}
void owdev_get_info(void)
{
    (void)owdev64_ready;
    /* TODO: implement owdev_get_info */
}

