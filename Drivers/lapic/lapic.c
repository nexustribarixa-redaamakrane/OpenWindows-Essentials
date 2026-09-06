/*
 * lapic.c - OpenWindows Local APIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "lapic.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool lapic_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void lapic_init(void)
{
    if (!lapic_initialized) { return; }
    /* TODO: implement lapic_init */
    (void)0;
}
void lapic_send_ipi(void)
{
    if (!lapic_initialized) { return; }
    /* TODO: implement lapic_send_ipi */
    (void)0;
}
void lapic_eoi(void)
{
    if (!lapic_initialized) { return; }
    /* TODO: implement lapic_eoi */
    (void)0;
}
void lapic_timer_set(void)
{
    if (!lapic_initialized) { return; }
    /* TODO: implement lapic_timer_set */
    (void)0;
}

