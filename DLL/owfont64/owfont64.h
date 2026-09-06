/*
 * owfont64.h - OpenWindows Font Rendering Engine (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWFONT64_H
#define OWFONT64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owfont_init(void);
void owfont_load(void);
void owfont_render_glyph(void);
void owfont_measure_text(void);
void owfont_set_size(void);

#endif /* OWFONT64_H */

