/*
 * sust16.h - SUST-16 Serialization Transport
 *
 * Self-contained freestanding compatibility layer. Mirrors the canonical
 * superunicode/sust/include/sust16.h semantics exactly so the identical
 * byte-serialization rules can be followed without linking libsust.a.
 *
 * SUST-16 is the canonical BYTE serialization of the SUTF-16 word
 * transformation (see <sutf16.h>). SUTF-16 defines the codepoint <->
 * 16-bit word sequence mapping (endian-neutral, native word order); SUST-16
 * defines how those words are physically packed onto a byte medium — files,
 * sockets, buses, console dumps.
 *
 * Byte serialization:
 * Every 16-bit framing word is written in an EXPLICITLY SELECTED byte order.
 * Two orderings are provided — BIG-ENDIAN (canonical, network order) and
 * LITTLE-ENDIAN:
 *   sust16_encode_bytes()/sust16_decode_bytes()         -> BIG-ENDIAN  (canonical)
 *   sust16_encode_bytes_be()/sust16_decode_bytes_be()   -> BIG-ENDIAN (explicit)
 *   sust16_encode_bytes_le()/sust16_decode_bytes_le()   -> LITTLE-ENDIAN
 *
 * There is deliberately NO byte order mark: every word >= 0x8000 is a
 * framing marker, so no signature word can exist. The byte order is a
 * transport attribute fixed per stream by the sender, not self-describing.
 * Hand-rolled packing is the one way to corrupt a SUTF-16 stream silently —
 * a stream decoded with the wrong order either fails loudly (marker bit
 * flips) or silently decodes a different valid codepoint. Always pair an
 * encode_* with its matching decode_* order.
 *
 * Framing (word-space only, same as the SUTF-16 transformation):
 * - 1-word form: 0x0000 - 0x7FFF (literal value; bit 15 is clear).
 * - 2-word form: word0 = 0x8000 | ((cp >> 16) & 0x7FFF), word1 = cp & 0xFFFF.
 *   A word with bit 15 set is ALWAYS a marker and never a literal.
 *
 * C99 freestanding - stdint.h / stdbool.h / stddef.h only, zero allocation.
 */

#ifndef OWE_SUST16_H
#define OWE_SUST16_H

#include "sucs_types.h"

/* Inline helper for SUST-16 serialized byte length (2 or 4). */
static inline size_t sust16_codepoint_bytes(sucs_char_t cp)
{
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    if (cp <= 0x7FFFUL) {
        return 2;
    } else {
        return 4;
    }
}

/* Encodes a SUCS codepoint into a canonical BIG-ENDIAN SUST-16 byte stream
 * (default / network order). Returns bytes written (2 or 4), or 0 on error.
 * Equivalent to sust16_encode_bytes_be(). */
static inline size_t sust16_encode_bytes(
    sucs_char_t cp,
    uint8_t    *out_bytes,
    size_t      buf_bytes);

/* Decodes one codepoint from a canonical BIG-ENDIAN SUST-16 byte stream.
 * Returns bytes read (2 or 4), or 0 on error (out_cp set to
 * SUCS_INVALID_CODEPOINT). Equivalent to sust16_decode_bytes_be().
 * A LITTLE-ENDIAN stream fails loudly here: swapped bytes flip the marker
 * bit, so misframed data is rejected instead of silently decoding wrong
 * values. */
static inline size_t sust16_decode_bytes(
    const uint8_t *in_bytes,
    size_t         buf_bytes,
    sucs_char_t   *out_cp);

/* Encodes a SUCS codepoint into an explicitly BIG-ENDIAN SUST-16 byte
 * stream (high byte of each word first). Returns bytes written (2 or 4),
 * or 0 on error. */
static inline size_t sust16_encode_bytes_be(
    sucs_char_t cp,
    uint8_t    *out_bytes,
    size_t      buf_bytes)
{
    if (out_bytes == NULL || !sucs_is_valid(cp)) {
        return 0;
    }

    if (cp <= 0x7FFFUL) {
        if (buf_bytes < 2) {
            return 0;
        }
        out_bytes[0] = (uint8_t)((cp >> 8) & 0x7FUL);
        out_bytes[1] = (uint8_t)(cp & 0xFFUL);
        return 2;
    }

    if (buf_bytes < 4) {
        return 0;
    }
    uint16_t w0 = (uint16_t)(0x8000U | ((cp >> 16) & 0x7FFFUL));
    uint16_t w1 = (uint16_t)(cp & 0xFFFFUL);
    out_bytes[0] = (uint8_t)((w0 >> 8) & 0xFFU);
    out_bytes[1] = (uint8_t)(w0 & 0xFFU);
    out_bytes[2] = (uint8_t)((w1 >> 8) & 0xFFU);
    out_bytes[3] = (uint8_t)(w1 & 0xFFU);
    return 4;
}

/* Decodes one codepoint from an explicitly BIG-ENDIAN SUST-16 byte stream.
 * Returns bytes read (2 or 4), or 0 on error (out_cp set to
 * SUCS_INVALID_CODEPOINT). Rejects truncated marker words and overlong
 * 2-word serializations. */
static inline size_t sust16_decode_bytes_be(
    const uint8_t *in_bytes,
    size_t         buf_bytes,
    sucs_char_t   *out_cp)
{
    if (out_cp == NULL) {
        return 0;
    }
    *out_cp = SUCS_INVALID_CODEPOINT;

    if (in_bytes == NULL || buf_bytes < 2) {
        return 0;
    }

    uint16_t w0 = (uint16_t)(((uint16_t)in_bytes[0] << 8) | (uint16_t)in_bytes[1]);

    if (w0 & 0x8000U) {
        /* Bit 15 set on the first word is the 2-word marker; a lone marker
         * word indicates a truncated 2-word serialization. */
        if (buf_bytes < 4) {
            return 0;
        }
        uint16_t w1 = (uint16_t)(((uint16_t)in_bytes[2] << 8) | (uint16_t)in_bytes[3]);
        sucs_char_t cp = (((sucs_char_t)(w0 & 0x7FFFU)) << 16) |
                         (sucs_char_t)w1;
        /* Overlong: values that fit in the 1-word form (<= 0x7FFF) must not
         * be carried in the 2-word form. */
        if (cp <= 0x7FFFUL) {
            return 0;
        }
        if (!sucs_is_valid(cp)) {
            return 0;
        }
        *out_cp = cp;
        return 4;
    }

    sucs_char_t cp = (sucs_char_t)w0;
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    *out_cp = cp;
    return 2;
}

/* Canonical (big-endian) wrappers — defined after the explicit _be variants
 * so the static-inline calls below resolve. */
static inline size_t sust16_encode_bytes(
    sucs_char_t cp,
    uint8_t    *out_bytes,
    size_t      buf_bytes)
{
    return sust16_encode_bytes_be(cp, out_bytes, buf_bytes);
}

static inline size_t sust16_decode_bytes(
    const uint8_t *in_bytes,
    size_t         buf_bytes,
    sucs_char_t   *out_cp)
{
    return sust16_decode_bytes_be(in_bytes, buf_bytes, out_cp);
}

/* Encodes a SUCS codepoint into a LITTLE-ENDIAN SUST-16 byte stream (low
 * byte of each word first). Returns bytes written (2 or 4), or 0 on error.
 * The marker framing is byte-order-independent: word0 still carries bit 15. */
static inline size_t sust16_encode_bytes_le(
    sucs_char_t cp,
    uint8_t    *out_bytes,
    size_t      buf_bytes)
{
    if (out_bytes == NULL || !sucs_is_valid(cp)) {
        return 0;
    }

    if (cp <= 0x7FFFUL) {
        if (buf_bytes < 2) {
            return 0;
        }
        out_bytes[0] = (uint8_t)(cp & 0xFFUL);
        out_bytes[1] = (uint8_t)((cp >> 8) & 0x7FUL);
        return 2;
    }

    if (buf_bytes < 4) {
        return 0;
    }
    uint16_t w0 = (uint16_t)(0x8000U | ((cp >> 16) & 0x7FFFUL));
    uint16_t w1 = (uint16_t)(cp & 0xFFFFUL);
    out_bytes[0] = (uint8_t)(w0 & 0xFFU);
    out_bytes[1] = (uint8_t)((w0 >> 8) & 0xFFU);
    out_bytes[2] = (uint8_t)(w1 & 0xFFU);
    out_bytes[3] = (uint8_t)((w1 >> 8) & 0xFFU);
    return 4;
}

/* Decodes one codepoint from a LITTLE-ENDIAN SUST-16 byte stream.
 * Returns bytes read (2 or 4), or 0 on error (out_cp set to
 * SUCS_INVALID_CODEPOINT). A BIG-ENDIAN stream fails loudly here (marker
 * bit flips). */
static inline size_t sust16_decode_bytes_le(
    const uint8_t *in_bytes,
    size_t         buf_bytes,
    sucs_char_t   *out_cp)
{
    if (out_cp == NULL) {
        return 0;
    }
    *out_cp = SUCS_INVALID_CODEPOINT;

    if (in_bytes == NULL || buf_bytes < 2) {
        return 0;
    }

    uint16_t w0 = (uint16_t)((uint16_t)in_bytes[0] | ((uint16_t)in_bytes[1] << 8));

    if (w0 & 0x8000U) {
        if (buf_bytes < 4) {
            return 0;
        }
        uint16_t w1 = (uint16_t)((uint16_t)in_bytes[2] | ((uint16_t)in_bytes[3] << 8));
        sucs_char_t cp = (((sucs_char_t)(w0 & 0x7FFFU)) << 16) |
                         (sucs_char_t)w1;
        if (cp <= 0x7FFFUL) {
            return 0;
        }
        if (!sucs_is_valid(cp)) {
            return 0;
        }
        *out_cp = cp;
        return 4;
    }

    sucs_char_t cp = (sucs_char_t)w0;
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    *out_cp = cp;
    return 2;
}

#endif /* OWE_SUST16_H */