/*
 * devnull.h - OpenWindows Null/Zero Device Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef DEVNULL_H
#define DEVNULL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void devnull_init(void);
void devnull_read(void);
void devnull_write(void);

#endif /* DEVNULL_H */

