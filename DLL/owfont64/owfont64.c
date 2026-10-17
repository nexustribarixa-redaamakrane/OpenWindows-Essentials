/*
 * owfont64.c - OpenWindows Font Rendering Engine (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owfont64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owfont64_ready = false;
static bool owfont64_loaded = false;
static owfont_descriptor_t owfont64_font;
static uint32_t owfont64_scale = 1u;

/* ── Internal helpers ─────────────────────────────────────────── */

/* Bytes occupied by one pixel of the given depth, 0 when unsupported. */
static size_t owfont_bpp_bytes(uint8_t bpp)
{
    switch (bpp) {
    case 8:  return 1;
    case 16: return 2;
    case 24: return 3;
    case 32: return 4;
    default: return 0;
    }
}

static bool owfont_surface_valid(const owfont_surface_t *fb)
{
    if (!owfont64_ready || !fb || !fb->pixels) return false;
    if (fb->width <= 0 || fb->height <= 0 || fb->stride <= 0) return false;
    if (owfont_bpp_bytes(fb->bpp) == 0) return false;
    return true;
}

static void owfont_store_pixel(uint8_t *pixels, int32_t stride, uint8_t bpp,
                               int32_t x, int32_t y, uint32_t color)
{
    uint8_t *p = pixels + ((size_t)y * (size_t)stride) +
                 ((size_t)x * owfont_bpp_bytes(bpp));

    switch (bpp) {
    case 8:
        p[0] = (uint8_t)(color & 0xFFu);
        break;
    case 16: {
        uint16_t v = (uint16_t)((((color >> 19) & 0x001Fu) << 11) |
                                (((color >> 10) & 0x003Fu) << 5)  |
                                 ((color >> 3)  & 0x001Fu));
        p[0] = (uint8_t)(v & 0x00FFu);
        p[1] = (uint8_t)((v >> 8) & 0x00FFu);
        break;
    }
    case 24:
        p[0] = (uint8_t)(color & 0xFFu);
        p[1] = (uint8_t)((color >> 8) & 0xFFu);
        p[2] = (uint8_t)((color >> 16) & 0xFFu);
        break;
    case 32:
        p[0] = (uint8_t)(color & 0xFFu);
        p[1] = (uint8_t)((color >> 8) & 0xFFu);
        p[2] = (uint8_t)((color >> 16) & 0xFFu);
        p[3] = 0xFFu;
        break;
    default:
        break;
    }
}

static void owfont_store_pixel_checked(owfont_surface_t *fb, int32_t x,
                                       int32_t y, uint32_t color)
{
    if (x < 0 || x >= fb->width || y < 0 || y >= fb->height) return;
    owfont_store_pixel(fb->pixels, fb->stride, fb->bpp, x, y, color);
}

/* Copy the descriptor face name, always NUL-terminating. */
static void owfont_copy_name(owfont_descriptor_t *dst,
                             const owfont_descriptor_t *src)
{
    uint32_t i;

    for (i = 0; i < OWFONT_NAME_MAX - 1u && src->name[i] != '\0'; ++i) {
        dst->name[i] = src->name[i];
    }
    dst->name[i] = '\0';
}

/* Extract one glyph bit (col, row) from the active font table. */
static bool owfont_glyph_bit(const owfont_descriptor_t *font,
                             uint32_t glyph_index, uint32_t col, uint32_t row)
{
    uint32_t glyph_bytes = font->row_bytes * font->glyph_height;
    uint32_t byte_index = (glyph_index * glyph_bytes) +
                          (row * font->row_bytes) + (col >> 3);
    uint8_t  b = font->glyphs[byte_index];

    return ((b >> (7u - (col & 7u))) & 1u) != 0u;
}

/* Effective horizontal advance for one glyph, in scaled pixels. */
static uint32_t owfont_advance_scaled(void)
{
    uint32_t base = (owfont64_font.advance != 0u) ? owfont64_font.advance
                                                  : owfont64_font.glyph_width;
    return base * owfont64_scale;
}

/* ── Implementation ──────────────────────────────────────────── */

bool owfont_init(void)
{
    owfont64_ready = true;
    owfont64_loaded = false;
    owfont64_scale = 1u;
    return owfont64_ready;
}

bool owfont_load(const owfont_descriptor_t *desc)
{
    if (!owfont64_ready) return false;
    if (!desc || !desc->glyphs) return false;
    if (desc->glyph_count == 0u) return false;
    if (desc->glyph_width == 0u || desc->glyph_height == 0u) return false;

    /* Each row must have room for the declared width. */
    uint32_t min_row = (desc->glyph_width + 7u) / 8u;
    if (desc->row_bytes < min_row) return false;

    /* Guard against a glyph table size overflow before we index it. */
    uint64_t bytes = (uint64_t)desc->row_bytes * (uint64_t)desc->glyph_height;
    if (bytes == 0u || bytes > (uint64_t)0xFFFFFFFFu) return false;

    owfont_copy_name(&owfont64_font, desc);
    owfont64_font.first_char   = desc->first_char;
    owfont64_font.glyph_count  = desc->glyph_count;
    owfont64_font.glyph_width  = desc->glyph_width;
    owfont64_font.glyph_height = desc->glyph_height;
    owfont64_font.row_bytes    = desc->row_bytes;
    owfont64_font.advance      = (desc->advance != 0u) ? desc->advance
                                                       : desc->glyph_width;
    owfont64_font.baseline     = desc->baseline;
    owfont64_font.glyphs       = desc->glyphs;

    owfont64_loaded = true;
    owfont64_scale = 1u;
    return true;
}

bool owfont_render_glyph(owfont_surface_t *fb, uint32_t codepoint,
                         int32_t x, int32_t y, uint32_t fg, uint32_t bg)
{
    if (!owfont64_loaded) return false;
    if (!owfont_surface_valid(fb)) return false;
    if (codepoint < owfont64_font.first_char) return false;

    uint32_t glyph_index = codepoint - owfont64_font.first_char;
    if (glyph_index >= owfont64_font.glyph_count) return false;

    uint32_t scale = owfont64_scale;

    for (uint32_t row = 0; row < owfont64_font.glyph_height; ++row) {
        for (uint32_t col = 0; col < owfont64_font.glyph_width; ++col) {
            bool  on    = owfont_glyph_bit(&owfont64_font, glyph_index, col, row);
            uint32_t col_bits = on ? fg : bg;

            /* Expand each font pixel into a scale x scale block. */
            for (uint32_t sy = 0; sy < scale; ++sy) {
                int32_t py = y + (int32_t)(row * scale + sy);
                for (uint32_t sx = 0; sx < scale; ++sx) {
                    int32_t px = x + (int32_t)(col * scale + sx);
                    owfont_store_pixel_checked(fb, px, py, col_bits);
                }
            }
        }
    }

    return true;
}

bool owfont_render_text(owfont_surface_t *fb, const char *text,
                        int32_t x, int32_t y, uint32_t fg, uint32_t bg)
{
    if (!owfont64_loaded) return false;
    if (!owfont_surface_valid(fb)) return false;
    if (!text) return false;

    uint32_t line_height = owfont_get_line_height();
    uint32_t advance = owfont_advance_scaled();
    int32_t  start_x = x;
    int32_t  cursor_x = x;
    int32_t  cursor_y = y;

    for (size_t i = 0; text[i] != '\0'; ++i) {
        uint8_t ch = (uint8_t)text[i];

        if (ch == (uint8_t)'\n') {
            cursor_x = start_x;
            cursor_y += (int32_t)line_height;
            continue;
        }
        if (ch == (uint8_t)'\r') continue;

        (void)owfont_render_glyph(fb, (uint32_t)ch, cursor_x, cursor_y, fg, bg);
        cursor_x += (int32_t)advance;
    }

    return true;
}

bool owfont_measure_text(const char *text, owfont_size_t *out)
{
    if (!owfont64_loaded || !out) return false;
    if (!text) return false;

    uint32_t advance = owfont_advance_scaled();
    uint32_t line_width = 0u;
    uint32_t max_width = 0u;
    uint32_t lines = 1u;

    for (size_t i = 0; text[i] != '\0'; ++i) {
        uint8_t ch = (uint8_t)text[i];

        if (ch == (uint8_t)'\n') {
            if (line_width > max_width) max_width = line_width;
            line_width = 0u;
            ++lines;
            continue;
        }
        if (ch == (uint8_t)'\r') continue;

        line_width += advance;
    }
    if (line_width > max_width) max_width = line_width;

    out->width  = max_width;
    out->height = lines * owfont_get_line_height();
    return true;
}

bool owfont_set_size(uint32_t pixel_height)
{
    if (!owfont64_loaded) return false;
    if (pixel_height == 0u) return false;

    uint32_t scale = pixel_height / owfont64_font.glyph_height;
    if (scale == 0u) scale = 1u;
    if (scale > OWFONT_MAX_SCALE) scale = OWFONT_MAX_SCALE;

    owfont64_scale = scale;
    return true;
}

uint32_t owfont_get_line_height(void)
{
    if (!owfont64_loaded) return 0u;
    return owfont64_font.glyph_height * owfont64_scale;
}
