/*
 * pitapic.c - OpenWindows PIT & Local APIC Timer Driver Implementation (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "pitapic.h"

void pitapic_init(pitapic_state_t *state, uint32_t target_hz)
{
    if (!state) return;
    if (target_hz == 0u) target_hz = 1000u;
    if (target_hz > PIT_BASE_FREQUENCY) target_hz = PIT_BASE_FREQUENCY;

    state->frequency_hz = target_hz;
    state->divisor = PIT_BASE_FREQUENCY / target_hz;
    state->total_ticks = 0u;
    state->tsc_frequency = 2400000000ULL; /* Default 2.4 GHz fallback */
}

void pitapic_on_tick(pitapic_state_t *state)
{
    if (!state) return;
    state->total_ticks++;
}

uint64_t pitapic_get_uptime_ms(const pitapic_state_t *state)
{
    if (!state || state->frequency_hz == 0u) return 0u;
    return (state->total_ticks * 1000u) / state->frequency_hz;
}
