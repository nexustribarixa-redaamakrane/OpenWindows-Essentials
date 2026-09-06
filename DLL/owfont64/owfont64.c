/*
 * owfont64.c - OpenWindows Font Rendering Engine (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owfont64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owfont64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owfont_init(void)
{
    (void)owfont64_ready;
    /* TODO: implement owfont_init */
}
void owfont_load(void)
{
    (void)owfont64_ready;
    /* TODO: implement owfont_load */
}
void owfont_render_glyph(void)
{
    (void)owfont64_ready;
    /* TODO: implement owfont_render_glyph */
}
void owfont_measure_text(void)
{
    (void)owfont64_ready;
    /* TODO: implement owfont_measure_text */
}
void owfont_set_size(void)
{
    (void)owfont64_ready;
    /* TODO: implement owfont_set_size */
}

