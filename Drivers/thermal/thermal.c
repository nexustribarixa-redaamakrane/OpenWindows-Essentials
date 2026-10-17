/*
 * thermal.c - OpenWindows Thermal Monitor Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * The driver owns a static table of zones.  Each zone carries a sampling hook
 * supplied by the caller (for example an MSR read or an embedded-controller
 * transfer) and reports temperatures in milli-degrees Celsius.
 */

#include "thermal.h"

/* ── Internal state ───────────────────────────────────────────── */

static thermal_zone_t thermal_zones[THERMAL_MAX_ZONES];
static uint32_t       thermal_zone_count;
static bool           thermal_initialized;

/* ── Helpers ──────────────────────────────────────────────────── */

/* Small freestanding clear; avoids pulling in <string.h>. */
static void thermal_clear(void *dst, uint32_t count)
{
    uint8_t *p = (uint8_t *)dst;
    uint32_t i;

    for (i = 0u; i < count; ++i) {
        p[i] = 0u;
    }
}

/* Constrain a milli-degree reading to the physical range. */
static int32_t thermal_clamp_mdeg(int32_t value)
{
    if (value < THERMAL_MDEG_MIN) {
        return THERMAL_MDEG_MIN;
    }
    if (value > THERMAL_MDEG_MAX) {
        return THERMAL_MDEG_MAX;
    }
    return value;
}

/* ── Implementation ───────────────────────────────────────────── */

bool thermal_init(void)
{
    thermal_clear(thermal_zones, (uint32_t)sizeof(thermal_zones));
    thermal_zone_count = 0u;
    thermal_initialized = true;
    return true;
}

int32_t thermal_register_zone(const char *name, thermal_read_fn read, void *context)
{
    uint32_t index;
    thermal_zone_t *zone;

    if (!thermal_initialized) {
        (void)thermal_init();
    }
    if (!read) {
        return -1;
    }
    if (thermal_zone_count >= THERMAL_MAX_ZONES) {
        return -1;
    }

    index = thermal_zone_count;
    zone = &thermal_zones[index];
    zone->name = name;
    zone->read = read;
    zone->context = context;
    zone->threshold_mdeg = THERMAL_MDEG_MAX;
    zone->last_mdeg = 0;
    zone->present = true;
    zone->alert = false;
    thermal_zone_count++;

    return (int32_t)index;
}

bool thermal_read_temp(uint32_t zone, int32_t *out_mdeg)
{
    thermal_zone_t *entry;
    int32_t sample = 0;

    if (!out_mdeg) {
        return false;
    }
    if (zone >= thermal_zone_count) {
        return false;
    }

    entry = &thermal_zones[zone];
    if (!entry->present || !entry->read) {
        return false;
    }
    if (!entry->read(entry->context, zone, &sample)) {
        return false;
    }

    sample = thermal_clamp_mdeg(sample);
    entry->last_mdeg = sample;
    entry->alert = (sample >= entry->threshold_mdeg);
    *out_mdeg = sample;
    return true;
}

bool thermal_set_threshold(uint32_t zone, int32_t threshold_mdeg)
{
    if (zone >= thermal_zone_count) {
        return false;
    }
    if (!thermal_zones[zone].present) {
        return false;
    }

    thermal_zones[zone].threshold_mdeg = thermal_clamp_mdeg(threshold_mdeg);
    return true;
}

const thermal_zone_t *thermal_get_zone(uint32_t zone)
{
    if (zone >= thermal_zone_count) {
        return NULL;
    }
    if (!thermal_zones[zone].present) {
        return NULL;
    }
    return &thermal_zones[zone];
}
