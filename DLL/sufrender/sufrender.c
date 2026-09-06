/*
 * sufrender.c - OpenWindows SuperUnicode Font (.suf) Rasterizer Library Implementation (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "sufrender.h"

void sufrender_init(suf_font_face_t *font)
{
    if (!font) return;
    font->glyph_count = 0u;
    for (size_t i = 0u; i < SUF_CACHED_GLYPHS; ++i) {
        font->glyphs[i].codepoint = 0u;
    }
}

bool sufrender_add_glyph(suf_font_face_t *font, uint32_t cp, const uint8_t bitmap[16])
{
    if (!font || !bitmap) return false;
    if (font->glyph_count >= SUF_CACHED_GLYPHS) return false;

    suf_glyph_entry_t *entry = &font->glyphs[font->glyph_count++];
    entry->codepoint = cp;
    for (size_t r = 0u; r < 16u; ++r) {
        entry->bitmap[r] = bitmap[r];
    }
    return true;
}

const uint8_t *sufrender_get_glyph(const suf_font_face_t *font, uint32_t cp)
{
    if (!font) return NULL;
    for (size_t i = 0u; i < font->glyph_count; ++i) {
        if (font->glyphs[i].codepoint == cp) {
            return font->glyphs[i].bitmap;
        }
    }
    return NULL;
}

void sufrender_blit_char(
    uint32_t *fb, uint32_t fb_pitch,
    int32_t x, int32_t y,
    const uint8_t bitmap[16],
    uint32_t fg_color, uint32_t bg_color)
{
    if (!fb || !bitmap || x < 0 || y < 0) return;

    for (uint32_t r = 0u; r < 16u; ++r) {
        uint8_t row_bits = bitmap[r];
        uint32_t *dst = fb + ((y + r) * (fb_pitch / 4u)) + x;
        for (uint32_t c = 0u; c < 8u; ++c) {
            if (row_bits & (0x80u >> c)) {
                dst[c] = fg_color;
            } else if (bg_color != 0u) {
                dst[c] = bg_color;
            }
        }
    }
}
