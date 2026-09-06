/*
 * owtime64.h - OpenWindows Time Management Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWTIME64_H
#define OWTIME64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owtime_init(void);
void owtime_get_ticks(void);
void owtime_sleep_ms(void);
void owtime_get_uptime(void);
void owtime_set_epoch(void);

#endif /* OWTIME64_H */

