#ifndef SUAS_SUAS_SDF_H
#define SUAS_SUAS_SDF_H

/* SUAS-001 — Structural Directional Framing (SDF)
 *
 * Core architecture for bi-directional text layout, scope isolation, and
 * glyph mirroring. Single-pass, zero-allocation, state-machine-driven,
 * with a Dual-Mode Directional Resolver over the SUCD BiDi database.
 *
 * See docs/suas/SUAS-001-sdf.md for the normative specification.
 */

#include <stdint.h>
#include <stddef.h>

#include "suas/suas_core.h"
#include "suas/suas_sucd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Compile-time configuration (freestanding-safe overridable) ──── */

#ifndef SUAS_SDF_STACK_DEPTH
#define SUAS_SDF_STACK_DEPTH 32
#endif

/* ── SCP Directional Directives ─────────────────────────────────── */

#define SUAS_SDF_DIR_BLOCK     0x00110100UL

#define SCP_DIR_LTR            (SUAS_SDF_DIR_BLOCK + 0x01UL)
#define SCP_DIR_RTL            (SUAS_SDF_DIR_BLOCK + 0x02UL)
#define SCP_DIR_ISOLATE_PUSH   (SUAS_SDF_DIR_BLOCK + 0x04UL)
#define SCP_DIR_ISOLATE_POP    (SUAS_SDF_DIR_BLOCK + 0x08UL)

/* ── Directional Classification (dir_type field) ────────────────── */

typedef enum {
    SUAS_DIR_NEUTRAL       = 0, /* No intrinsic direction; takes isolate resolved direction */
    SUAS_DIR_LTR           = 1, /* Intrinsically left-to-right */
    SUAS_DIR_RTL           = 2, /* Intrinsically right-to-left */
    SUAS_DIR_MIRRORED_LTR  = 3, /* Paired/mirrorable glyph resolving LTR */
    SUAS_DIR_MIRRORED_RTL  = 4  /* Paired/mirrorable glyph resolving RTL */
} suas_dir_type_t;

/* ── Framed Output Word (64-bit) ────────────────────────────────── */

typedef struct {
    uint64_t codepoint : 31; /* bits 0..30  */
    uint64_t dir_type  : 3;  /* bits 31..33 */
    uint64_t mirrored  : 1;  /* bit  34     */
    uint64_t reserved  : 29; /* bits 35..63 */
} suts32_framed_t;

#define SUAS_SDF_FRAMED_CP(w)    ((uint32_t)((w).codepoint))
#define SUAS_SDF_FRAMED_DIR(w)   ((suas_dir_type_t)((w).dir_type))
#define SUAS_SDF_FRAMED_MIRROR(w) ((int)((w).mirrored))

/* ── SDF structural state ───────────────────────────────────────── */

typedef enum {
    SUAS_SDF_RUNTIME_ACTIVE = 0,
    SUAS_SDF_RUNTIME_ENDED  = 1,
    SUAS_SDF_RUNTIME_ERROR  = 2
} suas_sdf_runtime_t;

typedef struct {
    suas_dir_type_t base_dir; /* explicit base direction installed on open */
    suas_dir_type_t cur_dir;  /* live/resolved direction at top of isolate */
} suas_sdf_isolate_t;

/**
 * suas_sdf_state_t — the full structural state of the SDF engine.
 *
 * Contains the fixed-depth isolation stack (zero heap) plus the runtime
 * condition. Push/pop are O(1) on the embedded array.
 */
typedef struct {
    suas_sdf_isolate_t stack[SUAS_SDF_STACK_DEPTH];
    int                depth;        /* isolate count; root == 1            */
    suas_dir_type_t    initial_dir;  /* base direction of the root isolate  */
    suas_sdf_runtime_t runtime;      /* ACTIVE / ENDED / ERROR              */
} suas_sdf_state_t;

/* ── Status codes (extend suas_status_t) ────────────────────────── */

typedef enum {
    SUAS_SDF_OK                   =  0,
    SUAS_SDF_ERR_INVALID_ARG      = -1,
    SUAS_SDF_ERR_BUFFER_TOO_SMALL = -2,
    SUAS_SDF_ERR_STACK_OVERFLOW   = -3,
    SUAS_SDF_ERR_STACK_UNDERFLOW  = -4,
    SUAS_SDF_ERR_STATE            = -5  /* input after finish() or while errored */
} suas_sdf_status_t;

/* ── Core dispatch: Dual-Mode Directional Resolver ──────────────── */

/**
 * Initializes the SDF structural state. The root isolate is created with
 * @p initial_dir (SUAS_DIR_LTR used when an invalid/neutral value is passed).
 */
void suas_sdf_init(suas_sdf_state_t* st, suas_dir_type_t initial_dir);

/**
 * Consumes a single codepoint through the Dual-Mode Directional Resolver:
 *
 *  - SCP directive       -> push/pop/switch the isolate stack; emit nothing.
 *  - SCP (non-directive) -> emit a neutral, non-advancing control word.
 *  - Unicode Bridge      -> classify from the SUCD BiDi mask.
 *  - Native SUCS         -> inherit the active isolate's resolved direction.
 *
 * @param st         SDF structural state.
 * @param cp         the decoded SUCS codepoint (31-bit).
 * @param out        buffer receiving a framed word (must be non-NULL for
 *                   codepoints; ignored for directives).
 * @param out_count  optional counter incremented by 1 when a framed word
 *                   is produced.
 * @return SUAS_SDF_OK or a negative structural error.
 */
suas_sdf_status_t suas_sdf_process_codepoint(suas_sdf_state_t* st, uint32_t cp,
                                             suts32_framed_t* out, size_t* out_count);

/**
 * Ends input. Subsequent process_codepoint() calls fail with SUAS_SDF_ERR_STATE.
 */
suas_sdf_status_t suas_sdf_finish(suas_sdf_state_t* st);

/**
 * Returns the active isolate's resolved direction.
 */
suas_dir_type_t suas_sdf_current_dir(const suas_sdf_state_t* st);

/**
 * Returns the current isolate depth (root isolate counts as depth 1).
 */
int suas_sdf_depth(const suas_sdf_state_t* st);

/**
 * Returns the runtime condition (ACTIVE / ENDED / ERROR).
 */
suas_sdf_runtime_t suas_sdf_runtime(const suas_sdf_state_t* st);

/* ── Classification helper (exposed for tests/tools) ────────────── */

/**
 * Classifies a codepoint into a dir_type given the active isolate's resolved
 * direction, using the SUCD BiDi mask. Produces dir_type + mirrored metadata.
 *
 * @param cp        the decoded SUCS codepoint (31-bit).
 * @param resolved  resolved direction of the active isolate.
 * @param bidi_mask SUCD BiDi bitmask for @p cp (from suas_sucd_bidi()).
 * @param out_dir   receives the 3-bit dir_type.
 * @param out_mirror receives 1 if the word must be auto-mirrored.
 * @return SUAS_SDF_OK or SUAS_SDF_ERR_INVALID_ARG.
 */
suas_sdf_status_t suas_sdf_classify(uint32_t cp, suas_dir_type_t resolved,
                                    uint32_t bidi_mask,
                                    suas_dir_type_t* out_dir, int* out_mirror);

/* ── Legacy convenience: one-shot framing of a full buffer ──────── */
/* Uses the same single-pass engine; documented for convenience only. */

/**
 * Frames a full length-bounded codepoint array in a single call.
 *
 * @param cps        input codepoints (logical order).
 * @param cp_count   number of input codepoints.
 * @param out        destination framed-word buffer.
 * @param out_cap    capacity of out in framed words.
 * @param out_count  optional: receives the number of framed words written.
 * @return SUAS_SDF_OK or a negative structural error.
 */
suas_sdf_status_t suas_sdf_frame(const uint32_t* cps, size_t cp_count,
                                 suts32_framed_t* out, size_t out_cap,
                                 size_t* out_count);

/* ────────────────────────────────────────────────────────────────
 * FULL BIDIRECTIONAL PROCESSING MODEL (UAX #9 equivalent)
 *
 * The single-pass framer above is the decode-time framing layer. SDF also
 * implements the full display-space bidirectional processing model
 * (UAX #9 semantics) so that a renderer can produce visual order, resolved
 * embedding levels, weak/neutral resolution and mirroring directly. All of
 * the following run on caller-provided arrays; no heap.
 * ──────────────────────────────────────────────────────────────── */

#ifndef SUAS_SDF_BIDI_MAX_LEN
#define SUAS_SDF_BIDI_MAX_LEN 4096 /* max codepoints per paragraph run */
#endif

/* Bidirectional character types (UAX #9 BD1). */
typedef enum {
    SUAS_SDF_BIDI_L   =  0,  /* Left-to-right */
    SUAS_SDF_BIDI_R   =  1,  /* Right-to-left */
    SUAS_SDF_BIDI_AL  =  2,  /* Arabic letter */
    SUAS_SDF_BIDI_EN  =  3,  /* European number */
    SUAS_SDF_BIDI_ES  =  4,  /* European separator */
    SUAS_SDF_BIDI_ET  =  5,  /* European terminator */
    SUAS_SDF_BIDI_AN  =  6,  /* Arabic number */
    SUAS_SDF_BIDI_CS  =  7,  /* Common separator */
    SUAS_SDF_BIDI_NSM =  8,  /* Nonspacing mark */
    SUAS_SDF_BIDI_BN  =  9,  /* Boundary neutral */
    SUAS_SDF_BIDI_B   = 10,  /* Paragraph separator */
    SUAS_SDF_BIDI_S   = 11,  /* Segment separator */
    SUAS_SDF_BIDI_WS  = 12,  /* Whitespace */
    SUAS_SDF_BIDI_ON  = 13,  /* Other neutral */
    SUAS_SDF_BIDI_LRE = 14,  /* Left-to-right embedding */
    SUAS_SDF_BIDI_LRO = 15,  /* Left-to-right override */
    SUAS_SDF_BIDI_RLE = 16,  /* Right-to-left embedding */
    SUAS_SDF_BIDI_RLO = 17,  /* Right-to-left override */
    SUAS_SDF_BIDI_PDF = 18,  /* Pop directional formatting */
    SUAS_SDF_BIDI_LRI = 19,  /* Left-to-right isolate */
    SUAS_SDF_BIDI_RLI = 20,  /* Right-to-left isolate */
    SUAS_SDF_BIDI_FSI = 21,  /* First strong isolate */
    SUAS_SDF_BIDI_PDI = 22,  /* Pop directional isolate */
    SUAS_SDF_BIDI_COUNT
} suas_sdf_bidi_class_t;

/* Directional formatting / isolate control characters. */
#define SUAS_SDF_CP_LRM  0x200EUL
#define SUAS_SDF_CP_RLM  0x200FUL
#define SUAS_SDF_CP_ALM  0x061CUL
#define SUAS_SDF_CP_LRE  0x202AUL
#define SUAS_SDF_CP_RLE  0x202BUL
#define SUAS_SDF_CP_PDF  0x202CUL
#define SUAS_SDF_CP_LRO  0x202DUL
#define SUAS_SDF_CP_RLO  0x202EUL
#define SUAS_SDF_CP_LRI  0x2066UL
#define SUAS_SDF_CP_RLI  0x2067UL
#define SUAS_SDF_CP_FSI  0x2068UL
#define SUAS_SDF_CP_PDI  0x2069UL

/* Paragraph base-direction selector (P2/P3 or explicit). */
typedef enum {
    SUAS_SDF_PARA_AUTO = 0, /* first-strong (P2/P3)  */
    SUAS_SDF_PARA_LTR  = 1, /* force level 0         */
    SUAS_SDF_PARA_RTL  = 2  /* force level 1         */
} suas_sdf_para_dir_t;

/* Per-codepoint display result produced by the paragraph resolver. */
typedef struct {
    uint32_t             cp;       /* the SUCS codepoint                 */
    suas_sdf_bidi_class_t cls;     /* resolved BiDi class                 */
    uint8_t              level;    /* resolved embedding level (0..126)   */
    int                  mirrored; /* 1 if glyph must be mirrored (L4)    */
    int                  removed;  /* 1 if removed by X9 (not rendered)   */
} suas_sdf_run_t;

typedef enum {
    SUAS_SDF_BIDI_OK                  =  0,
    SUAS_SDF_BIDI_ERR_INVALID_ARG     = -10,
    SUAS_SDF_BIDI_ERR_TOO_LONG        = -11,
    SUAS_SDF_BIDI_ERR_EXPLICIT_OVERFLOW = -12
} suas_sdf_bidi_status_t;

/**
 * Classifies a codepoint into its bidirectional character type using the
 * embedded SUCD class table. Unicode-Bridge codepoints use the table; SCP
 * directives and native codepoints default to L.
 */
suas_sdf_bidi_class_t suas_sdf_bidi_classify(uint32_t cp);

/**
 * Returns the mirrored counterpart (L4) for a pair-format glyph.
 * Non-mirrorable codepoints return paired==cp, mirrored==0.
 */
void suas_sdf_bidi_mirror(uint32_t cp, uint32_t* paired, int* mirrored);

/**
 * Resolves the full bidirectional layout of one paragraph (a contiguous run
 * of codepoints). Produces, for each input codepoint, a suas_sdf_run_t in
 * logical order (do not reorder `out`). When @p visual is non-NULL, it
 * receives (of size cp_count) the visual-order positions as indices into
 * `out` per the L1-L2 reordering.
 *
 * @return SUAS_SDF_BIDI_OK or a negative error.
 */
suas_sdf_bidi_status_t suas_sdf_resolve_paragraph(
    const uint32_t* cps, size_t cp_count,
    suas_sdf_para_dir_t para,
    suas_sdf_run_t* out,
    int* visual,
    uint8_t* out_para_level);

#ifdef __cplusplus
}
#endif

#endif /* SUAS_SUAS_SDF_H */
