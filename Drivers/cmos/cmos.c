/*
 * cmos.c - OpenWindows CMOS/RTC Access Driver (.owc)
 *
 * Every index write re-applies the tracked NMI disable bit so an access
 * never silently re-enables or disables NMIs. The calendar is sampled
 * twice around an update-in-progress poll; only two identical samples
 * are accepted, which avoids torn reads across the RTC carry.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "cmos.h"

/* ── Port I/O helpers ─────────────────────────────────────────── */

static inline uint8_t ow_inb(uint16_t port)
{
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void ow_outb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* ── Internal state ───────────────────────────────────────────── */

static bool cmos_nmi_off;
static bool cmos_initialized;

#define CMOS_UIP_LIMIT  1000000u

/* ── Helpers ──────────────────────────────────────────────────── */

/* Reads of the unused diagnostic port cost roughly one bus cycle each. */
static void cmos_io_delay(void)
{
    (void)ow_inb(0x80u);
    (void)ow_inb(0x80u);
    (void)ow_inb(0x80u);
    (void)ow_inb(0x80u);
}

static void cmos_select(uint8_t index)
{
    uint8_t value = index;
    if (cmos_nmi_off) value |= CMOS_NMI_DISABLE;
    ow_outb(CMOS_INDEX_PORT, value);
    cmos_io_delay();
}

static uint8_t cmos_bcd_to_bin(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10u) + (value & 0x0Fu));
}

/* Spin (bounded) until the RTC leaves its update cycle. */
static bool cmos_wait_ready(void)
{
    for (uint32_t i = 0u; i < CMOS_UIP_LIMIT; ++i) {
        if (!cmos_update_in_progress()) return true;
    }
    return !cmos_update_in_progress();
}

/* Grab the six calendar registers that make up a clock snapshot. */
static void cmos_sample(uint8_t *second, uint8_t *minute, uint8_t *hour,
                        uint8_t *day, uint8_t *month, uint8_t *year)
{
    *second = cmos_read_byte(CMOS_REG_SECONDS);
    *minute = cmos_read_byte(CMOS_REG_MINUTES);
    *hour   = cmos_read_byte(CMOS_REG_HOURS);
    *day    = cmos_read_byte(CMOS_REG_DAY);
    *month  = cmos_read_byte(CMOS_REG_MONTH);
    *year   = cmos_read_byte(CMOS_REG_YEAR);
}

/* ── Public API ───────────────────────────────────────────────── */

bool cmos_init(void)
{
    cmos_nmi_off = false;
    cmos_initialized = true;

    /* Probe the RTC configuration registers. */
    (void)cmos_read_byte(CMOS_REG_STATUS_A);
    (void)cmos_read_byte(CMOS_REG_STATUS_B);
    return true;
}

uint8_t cmos_read_byte(uint8_t index)
{
    cmos_select(index);
    return ow_inb(CMOS_DATA_PORT);
}

void cmos_write_byte(uint8_t index, uint8_t value)
{
    cmos_select(index);
    ow_outb(CMOS_DATA_PORT, value);
}

void cmos_set_nmi(bool disabled)
{
    cmos_nmi_off = disabled;
}

bool cmos_nmi_disabled(void)
{
    return cmos_nmi_off;
}

bool cmos_update_in_progress(void)
{
    return (cmos_read_byte(CMOS_REG_STATUS_A) & CMOS_STATUS_A_UIP) != 0u;
}

bool cmos_get_time(cmos_time_t *out)
{
    if (!out || !cmos_initialized) return false;

    uint8_t s1 = 0u, m1 = 0u, h1 = 0u, d1 = 0u, mo1 = 0u, y1 = 0u;
    uint8_t s2 = 0u, m2 = 0u, h2 = 0u, d2 = 0u, mo2 = 0u, y2 = 0u;
    bool stable = false;

    for (uint32_t attempt = 0u; attempt < 16u; ++attempt) {
        if (!cmos_wait_ready()) break;

        cmos_sample(&s1, &m1, &h1, &d1, &mo1, &y1);

        if (!cmos_wait_ready()) break;
        cmos_sample(&s2, &m2, &h2, &d2, &mo2, &y2);

        if (s1 == s2 && m1 == m2 && h1 == h2 &&
            d1 == d2 && mo1 == mo2 && y1 == y2) {
            stable = true;
            break;
        }
    }

    out->valid = stable;
    if (!stable) return false;

    uint8_t status_b = cmos_read_byte(CMOS_REG_STATUS_B);
    bool binary  = (status_b & CMOS_STATUS_B_BIN) != 0u;
    bool hour24  = (status_b & CMOS_STATUS_B_24H) != 0u;

    uint8_t second = s1;
    uint8_t minute = m1;
    uint8_t hour   = h1;
    uint8_t day    = d1;
    uint8_t month  = mo1;
    uint8_t year   = y1;

    if (!binary) {
        second = cmos_bcd_to_bin(second);
        minute = cmos_bcd_to_bin(minute);
        day    = cmos_bcd_to_bin(day);
        month  = cmos_bcd_to_bin(month);
        year   = cmos_bcd_to_bin(year);
    }

    if (!hour24) {
        bool pm = (hour & 0x80u) != 0u;   /* bit 7 selects PM */
        hour = (uint8_t)(hour & 0x7Fu);
        if (!binary) hour = cmos_bcd_to_bin(hour);
        if (pm && hour < 12u) hour = (uint8_t)(hour + 12u);
        if (!pm && hour == 12u) hour = 0u;
    } else if (!binary) {
        hour = cmos_bcd_to_bin(hour);
    }

    /* The century register is optional; assume 20xx when unsupported. */
    uint8_t century_raw = cmos_read_byte(CMOS_REG_CENTURY);
    uint16_t century;
    if (century_raw == 0x00u || century_raw == 0xFFu) {
        century = 20u;
    } else {
        century = binary ? (uint16_t)century_raw
                         : (uint16_t)cmos_bcd_to_bin(century_raw);
    }

    out->second  = second;
    out->minute  = minute;
    out->hour    = hour;
    out->weekday = cmos_read_byte(CMOS_REG_WEEKDAY);
    out->day     = day;
    out->month   = month;
    out->year    = (uint16_t)(century * 100u + year);
    return true;
}
