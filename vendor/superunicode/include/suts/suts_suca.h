#ifndef SUTS_SUTS_SUCA_H
#define SUTS_SUTS_SUCA_H

/* SUTS-001 — SuperUnicode Collation Algorithm (SUCA).
 *
 * A full UCA-equivalent multilevel collation engine, freestanding C99 with
 * zero dynamic allocation. Input codepoints are 64-bit ExtSUCS codepoints
 * (sucs_ex_char_t), so the same engine orders Base SUCS (31-bit) and the
 * unbounded ExtSUCS plugin space. Callers provide all output buffers.
 *
 * Reference implementation of docs/suts/SUTS-001-suca.md.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ──────────────────────────────────────────────────────── */
#define SUTS_SUCA_VERSION_MAJOR 0
#define SUTS_SUCA_VERSION_MINOR 1
#define SUTS_SUCA_VERSION_PATCH 0

/* ── 64-bit ExtSUCS codepoint type (guarded to coexist) ───────────── */
#ifndef EXTSUCS_CHAR_T_DEFINED
#define EXTSUCS_CHAR_T_DEFINED
typedef uint64_t sucs_ex_char_t;
#endif

/* ── Base SUCS zone boundaries (duplicated for freestanding SUCA) ─── */
#define SUCA_ZONE_UNICODE_MAX  ((sucs_ex_char_t)0x0010FFFFULL)
#define SUCA_ZONE_SCP_MIN      ((sucs_ex_char_t)0x00110000ULL)
#define SUCA_ZONE_SCP_MAX      ((sucs_ex_char_t)0x0011FFFFULL)
#define SUCA_ZONE_NATIVE_MIN   ((sucs_ex_char_t)0x00120000ULL)
#define SUCA_BASE_SUCS_MAX     ((sucs_ex_char_t)0x7FFFFFFFULL)
#define SUCA_BASE_TRAP_MIN     ((sucs_ex_char_t)0x7FFFFFF0ULL)
#define SUCA_BASE_TRAP_MAX     ((sucs_ex_char_t)0x7FFFFFFEULL)

/* ── Weight / level types ─────────────────────────────────────────── */
typedef uint32_t suts_suca_weight_t;

#define SUTS_SUCA_MAX_LEVELS 4

typedef enum {
    SUTS_SUCA_LEVEL_PRIMARY    = 1, /* L1 base characters */
    SUTS_SUCA_LEVEL_SECONDARY  = 2, /* L2 accents */
    SUTS_SUCA_LEVEL_TERTIARY   = 3, /* L3 case / variants */
    SUTS_SUCA_LEVEL_QUATERNARY = 4, /* L4 punctuation / variable L4 */
    SUTS_SUCA_LEVEL_IDENTICAL  = 5  /* Ln tie-break (NFD + native order) */
} suts_suca_level_t;

typedef enum {
    SUTS_SUCA_STRENGTH_PRIMARY     = 1,
    SUTS_SUCA_STRENGTH_SECONDARY   = 2,
    SUTS_SUCA_STRENGTH_TERTIARY    = 3, /* default */
    SUTS_SUCA_STRENGTH_QUATERNARY  = 4,
    SUTS_SUCA_STRENGTH_IDENTICAL   = 5
} suts_suca_strength_t;

typedef enum {
    SUTS_SUCA_VAR_NON_IGNORABLE = 0, /* variable CEs unchanged */
    SUTS_SUCA_VAR_SHIFTED       = 1, /* default */
    SUTS_SUCA_VAR_BLANKED       = 2,
    SUTS_SUCA_VAR_SHIFT_TRIMMED = 3
} suts_suca_variable_t;

typedef enum {
    SUTS_SUCA_CASE_LOWER_FIRST = 0,
    SUTS_SUCA_CASE_UPPER_FIRST = 1
} suts_suca_case_order_t;

/* ── Collation element (1..4 weights + flags) ─────────────────────── */
typedef struct {
    suts_suca_weight_t l1;
    suts_suca_weight_t l2;
    suts_suca_weight_t l3;
    suts_suca_weight_t l4;
    uint8_t levels;    /* number of populated levels (1..4) */
    uint8_t variable;  /* 1 if this is a variable collation element */
} suts_suca_ce_t;

/* ── Sort key (caller-owned weight buffer) ────────────────────────── */
typedef struct {
    suts_suca_weight_t* data;  /* caller buffer */
    size_t              capacity;
    size_t              length;
} suts_suca_key_t;

/* ── Parametric options (tailoring) ───────────────────────────────── */
typedef struct {
    suts_suca_strength_t strength;          /* SUTS_SUCA_STRENGTH_TERTIARY default */
    suts_suca_variable_t variable;          /* SUTS_SUCA_VAR_SHIFTED default */
    bool    backward_secondary;             /* French dictionary order */
    bool    normalization;                  /* NFD (S1.1); default true */
    bool    semi_stable;                    /* append identical level (S3.10); default false */
    suts_suca_case_order_t case_order;      /* ordering within L3 */
    /* Max primary weight treated as variable (interleave boundary). */
    suts_suca_weight_t var_max;             /* 0 => use table default */
} suts_suca_options_t;

/* ── Tailoring rule (programmatic; selected syntax from §8.2) ─────── */
typedef enum {
    SUTS_SUCA_RULE_PRIMARY_GT   = 1, /* & base < x   (L1 greater) */
    SUTS_SUCA_RULE_SECONDARY_GT = 2, /* & base << x  (L2 greater) */
    SUTS_SUCA_RULE_TERTIARY_GT  = 3, /* & base <<< x (L3 greater) */
    SUTS_SUCA_RULE_EQUAL        = 4  /* & base = x   (equal)      */
} suts_suca_rule_kind_t;

/* Sequence (input) and sequence (output) for a tailoring rule. */
#define SUTS_SUCA_MAX_SEQ 8
typedef struct {
    suts_suca_rule_kind_t kind;
    sucs_ex_char_t base[SUTS_SUCA_MAX_SEQ];  size_t base_len;
    sucs_ex_char_t value[SUTS_SUCA_MAX_SEQ]; size_t value_len;
} suts_suca_rule_t;

/* ── Status codes ─────────────────────────────────────────────────── */
typedef enum {
    SUTS_SUCA_OK                    =  0,
    SUTS_SUCA_ERR_INVALID_ARG       = -1,
    SUTS_SUCA_ERR_BUFFER_TOO_SMALL  = -2,
    SUTS_SUCA_ERR_CONTRACTION_BLOCK = -3, /* blocked contraction (CGJ) */
    SUTS_SUCA_ERR_UNSUPPORTED       = -4
} suts_suca_status_t;

/* ── Setup ────────────────────────────────────────────────────────── */
const char* suts_suca_version_string(void);
void        suts_suca_options_default(suts_suca_options_t* o);

/* ── Implicit weight derivation (§9.1) — works for any 64-bit cp ──── */
suts_suca_ce_t suts_suca_implicit_ce(sucs_ex_char_t cp);

/* ── Normalization (S1.1) ─────────────────────────────────────────── */
/* Decompose bridge codepoints to NFD into out (up to outcap). Returns
 * number of codepoints written, or negative status. Hangul decomposed
 * algorithmically; other decomposables via an embedded curated table. */
suts_suca_status_t suts_suca_nfd(sucs_ex_char_t cp,
                                 sucs_ex_char_t* out, size_t outcap, size_t* nout);
uint8_t suts_suca_ccc(sucs_ex_char_t cp);

/* ── Mapping lookup (S2) ──────────────────────────────────────────── */
/* Longest-match over contractions for the sequence at cps[0..n-1].
 * Writes up to ce_cap CEs; returns count. Falls back to implicit. */
suts_suca_status_t suts_suca_lookup(const sucs_ex_char_t* cps, size_t n,
                                    const suts_suca_options_t* o,
                                    suts_suca_ce_t* out, size_t ce_cap, size_t* nout);

/* ── CE array (step 2), sort key (step 3), compare (step 4) ───────── */
suts_suca_status_t suts_suca_ce_array(const sucs_ex_char_t* cps, size_t n,
                                      const suts_suca_options_t* o,
                                      suts_suca_ce_t* out, size_t ce_cap,
                                      size_t* nout);

suts_suca_status_t suts_suca_key(const sucs_ex_char_t* cps, size_t n,
                                 const suts_suca_options_t* o,
                                 suts_suca_key_t* key);

int suts_suca_compare_keys(const suts_suca_key_t* a, const suts_suca_key_t* b);

suts_suca_status_t suts_suca_compare(const sucs_ex_char_t* a, size_t an,
                                     const sucs_ex_char_t* b, size_t bn,
                                     const suts_suca_options_t* o,
                                     int* result);

/* ── Tailoring (parametric + programmatic rule list) ──────────────── */
/* Apply a rule list to the options + an internal tailoring overlay.
 * The overlay mutates the default mappings inside the engine. */
suts_suca_status_t suts_suca_apply_rules(const suts_suca_rule_t* rules,
                                         size_t nrules,
                                         suts_suca_options_t* o);

#ifdef __cplusplus
}
#endif

#endif /* SUTS_SUTS_SUCA_H */
