/*
 * sutf16.h - SUTF-16 Transformation Format
 *
 * Self-contained freestanding compatibility layer. Mirrors the canonical
 * superunicode/sutf/include/sutf16.h semantics exactly (same framing,
 * same error behavior) so the identical transformation rules can be
 * followed without linking libsutf.a.
 *
 * SUTF-16 defines the codepoint <-> 16-bit word sequence transformation:
 * SUCS codepoints are mapped to 1 or 2 16-bit words. This is strictly a
 * TRANSFORMATION FORMAT — it encodes/decodes values, but says nothing about
 * how the words are physically packed onto a byte medium. That physical
 * layout is the job of the SUST-16 SERIALIZATION TRANSPORT (see <sust16.h>),
 * which provides explicit big-endian (canonical) and little-endian byte
 * order variants.
 *
 * NO SURROGATES: SUCS has no surrogate concept. 0xD800-0xDFFF are ordinary
 * valid PUA codepoints — never surrogate halves and never combined with a
 * following word. They are encoded in the 2-word form like any value above
 * 0x7FFF.
 *
 * Framing (unambiguous by construction, word-space only):
 * - 1-word form: 0x0000 - 0x7FFF  (literal value; bit 15 is clear).
 * - 2-word form: 0x8000 - 0x7FFFFFFF. The first word has bit 15 SET as the
 *   2-word marker and carries the high 15 bits; the second word carries the
 *   low 16 bits:
 *       word0 = 0x8000 | ((cp >> 16) & 0x7FFF)
 *       word1 = cp & 0xFFFF
 *   A word with bit 15 set is ALWAYS a marker and never a literal, so a
 *   stream such as {0x8000, 0xD800} is unambiguously one codepoint
 *   (0xD800), and a lone marker word is a detectable truncation error.
 *
 * C99 freestanding - stdint.h / stdbool.h / stddef.h only, zero allocation.
 */

#ifndef OWE_SUTF16_H
#define OWE_SUTF16_H

#include "sucs_types.h"

/* Inline helper for SUTF-16 transformation word length calculation. */
static inline size_t sutf16_codepoint_length(sucs_char_t cp)
{
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    if (cp <= 0x7FFFUL) {
        return 1;
    } else {
        return 2;
    }
}

/* Encodes a SUCS codepoint into the SUTF-16 16-bit word transformation.
 * Returns 16-bit words written (1 or 2), or 0 on error. Endian-neutral:
 * words are produced in native memory order; use SUST-16 (see <sust16.h>)
 * for byte serialization. */
static inline size_t sutf16_encode_char(
    sucs_char_t  cp,
    uint16_t    *out_words,
    size_t       buf_words)
{
    if (out_words == NULL || !sucs_is_valid(cp)) {
        return 0;
    }

    if (cp <= 0x7FFFUL) {
        if (buf_words < 1) {
            return 0;
        }
        out_words[0] = (uint16_t)(cp & 0x7FFFUL);
        return 1;
    } else {
        if (buf_words < 2) {
            return 0;
        }
        out_words[0] = (uint16_t)(0x8000U | ((cp >> 16) & 0x7FFFUL));
        out_words[1] = (uint16_t)(cp & 0xFFFFUL);
        return 2;
    }
}

/* Decodes one codepoint from the SUTF-16 16-bit word transformation.
 * Returns words read (1 or 2), or 0 on error (out_cp set to
 * SUCS_INVALID_CODEPOINT).
 * Rejects truncated marker words and overlong 2-word encodings. */
static inline size_t sutf16_decode_char(
    const uint16_t *in_words,
    size_t          buf_words,
    sucs_char_t    *out_cp)
{
    if (out_cp == NULL) {
        return 0;
    }
    *out_cp = SUCS_INVALID_CODEPOINT;

    if (in_words == NULL || buf_words == 0) {
        return 0;
    }

    if ((in_words[0] & 0x8000U) != 0) {
        /* Bit 15 is the 2-word marker and never a literal; a lone marker
         * word is a truncated 2-word sequence. */
        if (buf_words < 2) {
            return 0;
        }
        sucs_char_t cp = (((sucs_char_t)(in_words[0] & 0x7FFFU)) << 16) |
                         (sucs_char_t)in_words[1];
        /* Overlong: values that fit in the 1-word form (<= 0x7FFF) must not
         * be carried in the 2-word form. */
        if (cp <= 0x7FFFUL) {
            return 0;
        }
        if (!sucs_is_valid(cp)) {
            return 0;
        }
        *out_cp = cp;
        return 2;
    }

    sucs_char_t cp = (sucs_char_t)in_words[0];
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    *out_cp = cp;
    return 1;
}

#endif /* OWE_SUTF16_H */