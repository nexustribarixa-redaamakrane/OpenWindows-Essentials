/*
 * owtheme64.h - OpenWindows Theme Engine Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWTHEME64_H
#define OWTHEME64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owtheme_init(void);
void owtheme_load(void);
void owtheme_apply(void);
void owtheme_get_color(void);
void owtheme_get_font(void);

#endif /* OWTHEME64_H */

