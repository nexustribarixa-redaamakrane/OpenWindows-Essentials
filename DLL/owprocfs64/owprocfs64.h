/*
 * owprocfs64.h - OpenWindows Process Filesystem Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWPROCFS64_H
#define OWPROCFS64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owprocfs_init(void);
void owprocfs_read_pid(void);
void owprocfs_list(void);
void owprocfs_get_stat(void);
void owprocfs_get_mem(void);

#endif /* OWPROCFS64_H */

