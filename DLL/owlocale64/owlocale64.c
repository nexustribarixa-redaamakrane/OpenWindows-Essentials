/*
 * owlocale64.c - OpenWindows Locale/i18n Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owlocale64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owlocale64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owlocale_init(void)
{
    (void)owlocale64_ready;
    /* TODO: implement owlocale_init */
}
void owlocale_set(void)
{
    (void)owlocale64_ready;
    /* TODO: implement owlocale_set */
}
void owlocale_get(void)
{
    (void)owlocale64_ready;
    /* TODO: implement owlocale_get */
}
void owlocale_format_number(void)
{
    (void)owlocale64_ready;
    /* TODO: implement owlocale_format_number */
}
void owlocale_format_date(void)
{
    (void)owlocale64_ready;
    /* TODO: implement owlocale_format_date */
}

