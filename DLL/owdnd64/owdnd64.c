/*
 * owdnd64.c - OpenWindows Drag and Drop Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owdnd64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owdnd64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owdnd_init(void)
{
    (void)owdnd64_ready;
    /* TODO: implement owdnd_init */
}
void owdnd_begin_drag(void)
{
    (void)owdnd64_ready;
    /* TODO: implement owdnd_begin_drag */
}
void owdnd_drop(void)
{
    (void)owdnd64_ready;
    /* TODO: implement owdnd_drop */
}
void owdnd_register_target(void)
{
    (void)owdnd64_ready;
    /* TODO: implement owdnd_register_target */
}
void owdnd_get_data(void)
{
    (void)owdnd64_ready;
    /* TODO: implement owdnd_get_data */
}

