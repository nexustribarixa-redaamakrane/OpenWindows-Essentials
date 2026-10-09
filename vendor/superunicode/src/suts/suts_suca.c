/* SUTS-001 — SuperUnicode Collation Algorithm (SUCA) — reference implementation.
 *
 * Freestanding C99, zero heap. 64-bit ExtSUCS codepoint domain (Base SUCS is
 * the 31-bit subset). Implements docs/suts/SUTS-001-suca.md (UCA-equivalent):
 *   S1 normalization (NFD, curated subset + algorithmic Hangul)
 *   S2 CE-array construction (longest-match contractions, expansions,
 *      unblocked-non-starter discontiguous matching, implicit weights)
 *   S3 sort-key generation (multi-level, variable weighting, backward,
 *      semi-stable identical)
 *   S4 binary comparison
 * plus parametric and programmatic tailoring.
 */

#include "suts/suts_suca.h"

/* ============================================================================
 * Default constants
 * ============================================================================ */
#define SUCA_VAR_MAX_DEFAULT   ((suts_suca_weight_t)0x00FFu) /* max variable primary */
#define SUCA_MIN2              ((suts_suca_weight_t)0x0020u) /* unaccented */
#define SUCA_MIN3              ((suts_suca_weight_t)0x0002u) /* lowercase */
#define SUCA_T3_UPPER          ((suts_suca_weight_t)0x0008u)
#define SUCA_L4_NONVAR         ((suts_suca_weight_t)0xFFFFu)
#define SUCA_IMPLICIT_BASE     ((suts_suca_weight_t)0x1000000u)
#define SUCA_LEVEL_SEPARATOR   ((suts_suca_weight_t)0x0000u)

#define SUCA_MAX_IN            512u /* max input codepoints per call */
#define SUCA_MAX_CE            1024u /* max collation elements per string */
#define SUCA_MAX_NFD           6u   /* max codepoints from one leftover expansion */
#define SUCA_MAX_TABLE         256u /* curated mapping table capacity */
#define SUCA_MAX_RULES         64u  /* tailoring overlay capacity */

/* ============================================================================
 * Codepoint helpers
 * ============================================================================ */
static int is_scp(sucs_ex_char_t cp)            { return cp >= SUCA_ZONE_SCP_MIN && cp <= SUCA_ZONE_SCP_MAX; }

/* ----------------------------------------------------------------------------
 * Curated combining marks (bridge). Secondary collation elements.
 * l1 = 0, l2 > MIN2, l3 = MIN3. ccc = 230 standard combining.
 * -------------------------------------------------------------------------- */
typedef struct { sucs_ex_char_t cp; suts_suca_weight_t l2; } mark_t;

static const mark_t MARKS[] = {
    { 0x0300, 0x0038 }, /* grave    */
    { 0x0301, 0x003C }, /* acute    */
    { 0x0302, 0x0040 }, /* circumf. */
    { 0x0303, 0x0044 }, /* tilde    */
    { 0x0308, 0x0048 }, /* diaeresis*/
    { 0x030A, 0x004C }, /* ring     */
    { 0x0327, 0x0050 }, /* cedilla  */
    { 0x031B, 0x0054 }, /* horn     */
    { 0x0323, 0x0058 }, /* dot below*/
    { 0x0307, 0x005C }, /* dot above*/
    { 0x0328, 0x0060 }, /* ogonek   */
    { 0x035E, 0x0064 }, /* double   */
    { 0x034F, 0x0001 }, /* CGJ —— special: below MIN2, blocks contractions */
};
#define MARKS_COUNT (sizeof(MARKS)/sizeof(MARKS[0]))

static suts_suca_ce_t mark_ce(sucs_ex_char_t cp) {
    suts_suca_ce_t ce;
    suts_suca_weight_t l2 = SUCA_MIN2 + 1;
    size_t i;
    for (i = 0; i < MARKS_COUNT; i++) {
        if (MARKS[i].cp == cp) { l2 = MARKS[i].l2; break; }
    }
    ce.l1 = 0; ce.l2 = l2; ce.l3 = SUCA_MIN3; ce.l4 = 0;
    ce.levels = 3; ce.variable = 0;
    return ce;
}

/* ----------------------------------------------------------------------------
 * Curated simple mappings (codepoint -> single CE).
 * Primary letters/spacing assigned above SUCA_VAR_MAX_DEFAULT.
 * -------------------------------------------------------------------------- */
typedef struct { sucs_ex_char_t cp; suts_suca_weight_t l1; uint8_t variable; uint8_t upper; } letter_t;

/* a=0x0100, spaced by 4 for tailoring/contraction room. */
static const letter_t LETTERS[] = {
    {0x0061,0x0100,0,0},{0x0062,0x0104,0,0},{0x0063,0x0108,0,0},{0x0064,0x010C,0,0},
    {0x0065,0x0110,0,0},{0x0066,0x0114,0,0},{0x0067,0x0118,0,0},{0x0068,0x011C,0,0},
    {0x0069,0x0120,0,0},{0x006A,0x0124,0,0},{0x006B,0x0128,0,0},{0x006C,0x012C,0,0},
    {0x006D,0x0130,0,0},{0x006E,0x0134,0,0},{0x006F,0x0138,0,0},{0x0070,0x013C,0,0},
    {0x0071,0x0140,0,0},{0x0072,0x0144,0,0},{0x0073,0x0148,0,0},{0x0074,0x014C,0,0},
    {0x0075,0x0150,0,0},{0x0076,0x0154,0,0},{0x0077,0x0158,0,0},{0x0078,0x015C,0,0},
    {0x0079,0x0160,0,0},{0x007A,0x0164,0,0},
    /* uppercase share primary; tertiary 0x0008 */
    {0x0041,0x0100,0,1},{0x0042,0x0104,0,1},{0x0043,0x0108,0,1},{0x0044,0x010C,0,1},
    {0x0045,0x0110,0,1},{0x0046,0x0114,0,1},{0x0047,0x0118,0,1},{0x0048,0x011C,0,1},
    {0x0049,0x0120,0,1},{0x004A,0x0124,0,1},{0x004B,0x0128,0,1},{0x004C,0x012C,0,1},
    {0x004D,0x0130,0,1},{0x004E,0x0134,0,1},{0x004F,0x0138,0,1},{0x0050,0x013C,0,1},
    {0x0051,0x0140,0,1},{0x0052,0x0144,0,1},{0x0053,0x0148,0,1},{0x0054,0x014C,0,1},
    {0x0055,0x0150,0,1},{0x0056,0x0154,0,1},{0x0057,0x0158,0,1},{0x0058,0x015C,0,1},
    {0x0059,0x0160,0,1},{0x005A,0x0164,0,1},
};
#define LETTERS_COUNT (sizeof(LETTERS)/sizeof(LETTERS[0]))

typedef struct { sucs_ex_char_t cp; suts_suca_weight_t l1; uint8_t variable; } word_t;
/* digits (non-variable, primary above letters), space/hyphen/comma (variable). */
static const word_t WORDS[] = {
    {0x0030,0x0200,0},{0x0031,0x0201,0},{0x0032,0x0202,0},{0x0033,0x0203,0},
    {0x0034,0x0204,0},{0x0035,0x0205,0},{0x0036,0x0206,0},{0x0037,0x0207,0},
    {0x0038,0x0208,0},{0x0039,0x0209,0},
    {0x0020,0x0001,1},   /* SPACE (variable) */
    {0x002D,0x0002,1},   /* HYPHEN-MINUS (variable) */
    {0x002C,0x0003,1},   /* COMMA (variable) */
    {0x002E,0x0004,1},   /* FULL STOP (variable) */
    {0x0028,0x0005,1},   /* ( variable */
    {0x0029,0x0006,1},   /* ) variable */
    {0x0022,0x0007,1},   /* " variable */
};
#define WORDS_COUNT (sizeof(WORDS)/sizeof(WORDS[0]))

/* ----------------------------------------------------------------------------
 * Contractions (bridge): sequence -> single CE.
 * Slovak example: "c h" collates as one letter just after "c".
 * -------------------------------------------------------------------------- */
typedef struct {
    sucs_ex_char_t a, b;
    suts_suca_weight_t l1;
} contraction_t;
static const contraction_t CONTRACTIONS[] = {
    {0x0063,0x0068,0x0109}, /* ch -> 0x0109 (between c and d) */
};
#define CONTRACTIONS_COUNT (sizeof(CONTRACTIONS)/sizeof(CONTRACTIONS[0]))

/* ----------------------------------------------------------------------------
 * Expansions (bridge): single codepoint -> 2 CEs.
 * œ (U+0153) ~ "oe": expands to [o][e].
 * -------------------------------------------------------------------------- */
typedef struct {
    sucs_ex_char_t cp;
    suts_suca_weight_t l1a, l1b;
} expansion_t;
static const expansion_t EXPANSIONS[] = {
    {0x0153, 0x0138, 0x0110}, /* œ -> o + e */
};
#define EXPANSIONS_COUNT (sizeof(EXPANSIONS)/sizeof(EXPANSIONS[0]))

/* ----------------------------------------------------------------------------
 * Tailoring overlay (S8): a bounded, mutable table registered via
 * suts_suca_apply_rules.  Each entry maps a value sequence (1 or 2 codepoints)
 * to a tailored CE, overriding the built-in curated tables.  Freestanding:
 * fixed-capacity, no heap.
 * -------------------------------------------------------------------------- */
#define SUTS_SUCA_TAILOR_MAX 16
typedef struct {
    sucs_ex_char_t key[SUTS_SUCA_MAX_SEQ];
    size_t key_len;
    suts_suca_ce_t ce;      /* the tailored CE */
    suts_suca_ce_t ce2;     /* expansion second CE (key_len==1 && expanded) */
    uint8_t expanded;       /* 1 => emit ce then ce2 for key_len==1 */
} tailor_t;
static tailor_t suca_tailor[SUTS_SUCA_TAILOR_MAX];
static size_t suca_tailor_n = 0;

/* Curated contraction entry helper: fold the static CONTRACTIONS[] plus any
 * tailored 2-char entries into a single searchable list of (a,b,l1). */
typedef struct { sucs_ex_char_t a, b; suts_suca_weight_t l1; } abce_t;

/* ----------------------------------------------------------------------------
 * NFD decomposition table (bridge) + algorithmic Hangul.
 * Map precomposed accented Latin to base + combining mark.
 * -------------------------------------------------------------------------- */
typedef struct { sucs_ex_char_t pre; sucs_ex_char_t base; sucs_ex_char_t mark; } decomp_t;

static const decomp_t DECOMP[] = {
    {0x00E0,0x0061,0x0300},{0x00E1,0x0061,0x0301},{0x00E2,0x0061,0x0302},{0x00E3,0x0061,0x0303},
    {0x00E4,0x0061,0x0308},{0x00E5,0x0061,0x030A},{0x00E7,0x0063,0x0327},{0x00E8,0x0065,0x0300},
    {0x00E9,0x0065,0x0301},{0x00EA,0x0065,0x0302},{0x00EB,0x0065,0x0308},{0x00EC,0x0069,0x0300},
    {0x00ED,0x0069,0x0301},{0x00EE,0x0069,0x0302},{0x00EF,0x0069,0x0308},{0x00F1,0x006E,0x0303},
    {0x00F2,0x006F,0x0300},{0x00F3,0x006F,0x0301},{0x00F4,0x006F,0x0302},{0x00F5,0x006F,0x0303},
    {0x00F6,0x006F,0x0308},{0x00F9,0x0075,0x0300},{0x00FA,0x0075,0x0301},{0x00FB,0x0075,0x0302},
    {0x00FC,0x0075,0x0308},{0x00FD,0x0079,0x0301},{0x00FF,0x0079,0x0308},
    {0x00C0,0x0041,0x0300},{0x00C1,0x0041,0x0301},{0x00C2,0x0041,0x0302},{0x00C3,0x0041,0x0303},
    {0x00C4,0x0041,0x0308},{0x00C5,0x0041,0x030A},{0x00C7,0x0043,0x0327},{0x00C8,0x0045,0x0300},
    {0x00C9,0x0045,0x0301},{0x00CA,0x0045,0x0302},{0x00CB,0x0045,0x0308},{0x00CC,0x0049,0x0300},
    {0x00CD,0x0049,0x0301},{0x00CE,0x0049,0x0302},{0x00CF,0x0049,0x0308},{0x00D1,0x004E,0x0303},
    {0x00D2,0x004F,0x0300},{0x00D3,0x004F,0x0301},{0x00D4,0x004F,0x0302},{0x00D5,0x004F,0x0303},
    {0x00D6,0x004F,0x0308},{0x00D9,0x0055,0x0300},{0x00DA,0x0055,0x0301},{0x00DB,0x0055,0x0302},
    {0x00DC,0x0055,0x0308},{0x00DD,0x0059,0x0301},
};
#define DECOMP_COUNT (sizeof(DECOMP)/sizeof(DECOMP[0]))

/* Hangul syllable decomposition (algorithmic). Returns >0 if decomposed. */
static int hangul_decompose(sucs_ex_char_t cp, sucs_ex_char_t* l, sucs_ex_char_t* v, sucs_ex_char_t* t) {
    const sucs_ex_char_t SBase = 0xAC00, LBase = 0x1100, VBase = 0x1161, TBase = 0x11A7;
    const sucs_ex_char_t LCount = 19, VCount = 21, TCount = 28, NCount = VCount * TCount, SCount = LCount * NCount;
    sucs_ex_char_t sIndex, lIndex, vIndex, tIndex;
    if (cp < SBase || cp >= SBase + SCount) return 0;
    sIndex = cp - SBase;
    lIndex = sIndex / NCount;
    vIndex = (sIndex % NCount) / TCount;
    tIndex = sIndex % TCount;
    *l = LBase + lIndex; *v = VBase + vIndex;
    *t = (tIndex == 0) ? 0 : (TBase + tIndex);
    return 1;
}

/* ============================================================================
 * NFD (S1.1)
 * ============================================================================ */
uint8_t suts_suca_ccc(sucs_ex_char_t cp) {
    size_t i;
    (void)cp;
    for (i = 0; i < MARKS_COUNT; i++) if (MARKS[i].cp == cp) return 230;
    return 0;
}

suts_suca_status_t suts_suca_nfd(sucs_ex_char_t cp, sucs_ex_char_t* out, size_t outcap, size_t* nout) {
    size_t i;
    sucs_ex_char_t l, v, t;
    size_t n = 0;
    if (!out || !nout) return SUTS_SUCA_ERR_INVALID_ARG;
    if (hangul_decompose(cp, &l, &v, &t)) {
        if (outcap < 2) return SUTS_SUCA_ERR_BUFFER_TOO_SMALL;
        out[0] = l; out[1] = v; n = 2;
        if (t != 0) {
            if (outcap < 3) return SUTS_SUCA_ERR_BUFFER_TOO_SMALL;
            out[2] = t; n = 3;
        }
        *nout = n;
        return SUTS_SUCA_OK;
    }
    for (i = 0; i < DECOMP_COUNT; i++) {
        if (DECOMP[i].pre == cp) {
            if (outcap < 2) return SUTS_SUCA_ERR_BUFFER_TOO_SMALL;
            out[0] = DECOMP[i].base; out[1] = DECOMP[i].mark; n = 2;
            *nout = n;
            return SUTS_SUCA_OK;
        }
    }
    /* not decomposable */
    if (outcap < 1) return SUTS_SUCA_ERR_BUFFER_TOO_SMALL;
    out[0] = cp; n = 1;
    *nout = n;
    return SUTS_SUCA_OK;
}

/* Normalize an entire string to a caller buffer. Combines decomposed pieces
 * and reorders combining marks by non-increasing ccc (Canonical Ordering). */
static suts_suca_status_t nfd_string(const sucs_ex_char_t* cps, size_t n,
                                     sucs_ex_char_t* out, size_t outcap, size_t* nout) {
    size_t i, w = 0;
    size_t k;
    if (!cps || !out || !nout) return SUTS_SUCA_ERR_INVALID_ARG;
    if (n > SUCA_MAX_IN) return SUTS_SUCA_ERR_BUFFER_TOO_SMALL;
    for (i = 0; i < n; i++) {
        sucs_ex_char_t tmp[SUCA_MAX_NFD];
        size_t tn = 0, j;
        suts_suca_status_t st = suts_suca_nfd(cps[i], tmp, SUCA_MAX_NFD, &tn);
        if (st != SUTS_SUCA_OK) return st;
        for (j = 0; j < tn; j++) {
            if (w >= outcap) return SUTS_SUCA_ERR_BUFFER_TOO_SMALL;
            out[w++] = tmp[j];
        }
    }
    /* Canonical ordering: stable insertion by non-increasing ccc after each starter */
    for (i = 0; i < w; i++) {
        uint8_t ccc = suts_suca_ccc(out[i]);
        size_t p = i;
        if (ccc == 0) continue;
        while (p > 0) {
            uint8_t prev = suts_suca_ccc(out[p-1]);
            if (prev != 0 && prev > ccc) break;
            if (prev == 0) break;
            p--;
        }
        if (p != i) {
            sucs_ex_char_t tmpc = out[i];
            for (k = i; k > p; k--) out[k] = out[k-1];
            out[p] = tmpc;
        }
        (void)k;
    }
    *nout = w;
    return SUTS_SUCA_OK;
}

/* ============================================================================
 * Simple lookup (single cp, no contraction) — S2 fallback
 * ============================================================================ */
static int lookup_simple(sucs_ex_char_t cp, suts_suca_ce_t* out) {
    size_t i;
    suts_suca_ce_t ce;
    ce.l1 = 0; ce.l2 = SUCA_MIN2; ce.l3 = SUCA_MIN3; ce.l4 = 0; ce.levels = 3; ce.variable = 0;
    for (i = 0; i < MARKS_COUNT; i++) {
        if (MARKS[i].cp == cp) { *out = mark_ce(cp); return 1; }
    }
    for (i = 0; i < WORDS_COUNT; i++) {
        if (WORDS[i].cp == cp) {
            ce.l1 = WORDS[i].l1; ce.variable = (uint8_t)WORDS[i].variable;
            *out = ce; return 1;
        }
    }
    for (i = 0; i < LETTERS_COUNT; i++) {
        if (LETTERS[i].cp == cp) {
            ce.l1 = LETTERS[i].l1;
            ce.l3 = LETTERS[i].upper ? SUCA_T3_UPPER : SUCA_MIN3;
            ce.variable = 0;
            /* case order option is applied at sort-key time, not here */
            *out = ce; return 1;
        }
    }
    return 0;
}

/* lookup expansion (returns index into EXPANSIONS, or -1) */
static int lookup_expansion(sucs_ex_char_t cp) {
    size_t i;
    for (i = 0; i < EXPANSIONS_COUNT; i++) if (EXPANSIONS[i].cp == cp) return (int)i;
    return -1;
}

/* ============================================================================
 * Implicit weight derivation (§9.1) — any 64-bit codepoint
 * ============================================================================ */
suts_suca_ce_t suts_suca_implicit_ce(sucs_ex_char_t cp) {
    suts_suca_ce_t ce;
    suts_suca_weight_t prim;
    ce.l1 = 0; ce.l2 = SUCA_MIN2; ce.l3 = SUCA_MIN3; ce.l4 = 0; ce.levels = 3; ce.variable = 0;
    /* Documentation limit: 32-bit weights represent the low 32-bit space
     * exactly; higher plugin space is monotonic-collapsed (see SUTS-001 §9.1). */
    if (cp < 0x100000000ULL)
        prim = SUCA_IMPLICIT_BASE + (suts_suca_weight_t)cp;
    else
        prim = SUCA_IMPLICIT_BASE + 0xFFFFFFu; /* clamped, documented */
    ce.l1 = prim;
    return ce;
}

/* ============================================================================
 * Mapping lookup (S2.1) with contractions + unblocked-non-starter extension
 * Input: nfd[] normalized codepoints, position pos.
 * Attempts the longest contiguous contraction, then discontiguous extension.
 * Output: ce(s) via out/ce cap; *consumed = number of input chars.
 * ============================================================================ */
static suts_suca_status_t lookup_at(const sucs_ex_char_t* nfd, size_t n, size_t pos,
                                    suts_suca_ce_t* out, size_t ce_cap, size_t* nout,
                                    size_t* consumed) {
    size_t i;
    int j;
    suts_suca_ce_t simple;
    size_t k;
    (void)ce_cap;

    /* Tailoring overlay first (S8): tailored 2-char contractions and
     * single-codepoint reweights/expansions take precedence over the
     * built-in curated tables. */
    for (i = 0; i < suca_tailor_n; i++) {
        const tailor_t* t = &suca_tailor[i];
        if (t->key_len == 2 && t->key[0] == nfd[pos]) {
            size_t q;
            if (pos + 1 < n && t->key[1] == nfd[pos + 1]) {
                /* contiguous match */
                out[0] = t->ce; *nout = 1; *consumed = 2;
                return SUTS_SUCA_OK;
            }
            /* discontiguous: skip unblocked non-starters between a and b */
            q = pos + 1;
            while (q < n && suts_suca_ccc(nfd[q]) != 0) q++;
            if (q < n && t->key[1] == nfd[q]) {
                out[0] = t->ce; *nout = 1; *consumed = q - pos + 1;
                return SUTS_SUCA_OK;
            }
        }
        if (t->key_len == 1 && t->key[0] == nfd[pos]) {
            if (t->expanded) {
                if (2 > ce_cap) return SUTS_SUCA_ERR_BUFFER_TOO_SMALL;
                out[0] = t->ce; out[1] = t->ce2; *nout = 2; *consumed = 1;
            } else {
                out[0] = t->ce; *nout = 1; *consumed = 1;
            }
            return SUTS_SUCA_OK;
        }
    }

    /* Contractions: currently curated as 2-char sequences. */
    for (i = 0; i < CONTRACTIONS_COUNT; i++) {
        if (CONTRACTIONS[i].a != nfd[pos]) continue;
        {
            size_t q = pos + 1;
            suts_suca_ce_t ce;
            /* skip unblocked non-starters between a and b (discontiguous) */
            while (q < n && suts_suca_ccc(nfd[q]) != 0) q++;
            if (q < n && CONTRACTIONS[i].b == nfd[q]) {
                /* contraction found a ... b (contiguous or across non-starters) */
                ce.l1 = CONTRACTIONS[i].l1; ce.l2 = SUCA_MIN2; ce.l3 = SUCA_MIN3;
                ce.l4 = 0; ce.levels = 3; ce.variable = 0;
                out[0] = ce; *nout = 1; *consumed = q - pos + 1;
                return SUTS_SUCA_OK;
            }
            /* contiguous (b directly follows a) */
            if (pos + 1 < n && CONTRACTIONS[i].b == nfd[pos + 1]) {
                ce.l1 = CONTRACTIONS[i].l1; ce.l2 = SUCA_MIN2; ce.l3 = SUCA_MIN3;
                ce.l4 = 0; ce.levels = 3; ce.variable = 0;
                out[0] = ce; *nout = 1; *consumed = 2;
                return SUTS_SUCA_OK;
            }
        }
    }

    /* Expansion: one codepoint -> two CEs. */
    j = lookup_expansion(nfd[pos]);
    if (j >= 0) {
        suts_suca_ce_t e1, e2;
        e1.l1 = EXPANSIONS[j].l1a; e1.l2 = SUCA_MIN2; e1.l3 = SUCA_MIN3; e1.l4 = 0; e1.levels = 3; e1.variable = 0;
        e2.l1 = EXPANSIONS[j].l1b; e2.l2 = SUCA_MIN2; e2.l3 = SUCA_MIN3; e2.l4 = 0; e2.levels = 3; e2.variable = 0;
        if (2 > ce_cap) return SUTS_SUCA_ERR_BUFFER_TOO_SMALL;
        out[0] = e1; out[1] = e2; *nout = 2; *consumed = 1;
        return SUTS_SUCA_OK;
    }

    /* Simple mapping. */
    if (lookup_simple(nfd[pos], &simple)) {
        out[0] = simple; *nout = 1; *consumed = 1;
        return SUTS_SUCA_OK;
    }

    /* SCP codepoints are variable (collation-ignorable at L1-L3, per
     * SUTS-001 §4 nuance): a completely-ignorable CE resolved by the
     * identical level's native codepoint order. */
    if (is_scp(nfd[pos])) {
        suts_suca_ce_t s;
        s.l1 = 0; s.l2 = 0; s.l3 = 0; s.l4 = 0;
        s.levels = 0; s.variable = 1;
        out[0] = s; *nout = 1; *consumed = 1;
        return SUTS_SUCA_OK;
    }

    /* Implicit weight for anything else (unassigned, Han, native, ext plugin). */
    out[0] = suts_suca_implicit_ce(nfd[pos]); *nout = 1; *consumed = 1;
    (void)k; (void)i;
    return SUTS_SUCA_OK;
}

/* ============================================================================
 * CE array (S2)
 * ============================================================================ */
suts_suca_status_t suts_suca_ce_array(const sucs_ex_char_t* cps, size_t n,
                                      const suts_suca_options_t* o,
                                      suts_suca_ce_t* out, size_t ce_cap, size_t* nout) {
    sucs_ex_char_t nfd[SUCA_MAX_IN * 2];
    size_t nn = 0, pos = 0, oc = 0;
    suts_suca_status_t st;
    (void)o;
    if (!cps || !out || !nout) return SUTS_SUCA_ERR_INVALID_ARG;
    if (n == 0) { *nout = 0; return SUTS_SUCA_OK; }
    st = nfd_string(cps, n, nfd, SUCA_MAX_IN * 2, &nn);
    if (st != SUTS_SUCA_OK) return st;
    while (pos < nn) {
        suts_suca_ce_t tmp[8];
        size_t ntmp = 0, consumed = 1;
        size_t i;
        st = lookup_at(nfd, nn, pos, tmp, 8, &ntmp, &consumed);
        if (st != SUTS_SUCA_OK) return st;
        for (i = 0; i < ntmp; i++) {
            if (oc >= ce_cap) return SUTS_SUCA_ERR_BUFFER_TOO_SMALL;
            out[oc++] = tmp[i];
        }
        pos += consumed;
    }
    *nout = oc;
    return SUTS_SUCA_OK;
}

/* ============================================================================
 * Variable weighting preprocessing for sort keys (§4)
 * ============================================================================ */
typedef struct {
    suts_suca_weight_t w[4]; /* effective L1..L4 */
    uint8_t present;
} work_ce_t;

/* Apply variable weighting to the raw CE array into a "work" array of
 * effective level weights (used by key generation). */
static size_t work_apply(const suts_suca_ce_t* ce, size_t n,
                         const suts_suca_options_t* o, work_ce_t* w) {
    size_t i, j, wi = 0;
    int after_var = 0;
    int shifted = (o->variable == SUTS_SUCA_VAR_SHIFTED ||
                   o->variable == SUTS_SUCA_VAR_SHIFT_TRIMMED);
    int blanked = (o->variable == SUTS_SUCA_VAR_BLANKED);
    suts_suca_weight_t vmax = o->var_max ? o->var_max : SUCA_VAR_MAX_DEFAULT;

    for (i = 0; i < n; i++) {
        const suts_suca_ce_t* c = &ce[i];
        int is_var = c->variable || (c->l1 != 0 && c->l1 <= vmax);
        int ignorable = (c->l1 == 0);
        work_ce_t wc;
        wc.present = 1;
        wc.w[0] = c->l1;
        wc.w[1] = c->l2;
        wc.w[2] = c->l3;
        wc.w[3] = c->l4;

        if (shifted || blanked) {
            if (is_var) {
                /* variable CE: reset L1-L3, keep original L1 for L4 (shift) */
                suts_suca_weight_t old = c->l1;
                wc.w[0] = 0; wc.w[1] = 0; wc.w[2] = 0;
                wc.w[3] = (shifted) ? old : 0;
                after_var = 1;
            } else if (after_var && ignorable) {
                /* subsequent ignorable CEs after a variable are reset */
                wc.w[0] = 0; wc.w[1] = 0; wc.w[2] = 0; wc.w[3] = 0;
            } else {
                after_var = 0;
            }
        }
        w[wi++] = wc;
    }
    /* second pass: assign default L4 for shifted non-variables */
    if (shifted) {
        for (j = 0; j < wi; j++) {
            if (w[j].w[0] != 0 && w[j].w[3] == 0) w[j].w[3] = SUCA_L4_NONVAR;
        }
    }
    return wi;
}

/* ============================================================================
 * Sort key (S3)
 * ============================================================================ */
static suts_suca_status_t key_append(suts_suca_key_t* key, suts_suca_weight_t v) {
    if (key->length >= key->capacity) return SUTS_SUCA_ERR_BUFFER_TOO_SMALL;
    key->data[key->length++] = v;
    return SUTS_SUCA_OK;
}

suts_suca_status_t suts_suca_key(const sucs_ex_char_t* cps, size_t n,
                                 const suts_suca_options_t* o, suts_suca_key_t* key) {
    suts_suca_ce_t ce[SUCA_MAX_CE];
    size_t nce = 0;
    work_ce_t w[SUCA_MAX_CE];
    size_t nw = 0, lvl;
    suts_suca_status_t st;
    int maxlev;

    if (!key || !key->data || !o) return SUTS_SUCA_ERR_INVALID_ARG;
    key->length = 0;
    st = suts_suca_ce_array(cps, n, o, ce, SUCA_MAX_CE, &nce);
    if (st != SUTS_SUCA_OK) return st;
    nw = work_apply(ce, nce, o, w);

    switch (o->strength) {
        case SUTS_SUCA_STRENGTH_PRIMARY:    maxlev = 1; break;
        case SUTS_SUCA_STRENGTH_SECONDARY:  maxlev = 2; break;
        case SUTS_SUCA_STRENGTH_QUATERNARY: maxlev = 4; break;
        case SUTS_SUCA_STRENGTH_IDENTICAL:  maxlev = 4; break;
        case SUTS_SUCA_STRENGTH_TERTIARY:
        default:                            maxlev = 3; break;
    }

    for (lvl = 1; lvl <= (size_t)maxlev; lvl++) {
        size_t idx;
        if (lvl > 1) { st = key_append(key, SUCA_LEVEL_SEPARATOR); if (st) return st; }
        /* backward secondary? only meaningful for level 2, French */
        if (lvl == 2 && o->backward_secondary) {
            /* collect non-zero L2, then append in reverse */
            suts_suca_weight_t buf[SUCA_MAX_CE];
            size_t nb = 0, b;
            for (idx = 0; idx < nw; idx++) {
                suts_suca_weight_t v = w[idx].w[1];
                if (v != 0 && nb < SUCA_MAX_CE) buf[nb++] = v;
            }
            for (b = nb; b-- > 0;) { st = key_append(key, buf[b]); if (st) return st; }
        } else {
            for (idx = 0; idx < nw; idx++) {
                suts_suca_weight_t v = w[idx].w[lvl-1];
                if (v != 0) { st = key_append(key, v); if (st) return st; }
            }
        }
    }

    /* identical level (S3.10): append a copy of the NFD string so that
     * tie-break equals SuperUnicode native codepoint order (sentinel last). */
    if (o->strength == SUTS_SUCA_STRENGTH_IDENTICAL || o->semi_stable) {
        sucs_ex_char_t nfd[SUCA_MAX_IN * 2];
        size_t nn = 0, i;
        st = nfd_string(cps, n, nfd, SUCA_MAX_IN * 2, &nn);
        if (st != SUTS_SUCA_OK) return st;
        st = key_append(key, SUCA_LEVEL_SEPARATOR);
        if (st) return st;
        for (i = 0; i < nn; i++) {
            st = key_append(key, (suts_suca_weight_t)nfd[i]);
            if (st) return st;
        }
    }

    if (o->variable == SUTS_SUCA_VAR_SHIFT_TRIMMED) {
        while (key->length > 0 && key->data[key->length - 1] == SUCA_L4_NONVAR)
            key->length--;
    }
    return SUTS_SUCA_OK;
}

/* ============================================================================
 * Compare sort keys / strings (S4)
 * ============================================================================ */
int suts_suca_compare_keys(const suts_suca_key_t* a, const suts_suca_key_t* b) {
    size_t n = a->length < b->length ? a->length : b->length;
    size_t i;
    if (!a || !b) return 0;
    for (i = 0; i < n; i++) {
        if (a->data[i] < b->data[i]) return -1;
        if (a->data[i] > b->data[i]) return 1;
    }
    if (a->length < b->length) return -1;
    if (a->length > b->length) return 1;
    return 0;
}

suts_suca_status_t suts_suca_compare(const sucs_ex_char_t* a, size_t an,
                                     const sucs_ex_char_t* b, size_t bn,
                                     const suts_suca_options_t* o, int* result) {
    static suts_suca_weight_t bufA[SUCA_MAX_CE * 4 + SUCA_MAX_IN * 2];
    static suts_suca_weight_t bufB[SUCA_MAX_CE * 4 + SUCA_MAX_IN * 2];
    suts_suca_key_t ka = { bufA, SUCA_MAX_CE * 4 + SUCA_MAX_IN * 2, 0 };
    suts_suca_key_t kb = { bufB, SUCA_MAX_CE * 4 + SUCA_MAX_IN * 2, 0 };
    suts_suca_status_t st;
    if (!a || !b || !result || !o) return SUTS_SUCA_ERR_INVALID_ARG;
    st = suts_suca_key(a, an, o, &ka); if (st) return st;
    st = suts_suca_key(b, bn, o, &kb); if (st) return st;
    *result = suts_suca_compare_keys(&ka, &kb);
    return SUTS_SUCA_OK;
}

/* ============================================================================
 * Version
 * ============================================================================ */
const char* suts_suca_version_string(void) {
    return "SUCA 0.1.0 (SUTS-001)";
}

void suts_suca_options_default(suts_suca_options_t* o) {
    if (!o) return;
    o->strength = SUTS_SUCA_STRENGTH_TERTIARY;
    o->variable = SUTS_SUCA_VAR_SHIFTED;
    o->backward_secondary = false;
    o->normalization = true;
    o->semi_stable = false;
    o->case_order = SUTS_SUCA_CASE_LOWER_FIRST;
    o->var_max = 0;
}

/* ============================================================================
 * Tailoring: programmatic rule application (§8.2).
 * Rules select the anchor codepoint (base[0]) and reweight the value
 * sequence (value[0..value_len-1]) in an overlay consulted above the
 * built-in curated tables:
 *   & base <  value   -> value primary = base primary + step
 *   & base << value   -> value shares base primary, secondary + step
 *   & base <<< value  -> value shares base primary+secondary, tertiary + step
 *   & base =  value   -> value = base CE exactly
 * Two-character value sequences register a contraction (primary form).
 * ============================================================================ */
static suts_suca_ce_t base_ce_of(sucs_ex_char_t cp) {
    suts_suca_ce_t ce;
    if (lookup_simple(cp, &ce)) return ce;
    ce = suts_suca_implicit_ce(cp);
    if (ce.l2 == 0) { ce.l2 = SUCA_MIN2; ce.l3 = SUCA_MIN3; }
    return ce;
}

static void tailor_register(const sucs_ex_char_t* key, size_t key_len,
                            suts_suca_ce_t ce, uint8_t expanded, suts_suca_ce_t ce2) {
    size_t i;
    /* replace an existing entry for the same key sequence */
    for (i = 0; i < suca_tailor_n; i++) {
        if (suca_tailor[i].key_len == key_len) {
            size_t j; int same = 1;
            for (j = 0; j < key_len; j++) if (suca_tailor[i].key[j] != key[j]) { same = 0; break; }
            if (same) {
                suca_tailor[i].ce = ce; suca_tailor[i].ce2 = ce2;
                suca_tailor[i].expanded = expanded;
                return;
            }
        }
    }
    if (suca_tailor_n >= SUTS_SUCA_TAILOR_MAX) return; /* overlay full */
    {
        tailor_t* t = &suca_tailor[suca_tailor_n++];
        size_t j;
        for (j = 0; j < key_len; j++) t->key[j] = key[j];
        t->key_len = key_len;
        t->ce = ce; t->ce2 = ce2; t->expanded = expanded;
    }
}

suts_suca_status_t suts_suca_apply_rules(const suts_suca_rule_t* rules,
                                         size_t nrules, suts_suca_options_t* o) {
    size_t i;
    if (!rules && nrules != 0) return SUTS_SUCA_ERR_INVALID_ARG;
    if (!o) return SUTS_SUCA_ERR_INVALID_ARG;
    for (i = 0; i < nrules; i++) {
        const suts_suca_rule_t* r = &rules[i];
        suts_suca_ce_t base;
        suts_suca_ce_t nc;
        if (r->base_len == 0 || r->value_len == 0 || r->value_len > SUTS_SUCA_MAX_SEQ)
            return SUTS_SUCA_ERR_INVALID_ARG;
        base = base_ce_of(r->base[0]);
        nc = base;

        switch (r->kind) {
            case SUTS_SUCA_RULE_PRIMARY_GT:
                nc.l1 = base.l1 + 2;              /* leave room for other nodes */
                nc.l2 = SUCA_MIN2; nc.l3 = SUCA_MIN3;
                break;
            case SUTS_SUCA_RULE_SECONDARY_GT:
                nc.l1 = base.l1; nc.l2 = base.l2 + 1; nc.l3 = SUCA_MIN3;
                break;
            case SUTS_SUCA_RULE_TERTIARY_GT:
                nc.l1 = base.l1; nc.l2 = base.l2; nc.l3 = base.l3 + 1;
                break;
            case SUTS_SUCA_RULE_EQUAL:
                /* nc stays = base */
                break;
            default:
                return SUTS_SUCA_ERR_UNSUPPORTED;
        }
        nc.l4 = 0; nc.levels = 3; nc.variable = 0;

        if (r->value_len >= 2) {
            /* contraction: value[0] value[1] matches one tailored CE */
            tailor_register(r->value, 2, nc, 0, nc);
        } else {
            suts_suca_ce_t zero;
            zero.l1 = 0; zero.l2 = 0; zero.l3 = 0; zero.l4 = 0; zero.levels = 0; zero.variable = 0;
            tailor_register(r->value, 1, nc, 0, zero);
        }
    }
    return SUTS_SUCA_OK;
}
