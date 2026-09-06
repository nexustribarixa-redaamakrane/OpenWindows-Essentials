/*
 * vbedrv.c - OpenWindows VESA BIOS Extensions Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "vbedrv.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool vbedrv_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void vbe_init(void)
{
    if (!vbedrv_initialized) { return; }
    /* TODO: implement vbe_init */
    (void)0;
}
void vbe_set_mode(void)
{
    if (!vbedrv_initialized) { return; }
    /* TODO: implement vbe_set_mode */
    (void)0;
}
void vbe_get_framebuffer(void)
{
    if (!vbedrv_initialized) { return; }
    /* TODO: implement vbe_get_framebuffer */
    (void)0;
}
void vbe_get_info(void)
{
    if (!vbedrv_initialized) { return; }
    /* TODO: implement vbe_get_info */
    (void)0;
}

