/*
 * rtc.h - OpenWindows CMOS Real-Time Clock Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef RTC_H
#define RTC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define RTC_INDEX_PORT  0x70
#define RTC_DATA_PORT   0x71

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} rtc_datetime_t;

void rtc_init(void);
void rtc_read_datetime(rtc_datetime_t *out);

#endif /* RTC_H */
