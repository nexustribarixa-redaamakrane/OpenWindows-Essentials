/*
 * owsysfs64.h - OpenWindows System Filesystem Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWSYSFS64_H
#define OWSYSFS64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owsysfs_init(void);
void owsysfs_read(void);
void owsysfs_write(void);
void owsysfs_enumerate(void);
void owsysfs_get_attr(void);

#endif /* OWSYSFS64_H */

