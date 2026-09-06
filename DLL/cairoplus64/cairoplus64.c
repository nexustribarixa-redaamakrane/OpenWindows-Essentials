/*
 * cairoplus64.c - cairoplus64.owd implementation (SUTF text on cairo64)
 *
 * A text layer over the genuine cairo engine exported by cairo64.owd.
 * Every cairo_* symbol used here comes from the cairo64 ABI mirror
 * (Extensions/cairo64.h): with CAIRO_WIN32_STATIC_BUILD it binds the
 * vendor archives (hosted tests); without, __declspec(dllimport) binds
 * cairo64.owd at module link.
 *
 * Cell-glyph model (suct-compatible semantics):
 *   - basic Latin runs (0x20..0x7E) use the cairo twin fallback toy font
 *     at size_px; advance comes from real text extents.
 *   - every other SUCS codepoint (PUA cell glyphs, wide CJK, soft
 *     markers) goes through a user-font face whose scaled-font render
 *     callback stamps cell-coverage rectangles into the glyph recording
 *     surface.
 *   - ink geometry is constant in font-design units so the caller's font
 *     size maps directly: ascent 0.8S / descent 0.2S / cell 0.5S.
 *
 * C99 freestanding strict profile; no heap of its own (cairo objects are
 * owned by the layer and go through cairo64's allocator).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cairoplus64.h"        /* Extensions ABI mirror */
#include "owd_format.h"
#include "owc_format.h"
#include "kernel64.h"

/* OWD1 binary header metadata (documentary; see Extensions/owd_format.h). */
#define CAIROPLUS64_LIB_NAME        "cairoplus64.owd"
#define CAIROPLUS64_LIB_TYPE        OWD_LIBTYPE_HYBRID
#define CAIROPLUS64_TARGET_ARCH     0x02u
#define CAIROPLUS64_ALIGNMENT_LOG2  4u
#define CAIROPLUS64_INIT_FLAGS      (OWC_INIT_REQUIRES_OWRP | \
                                     OWC_INIT_REQUIRES_BANC)

#define CAIROPLUS64_LIB_VERSION     "cairoplus64.owd 1.0.0"

static const uint8_t caplus_ident[] = CAIROPLUS64_LIB_VERSION;

/* ------------------------------------------------------------------ */
/*  Module lifecycle                                                   */
/* ------------------------------------------------------------------ */

static bool caplus_initialized = false;

cairoplus64_status_t cairoplus64_module_init(void)
{
    if (caplus_initialized) {
        return CAIROPLUS64_OK;
    }
    if (k64_initialize_api() != K64_OK) {
        return CAIROPLUS64_BAN_K64_BOOT;
    }
    caplus_initialized = true;
    return CAIROPLUS64_OK;
}

cairoplus64_status_t cairoplus64_module_shutdown(void)
{
    if (!caplus_initialized) {
        return CAIROPLUS64_ERR_UNINITIALIZED;
    }
    caplus_initialized = false;
    return CAIROPLUS64_OK;
}

uint32_t cairoplus64_abi_version(void)
{
    return (uint32_t)((1u * 10000u) + (0u * 100u) + 0u);
}

uint16_t cairoplus64_abi_major(void)
{
    return 1u;
}

uint16_t cairoplus64_abi_minor(void)
{
    return 0u;
}

const uint8_t *cairoplus64_ident(void)
{
    return caplus_ident;
}

/*
 * PE DLL entry for the .owd (GNU ld needs an entry address). Mirrors the
 * cairo64 module: a pure attach stub that runs nothing.
 */
#if defined(_WIN32)
unsigned long __stdcall owdplus64_dll_ep(void)
{
    return 1ul;
}
#endif

/* ------------------------------------------------------------------ */
/*  Palette                                                            */
/* ------------------------------------------------------------------ */

static const uint8_t caplus_palette_rgb[CAIROPLUS64_PALETTE_COUNT][3] = {
    {0x00, 0x00, 0x00}, {0x80, 0x00, 0x00}, {0x00, 0x80, 0x00},
    {0x80, 0x80, 0x00}, {0x00, 0x00, 0x80}, {0x80, 0x00, 0x80},
    {0x00, 0x80, 0x80}, {0xC0, 0xC0, 0xC0}, {0x80, 0x80, 0x80},
    {0xFF, 0x00, 0x00}, {0x00, 0xFF, 0x00}, {0xFF, 0xFF, 0x00},
    {0x00, 0x00, 0xFF}, {0xFF, 0x00, 0xFF}, {0x00, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF}
};

uint32_t cairoplus64_palette_argb(uint8_t idx)
{
    if (idx >= CAIROPLUS64_PALETTE_COUNT) {
        idx = 0u;
    }
    return (0xFF000000u |
            ((uint32_t)caplus_palette_rgb[idx][0] << 16u) |
            ((uint32_t)caplus_palette_rgb[idx][1] <<  8u) |
            ((uint32_t)caplus_palette_rgb[idx][2]));
}

/* ------------------------------------------------------------------ */
/*  Cell width model                                                   */
/* ------------------------------------------------------------------ */

static bool caplus_is_cjk_wide(sucs_char_t cp)
{
    if (cp >= 0x1100u && cp <= 0x115Fu)       return true; /* Hangul Jamo   */
    if (cp >= 0x2E80u && cp <= 0x303Eu)       return true; /* CJK radicals  */
    if (cp >= 0x3041u && cp <= 0x33FFu)       return true; /* Kana / CJK    */
    if (cp >= 0x3400u && cp <= 0x4DBFu)       return true; /* CJK ext A     */
    if (cp >= 0x4E00u && cp <= 0x9FFFu)       return true; /* CJK unified   */
    if (cp >= 0xA000u && cp <= 0xA4CFu)       return true; /* Yi            */
    if (cp >= 0xAC00u && cp <= 0xD7A3u)       return true; /* Hangul syll.  */
    if (cp >= 0xF900u && cp <= 0xFAFFu)       return true; /* CJK compat    */
    if (cp >= 0xFE30u && cp <= 0xFE4Fu)       return true; /* CJK compat F  */
    if (cp >= 0xFF00u && cp <= 0xFF60u)       return true; /* Fullwidth     */
    if (cp >= 0xFFE0u && cp <= 0xFFE6u)       return true; /* Fullwidth F   */
    if (cp >= 0x20000u && cp <= 0x2FFFDu)     return true; /* Supp A wide   */
    return false;
}

static bool caplus_is_zero_width(sucs_char_t cp)
{
    if (cp >= 0x0300u && cp <= 0x036Fu) return true; /* combining marks */
    switch (cp) {
    case 0x200Bu: case 0x200Cu: case 0x200Du:
    case 0x2060u: case 0xFEFFu:
        return true;
    default:
        return false;
    }
}

uint8_t cairoplus64_cell_width(sucs_char_t cp)
{
    if (cp == CAIROPLUS64_GLYPH_CONT || cp == CAIROPLUS64_GLYPH_BLANK) {
        return 0u;                            /* continuation / blank */
    }
    if (caplus_is_zero_width(cp)) {
        return 0u;
    }
    if (caplus_is_cjk_wide(cp)) {
        return 2u;
    }
    return 1u;
}

/* ------------------------------------------------------------------ */
/*  User-font callbacks (SUCS cell glyphs)                             */
/* ------------------------------------------------------------------ */

/*
 * Design constants in font units. With font size S set by the caller,
 * cairo composites design units * S/72, so ink = 0.8S tall (0.8 ascent +
 * 0.2 descent) and a cell = 0.5S wide.
 */
#define CA_PLUS_ASC_DESIGN    57.6
#define CA_PLUS_DESC_DESIGN   14.4
#define CA_PLUS_CELL_DESIGN   36.0

static cairo_status_t
caplus_scaled_font_init(cairo_scaled_font_t  *scaled_font,
                        cairo_t              *cr,
                        cairo_font_extents_t *extents)
{
    (void)scaled_font;
    (void)cr;
    extents->ascent  = CA_PLUS_ASC_DESIGN;
    extents->descent = CA_PLUS_DESC_DESIGN;
    extents->height  = CA_PLUS_ASC_DESIGN + CA_PLUS_DESC_DESIGN;
    extents->max_x_advance = CA_PLUS_CELL_DESIGN * 2.0;
    extents->max_y_advance = 0.0;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
caplus_scaled_font_render_glyph(cairo_scaled_font_t  *scaled_font,
                                unsigned long         glyph,
                                cairo_t              *cr,
                                cairo_text_extents_t *metrics)
{
    sucs_char_t cp = (sucs_char_t)glyph;
    double w;

    (void)scaled_font;

    if (!sucs_is_valid(cp)) {
        metrics->x_advance = 0.0;
        return CAIRO_STATUS_SUCCESS;
    }

    w = (double)cairoplus64_cell_width(cp) * CA_PLUS_CELL_DESIGN;

    if (cp != CAIROPLUS64_GLYPH_BLANK) {
        cairo_save(cr);
        cairo_rectangle(cr, 0.0, -CA_PLUS_ASC_DESIGN, w,
                        CA_PLUS_ASC_DESIGN);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    metrics->x_advance = w;
    metrics->y_advance = 0.0;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
caplus_scaled_font_unicode_to_glyph(cairo_scaled_font_t *scaled_font,
                                    unsigned long        unicode,
                                    unsigned long       *glyph)
{
    (void)scaled_font;
    *glyph = unicode;
    return CAIRO_STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static void caplus_set_source_argb(cairo_t *cr, uint32_t argb)
{
    cairo_set_source_rgba(cr,
                          (double)((argb >> 16u) & 0xFFu) / 255.0,
                          (double)((argb >>  8u) & 0xFFu) / 255.0,
                          (double)( argb        & 0xFFu) / 255.0,
                          (double)((argb >> 24u) & 0xFFu) / 255.0);
}

static uint32_t caplus_attr_fg(cairoplus64_layer_t *L, uint16_t attrs)
{
    uint8_t  idx  = (uint8_t)(attrs & CAIROPLUS64_ATTR_FG_MASK);
    uint32_t argb = cairoplus64_palette_argb(idx);

    if (attrs & CAIROPLUS64_ATTR_BOLD) {
        uint8_t r = (uint8_t)((argb >> 16u) & 0xFFu);
        uint8_t g = (uint8_t)((argb >>  8u) & 0xFFu);
        uint8_t b = (uint8_t)( argb        & 0xFFu);
        r = (uint8_t)((r * 3u + 255u) / 4u);
        g = (uint8_t)((g * 3u + 255u) / 4u);
        b = (uint8_t)((b * 3u + 255u) / 4u);
        argb = 0xFF000000u | ((uint32_t)r << 16u) |
               ((uint32_t)g << 8u) | b;
    }
    if (attrs & CAIROPLUS64_ATTR_DIM) {
        argb = 0xFF000000u |
               (((argb >> 16u) & 0xFFu) / 2u) << 16u |
               (((argb >>  8u) & 0xFFu) / 2u) <<  8u |
               ((argb        & 0xFFu) / 2u);
    }
    (void)L;
    return argb;
}

static uint32_t caplus_attr_bg(cairoplus64_layer_t *L, uint16_t attrs)
{
    uint8_t idx = (uint8_t)((attrs & CAIROPLUS64_ATTR_BG_MASK) >> 4u);
    (void)L;
    return cairoplus64_palette_argb(idx);
}

static void caplus_select_toy(cairo_t *cr, double size_px, bool bold)
{
    cairo_select_font_face(cr, "twin",
                           CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD
                                : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size_px);
}

static double caplus_cp_advance(cairoplus64_layer_t *L, sucs_char_t cp)
{
    if (cp >= 0x20u && cp <= 0x7Eu) {
        char ch[2];
        cairo_text_extents_t te;

        ch[0] = (char)(uint8_t)cp;
        ch[1] = '\0';

        cairo_save(L->cr);
        caplus_select_toy(L->cr, L->size_px, false);
        cairo_text_extents(L->cr, ch, &te);
        cairo_restore(L->cr);
        return te.x_advance;
    }
    return (double)cairoplus64_cell_width(cp) * (L->cell_px);
}

/* ------------------------------------------------------------------ */
/*  Layer API                                                          */
/* ------------------------------------------------------------------ */

cairoplus64_status_t cairoplus64_layer_open(cairoplus64_layer_t *L,
                                            cairo_surface_t    *target,
                                            uint32_t            fg_argb,
                                            uint32_t            bg_argb,
                                            double              size_px)
{
    cairo_font_face_t *face;

    if (!L || !target || size_px <= 0.0) {
        return CAIROPLUS64_ERR_BAD_LAYER;
    }
    if (!caplus_initialized) {
        return CAIROPLUS64_ERR_UNINITIALIZED;
    }

    face = cairo_user_font_face_create();
    if (!face) {
        return CAIROPLUS64_BAN_FONT_FAULT;
    }
    cairo_user_font_face_set_init_func(face, caplus_scaled_font_init);
    cairo_user_font_face_set_render_glyph_func(face,
                                               caplus_scaled_font_render_glyph);
    cairo_user_font_face_set_unicode_to_glyph_func(
        face, caplus_scaled_font_unicode_to_glyph);

    L->target     = target;
    L->cr         = cairo_create(target);
    L->ufont      = face;
    L->fg_argb    = fg_argb;
    L->bg_argb    = bg_argb;
    L->size_px    = size_px;
    L->asc_px     = 0.8 * size_px;
    L->desc_px    = 0.2 * size_px;
    L->cell_px    = 0.5 * size_px;
    L->palette_fg = 0xFFu;
    L->palette_bg = 0xFFu;
    L->active     = true;
    L->puts       = 0u;
    L->flushes    = 0u;

    if (!L->cr) {
        cairo_font_face_destroy(face);
        L->ufont  = NULL;
        L->active = false;
        return CAIROPLUS64_BAN_CTX_FAULT;
    }

    return CAIROPLUS64_OK;
}

cairoplus64_status_t cairoplus64_layer_close(cairoplus64_layer_t *L)
{
    if (!L || !L->active) {
        return CAIROPLUS64_ERR_BAD_LAYER;
    }
    cairo_surface_flush(L->target);
    cairo_destroy(L->cr);
    cairo_font_face_destroy(L->ufont);
    L->cr     = NULL;
    L->ufont  = NULL;
    L->target = NULL;
    L->active = false;
    L->flushes++;
    return CAIROPLUS64_OK;
}

cairoplus64_status_t cairoplus64_layer_set_style(cairoplus64_layer_t *L,
                                                 uint32_t fg_argb,
                                                 uint32_t bg_argb)
{
    if (!L || !L->active) {
        return CAIROPLUS64_ERR_BAD_LAYER;
    }
    L->fg_argb = fg_argb;
    L->bg_argb = bg_argb;
    L->palette_fg = 0xFFu;
    L->palette_bg = 0xFFu;
    return CAIROPLUS64_OK;
}

cairoplus64_status_t cairoplus64_layer_put_cp(cairoplus64_layer_t *L,
                                              double x, double y,
                                              sucs_char_t cp,
                                              uint16_t attrs,
                                              double *x_out)
{
    uint32_t fg, bg;
    uint8_t  cells;
    double   width;

    if (!L || !L->active) {
        return CAIROPLUS64_ERR_BAD_LAYER;
    }

    cells = cairoplus64_cell_width(cp);
    if (cells == 0u) {
        if (x_out) *x_out = x;
        return CAIROPLUS64_OK;
    }

    width = L->cell_px * (double)cells;
    fg    = caplus_attr_fg(L, attrs);
    bg    = caplus_attr_bg(L, attrs);
    if (attrs & CAIROPLUS64_ATTR_INVERSE) {
        uint32_t t = fg; fg = bg; bg = t;
    }

    cairo_save(L->cr);

    /* Background cell (ink top at baseline - ascent). */
    caplus_set_source_argb(L->cr, bg);
    cairo_rectangle(L->cr, x, y - L->asc_px, width, L->size_px);
    cairo_fill(L->cr);

    /* Underline just below the baseline. */
    if (attrs & CAIROPLUS64_ATTR_UNDERLINE) {
        caplus_set_source_argb(L->cr, fg);
        cairo_rectangle(L->cr, x, y + L->desc_px * 0.2,
                        width, L->size_px * 0.04);
        cairo_fill(L->cr);
    }

    if (cp >= 0x20u && cp <= 0x7Eu) {
        char ch[2];
        ch[0] = (char)(uint8_t)cp;
        ch[1] = '\0';

        caplus_select_toy(L->cr, L->size_px,
                          (attrs & CAIROPLUS64_ATTR_BOLD) != 0u);
        caplus_set_source_argb(L->cr, fg);
        cairo_move_to(L->cr, x, y);
        cairo_show_text(L->cr, ch);
    } else {
        cairo_glyph_t g;

        g.index = (unsigned long)cp;
        g.x = x;
        g.y = y;

        cairo_set_font_face(L->cr, L->ufont);
        cairo_set_font_size(L->cr, L->size_px);
        caplus_set_source_argb(L->cr, fg);
        cairo_show_glyphs(L->cr, &g, 1);
    }

    cairo_restore(L->cr);
    L->puts++;

    if (x_out) *x_out = x + caplus_cp_advance(L, cp);
    return CAIROPLUS64_OK;
}

cairoplus64_status_t cairoplus64_layer_write_sutf8(cairoplus64_layer_t *L,
                                                   double x, double y,
                                                   const uint8_t *sutf8,
                                                   size_t len,
                                                   uint16_t attrs,
                                                   double *x_out)
{
    size_t off = 0u;
    double pen = x;

    if (!L || !L->active) {
        return CAIROPLUS64_ERR_BAD_LAYER;
    }
    if (!sutf8) {
        return CAIROPLUS64_ERR_BAD_STREAM;
    }
    if (len == 0u) {
        while (sutf8[len] != 0u) {
            len++;
        }
    }

    while (off < len) {
        sucs_char_t cp;
        size_t n = sutf8_decode_char(sutf8 + off, len - off, &cp);
        if (n == 0u || !sucs_is_valid(cp)) {
            if (x_out) *x_out = pen;
            return CAIROPLUS64_ERR_BAD_STREAM;
        }
        {
            double nx;
            cairoplus64_status_t st =
                cairoplus64_layer_put_cp(L, pen, y, cp, attrs, &nx);
            if (st != CAIROPLUS64_OK) {
                if (x_out) *x_out = pen;
                return st;
            }
            pen = nx;
        }
        off += n;
    }

    if (x_out) *x_out = pen;
    return CAIROPLUS64_OK;
}

cairoplus64_status_t cairoplus64_layer_measure(const cairoplus64_layer_t *L,
                                               const uint8_t *sutf8,
                                               size_t len,
                                               double *advance_out)
{
    cairoplus64_layer_t local;
    size_t off = 0u;
    double adv = 0.0;

    if (!L || !L->active || !advance_out) {
        return CAIROPLUS64_ERR_BAD_LAYER;
    }
    if (!sutf8) {
        return CAIROPLUS64_ERR_BAD_STREAM;
    }
    if (len == 0u) {
        while (sutf8[len] != 0u) {
            len++;
        }
    }

    /* Measure needs a live cr for toy extents; a shallow copy of the layer
     * state is fine since every mutating helper save/restores. */
    local = *L;

    while (off < len) {
        sucs_char_t cp;
        size_t n = sutf8_decode_char(sutf8 + off, len - off, &cp);
        if (n == 0u || !sucs_is_valid(cp)) {
            return CAIROPLUS64_ERR_BAD_STREAM;
        }
        adv += caplus_cp_advance(&local, cp);
        off += n;
    }

    *advance_out = adv;
    return CAIROPLUS64_OK;
}

cairoplus64_status_t cairoplus64_layer_flush(cairoplus64_layer_t *L)
{
    if (!L || !L->active) {
        return CAIROPLUS64_ERR_BAD_LAYER;
    }
    cairo_surface_flush(L->target);
    L->flushes++;
    return CAIROPLUS64_OK;
}