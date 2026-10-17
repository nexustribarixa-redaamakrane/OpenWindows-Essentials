/*
 * watchdog.h - OpenWindows Hardware Watchdog Timer (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 * Programs a register-level watchdog through a caller-supplied vtable.
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Configuration ────────────────────────────────────────────── */

#define WDT_DEFAULT_TIMEOUT_MS  10000u   /* arm for 10 s by default   */
#define WDT_MAX_TIMEOUT_MS      600000u  /* clamp at 10 min           */

/* Register description of the watchdog exposed by the platform. */
typedef struct {
    uint16_t reload_port;    /* port latched with the timeout count      */
    uint16_t kick_port;      /* port written to pet the timer            */
    uint16_t control_port;   /* port that arms / disarms the timer       */
    uint8_t  kick_value;     /* magic value written to kick_port         */
    uint8_t  enable_value;   /* value written to control_port to arm     */
    uint8_t  disable_value;  /* value written to control_port to disarm  */
    uint8_t  count_bytes;    /* 1 or 2: width of the reload register     */
    uint32_t ticks_per_ms;   /* reload ticks corresponding to one ms     */
} wdt_config_t;

/* Runtime snapshot of the watchdog. */
typedef struct {
    bool     initialized;
    bool     enabled;
    uint32_t timeout_ms;
    uint32_t kicks;
    uint16_t reload_port;
    uint16_t kick_port;
    uint16_t control_port;
} wdt_state_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Install the register description and arm the timer. */
bool wdt_init(const wdt_config_t *config);

/* Pet the watchdog so it does not fire. */
bool wdt_kick(void);

/* Reprogram the timeout window, in milliseconds. */
bool wdt_set_timeout(uint32_t milliseconds);

/* Disarm the watchdog. */
bool wdt_disable(void);

/* Copy the current watchdog state into *out. */
void wdt_get_state(wdt_state_t *out);

#endif /* WATCHDOG_H */
