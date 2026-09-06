/*
 * cairoplus64.h - cairoplus64.owd ABI mirror (SUTF text on cairo64)
 *
 * Freestanding C99 ABI for the cairoplus64 module: an SUTF text layer that
 * renders SUCS codepoints onto a genuine-cairo image surface using the
 * cairo user-font pipeline (SUCS PUA / cell glyphs rasterized as coverage
 * through a user scaled-font) and the twin fallback toy font for basic
 * Latin runs. Everything below is a header-only contract + the cairo64
 * draw API; the implementation lives in cairoplus64.owd.
 *
 * Layer geometry model (matched to suct.owd cell semantics):
 *   - each cell is cell-px wide; 1-cell glyphs advance 0.5*size, 2-cell
 *     glyphs advance 1.0*size, 0-cell glyphs advance nothing.
 *   - baseline sits at (0,0); ink occupies [0, asc_px] up from baseline.
 */

#ifndef OWE_CAIROPLUS64_H
#define OWE_CAIROPLUS64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cairo64.h"
#include "sucs_types.h"
#include "sutf8.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Status Codes - BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A7FF): Fatal module faults                    */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal degradations                 */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                     */
/* ------------------------------------------------------------------ */

typedef uint32_t cairoplus64_status_t;

#define CAIROPLUS64_OK                         0x00000000u  /* Success        */
/* B+ Fatal */
#define CAIROPLUS64_BAN_K64_BOOT               0x0011A240u  /* k64 API dead   */
#define CAIROPLUS64_BAN_FONT_FAULT             0x0011A241u  /* scaled font NG */
#define CAIROPLUS64_BAN_CTX_FAULT              0x0011A242u  /* compositor NG  */
/* W+ Warning */
#define CAIROPLUS64_ERR_CLIPPED                0x0011AA40u  /* drew off-target */
/* S+ Soft */
#define CAIROPLUS64_ERR_UNINITIALIZED          0x0011AE40u  /* Not init yet   */
#define CAIROPLUS64_ERR_BAD_LAYER              0x0011AE41u  /* Layer not open */
#define CAIROPLUS64_ERR_BAD_STREAM             0x0011AE42u  /* Malformed SUTF */
#define CAIROPLUS64_ERR_PUA_VIOLATION          0x0011AE43u  /* PUA out of zone */

/* ------------------------------------------------------------------ */
/*  Cell Attributes (suct.owd compatible bit layout)                   */
/* ------------------------------------------------------------------ */

#define CAIROPLUS64_ATTR_FG_MASK        0x000Fu   /* 4-bit palette index */
#define CAIROPLUS64_ATTR_BG_MASK        0x00F0u   /* 4-bit palette index */
#define CAIROPLUS64_ATTR_BOLD           0x0100u
#define CAIROPLUS64_ATTR_DIM            0x0200u
#define CAIROPLUS64_ATTR_INVERSE        0x0400u
#define CAIROPLUS64_ATTR_UNDERLINE      0x0800u
#define CAIROPLUS64_ATTR_WIDE_LEAD      0x1000u
#define CAIROPLUS64_ATTR_WIDE_TAIL      0x2000u

#define CAIROPLUS64_PALETTE_BLACK       0xFF000000u
#define CAIROPLUS64_PALETTE_MAROON      0xFF800000u
#define CAIROPLUS64_PALETTE_GREEN       0xFF008000u
#define CAIROPLUS64_PALETTE_OLIVE       0xFF808000u
#define CAIROPLUS64_PALETTE_NAVY        0xFF000080u
#define CAIROPLUS64_PALETTE_PURPLE      0xFF800080u
#define CAIROPLUS64_PALETTE_TEAL        0xFF008080u
#define CAIROPLUS64_PALETTE_SILVER      0xFFC0C0C0u
#define CAIROPLUS64_PALETTE_GRAY        0xFF808080u
#define CAIROPLUS64_PALETTE_RED         0xFFFF0000u
#define CAIROPLUS64_PALETTE_LIME        0xFF00FF00u
#define CAIROPLUS64_PALETTE_YELLOW      0xFFFFFF00u
#define CAIROPLUS64_PALETTE_BLUE        0xFF0000FFu
#define CAIROPLUS64_PALETTE_FUCHSIA     0xFFFF00FFu
#define CAIROPLUS64_PALETTE_AQUA        0xFF00FFFFu
#define CAIROPLUS64_PALETTE_WHITE       0xFFFFFFFFu
#define CAIROPLUS64_PALETTE_COUNT       16u

/* ------------------------------------------------------------------ */
/*  SUCS PUA zones re-used by the cell-glyph renderer                  */
/* ------------------------------------------------------------------ */

#define CAIROPLUS64_PUA_BMP_START      0x00E000u
#define CAIROPLUS64_PUA_BMP_END        0x00F8FFu
#define CAIROPLUS64_OW_GLYPHZONE_START 0x00EE00u
#define CAIROPLUS64_OW_GLYPHZONE_END   0x00EE7Fu
#define CAIROPLUS64_OW_BOOTZONE_START  0x00EF00u
#define CAIROPLUS64_OW_BOOTZONE_END    0x00EF3Fu
#define CAIROPLUS64_OW_TELZONE_START   0x00EF80u
#define CAIROPLUS64_OW_TELZONE_END     0x00EF9Fu
#define CAIROPLUS64_GLYPH_CONT         0x00EE00u  /* wide tail continuation */
#define CAIROPLUS64_GLYPH_BLANK        0x00EE01u  /* blank cell tail        */

/* ------------------------------------------------------------------ */
/*  Text Layer                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    cairo_surface_t *target;      /* bound image/recording surface     */
    cairo_t         *cr;          /* draw context over target          */
    cairo_font_face_t *ufont;     /* user-font face (SUCS cell glyphs) */
    uint32_t        fg_argb;      /* active foreground                 */
    uint32_t        bg_argb;      /* active background                 */
    double          size_px;      /* cell height in target pixels      */
    double          asc_px;       /* ascent  (ink above baseline)      */
    double          desc_px;      /* descent (ink below baseline)      */
    double          cell_px;      /* width of a 1-cell glyph           */
    uint8_t         palette_fg;   /* last set 4-bit fg index (0xFF=n/a)*/
    uint8_t         palette_bg;   /* last set 4-bit bg index (0xFF=n/a)*/
    bool            active;
    uint64_t        puts;
    uint64_t        flushes;
} cairoplus64_layer_t;

/* ------------------------------------------------------------------ */
/*  Module lifecycle & identity                                        */
/* ------------------------------------------------------------------ */

cairoplus64_status_t cairoplus64_module_init(void);
cairoplus64_status_t cairoplus64_module_shutdown(void);
uint32_t cairoplus64_abi_version(void);
uint16_t cairoplus64_abi_major(void);
uint16_t cairoplus64_abi_minor(void);
const uint8_t *cairoplus64_ident(void);

/* Width class of a codepoint in the cell model: 0 (none), 1, 2. */
uint8_t cairoplus64_cell_width(sucs_char_t cp);

/* Map a 4-bit palette index to a 0xAARRGGBB value. */
uint32_t cairoplus64_palette_argb(uint8_t idx);

/* ------------------------------------------------------------------ */
/*  Text layer API                                                     */
/* ------------------------------------------------------------------ */

/*
 * Open a text layer over `target`. fg/bg are 0xAARRGGBB defaults; size_px
 * is the cell height. The layer creates its own draw context and font
 * faces (through cairo64); accessible until close.
 */
cairoplus64_status_t cairoplus64_layer_open(cairoplus64_layer_t *L,
                                            cairo_surface_t    *target,
                                            uint32_t            fg_argb,
                                            uint32_t            bg_argb,
                                            double              size_px);

/* Tear the layer down; flushes the target and releases the context. */
cairoplus64_status_t cairoplus64_layer_close(cairoplus64_layer_t *L);

/* Set default fg/bg (0xAARRGGBB). */
cairoplus64_status_t cairoplus64_layer_set_style(cairoplus64_layer_t *L,
                                                 uint32_t fg_argb,
                                                 uint32_t bg_argb);

/*
 * Draw one SUCS codepoint at (x, baseline_y) with suct-style attrs.
 * Advances x_out by the cell width of the glyph.
 */
cairoplus64_status_t cairoplus64_layer_put_cp(cairoplus64_layer_t *L,
                                              double x, double y,
                                              sucs_char_t cp,
                                              uint16_t attrs,
                                              double *x_out);

/*
 * Draw a SUTF-8 stream at (x, baseline_y). `len` bounds the buffer
 * (0 = NUL-terminated). Stops cleanly at the first malformed encoding
 * and reports CAIROPLUS64_ERR_BAD_STREAM. x_out (optional) receives the
 * advance to the end of the drawn run.
 */
cairoplus64_status_t cairoplus64_layer_write_sutf8(cairoplus64_layer_t *L,
                                                   double x, double y,
                                                   const uint8_t *sutf8,
                                                   size_t len,
                                                   uint16_t attrs,
                                                   double *x_out);

/* Measure the advance of a SUTF-8 run without drawing. */
cairoplus64_status_t cairoplus64_layer_measure(const cairoplus64_layer_t *L,
                                               const uint8_t *sutf8,
                                               size_t len,
                                               double *advance_out);

/* Push pending rendering to the target surface. */
cairoplus64_status_t cairoplus64_layer_flush(cairoplus64_layer_t *L);

#ifdef __cplusplus
}
#endif

#endif /* OWE_CAIROPLUS64_H */