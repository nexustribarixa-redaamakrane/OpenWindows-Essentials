/*
 * owlocale64.c - OpenWindows Locale/i18n Library (.owd)
 *
 * Built-in locales are copied into a static table on init. Number
 * formatting operates on a fixed-point integer (value scaled by
 * 10^decimals) so no floating point support is needed. Date formatting
 * expands a token template such as "MM/DD/YYYY" from a broken-down
 * date structure.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owlocale64.h"

/* ── Internal state ───────────────────────────────────────────── */

static owlocale_t        owlocale64_table[OWLOCALE64_MAX_LOCALES];
static const owlocale_t *owlocale64_current = NULL;
static bool              owlocale64_ready   = false;

/* ── Name tables ──────────────────────────────────────────────── */

static const char *const owlocale64_month_abbr[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char *const owlocale64_month_full[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static const char *const owlocale64_weekday_abbr[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char *const owlocale64_weekday_full[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

/* ── String helpers ───────────────────────────────────────────── */

static size_t ow_strlen(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static int32_t ow_strcmp(const char *a, const char *b)
{
    size_t i = 0;
    while (a[i] != '\0' && a[i] == b[i]) {
        ++i;
    }
    return (int32_t)(uint8_t)a[i] - (int32_t)(uint8_t)b[i];
}

/* Bounded copy that always NUL-terminates within dst_cap bytes. */
static void ow_strncpy(char *dst, const char *src, size_t dst_cap)
{
    size_t i = 0;
    if (dst_cap == 0u) {
        return;
    }
    while (i + 1u < dst_cap && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

/* ── Output writer ────────────────────────────────────────────── */

typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
    bool   overflow;
} owloc_writer_t;

static void w_putc(owloc_writer_t *w, char c)
{
    if (w->len + 1u < w->cap) {
        w->buf[w->len] = c;
        ++w->len;
    } else {
        w->overflow = true;
    }
}

static void w_puts(owloc_writer_t *w, const char *s)
{
    while (*s != '\0') {
        w_putc(w, *s);
        ++s;
    }
}

/* Write an unsigned value, left-padded with zeros to `width`. */
static void w_u32_pad(owloc_writer_t *w, uint32_t value, uint32_t width)
{
    char     rev[12];
    uint32_t n = 0u;

    if (value == 0u) {
        rev[n++] = '0';
    } else {
        while (value > 0u && n < (uint32_t)sizeof(rev)) {
            rev[n++] = (char)('0' + (value % 10u));
            value /= 10u;
        }
    }
    while (n < width && n < (uint32_t)sizeof(rev)) {
        rev[n++] = '0';
    }
    while (n > 0u) {
        w_putc(w, rev[--n]);
    }
}

/* ── Built-in locale definitions ──────────────────────────────── */

static void owlocale64_define(uint32_t slot, const char *code,
                              char decimal_point, char thousands_sep,
                              uint8_t group_size, const char *date_format,
                              const char *currency, uint8_t first_weekday)
{
    owlocale_t *loc = &owlocale64_table[slot];

    loc->in_use        = true;
    loc->decimal_point = decimal_point;
    loc->thousands_sep = thousands_sep;
    loc->group_size    = group_size;
    loc->first_weekday = first_weekday;
    ow_strncpy(loc->code, code, OWLOCALE64_CODE_MAX);
    ow_strncpy(loc->date_format, date_format, OWLOCALE64_FMT_MAX);
    ow_strncpy(loc->currency, currency, OWLOCALE64_CURRENCY_MAX);
}

/* ── Initialisation and selection ─────────────────────────────── */

void owlocale_init(void)
{
    for (uint32_t i = 0; i < OWLOCALE64_MAX_LOCALES; ++i) {
        owlocale64_table[i].in_use = false;
        owlocale64_table[i].code[0] = '\0';
        owlocale64_table[i].date_format[0] = '\0';
        owlocale64_table[i].currency[0] = '\0';
        owlocale64_table[i].decimal_point = '.';
        owlocale64_table[i].thousands_sep = ',';
        owlocale64_table[i].group_size = 0u;
        owlocale64_table[i].first_weekday = 0u;
    }

    owlocale64_define(0u, "en_US", '.', ',', 3u, "MM/DD/YYYY", "USD", 0u);
    owlocale64_define(1u, "de_DE", ',', '.', 3u, "DD.MM.YYYY", "EUR", 1u);
    owlocale64_define(2u, "fr_FR", ',', ' ', 3u, "DD/MM/YYYY", "EUR", 1u);
    owlocale64_define(3u, "en_GB", '.', ',', 3u, "DD/MM/YYYY", "GBP", 1u);
    owlocale64_define(4u, "de_AT", ',', '.', 3u, "DDD, DD. MMM YYYY", "EUR", 1u);

    owlocale64_ready   = true;
    owlocale64_current = &owlocale64_table[0];
}

const owlocale_t *owlocale_lookup(const char *code)
{
    if (!owlocale64_ready || code == NULL) {
        return NULL;
    }
    if (ow_strlen(code) >= (size_t)OWLOCALE64_CODE_MAX) {
        return NULL;
    }
    for (uint32_t i = 0; i < OWLOCALE64_MAX_LOCALES; ++i) {
        if (owlocale64_table[i].in_use &&
            ow_strcmp(owlocale64_table[i].code, code) == 0) {
            return &owlocale64_table[i];
        }
    }
    return NULL;
}

bool owlocale_set(const char *code)
{
    const owlocale_t *loc;

    if (!owlocale64_ready) {
        owlocale_init();
    }
    loc = owlocale_lookup(code);
    if (loc == NULL) {
        return false;
    }
    owlocale64_current = loc;
    return true;
}

const owlocale_t *owlocale_get(void)
{
    return owlocale64_current;
}

/* ── Number formatting ────────────────────────────────────────── */

int32_t owlocale_format_number(int64_t value, uint32_t decimals,
                               char *out, size_t out_cap)
{
    const owlocale_t *loc = owlocale64_current;
    char              rev[24];
    size_t            n = 0u;
    uint64_t          mag;
    size_t            int_len;
    owloc_writer_t    w;

    if (loc == NULL) {
        return OWLOCALE_ERR_UNKNOWN;
    }
    if (out == NULL || out_cap == 0u) {
        return OWLOCALE_ERR_PARAM;
    }
    if (decimals > (uint32_t)OWLOCALE64_MAX_DECIMALS) {
        return OWLOCALE_ERR_RANGE;
    }

    /* Magnitude computed without signed overflow at INT64_MIN. */
    if (value < 0) {
        mag = (uint64_t)(-(value + 1)) + 1u;
    } else {
        mag = (uint64_t)value;
    }

    if (mag == 0u) {
        rev[n++] = '0';
    } else {
        while (mag > 0u && n < sizeof(rev)) {
            rev[n++] = (char)('0' + (uint32_t)(mag % 10u));
            mag /= 10u;
        }
    }

    int_len = (n > (size_t)decimals) ? (n - (size_t)decimals) : 0u;

    w.buf      = out;
    w.cap      = out_cap;
    w.len      = 0u;
    w.overflow = false;

    if (value < 0) {
        w_putc(&w, '-');
    }

    if (int_len == 0u) {
        w_putc(&w, '0');
    } else {
        uint32_t group   = loc->group_size;       /* 0 disables grouping */
        uint32_t first   = (group == 0u)
                               ? (uint32_t)int_len
                               : (uint32_t)(int_len % (size_t)group);
        size_t   stop    = n - int_len;           /* LSB index of int part */
        size_t   pos     = n;                     /* one past the MSB    */
        uint32_t emitted = 0u;

        if (first == 0u) {
            first = (group == 0u) ? (uint32_t)int_len : group;
        }

        while (pos > stop) {
            uint32_t take = (emitted == 0u) ? first : group;

            if (emitted > 0u && group != 0u) {
                w_putc(&w, loc->thousands_sep);
            }
            for (uint32_t k = 0u; k < take && pos > stop; ++k) {
                w_putc(&w, rev[--pos]);
                ++emitted;
            }
        }
    }

    if (decimals > 0u) {
        w_putc(&w, loc->decimal_point);
        if (n < (size_t)decimals) {
            uint32_t zeros = decimals - (uint32_t)n;
            for (uint32_t i = 0u; i < zeros; ++i) {
                w_putc(&w, '0');
            }
            for (size_t i = n; i > 0u; --i) {
                w_putc(&w, rev[i - 1u]);
            }
        } else {
            for (size_t i = (size_t)decimals; i > 0u; --i) {
                w_putc(&w, rev[i - 1u]);
            }
        }
    }

    if (w.overflow || w.len + 1u > out_cap) {
        return OWLOCALE_ERR_SPACE;
    }
    out[w.len] = '\0';
    return (int32_t)w.len;
}

/* ── Date formatting ──────────────────────────────────────────── */

/* True when the token at tpl[pos..] matches tok exactly. */
static bool ow_tok_match(const char *tpl, size_t pos, const char *tok)
{
    size_t i = 0u;
    while (tok[i] != '\0') {
        if (tpl[pos + i] != tok[i]) {
            return false;
        }
        ++i;
    }
    return true;
}

int32_t owlocale_format_date(const owdate_t *date, char *out, size_t out_cap)
{
    const owlocale_t *loc = owlocale64_current;
    owloc_writer_t    w;
    size_t            pos = 0u;
    uint32_t          year_mag;
    bool              year_neg;

    if (loc == NULL) {
        return OWLOCALE_ERR_UNKNOWN;
    }
    if (date == NULL || out == NULL || out_cap == 0u) {
        return OWLOCALE_ERR_PARAM;
    }
    if (date->month < 1u || date->month > 12u || date->day < 1u ||
        date->day > 31u || date->hour > 23u || date->minute > 59u ||
        date->second > 59u) {
        return OWLOCALE_ERR_RANGE;
    }

    year_neg = (date->year < 0);
    year_mag = year_neg ? (uint32_t)(-(int64_t)date->year) : (uint32_t)date->year;

    w.buf      = out;
    w.cap      = out_cap;
    w.len      = 0u;
    w.overflow = false;

    while (loc->date_format[pos] != '\0') {
        if (ow_tok_match(loc->date_format, pos, "YYYY")) {
            if (year_neg) {
                w_putc(&w, '-');
            }
            w_u32_pad(&w, year_mag, 4u);
            pos += 4u;
        } else if (ow_tok_match(loc->date_format, pos, "MMMM")) {
            w_puts(&w, owlocale64_month_full[date->month - 1u]);
            pos += 4u;
        } else if (ow_tok_match(loc->date_format, pos, "MMM")) {
            w_puts(&w, owlocale64_month_abbr[date->month - 1u]);
            pos += 3u;
        } else if (ow_tok_match(loc->date_format, pos, "DDDD")) {
            w_puts(&w, (date->weekday <= 6u)
                           ? owlocale64_weekday_full[date->weekday]
                           : "?");
            pos += 4u;
        } else if (ow_tok_match(loc->date_format, pos, "DDD")) {
            w_puts(&w, (date->weekday <= 6u)
                           ? owlocale64_weekday_abbr[date->weekday]
                           : "?");
            pos += 3u;
        } else if (ow_tok_match(loc->date_format, pos, "YY")) {
            w_u32_pad(&w, year_mag % 100u, 2u);
            pos += 2u;
        } else if (ow_tok_match(loc->date_format, pos, "MM")) {
            w_u32_pad(&w, date->month, 2u);
            pos += 2u;
        } else if (ow_tok_match(loc->date_format, pos, "DD")) {
            w_u32_pad(&w, date->day, 2u);
            pos += 2u;
        } else if (ow_tok_match(loc->date_format, pos, "HH")) {
            w_u32_pad(&w, date->hour, 2u);
            pos += 2u;
        } else if (ow_tok_match(loc->date_format, pos, "mm")) {
            w_u32_pad(&w, date->minute, 2u);
            pos += 2u;
        } else if (ow_tok_match(loc->date_format, pos, "ss")) {
            w_u32_pad(&w, date->second, 2u);
            pos += 2u;
        } else if (ow_tok_match(loc->date_format, pos, "M")) {
            w_u32_pad(&w, date->month, 1u);
            pos += 1u;
        } else if (ow_tok_match(loc->date_format, pos, "D")) {
            w_u32_pad(&w, date->day, 1u);
            pos += 1u;
        } else {
            w_putc(&w, loc->date_format[pos]);
            pos += 1u;
        }
    }

    if (w.overflow || w.len + 1u > out_cap) {
        return OWLOCALE_ERR_SPACE;
    }
    out[w.len] = '\0';
    return (int32_t)w.len;
}
