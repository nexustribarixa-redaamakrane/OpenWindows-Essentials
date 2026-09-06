/* SUAS-001 — Structural Directional Framing (SDF) — freestanding impl.
 *
 * Single-pass, zero-allocation, state-machine-driven bidirectional framing
 * with a Dual-Mode Directional Resolver over the SUCD BiDi database.
 * No heap allocation. Compiles with -std=c99 -ffreestanding.
 */

#include "suas/suas_sdf.h"

/* Validate a dir_type value. */
static int sdf_dir_valid(suas_dir_type_t d)
{
    switch (d) {
    case SUAS_DIR_NEUTRAL:
    case SUAS_DIR_LTR:
    case SUAS_DIR_RTL:
    case SUAS_DIR_MIRRORED_LTR:
    case SUAS_DIR_MIRRORED_RTL:
        return 1;
    default:
        return 0;
    }
}

static int sdf_is_writable_dir(suas_dir_type_t d)
{
    return (d == SUAS_DIR_LTR || d == SUAS_DIR_RTL);
}

/* ── SUCD BiDi lookup (embedded subset of sucd/Props/BidiProps.txt) ─ */

typedef struct {
    uint32_t lo;
    uint32_t hi;
    uint32_t mask;
} sucd_bidi_range_t;

static const sucd_bidi_range_t SUCD_BIDI_RANGES[] = {
    /* Whitespace / separators */
    { 0x0009, 0x000D, SUCD_BIDI_WHITESPACE },
    { 0x0020, 0x0020, SUCD_BIDI_WHITESPACE },
    { 0x00A0, 0x00A0, SUCD_BIDI_WHITESPACE },
    { 0x1680, 0x1680, SUCD_BIDI_WHITESPACE },
    { 0x2000, 0x200A, SUCD_BIDI_WHITESPACE },
    { 0x2028, 0x2029, SUCD_BIDI_WHITESPACE },
    { 0x202F, 0x202F, SUCD_BIDI_WHITESPACE },
    { 0x205F, 0x205F, SUCD_BIDI_WHITESPACE },
    { 0x3000, 0x3000, SUCD_BIDI_WHITESPACE },

    /* ASCII paired-format (mirrorable) glyphs */
    { 0x0028, 0x0029, SUCD_BIDI_MIRRORED },
    { 0x003C, 0x003E, SUCD_BIDI_MIRRORED },
    { 0x005B, 0x005D, SUCD_BIDI_MIRRORED },
    { 0x007B, 0x007D, SUCD_BIDI_MIRRORED },

    /* General Punctuation paired-format glyphs */
    { 0x2018, 0x2019, SUCD_BIDI_MIRRORED },
    { 0x201C, 0x201D, SUCD_BIDI_MIRRORED },
    { 0x2039, 0x203A, SUCD_BIDI_MIRRORED },

    /* Mathematical brackets (mirrorable) */
    { 0x27E6, 0x27EB, SUCD_BIDI_MIRRORED },

    /* Heavy brackets */
    { 0x2772, 0x2773, SUCD_BIDI_MIRRORED },

    /* White square brackets */
    { 0x301A, 0x301B, SUCD_BIDI_MIRRORED },

    /* Fullwidth forms (mirrorable) */
    { 0xFF08, 0xFF09, SUCD_BIDI_MIRRORED },
    { 0xFF3B, 0xFF3D, SUCD_BIDI_MIRRORED },
    { 0xFF5B, 0xFF5D, SUCD_BIDI_MIRRORED },

    /* Latin script (strong LTR) */
    { 0x0041, 0x005A, SUCD_BIDI_LTR },
    { 0x0061, 0x007A, SUCD_BIDI_LTR },
    { 0x00C0, 0x024F, SUCD_BIDI_LTR },
    { 0x0370, 0x03FF, SUCD_BIDI_LTR },  /* Greek */
    { 0x0400, 0x04FF, SUCD_BIDI_LTR },  /* Cyrillic */
    { 0x1E00, 0x1EFF, SUCD_BIDI_LTR },  /* Latin Extended Additional */

    /* Arabic script (right-to-left, ARABIC_AL) */
    { 0x0600, 0x06FF, SUCD_BIDI_ARABIC_AL },
    { 0x0750, 0x077F, SUCD_BIDI_ARABIC_AL },
    { 0x08A0, 0x08FF, SUCD_BIDI_ARABIC_AL },
    { 0xFB50, 0xFDFF, SUCD_BIDI_ARABIC_AL },
    { 0xFE70, 0xFEFF, SUCD_BIDI_ARABIC_AL },

    /* Hebrew script (strong RTL) */
    { 0x0590, 0x05FF, SUCD_BIDI_RTL },
    { 0xFB1D, 0xFB4F, SUCD_BIDI_RTL },

    /* Thaana (RTL) */
    { 0x0780, 0x07BF, SUCD_BIDI_RTL },

    /* N'Ko (RTL) */
    { 0x07C0, 0x07FF, SUCD_BIDI_RTL },

    /* Adlam (RTL) */
    { 0x1E900, 0x1E95F, SUCD_BIDI_RTL },

    /* Arabic-Indic digits (neutral) */
    { 0x0660, 0x0669, SUCD_BIDI_NEUTRAL },
    { 0x06F0, 0x06F9, SUCD_BIDI_NEUTRAL },
};

#define SUCD_BIDI_RANGE_COUNT \
    (sizeof(SUCD_BIDI_RANGES) / sizeof(SUCD_BIDI_RANGES[0]))

uint32_t suas_sucd_bidi(uint32_t cp)
{
    size_t i;

    if (!suas_sucd_is_unicode_bridge(cp)) {
        return SUCD_BIDI_NEUTRAL;
    }

    for (i = 0; i < SUCD_BIDI_RANGE_COUNT; ++i) {
        if (cp >= SUCD_BIDI_RANGES[i].lo && cp <= SUCD_BIDI_RANGES[i].hi) {
            return SUCD_BIDI_RANGES[i].mask;
        }
    }

    /* Unmapped codepoints within the bridge default to neutral. */
    return SUCD_BIDI_NEUTRAL;
}

/* ── Classification ────────────────────────────────────────────── */

suas_sdf_status_t suas_sdf_classify(uint32_t cp, suas_dir_type_t resolved,
                                    uint32_t bidi_mask,
                                    suas_dir_type_t* out_dir, int* out_mirror)
{
    if (out_dir == NULL || out_mirror == NULL || !sdf_dir_valid(resolved)) {
        return SUAS_SDF_ERR_INVALID_ARG;
    }

    /* `cp` is retained in the public signature for forward compatibility
     * (e.g. intrinsic script tables); classification is mask-driven. */
    (void)cp;

    *out_mirror = 0;

    /* 1. Paired-format glyph: mirror per active isolate direction. */
    if (bidi_mask & SUCD_BIDI_MIRRORED) {
        *out_mirror = 1;
        if (resolved == SUAS_DIR_RTL || resolved == SUAS_DIR_MIRRORED_RTL) {
            *out_dir = SUAS_DIR_MIRRORED_RTL;
        } else {
            *out_dir = SUAS_DIR_MIRRORED_LTR;
        }
        return SUAS_SDF_OK;
    }

    /* 2. Arabic letter or strong RTL resolves right-to-left. */
    if ((bidi_mask & SUCD_BIDI_ARABIC_AL) || (bidi_mask & SUCD_BIDI_RTL)) {
        *out_dir = SUAS_DIR_RTL;
        return SUAS_SDF_OK;
    }

    /* 3. Strong LTR resolves left-to-right. */
    if (bidi_mask & SUCD_BIDI_LTR) {
        *out_dir = SUAS_DIR_LTR;
        return SUAS_SDF_OK;
    }

    /* 4. Everything else (neutral / whitespace / unmapped) is neutral. */
    *out_dir = SUAS_DIR_NEUTRAL;
    return SUAS_SDF_OK;
}

/* ── State machine ─────────────────────────────────────────────── */

static suas_sdf_isolate_t* sdf_active(suas_sdf_state_t* st)
{
    return &st->stack[st->depth - 1];
}

void suas_sdf_init(suas_sdf_state_t* st, suas_dir_type_t initial_dir)
{
    size_t i;

    if (st == NULL) {
        return;
    }
    if (!sdf_dir_valid(initial_dir) || !sdf_is_writable_dir(initial_dir)) {
        initial_dir = SUAS_DIR_LTR;
    }
    for (i = 0; i < SUAS_SDF_STACK_DEPTH; ++i) {
        st->stack[i].base_dir = SUAS_DIR_LTR;
        st->stack[i].cur_dir  = SUAS_DIR_LTR;
    }
    st->depth       = 1; /* root isolate always present */
    st->initial_dir = initial_dir;
    st->runtime     = SUAS_SDF_RUNTIME_ACTIVE;
    st->stack[0].base_dir = initial_dir;
    st->stack[0].cur_dir  = initial_dir;
}

suas_sdf_status_t suas_sdf_process_codepoint(suas_sdf_state_t* st, uint32_t cp,
                                             suts32_framed_t* out, size_t* out_count)
{
    uint32_t bidi_mask;
    suas_dir_type_t dir;
    int mirror;

    if (st == NULL || out == NULL) {
        return SUAS_SDF_ERR_INVALID_ARG;
    }
    if (st->runtime == SUAS_SDF_RUNTIME_ERROR ||
        st->runtime == SUAS_SDF_RUNTIME_ENDED) {
        return SUAS_SDF_ERR_STATE;
    }

    /* ── Explicit Directorial Mode: SCP directives mutate the stack ── */
    switch (cp) {
    case SCP_DIR_LTR:
        sdf_active(st)->cur_dir = SUAS_DIR_LTR;
        return SUAS_SDF_OK;
    case SCP_DIR_RTL:
        sdf_active(st)->cur_dir = SUAS_DIR_RTL;
        return SUAS_SDF_OK;
    case SCP_DIR_ISOLATE_PUSH:
        if (st->depth >= SUAS_SDF_STACK_DEPTH) {
            st->runtime = SUAS_SDF_RUNTIME_ERROR;
            return SUAS_SDF_ERR_STACK_OVERFLOW;
        }
        {
            suas_sdf_isolate_t* parent = sdf_active(st);
            st->stack[st->depth].base_dir = parent->cur_dir;
            st->stack[st->depth].cur_dir  = parent->cur_dir;
            st->depth++;
        }
        return SUAS_SDF_OK;
    case SCP_DIR_ISOLATE_POP:
        if (st->depth <= 1) {
            st->runtime = SUAS_SDF_RUNTIME_ERROR;
            return SUAS_SDF_ERR_STACK_UNDERFLOW;
        }
        st->depth--;
        return SUAS_SDF_OK;
    default:
        break;
    }

    /* ── Ordinary codepoint: resolve via Dual-Mode resolver ───────── */
    bidi_mask = suas_sucd_bidi(cp);

    if (suas_sdf_classify(cp, sdf_active(st)->cur_dir, bidi_mask,
                          &dir, &mirror) != SUAS_SDF_OK) {
        st->runtime = SUAS_SDF_RUNTIME_ERROR;
        return SUAS_SDF_ERR_INVALID_ARG;
    }

    out->codepoint = cp & SUAS_CODEPOINT_MAX;
    out->dir_type  = (uint64_t)dir;
    out->mirrored  = (uint64_t)(mirror ? 1u : 0u);
    out->reserved  = 0;

    if (out_count != NULL) {
        (*out_count)++;
    }
    return SUAS_SDF_OK;
}

suas_sdf_status_t suas_sdf_finish(suas_sdf_state_t* st)
{
    if (st == NULL) {
        return SUAS_SDF_ERR_INVALID_ARG;
    }
    if (st->runtime == SUAS_SDF_RUNTIME_ERROR) {
        return SUAS_SDF_ERR_STATE;
    }
    st->runtime = SUAS_SDF_RUNTIME_ENDED;
    return SUAS_SDF_OK;
}

suas_dir_type_t suas_sdf_current_dir(const suas_sdf_state_t* st)
{
    if (st == NULL || st->depth < 1) {
        return SUAS_DIR_LTR;
    }
    return st->stack[st->depth - 1].cur_dir;
}

int suas_sdf_depth(const suas_sdf_state_t* st)
{
    if (st == NULL) {
        return 0;
    }
    return st->depth;
}

suas_sdf_runtime_t suas_sdf_runtime(const suas_sdf_state_t* st)
{
    if (st == NULL) {
        return SUAS_SDF_RUNTIME_ERROR;
    }
    return st->runtime;
}

/* ── One-shot full-buffer framing (streaming API is normative) ─── */

suas_sdf_status_t suas_sdf_frame(const uint32_t* cps, size_t cp_count,
                                 suts32_framed_t* out, size_t out_cap,
                                 size_t* out_count)
{
    suas_sdf_state_t st;
    size_t i;
    size_t written = 0;

    if (cps == NULL || out == NULL || out_cap == 0) {
        return SUAS_SDF_ERR_INVALID_ARG;
    }

    suas_sdf_init(&st, SUAS_DIR_LTR);

    for (i = 0; i < cp_count; ++i) {
        size_t before = written;
        suts32_framed_t scratch;
        suts32_framed_t* slot = (written < out_cap) ? &out[written] : &scratch;
        suas_sdf_status_t rc =
            suas_sdf_process_codepoint(&st, cps[i], slot, &written);
        if (rc == SUAS_SDF_ERR_STATE) {
            return SUAS_SDF_ERR_STATE;
        }
        if (written > before && slot == &scratch) {
            return SUAS_SDF_ERR_BUFFER_TOO_SMALL;
        }
    }

    if (out_count != NULL) {
        *out_count = written;
    }
    return SUAS_SDF_OK;
}

/* ────────────────────────────────────────────────────────────────
 * FULL BIDIRECTIONAL PROCESSING MODEL (UAX #9 )
 *
 * The single-pass framer above is the decode-time framing layer. The
 * following implements the full display-space bidirectional processing
 * model (UAX #9 semantics): paragraph level (P1-P3), explicit embedding /
 * override / isolate levels (X1-X8), weak types (W1-W7), neutral + bracket
 * pairs (N0-N2), implicit levels (I1-I2) and reordering + mirroring
 * (L1-L4). Zero heap; operates on caller-provided arrays.
 * ──────────────────────────────────────────────────────────────── */

#define SDF_BIDI_NELEM(a) (sizeof(a) / sizeof((a)[0]))
#define SDF_BIDI_MAX_LEVEL 125

/* Level helpers. */
static int sdf_bidi_is_odd(uint8_t l)  { return (l & 1) != 0; }
static int sdf_bidi_is_even(uint8_t l) { return (l & 1) == 0; }

/* BD6: least even / odd embedding level strictly greater than L. */
static uint8_t sdf_bidi_next_even(uint8_t l) { return (uint8_t)((l & ~1u) + 2u); }
static uint8_t sdf_bidi_next_odd(uint8_t l)  { return (uint8_t)(((l + 1u) & ~1u) + 1u); }

/* ── Embedded classification table (subset of sucd/Props/BidiProps.txt) ─ */

typedef struct { uint32_t lo, hi; suas_sdf_bidi_class_t cls; } sdf_crange_t;

static const sdf_crange_t SDF_BIDI_CLASS[] = {
    { 0x0000, 0x0008, SUAS_SDF_BIDI_BN },
    { 0x000E, 0x001B, SUAS_SDF_BIDI_BN },
    { 0x001C, 0x001E, SUAS_SDF_BIDI_B  },
    { 0x001F, 0x001F, SUAS_SDF_BIDI_S  },
    { 0x0020, 0x0020, SUAS_SDF_BIDI_WS },
    { 0x007F, 0x009F, SUAS_SDF_BIDI_BN },
    { 0x00A0, 0x00A0, SUAS_SDF_BIDI_WS },
    { 0x00AD, 0x00AD, SUAS_SDF_BIDI_BN },
    { 0x061C, 0x061C, SUAS_SDF_BIDI_AL },
    { 0x1680, 0x1680, SUAS_SDF_BIDI_WS },
    { 0x2000, 0x200A, SUAS_SDF_BIDI_WS },
    { 0x200B, 0x200D, SUAS_SDF_BIDI_BN },
    { 0x200E, 0x200E, SUAS_SDF_BIDI_L  },
    { 0x200F, 0x200F, SUAS_SDF_BIDI_R  },
    { 0x2028, 0x2028, SUAS_SDF_BIDI_WS },
    { 0x2029, 0x2029, SUAS_SDF_BIDI_B  },
    { 0x202A, 0x202A, SUAS_SDF_BIDI_LRE },
    { 0x202B, 0x202B, SUAS_SDF_BIDI_RLE },
    { 0x202C, 0x202C, SUAS_SDF_BIDI_PDF },
    { 0x202D, 0x202D, SUAS_SDF_BIDI_LRO },
    { 0x202E, 0x202E, SUAS_SDF_BIDI_RLO },
    { 0x202F, 0x202F, SUAS_SDF_BIDI_WS },
    { 0x205F, 0x205F, SUAS_SDF_BIDI_WS },
    { 0x2060, 0x2064, SUAS_SDF_BIDI_BN },
    { 0x2066, 0x2066, SUAS_SDF_BIDI_LRI },
    { 0x2067, 0x2067, SUAS_SDF_BIDI_RLI },
    { 0x2068, 0x2068, SUAS_SDF_BIDI_FSI },
    { 0x2069, 0x2069, SUAS_SDF_BIDI_PDI },
    { 0x206A, 0x206F, SUAS_SDF_BIDI_BN },
    { 0x3000, 0x3000, SUAS_SDF_BIDI_WS },
    { 0xFEFF, 0xFEFF, SUAS_SDF_BIDI_BN },
    { 0x0300, 0x036F, SUAS_SDF_BIDI_NSM },
    { 0x0483, 0x0489, SUAS_SDF_BIDI_NSM },
    { 0x0591, 0x05BD, SUAS_SDF_BIDI_NSM },
    { 0x05BF, 0x05BF, SUAS_SDF_BIDI_NSM },
    { 0x05C1, 0x05C2, SUAS_SDF_BIDI_NSM },
    { 0x05C4, 0x05C5, SUAS_SDF_BIDI_NSM },
    { 0x0610, 0x061A, SUAS_SDF_BIDI_NSM },
    { 0x064B, 0x065F, SUAS_SDF_BIDI_NSM },
    { 0x20D0, 0x20FF, SUAS_SDF_BIDI_NSM },
    { 0xFE20, 0xFE2F, SUAS_SDF_BIDI_NSM },
    { 0x0030, 0x0039, SUAS_SDF_BIDI_EN },
    { 0x002B, 0x002B, SUAS_SDF_BIDI_ES },
    { 0x002D, 0x002D, SUAS_SDF_BIDI_ES },
    { 0x0023, 0x0023, SUAS_SDF_BIDI_ET },
    { 0x0025, 0x0025, SUAS_SDF_BIDI_ET },
    { 0x00A2, 0x00A5, SUAS_SDF_BIDI_ET },
    { 0x00B0, 0x00B0, SUAS_SDF_BIDI_ET },
    { 0x2030, 0x2034, SUAS_SDF_BIDI_ET },
    { 0x0024, 0x0024, SUAS_SDF_BIDI_ET },
    { 0x00B2, 0x00B3, SUAS_SDF_BIDI_EN },
    { 0x00B9, 0x00B9, SUAS_SDF_BIDI_EN },
    { 0x00BC, 0x00BE, SUAS_SDF_BIDI_EN },
    { 0x0660, 0x0669, SUAS_SDF_BIDI_AN },
    { 0x06F0, 0x06F9, SUAS_SDF_BIDI_AN },
    { 0x066B, 0x066B, SUAS_SDF_BIDI_CS },
    { 0x066C, 0x066C, SUAS_SDF_BIDI_CS },
    { 0x0021, 0x0022, SUAS_SDF_BIDI_ON },
    { 0x0026, 0x0027, SUAS_SDF_BIDI_ON },
    { 0x002A, 0x002A, SUAS_SDF_BIDI_ON },
    { 0x002C, 0x002C, SUAS_SDF_BIDI_CS },
    { 0x002E, 0x002E, SUAS_SDF_BIDI_CS },
    { 0x002F, 0x002F, SUAS_SDF_BIDI_ON },
    { 0x003A, 0x003A, SUAS_SDF_BIDI_CS },
    { 0x003B, 0x003B, SUAS_SDF_BIDI_ON },
    { 0x003F, 0x0040, SUAS_SDF_BIDI_ON },
    { 0x005C, 0x005C, SUAS_SDF_BIDI_ON },
    { 0x005E, 0x0060, SUAS_SDF_BIDI_ON },
    { 0x0028, 0x0029, SUAS_SDF_BIDI_ON },
    { 0x003C, 0x003E, SUAS_SDF_BIDI_ON },
    { 0x005B, 0x005D, SUAS_SDF_BIDI_ON },
    { 0x007B, 0x007D, SUAS_SDF_BIDI_ON },
    { 0x007C, 0x007C, SUAS_SDF_BIDI_ON },
    { 0x00AB, 0x00BB, SUAS_SDF_BIDI_ON },
    { 0x2018, 0x2019, SUAS_SDF_BIDI_ON },
    { 0x201C, 0x201D, SUAS_SDF_BIDI_ON },
    { 0x2039, 0x203A, SUAS_SDF_BIDI_ON },
    { 0x27E6, 0x27EB, SUAS_SDF_BIDI_ON },
    { 0x2772, 0x2773, SUAS_SDF_BIDI_ON },
    { 0x301A, 0x301B, SUAS_SDF_BIDI_ON },
    { 0xFF08, 0xFF09, SUAS_SDF_BIDI_ON },
    { 0xFF3B, 0xFF3D, SUAS_SDF_BIDI_ON },
    { 0xFF5B, 0xFF5D, SUAS_SDF_BIDI_ON },
    { 0x0041, 0x005A, SUAS_SDF_BIDI_L  },
    { 0x0061, 0x007A, SUAS_SDF_BIDI_L  },
    { 0x00C0, 0x024F, SUAS_SDF_BIDI_L  },
    { 0x0370, 0x03FF, SUAS_SDF_BIDI_L  },
    { 0x0400, 0x04FF, SUAS_SDF_BIDI_L  },
    { 0x1E00, 0x1EFF, SUAS_SDF_BIDI_L  },
    { 0x2C00, 0x2C5F, SUAS_SDF_BIDI_L  },
    { 0x0600, 0x06FF, SUAS_SDF_BIDI_AL },
    { 0x0750, 0x077F, SUAS_SDF_BIDI_AL },
    { 0x08A0, 0x08FF, SUAS_SDF_BIDI_AL },
    { 0xFB50, 0xFDFF, SUAS_SDF_BIDI_AL },
    { 0xFE70, 0xFEFF, SUAS_SDF_BIDI_AL },
    { 0x0590, 0x05FF, SUAS_SDF_BIDI_R  },
    { 0xFB1D, 0xFB4F, SUAS_SDF_BIDI_R  },
    { 0x0780, 0x07BF, SUAS_SDF_BIDI_R  },
    { 0x07C0, 0x07FF, SUAS_SDF_BIDI_R  },
    { 0x1E900, 0x1E95F, SUAS_SDF_BIDI_R },
};

#define SDF_BIDI_CLASS_COUNT (SDF_BIDI_NELEM(SDF_BIDI_CLASS))

suas_sdf_bidi_class_t suas_sdf_bidi_classify(uint32_t cp)
{
    size_t i;
    if (!suas_sucd_is_unicode_bridge(cp)) return SUAS_SDF_BIDI_L;
    for (i = 0; i < SDF_BIDI_CLASS_COUNT; ++i) {
        if (cp >= SDF_BIDI_CLASS[i].lo && cp <= SDF_BIDI_CLASS[i].hi)
            return SDF_BIDI_CLASS[i].cls;
    }
    return SUAS_SDF_BIDI_L;
}

/* ── Mirroring (L4) ───────────────────────────────────────────── */

typedef struct { uint32_t from, to; } sdf_mirror_t;

static const sdf_mirror_t SDF_BIDI_MIRRORS[] = {
    { 0x0028, 0x0029 }, { 0x0029, 0x0028 },
    { 0x003C, 0x003E }, { 0x003E, 0x003C },
    { 0x005B, 0x005D }, { 0x005D, 0x005B },
    { 0x007B, 0x007D }, { 0x007D, 0x007B },
    { 0x00AB, 0x00BB }, { 0x00BB, 0x00AB },
    { 0x2018, 0x2019 }, { 0x2019, 0x2018 },
    { 0x201C, 0x201D }, { 0x201D, 0x201C },
    { 0x2039, 0x203A }, { 0x203A, 0x2039 },
    { 0x27E6, 0x27E7 }, { 0x27E7, 0x27E6 },
    { 0x27E8, 0x27E9 }, { 0x27E9, 0x27E8 },
    { 0x27EA, 0x27EB }, { 0x27EB, 0x27EA },
    { 0x2772, 0x2773 }, { 0x2773, 0x2772 },
    { 0x301A, 0x301B }, { 0x301B, 0x301A },
    { 0xFF08, 0xFF09 }, { 0xFF09, 0xFF08 },
    { 0xFF3B, 0xFF3D }, { 0xFF3D, 0xFF3B },
    { 0xFF5B, 0xFF5D }, { 0xFF5D, 0xFF5B },
};

void suas_sdf_bidi_mirror(uint32_t cp, uint32_t* paired, int* mirrored)
{
    size_t i;
    if (!suas_sucd_is_unicode_bridge(cp)) {
        if (paired) *paired = cp;
        if (mirrored) *mirrored = 0;
        return;
    }
    for (i = 0; i < SDF_BIDI_NELEM(SDF_BIDI_MIRRORS); ++i) {
        if (SDF_BIDI_MIRRORS[i].from == cp) {
            if (paired) *paired = SDF_BIDI_MIRRORS[i].to;
            if (mirrored) *mirrored = 1;
            return;
        }
    }
    if (paired) *paired = cp;
    if (mirrored) *mirrored = 0;
}

/* ── Bracket pairs (N0) ───────────────────────────────────────── */

typedef struct { uint32_t open, close; } sdf_bracket_t;

static const sdf_bracket_t SDF_BIDI_BRACKETS[] = {
    { 0x0028, 0x0029 }, { 0x005B, 0x005D }, { 0x007B, 0x007D },
    { 0x00AB, 0x00BB }, { 0x2018, 0x2019 }, { 0x201C, 0x201D },
    { 0x2039, 0x203A }, { 0x27E6, 0x27E7 }, { 0x27E8, 0x27E9 },
    { 0x27EA, 0x27EB }, { 0x2772, 0x2773 }, { 0x301A, 0x301B },
    { 0xFF08, 0xFF09 }, { 0xFF3B, 0xFF3D }, { 0xFF5B, 0xFF5D },
};

#define SDF_BIDI_BRACKET_COUNT (SDF_BIDI_NELEM(SDF_BIDI_BRACKETS))

static int sdf_bracket_open(uint32_t cp, uint32_t* close)
{
    size_t i;
    for (i = 0; i < SDF_BIDI_BRACKET_COUNT; ++i) {
        if (SDF_BIDI_BRACKETS[i].open == cp) { *close = SDF_BIDI_BRACKETS[i].close; return 1; }
    }
    return 0;
}

/* ── Paragraph level (P2/P3 first-strong) ──────────────────────── */

static int sdf_first_strong_rtl(const suas_sdf_run_t* r, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        if (r[i].cls == SUAS_SDF_BIDI_L) return 0;
        if (r[i].cls == SUAS_SDF_BIDI_R || r[i].cls == SUAS_SDF_BIDI_AL) return 1;
    }
    return 0;
}

/* ── X1-X8: explicit embedding / override / isolate levels ────── */

typedef struct {
    uint8_t level;
    int8_t  override;       /* -1 neutral, 0 L, 1 R */
    int     isolate_status; /* directional isolate status (BD12) */
} sdf_status_t;

#define SDF_BIDI_STATUS_STACK 128

static void sdf_phase_explicit(suas_sdf_run_t* r, size_t n, uint8_t para_level)
{
    sdf_status_t stack[SDF_BIDI_STATUS_STACK];
    int sp = 0;
    int over_iso = 0, over_emb = 0, valid_iso = 0;
    size_t i;

    stack[0].level = para_level;
    stack[0].override = -1;
    stack[0].isolate_status = 0;

    for (i = 0; i < n; ++i) {
        uint8_t cur_level = stack[sp].level;
        int cur_override = stack[sp].override;

        switch (r[i].cls) {
        case SUAS_SDF_BIDI_RLE: case SUAS_SDF_BIDI_LRE:
        case SUAS_SDF_BIDI_RLO: case SUAS_SDF_BIDI_LRO: {
            int odd = (r[i].cls == SUAS_SDF_BIDI_RLE || r[i].cls == SUAS_SDF_BIDI_RLO);
            uint8_t newl = odd ? sdf_bidi_next_odd(cur_level)
                               : sdf_bidi_next_even(cur_level);
            r[i].removed = 1;
            if (newl <= SDF_BIDI_MAX_LEVEL && over_iso == 0 && over_emb == 0) {
                ++sp;
                stack[sp].level = newl;
                stack[sp].override = (r[i].cls == SUAS_SDF_BIDI_LRO) ? 0 :
                                     (r[i].cls == SUAS_SDF_BIDI_RLO) ? 1 : -1;
                stack[sp].isolate_status = 0;
            } else if (over_iso == 0) {
                ++over_emb;
            }
            break;
        }
        case SUAS_SDF_BIDI_PDF:
            r[i].removed = 1;
            if (over_iso > 0) {
                /* inside an overflow isolate: nothing */
            } else if (over_emb > 0) {
                --over_emb;
            } else if (stack[sp].isolate_status == 0 && sp > 0) {
                --sp;
            }
            break;

        case SUAS_SDF_BIDI_RLI: case SUAS_SDF_BIDI_LRI: case SUAS_SDF_BIDI_FSI: {
            int odd;
            uint8_t newl;
            if (r[i].cls == SUAS_SDF_BIDI_RLI) odd = 1;
            else if (r[i].cls == SUAS_SDF_BIDI_LRI) odd = 0;
            else odd = sdf_first_strong_rtl(&r[i + 1], n - (i + 1));
            r[i].level = cur_level;
            if (cur_override != -1)
                r[i].cls = (cur_override == 0) ? SUAS_SDF_BIDI_L : SUAS_SDF_BIDI_R;
            newl = odd ? sdf_bidi_next_odd(cur_level) : sdf_bidi_next_even(cur_level);
            if (newl <= SDF_BIDI_MAX_LEVEL && over_iso == 0 && over_emb == 0) {
                ++valid_iso;
                ++sp;
                stack[sp].level = newl;
                stack[sp].override = -1;
                stack[sp].isolate_status = 1;
            } else {
                ++over_iso;
            }
            r[i].removed = 0;
            break;
        }
        case SUAS_SDF_BIDI_PDI:
            if (over_iso > 0) {
                --over_iso;
            } else if (valid_iso == 0) {
                /* matches no isolate */
            } else {
                over_emb = 0;
                while (sp > 0 && stack[sp].isolate_status == 0) --sp;
                --sp;
                --valid_iso;
            }
            r[i].level = stack[sp].level;
            r[i].removed = 0;
            if (stack[sp].override != -1)
                r[i].cls = (stack[sp].override == 0) ? SUAS_SDF_BIDI_L : SUAS_SDF_BIDI_R;
            break;

        case SUAS_SDF_BIDI_B:
            r[i].level = para_level;
            break;
        case SUAS_SDF_BIDI_BN:
            r[i].removed = 1;
            r[i].level = cur_level;
            break;

        default:
            r[i].level = cur_level;
            if (cur_override == 0) r[i].cls = SUAS_SDF_BIDI_L;
            else if (cur_override == 1) r[i].cls = SUAS_SDF_BIDI_R;
            break;
        }
    }

    (void)over_iso; (void)over_emb; (void)valid_iso;
}

/* ── W1-W7: weak types ────────────────────────────────────────── */

static int sdf_prev_nr(suas_sdf_run_t* r, size_t n, size_t i)
{
    size_t k = i;
    (void)n;
    while (k-- > 0) if (!r[k].removed) return (int)k;
    return -1;
}
static int sdf_next_nr(suas_sdf_run_t* r, size_t n, size_t i)
{
    size_t k = i;
    while (++k < n) if (!r[k].removed) return (int)k;
    return -1;
}

static void sdf_phase_weak(suas_sdf_run_t* r, size_t n)
{
    size_t i;

    /* W1: NSM takes the preceding character's class */
    for (i = 0; i < n; ++i) {
        int p;
        if (r[i].removed || r[i].cls != SUAS_SDF_BIDI_NSM) continue;
        p = sdf_prev_nr(r, n, i);
        if (p >= 0) r[i].cls = r[(size_t)p].cls;
    }

    /* W2/W3: track last strong; EN after AL -> AN; AL -> R */
    {
        int last_strong_al = 0;
        for (i = 0; i < n; ++i) {
            if (r[i].removed) continue;
            switch (r[i].cls) {
            case SUAS_SDF_BIDI_AL:
                r[i].cls = SUAS_SDF_BIDI_R;
                last_strong_al = 1;
                break;
            case SUAS_SDF_BIDI_R:
            case SUAS_SDF_BIDI_L:
                last_strong_al = 0;
                break;
            case SUAS_SDF_BIDI_EN:
                if (last_strong_al) r[i].cls = SUAS_SDF_BIDI_AN;
                break;
            default: break;
            }
        }
    }

    /* W4: ES -> EN between EN; CS -> EN/AN/ON */
    for (i = 0; i < n; ++i) {
        int p, nx;
        if (r[i].removed) continue;
        if (r[i].cls == SUAS_SDF_BIDI_ES) {
            p = sdf_prev_nr(r, n, i); nx = sdf_next_nr(r, n, i);
            if (p >= 0 && nx >= 0 &&
                r[p].cls == SUAS_SDF_BIDI_EN && r[nx].cls == SUAS_SDF_BIDI_EN)
                r[i].cls = SUAS_SDF_BIDI_EN;
        } else if (r[i].cls == SUAS_SDF_BIDI_CS) {
            p = sdf_prev_nr(r, n, i); nx = sdf_next_nr(r, n, i);
            if (p >= 0 && nx >= 0) {
                if (r[p].cls == r[nx].cls &&
                    (r[p].cls == SUAS_SDF_BIDI_EN || r[p].cls == SUAS_SDF_BIDI_AN))
                    r[i].cls = r[p].cls;
                else if (r[p].cls == SUAS_SDF_BIDI_EN && r[nx].cls == SUAS_SDF_BIDI_EN)
                    r[i].cls = SUAS_SDF_BIDI_EN;
                else
                    r[i].cls = SUAS_SDF_BIDI_ON;
            } else {
                r[i].cls = SUAS_SDF_BIDI_ON;
            }
        }
    }

    /* W5: ET run adjacent to EN -> EN */
    for (i = 0; i < n; ++i) {
        size_t start, end, k;
        int le, re, p, nx;
        if (r[i].removed || r[i].cls != SUAS_SDF_BIDI_ET) continue;
        start = i; end = i;
        while (end + 1 < n && !r[end + 1].removed &&
               r[end + 1].cls == SUAS_SDF_BIDI_ET) ++end;
        p = sdf_prev_nr(r, n, start);
        nx = sdf_next_nr(r, n, end);
        le = (p >= 0 && r[p].cls == SUAS_SDF_BIDI_EN);
        re = (nx >= 0 && r[nx].cls == SUAS_SDF_BIDI_EN);
        if (le || re) {
            for (k = start; k <= end; ++k) r[k].cls = SUAS_SDF_BIDI_EN;
        }
        i = end;
    }

    /* W6: ES/ET/CS -> ON */
    for (i = 0; i < n; ++i) {
        if (r[i].removed) continue;
        if (r[i].cls == SUAS_SDF_BIDI_ES || r[i].cls == SUAS_SDF_BIDI_ET ||
            r[i].cls == SUAS_SDF_BIDI_CS)
            r[i].cls = SUAS_SDF_BIDI_ON;
    }

    /* W7: EN -> L if last strong was L */
    {
        int last_strong_l = 0;
        for (i = 0; i < n; ++i) {
            if (r[i].removed) continue;
            switch (r[i].cls) {
            case SUAS_SDF_BIDI_L: last_strong_l = 1; break;
            case SUAS_SDF_BIDI_R:
            case SUAS_SDF_BIDI_AL: last_strong_l = 0; break;
            case SUAS_SDF_BIDI_EN:
                if (last_strong_l) r[i].cls = SUAS_SDF_BIDI_L;
                break;
            default: break;
            }
        }
    }
}

/* ── N0-N2: neutrals + bracket pairs ──────────────────────────── */

static int sdf_strong_dir(suas_sdf_bidi_class_t c)
{
    if (c == SUAS_SDF_BIDI_L) return 0;
    if (c == SUAS_SDF_BIDI_R || c == SUAS_SDF_BIDI_AN) return 1;
    return -1;
}
static int sdf_is_neutral(suas_sdf_bidi_class_t c)
{
    return (c == SUAS_SDF_BIDI_B || c == SUAS_SDF_BIDI_S || c == SUAS_SDF_BIDI_WS ||
            c == SUAS_SDF_BIDI_ON || c == SUAS_SDF_BIDI_LRI ||
            c == SUAS_SDF_BIDI_RLI || c == SUAS_SDF_BIDI_FSI || c == SUAS_SDF_BIDI_PDI);
}

static void sdf_phase_neutral(suas_sdf_run_t* r, size_t n)
{
    size_t i;

    /* N0: bracket pairs */
    for (i = 0; i < n; ++i) {
        size_t k;
        uint32_t close;
        if (r[i].removed || r[i].cls != SUAS_SDF_BIDI_ON) continue;
        if (!sdf_bracket_open(r[i].cp, &close)) continue;
        for (k = i + 1; k < n; ++k) {
            if (r[k].removed) continue;
            if (r[k].cls == SUAS_SDF_BIDI_ON && r[k].cp == close) {
                int dir = -1;
                size_t m;
                for (m = i + 1; m < k; ++m) {
                    if (r[m].removed) continue;
                    if (sdf_strong_dir(r[m].cls) >= 0) { dir = sdf_strong_dir(r[m].cls); break; }
                }
                if (dir < 0) dir = sdf_bidi_is_odd(r[i].level) ? 1 : 0;
                r[i].cls = dir ? SUAS_SDF_BIDI_R : SUAS_SDF_BIDI_L;
                r[k].cls = dir ? SUAS_SDF_BIDI_R : SUAS_SDF_BIDI_L;
                break;
            }
        }
    }

    /* N1: neutrals between two strongs of the same direction */
    for (i = 0; i < n; ++i) {
        size_t start = i, end = i, k;
        if (r[i].removed || !sdf_is_neutral(r[i].cls)) continue;
        while (end + 1 < n && !r[end + 1].removed && sdf_is_neutral(r[end + 1].cls))
            ++end;
        {
            int ld = -1, rd = -1;
            size_t p = start;
            while (p-- > 0) { if (!r[p].removed) { if (sdf_strong_dir(r[p].cls) >= 0) { ld = sdf_strong_dir(r[p].cls); break; } break; } }
            {
                size_t q = end;
                while (++q < n) { if (!r[q].removed) { if (sdf_strong_dir(r[q].cls) >= 0) { rd = sdf_strong_dir(r[q].cls); break; } break; } }
            }
            if (ld >= 0 && rd >= 0 && ld == rd) {
                suas_sdf_bidi_class_t tgt = ld ? SUAS_SDF_BIDI_R : SUAS_SDF_BIDI_L;
                for (k = start; k <= end; ++k)
                    if (!r[k].removed && sdf_is_neutral(r[k].cls))
                        r[k].cls = tgt;
            }
            i = end;
        }
    }

    /* N2: remaining neutrals take the embedding direction */
    for (i = 0; i < n; ++i) {
        if (r[i].removed) continue;
        if (sdf_is_neutral(r[i].cls))
            r[i].cls = sdf_bidi_is_odd(r[i].level) ? SUAS_SDF_BIDI_R : SUAS_SDF_BIDI_L;
    }
}

/* ── I1-I2: implicit levels ───────────────────────────────────── */

static void sdf_phase_implicit(suas_sdf_run_t* r, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        uint8_t l = r[i].level;
        if (r[i].removed) continue;
        switch (r[i].cls) {
        case SUAS_SDF_BIDI_R:
            if (sdf_bidi_is_even(l)) r[i].level = (uint8_t)(l + 1);
            break;
        case SUAS_SDF_BIDI_EN:
        case SUAS_SDF_BIDI_AN:
            if (sdf_bidi_is_even(l)) r[i].level = (uint8_t)(l + 2);
            else r[i].level = (uint8_t)(l + 1);
            break;
        case SUAS_SDF_BIDI_L:
            if (sdf_bidi_is_odd(l)) r[i].level = (uint8_t)(l + 1);
            break;
        default: break;
        }
    }
}

/* ── L1-L4: reordering + mirroring ────────────────────────────── */

static void sdf_phase_reorder(const suas_sdf_run_t* r, size_t n,
                              uint8_t para_level, int* visual)
{
    uint8_t highest = para_level;
    size_t i;
    int lvl[SUAS_SDF_BIDI_MAX_LEN];
    int ord[SUAS_SDF_BIDI_MAX_LEN];
    uint8_t l;

    if (n > SUAS_SDF_BIDI_MAX_LEN) return;

    for (i = 0; i < n; ++i) {
        lvl[i] = r[i].level;
        ord[i] = (int)i;
        if (r[i].level > highest) highest = r[i].level;
    }

    /* L1: trailing WS/segment/paragraph separators -> paragraph level */
    if (n > 0) {
        size_t t = n;
        while (t > 0) {
            size_t j = t - 1;
            if (r[j].removed) { --t; continue; }
            if (r[j].cls == SUAS_SDF_BIDI_S || r[j].cls == SUAS_SDF_BIDI_WS ||
                r[j].cls == SUAS_SDF_BIDI_B) {
                lvl[j] = para_level;
                --t;
            } else {
                break;
            }
        }
    }

    /* L2: reverse each maximal run at level >= l, for l from highest down. */
    for (l = (uint8_t)(para_level + 1); l <= highest; ++l) {
        size_t a = 0;
        while (a < n) {
            if (lvl[a] >= (int)l) {
                size_t b = a;
                size_t lo, hi;
                while (b < n && lvl[b] >= (int)l) ++b;
                lo = a; hi = b - 1;
                while (lo < hi) {
                    int t = ord[lo]; ord[lo] = ord[hi]; ord[hi] = t;
                    ++lo; --hi;
                }
                a = b;
            } else {
                ++a;
            }
        }
    }

    for (i = 0; i < n; ++i) visual[i] = ord[i];
}

suas_sdf_bidi_status_t suas_sdf_resolve_paragraph(
    const uint32_t* cps, size_t cp_count,
    suas_sdf_para_dir_t para,
    suas_sdf_run_t* out,
    int* visual,
    uint8_t* out_para_level)
{
    size_t i;
    uint8_t para_level;

    if (cps == NULL || out == NULL) return SUAS_SDF_BIDI_ERR_INVALID_ARG;
    if (cp_count == 0) return SUAS_SDF_BIDI_OK;
    if (cp_count > SUAS_SDF_BIDI_MAX_LEN) return SUAS_SDF_BIDI_ERR_TOO_LONG;

    for (i = 0; i < cp_count; ++i) {
        out[i].cp = cps[i];
        out[i].cls = suas_sdf_bidi_classify(cps[i]);
        out[i].level = 0;
        out[i].mirrored = 0;
        out[i].removed = 0;
    }

    /* P1-P3: paragraph level */
    if (para == SUAS_SDF_PARA_LTR) para_level = 0;
    else if (para == SUAS_SDF_PARA_RTL) para_level = 1;
    else para_level = sdf_first_strong_rtl(out, cp_count) ? 1 : 0;

    if (out_para_level) *out_para_level = para_level;

    sdf_phase_explicit(out, cp_count, para_level);
    sdf_phase_weak(out, cp_count);
    sdf_phase_neutral(out, cp_count);
    sdf_phase_implicit(out, cp_count);

    if (visual) sdf_phase_reorder(out, cp_count, para_level, visual);

    /* L4: mirroring */
    for (i = 0; i < cp_count; ++i) {
        uint32_t paired = 0;
        int mirrored = 0;
        if (out[i].removed) { out[i].mirrored = 0; continue; }
        if (sdf_bidi_is_odd(out[i].level)) {
            suas_sdf_bidi_mirror(out[i].cp, &paired, &mirrored);
        }
        if (mirrored) { out[i].cp = paired; out[i].mirrored = 1; }
        else out[i].mirrored = 0;
    }

    return SUAS_SDF_BIDI_OK;
}
