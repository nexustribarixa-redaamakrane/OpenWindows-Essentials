/*
 * kbdlayout.c - OpenWindows Keyboard Layout Manager (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kbdlayout.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool kbdlayout_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void kbl_init(void)
{
    if (!kbdlayout_initialized) { return; }
    /* TODO: implement kbl_init */
    (void)0;
}
void kbl_set_layout(void)
{
    if (!kbdlayout_initialized) { return; }
    /* TODO: implement kbl_set_layout */
    (void)0;
}
void kbl_scancode_to_char(void)
{
    if (!kbdlayout_initialized) { return; }
    /* TODO: implement kbl_scancode_to_char */
    (void)0;
}
void kbl_get_current(void)
{
    if (!kbdlayout_initialized) { return; }
    /* TODO: implement kbl_get_current */
    (void)0;
}

