/*
 * thermal.c - OpenWindows Thermal Monitor Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "thermal.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool thermal_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void thermal_init(void)
{
    if (!thermal_initialized) { return; }
    /* TODO: implement thermal_init */
    (void)0;
}
void thermal_read_temp(void)
{
    if (!thermal_initialized) { return; }
    /* TODO: implement thermal_read_temp */
    (void)0;
}
void thermal_set_threshold(void)
{
    if (!thermal_initialized) { return; }
    /* TODO: implement thermal_set_threshold */
    (void)0;
}
void thermal_get_zone(void)
{
    if (!thermal_initialized) { return; }
    /* TODO: implement thermal_get_zone */
    (void)0;
}

