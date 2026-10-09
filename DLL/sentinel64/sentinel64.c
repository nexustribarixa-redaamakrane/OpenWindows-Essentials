/*
 * sentinel64.c - OpenWindows Sentinel Recovery Library Implementation (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "sentinel64.h"

void sentinel64_init(sentinel_watchdog_state_t *state)
{
    if (!state) return;
    state->frozen_count = 0u;
    state->recovery_pass_count = 0u;
    for (size_t i = 0u; i < SENTINEL_MAX_ISOLATED_TASKS; ++i) {
        state->tasks[i].is_frozen = false;
        state->tasks[i].process_id = 0u;
    }
}

bool sentinel64_freeze_process(sentinel_watchdog_state_t *state, uint32_t pid, uint32_t tid, uint32_t bancode, uint64_t tsc)
{
    if (!state || pid == 0u) return false;

    for (size_t i = 0u; i < SENTINEL_MAX_ISOLATED_TASKS; ++i) {
        if (!state->tasks[i].is_frozen) {
            state->tasks[i].process_id = pid;
            state->tasks[i].thread_id = tid;
            state->tasks[i].reason_bancode = bancode;
            state->tasks[i].isolation_timestamp = tsc;
            state->tasks[i].is_frozen = true;
            state->frozen_count++;
            return true;
        }
    }
    return false;
}

bool sentinel64_terminate_frozen(sentinel_watchdog_state_t *state, uint32_t pid)
{
    if (!state || pid == 0u) return false;

    bool killed = false;
    for (size_t i = 0u; i < SENTINEL_MAX_ISOLATED_TASKS; ++i) {
        if (state->tasks[i].is_frozen && state->tasks[i].process_id == pid) {
            state->tasks[i].is_frozen = false;
            state->tasks[i].process_id = 0u;
            if (state->frozen_count > 0u) state->frozen_count--;
            killed = true;
        }
    }
    return killed;
}

uint32_t sentinel64_get_frozen_count(const sentinel_watchdog_state_t *state)
{
    if (!state) return 0u;
    return state->frozen_count;
}
