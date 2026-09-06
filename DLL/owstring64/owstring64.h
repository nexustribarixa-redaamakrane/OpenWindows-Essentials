/*
 * owstring64.h - OpenWindows String Operations Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWSTRING64_H
#define OWSTRING64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owstr_len(void);
void owstr_copy(void);
void owstr_compare(void);
void owstr_concat(void);
void owstr_find(void);
void owstr_to_upper(void);
void owstr_to_lower(void);
void owstr_format(void);

#endif /* OWSTRING64_H */

