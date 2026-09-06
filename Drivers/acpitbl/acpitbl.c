/*
 * acpitbl.c - OpenWindows ACPI Table Parser (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "acpitbl.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool acpitbl_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void acpitbl_init(void)
{
    if (!acpitbl_initialized) { return; }
    /* TODO: implement acpitbl_init */
    (void)0;
}
void acpitbl_find_table(void)
{
    if (!acpitbl_initialized) { return; }
    /* TODO: implement acpitbl_find_table */
    (void)0;
}
void acpitbl_validate_rsdp(void)
{
    if (!acpitbl_initialized) { return; }
    /* TODO: implement acpitbl_validate_rsdp */
    (void)0;
}

