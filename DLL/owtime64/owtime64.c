/*
 * owtime64.c - OpenWindows Time Management Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owtime64.h"

/* ── Internal state ───────────────────────────────────────────── */

#define OWTIME_PIT_CH0      0x40u   /* channel 0 data port           */
#define OWTIME_PIT_CMD      0x43u   /* mode/command register         */

/* Channel 0, access lobyte/hibyte, mode 3 (square wave), binary. */
#define OWTIME_PIT_MODE3    0x36u

static owtime_state_t owtime64_state;

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

/* ── Helpers ──────────────────────────────────────────────────── */

/* Latch and read channel 0's live down-counter. */
static uint16_t owtime_read_pit(void)
{
    uint16_t lo;
    uint16_t hi;

    /* 0x00 = counter latch command for channel 0. */
    ow_outb((uint16_t)OWTIME_PIT_CMD, 0x00u);
    lo = (uint16_t)ow_inb((uint16_t)OWTIME_PIT_CH0);
    hi = (uint16_t)ow_inb((uint16_t)OWTIME_PIT_CH0);
    return (uint16_t)(lo | (hi << 8));
}

/* Spin hint used while busy-waiting. */
static void ow_cpu_relax(void)
{
    __asm__ __volatile__("pause");
}

/* ── Implementation ───────────────────────────────────────────── */

bool owtime_init(uint32_t tick_hz)
{
    uint32_t divisor;

    if (tick_hz == 0u) {
        return false;
    }

    divisor = OWTIME_PIT_HZ / tick_hz;
    if (divisor == 0u) {
        divisor = 1u;               /* clamp to the fastest possible rate */
    }
    if (divisor > 0xFFFFu) {
        divisor = 0xFFFFu;          /* PIT reload register is 16 bits     */
    }

    owtime64_state.ticks = 0u;
    owtime64_state.tick_hz = OWTIME_PIT_HZ / divisor;
    if (owtime64_state.tick_hz == 0u) {
        owtime64_state.tick_hz = 1u;
    }
    owtime64_state.divisor = (uint16_t)divisor;
    owtime64_state.epoch = 0u;
    owtime64_state.last_pit = (uint16_t)divisor;

    /* Program channel 0: command byte then reload value (lo, hi). */
    ow_outb((uint16_t)OWTIME_PIT_CMD, (uint8_t)OWTIME_PIT_MODE3);
    ow_outb((uint16_t)OWTIME_PIT_CH0, (uint8_t)(divisor & 0xFFu));
    ow_outb((uint16_t)OWTIME_PIT_CH0, (uint8_t)((divisor >> 8) & 0xFFu));

    owtime64_state.initialized = true;
    return true;
}

void owtime_on_tick(void)
{
    owtime64_state.ticks++;
    owtime64_state.last_pit = owtime_read_pit();
}

uint64_t owtime_get_ticks(void)
{
    return owtime64_state.ticks;
}

uint64_t owtime_get_ns(void)
{
    uint64_t ns_per_period;
    uint64_t delta;
    uint16_t current;
    uint16_t last;
    uint16_t divisor;

    if (!owtime64_state.initialized) {
        return 0u;
    }

    divisor = owtime64_state.divisor;
    last = owtime64_state.last_pit;
    current = owtime_read_pit();

    /* The counter counts down and reloads, so recover the elapsed counts. */
    if (last >= current) {
        delta = (uint64_t)(last - current);
    } else {
        delta = (uint64_t)last + ((uint64_t)divisor - (uint64_t)current);
    }

    ns_per_period = ((uint64_t)divisor * 1000000000ull) / (uint64_t)OWTIME_PIT_HZ;
    return (owtime64_state.ticks * ns_per_period) +
           ((delta * 1000000000ull) / (uint64_t)OWTIME_PIT_HZ);
}

void owtime_sleep_ms(uint32_t milliseconds)
{
    uint64_t start;
    uint64_t wanted;

    if (!owtime64_state.initialized || milliseconds == 0u) {
        return;
    }

    /* Convert milliseconds to ticks, rounding up. */
    wanted = ((uint64_t)milliseconds * (uint64_t)owtime64_state.tick_hz +
              999ull) / 1000ull;
    start = owtime64_state.ticks;

    /* Spin until the tick source has advanced far enough. */
    while ((owtime64_state.ticks - start) < wanted) {
        ow_cpu_relax();
    }
}

uint64_t owtime_get_uptime(void)
{
    return owtime_get_ns() / 1000000ull;
}

void owtime_set_epoch(uint64_t unix_seconds)
{
    owtime64_state.epoch = unix_seconds;
}

uint64_t owtime_get_epoch(void)
{
    return owtime64_state.epoch;
}

uint64_t owtime_get_unix_seconds(void)
{
    return owtime64_state.epoch + (owtime_get_uptime() / 1000ull);
}

owtime_state_t *owtime_state(void)
{
    return &owtime64_state;
}
