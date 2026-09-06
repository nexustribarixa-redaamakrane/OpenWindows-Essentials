/*
 * owfifo64.h - OpenWindows FIFO Ring Buffer Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWFIFO64_H
#define OWFIFO64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owfifo_init(void);
void owfifo_push(void);
void owfifo_pop(void);
void owfifo_peek(void);
void owfifo_count(void);
void owfifo_is_empty(void);

#endif /* OWFIFO64_H */

