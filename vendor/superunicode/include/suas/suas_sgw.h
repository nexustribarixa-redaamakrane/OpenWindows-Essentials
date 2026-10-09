#ifndef SUAS_SUAS_SGW_H
#define SUAS_SUAS_SGW_H

/* SUAS-002 — System Glyph Width & Monospace Grid (SGW)
 *
 * Core architecture for single-byte / fixed-cell layout metrics: an
 * East_Asian_Width-style width classification over the full 64-bit SUCS
 * codepoint space, plus an O(1) monospace grid model for terminal emulators
 * and CJK/ideographic framebuffer consoles. Zero allocation, freestanding C99.
 *
 * See docs/suas/SUAS-002-sgw.md for the normative specification.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ──────────────────────────────────────────────────────── */
#define SUAS_SGW_VERSION_MAJOR 0
#define SUAS_SGW_VERSION_MINOR 1
#define SUAS_SGW_VERSION_PATCH 0

/* ── 64-bit ExtSUCS codepoint type (guarded to coexist) ───────────── */
#ifndef EXTSUCS_CHAR_T_DEFINED
#define EXTSUCS_CHAR_T_DEFINED
typedef uint64_t sucs_ex_char_t;
#endif

/* ── Base SUCS zone boundaries (duplicated for freestanding SGW) ──── */
#define SGW_ZONE_UNICODE_MAX  ((sucs_ex_char_t)0x0010FFFFULL)
#define SGW_ZONE_SCP_MIN      ((sucs_ex_char_t)0x00110000ULL)
#define SGW_ZONE_SCP_MAX      ((sucs_ex_char_t)0x0011FFFFULL)
#define SGW_ZONE_NATIVE_MIN   ((sucs_ex_char_t)0x00120000ULL)
#define SGW_BASE_SUCS_MAX     ((sucs_ex_char_t)0x7FFFFFFFULL)
#define SGW_BASE_TRAP_MIN     ((sucs_ex_char_t)0x7FFFFFF0ULL)
#define SGW_BASE_TRAP_MAX     ((sucs_ex_char_t)0x7FFFFFFEULL)
#define SGW_BASE_SENTINEL     ((sucs_ex_char_t)0x7FFFFFFFULL)

/* ── Width classes (the six East_Asian_Width-analogous classes) ───── */
typedef enum {
    SUAS_SGW_W_NEUTRAL     = 0, /* N:  neither wide nor narrow         */
    SUAS_SGW_W_NARROW      = 1, /* Na: one cell (default)              */
    SUAS_SGW_W_WIDE        = 2, /* W:  two cells                       */
    SUAS_SGW_W_AMBIGUOUS   = 3, /* A:  one or two cells by context     */
    SUAS_SGW_W_FULLWIDTH   = 4, /* F:  two cells, <wide> decomp        */
    SUAS_SGW_W_HALFWIDTH   = 5  /* H:  one cell, <narrow> decomp / ₩   */
} suas_sgw_width_t;

/* ── Grid cell count (storage-independent fixed-cell metric) ──────── */
typedef enum {
    SUAS_SGW_GRID_NONE     = 0, /* combining / control:  no cell       */
    SUAS_SGW_GRID_ONE      = 1, /* halfwidth / narrow / neutral: 1 cell */
    SUAS_SGW_GRID_TWO      = 2  /* fullwidth / wide: 2 cells           */
} suas_sgw_grid_t;

/* ── Status codes ─────────────────────────────────────────────────── */
typedef enum {
    SUAS_SGW_OK                  =  0,
    SUAS_SGW_ERR_INVALID_ARG     = -1,
    SUAS_SGW_ERR_BUFFER_TOO_SMALL = -2,
    SUAS_SGW_ERR_NULL_POINTER    = -3
} suas_sgw_status_t;

/* ── Per-instance tailoring override entry (sorted range list) ────────
 * Consulted before the normative table. A single codepoint is written as
 * { cp, cp, class }. The list MUST be sorted ascending by lo and free of
 * overlaps. When count == 0 (default), the normative table applies. */
typedef struct {
    sucs_ex_char_t   lo;
    sucs_ex_char_t   hi;
    suas_sgw_width_t cls;
} suas_sgw_override_t;

typedef struct {
    const suas_sgw_override_t* overrides;
    size_t                     count;
} suas_sgw_options_t;

/* ── Setup ────────────────────────────────────────────────────────── */
const char* suas_sgw_version_string(void);
void        suas_sgw_options_default(suas_sgw_options_t* o);

/* ── Classification (§5) — works for any 64-bit codepoint ─────────── */
/* Resolve a codepoint to its width class per §5.4 zone dispatch. */
suas_sgw_width_t suas_sgw_resolve(sucs_ex_char_t cp, const suas_sgw_options_t* o);

/* Halfwidth vs fullwidth booleans; convenience over resolve(). */
bool suas_sgw_is_halfwidth(sucs_ex_char_t cp, const suas_sgw_options_t* o);
bool suas_sgw_is_fullwidth(sucs_ex_char_t cp, const suas_sgw_options_t* o);

/* ── Grid metric (§3) — the O(1) fixed-cell contract ──────────────── */
/* Number of monospace cells a codepoint occupies (0/1/2) in the given
 * context. Ambiguous codepoints resolve to 2 when wide_context is true. */
suas_sgw_grid_t suas_sgw_cells(sucs_ex_char_t cp, bool wide_context,
                               const suas_sgw_options_t* o);

/* Column cursor advance: returns *col advanced by the grid width of cp.
 * Non-advancing marks/controls leave col unchanged. O(1), no allocation. */
suas_sgw_status_t suas_sgw_column_advance(sucs_ex_char_t cp, bool wide_context,
                                          const suas_sgw_options_t* o,
                                          size_t* col);

/* Fill col[i] with the cell count of each codepoint and, when out_cols is
 * non-NULL, the cumulative column position after each codepoint. */
suas_sgw_status_t suas_sgw_grid(const sucs_ex_char_t* cps, size_t n,
                                bool wide_context, const suas_sgw_options_t* o,
                                suas_sgw_grid_t* out, size_t* out_cols);

/* ── Ambiguous contextual resolution (§5.2) ───────────────────────── */
/* Returns GRID_TWO when wide_context is true, else GRID_ONE. */
suas_sgw_grid_t suas_sgw_resolve_ambiguous(sucs_ex_char_t cp, bool wide_context,
                                           const suas_sgw_options_t* o);

#ifdef __cplusplus
}
#endif

#endif /* SUAS_SUAS_SGW_H */
