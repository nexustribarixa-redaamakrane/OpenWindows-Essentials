/*
 * owgfx64.c - OpenWindows Graphics Primitives Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owgfx64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owgfx64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owgfx_init(void)
{
    (void)owgfx64_ready;
    /* TODO: implement owgfx_init */
}
void owgfx_draw_line(void)
{
    (void)owgfx64_ready;
    /* TODO: implement owgfx_draw_line */
}
void owgfx_draw_rect(void)
{
    (void)owgfx64_ready;
    /* TODO: implement owgfx_draw_rect */
}
void owgfx_draw_circle(void)
{
    (void)owgfx64_ready;
    /* TODO: implement owgfx_draw_circle */
}
void owgfx_fill_rect(void)
{
    (void)owgfx64_ready;
    /* TODO: implement owgfx_fill_rect */
}
void owgfx_blit_bitmap(void)
{
    (void)owgfx64_ready;
    /* TODO: implement owgfx_blit_bitmap */
}

