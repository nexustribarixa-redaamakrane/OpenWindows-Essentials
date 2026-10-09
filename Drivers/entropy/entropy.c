/*
 * entropy.c - OpenWindows Hardware Entropy Source (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "entropy.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool entropy_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void entropy_init(void)
{
    if (!entropy_initialized) { return; }
    /* TODO: implement entropy_init */
    (void)0;
}
void entropy_read(void)
{
    if (!entropy_initialized) { return; }
    /* TODO: implement entropy_read */
    (void)0;
}
void entropy_available(void)
{
    if (!entropy_initialized) { return; }
    /* TODO: implement entropy_available */
    (void)0;
}
void entropy_seed(void)
{
    if (!entropy_initialized) { return; }
    /* TODO: implement entropy_seed */
    (void)0;
}

