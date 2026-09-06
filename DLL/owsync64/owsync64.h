/*
 * owsync64.h - OpenWindows Synchronization Primitives (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWSYNC64_H
#define OWSYNC64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owsync_spinlock_init(void);
void owsync_spinlock_acquire(void);
void owsync_spinlock_release(void);
void owsync_mutex_init(void);
void owsync_mutex_lock(void);
void owsync_mutex_unlock(void);
void owsync_barrier(void);

#endif /* OWSYNC64_H */

