/*
 * watchdog.h - OpenWindows Hardware Watchdog Timer (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void wdt_init(void);
void wdt_kick(void);
void wdt_set_timeout(void);
void wdt_disable(void);

#endif /* WATCHDOG_H */

