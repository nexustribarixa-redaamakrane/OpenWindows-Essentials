/*
 * floppy.h - OpenWindows Floppy Disk Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef FLOPPY_H
#define FLOPPY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void floppy_init(void);
void floppy_read(void);
void floppy_write(void);
void floppy_seek(void);
void floppy_reset(void);

#endif /* FLOPPY_H */

