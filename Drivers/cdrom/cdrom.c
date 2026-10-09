/*
 * cdrom.c - OpenWindows ATAPI CD-ROM Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "cdrom.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool cdrom_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void cdrom_init(void)
{
    if (!cdrom_initialized) { return; }
    /* TODO: implement cdrom_init */
    (void)0;
}
void cdrom_read_sector(void)
{
    if (!cdrom_initialized) { return; }
    /* TODO: implement cdrom_read_sector */
    (void)0;
}
void cdrom_eject(void)
{
    if (!cdrom_initialized) { return; }
    /* TODO: implement cdrom_eject */
    (void)0;
}
void cdrom_capacity(void)
{
    if (!cdrom_initialized) { return; }
    /* TODO: implement cdrom_capacity */
    (void)0;
}

