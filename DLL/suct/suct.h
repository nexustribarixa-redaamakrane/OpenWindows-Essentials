/*
 * suct.h - SuperUnicode Text Services (.owd)
 *
 * Subsystem library implementing the SuperUnicode text protocol for user and
 * kernel layers:
 *
 *   - Private Use Area (PUA) glyph zones: OpenWindows assigns compact
 *     PUA codepoints inside the BMP PUA tail (0xEE00-0xEFFF) for the OWM
 *     cell glyph set, boot-stage glyphs and telemetry markers. All glyph
 *     ids map 1:1 to SUCS PUA codepoints and back.
 *
 *   - Legacy encoding bridges: ASCII-8 and UTF-16LE byte streams decode to
 *     SUCS 31-bit codepoints and re-encode as SUTF-8 transport frames.
 *
 *   - Proportional cell width model (zero / single / double) applied to the
 *     monospaced text plane used by bootvid and console replay.
 *
 *   - OWM text plane: raster-independent cell buffer with attributes,
 *     cursor, wrap/scroll, and SUTF-8 row serialization for hot replay.
 *
 * Ring Portal integration: a console sink can be bound to owrp.owd so rows
 * flush through OWRP_SYS_SIO_WRITE (ring-3 to serial driver). suct.owd
 * links against owrp.owd.
 *
 * Conforms to OWD1 binary layout (Extensions/owd_format.h).
 * C99 freestanding - stdint/stdbool/stddef only, zero heap.
 */

#ifndef OWE_SUCT_H
#define OWE_SUCT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "owd_format.h"
#include "owc_format.h"
#include "owrp.h"
#include "sucs_types.h"
#include "sutf8.h"

/* ------------------------------------------------------------------ */
/*  OWD1 Binary Header Metadata                                        */
/* ------------------------------------------------------------------ */

#define SUCT_LIB_NAME            "suct.owd"
#define SUCT_LIB_TYPE            OWD_LIBTYPE_HYBRID
#define SUCT_TARGET_ARCH         0x02u
#define SUCT_ALIGNMENT_LOG2      4u
#define SUCT_INIT_FLAGS          (OWC_INIT_REQUIRES_OWRP | \
                                  OWC_INIT_REQUIRES_BANC)

/* ------------------------------------------------------------------ */
/*  Status Codes - BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A7FF): Fatal plane corruption                 */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal truncation                   */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                     */
/* ------------------------------------------------------------------ */

typedef uint32_t suct_status_t;

#define SUCT_OK                         0x00000000u  /* Success            */
/* B+ Fatal */
#define SUCT_BAN_PLANE_CORRUPT          0x0011A1C0u  /* Plane state bad    */
#define SUCT_BAN_ZONE_VIOLATION         0x0011A1C1u  /* PUA out of zone    */
#define SUCT_BAN_ROW_OVERFLOW           0x0011A1C2u  /* Row growth blew    */
#define SUCT_BAN_CONSOLE_FAULT          0x0011A1C3u  /* Sink transport     */
/* W+ Warning */
#define SUCT_ERR_TRUNCATED              0x0011A946u  /* Stream cut short   */
/* S+ Soft */
#define SUCT_ERR_NOT_INIT               0x0011AE90u  /* Not initialized    */
#define SUCT_ERR_NULL_POINTER           0x0011AE91u  /* Null argument      */
#define SUCT_ERR_ROW_RANGE              0x0011AE92u  /* Row out of range   */
#define SUCT_ERR_COL_RANGE              0x0011AE93u  /* Column out of range*/
#define SUCT_ERR_NO_SINK                0x0011AE94u  /* Console unbound    */
#define SUCT_ERR_BAD_STREAM             0x0011AE95u  /* Malformed SUTF-8   */
#define SUCT_ERR_UNTRANSMITTABLE        0x0011AE96u  /* Codepoint barred   */

/* ------------------------------------------------------------------ */
/*  PUA Glyph Zones (OpenWindows assignments)                          */
/*  All zones sit inside the SUCS BMP PUA tail 0xEE00-0xEFFF.          */
/* ------------------------------------------------------------------ */

#define SUCT_PUA_BMP_START          0x00E000u
#define SUCT_PUA_BMP_END            0x00F8FFu
#define SUCT_PUA_SUPP_A_START       0x0F0000u
#define SUCT_PUA_SUPP_A_END         0x0FFFFDu
#define SUCT_PUA_SUPP_B_START       0x100000u
#define SUCT_PUA_SUPP_B_END         0x10FFFDu

#define SUCT_OW_GLYPHZONE_START     0x00EE00u     /* OWM cell glyphs    */
#define SUCT_OW_GLYPHZONE_END       0x00EE7Fu     /* 128 slots          */
#define SUCT_OW_GLYPHZONE_COUNT     128u
#define SUCT_OW_BOOTZONE_START      0x00EF00u     /* boot-stage glyphs  */
#define SUCT_OW_BOOTZONE_END        0x00EF3Fu     /* 64 slots           */
#define SUCT_OW_TELZONE_START       0x00EF80u     /* telemetry markers  */
#define SUCT_OW_TELZONE_END         0x00EF9Fu     /* 32 slots           */

/* Reserved zone markers (continuation cell of a wide glyph, blank tail). */
#define SUCT_GLYPH_CONT             0x00EE00u
#define SUCT_GLYPH_BLANK            0x00EE01u

/* ------------------------------------------------------------------ */
/*  Cell Attributes                                                   */
/* ------------------------------------------------------------------ */

#define SUCT_ATTR_FG_MASK           0x000Fu
#define SUCT_ATTR_BG_MASK           0x00F0u
#define SUCT_ATTR_BOLD              0x0100u
#define SUCT_ATTR_DIM               0x0200u
#define SUCT_ATTR_INVERSE           0x0400u
#define SUCT_ATTR_UNDERLINE         0x0800u
#define SUCT_ATTR_WIDE_LEAD         0x1000u
#define SUCT_ATTR_WIDE_TAIL         0x2000u

/* ------------------------------------------------------------------ */
/*  Text Plane Geometry                                               */
/* ------------------------------------------------------------------ */

#define SUCT_PLANE_ROWS             25u
#define SUCT_PLANE_COLS             80u
#define SUCT_ROW_BYTES_MAX          (SUCT_PLANE_COLS * 6u)  /* SUTF-8 cap */

typedef struct {
    sucs_char_t cp;                  /* SUCS codepoint                    */
    uint16_t    attrs;               /* SUCT_ATTR_*                       */
} suct_cell_t;

typedef struct {
    suct_cell_t cells[SUCT_PLANE_ROWS][SUCT_PLANE_COLS];
    uint16_t cursor_row;
    uint16_t cursor_col;
    uint16_t fg;
    uint16_t bg;
    bool      wrap;
    bool      initialized;
    uint64_t  writes;
    uint64_t  scrolls;
} suct_plane_t;

/* ------------------------------------------------------------------ */
/*  Console + Sink                                                     */
/* ------------------------------------------------------------------ */

typedef void (*suct_emit_fn)(uintptr_t token, const uint8_t *sutf8,
                             uint32_t len);

typedef struct {
    suct_emit_fn emit;
    uintptr_t    token;
} suct_sink_t;

typedef struct {
    suct_plane_t plane;
    suct_sink_t  sink;
    bool         sink_bound;
    uint32_t     dirty_rows;          /* bit per row: pending replay */
} suct_console_t;

/* ================================================================== */
/*  Public API                                                        */
/* ================================================================== */

/* PUA geometry ------------------------------------------------------ */

bool suct_is_pua(sucs_char_t cp);
bool suct_is_ow_glyph(sucs_char_t cp);            /* any OW zone */
uint32_t suct_ow_glyph_id(sucs_char_t cp);        /* 0xFFFFFFFF if none */
sucs_char_t suct_ow_glyph_cp(uint8_t zone, uint32_t glyph_id);

/* Cell width model -------------------------------------------------- */

uint8_t suct_cell_width(sucs_char_t cp);          /* 0, 1, or 2 cells */

/* Legacy encoding bridges ------------------------------------------- */

suct_status_t suct_ascii_to_sucs(const uint8_t *ascii, size_t len,
                                 sucs_char_t *out, size_t cap,
                                 size_t *out_count);
suct_status_t suct_utf16le_to_sucs(const uint8_t *u16, size_t bytes,
                                   sucs_char_t *out, size_t cap,
                                   size_t *out_count);

/* Stream validation -------------------------------------------------- */

suct_status_t suct_stream_validate(const uint8_t *sutf8,
                                   size_t *out_codepoints);

/* Text plane --------------------------------------------------------- */

suct_status_t suct_plane_init(suct_plane_t *plane);
suct_status_t suct_plane_clear(suct_plane_t *plane);
suct_status_t suct_plane_set_style(suct_plane_t *plane,
                                   uint16_t fg, uint16_t bg);
suct_status_t suct_plane_put_cp(suct_plane_t *plane, uint16_t row,
                                uint16_t col, sucs_char_t cp,
                                uint16_t attrs);
suct_status_t suct_plane_write_sutf8(suct_plane_t *plane, uint16_t row,
                                     uint16_t col, const uint8_t *sutf8,
                                     uint16_t attrs, uint16_t *out_col);
suct_status_t suct_plane_cursor_put_sutf8(suct_plane_t *plane,
                                          const uint8_t *sutf8);
suct_status_t suct_plane_newline(suct_plane_t *plane);
suct_status_t suct_plane_scroll(suct_plane_t *plane, uint16_t lines);
suct_status_t suct_plane_row_to_sutf8(const suct_plane_t *plane,
                                      uint16_t row, uint8_t *out,
                                      size_t cap, size_t *out_len);

/* Console (plane + sink) --------------------------------------------- */

suct_status_t suct_console_init(suct_console_t *console);
suct_status_t suct_console_bind_sink(suct_console_t *console,
                                     suct_sink_t sink);
suct_status_t suct_console_write_sutf8(suct_console_t *console,
                                       const uint8_t *sutf8);
suct_status_t suct_console_render(suct_console_t *console);

/* OWRP-backed sink binding (owrp.owd link) ---------------------------- */

suct_status_t suct_console_bind_owrp(suct_console_t *console,
                                     owrp_context_t *g);

#endif /* OWE_SUCT_H */