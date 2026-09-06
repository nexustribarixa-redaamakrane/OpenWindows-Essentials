/*
 * owbitmap64.c - OpenWindows Bitmap Allocator (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owbitmap64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owbitmap64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owbmp_init(void)
{
    (void)owbitmap64_ready;
    /* TODO: implement owbmp_init */
}
void owbmp_alloc(void)
{
    (void)owbitmap64_ready;
    /* TODO: implement owbmp_alloc */
}
void owbmp_free(void)
{
    (void)owbitmap64_ready;
    /* TODO: implement owbmp_free */
}
void owbmp_test(void)
{
    (void)owbitmap64_ready;
    /* TODO: implement owbmp_test */
}
void owbmp_find_first_free(void)
{
    (void)owbitmap64_ready;
    /* TODO: implement owbmp_find_first_free */
}

