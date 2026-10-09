/*
 * fbdev.c - OpenWindows Framebuffer Device Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "fbdev.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool fbdev_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void fbdev_init(void)
{
    if (!fbdev_initialized) { return; }
    /* TODO: implement fbdev_init */
    (void)0;
}
void fbdev_putpixel(void)
{
    if (!fbdev_initialized) { return; }
    /* TODO: implement fbdev_putpixel */
    (void)0;
}
void fbdev_fill_rect(void)
{
    if (!fbdev_initialized) { return; }
    /* TODO: implement fbdev_fill_rect */
    (void)0;
}
void fbdev_blit(void)
{
    if (!fbdev_initialized) { return; }
    /* TODO: implement fbdev_blit */
    (void)0;
}
void fbdev_clear(void)
{
    if (!fbdev_initialized) { return; }
    /* TODO: implement fbdev_clear */
    (void)0;
}

