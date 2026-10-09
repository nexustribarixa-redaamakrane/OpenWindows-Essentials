/*
 * owdev64.h - OpenWindows Device Manager Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWDEV64_H
#define OWDEV64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owdev_init(void);
void owdev_register(void);
void owdev_unregister(void);
void owdev_find(void);
void owdev_enumerate(void);
void owdev_get_info(void);

#endif /* OWDEV64_H */

