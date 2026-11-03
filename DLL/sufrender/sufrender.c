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

bool sufrender_load_suf(suf_font_face_t *font, const void *buffer, size_t size)
{
    suf_header_t header;
    uint32_t entry_index;
    const uint8_t *bitmap;
    size_t bitmap_size;

    if (!font || !buffer ||
        suf_validate_header(buffer, size, &header) != SUF_OK ||
        header.boot_bitmap_width != SUF_GLYPH_WIDTH ||
        header.boot_bitmap_height != SUF_GLYPH_HEIGHT ||
        header.boot_bitmap_bpp != 1u) {
        return false;
    }

    sufrender_init(font);
    if ((header.flags & SUF_FLAG_EXTSUCS) != 0u ||
        header.cmap_size < sizeof(suf_cmap_entry_t)) {
        return false;
    }
    for (entry_index = 0u;
         entry_index < header.cmap_size / sizeof(suf_cmap_entry_t) &&
         font->glyph_count < SUF_CACHED_GLYPHS; ++entry_index) {
        const suf_cmap_entry_t *entry =
            &((const suf_cmap_entry_t *)((const uint8_t *)buffer +
                                         header.cmap_offset))[entry_index];
        if (suf_get_boot_bitmap(buffer, size, entry->glyph_id, &bitmap,
                                &bitmap_size) != SUF_OK ||
            bitmap_size != SUF_GLYPH_HEIGHT ||
            !sufrender_add_glyph(font, entry->codepoint, bitmap)) {
            continue;
        }
    }
    return font->glyph_count != 0u;
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
