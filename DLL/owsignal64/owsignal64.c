/*
 * owsignal64.c - OpenWindows Signal Handling Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owsignal64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owsignal64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owsig_init(void)
{
    (void)owsignal64_ready;
    /* TODO: implement owsig_init */
}
void owsig_register(void)
{
    (void)owsignal64_ready;
    /* TODO: implement owsig_register */
}
void owsig_raise(void)
{
    (void)owsignal64_ready;
    /* TODO: implement owsig_raise */
}
void owsig_mask(void)
{
    (void)owsignal64_ready;
    /* TODO: implement owsig_mask */
}
void owsig_pending(void)
{
    (void)owsignal64_ready;
    /* TODO: implement owsig_pending */
}

