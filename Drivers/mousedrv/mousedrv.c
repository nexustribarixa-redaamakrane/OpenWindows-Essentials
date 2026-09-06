/*
 * mousedrv.c - OpenWindows Mouse Input Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "mousedrv.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool mousedrv_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void mouse_init(void)
{
    if (!mousedrv_initialized) { return; }
    /* TODO: implement mouse_init */
    (void)0;
}
void mouse_poll(void)
{
    if (!mousedrv_initialized) { return; }
    /* TODO: implement mouse_poll */
    (void)0;
}
void mouse_get_state(void)
{
    if (!mousedrv_initialized) { return; }
    /* TODO: implement mouse_get_state */
    (void)0;
}
void mouse_set_rate(void)
{
    if (!mousedrv_initialized) { return; }
    /* TODO: implement mouse_set_rate */
    (void)0;
}

