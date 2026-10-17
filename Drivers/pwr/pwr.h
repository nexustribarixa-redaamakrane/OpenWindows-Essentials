/*
 * pwr.h - OpenWindows Power Management Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 * ACPI S1/S3/S5 transitions plus legacy 0xCF9 / 8042 reset.
 */

#ifndef PWR_H
#define PWR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── ACPI PM1 control register bits ───────────────────────────── */

#define PWR_SLP_TYP_SHIFT   10u      /* SLP_TYP occupies bits 12:10      */
#define PWR_SLP_TYP_MASK    0x1C00u
#define PWR_SLP_EN          0x2000u  /* SLP_EN: trigger the transition   */

/* ── Enumerations ─────────────────────────────────────────────── */

/* Last requested / observed power transition. */
typedef enum {
    PWR_STATE_RUNNING = 0,
    PWR_STATE_SLEEP_S1,
    PWR_STATE_SLEEP_S3,
    PWR_STATE_SHUTDOWN,
    PWR_STATE_REBOOT,
    PWR_STATE_UNKNOWN
} pwr_state_t;

/* Sleep depths understood by pwr_sleep(). */
typedef enum {
    PWR_SLEEP_S1 = 1,
    PWR_SLEEP_S3 = 3
} pwr_sleep_mode_t;

/* Platform description of the ACPI power registers. */
typedef struct {
    uint16_t pm1a_control;      /* PM1a_CNT_BLK port (0 = absent)      */
    uint16_t pm1b_control;      /* PM1b_CNT_BLK port (0 = absent)      */
    uint8_t  slp_typ_s1;        /* SLP_TYP value for S1                */
    uint8_t  slp_typ_s3;        /* SLP_TYP value for S3                */
    uint8_t  slp_typ_s5;        /* SLP_TYP value for S5 (soft off)     */
    bool     prefer_kbc_reset;  /* use the 8042 reset pulse to reboot  */
} pwr_config_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Install the platform register description. */
bool pwr_init(const pwr_config_t *config);

/* Enter ACPI S5 (soft off); returns false if no control port exists. */
bool pwr_shutdown(void);

/* Reset the machine via 0xCF9 or the 8042 controller. */
bool pwr_reboot(void);

/* Enter the requested ACPI sleep state (S1 or S3). */
bool pwr_sleep(pwr_sleep_mode_t mode);

/* Report the last power action requested. */
pwr_state_t pwr_get_state(void);

#endif /* PWR_H */
