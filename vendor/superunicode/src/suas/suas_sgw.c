/* SUAS-002 — System Glyph Width & Monospace Grid (SGW)
 *
 * Reference implementation of docs/suas/SUAS-002-sgw.md.
 *
 * Freestanding C99, zero allocation. Input codepoints are 64-bit ExtSUCS
 * (sucs_ex_char_t). Classification uses a curated East_Asian_Width-style
 * range table over the Unicode Bridge plus zone dispatch for the rest of the
 * SUCS space; grid cell count and column advance are O(1).
 */

#include "suas/suas_sgw.h"

#define SGW_NELEM(a) (sizeof((a)) / sizeof((a)[0]))

typedef struct {
    sucs_ex_char_t   lo;
    sucs_ex_char_t   hi;
    suas_sgw_width_t cls;
} sgw_range_t;

/* ── Curated East_Asian_Width-style table (Unicode Bridge) ────────────
 * Sorted ascending by lo; non-overlapping. Codepoints not listed default to
 * Narrow (Na) for the Bridge. Semantics follow UAX #11 + UTS #51. */

static const sgw_range_t SGW_BRIDGE_TABLE[] = {
    /* Neutral overrides (decompositions start with a Narrow form). */
    { 0x00C5, 0x00C5, SUAS_SGW_W_NEUTRAL }, /* Å */
    { 0x01D3, 0x01D3, SUAS_SGW_W_NEUTRAL }, /* Ǔ */

    /* Ambiguous. */
    { 0x01D4, 0x01D4, SUAS_SGW_W_AMBIGUOUS }, /* ǔ */

    /* Hangul Jamo. */
    { 0x1100, 0x115F, SUAS_SGW_W_WIDE },

    /* Halfwidth: U+20A9 WON SIGN (per EAW H rule without <narrow>). */
    { 0x20A9, 0x20A9, SUAS_SGW_W_HALFWIDTH },

    /* Ambiguous: U+212B ANGSTROM SIGN. */
    { 0x212B, 0x212B, SUAS_SGW_W_AMBIGUOUS },

    /* CJK Radicals Supplement, Kangxi Radicals. */
    { 0x2E80, 0x2FDF, SUAS_SGW_W_WIDE },

    /* CJK Symbols and Punctuation (incl. U+3000 ideographic space). */
    { 0x3000, 0x303E, SUAS_SGW_W_WIDE },

    /* Hiragana .. Enclosed CJK .. CJK Compatibility. */
    { 0x3041, 0x33FF, SUAS_SGW_W_WIDE },

    /* CJK Extension A; CJK Unified Ideographs. */
    { 0x3400, 0x4DBF, SUAS_SGW_W_WIDE },
    { 0x4E00, 0x9FFF, SUAS_SGW_W_WIDE },

    /* Yi Syllables. */
    { 0xA000, 0xA4CF, SUAS_SGW_W_WIDE },

    /* Hangul Jamo Extended-A; Hangul Syllables. */
    { 0xA960, 0xA97F, SUAS_SGW_W_WIDE },
    { 0xAC00, 0xD7A3, SUAS_SGW_W_WIDE },
    { 0xD7B0, 0xD7FF, SUAS_SGW_W_WIDE },

    /* CJK Compatibility Ideographs; Vertical Forms. */
    { 0xF900, 0xFAFF, SUAS_SGW_W_WIDE },
    { 0xFE10, 0xFE19, SUAS_SGW_W_WIDE },

    /* CJK Compatibility Forms. */
    { 0xFE30, 0xFE52, SUAS_SGW_W_WIDE },
    { 0xFE54, 0xFE66, SUAS_SGW_W_WIDE },
    { 0xFE68, 0xFE6B, SUAS_SGW_W_WIDE },

    /* Fullwidth Forms (F) and Halfwidth Forms (H). */
    { 0xFF00, 0xFF00, SUAS_SGW_W_FULLWIDTH },
    { 0xFF01, 0xFF5E, SUAS_SGW_W_FULLWIDTH },
    { 0xFF61, 0xFF9F, SUAS_SGW_W_HALFWIDTH },
    { 0xFFA0, 0xFFBE, SUAS_SGW_W_HALFWIDTH },
    { 0xFFC2, 0xFFC7, SUAS_SGW_W_HALFWIDTH },
    { 0xFFCA, 0xFFCF, SUAS_SGW_W_HALFWIDTH },
    { 0xFFD2, 0xFFD7, SUAS_SGW_W_HALFWIDTH },
    { 0xFFDA, 0xFFDC, SUAS_SGW_W_HALFWIDTH },
    { 0xFFE0, 0xFFE6, SUAS_SGW_W_FULLWIDTH },
    { 0xFFE8, 0xFFEE, SUAS_SGW_W_HALFWIDTH },

    /* Ideographic marks & Kana extension. */
    { 0x16FE0, 0x16FE4, SUAS_SGW_W_WIDE },
    { 0x17000, 0x187F7, SUAS_SGW_W_WIDE }, /* Tangut */
    { 0x18800, 0x18CD5, SUAS_SGW_W_WIDE }, /* Tangut Components */
    { 0x1B000, 0x1B2FB, SUAS_SGW_W_WIDE }, /* Kana Extension */

    /* Emoji_Presentation is Wide — except Regional_Indicator (U+1F1E6..1F1FF)
     * which is not listed and therefore resolves Narrow (per UTS #51). */
    { 0x1F004, 0x1F004, SUAS_SGW_W_WIDE },
    { 0x1F0CF, 0x1F0CF, SUAS_SGW_W_WIDE },
    { 0x1F18E, 0x1F18E, SUAS_SGW_W_WIDE },
    { 0x1F191, 0x1F19A, SUAS_SGW_W_WIDE },
    { 0x1F200, 0x1F202, SUAS_SGW_W_WIDE },
    { 0x1F210, 0x1F23B, SUAS_SGW_W_WIDE },
    { 0x1F240, 0x1F248, SUAS_SGW_W_WIDE },
    { 0x1F250, 0x1F251, SUAS_SGW_W_WIDE },
    { 0x1F260, 0x1F265, SUAS_SGW_W_WIDE },
    { 0x1F300, 0x1F320, SUAS_SGW_W_WIDE },
    { 0x1F32D, 0x1F335, SUAS_SGW_W_WIDE },
    { 0x1F337, 0x1F37C, SUAS_SGW_W_WIDE },
    { 0x1F37E, 0x1F393, SUAS_SGW_W_WIDE },
    { 0x1F3A0, 0x1F3CA, SUAS_SGW_W_WIDE },
    { 0x1F3CF, 0x1F3D3, SUAS_SGW_W_WIDE },
    { 0x1F3E0, 0x1F3F0, SUAS_SGW_W_WIDE },
    { 0x1F3F4, 0x1F3F4, SUAS_SGW_W_WIDE },
    { 0x1F3F8, 0x1F43E, SUAS_SGW_W_WIDE },
    { 0x1F440, 0x1F440, SUAS_SGW_W_WIDE },
    { 0x1F442, 0x1F4FC, SUAS_SGW_W_WIDE },
    { 0x1F4FF, 0x1F53D, SUAS_SGW_W_WIDE },
    { 0x1F54B, 0x1F54E, SUAS_SGW_W_WIDE },
    { 0x1F550, 0x1F567, SUAS_SGW_W_WIDE },
    { 0x1F57A, 0x1F57A, SUAS_SGW_W_WIDE },
    { 0x1F595, 0x1F596, SUAS_SGW_W_WIDE },
    { 0x1F5A4, 0x1F5A4, SUAS_SGW_W_WIDE },
    { 0x1F5FB, 0x1F64F, SUAS_SGW_W_WIDE },
    { 0x1F680, 0x1F6C5, SUAS_SGW_W_WIDE },
    { 0x1F6CC, 0x1F6CC, SUAS_SGW_W_WIDE },
    { 0x1F6D0, 0x1F6D2, SUAS_SGW_W_WIDE },
    { 0x1F6D5, 0x1F6D7, SUAS_SGW_W_WIDE },
    { 0x1F6DC, 0x1F6DF, SUAS_SGW_W_WIDE },
    { 0x1F6EB, 0x1F6EC, SUAS_SGW_W_WIDE },
    { 0x1F6F4, 0x1F6FC, SUAS_SGW_W_WIDE },
    { 0x1F7E0, 0x1F7EB, SUAS_SGW_W_WIDE },
    { 0x1F7F0, 0x1F7F0, SUAS_SGW_W_WIDE },
    { 0x1F90C, 0x1F93A, SUAS_SGW_W_WIDE },
    { 0x1F93C, 0x1F945, SUAS_SGW_W_WIDE },
    { 0x1F947, 0x1F9FF, SUAS_SGW_W_WIDE },
    { 0x1FA70, 0x1FAFF, SUAS_SGW_W_WIDE },

    /* CJK Extension B..F, G (incl. unassigned Han ranges → Wide). */
    { 0x20000, 0x2FFFD, SUAS_SGW_W_WIDE },
    { 0x30000, 0x3FFFD, SUAS_SGW_W_WIDE },
};

/* Binary search over the sorted range table. Returns the matching class or 0
 * (not found) as a boolean. */
static int sgw_bridge_lookup(uint32_t cp, suas_sgw_width_t* out)
{
    size_t lo = 0, hi = SGW_NELEM(SGW_BRIDGE_TABLE);
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const sgw_range_t* r = &SGW_BRIDGE_TABLE[mid];
        if (cp < (uint32_t)r->lo) hi = mid;
        else if (cp > (uint32_t)r->hi) lo = mid + 1;
        else { *out = r->cls; return 1; }
    }
    return 0;
}

static int sgw_find_override(const suas_sgw_options_t* o, sucs_ex_char_t cp,
                             suas_sgw_width_t* out)
{
    if (o == NULL || o->overrides == NULL || o->count == 0) return 0;
    size_t lo = 0, hi = o->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const suas_sgw_override_t* r = &o->overrides[mid];
        if (cp < r->lo) hi = mid;
        else if (cp > r->hi) lo = mid + 1;
        else { *out = r->cls; return 1; }
    }
    return 0;
}

const char* suas_sgw_version_string(void)
{
    return "SUAS-002 SGW 0.1.0";
}

void suas_sgw_options_default(suas_sgw_options_t* o)
{
    if (o == NULL) return;
    o->overrides = NULL;
    o->count = 0;
}

/* Whether a Bridge codepoint resolves to two cells given its class. */
static int sgw_class_wide(suas_sgw_width_t w, bool wide_context)
{
    switch (w) {
    case SUAS_SGW_W_FULLWIDTH:
    case SUAS_SGW_W_WIDE:
        return 1;
    case SUAS_SGW_W_AMBIGUOUS:
        return wide_context ? 1 : 0;
    default:
        return 0; /* H, Na, N — single cell */
    }
}

suas_sgw_width_t suas_sgw_resolve(sucs_ex_char_t cp, const suas_sgw_options_t* o)
{
    suas_sgw_width_t cls;

    /* Tailoring override takes absolute precedence. */
    if (sgw_find_override(o, cp, &cls)) return cls;

    if (cp <= SGW_ZONE_UNICODE_MAX) {
        if (sgw_bridge_lookup((uint32_t)cp, &cls)) return cls;
        return SUAS_SGW_W_NARROW; /* Bridge default: Narrow */
    }
    if (cp <= SGW_BASE_SUCS_MAX) {
        /* Native SUCS or SCP or trap - all resolve Neutral. */
        return SUAS_SGW_W_NEUTRAL;
    }
    /* ExtSUCS plugin space (and everything above Base). */
    return SUAS_SGW_W_NEUTRAL;
}

bool suas_sgw_is_halfwidth(sucs_ex_char_t cp, const suas_sgw_options_t* o)
{
    suas_sgw_width_t w = suas_sgw_resolve(cp, o);
    return (w == SUAS_SGW_W_HALFWIDTH) || (w == SUAS_SGW_W_NARROW) ||
           (w == SUAS_SGW_W_NEUTRAL) || (w == SUAS_SGW_W_AMBIGUOUS);
}

bool suas_sgw_is_fullwidth(sucs_ex_char_t cp, const suas_sgw_options_t* o)
{
    suas_sgw_width_t w = suas_sgw_resolve(cp, o);
    return (w == SUAS_SGW_W_FULLWIDTH) || (w == SUAS_SGW_W_WIDE);
}

suas_sgw_grid_t suas_sgw_resolve_ambiguous(sucs_ex_char_t cp, bool wide_context,
                                           const suas_sgw_options_t* o)
{
    (void)cp; (void)o;
    return wide_context ? SUAS_SGW_GRID_TWO : SUAS_SGW_GRID_ONE;
}

/* Computes the grid metric, honoring the per-zone control policy. The
 * override path is honored everywhere; otherwise SCP/trap/sentinel are the
 * zero-cell control plane. */
suas_sgw_grid_t suas_sgw_cells(sucs_ex_char_t cp, bool wide_context,
                               const suas_sgw_options_t* o)
{
    suas_sgw_width_t cls;

    if (sgw_find_override(o, cp, &cls))
        return sgw_class_wide(cls, wide_context) ? SUAS_SGW_GRID_TWO
                                                 : SUAS_SGW_GRID_ONE;

    /* Control plane, trap range, and sentinel are non-advancing. */
    if (cp >= SGW_ZONE_SCP_MIN && cp <= SGW_ZONE_SCP_MAX) return SUAS_SGW_GRID_NONE;
    if (cp >= SGW_BASE_TRAP_MIN && cp <= SGW_BASE_TRAP_MAX) return SUAS_SGW_GRID_NONE;
    if (cp == SGW_BASE_SENTINEL) return SUAS_SGW_GRID_NONE;

    if (cp <= SGW_ZONE_UNICODE_MAX) {
        suas_sgw_width_t w;
        if (sgw_bridge_lookup((uint32_t)cp, &w))
            return sgw_class_wide(w, wide_context) ? SUAS_SGW_GRID_TWO
                                                   : SUAS_SGW_GRID_ONE;
        return SUAS_SGW_GRID_ONE; /* Bridge default: one cell */
    }
    /* Native SUCS and ExtSUCS plugin space: single cell by default. */
    return SUAS_SGW_GRID_ONE;
}

suas_sgw_status_t suas_sgw_column_advance(sucs_ex_char_t cp, bool wide_context,
                                          const suas_sgw_options_t* o,
                                          size_t* col)
{
    if (col == NULL) return SUAS_SGW_ERR_NULL_POINTER;
    *col += (size_t)suas_sgw_cells(cp, wide_context, o);
    return SUAS_SGW_OK;
}

suas_sgw_status_t suas_sgw_grid(const sucs_ex_char_t* cps, size_t n,
                                bool wide_context, const suas_sgw_options_t* o,
                                suas_sgw_grid_t* out, size_t* out_cols)
{
    size_t i, col = 0;
    if (cps == NULL) return SUAS_SGW_ERR_NULL_POINTER;
    if (out == NULL) return SUAS_SGW_ERR_NULL_POINTER;
    for (i = 0; i < n; ++i) {
        suas_sgw_grid_t g = suas_sgw_cells(cps[i], wide_context, o);
        out[i] = g;
        col += (size_t)g;
        if (out_cols) out_cols[i] = col;
    }
    return SUAS_SGW_OK;
}
