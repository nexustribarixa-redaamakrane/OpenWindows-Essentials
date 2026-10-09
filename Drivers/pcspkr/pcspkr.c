/*
 * pcspkr.c - OpenWindows PC Speaker Beep Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "pcspkr.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool pcspkr_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void pcspkr_init(void)
{
    if (!pcspkr_initialized) { return; }
    /* TODO: implement pcspkr_init */
    (void)0;
}
void pcspkr_beep(void)
{
    if (!pcspkr_initialized) { return; }
    /* TODO: implement pcspkr_beep */
    (void)0;
}
void pcspkr_set_freq(void)
{
    if (!pcspkr_initialized) { return; }
    /* TODO: implement pcspkr_set_freq */
    (void)0;
}
void pcspkr_stop(void)
{
    if (!pcspkr_initialized) { return; }
    /* TODO: implement pcspkr_stop */
    (void)0;
}

