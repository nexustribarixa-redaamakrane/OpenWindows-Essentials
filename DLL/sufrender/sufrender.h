/*
 * sufrender.h - OpenWindows SuperUnicode Font (.suf) Rasterizer Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef SUFRENDER_H
#define SUFRENDER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SUF_GLYPH_WIDTH     8u
#define SUF_GLYPH_HEIGHT    16u

typedef struct {
    uint32_t codepoint;
    uint8_t  bitmap[16]; /* 8x16 1bpp glyph bitmap */
} suf_glyph_entry_t;

#define SUF_CACHED_GLYPHS   128u

typedef struct {
    suf_glyph_entry_t glyphs[SUF_CACHED_GLYPHS];
    uint32_t          glyph_count;
} suf_font_face_t;

void sufrender_init(suf_font_face_t *font);
bool sufrender_add_glyph(suf_font_face_t *font, uint32_t cp, const uint8_t bitmap[16]);
const uint8_t *sufrender_get_glyph(const suf_font_face_t *font, uint32_t cp);
void sufrender_blit_char(
    uint32_t *fb, uint32_t fb_pitch,
    int32_t x, int32_t y,
    const uint8_t bitmap[16],
    uint32_t fg_color, uint32_t bg_color);

#endif /* SUFRENDER_H */
