/* SUAS-004 — SuperUnicode Canonical Forms (SUCF)
 *
 * Reference implementation of docs/suas/SUAS-004-sucf.md.
 *
 * Freestanding C99, zero allocation. Input codepoints are 64-bit ExtSUCS
 * (sucs_ex_char_t). A single-pass canonical (de)composition engine produces
 * the two targets — SUCF-C (canonical composition) and SUCF-D (canonical
 * decomposition) — inside a small stack-allocated sliding reordering window,
 * with combining marks sorted ascending by CCC (swap iff CCC_A > CCC_B and
 * CCC_B != 0). Unicode Bridge semantics follow UAX #15 via SUCD binary
 * property tables; SCP / BANcode / trap markers are canonically invariant.
 */

#include "suas/suas_sucf.h"

#define SUCF_NELEM(a) (sizeof((a)) / sizeof((a)[0]))

/* ── Curated Combining Canonical Class (CCC) table ───────────────────
 * Sorted ascending by lo; non-overlapping. Codepoints not listed default to
 * CCC 0 (starters). Values follow the Unicode Canonical_Combining_Class
 * property for the bridge; SUCD is the source of truth. */
typedef struct {
    sucs_ex_char_t lo;
    sucs_ex_char_t hi;
    int            ccc;
} sucf_ccc_range_t;

static const sucf_ccc_range_t SUCF_CCC_TABLE[] = {
    { 0x0300, 0x0314, 230 }, /* grave..MACRON grave combiner block */
    { 0x0315, 0x0315, 232 }, /* COMBINING COMMA ABOVE RIGHT */
    { 0x0316, 0x0319, 220 },
    { 0x031A, 0x031A, 232 },
    { 0x031B, 0x031B, 216 },
    { 0x031C, 0x0320, 220 },
    { 0x0321, 0x0322, 202 },
    { 0x0323, 0x0326, 220 },
    { 0x0327, 0x0328, 202 },
    { 0x0329, 0x0333, 220 },
    { 0x0334, 0x0338, 1 },   /* overlay */
    { 0x0339, 0x033C, 220 },
    { 0x033D, 0x0344, 230 },
    { 0x0345, 0x0345, 240 }, /* ypogegrammeni */
    { 0x0346, 0x0346, 230 },
    { 0x0347, 0x0349, 220 },
    { 0x034A, 0x034C, 230 },
    { 0x034D, 0x034E, 220 },
    { 0x0350, 0x0352, 230 },
    { 0x0353, 0x0356, 220 },
    { 0x0357, 0x0357, 230 },
    { 0x0358, 0x0358, 232 },
    { 0x0359, 0x035A, 220 },
    { 0x035B, 0x035B, 230 },
    { 0x035C, 0x035C, 233 },
    { 0x035D, 0x035E, 234 },
    { 0x035F, 0x035F, 233 },
    { 0x0360, 0x0361, 234 },
    { 0x0362, 0x0362, 233 },
    { 0x0363, 0x036F, 230 },
    { 0x0483, 0x0487, 230 },
    { 0x0591, 0x05BD, 220 },
    { 0x05BF, 0x05BF, 230 },
    { 0x05C1, 0x05C2, 230 },
    { 0x05C4, 0x05C5, 230 },
    { 0x05C7, 0x05C7, 220 },
    { 0x0610, 0x061A, 230 },
    { 0x064B, 0x065F, 230 },
    { 0x0670, 0x0670, 35 },
    { 0x06D6, 0x06DC, 230 },
    { 0x06DF, 0x06E4, 230 },
    { 0x06E7, 0x06E8, 230 },
    { 0x06EA, 0x06ED, 220 },
    { 0x0711, 0x0711, 36 },
    { 0x0730, 0x074A, 230 },
    { 0x07A6, 0x07B0, 220 },
    { 0x0E48, 0x0E4B, 107 }, /* Thai above vowels */
    { 0x0EB8, 0x0EBC, 118 }, /* Lao */
    { 0x1AB0, 0x1AC1, 230 },
    { 0x1DC0, 0x1DFF, 230 },
    { 0x20D0, 0x20F0, 230 },
    { 0xFE20, 0xFE26, 230 },
};

/* ── Curated canonical decomposition table (Unicode Bridge) ──────────
 * Sorted ascending by src (the composed codepoint). Each entry maps to its
 * canonical decomposition. Values follow Unicode Normalization for the
 * bridge; SUCD (UnicodeData.txt) is the source of truth. */
typedef struct {
    sucs_ex_char_t src;
    sucs_ex_char_t d1;
    sucs_ex_char_t d2;
} sucf_decomp_t;

static const sucf_decomp_t SUCF_DECOMP_TABLE[] = {
    /* Latin-1 precomposed: A + combining marks */
    { 0x00C0, 0x0041, 0x0300 }, /* À */
    { 0x00C1, 0x0041, 0x0301 }, /* Á */
    { 0x00C2, 0x0041, 0x0302 }, /* Â */
    { 0x00C3, 0x0041, 0x0303 }, /* Ã */
    { 0x00C4, 0x0041, 0x0308 }, /* Ä */
    { 0x00C5, 0x0041, 0x030A }, /* Å */
    { 0x00C7, 0x0043, 0x0327 }, /* Ç */
    { 0x00C8, 0x0045, 0x0300 }, /* È */
    { 0x00C9, 0x0045, 0x0301 }, /* É */
    { 0x00CA, 0x0045, 0x0302 }, /* Ê */
    { 0x00CB, 0x0045, 0x0308 }, /* Ë */
    { 0x00CC, 0x0049, 0x0300 }, /* Ì */
    { 0x00CD, 0x0049, 0x0301 }, /* Í */
    { 0x00CE, 0x0049, 0x0302 }, /* Î */
    { 0x00CF, 0x0049, 0x0308 }, /* Ï */
    { 0x00D1, 0x004E, 0x0303 }, /* Ñ */
    { 0x00D2, 0x004F, 0x0300 }, /* Ò */
    { 0x00D3, 0x004F, 0x0301 }, /* Ó */
    { 0x00D4, 0x004F, 0x0302 }, /* Ô */
    { 0x00D5, 0x004F, 0x0303 }, /* Õ */
    { 0x00D6, 0x004F, 0x0308 }, /* Ö */
    { 0x00D9, 0x0055, 0x0300 }, /* Ù */
    { 0x00DA, 0x0055, 0x0301 }, /* Ú */
    { 0x00DB, 0x0055, 0x0302 }, /* Û */
    { 0x00DC, 0x0055, 0x0308 }, /* Ü */
    { 0x00DD, 0x0059, 0x0301 }, /* Ý */
    { 0x00E0, 0x0061, 0x0300 }, /* à */
    { 0x00E1, 0x0061, 0x0301 }, /* á */
    { 0x00E2, 0x0061, 0x0302 }, /* â */
    { 0x00E3, 0x0061, 0x0303 }, /* ã */
    { 0x00E4, 0x0061, 0x0308 }, /* ä */
    { 0x00E5, 0x0061, 0x030A }, /* å */
    { 0x00E7, 0x0063, 0x0327 }, /* ç */
    { 0x00E8, 0x0065, 0x0300 }, /* è */
    { 0x00E9, 0x0065, 0x0301 }, /* é */
    { 0x00EA, 0x0065, 0x0302 }, /* ê */
    { 0x00EB, 0x0065, 0x0308 }, /* ë */
    { 0x00EC, 0x0069, 0x0300 }, /* ì */
    { 0x00ED, 0x0069, 0x0301 }, /* í */
    { 0x00EE, 0x0069, 0x0302 }, /* î */
    { 0x00EF, 0x0069, 0x0308 }, /* ï */
    { 0x00F1, 0x006E, 0x0303 }, /* ñ */
    { 0x00F2, 0x006F, 0x0300 }, /* ò */
    { 0x00F3, 0x006F, 0x0301 }, /* ó */
    { 0x00F4, 0x006F, 0x0302 }, /* ô */
    { 0x00F5, 0x006F, 0x0303 }, /* õ */
    { 0x00F6, 0x006F, 0x0308 }, /* ö */
    { 0x00F9, 0x0075, 0x0300 }, /* ù */
    { 0x00FA, 0x0075, 0x0301 }, /* ú */
    { 0x00FB, 0x0075, 0x0302 }, /* û */
    { 0x00FC, 0x0075, 0x0308 }, /* ü */
    { 0x00FD, 0x0079, 0x0301 }, /* ý */
    { 0x00FF, 0x0079, 0x0308 }, /* ÿ */

    /* Latin Extended-A */
    { 0x0100, 0x0041, 0x0304 }, /* Ā */
    { 0x0101, 0x0061, 0x0304 }, /* ā */
    { 0x0102, 0x0041, 0x0306 }, /* Ă */
    { 0x0103, 0x0061, 0x0306 }, /* ă */
    { 0x0104, 0x0041, 0x0328 }, /* Ą */
    { 0x0105, 0x0061, 0x0328 }, /* ą */
    { 0x0106, 0x0043, 0x0301 }, /* Ć */
    { 0x0107, 0x0063, 0x0301 }, /* ć */
    { 0x0108, 0x0043, 0x0302 }, /* Ĉ */
    { 0x0109, 0x0063, 0x0302 }, /* ĉ */
    { 0x010A, 0x0043, 0x0307 }, /* Ċ */
    { 0x010B, 0x0063, 0x0307 }, /* ċ */
    { 0x010C, 0x0043, 0x030C }, /* Č */
    { 0x010D, 0x0063, 0x030C }, /* č */
    { 0x010E, 0x0044, 0x030C }, /* Ď */
    { 0x010F, 0x0064, 0x030C }, /* ď */
    { 0x0112, 0x0045, 0x0304 }, /* Ē */
    { 0x0113, 0x0065, 0x0304 }, /* ē */
    { 0x0116, 0x0045, 0x0307 }, /* Ė */
    { 0x0117, 0x0065, 0x0307 }, /* ė */
    { 0x0118, 0x0045, 0x0328 }, /* Ę */
    { 0x0119, 0x0065, 0x0328 }, /* ę */
    { 0x011A, 0x0045, 0x030C }, /* Ě */
    { 0x011B, 0x0065, 0x030C }, /* ě */
    { 0x011C, 0x0047, 0x0302 }, /* Ĝ */
    { 0x011D, 0x0067, 0x0302 }, /* ĝ */
    { 0x011E, 0x0047, 0x0306 }, /* Ğ */
    { 0x011F, 0x0067, 0x0306 }, /* ğ */
    { 0x0120, 0x0047, 0x0307 }, /* Ġ */
    { 0x0121, 0x0067, 0x0307 }, /* ġ */
    { 0x0122, 0x0047, 0x0327 }, /* Ģ */
    { 0x0123, 0x0067, 0x0327 }, /* ģ */
    { 0x0124, 0x0048, 0x0302 }, /* Ĥ */
    { 0x0125, 0x0068, 0x0302 }, /* ĥ */
    { 0x0128, 0x0049, 0x0303 }, /* Ĩ */
    { 0x0129, 0x0069, 0x0303 }, /* ĩ */
    { 0x012A, 0x0049, 0x0304 }, /* Ī */
    { 0x012B, 0x0069, 0x0304 }, /* ī */
    { 0x012E, 0x0049, 0x0328 }, /* Į */
    { 0x012F, 0x0069, 0x0328 }, /* į */
    { 0x0130, 0x0049, 0x0307 }, /* İ */
    { 0x0132, 0x0049, 0x004A }, /* Ĳ */
    { 0x0133, 0x0069, 0x006A }, /* ĳ */
    { 0x0134, 0x004A, 0x0302 }, /* Ĵ */
    { 0x0135, 0x006A, 0x0302 }, /* ĵ */
    { 0x0136, 0x004B, 0x0327 }, /* Ķ */
    { 0x0137, 0x006B, 0x0327 }, /* ķ */
    { 0x0139, 0x004C, 0x0301 }, /* Ĺ */
    { 0x013A, 0x006C, 0x0301 }, /* ĺ */
    { 0x013B, 0x004C, 0x0327 }, /* Ļ */
    { 0x013C, 0x006C, 0x0327 }, /* ļ */
    { 0x013D, 0x004C, 0x030C }, /* Ľ */
    { 0x013E, 0x006C, 0x030C }, /* ľ */
    { 0x0143, 0x004E, 0x0301 }, /* Ń */
    { 0x0144, 0x006E, 0x0301 }, /* ń */
    { 0x0145, 0x004E, 0x0327 }, /* Ņ */
    { 0x0146, 0x006E, 0x0327 }, /* ņ */
    { 0x0147, 0x004E, 0x030C }, /* Ň */
    { 0x0148, 0x006E, 0x030C }, /* ň */
    { 0x014C, 0x004F, 0x0304 }, /* Ō */
    { 0x014D, 0x006F, 0x0304 }, /* ō */
    { 0x0150, 0x004F, 0x030B }, /* Ő */
    { 0x0151, 0x006F, 0x030B }, /* ő */
    { 0x0152, 0x004F, 0x0045 }, /* Œ */
    { 0x0153, 0x006F, 0x0065 }, /* œ */
    { 0x0154, 0x0052, 0x0301 }, /* Ŕ */
    { 0x0155, 0x0072, 0x0301 }, /* ŕ */
    { 0x0156, 0x0052, 0x0327 }, /* Ŗ */
    { 0x0157, 0x0072, 0x0327 }, /* ŗ */
    { 0x0158, 0x0052, 0x030C }, /* Ř */
    { 0x0159, 0x0072, 0x030C }, /* ř */
    { 0x015A, 0x0053, 0x0301 }, /* Ś */
    { 0x015B, 0x0073, 0x0301 }, /* ś */
    { 0x015C, 0x0053, 0x0302 }, /* Ŝ */
    { 0x015D, 0x0073, 0x0302 }, /* ŝ */
    { 0x015E, 0x0053, 0x0327 }, /* Ş */
    { 0x015F, 0x0073, 0x0327 }, /* ş */
    { 0x0160, 0x0053, 0x030C }, /* Š */
    { 0x0161, 0x0073, 0x030C }, /* š */
    { 0x0162, 0x0054, 0x0327 }, /* Ţ */
    { 0x0163, 0x0074, 0x0327 }, /* ţ */
    { 0x0164, 0x0054, 0x030C }, /* Ť */
    { 0x0165, 0x0074, 0x030C }, /* ť */
    { 0x0168, 0x0055, 0x0303 }, /* Ũ */
    { 0x0169, 0x0075, 0x0303 }, /* ũ */
    { 0x016A, 0x0055, 0x0304 }, /* Ū */
    { 0x016B, 0x0075, 0x0304 }, /* ū */
    { 0x016C, 0x0055, 0x0306 }, /* Ŭ */
    { 0x016D, 0x0075, 0x0306 }, /* ŭ */
    { 0x016E, 0x0055, 0x030A }, /* Ů */
    { 0x016F, 0x0075, 0x030A }, /* ů */
    { 0x0170, 0x0055, 0x030B }, /* Ű */
    { 0x0171, 0x0075, 0x030B }, /* ű */
    { 0x0172, 0x0055, 0x0328 }, /* Ų */
    { 0x0173, 0x0075, 0x0328 }, /* ų */
    { 0x0174, 0x0057, 0x0302 }, /* Ŵ */
    { 0x0175, 0x0077, 0x0302 }, /* ŵ */
    { 0x0176, 0x0059, 0x0302 }, /* Ŷ */
    { 0x0177, 0x0079, 0x0302 }, /* ŷ */
    { 0x0178, 0x0059, 0x0308 }, /* Ÿ */
    { 0x0179, 0x005A, 0x0301 }, /* Ź */
    { 0x017A, 0x007A, 0x0301 }, /* ź */
    { 0x017B, 0x005A, 0x0307 }, /* Ż */
    { 0x017C, 0x007A, 0x0307 }, /* ż */
    { 0x017E, 0x007A, 0x030C }, /* ž */
    { 0x017F, 0x0073, 0x0000 }, /* ſ (long s) — singleton */

    /* Greek + other combining */
    { 0x0344, 0x0308, 0x0301 }, /* ̈́ COMBINING GREEK DIALYTIKA TONOS (non-starter exclusion) */
    { 0x0391, 0x0391, 0x0301 }, /* Ά (alpha + acute) */

    /* Tibetan (composition exclusion example) */
    { 0x0F73, 0x0F71, 0x0F72 }, /* ཱི */

    /* Non-starter composition exclusion */
    { 0x1E9B, 0x017F, 0x0307 }, /* ẛ (long s + dot) */

    /* Singleton composition exclusions (decompose but never recompose) */
    { 0x2126, 0x03A9, 0x0000 }, /* Ω OHM SIGN → Ω */
    { 0x212A, 0x004B, 0x0000 }, /* K KELVIN SIGN → K */
    { 0x212B, 0x00C5, 0x0000 }, /* Å ANGSTROM SIGN → Å */
};

/* ── Whether a codepoint is excluded from composition ────────────────
 * Tagged in the decomposition table: entries whose d2 == 0 are singletons
 * and never compose; the Greek/tibetan/non-starter entries above are the
 * canonical exclusion examples. The reorder engine composes only when the
 * (starter, mark) pair appears as a *two-element* decomposition whose d2
 * is the mark — singletons (d2==0) are excluded. */

/* Binary search for a canonical decomposition by composed source. */
static const sucf_decomp_t* sucf_decomp_find(sucs_ex_char_t cp)
{
    size_t lo = 0, hi = SUCF_NELEM(SUCF_DECOMP_TABLE);
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (SUCF_DECOMP_TABLE[mid].src < cp) lo = mid + 1;
        else hi = mid;
    }
    if (lo < SUCF_NELEM(SUCF_DECOMP_TABLE) &&
        SUCF_DECOMP_TABLE[lo].src == cp)
        return &SUCF_DECOMP_TABLE[lo];
    return NULL;
}

/* Binary search the CCC range table (sorted ascending by lo, non-overlap):
 * finds the range containing cp, or NULL when cp is a starter (CCC 0). */
static const sucf_ccc_range_t* sucf_ccc_find(sucs_ex_char_t cp)
{
    size_t lo = 0, hi = SUCF_NELEM(SUCF_CCC_TABLE);
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const sucf_ccc_range_t* r = &SUCF_CCC_TABLE[mid];
        if (cp < r->lo) hi = mid;
        else if (cp > r->hi) lo = mid + 1;
        else return r;
    }
    return NULL;
}

/* ── Zone classification ──────────────────────────────────────────── */

bool suas_sucf_is_scp(sucs_ex_char_t cp)
{
    return (cp >= SUCF_ZONE_SCP_MIN && cp <= SUCF_ZONE_SCP_MAX) ||
           (cp >= SUCF_BASE_TRAP_MIN && cp <= SUCF_BASE_SENTINEL);
}

bool suas_sucf_is_hangul(sucs_ex_char_t cp)
{
    return cp >= SUCF_HANGUL_BASE && cp <= SUCF_HANGUL_END;
}

/* Canonically invariant: SCP/BANcode/trap/sentinel, or a native/plugin
 * codepoint (with no combining properties by default). Bridge codepoints
 * with a CCC of 0 and no decomposition are also trivially invariant. */
bool suas_sucf_is_invariant(sucs_ex_char_t cp)
{
    if (suas_sucf_is_scp(cp))
        return true;
    if (cp >= SUCF_ZONE_NATIVE_MIN)  /* native SUCS & ExtSUCS plugin */
        return true;
    if (suas_sucf_is_hangul(cp))
        return false;                /* Hangul decomposes algorithmically */
    if (sucf_decomp_find(cp))
        return false;                /* has a canonical decomposition */
    if (sucf_ccc_find(cp) && suas_sucf_ccc(cp, NULL) != 0)
        return false;                /* is a combining mark */
    return true;
}

/* ── Properties ───────────────────────────────────────────────────── */

int suas_sucf_ccc(sucs_ex_char_t cp, const suas_sucf_options_t* o)
{
    if (o && o->count) {
        size_t i;
        for (i = 0; i < o->count; ++i) {
            if (cp >= o->overrides[i].lo && cp <= o->overrides[i].hi) {
                if (o->overrides[i].have_ccc)
                    return o->overrides[i].ccc;
                break;
            }
        }
    }
    if (cp >= SUCF_ZONE_NATIVE_MIN)
        return 0;
    {
        const sucf_ccc_range_t* r = sucf_ccc_find(cp);
        if (r) return r->ccc;
    }
    return 0;
}

bool suas_sucf_is_starter(sucs_ex_char_t cp, const suas_sucf_options_t* o)
{
    return suas_sucf_ccc(cp, o) == 0;
}

size_t suas_sucf_decompose_one(sucs_ex_char_t cp,
                               const suas_sucf_options_t* o,
                               sucs_ex_char_t* out, size_t cap)
{
    (void)o;
    if (!out || cap == 0) return 0;
    if (suas_sucf_is_hangul(cp)) {
        /* Algorithmic Hangul decomposition: S = L + V + T. */
        uint64_t s = cp - SUCF_HANGUL_BASE;
        uint64_t t = s % SUCF_HANGUL_T_COUNT;
        uint64_t lv = s / SUCF_HANGUL_T_COUNT;
        sucs_ex_char_t L = 0x1100 + (lv / SUCF_HANGUL_V_COUNT);
        sucs_ex_char_t V = 0x1161 + (lv % SUCF_HANGUL_V_COUNT);
        size_t n = 0;
        out[n++] = L;
        out[n++] = V;
        if (t != 0) {
            if (n >= cap) return cap;
            out[n++] = 0x11A7 + t;
        }
        return (n > cap) ? cap : n;
    }
    {
        const sucf_decomp_t* d = sucf_decomp_find(cp);
        if (d) {
            if (cap >= 1) out[0] = d->d1;
            if (d->d2 != 0 && cap >= 2) out[1] = d->d2;
            return (d->d2 != 0) ? 2 : 1;
        }
    }
    out[0] = cp;
    return 1;
}

/* ── Composition lookup for SUCF-C ──────────────────────────────────
 * Given a starter and a following combining mark, return the composed
 * codepoint, or 0 if no canonical composition exists (or the combination
 * is excluded from composition). */
static sucs_ex_char_t sucf_compose_pair(sucs_ex_char_t base,
                                        sucs_ex_char_t mark)
{
    size_t i;
    for (i = 0; i < SUCF_NELEM(SUCF_DECOMP_TABLE); ++i) {
        const sucf_decomp_t* d = &SUCF_DECOMP_TABLE[i];
        if (d->d1 == base && d->d2 == mark)
            return d->src;
    }
    return 0;
}

/* ── Algorithmic Hangul composition (suas_sucf_is_hangul) ────────────
 * Unicode's algorithmic syllable composition: leading Jamo range
 * 0x1100-0x1112, vowel 0x1161-0x1175, trailing 0x11A8-0x11C2. Returns the
 * composed LV or LVT syllable, or 0 when the pair does not combine. */
static int sucf_jamo_range(sucs_ex_char_t cp, sucs_ex_char_t* base, int* count)
{
    /* L: 0x1100..0x1112 (19) */
    if (cp >= 0x1100 && cp <= 0x1112) { *base = 0x1100; *count = 19; return 0; }
    /* V: 0x1161..0x1175 (21) */
    if (cp >= 0x1161 && cp <= 0x1175) { *base = 0x1161; *count = 21; return 1; }
    /* T: 0x11A8..0x11C2 (27) */
    if (cp >= 0x11A8 && cp <= 0x11C2) { *base = 0x11A8; *count = 27; return 2; }
    return -1;
}

static sucs_ex_char_t sucf_compose_hangul(sucs_ex_char_t a, sucs_ex_char_t b)
{
    sucs_ex_char_t abase, bbase;
    int acount, bcount, at, bt;
    at = sucf_jamo_range(a, &abase, &acount);
    bt = sucf_jamo_range(b, &bbase, &bcount);
    if (at == 0 && bt == 1) {
        /* L + V -> LV syllable */
        return SUCF_HANGUL_BASE + (a - abase) * SUCF_HANGUL_N_COUNT +
               (b - bbase) * SUCF_HANGUL_T_COUNT;
    }
    if (at == -1 && bt == 2 && suas_sucf_is_hangul(a)) {
        /* LV syllable + T -> LVT syllable (only when a has no trailing) */
        uint64_t s = a - SUCF_HANGUL_BASE;
        if (s % SUCF_HANGUL_T_COUNT == 0)
            return a + (b - bbase + 1);
    }
    return 0;
}

/* ── Sliding-window reordering engine ───────────────────────────────
 * Maintains the current starter plus a window of pending non-zero-CCC
 * marks. New marks are inserted into the window in CCC-ascending order
 * (swap while the preceding mark has a greater CCC), so the flushed
 * output is always canonically ordered. */

suas_sucf_status_t suas_sucf_state_init(suas_sucf_state_t* st,
                                        suas_sucf_form_t form,
                                        const suas_sucf_options_t* o)
{
    (void)o;
    if (!st) return SUAS_SUCF_ERR_NULL_POINTER;
    st->form     = form;
    st->pending  = 0;
    st->starter  = SUCF_BASE_SENTINEL;
    st->ninv     = 0;
    st->seeded   = false;
    return SUAS_SUCF_OK;
}

static suas_sucf_status_t sucf_emit(suas_sucf_state_t* st, sucs_ex_char_t cp,
                                    sucs_ex_char_t* out, size_t cap,
                                    size_t* out_count)
{
    size_t idx = *out_count;
    (void)st;
    if (idx >= cap) return SUAS_SUCF_ERR_BUFFER_TOO_SMALL;
    out[idx] = cp;
    *out_count = idx + 1;
    return SUAS_SUCF_OK;
}

/* Flush the pending-invariant FIFO (SCP / native / plugin markers) in
 * arrival order, preserving their position in the canonical stream. */
static suas_sucf_status_t sucf_flush_inv(suas_sucf_state_t* st,
                                         sucs_ex_char_t* out,
                                         size_t cap, size_t* out_count)
{
    int i;
    suas_sucf_status_t rc;
    for (i = 0; i < st->ninv; ++i) {
        rc = sucf_emit(st, st->inv[i], out, cap, out_count);
        if (rc != SUAS_SUCF_OK) return rc;
    }
    st->ninv = 0;
    return SUAS_SUCF_OK;
}

/* Finalize the current combining sequence and emit it in canonical order:
 * the (possibly composed) starter first, then the non-composed combining
 * marks in CCC-ascending window order, then any buffered invariant
 * markers. Emits nothing when the starter is the reserved sentinel. */
static suas_sucf_status_t sucf_finalize(suas_sucf_state_t* st,
                                        sucs_ex_char_t* out,
                                        size_t cap, size_t* out_count)
{
    int drop[SUCF_WINDOW];
    int i, blocked = 0;
    sucs_ex_char_t base = st->starter;
    suas_sucf_status_t rc;

    for (i = 0; i < st->pending; ++i) {
        if (st->form == SUAS_SUCF_FORM_C && !blocked) {
            sucs_ex_char_t c = sucf_compose_pair(base, st->win[i]);
            if (c) { base = c; drop[i] = 1; continue; }
        }
        drop[i] = 0;
        blocked = 1; /* a non-composing mark blocks later composition */
    }
    if (st->starter != SUCF_BASE_SENTINEL) {
        rc = sucf_emit(st, base, out, cap, out_count);
        if (rc != SUAS_SUCF_OK) return rc;
    }
    for (i = 0; i < st->pending; ++i) {
        if (!drop[i]) {
            rc = sucf_emit(st, st->win[i], out, cap, out_count);
            if (rc != SUAS_SUCF_OK) return rc;
        }
    }
    st->pending = 0;
    st->starter = SUCF_BASE_SENTINEL;
    return sucf_flush_inv(st, out, cap, out_count);
}

/* Absorb a single *already-decomposed* (or invariant) codepoint into the
 * sliding window, in the target form. A starter finalizes the previous
 * sequence (composed when possible under SUCF-C); a combining mark inserts
 * into the window in CCC-ascending order; an invariant marker is buffered
 * so it passes through at its original stream position. */
static suas_sucf_status_t sucf_absorb(suas_sucf_state_t* st,
                                      sucs_ex_char_t cp,
                                      const suas_sucf_options_t* o,
                                      sucs_ex_char_t* out,
                                      size_t cap, size_t* out_count)
{
    int k;
    suas_sucf_status_t rc;

    if (!st->seeded) {
        if (suas_sucf_is_scp(cp) || (cp >= SUCF_ZONE_NATIVE_MIN)) {
            /* Controls at stream head: buffer until a starter arrives so
             * their relative position is preserved. */
            if (st->ninv >= SUCF_INVAR_DEPTH) {
                rc = sucf_emit(st, st->inv[0], out, cap, out_count);
                if (rc != SUAS_SUCF_OK) return rc;
                for (k = 1; k < st->ninv; ++k) st->inv[k - 1] = st->inv[k];
                st->ninv--;
            }
            st->inv[st->ninv++] = cp;
            return SUAS_SUCF_OK;
        }
        rc = sucf_flush_inv(st, out, cap, out_count);
        if (rc != SUAS_SUCF_OK) return rc;
        st->seeded  = true;
        st->starter = cp;
        st->pending = 0;
        return SUAS_SUCF_OK;
    }

    /* SCP / native / plugin codepoints are canonically invariant: they pass
     * through unchanged and never disturb the combining-reorder state. */
    if (suas_sucf_is_scp(cp) || (cp >= SUCF_ZONE_NATIVE_MIN)) {
        if (st->ninv >= SUCF_INVAR_DEPTH) {
            rc = sucf_emit(st, st->inv[0], out, cap, out_count);
            if (rc != SUAS_SUCF_OK) return rc;
            for (k = 1; k < st->ninv; ++k) st->inv[k - 1] = st->inv[k];
            st->ninv--;
        }
        st->inv[st->ninv++] = cp;
        return SUAS_SUCF_OK;
    }

    {
        int ccc = suas_sucf_ccc(cp, o);

        if (ccc == 0) {
            /* A new starter. For SUCF-C, if this codepoint composes
             * algorithmically with the current starter (Hangul L+V or
             * LV+T), merge it in instead of emitting the old starter. */
            if (st->form == SUAS_SUCF_FORM_C &&
                st->pending == 0 && st->starter != SUCF_BASE_SENTINEL) {
                sucs_ex_char_t hc = sucf_compose_hangul(st->starter, cp);
                if (hc) {
                    st->starter = hc;
                    return SUAS_SUCF_OK;
                }
            }
            /* Finalize the previous sequence, then make cp the starter. */
            rc = sucf_finalize(st, out, cap, out_count);
            if (rc != SUAS_SUCF_OK) return rc;
            st->starter = cp;
            st->pending = 0;
        } else {
            /* A combining mark: insert into the window in CCC-ascending
             * order (stable for equal CCC; never move a starter). */
            if (st->pending >= SUCF_WINDOW) {
                /* Window full (stream-safe bound): commit the whole
                 * sequence, then the incoming mark re-seeds a fresh
                 * leading-mark window for any subsequent marks. */
                rc = sucf_finalize(st, out, cap, out_count);
                if (rc != SUAS_SUCF_OK) return rc;
                st->starter = SUCF_BASE_SENTINEL;
                st->win[0]  = cp;
                st->wccc[0] = ccc;
                st->pending = 1;
                return SUAS_SUCF_OK;
            }
            st->win[st->pending]  = cp;
            st->wccc[st->pending] = ccc;
            st->pending++;
            /* Reorder ascending (swap iff CCC_A > CCC_B and CCC_B != 0). */
            k = st->pending - 1;
            while (k > 0 && st->wccc[k - 1] > st->wccc[k] && st->wccc[k] != 0) {
                sucs_ex_char_t tc  = st->win[k - 1];
                int            tc2 = st->wccc[k - 1];
                st->win[k - 1]   = st->win[k];
                st->wccc[k - 1]  = st->wccc[k];
                st->win[k]       = tc;
                st->wccc[k]      = tc2;
                k--;
            }
        }
    }
    return SUAS_SUCF_OK;
}

suas_sucf_status_t suas_sucf_process_codepoint(suas_sucf_state_t* st,
                                               sucs_ex_char_t cp,
                                               const suas_sucf_options_t* o,
                                               sucs_ex_char_t* out,
                                               size_t cap, size_t* out_count)
{
    /* An incoming codepoint is canonicalized by decomposing it first, then
     * absorbing each decomposed element into the sliding window. This makes
     * SUCF-C (decompose → reorder → recompose) and SUCF-D (decompose →
     * reorder) both correct in a single pass. */
    sucs_ex_char_t parts[4];
    size_t n, i;
    suas_sucf_status_t rc;
    if (!st || !out || !out_count)
        return SUAS_SUCF_ERR_NULL_POINTER;
    if (suas_sucf_is_scp(cp) || (cp >= SUCF_ZONE_NATIVE_MIN)) {
        /* Invariant codepoint: buffer for pass-through in stream order. */
        return sucf_absorb(st, cp, o, out, cap, out_count);
    }
    if (suas_sucf_is_hangul(cp)) {
        if (st->form == SUAS_SUCF_FORM_D) {
            /* SUCF-D decomposes the syllable algorithmically. */
            n = suas_sucf_decompose_one(cp, o, parts, 4);
            for (i = 0; i < n; ++i) {
                rc = sucf_absorb(st, parts[i], o, out, cap, out_count);
                if (rc != SUAS_SUCF_OK) return rc;
            }
            return SUAS_SUCF_OK;
        }
        /* SUCF-C: an already-composed syllable is canonical; absorb alone;
         * explicit Jamo sequences are composed algorithmically in absorb. */
        return sucf_absorb(st, cp, o, out, cap, out_count);
    }
    n = suas_sucf_decompose_one(cp, o, parts, 4);
    for (i = 0; i < n; ++i) {
        rc = sucf_absorb(st, parts[i], o, out, cap, out_count);
        if (rc != SUAS_SUCF_OK) return rc;
    }
    return SUAS_SUCF_OK;
}

suas_sucf_status_t suas_sucf_flush(suas_sucf_state_t* st,
                                   sucs_ex_char_t* out,
                                   size_t cap, size_t* out_count)
{
    suas_sucf_status_t rc;
    if (!st || !out || !out_count)
        return SUAS_SUCF_ERR_NULL_POINTER;
    if (!st->seeded)
        return sucf_flush_inv(st, out, cap, out_count);
    rc = sucf_finalize(st, out, cap, out_count);
    if (rc != SUAS_SUCF_OK) return rc;
    st->seeded = false;
    return SUAS_SUCF_OK;
}

/* ── Quick check ──────────────────────────────────────────────────── */

suas_sucf_quick_t suas_sucf_quick_check(const sucs_ex_char_t* cps, size_t n,
                                        suas_sucf_form_t form,
                                        const suas_sucf_options_t* o)
{
    size_t i;
    int    prev_ccc = -1;
    sucs_ex_char_t starter = 0;
    if (!cps && n != 0) return SUAS_SUCF_QC_MAYBE;
    for (i = 0; i < n; ++i) {
        sucs_ex_char_t cp = cps[i];
        if (suas_sucf_is_scp(cp) || (cp >= SUCF_ZONE_NATIVE_MIN))
            continue;
        if (suas_sucf_is_hangul(cp)) {
            if (form == SUAS_SUCF_FORM_D) return SUAS_SUCF_QC_NO;
            starter = cp;  /* a composed syllable is already SUCF-C */
            prev_ccc = 0;
            continue;
        }
        {
            const sucf_decomp_t* d = sucf_decomp_find(cp);
            if (d) {
                if (form == SUAS_SUCF_FORM_D)
                    return SUAS_SUCF_QC_NO;   /* needs decomposition */
                if (d->d2 == 0)
                    return SUAS_SUCF_QC_NO;   /* singleton exclusion → decomposes in C too */
                starter = cp;                 /* precomposed pair is already SUCF-C */
                prev_ccc = 0;
                continue;
            }
        }
        {
            int ccc = suas_sucf_ccc(cp, o);
            if (ccc == 0) {
                starter  = cp;
                prev_ccc = 0;
            } else {
                if (form == SUAS_SUCF_FORM_C && starter != 0) {
                    if (sucf_compose_pair(starter, cp) != 0)
                        return SUAS_SUCF_QC_NO;  /* would compose → not maximal */
                }
                if (prev_ccc > ccc)
                    return SUAS_SUCF_QC_NO;      /* out of order */
                prev_ccc = ccc;
            }
        }
    }
    return SUAS_SUCF_QC_YES;
}

/* ── Bulk transform ───────────────────────────────────────────────── */

suas_sucf_status_t suas_sucf_transform(const sucs_ex_char_t* in, size_t n,
                                       suas_sucf_form_t form,
                                       const suas_sucf_options_t* o,
                                       sucs_ex_char_t* out, size_t cap,
                                       size_t* out_count)
{
    suas_sucf_state_t st;
    suas_sucf_status_t rc;
    size_t i;
    size_t written = 0;
    if (!in || !out || !out_count)
        return SUAS_SUCF_ERR_NULL_POINTER;
    if (cap < n)
        return SUAS_SUCF_ERR_BUFFER_TOO_SMALL; /* decomposition can exceed n */
    rc = suas_sucf_state_init(&st, form, o);
    if (rc != SUAS_SUCF_OK) return rc;
    for (i = 0; i < n; ++i) {
        rc = suas_sucf_process_codepoint(&st, in[i], o, out, cap, &written);
        if (rc != SUAS_SUCF_OK) return rc;
    }
    rc = suas_sucf_flush(&st, out, cap, &written);
    if (rc != SUAS_SUCF_OK) return rc;
    *out_count = written;
    return SUAS_SUCF_OK;
}

/* ── Version ──────────────────────────────────────────────────────── */

const char* suas_sucf_version_string(void)
{
    return "0.1.0";
}

void suas_sucf_options_default(suas_sucf_options_t* o)
{
    if (o) {
        o->overrides = NULL;
        o->count     = 0;
    }
}
