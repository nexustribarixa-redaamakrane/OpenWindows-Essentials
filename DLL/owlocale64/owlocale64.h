/*
 * owlocale64.h - OpenWindows Locale/i18n Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWLOCALE64_H
#define OWLOCALE64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owlocale_init(void);
void owlocale_set(void);
void owlocale_get(void);
void owlocale_format_number(void);
void owlocale_format_date(void);

#endif /* OWLOCALE64_H */

