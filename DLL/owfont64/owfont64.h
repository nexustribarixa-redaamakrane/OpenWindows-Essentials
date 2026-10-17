/*
 * owfont64.h - OpenWindows Font Rendering Engine (.owd)
 *
 * Renders 1-bpp bitmap fonts into a caller-provided surface. A font is
 * described by an in-memory descriptor holding a fixed-width glyph table
 * (MSB-first rows). Glyphs can optionally be scaled by an integer factor.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWFONT64_H
#define OWFONT64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Public types ─────────────────────────────────────────────── */

#define OWFONT_NAME_MAX   32u   /* stored font face name length      */
#define OWFONT_MAX_SCALE   8u   /* largest integer glyph scale       */

/* Destination surface (same layout as the graphics library). */
typedef struct {
    int32_t  width;
    int32_t  height;
    int32_t  stride;        /* bytes between rows                    */
    uint8_t  bpp;           /* 8, 16, 24 or 32 bits per pixel         */
    uint8_t  reserved[3];
    uint8_t *pixels;
} owfont_surface_t;

/* Bitmap font descriptor supplied by the caller. `glyphs` holds
 * glyph_count glyphs laid out back to back; each glyph is glyph_height
 * rows of row_bytes bytes, one bit per pixel, most-significant bit first.
 * Rows are padded out to row_bytes, so a glyph occupies
 * row_bytes * glyph_height bytes. */
typedef struct {
    char           name[OWFONT_NAME_MAX];
    uint32_t       first_char;   /* codepoint of the first glyph     */
    uint32_t       glyph_count;  /* number of glyphs in the table     */
    uint32_t       glyph_width;  /* cell width in pixels              */
    uint32_t       glyph_height; /* cell height in pixels             */
    uint32_t       row_bytes;    /* bytes per glyph row               */
    uint32_t       advance;      /* horizontal advance, 0 => width    */
    uint32_t       baseline;     /* rows from cell top to baseline    */
    const uint8_t *glyphs;
} owfont_descriptor_t;

/* Measured text extent in pixels. */
typedef struct {
    uint32_t width;
    uint32_t height;
} owfont_size_t;

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize the engine. Must succeed before any other call. */
bool owfont_init(void);

/* Install a bitmap font descriptor; the glyph data is retained by
 * reference. Returns false on a malformed descriptor. */
bool owfont_load(const owfont_descriptor_t *desc);

/* Render one glyph with its top-left cell corner at (x, y). Set bits use
 * `fg`, clear bits use `bg`. Returns false when the codepoint is absent. */
bool owfont_render_glyph(owfont_surface_t *fb, uint32_t codepoint,
                         int32_t x, int32_t y, uint32_t fg, uint32_t bg);

/* Render a NUL-terminated string, honouring '\n'. Carriage returns are
 * ignored. Returns false when no font is loaded. */
bool owfont_render_text(owfont_surface_t *fb, const char *text,
                        int32_t x, int32_t y, uint32_t fg, uint32_t bg);

/* Measure a (possibly multi-line) string. Height is scaled line height. */
bool owfont_measure_text(const char *text, owfont_size_t *out);

/* Set the requested pixel line height; converts to an integer scale
 * clamped to [1, OWFONT_MAX_SCALE]. Returns false when no font is loaded. */
bool owfont_set_size(uint32_t pixel_height);

/* Current scaled line height in pixels, 0 when no font is loaded. */
uint32_t owfont_get_line_height(void);

#endif /* OWFONT64_H */
