/*
 * ahci.c - OpenWindows Advanced Host Controller Interface (SATA) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ahci.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool ahci_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void ahci_init(void)
{
    if (!ahci_initialized) { return; }
    /* TODO: implement ahci_init */
    (void)0;
}
void ahci_probe_ports(void)
{
    if (!ahci_initialized) { return; }
    /* TODO: implement ahci_probe_ports */
    (void)0;
}
void ahci_read_sectors(void)
{
    if (!ahci_initialized) { return; }
    /* TODO: implement ahci_read_sectors */
    (void)0;
}
void ahci_write_sectors(void)
{
    if (!ahci_initialized) { return; }
    /* TODO: implement ahci_write_sectors */
    (void)0;
}

