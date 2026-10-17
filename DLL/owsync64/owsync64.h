/*
 * owsync64.h - OpenWindows Synchronization Primitives (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Primitive kernel synchronization objects built on atomic builtins:
 *   - test-and-set spinlock
 *   - recursive mutex layered on a spinlock
 *   - sense-reversing barrier
 */

#ifndef OWSYNC64_H
#define OWSYNC64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────────── */

#define OWSYNC_OK           0u  /* operation succeeded                  */
#define OWSYNC_ERR_NULL     1u  /* null object pointer                  */
#define OWSYNC_ERR_STATE    2u  /* object not initialized / bad state   */
#define OWSYNC_ERR_OWNER    3u  /* caller does not own the object       */
#define OWSYNC_ERR_PARAM    4u  /* invalid parameter                    */

/* ── Public types ─────────────────────────────────────────────────── */

/* Test-and-set spinlock. `locked` is manipulated with __atomic builtins. */
typedef struct {
    volatile uint32_t locked;        /* 0 = free, 1 = held               */
    uint32_t          owner;         /* logical CPU/context holding it   */
    uint64_t          acquisitions;  /* successful acquire counter       */
} owspinlock_t;

/* Recursive mutex layered on top of a spinlock. */
typedef struct {
    owspinlock_t guard;              /* protects the mutex bookkeeping   */
    uint32_t     owner;              /* CPU id + 1 that owns, 0 = free   */
    uint32_t     depth;              /* recursion depth                  */
    bool         initialized;        /* set by owmutex init              */
} owmutex_t;

/* Sense-reversing barrier; `generation` flips on every completed round. */
typedef struct {
    owspinlock_t     guard;
    volatile uint32_t generation;    /* bumped when the round completes   */
    uint32_t         expected;       /* participants required per round   */
    uint32_t         arrived;        /* participants arrived this round   */
    bool             initialized;
} owbarrier_t;

/* ── Context identity ─────────────────────────────────────────────── */

/* Set the logical CPU/context id used for mutex ownership tracking. */
void owsync_set_cpu_id(uint32_t cpu_id);

/* Current logical CPU/context id (defaults to 0). */
uint32_t owsync_cpu_id(void);

/* ── Spinlock ─────────────────────────────────────────────────────── */

/* Reset a spinlock to the free state. */
bool owsync_spinlock_init(owspinlock_t *lock);

/* Busy-wait until the lock is acquired by the caller. */
bool owsync_spinlock_acquire(owspinlock_t *lock);

/* Release a lock previously acquired by the caller. */
bool owsync_spinlock_release(owspinlock_t *lock);

/* ── Mutex ────────────────────────────────────────────────────────── */

/* Reset a mutex to the unlocked state. */
bool owsync_mutex_init(owmutex_t *mutex);

/* Acquire the mutex; re-entrant for the owning context. */
bool owsync_mutex_lock(owmutex_t *mutex);

/* Release one recursion level of the mutex. */
bool owsync_mutex_unlock(owmutex_t *mutex);

/* ── Barrier ──────────────────────────────────────────────────────── */

/* Initialise a barrier that releases once `participants` arrive. */
bool owsync_barrier_init(owbarrier_t *barrier, uint32_t participants);

/* Wait at the barrier; on success `*out_generation` receives the round id. */
bool owsync_barrier(owbarrier_t *barrier, uint32_t *out_generation);

#endif /* OWSYNC64_H */
