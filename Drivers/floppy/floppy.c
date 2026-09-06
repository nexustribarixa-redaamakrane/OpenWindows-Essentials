/*
 * floppy.c - OpenWindows Floppy Disk Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "floppy.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool floppy_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void floppy_init(void)
{
    if (!floppy_initialized) { return; }
    /* TODO: implement floppy_init */
    (void)0;
}
void floppy_read(void)
{
    if (!floppy_initialized) { return; }
    /* TODO: implement floppy_read */
    (void)0;
}
void floppy_write(void)
{
    if (!floppy_initialized) { return; }
    /* TODO: implement floppy_write */
    (void)0;
}
void floppy_seek(void)
{
    if (!floppy_initialized) { return; }
    /* TODO: implement floppy_seek */
    (void)0;
}
void floppy_reset(void)
{
    if (!floppy_initialized) { return; }
    /* TODO: implement floppy_reset */
    (void)0;
}

