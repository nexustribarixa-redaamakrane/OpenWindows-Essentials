/*
 * hpet.c - OpenWindows HPET Timer Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "hpet.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool hpet_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void hpet_init(void)
{
    if (!hpet_initialized) { return; }
    /* TODO: implement hpet_init */
    (void)0;
}
void hpet_read_counter(void)
{
    if (!hpet_initialized) { return; }
    /* TODO: implement hpet_read_counter */
    (void)0;
}
void hpet_set_comparator(void)
{
    if (!hpet_initialized) { return; }
    /* TODO: implement hpet_set_comparator */
    (void)0;
}
void hpet_enable(void)
{
    if (!hpet_initialized) { return; }
    /* TODO: implement hpet_enable */
    (void)0;
}

