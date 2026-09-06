/*
 * partmgr.c - OpenWindows Partition Manager (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "partmgr.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool partmgr_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void partmgr_init(void)
{
    if (!partmgr_initialized) { return; }
    /* TODO: implement partmgr_init */
    (void)0;
}
void partmgr_scan(void)
{
    if (!partmgr_initialized) { return; }
    /* TODO: implement partmgr_scan */
    (void)0;
}
void partmgr_read_mbr(void)
{
    if (!partmgr_initialized) { return; }
    /* TODO: implement partmgr_read_mbr */
    (void)0;
}
void partmgr_read_gpt(void)
{
    if (!partmgr_initialized) { return; }
    /* TODO: implement partmgr_read_gpt */
    (void)0;
}

