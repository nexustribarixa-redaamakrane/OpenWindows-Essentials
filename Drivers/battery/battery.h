/*
 * battery.h - OpenWindows Battery/ACPI Battery Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void batt_init(void);
void batt_get_level(void);
void batt_get_status(void);
void batt_get_time_remain(void);

#endif /* BATTERY_H */

