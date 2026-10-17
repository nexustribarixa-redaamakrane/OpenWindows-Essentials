/*
 * thermal.h - OpenWindows Thermal Monitor Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 * Zone table fed by caller-supplied sampling hooks (MSR or EC backed).
 */

#ifndef THERMAL_H
#define THERMAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Limits ───────────────────────────────────────────────────── */

#define THERMAL_MAX_ZONES   8u        /* fixed static zone table         */
#define THERMAL_MDEG_MIN    (-40000)  /* -40.000 C                       */
#define THERMAL_MDEG_MAX    (150000)  /* +150.000 C                      */

/* ── Types ────────────────────────────────────────────────────── */

/* Sample one zone in milli-degrees Celsius. */
typedef bool (*thermal_read_fn)(void *context, uint32_t zone, int32_t *out_mdeg);

/* A single registered thermal zone. */
typedef struct {
    const char     *name;             /* static, caller-owned label      */
    thermal_read_fn read;             /* sampling hook                   */
    void           *context;          /* opaque hook context             */
    int32_t         threshold_mdeg;   /* alert threshold (milli-degrees) */
    int32_t         last_mdeg;        /* most recent sample              */
    bool            present;          /* slot is registered              */
    bool            alert;            /* last sample reached threshold   */
} thermal_zone_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Reset and clear the zone table. */
bool thermal_init(void);

/* Register a zone; returns its index, or -1 when the table is full. */
int32_t thermal_register_zone(const char *name, thermal_read_fn read, void *context);

/* Sample a zone into *out_mdeg; returns false for unknown zones. */
bool thermal_read_temp(uint32_t zone, int32_t *out_mdeg);

/* Set the alert threshold for a zone (clamped to the supported range). */
bool thermal_set_threshold(uint32_t zone, int32_t threshold_mdeg);

/* Borrow the descriptor of a registered zone, or NULL. */
const thermal_zone_t *thermal_get_zone(uint32_t zone);

#endif /* THERMAL_H */
