/*
 * owthread64.h - OpenWindows Thread Management Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWTHREAD64_H
#define OWTHREAD64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owthread_create(void);
void owthread_join(void);
void owthread_detach(void);
void owthread_yield(void);
void owthread_exit(void);
void owthread_self(void);

#endif /* OWTHREAD64_H */

