/*
 * sentinel64.h - OpenWindows Sentinel Recovery & Task Isolation Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef SENTINEL64_H
#define SENTINEL64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SENTINEL_MAX_ISOLATED_TASKS 32u

typedef struct {
    uint32_t process_id;
    uint32_t thread_id;
    uint32_t reason_bancode;
    uint64_t isolation_timestamp;
    bool     is_frozen;
} sentinel_frozen_task_t;

typedef struct {
    sentinel_frozen_task_t tasks[SENTINEL_MAX_ISOLATED_TASKS];
    uint32_t               frozen_count;
    uint32_t               recovery_pass_count;
} sentinel_watchdog_state_t;

void sentinel64_init(sentinel_watchdog_state_t *state);
bool sentinel64_freeze_process(sentinel_watchdog_state_t *state, uint32_t pid, uint32_t tid, uint32_t bancode, uint64_t tsc);
bool sentinel64_terminate_frozen(sentinel_watchdog_state_t *state, uint32_t pid);
uint32_t sentinel64_get_frozen_count(const sentinel_watchdog_state_t *state);

#endif /* SENTINEL64_H */
