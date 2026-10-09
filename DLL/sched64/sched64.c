/*
 * sched64.c - OpenWindows Thread & Task Scheduler Library Implementation (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "sched64.h"

void sched_init(sched_scheduler_t *sched)
{
    if (!sched) return;
    sched->current_thread_idx = 0u;
    sched->active_thread_count = 0u;
    for (size_t i = 0u; i < SCHED_MAX_THREADS; ++i) {
        sched->threads[i].state = THREAD_STATE_UNUSED;
        sched->threads[i].thread_id = 0u;
    }
}

int32_t sched_create_thread(sched_scheduler_t *sched, uint32_t pid, uint64_t entry, uint64_t stack, uint32_t prio)
{
    if (!sched) return -1;
    for (size_t i = 0u; i < SCHED_MAX_THREADS; ++i) {
        if (sched->threads[i].state == THREAD_STATE_UNUSED || sched->threads[i].state == THREAD_STATE_TERMINATED) {
            sched->threads[i].thread_id = (uint32_t)(i + 1u);
            sched->threads[i].process_id = pid;
            sched->threads[i].instruction_ptr = entry;
            sched->threads[i].stack_ptr = stack;
            sched->threads[i].priority = prio;
            sched->threads[i].state = THREAD_STATE_READY;
            sched->threads[i].quantum_remaining = SCHED_DEFAULT_QUANTUM;
            sched->threads[i].total_cpu_time = 0u;
            sched->active_thread_count++;
            return (int32_t)sched->threads[i].thread_id;
        }
    }
    return -1;
}

bool sched_terminate_thread(sched_scheduler_t *sched, uint32_t tid)
{
    if (!sched || tid == 0u) return false;
    for (size_t i = 0u; i < SCHED_MAX_THREADS; ++i) {
        if (sched->threads[i].thread_id == tid && sched->threads[i].state != THREAD_STATE_UNUSED) {
            sched->threads[i].state = THREAD_STATE_TERMINATED;
            if (sched->active_thread_count > 0u) sched->active_thread_count--;
            return true;
        }
    }
    return false;
}

bool sched_freeze_thread(sched_scheduler_t *sched, uint32_t tid)
{
    if (!sched || tid == 0u) return false;
    for (size_t i = 0u; i < SCHED_MAX_THREADS; ++i) {
        if (sched->threads[i].thread_id == tid && sched->threads[i].state == THREAD_STATE_RUNNING) {
            sched->threads[i].state = THREAD_STATE_FROZEN;
            return true;
        }
    }
    return false;
}

int32_t sched_yield(sched_scheduler_t *sched)
{
    if (!sched || sched->active_thread_count == 0u) return -1;

    size_t start = sched->current_thread_idx;
    for (size_t offset = 1u; offset <= SCHED_MAX_THREADS; ++offset) {
        size_t idx = (start + offset) % SCHED_MAX_THREADS;
        if (sched->threads[idx].state == THREAD_STATE_READY || sched->threads[idx].state == THREAD_STATE_RUNNING) {
            sched->threads[idx].state = THREAD_STATE_RUNNING;
            sched->current_thread_idx = (uint32_t)idx;
            return (int32_t)sched->threads[idx].thread_id;
        }
    }
    return -1;
}
