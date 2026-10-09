/*
 * sched64.h - OpenWindows Thread & Task Scheduler Library (.owd)
 *
 * Pre-allocated Thread Control Blocks (TCB), zero dynamic heap allocation.
 * C99 freestanding.
 */

#ifndef SCHED64_H
#define SCHED64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SCHED_MAX_THREADS   64u
#define SCHED_DEFAULT_QUANTUM 10u

typedef enum {
    THREAD_STATE_UNUSED   = 0,
    THREAD_STATE_READY    = 1,
    THREAD_STATE_RUNNING  = 2,
    THREAD_STATE_BLOCKED  = 3,
    THREAD_STATE_FROZEN   = 4,  /* Intercepted by sentinel watchdog */
    THREAD_STATE_TERMINATED = 5
} thread_state_t;

typedef struct {
    uint32_t       thread_id;
    uint32_t       process_id;
    uint32_t       priority;
    thread_state_t state;
    uint64_t       instruction_ptr;
    uint64_t       stack_ptr;
    uint32_t       quantum_remaining;
    uint32_t       total_cpu_time;
} sched_tcb_t;

typedef struct {
    sched_tcb_t threads[SCHED_MAX_THREADS];
    uint32_t    current_thread_idx;
    uint32_t    active_thread_count;
} sched_scheduler_t;

void sched_init(sched_scheduler_t *sched);
int32_t sched_create_thread(sched_scheduler_t *sched, uint32_t pid, uint64_t entry, uint64_t stack, uint32_t prio);
bool sched_terminate_thread(sched_scheduler_t *sched, uint32_t tid);
bool sched_freeze_thread(sched_scheduler_t *sched, uint32_t tid);
int32_t sched_yield(sched_scheduler_t *sched);

#endif /* SCHED64_H */
