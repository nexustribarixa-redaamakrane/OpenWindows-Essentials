/*
 * sutf8.h - SUTF-8 Transformation Format
 *
 * Self-contained freestanding compatibility layer. Mirrors the canonical
 * superunicode/sutf/include/sutf8.h semantics and byte-packing exactly.
 *
 * SUTF-8 defines the endian-neutral mapping between SUCS codepoints and
 * 1 to 6 byte sequences. Physical byte packing and stream framing rules
 * for serialized streams are defined by the SUST layer (see <sust.h>);
 * this header is strictly the transformation.
 *
 * Pure static-inline, zero external dependencies, zero allocation.
 */

#ifndef OWE_SUTF8_H
#define OWE_SUTF8_H

#include "sucs_types.h"

/* ------------------------------------------------------------------ */
/*  Codepoint Length                                                   */
/* ------------------------------------------------------------------ */

static inline size_t sutf8_codepoint_length(sucs_char_t cp)
{
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    if (cp <= 0x7FUL) {
        return 1;
    } else if (cp <= 0x7FFUL) {
        return 2;
    } else if (cp <= 0xFFFFUL) {
        return 3;
    } else if (cp <= 0x0010FFFFUL) {
        return 4;
    } else if (cp <= 0x03FFFFFFUL) {
        return 5;
    } else {
        return 6;
    }
}

/* ------------------------------------------------------------------ */
/*  Decode                                                             */
/* ------------------------------------------------------------------ */

/*
 * Decodes one SUCS codepoint from a SUTF-8 transport stream.
 * Returns bytes consumed (1-6), or 0 on error (out_cp set to
 * SUCS_INVALID_CODEPOINT). Rejects overlong forms, bad continuation
 * bytes, and codepoints outside the SUCS valid address space.
 * `buf_size` limits how many bytes may be inspected.
 */
static inline size_t sutf8_decode_char(
    const uint8_t *in_buf,
    size_t         buf_size,
    sucs_char_t   *out_cp)
{
    sucs_char_t cp;
    size_t len;

    if (in_buf == NULL || out_cp == NULL) {
        if (out_cp != NULL) *out_cp = SUCS_INVALID_CODEPOINT;
        return 0;
    }
    if (buf_size == 0) {
        *out_cp = SUCS_INVALID_CODEPOINT;
        return 0;
    }

    uint8_t b0 = in_buf[0];
    if (b0 < 0x80u) {
        cp  = (sucs_char_t)b0;
        len = 1;
    } else if ((b0 & 0xE0u) == 0xC0u) {
        cp  = (sucs_char_t)(b0 & 0x1Fu);
        len = 2;
    } else if ((b0 & 0xF0u) == 0xE0u) {
        cp  = (sucs_char_t)(b0 & 0x0Fu);
        len = 3;
    } else if ((b0 & 0xF8u) == 0xF0u) {
        cp  = (sucs_char_t)(b0 & 0x07u);
        len = 4;
    } else if ((b0 & 0xFCu) == 0xF8u) {
        cp  = (sucs_char_t)(b0 & 0x03u);
        len = 5;
    } else if ((b0 & 0xFEu) == 0xFCu) {
        cp  = (sucs_char_t)(b0 & 0x01u);
        len = 6;
    } else {
        *out_cp = SUCS_INVALID_CODEPOINT;
        return 0;
    }

    if (buf_size < len) {
        *out_cp = SUCS_INVALID_CODEPOINT;
        return 0;
    }

    for (size_t i = 1; i < len; i++) {
        uint8_t bc = in_buf[i];
        if ((bc & 0xC0u) != 0x80u) {
            *out_cp = SUCS_INVALID_CODEPOINT;
            return 0;
        }
        cp = (sucs_char_t)((cp << 6) | (sucs_char_t)(bc & 0x3Fu));
    }

    /* Reject overlong encodings. */
    if (len == 2 && cp < 0x80UL)          { *out_cp = SUCS_INVALID_CODEPOINT; return 0; }
    if (len == 3 && cp < 0x800UL)         { *out_cp = SUCS_INVALID_CODEPOINT; return 0; }
    if (len == 4 && cp < 0x10000UL)       { *out_cp = SUCS_INVALID_CODEPOINT; return 0; }
    if (len == 5 && cp < 0x200000UL)      { *out_cp = SUCS_INVALID_CODEPOINT; return 0; }
    if (len == 6 && cp < 0x04000000UL)    { *out_cp = SUCS_INVALID_CODEPOINT; return 0; }

    /* Reject codepoints outside the SUCS address space / trap range. */
    if (cp > SUCS_MAX_CODEPOINT || !sucs_is_valid(cp)) {
        *out_cp = SUCS_INVALID_CODEPOINT;
        return 0;
    }

    *out_cp = cp;
    return len;
}

/* ------------------------------------------------------------------ */
/*  Encode                                                             */
/* ------------------------------------------------------------------ */

/*
 * Encodes one SUCS codepoint into a SUTF-8 transport stream.
 * Returns bytes written (1-6), or 0 on error.
 * `buf_size` limits how many bytes may be written.
 */
static inline size_t sutf8_encode_char(
    sucs_char_t  cp,
    uint8_t     *out_buf,
    size_t       buf_size)
{
    if (out_buf == NULL || !sucs_is_valid(cp)) {
        return 0;
    }

    size_t len = sutf8_codepoint_length(cp);
    if (len == 0 || buf_size < len) {
        return 0;
    }

    /* Encode from the least significant 6-bit group upward. */
    uint8_t groups[6];
    size_t gcount = 0;
    sucs_char_t tmp = cp;
    while (gcount < len) {
        if (gcount == 0) {
            groups[gcount++] = (uint8_t)(tmp & 0x3Fu);
            tmp >>= 6;
        } else if (gcount < len) {
            groups[gcount++] = (uint8_t)((tmp & 0x3Fu) | 0x80u);
            tmp >>= 6;
        }
    }
    /* Re-assemble: multi-byte leading byte owns the high bits. */
    switch (len) {
    case 1:
        out_buf[0] = (uint8_t)cp;
        break;
    case 2:
        out_buf[0] = (uint8_t)(0xC0u | ((cp >> 6) & 0x1Fu));
        out_buf[1] = groups[0];
        break;
    case 3:
        out_buf[0] = (uint8_t)(0xE0u | ((cp >> 12) & 0x0Fu));
        out_buf[1] = groups[2];
        out_buf[2] = groups[0] | 0x80u;
        break;
    case 4:
        out_buf[0] = (uint8_t)(0xF0u | ((cp >> 18) & 0x07u));
        out_buf[1] = groups[3];
        out_buf[2] = groups[2];
        out_buf[3] = groups[0] | 0x80u;
        break;
    case 5:
        out_buf[0] = (uint8_t)(0xF8u | ((cp >> 24) & 0x03u));
        out_buf[1] = groups[4];
        out_buf[2] = groups[3];
        out_buf[3] = groups[2];
        out_buf[4] = groups[0] | 0x80u;
        break;
    default: /* 6 */
        out_buf[0] = (uint8_t)(0xFCu | ((cp >> 30) & 0x01u));
        out_buf[1] = groups[5];
        out_buf[2] = groups[4];
        out_buf[3] = groups[3];
        out_buf[4] = groups[2];
        out_buf[5] = groups[0] | 0x80u;
        break;
    }

    return len;
}

/* ------------------------------------------------------------------ */
/*  Stream Walk Helper                                                 */
/* ------------------------------------------------------------------ */

/*
 * Advances `*pos` past the next codepoint in a NUL-terminated SUTF-8
 * stream, writing the codepoint to `*out_cp`. Returns bytes advanced,
 * or 0 at end-of-stream. Skips a single malformed byte and continues
 * (caller decides via return of 0 only at true end).
 */
static inline size_t sutf8_next_codepoint(
    const uint8_t *stream,
    size_t         remaining,
    sucs_char_t   *out_cp)
{
    if (stream == NULL || out_cp == NULL || remaining == 0 || stream[0] == 0) {
        if (out_cp != NULL) *out_cp = SUCS_INVALID_CODEPOINT;
        return 0;
    }
    size_t adv = sutf8_decode_char(stream, remaining, out_cp);
    if (adv == 0) {
        return 1;  /* skip the malformed byte, keep scanning */
    }
    return adv;
}

#endif /* OWE_SUTF8_H */