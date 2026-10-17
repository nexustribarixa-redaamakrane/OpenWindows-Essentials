/*
 * owlocale64.h - OpenWindows Locale/i18n Library (.owd)
 *
 * Locale selection and formatting of numbers and dates. The locale
 * table is a fixed static array; formatting writes into caller-provided
 * buffers and never allocates. Numbers are passed as fixed-point
 * integers (value scaled by 10^decimals) so floating point is not
 * required.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWLOCALE64_H
#define OWLOCALE64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Limits ───────────────────────────────────────────────────── */

#define OWLOCALE64_MAX_LOCALES  8u
#define OWLOCALE64_CODE_MAX     16u     /* "en_US" + NUL               */
#define OWLOCALE64_FMT_MAX      24u     /* date template + NUL         */
#define OWLOCALE64_CURRENCY_MAX 8u      /* "USD" + NUL                 */
#define OWLOCALE64_MAX_DECIMALS 6u      /* fractional digits supported */

/* ── Status codes ─────────────────────────────────────────────── */

typedef enum {
    OWLOCALE_OK          = 0,   /* success                            */
    OWLOCALE_ERR_PARAM   = -1,  /* bad parameter                      */
    OWLOCALE_ERR_UNKNOWN = -2,  /* locale not selected                */
    OWLOCALE_ERR_SPACE   = -3,  /* output buffer too small            */
    OWLOCALE_ERR_RANGE   = -4   /* value out of range for the format  */
} owlocale_status_t;

/* ── Types ────────────────────────────────────────────────────── */

/* A locale description. */
typedef struct {
    char     code[OWLOCALE64_CODE_MAX];     /* "en_US"                 */
    char     decimal_point;                 /* '.' or ','              */
    char     thousands_sep;                 /* ',' or '.' or ' '       */
    uint8_t  group_size;                    /* digits per group (3)    */
    char     date_format[OWLOCALE64_FMT_MAX];/* token template         */
    char     currency[OWLOCALE64_CURRENCY_MAX];/* ISO 4217 code        */
    uint8_t  first_weekday;                 /* 0 = Sunday              */
    bool     in_use;                        /* slot holds a locale     */
} owlocale_t;

/* Broken-down date/time. */
typedef struct {
    int32_t year;       /* full year, e.g. 2026                   */
    uint8_t month;      /* 1..12                                   */
    uint8_t day;        /* 1..31                                   */
    uint8_t hour;       /* 0..23                                   */
    uint8_t minute;     /* 0..59                                   */
    uint8_t second;     /* 0..59                                   */
    uint8_t weekday;    /* 0 = Sunday .. 6 = Saturday              */
} owdate_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Initialise the built-in locale table and select en_US. */
void owlocale_init(void);

/* Select the locale with the given code (for example "de_DE"). */
bool owlocale_set(const char *code);

/* Return the currently selected locale, or NULL when uninitialised. */
const owlocale_t *owlocale_get(void);

/* Return the locale descriptor for a code, or NULL when unknown. */
const owlocale_t *owlocale_lookup(const char *code);

/* Format a fixed-point value: `value` is the integer scaled by
 * 10^decimals. Writes a NUL-terminated string with the locale's
 * thousands separator and decimal point. Returns the new length, or a
 * negative owlocale_status_t. */
int32_t owlocale_format_number(int64_t value, uint32_t decimals,
                               char *out, size_t out_cap);

/* Format a broken-down date using the selected locale's template.
 * Supported tokens: YYYY YY MMMM MMM MM M DDDD DDD DD D HH mm ss.
 * Returns the new length, or a negative owlocale_status_t. */
int32_t owlocale_format_date(const owdate_t *date, char *out, size_t out_cap);

#endif /* OWLOCALE64_H */
