/*
 * owclip64.c - OpenWindows Clipboard Manager (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owclip64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owclip64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owclip_init(void)
{
    (void)owclip64_ready;
    /* TODO: implement owclip_init */
}
void owclip_copy(void)
{
    (void)owclip64_ready;
    /* TODO: implement owclip_copy */
}
void owclip_paste(void)
{
    (void)owclip64_ready;
    /* TODO: implement owclip_paste */
}
void owclip_clear(void)
{
    (void)owclip64_ready;
    /* TODO: implement owclip_clear */
}
void owclip_get_format(void)
{
    (void)owclip64_ready;
    /* TODO: implement owclip_get_format */
}

