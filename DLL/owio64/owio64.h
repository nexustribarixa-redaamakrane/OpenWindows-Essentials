/*
 * owio64.h - OpenWindows Port I/O Abstraction Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWIO64_H
#define OWIO64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owio_inb(void);
void owio_outb(void);
void owio_inw(void);
void owio_outw(void);
void owio_ind(void);
void owio_outd(void);
void owio_wait(void);

#endif /* OWIO64_H */

