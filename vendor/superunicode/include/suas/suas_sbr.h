#ifndef SUAS_SUAS_SBR_H
#define SUAS_SUAS_SBR_H

/* SUAS-003 — System Boundary & Line Break Rules (SBR)
 *
 * Core architecture for line breaking over the full 64-bit SUCS / ExtSUCS
 * codepoint space. A single-pass deterministic state transition table
 * (UAX #14 analog) resolves adjacent codepoint pairs to a break status in
 * constant time. Zero allocation, freestanding C99.
 *
 * See docs/suas/SUAS-003-sbr.md for the normative specification.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ──────────────────────────────────────────────────────── */
#define SUAS_SBR_VERSION_MAJOR 0
#define SUAS_SBR_VERSION_MINOR 1
#define SUAS_SBR_VERSION_PATCH 0

/* ── 64-bit ExtSUCS codepoint type (guarded to coexist) ───────────── */
#ifndef EXTSUCS_CHAR_T_DEFINED
#define EXTSUCS_CHAR_T_DEFINED
typedef uint64_t sucs_ex_char_t;
#endif

/* ── Base SUCS zone boundaries (guarded to coexist) ───────────────── */
#ifndef SBR_ZONE_UNICODE_MAX
#define SBR_ZONE_UNICODE_MAX  ((sucs_ex_char_t)0x0010FFFFULL)
#define SBR_ZONE_SCP_MIN      ((sucs_ex_char_t)0x00110000ULL)
#define SBR_ZONE_SCP_MAX      ((sucs_ex_char_t)0x0011FFFFULL)
#define SBR_ZONE_NATIVE_MIN   ((sucs_ex_char_t)0x00120000ULL)
#define SBR_BASE_SUCS_MAX     ((sucs_ex_char_t)0x7FFFFFFFULL)
#define SBR_BASE_TRAP_MIN     ((sucs_ex_char_t)0x7FFFFFF0ULL)
#define SBR_BASE_TRAP_MAX     ((sucs_ex_char_t)0x7FFFFFFEULL)
#define SBR_BASE_SENTINEL     ((sucs_ex_char_t)0x7FFFFFFFULL)
#endif

/* ── Explicit SCP Break Markers (System Control Plane) ────────────── */
#define SUAS_SBR_BREAK_BLOCK  0x00110020UL

#define SCP_BRK_MANDATORY     (SUAS_SBR_BREAK_BLOCK + 0x00UL) /* 0x00110020 */
#define SCP_BRK_PROHIBITED    (SUAS_SBR_BREAK_BLOCK + 0x01UL) /* 0x00110021 */
#define SCP_BRK_OPPORTUNISTIC (SUAS_SBR_BREAK_BLOCK + 0x02UL) /* 0x00110022 */

/* Invisible formatting controls (Unicode Bridge, UAX #14). */
#define SUAS_SBR_CP_ZWSP 0x200BUL /* ZERO WIDTH SPACE */
#define SUAS_SBR_CP_WJ   0x2060UL /* WORD JOINER */

/* ── Break status classification ──────────────────────────────────── */
/* Break outcomes are non-negative; error codes are negative and share this
 * enum (mirroring the SUAS-001 SDF status enum pattern). */
typedef enum {
    SUAS_BRK_MUST_BREAK     =  0, /* mandatory line boundary            */
    SUAS_BRK_CAN_BREAK      =  1, /* allowed / opportunistic boundary   */
    SUAS_BRK_NO_BREAK       =  2, /* prohibited boundary                */
    SUAS_BRK_ALPHANUM_BREAK =  3, /* mandatory CJK/numeric alphab. break */
    SUAS_SBR_ERR_INVALID_ARG  = -1,
    SUAS_SBR_ERR_NULL_POINTER = -2,
    SUAS_SBR_ERR_UNSEEDED     = -3
} suas_sbr_status_t;

/* ── Break classes (UAX #14 set; Bridge) ──────────────────────────── */
typedef enum {
    SUAS_SBR_CLS_BK  = 0,  /* mandatory break            */
    SUAS_SBR_CLS_CR  = 1,  /* carriage return            */
    SUAS_SBR_CLS_LF  = 2,  /* line feed                  */
    SUAS_SBR_CLS_CM  = 3,  /* combining mark             */
    SUAS_SBR_CLS_NL  = 4,  /* next line                  */
    SUAS_SBR_CLS_SG  = 5,  /* surrogate                  */
    SUAS_SBR_CLS_WJ  = 6,  /* word joiner                */
    SUAS_SBR_CLS_ZW  = 7,  /* zero width space           */
    SUAS_SBR_CLS_GL  = 8,  /* non-breaking glue          */
    SUAS_SBR_CLS_SP  = 9,  /* space                      */
    SUAS_SBR_CLS_ZWJ = 10, /* zero width joiner          */
    SUAS_SBR_CLS_B2  = 11, /* break opportunity before & after */
    SUAS_SBR_CLS_BA  = 12, /* break opportunity after    */
    SUAS_SBR_CLS_BB  = 13, /* break opportunity before   */
    SUAS_SBR_CLS_HY  = 14, /* hyphen                     */
    SUAS_SBR_CLS_CB  = 15, /* contingent break           */
    SUAS_SBR_CLS_CL  = 16, /* close punctuation          */
    SUAS_SBR_CLS_CP  = 17, /* close parenthesis          */
    SUAS_SBR_CLS_EX  = 18, /* exclamation/interrogation  */
    SUAS_SBR_CLS_IN  = 19, /* inseparable                */
    SUAS_SBR_CLS_IS  = 20, /* infix separator            */
    SUAS_SBR_CLS_NU  = 21, /* numeric                    */
    SUAS_SBR_CLS_OP  = 22, /* open punctuation           */
    SUAS_SBR_CLS_PO  = 23, /* postfix numeric            */
    SUAS_SBR_CLS_PR  = 24, /* prefix numeric             */
    SUAS_SBR_CLS_QU  = 25, /* quotation                  */
    SUAS_SBR_CLS_SA  = 26, /* complex context            */
    SUAS_SBR_CLS_AL  = 27, /* alphabetic                 */
    SUAS_SBR_CLS_ID  = 28, /* ideographic                */
    SUAS_SBR_CLS_EB  = 29, /* emoji base                 */
    SUAS_SBR_CLS_EM  = 30, /* emoji modifier             */
    SUAS_SBR_CLS_H2  = 31, /* Hangul LV syllable         */
    SUAS_SBR_CLS_H3  = 32, /* Hangul LVT syllable        */
    SUAS_SBR_CLS_HL  = 33, /* Hebrew letter              */
    SUAS_SBR_CLS_RI  = 34, /* regional indicator         */
    SUAS_SBR_CLS_JL  = 35, /* Hangul L jamo             */
    SUAS_SBR_CLS_JV  = 36, /* Hangul V jamo             */
    SUAS_SBR_CLS_JT  = 37, /* Hangul T jamo             */
    SUAS_SBR_CLS_XX  = 38, /* unknown / default          */
    SUAS_SBR_CLS_COUNT = 39
} suas_sbr_break_class_t;

/* ── Per-instance tailoring override (sorted range list) ──────────── */
/* Consulted before the normative table. A single codepoint is written as
 * { cp, cp, cls }. The list MUST be sorted ascending by lo and free of
 * overlaps. When count == 0 (default), the normative classification
 * applies. */
typedef struct {
    sucs_ex_char_t        lo;
    sucs_ex_char_t        hi;
    suas_sbr_break_class_t cls;
} suas_sbr_override_t;

typedef struct {
    const suas_sbr_override_t* overrides;
    size_t                     count;
} suas_sbr_options_t;

/* ── Single-pass engine state ─────────────────────────────────────── */
typedef struct {
    suas_sbr_break_class_t prev; /* previous break class   */
    int                    seeded; /* 1 once prev is valid   */
} suas_sbr_state_t;

/* ── Setup ────────────────────────────────────────────────────────── */
const char* suas_sbr_version_string(void);
void        suas_sbr_options_default(suas_sbr_options_t* o);
void        suas_sbr_state_init(suas_sbr_state_t* st);

/* ── Classification (§5) — any 64-bit codepoint ───────────────────── */
/* Resolve a codepoint to its break class per §5.4 zone dispatch. */
suas_sbr_break_class_t suas_sbr_classify(sucs_ex_char_t cp,
                                         const suas_sbr_options_t* o);

/* Native SUCS / plugin "word" test — O(1) high-bit range bitmask. */
bool suas_sbr_is_native_word(sucs_ex_char_t cp);

/* ── Pair resolution (§5.2) — the O(1) transition-matrix contract ── */
/* Stateless: resolves exactly one adjacent (prev, cur) pair. */
suas_sbr_status_t suas_sbr_pair(sucs_ex_char_t prev, sucs_ex_char_t cur,
                                const suas_sbr_options_t* o);

/* ── Single-pass stream engine ────────────────────────────────────── */
/* Advance the state by one codepoint; returns the break status of the
 * boundary just consumed (i.e. between the previous and current cp).
 * The first codepoint seeds the engine and yields SUAS_BRK_MUST_BREAK. */
suas_sbr_status_t suas_sbr_process_codepoint(suas_sbr_state_t* st,
                                             sucs_ex_char_t cp,
                                             const suas_sbr_options_t* o);

#ifdef __cplusplus
}
#endif

#endif /* SUAS_SUAS_SBR_H */
