#ifndef SUAS_SUAS_SUCF_H
#define SUAS_SUAS_SUCF_H

/* SUAS-004 — SuperUnicode Canonical Forms (SUCF)
 *
 * Core architecture for canonical equivalence over the full 64-bit SUCS
 * codepoint space: two canonical transforms — SUCF-C (Canonical Composition,
 * the compact storage/equality target) and SUCF-D (Canonical Decomposition,
 * the exploded analysis/sorting target) — computed in a single pass, with
 * zero allocation, inside a small stack-allocated sliding reordering window.
 * Unicode Bridge semantics follow UAX #15 via SUCD binary property tables;
 * SCP instructions, BANcodes and trap markers are canonically invariant.
 *
 * See docs/suas/SUAS-004-sucf.md for the normative specification.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ──────────────────────────────────────────────────────── */
#define SUAS_SUCF_VERSION_MAJOR 0
#define SUAS_SUCF_VERSION_MINOR 1
#define SUAS_SUCF_VERSION_PATCH 0

/* ── 64-bit ExtSUCS codepoint type (guarded to coexist) ───────────── */
#ifndef EXTSUCS_CHAR_T_DEFINED
#define EXTSUCS_CHAR_T_DEFINED
typedef uint64_t sucs_ex_char_t;
#endif

/* ── Base SUCS zone boundaries (duplicated for freestanding SUCF) ─── */
#define SUCF_ZONE_UNICODE_MAX  ((sucs_ex_char_t)0x0010FFFFULL)
#define SUCF_ZONE_SCP_MIN      ((sucs_ex_char_t)0x00110000ULL)
#define SUCF_ZONE_SCP_MAX      ((sucs_ex_char_t)0x0011FFFFULL)
#define SUCF_ZONE_NATIVE_MIN   ((sucs_ex_char_t)0x00120000ULL)
#define SUCF_BASE_SUCS_MAX     ((sucs_ex_char_t)0x7FFFFFFFULL)
#define SUCF_BASE_TRAP_MIN     ((sucs_ex_char_t)0x7FFFFFF0ULL)
#define SUCF_BASE_TRAP_MAX     ((sucs_ex_char_t)0x7FFFFFFEULL)
#define SUCF_BASE_SENTINEL     ((sucs_ex_char_t)0x7FFFFFFFULL)

/* ── Hangul algorithmic constants (Unicode algorithm, no table) ───── */
#define SUCF_HANGUL_BASE        ((sucs_ex_char_t)0xAC00ULL)
#define SUCF_HANGUL_END         ((sucs_ex_char_t)0xD7A3ULL)
#define SUCF_HANGUL_L_COUNT     19
#define SUCF_HANGUL_V_COUNT     21
#define SUCF_HANGUL_T_COUNT     28
#define SUCF_HANGUL_N_COUNT     (SUCF_HANGUL_V_COUNT * SUCF_HANGUL_T_COUNT)

/* ── Combining / control special codepoints ──────────────────────── */
#define SUCF_CP_CGJ             0x034FUL   /* COMBINING GRAPHEME JOINER */
#define SUCF_CP_MAXCCC          0xFEUL     /* largest legal CCC (254)  */

/* ── Stream-safe combining window bound (UAX #15 convention) ────────
 * The engine guarantees correct reordering across at most this many
 * non-starters following a starter; the window buffer holds a little
 * more to flush the earliest pending marks when the bound is exceeded. */
#define SUCF_MAX_NONSTARTERS    30
#define SUCF_WINDOW             32

/* Depth of the pending-invariant FIFO held between the current starter
 * and its combining marks (extra slack beyond SUCF_WINDOW). */
#define SUCF_INVAR_DEPTH        16

/* ── Canonical form target ───────────────────────────────────────── */
typedef enum {
    SUAS_SUCF_FORM_D = 0, /* canonical decomposition (analysis / stripping) */
    SUAS_SUCF_FORM_C = 1  /* canonical composition (storage / equality)     */
} suas_sucf_form_t;

/* ── Status codes ────────────────────────────────────────────────── */
typedef enum {
    SUAS_SUCF_OK                  =  0,
    SUAS_SUCF_ERR_INVALID_ARG     = -1,
    SUAS_SUCF_ERR_BUFFER_TOO_SMALL = -2,
    SUAS_SUCF_ERR_NULL_POINTER    = -3,
    SUAS_SUCF_ERR_OVERFLOW        = -4
} suas_sucf_status_t;

/* ── Quick check (fast-path hint; never changes the result) ───────── */
typedef enum {
    SUAS_SUCF_QC_YES  = 0, /* already in the target form (skip)  */
    SUAS_SUCF_QC_NO   = 1, /* requires transformation            */
    SUAS_SUCF_QC_MAYBE = 2 /* needs the full routine to confirm  */
} suas_sucf_quick_t;

/* ── Per-instance tailoring override entry (sorted range list) ────────
 * Consulted before the normative tables. A single codepoint is written as
 * { cp, cp, ... }. The list MUST be sorted ascending by lo and free of
 * overlaps. When count == 0 (default), the normative model applies. */
typedef struct {
    sucs_ex_char_t lo;
    sucs_ex_char_t hi;
    int            ccc;      /* override CCC (-1 = use normative)          */
    bool           compose;  /* override composition exclusion (-1 never)  */
    bool           have_ccc; /* when false, ccc is ignored                 */
    bool           have_compose;
} suas_sucf_override_t;

typedef struct {
    const suas_sucf_override_t* overrides;
    size_t                      count;
} suas_sucf_options_t;

/* ── Sliding-window canonical reordering state (streaming engine) ──── */
typedef struct {
    suas_sucf_form_t form;
    int              pending;            /* marks currently buffered        */
    sucs_ex_char_t   starter;            /* current starter (or sentinel)  */
    sucs_ex_char_t   win[SUCF_WINDOW];   /* stack-allocated sliding window */
    int              wccc[SUCF_WINDOW];  /* parallel CCC for the window    */
    sucs_ex_char_t   inv[SUCF_INVAR_DEPTH]; /* pending SCP/native markers  */
    int              ninv;               /* invariant FIFO fill            */
    bool             seeded;
} suas_sucf_state_t;

/* ── Setup ────────────────────────────────────────────────────────── */
const char* suas_sucf_version_string(void);
void        suas_sucf_options_default(suas_sucf_options_t* o);

/* ── Properties (§5) — work for any 64-bit codepoint ──────────────── */
/* Combining Canonical Class of a codepoint (0..254; 0 = starter). */
int suas_sucf_ccc(sucs_ex_char_t cp, const suas_sucf_options_t* o);

/* True when the codepoint is a starter (canonically, a CCC-0 base glyph). */
bool suas_sucf_is_starter(sucs_ex_char_t cp, const suas_sucf_options_t* o);

/* True when the codepoint is canonically invariant: an SCP instruction,
 * BANcode, trap marker, sentinel, or a native/plugin codepoint with no
 * combining properties. Invariant codepoints pass through unchanged. */
bool suas_sucf_is_invariant(sucs_ex_char_t cp);

/* True when cp is an SCP / BANcode / trap-marker control codepoint. */
bool suas_sucf_is_scp(sucs_ex_char_t cp);

/* Whether a codepoint is within the algorithmic Hangul syllable block. */
bool suas_sucf_is_hangul(sucs_ex_char_t cp);

/* ── Canonical decomposition query ───────────────────────────────────
 * Decompose a single codepoint canonically, writing at most cap codepoints
 * to out. Returns the number of codepoints written (1 = already primitive).
 * Hangul decomposes algorithmically; Bridge codepoints via the SUCD table;
 * invariant codepoints decompose to themselves. */
size_t suas_sucf_decompose_one(sucs_ex_char_t cp, const suas_sucf_options_t* o,
                               sucs_ex_char_t* out, size_t cap);

/* ── Quick check ─────────────────────────────────────────────────────
 * Returns SUAS_SUCF_QC_YES/NO/MAYBE for the given form. */
suas_sucf_quick_t suas_sucf_quick_check(const sucs_ex_char_t* cps, size_t n,
                                        suas_sucf_form_t form,
                                        const suas_sucf_options_t* o);

/* ── Streaming engine ────────────────────────────────────────────────
 * Process codepoints in forward order; written output is appended to out
 * (cap = capacity in codepoints) with out_count accrued. Call
 * suas_sucf_flush() at end-of-stream to drain the sliding window. */
suas_sucf_status_t suas_sucf_state_init(suas_sucf_state_t* st,
                                        suas_sucf_form_t form,
                                        const suas_sucf_options_t* o);

suas_sucf_status_t suas_sucf_process_codepoint(suas_sucf_state_t* st,
                                               sucs_ex_char_t cp,
                                               const suas_sucf_options_t* o,
                                               sucs_ex_char_t* out,
                                               size_t cap, size_t* out_count);

suas_sucf_status_t suas_sucf_flush(suas_sucf_state_t* st,
                                   sucs_ex_char_t* out,
                                   size_t cap, size_t* out_count);

/* ── Bulk transform ──────────────────────────────────────────────────
 * Canonical-form a whole array in one call. out must hold at least the
 * size of in (decomposition never shrinks under the stream-safe bound;
 * see note in suas_sucf.c). Returns the number of codepoints written. */
suas_sucf_status_t suas_sucf_transform(const sucs_ex_char_t* in, size_t n,
                                       suas_sucf_form_t form,
                                       const suas_sucf_options_t* o,
                                       sucs_ex_char_t* out, size_t cap,
                                       size_t* out_count);

#ifdef __cplusplus
}
#endif

#endif /* SUAS_SUAS_SUCF_H */
