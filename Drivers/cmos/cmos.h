/*
 * cmos.h - OpenWindows CMOS/RTC Access Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef CMOS_H
#define CMOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void cmos_init(void);
void cmos_read_byte(void);
void cmos_write_byte(void);
void cmos_get_time(void);

#endif /* CMOS_H */

