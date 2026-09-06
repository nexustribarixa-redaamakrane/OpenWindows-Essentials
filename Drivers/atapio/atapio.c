/*
 * atapio.c - OpenWindows ATA PIO Mode Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "atapio.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool atapio_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void ata_init(void)
{
    if (!atapio_initialized) { return; }
    /* TODO: implement ata_init */
    (void)0;
}
void ata_identify(void)
{
    if (!atapio_initialized) { return; }
    /* TODO: implement ata_identify */
    (void)0;
}
void ata_read_sectors(void)
{
    if (!atapio_initialized) { return; }
    /* TODO: implement ata_read_sectors */
    (void)0;
}
void ata_write_sectors(void)
{
    if (!atapio_initialized) { return; }
    /* TODO: implement ata_write_sectors */
    (void)0;
}

