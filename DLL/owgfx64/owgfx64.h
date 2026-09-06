/*
 * owgfx64.h - OpenWindows Graphics Primitives Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWGFX64_H
#define OWGFX64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owgfx_init(void);
void owgfx_draw_line(void);
void owgfx_draw_rect(void);
void owgfx_draw_circle(void);
void owgfx_fill_rect(void);
void owgfx_blit_bitmap(void);

#endif /* OWGFX64_H */

