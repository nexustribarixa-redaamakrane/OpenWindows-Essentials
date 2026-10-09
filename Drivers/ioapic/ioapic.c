/*
 * ioapic.c - OpenWindows I/O APIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ioapic.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool ioapic_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void ioapic_init(void)
{
    if (!ioapic_initialized) { return; }
    /* TODO: implement ioapic_init */
    (void)0;
}
void ioapic_set_irq(void)
{
    if (!ioapic_initialized) { return; }
    /* TODO: implement ioapic_set_irq */
    (void)0;
}
void ioapic_mask(void)
{
    if (!ioapic_initialized) { return; }
    /* TODO: implement ioapic_mask */
    (void)0;
}
void ioapic_unmask(void)
{
    if (!ioapic_initialized) { return; }
    /* TODO: implement ioapic_unmask */
    (void)0;
}
void ioapic_eoi(void)
{
    if (!ioapic_initialized) { return; }
    /* TODO: implement ioapic_eoi */
    (void)0;
}

