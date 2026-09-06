/*
 * owreg64.c - OpenWindows Registry Access Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owreg64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owreg64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owreg_open(void)
{
    (void)owreg64_ready;
    /* TODO: implement owreg_open */
}
void owreg_close(void)
{
    (void)owreg64_ready;
    /* TODO: implement owreg_close */
}
void owreg_read(void)
{
    (void)owreg64_ready;
    /* TODO: implement owreg_read */
}
void owreg_write(void)
{
    (void)owreg64_ready;
    /* TODO: implement owreg_write */
}
void owreg_delete(void)
{
    (void)owreg64_ready;
    /* TODO: implement owreg_delete */
}
void owreg_enum_keys(void)
{
    (void)owreg64_ready;
    /* TODO: implement owreg_enum_keys */
}

