/*
 * thermal.h - OpenWindows Thermal Monitor Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef THERMAL_H
#define THERMAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void thermal_init(void);
void thermal_read_temp(void);
void thermal_set_threshold(void);
void thermal_get_zone(void);

#endif /* THERMAL_H */

