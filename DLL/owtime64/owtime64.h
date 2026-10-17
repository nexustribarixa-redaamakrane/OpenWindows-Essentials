/*
 * owtime64.h - OpenWindows Time Management Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Maintains a monotonic tick counter driven from the 8254 PIT channel 0
 * (programmed in rate-generator mode). Sub-tick resolution is recovered by
 * latching the live PIT down-counter.
 */

#ifndef OWTIME64_H
#define OWTIME64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 8254 PIT input clock (Hz). */
#define OWTIME_PIT_HZ       1193182u

/* Default kernel tick rate: 1 tick every millisecond. */
#define OWTIME_DEF_TICK_HZ  1000u

/* ── Status codes ─────────────────────────────────────────────────── */

#define OWTIME_OK           0u  /* operation succeeded                  */
#define OWTIME_ERR_STATE    1u  /* library not initialized              */
#define OWTIME_ERR_PARAM    2u  /* invalid tick rate                    */

/* ── Public types ─────────────────────────────────────────────────── */

typedef struct {
    uint64_t ticks;         /* completed PIT periods since init         */
    uint32_t tick_hz;       /* ticks per second                         */
    uint16_t divisor;       /* PIT reload value                         */
    uint16_t last_pit;      /* down-counter latched on the last tick    */
    uint64_t epoch;         /* wall-clock seconds for tick 0            */
    bool     initialized;
} owtime_state_t;

/* ── API ──────────────────────────────────────────────────────────── */

/* Program PIT channel 0 for `tick_hz` ticks per second. */
bool owtime_init(uint32_t tick_hz);

/* Monotonic tick count since owtime_init(). */
uint64_t owtime_get_ticks(void);

/* Nanoseconds since owtime_init(), including the sub-tick remainder. */
uint64_t owtime_get_ns(void);

/* Busy-wait for at least `milliseconds` while spinning on the tick source. */
void owtime_sleep_ms(uint32_t milliseconds);

/* Milliseconds since owtime_init(). */
uint64_t owtime_get_uptime(void);

/* Offset in seconds added to the monotonic clock to form wall-clock time. */
void owtime_set_epoch(uint64_t unix_seconds);

/* Current wall-clock epoch offset (seconds). */
uint64_t owtime_get_epoch(void);

/* Wall-clock seconds = epoch + uptime. */
uint64_t owtime_get_unix_seconds(void);

/* Called from the timer ISR (IRQ0) to advance the tick counter. */
void owtime_on_tick(void);

/* Access the underlying time state. */
owtime_state_t *owtime_state(void);

#endif /* OWTIME64_H */
