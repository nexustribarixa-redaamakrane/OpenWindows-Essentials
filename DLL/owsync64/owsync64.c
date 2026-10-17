/*
 * owsync64.c - OpenWindows Synchronization Primitives (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owsync64.h"

/* ── Internal state ───────────────────────────────────────────── */

/* Logical CPU/context id, used as the mutex ownership token. */
static uint32_t owsync64_cpu_id = 0u;

/* ── Helpers ──────────────────────────────────────────────────── */

/* Emit an x86 PAUSE hint while spinning to be nice to SMT siblings. */
static void ow_cpu_relax(void)
{
    __asm__ __volatile__("pause");
}

/* Zero the raw bytes of an arbitrary object without libc. */
static void ow_zero(void *dst, size_t len)
{
    uint8_t *p = (uint8_t *)dst;
    size_t i;

    for (i = 0u; i < len; ++i) {
        p[i] = 0u;
    }
}

/* ── Context identity ─────────────────────────────────────────── */

void owsync_set_cpu_id(uint32_t cpu_id)
{
    owsync64_cpu_id = cpu_id;
}

uint32_t owsync_cpu_id(void)
{
    return owsync64_cpu_id;
}

/* ── Spinlock ─────────────────────────────────────────────────── */

bool owsync_spinlock_init(owspinlock_t *lock)
{
    if (lock == NULL) {
        return false;
    }

    ow_zero(lock, sizeof(*lock));
    return true;
}

bool owsync_spinlock_acquire(owspinlock_t *lock)
{
    if (lock == NULL) {
        return false;
    }

    /* Test-and-set: exchange 1 into `locked`; retry while it was held. */
    while (__atomic_exchange_n(&lock->locked, 1u, __ATOMIC_ACQUIRE) != 0u) {
        ow_cpu_relax();
    }

    __atomic_add_fetch(&lock->acquisitions, 1u, __ATOMIC_RELAXED);
    lock->owner = owsync64_cpu_id;
    return true;
}

bool owsync_spinlock_release(owspinlock_t *lock)
{
    if (lock == NULL) {
        return false;
    }

    lock->owner = 0u;
    __atomic_store_n(&lock->locked, 0u, __ATOMIC_RELEASE);
    return true;
}

/* ── Mutex ────────────────────────────────────────────────────── */

bool owsync_mutex_init(owmutex_t *mutex)
{
    if (mutex == NULL) {
        return false;
    }

    ow_zero(mutex, sizeof(*mutex));
    (void)owsync_spinlock_init(&mutex->guard);
    mutex->initialized = true;
    return true;
}

bool owsync_mutex_lock(owmutex_t *mutex)
{
    uint32_t me;

    if (mutex == NULL || !mutex->initialized) {
        return false;
    }

    /* Store the owner as cpu+1 so that 0 keeps meaning "free". */
    me = owsync64_cpu_id + 1u;

    for (;;) {
        (void)owsync_spinlock_acquire(&mutex->guard);

        if (mutex->owner == 0u) {
            mutex->owner = me;
            mutex->depth = 1u;
            (void)owsync_spinlock_release(&mutex->guard);
            return true;
        }

        if (mutex->owner == me) {
            /* Recursive acquisition by the owning context. */
            mutex->depth++;
            (void)owsync_spinlock_release(&mutex->guard);
            return true;
        }

        (void)owsync_spinlock_release(&mutex->guard);
        ow_cpu_relax();
    }
}

bool owsync_mutex_unlock(owmutex_t *mutex)
{
    uint32_t me;
    bool ok = true;

    if (mutex == NULL || !mutex->initialized) {
        return false;
    }

    me = owsync64_cpu_id + 1u;

    (void)owsync_spinlock_acquire(&mutex->guard);

    if (mutex->owner != me) {
        ok = false;
    } else if (mutex->depth > 1u) {
        mutex->depth--;
    } else {
        mutex->depth = 0u;
        mutex->owner = 0u;
    }

    (void)owsync_spinlock_release(&mutex->guard);
    return ok;
}

/* ── Barrier ──────────────────────────────────────────────────── */

bool owsync_barrier_init(owbarrier_t *barrier, uint32_t participants)
{
    if (barrier == NULL || participants == 0u) {
        return false;
    }

    ow_zero(barrier, sizeof(*barrier));
    (void)owsync_spinlock_init(&barrier->guard);
    barrier->expected = participants;
    barrier->generation = 0u;
    barrier->initialized = true;
    return true;
}

bool owsync_barrier(owbarrier_t *barrier, uint32_t *out_generation)
{
    uint32_t generation;

    if (barrier == NULL || !barrier->initialized || barrier->expected == 0u) {
        return false;
    }

    (void)owsync_spinlock_acquire(&barrier->guard);
    generation = barrier->generation;
    barrier->arrived++;

    if (barrier->arrived >= barrier->expected) {
        /* Last arrival: reset the round and flip the sense. */
        barrier->arrived = 0u;
        __atomic_add_fetch(&barrier->generation, 1u, __ATOMIC_RELEASE);
    }

    (void)owsync_spinlock_release(&barrier->guard);

    if (out_generation != NULL) {
        *out_generation = generation;
    }

    /* Spin until the generation moves past the round we joined. */
    while (__atomic_load_n(&barrier->generation, __ATOMIC_ACQUIRE) ==
           generation) {
        ow_cpu_relax();
    }

    return true;
}
