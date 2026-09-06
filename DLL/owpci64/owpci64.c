/*
 * owpci64.c - OpenWindows PCI Configuration Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owpci64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owpci64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owpci_read8(void)
{
    (void)owpci64_ready;
    /* TODO: implement owpci_read8 */
}
void owpci_read16(void)
{
    (void)owpci64_ready;
    /* TODO: implement owpci_read16 */
}
void owpci_read32(void)
{
    (void)owpci64_ready;
    /* TODO: implement owpci_read32 */
}
void owpci_write8(void)
{
    (void)owpci64_ready;
    /* TODO: implement owpci_write8 */
}
void owpci_write16(void)
{
    (void)owpci64_ready;
    /* TODO: implement owpci_write16 */
}
void owpci_write32(void)
{
    (void)owpci64_ready;
    /* TODO: implement owpci_write32 */
}
void owpci_find_device(void)
{
    (void)owpci64_ready;
    /* TODO: implement owpci_find_device */
}

