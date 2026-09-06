/*
 * pitapic.h - OpenWindows PIT & Local APIC Timer Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef PITAPIC_H
#define PITAPIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PIT_CHANNEL0_DATA   0x40
#define PIT_COMMAND_PORT    0x43
#define PIT_BASE_FREQUENCY  1193182u

typedef struct {
    uint32_t frequency_hz;
    uint32_t divisor;
    uint64_t total_ticks;
    uint64_t tsc_frequency;
} pitapic_state_t;

void pitapic_init(pitapic_state_t *state, uint32_t target_hz);
void pitapic_on_tick(pitapic_state_t *state);
uint64_t pitapic_get_uptime_ms(const pitapic_state_t *state);

#endif /* PITAPIC_H */
