/*
 * battery.h - OpenWindows Battery/ACPI Battery Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 * ACPI / embedded-controller battery status with static state.
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Enumerations ─────────────────────────────────────────────── */

/* Aggregate battery status reported to the caller. */
typedef enum {
    BATT_STATUS_UNKNOWN = 0,
    BATT_STATUS_CHARGING,
    BATT_STATUS_DISCHARGING,
    BATT_STATUS_FULL,
    BATT_STATUS_NOT_PRESENT
} batt_status_t;

/* ── Types ────────────────────────────────────────────────────── */

/* Raw reading produced by the platform ACPI/EC hook. */
typedef struct {
    bool     present;         /* a battery is installed                   */
    bool     charging;        /* external power is charging the battery   */
    bool     ac_online;       /* AC adapter is connected                  */
    uint32_t voltage_mv;      /* present voltage, millivolts              */
    uint32_t design_mah;      /* design capacity, milliamps-hour          */
    uint32_t remaining_mah;   /* remaining capacity, milliamps-hour       */
    uint32_t rate_ma;         /* charge/discharge rate, milliamps         */
} batt_sample_t;

/* Refresh the raw reading; returns false when the EC/ACPI read fails. */
typedef bool (*batt_read_fn)(void *context, batt_sample_t *out);

/* ── API ──────────────────────────────────────────────────────── */

/* Install the read hook and take a first reading. */
bool batt_init(batt_read_fn read, void *context);

/* Refresh and return the charge level as a percentage. */
bool batt_get_level(uint32_t *out_percent);

/* Refresh and return the aggregate status. */
batt_status_t batt_get_status(void);

/* Refresh and return the estimated remaining runtime, in minutes. */
bool batt_get_time_remain(uint32_t *out_minutes);

/* Re-read the raw sample and recompute the derived fields. */
bool batt_refresh(void);

/* Borrow the most recent raw sample, or NULL. */
const batt_sample_t *batt_sample(void);

#endif /* BATTERY_H */
