/*
 * watchdog.c - OpenWindows Hardware Watchdog Timer (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * The driver is intentionally register-agnostic: the platform describes its
 * watchdog through a small port vtable and this module performs the actual
 * port I/O, count encoding and state bookkeeping.
 */

#include "watchdog.h"

/* ── Port I/O helpers ─────────────────────────────────────────── */

static inline uint8_t ow_inb(uint16_t port)
{
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void ow_outb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint16_t ow_inw(uint16_t port)
{
    uint16_t v;
    __asm__ __volatile__("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void ow_outw(uint16_t port, uint16_t value)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(port));
}

/* ── Internal state ───────────────────────────────────────────── */

static wdt_config_t wdt_cfg;
static wdt_state_t  wdt_state;

/* ── Helpers ──────────────────────────────────────────────────── */

/* Clamp a requested timeout into the supported window. */
static uint32_t wdt_clamp_timeout(uint32_t milliseconds)
{
    if (milliseconds < 1u) {
        return 1u;
    }
    if (milliseconds > WDT_MAX_TIMEOUT_MS) {
        return WDT_MAX_TIMEOUT_MS;
    }
    return milliseconds;
}

/* Write the reload count using the configured register width. */
static void wdt_write_count(uint32_t ticks)
{
    if (wdt_cfg.reload_port == 0u) {
        return;
    }
    if (wdt_cfg.count_bytes >= 2u) {
        ow_outw(wdt_cfg.reload_port, (uint16_t)ticks);
    } else {
        ow_outb(wdt_cfg.reload_port, (uint8_t)ticks);
    }
}

/* Convert milliseconds to reload ticks, saturated to the register width. */
static uint32_t wdt_ms_to_ticks(uint32_t milliseconds)
{
    uint64_t ticks = (uint64_t)milliseconds * (uint64_t)wdt_cfg.ticks_per_ms;
    uint32_t limit = (wdt_cfg.count_bytes >= 2u) ? 0xFFFFu : 0xFFu;

    if (ticks > (uint64_t)limit) {
        ticks = (uint64_t)limit;
    }
    return (uint32_t)ticks;
}

/* ── Implementation ───────────────────────────────────────────── */

bool wdt_init(const wdt_config_t *config)
{
    if (!config) {
        return false;
    }
    if (config->control_port == 0u && config->kick_port == 0u) {
        return false;
    }

    wdt_cfg = *config;
    if (wdt_cfg.ticks_per_ms == 0u) {
        wdt_cfg.ticks_per_ms = 1u;
    }
    if (wdt_cfg.count_bytes == 0u) {
        wdt_cfg.count_bytes = 1u;
    }

    wdt_state.initialized = true;
    wdt_state.enabled = false;
    wdt_state.timeout_ms = 0u;
    wdt_state.kicks = 0u;
    wdt_state.reload_port = wdt_cfg.reload_port;
    wdt_state.kick_port = wdt_cfg.kick_port;
    wdt_state.control_port = wdt_cfg.control_port;

    /* Arm the timer with the default window, then pet it once. */
    if (!wdt_set_timeout(WDT_DEFAULT_TIMEOUT_MS)) {
        return false;
    }
    if (wdt_cfg.control_port != 0u) {
        ow_outb(wdt_cfg.control_port, wdt_cfg.enable_value);
    }
    wdt_state.enabled = true;
    (void)wdt_kick();
    return true;
}

bool wdt_kick(void)
{
    if (!wdt_state.initialized) {
        return false;
    }
    if (wdt_cfg.kick_port == 0u) {
        return false;
    }

    ow_outb(wdt_cfg.kick_port, wdt_cfg.kick_value);
    wdt_state.kicks++;
    return true;
}

bool wdt_set_timeout(uint32_t milliseconds)
{
    uint32_t clamped;

    if (!wdt_state.initialized) {
        return false;
    }

    clamped = wdt_clamp_timeout(milliseconds);
    wdt_write_count(wdt_ms_to_ticks(clamped));
    wdt_state.timeout_ms = clamped;
    return true;
}

bool wdt_disable(void)
{
    if (!wdt_state.initialized) {
        return false;
    }

    if (wdt_cfg.control_port != 0u) {
        ow_outb(wdt_cfg.control_port, wdt_cfg.disable_value);
    }
    wdt_state.enabled = false;
    return true;
}

void wdt_get_state(wdt_state_t *out)
{
    if (!out) {
        return;
    }
    *out = wdt_state;
}
