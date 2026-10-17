/*
 * battery.c - OpenWindows Battery/ACPI Battery Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * The platform hook returns a raw ACPI/EC sample; this module derives the
 * charge percentage, the aggregate status and the estimated runtime, keeping
 * the last good reading in static storage so the query API is allocation
 * free and predictable.
 */

#include "battery.h"

/* ── Limits ───────────────────────────────────────────────────── */

#define BATT_TIME_MAX_MINUTES   0xFFFFu  /* saturate the reported runtime */

/* ── Internal state ───────────────────────────────────────────── */

typedef struct {
    bool          initialized;
    batt_read_fn  read;
    void         *context;
    bool          have_sample;
    batt_sample_t sample;
    uint32_t      level_percent;
    uint32_t      time_remain_min;
    batt_status_t status;
} batt_state_t;

static batt_state_t batt;

/* ── Helpers ──────────────────────────────────────────────────── */

/* Clamp an unsigned value to an inclusive maximum. */
static uint32_t batt_clamp_u32(uint32_t value, uint32_t maximum)
{
    if (value > maximum) {
        return maximum;
    }
    return value;
}

/* Derive status / level / runtime from the raw sample. */
static void batt_derive(void)
{
    const batt_sample_t *s = &batt.sample;

    if (!s->present) {
        batt.status = BATT_STATUS_NOT_PRESENT;
        batt.level_percent = 0u;
        batt.time_remain_min = 0u;
        return;
    }
    if (s->design_mah == 0u) {
        batt.status = BATT_STATUS_UNKNOWN;
        batt.level_percent = 0u;
        batt.time_remain_min = 0u;
        return;
    }

    batt.level_percent = (s->remaining_mah * 100u) / s->design_mah;
    batt.level_percent = batt_clamp_u32(batt.level_percent, 100u);

    if (s->charging && s->remaining_mah < s->design_mah) {
        batt.status = BATT_STATUS_CHARGING;
    } else if (s->charging || s->ac_online || (s->remaining_mah >= s->design_mah)) {
        batt.status = BATT_STATUS_FULL;
    } else {
        batt.status = BATT_STATUS_DISCHARGING;
    }

    if (s->rate_ma == 0u) {
        batt.time_remain_min = 0u;
    } else {
        uint32_t minutes = (s->remaining_mah * 60u) / s->rate_ma;
        batt.time_remain_min = batt_clamp_u32(minutes, BATT_TIME_MAX_MINUTES);
    }
}

/* ── Implementation ───────────────────────────────────────────── */

bool batt_init(batt_read_fn read, void *context)
{
    batt.read = read;
    batt.context = context;
    batt.have_sample = false;
    batt.level_percent = 0u;
    batt.time_remain_min = 0u;
    batt.status = BATT_STATUS_UNKNOWN;
    batt.initialized = true;

    if (read) {
        (void)batt_refresh();
    }
    return true;
}

bool batt_refresh(void)
{
    batt_sample_t fresh;

    if (!batt.initialized || !batt.read) {
        return false;
    }
    if (!batt.read(batt.context, &fresh)) {
        return false;
    }

    batt.sample = fresh;
    batt.have_sample = true;
    batt_derive();
    return true;
}

bool batt_get_level(uint32_t *out_percent)
{
    if (!out_percent) {
        return false;
    }
    if (!batt_refresh()) {
        return false;
    }
    *out_percent = batt.level_percent;
    return true;
}

batt_status_t batt_get_status(void)
{
    if (!batt_refresh()) {
        return BATT_STATUS_UNKNOWN;
    }
    return batt.status;
}

bool batt_get_time_remain(uint32_t *out_minutes)
{
    if (!out_minutes) {
        return false;
    }
    if (!batt_refresh()) {
        return false;
    }
    *out_minutes = batt.time_remain_min;
    return batt.status != BATT_STATUS_NOT_PRESENT;
}

const batt_sample_t *batt_sample(void)
{
    if (!batt.have_sample) {
        return NULL;
    }
    return &batt.sample;
}
