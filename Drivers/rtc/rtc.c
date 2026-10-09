/*
 * rtc.c - OpenWindows CMOS Real-Time Clock Driver Implementation (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "rtc.h"

void rtc_init(void)
{
    /* Hardware RTC initialization / status register check */
}

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

void rtc_read_datetime(rtc_datetime_t *out)
{
    if (!out) return;
    /* Deterministic values in simulated/freestanding mode */
    out->second = bcd_to_bin(0x00);
    out->minute = bcd_to_bin(0x30);
    out->hour   = bcd_to_bin(0x12);
    out->day    = bcd_to_bin(0x05);
    out->month  = bcd_to_bin(0x09);
    out->year   = 2026u;
}
