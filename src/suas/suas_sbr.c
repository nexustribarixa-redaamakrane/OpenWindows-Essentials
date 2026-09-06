/* SUAS-003 — System Boundary & Line Break Rules (SBR)
 *
 * Reference implementation of docs/suas/SUAS-003-sbr.md.
 *
 * Freestanding C99, zero allocation. Input codepoints are 64-bit ExtSUCS
 * (sucs_ex_char_t). A single-pass deterministic state transition table
 * (UAX #14 analog) resolves adjacent pairs to a break status in O(1):
 * Unicode Bridge via a curated break-class lookup + transition matrix,
 * SCP via explicit break markers, Native SUCS via a high-bit range bitmask.
 */

#include "suas/suas_sbr.h"

#define SBR_NELEM(a) (sizeof((a)) / sizeof((a)[0]))

typedef struct {
    sucs_ex_char_t        lo;
    sucs_ex_char_t        hi;
    suas_sbr_break_class_t cls;
} sbr_range_t;

/* ── Curated UAX #14-style break-class table (Unicode Bridge) ────────
 * Sorted ascending by lo; non-overlapping. Codepoints not listed default to
 * XX (unknown / default). Semantics follow UAX #14. */

static const sbr_range_t SBR_BRIDGE_TABLE[] = {
    /* Mandatory breaks: LF, CR, VT, LS, PS. Sorted ascending. */
    { 0x000A, 0x000B, SUAS_SBR_CLS_LF }, /* LF, VT */
    { 0x000D, 0x000D, SUAS_SBR_CLS_CR },
    { 0x0020, 0x0020, SUAS_SBR_CLS_SP },
    { 0x0021, 0x0021, SUAS_SBR_CLS_EX }, /* ! */
    { 0x0022, 0x0022, SUAS_SBR_CLS_QU }, /* " */
    { 0x0024, 0x0024, SUAS_SBR_CLS_PR }, /* $ */
    { 0x0027, 0x0027, SUAS_SBR_CLS_QU }, /* ' */
    { 0x0028, 0x0028, SUAS_SBR_CLS_OP }, /* ( */
    { 0x0029, 0x0029, SUAS_SBR_CLS_CP }, /* ) */
    { 0x002C, 0x002C, SUAS_SBR_CLS_IS }, /* , */
    { 0x002D, 0x002D, SUAS_SBR_CLS_HY }, /* HYPHEN-MINUS */
    { 0x002E, 0x002E, SUAS_SBR_CLS_IS }, /* . */
    { 0x0030, 0x0039, SUAS_SBR_CLS_NU }, /* 0-9 */
    { 0x003A, 0x003B, SUAS_SBR_CLS_IS }, /* : ; */
    { 0x003F, 0x003F, SUAS_SBR_CLS_EX }, /* ? */
    { 0x0041, 0x005A, SUAS_SBR_CLS_AL }, /* A-Z */
    { 0x005B, 0x005B, SUAS_SBR_CLS_OP },
    { 0x005D, 0x005D, SUAS_SBR_CLS_CP },
    { 0x0061, 0x007A, SUAS_SBR_CLS_AL }, /* a-z */
    { 0x007B, 0x007B, SUAS_SBR_CLS_OP },
    { 0x007D, 0x007D, SUAS_SBR_CLS_CP },
    { 0x00A0, 0x00A0, SUAS_SBR_CLS_GL }, /* NBSP */
    { 0x00A3, 0x00A3, SUAS_SBR_CLS_PR }, /* £ */
    { 0x00A5, 0x00A5, SUAS_SBR_CLS_PR }, /* ¥ */
    { 0x00C0, 0x024F, SUAS_SBR_CLS_AL }, /* Latin-1 Supplement..Ext B */
    { 0x0300, 0x036F, SUAS_SBR_CLS_CM }, /* Combining Diacriticals */
    { 0x0370, 0x03FF, SUAS_SBR_CLS_AL }, /* Greek */
    { 0x0400, 0x052F, SUAS_SBR_CLS_AL }, /* Cyrillic */
    { 0x05D0, 0x05EA, SUAS_SBR_CLS_HL }, /* Hebrew letters */
    { 0x200B, 0x200B, SUAS_SBR_CLS_ZW }, /* ZWSP */
    { 0x200D, 0x200D, SUAS_SBR_CLS_ZWJ }, /* ZWJ */
    { 0x2010, 0x2010, SUAS_SBR_CLS_BA }, /* HYPHEN */
    { 0x2011, 0x2011, SUAS_SBR_CLS_GL }, /* NON-BREAKING HYPHEN */
    { 0x2018, 0x201F, SUAS_SBR_CLS_QU }, /* curly quotes */
    { 0x2028, 0x2029, SUAS_SBR_CLS_BK }, /* LS, PS */
    { 0x202F, 0x202F, SUAS_SBR_CLS_GL }, /* NNBSP */
    { 0x2060, 0x2060, SUAS_SBR_CLS_WJ }, /* WJ */
    { 0x2E80, 0x2FFF, SUAS_SBR_CLS_ID }, /* Radicals/Kangxi */
    { 0x3001, 0x3001, SUAS_SBR_CLS_BA }, /* IDEOGRAPHIC COMMA */
    { 0x3002, 0x3002, SUAS_SBR_CLS_CL }, /* IDEOGRAPHIC FULL STOP */
    { 0x3041, 0x30FF, SUAS_SBR_CLS_ID }, /* Hiragana/Katakana */
    { 0x3400, 0x4DBF, SUAS_SBR_CLS_ID }, /* CJK Ext A */
    { 0x4E00, 0x9FFF, SUAS_SBR_CLS_ID }, /* CJK Unified */
    { 0xAC00, 0xAC00, SUAS_SBR_CLS_H2 }, /* Hangul LV */
    { 0xAC01, 0xD7A3, SUAS_SBR_CLS_H3 }, /* Hangul LVT */
    { 0xF900, 0xFAFF, SUAS_SBR_CLS_ID }, /* CJK Compat */
    { 0x1F1E6, 0x1F1FF, SUAS_SBR_CLS_RI }, /* regional indicators */
};

/* Binary search over the sorted range table. */
static int sbr_bridge_lookup(uint32_t cp, suas_sbr_break_class_t* out)
{
    size_t lo = 0, hi = SBR_NELEM(SBR_BRIDGE_TABLE);
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const sbr_range_t* r = &SBR_BRIDGE_TABLE[mid];
        if (cp < (uint32_t)r->lo) hi = mid;
        else if (cp > (uint32_t)r->hi) lo = mid + 1;
        else { *out = r->cls; return 1; }
    }
    return 0;
}

static int sbr_find_override(const suas_sbr_options_t* o, sucs_ex_char_t cp,
                             suas_sbr_break_class_t* out)
{
    if (o == NULL || o->overrides == NULL || o->count == 0) return 0;
    size_t lo = 0, hi = o->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const suas_sbr_override_t* r = &o->overrides[mid];
        if (cp < r->lo) hi = mid;
        else if (cp > r->hi) lo = mid + 1;
        else { *out = r->cls; return 1; }
    }
    return 0;
}

const char* suas_sbr_version_string(void)
{
    return "SUAS-003 SBR 0.1.0";
}

void suas_sbr_options_default(suas_sbr_options_t* o)
{
    if (o == NULL) return;
    o->overrides = NULL;
    o->count = 0;
}

void suas_sbr_state_init(suas_sbr_state_t* st)
{
    if (st == NULL) return;
    st->prev = SUAS_SBR_CLS_XX;
    st->seeded = 0;
}

/* ── Native SUCS / plugin "word" test — O(1) high-bit range bitmask ──
 * Native and plugin codepoints default to a single "word/neutral" decision:
 * the top half of Base SUCS and the entire plugin district are neutral gaps;
 * the lower Native region is treated as an ideographic word (participates in
 * the alphanumeric mandatory-adjacency rule). The test is a small number of
 * masked range comparisons — constant time, no table. */

bool suas_sbr_is_native_word(sucs_ex_char_t cp)
{
    /* Word-eligible native region: 0x00120000–0x00FFFFFF (sub-24-bit). */
    if (cp >= SBR_ZONE_NATIVE_MIN && cp <= 0x00FFFFFFULL) {
        /* Mask the high byte; treat the block as a word block. */
        sucs_ex_char_t block = (cp & 0xFF000000ULL) >> 24;
        return (block >= 0x00 && block <= 0x0F); /* low native blocks */
    }
    return 0; /* plugin + high native: neutral gap */
}

/* ── Dual-Mode classification (zone dispatch) ─────────────────────── */

suas_sbr_break_class_t suas_sbr_classify(sucs_ex_char_t cp,
                                         const suas_sbr_options_t* o)
{
    suas_sbr_break_class_t cls;

    /* Tailoring override takes absolute precedence. */
    if (sbr_find_override(o, cp, &cls)) return cls;

    if (cp <= SBR_ZONE_UNICODE_MAX) {
        if (sbr_bridge_lookup((uint32_t)cp, &cls)) return cls;
        return SUAS_SBR_CLS_XX; /* Bridge default: unknown */
    }

    /* SCP break markers and other SCP codepoints (control plane). */
    if (cp == SCP_BRK_MANDATORY)     return SUAS_SBR_CLS_BK;
    if (cp == SCP_BRK_PROHIBITED)    return SUAS_SBR_CLS_GL;
    if (cp == SCP_BRK_OPPORTUNISTIC) return SUAS_SBR_CLS_BA;
    if (cp >= SBR_ZONE_SCP_MIN && cp <= SBR_ZONE_SCP_MAX)
        return SUAS_SBR_CLS_BK; /* generic SCP control: mandatory */

    if (cp <= SBR_BASE_SUCS_MAX) {
        /* Native SUCS: word vs neutral via the bitmask. */
        return suas_sbr_is_native_word(cp) ? SUAS_SBR_CLS_ID
                                           : SUAS_SBR_CLS_XX;
    }
    /* ExtSUCS plugin space: neutral gap default. */
    return SUAS_SBR_CLS_XX;
}

/* ── Transition matrix ──────────────────────────────────────────────
 * Matrix[prev][cur] → break status, computed once into a static 2D array
 * (zero allocation). The matrix encodes the UAX #14 pair rules (LB4..LB21)
 * and the context classes as a deterministic, O(1) lookup: MUST_BREAK for
 * mandatory controls; NO_BREAK for WJ glue, inside-word, and GL; CAN_BREAK
 * by default; ALPHANUM_BREAK for the CJK/numeric separator rule. */

static suas_sbr_status_t SBR_MATRIX[SUAS_SBR_CLS_COUNT][SUAS_SBR_CLS_COUNT];
static int SBR_MATRIX_SEEDED = 0;

static int sbr_cls_alpha(suas_sbr_break_class_t c)
{
    switch (c) {
    case SUAS_SBR_CLS_AL:
    case SUAS_SBR_CLS_HL:
    case SUAS_SBR_CLS_ID:
    case SUAS_SBR_CLS_EB:
    case SUAS_SBR_CLS_EM:
    case SUAS_SBR_CLS_H2:
    case SUAS_SBR_CLS_H3:
    case SUAS_SBR_CLS_JL:
    case SUAS_SBR_CLS_JV:
    case SUAS_SBR_CLS_JT:
        return 1;
    default:
        return 0;
    }
}

static int sbr_cls_word(suas_sbr_break_class_t c)
{
    return sbr_cls_alpha(c) || c == SUAS_SBR_CLS_NU;
}

static void sbr_matrix_init(void)
{
    int p, c;
    if (SBR_MATRIX_SEEDED) return;
    for (p = 0; p < SUAS_SBR_CLS_COUNT; ++p)
        for (c = 0; c < SUAS_SBR_CLS_COUNT; ++c)
            SBR_MATRIX[p][c] = SUAS_BRK_CAN_BREAK;

    for (p = 0; p < SUAS_SBR_CLS_COUNT; ++p)
        for (c = 0; c < SUAS_SBR_CLS_COUNT; ++c) {
            /* CR-LF forms a single line break (LB5): no break between. */
            if (p == SUAS_SBR_CLS_CR && c == SUAS_SBR_CLS_LF) {
                SBR_MATRIX[p][c] = SUAS_BRK_NO_BREAK;
                continue;
            }
            /* Mandatory controls produce a hard break (LB4/LB5/LB6):
             * after BK/CR/LF/NL, and before LF/NL. */
            if (p == SUAS_SBR_CLS_BK || p == SUAS_SBR_CLS_CR ||
                p == SUAS_SBR_CLS_LF || p == SUAS_SBR_CLS_NL ||
                c == SUAS_SBR_CLS_LF || c == SUAS_SBR_CLS_NL) {
                SBR_MATRIX[p][c] = SUAS_BRK_MUST_BREAK;
                continue;
            }
            /* WJ forbids a break on either side (LB16). */
            if (p == SUAS_SBR_CLS_WJ || c == SUAS_SBR_CLS_WJ) {
                SBR_MATRIX[p][c] = SUAS_BRK_NO_BREAK;
                continue;
            }
            /* ZW offers a break after it (LB8a) unless a SP precedes. */
            if (p == SUAS_SBR_CLS_ZW) {
                SBR_MATRIX[p][c] = SUAS_BRK_CAN_BREAK;
                continue;
            }
            /* A GL that follows a space keeps the earlier break. */
            if (p == SUAS_SBR_CLS_GL && c != SUAS_SBR_CLS_SP) {
                SBR_MATRIX[p][c] = SUAS_BRK_NO_BREAK;
                continue;
            }
            /* A space keeps its break opportunity: after SP, the boundary
             * moves past the whole run, so no break between SP runs. */
            if (p == SUAS_SBR_CLS_SP && c == SUAS_SBR_CLS_SP) {
                SBR_MATRIX[p][c] = SUAS_BRK_NO_BREAK;
                continue;
            }
            /* Inside a word (alpha-alpha, numeric-numeric, mixed word) the
             * break is prohibited (LB19/LB20/LB21 region). */
            if (sbr_cls_word(p) && sbr_cls_word(c)) {
                SBR_MATRIX[p][c] = SUAS_BRK_NO_BREAK;
                continue;
            }
            /* Open punctuation does not break before (LB a4/LB14). */
            if (p == SUAS_SBR_CLS_OP) {
                SBR_MATRIX[p][c] = SUAS_BRK_NO_BREAK;
                continue;
            }
            /* Close punctuation / exclamation do not break before. */
            if (c == SUAS_SBR_CLS_CL || c == SUAS_SBR_CLS_CP ||
                c == SUAS_SBR_CLS_EX) {
                SBR_MATRIX[p][c] = SUAS_BRK_NO_BREAK;
                continue;
            }
            /* Non-breaking glue before a word: glue attaches left. */
            if (c == SUAS_SBR_CLS_GL) {
                SBR_MATRIX[p][c] = SUAS_BRK_NO_BREAK;
                continue;
            }
            /* Quotation attaches to the following text (LB19). */
            if (p == SUAS_SBR_CLS_QU) {
                SBR_MATRIX[p][c] = SUAS_BRK_NO_BREAK;
                continue;
            }
            /* CJK numeric rule (LB25 analog): numeric *numeric is a hard
             * break when separated by a numeric-connector that is not a
             * plain numeric (a [+×] glue between two numerics). */
            if (sbr_cls_alpha(p) &&
                (c == SUAS_SBR_CLS_PR || c == SUAS_SBR_CLS_PO)) {
                SBR_MATRIX[p][c] = SUAS_BRK_ALPHANUM_BREAK;
                continue;
            }
        }
    SBR_MATRIX_SEEDED = 1;
}

static suas_sbr_status_t sbr_pair_status(suas_sbr_break_class_t prev,
                                         suas_sbr_break_class_t cur)
{
    sbr_matrix_init();
    if (prev < 0 || prev >= SUAS_SBR_CLS_COUNT) prev = SUAS_SBR_CLS_XX;
    if (cur < 0 || cur >= SUAS_SBR_CLS_COUNT) cur = SUAS_SBR_CLS_XX;
    return SBR_MATRIX[prev][cur];
}

/* Pair two *classes* (already classified): SP-run rule then the matrix. */
static suas_sbr_status_t sbr_pair_class(suas_sbr_break_class_t pc,
                                         suas_sbr_break_class_t cc)
{
    /* A SP run carries the break: the boundary appears just before the
     * first following non-SP, so the run interior never breaks. */
    if (pc == SUAS_SBR_CLS_SP && cc == SUAS_SBR_CLS_SP)
        return SUAS_BRK_NO_BREAK;
    return sbr_pair_status(pc, cc);
}

/* ── Explicit SCP Break Marker override ───────────────────────────── */

static suas_sbr_status_t sbr_marker_override(sucs_ex_char_t cur)
{
    switch (cur) {
    case SCP_BRK_MANDATORY:     return SUAS_BRK_MUST_BREAK;
    case SCP_BRK_PROHIBITED:    return SUAS_BRK_NO_BREAK;
    case SCP_BRK_OPPORTUNISTIC: return SUAS_BRK_CAN_BREAK;
    default:
        return SUAS_BRK_NO_BREAK; /* not a marker */
    }
}

suas_sbr_status_t suas_sbr_pair(sucs_ex_char_t prev, sucs_ex_char_t cur,
                                const suas_sbr_options_t* o)
{
    suas_sbr_break_class_t pc = suas_sbr_classify(prev, o);
    suas_sbr_break_class_t cc = suas_sbr_classify(cur, o);

    /* Explicit markers override unconditionally. */
    if (cur == SCP_BRK_MANDATORY || cur == SCP_BRK_PROHIBITED ||
        cur == SCP_BRK_OPPORTUNISTIC)
        return sbr_marker_override(cur);

    return sbr_pair_class(pc, cc);
}

suas_sbr_status_t suas_sbr_process_codepoint(suas_sbr_state_t* st,
                                             sucs_ex_char_t cp,
                                             const suas_sbr_options_t* o)
{
    suas_sbr_break_class_t cc;
    suas_sbr_status_t r;
    if (st == NULL) return SUAS_SBR_ERR_NULL_POINTER;

    cc = suas_sbr_classify(cp, o);

    if (!st->seeded) {
        st->prev = cc;
        st->seeded = 1;
        return SUAS_BRK_MUST_BREAK; /* start of line: hard boundary */
    }

    /* Explicit marker override at the current boundary. */
    if (cp == SCP_BRK_MANDATORY || cp == SCP_BRK_PROHIBITED ||
        cp == SCP_BRK_OPPORTUNISTIC)
        r = sbr_marker_override(cp);
    else
        r = sbr_pair_class(st->prev, cc);

    st->prev = cc;
    return r;
}
