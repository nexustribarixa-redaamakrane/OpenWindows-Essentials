/*
 * owdisk64.h - OpenWindows Disk I/O Abstraction (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWDISK64_H
#define OWDISK64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owdisk_init(void);
void owdisk_read(void);
void owdisk_write(void);
void owdisk_flush(void);
void owdisk_get_info(void);
void owdisk_enumerate(void);

#endif /* OWDISK64_H */

