/*
 * owlog64.h - OpenWindows Kernel Logging Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWLOG64_H
#define OWLOG64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owlog_init(void);
void owlog_write(void);
void owlog_flush(void);
void owlog_set_level(void);
void owlog_get_buffer(void);

#endif /* OWLOG64_H */

