/*
 * owirq64.c - OpenWindows IRQ Management Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owirq64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owirq64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owirq_init(void)
{
    (void)owirq64_ready;
    /* TODO: implement owirq_init */
}
void owirq_register(void)
{
    (void)owirq64_ready;
    /* TODO: implement owirq_register */
}
void owirq_unregister(void)
{
    (void)owirq64_ready;
    /* TODO: implement owirq_unregister */
}
void owirq_enable(void)
{
    (void)owirq64_ready;
    /* TODO: implement owirq_enable */
}
void owirq_disable(void)
{
    (void)owirq64_ready;
    /* TODO: implement owirq_disable */
}
void owirq_acknowledge(void)
{
    (void)owirq64_ready;
    /* TODO: implement owirq_acknowledge */
}

