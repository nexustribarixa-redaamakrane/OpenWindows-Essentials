/*
 * pcibridge.c - OpenWindows PCI Bus Bridge Enumerator (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "pcibridge.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool pcibridge_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void pcib_init(void)
{
    if (!pcibridge_initialized) { return; }
    /* TODO: implement pcib_init */
    (void)0;
}
void pcib_enumerate(void)
{
    if (!pcibridge_initialized) { return; }
    /* TODO: implement pcib_enumerate */
    (void)0;
}
void pcib_read_config(void)
{
    if (!pcibridge_initialized) { return; }
    /* TODO: implement pcib_read_config */
    (void)0;
}
void pcib_write_config(void)
{
    if (!pcibridge_initialized) { return; }
    /* TODO: implement pcib_write_config */
    (void)0;
}

