/*
 * cmos.h - OpenWindows CMOS/RTC Access Driver (.owc)
 *
 * Accesses the battery-backed CMOS through the index/data port pair:
 * port 0x70 selects a register (bit 7 carries the NMI disable flag,
 * which is preserved across accesses) and port 0x71 carries the data.
 * The RTC calendar is read with an update-in-progress stability loop
 * and converted from BCD when the chip is not in binary mode.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef CMOS_H
#define CMOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CMOS_INDEX_PORT     0x70u   /* register selector / NMI bit    */
#define CMOS_DATA_PORT      0x71u   /* register data                  */
#define CMOS_NMI_DISABLE    0x80u   /* bit 7 of the index port        */

/* CMOS/RTC register indices. */
#define CMOS_REG_SECONDS    0x00u
#define CMOS_REG_MINUTES    0x02u
#define CMOS_REG_HOURS      0x04u
#define CMOS_REG_WEEKDAY    0x06u
#define CMOS_REG_DAY        0x07u
#define CMOS_REG_MONTH      0x08u
#define CMOS_REG_YEAR       0x09u
#define CMOS_REG_STATUS_A   0x0Au
#define CMOS_REG_STATUS_B   0x0Bu
#define CMOS_REG_CENTURY    0x32u

/* Status register A/B bits. */
#define CMOS_STATUS_A_UIP   0x80u   /* update in progress             */
#define CMOS_STATUS_B_24H   0x02u   /* 1 = 24-hour mode               */
#define CMOS_STATUS_B_BIN   0x04u   /* 1 = binary, 0 = BCD            */

/* Calendar snapshot in binary form, year as a full four-digit value. */
typedef struct {
    uint8_t  second;    /* 0..59                                     */
    uint8_t  minute;    /* 0..59                                     */
    uint8_t  hour;      /* 0..23                                     */
    uint8_t  weekday;   /* 1..7, 1 = Sunday                          */
    uint8_t  day;       /* 1..31                                     */
    uint8_t  month;     /* 1..12                                     */
    uint16_t year;      /* full year, e.g. 2026                      */
    bool     valid;     /* true when the snapshot was stable         */
} cmos_time_t;

/* Initialise NMI handling and probe the RTC status registers. */
bool cmos_init(void);

/* Read/write one CMOS register (NMI flag preserved). */
uint8_t cmos_read_byte(uint8_t index);
void    cmos_write_byte(uint8_t index, uint8_t value);

/* Read the wall-clock time; returns false when no stable read succeeded. */
bool cmos_get_time(cmos_time_t *out);

/* NMI steering. */
void cmos_set_nmi(bool disabled);
bool cmos_nmi_disabled(void);

/* True while the RTC update cycle is in progress. */
bool cmos_update_in_progress(void);

#endif /* CMOS_H */
