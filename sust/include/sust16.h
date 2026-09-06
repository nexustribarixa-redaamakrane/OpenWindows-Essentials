#ifndef SUST_SUST16_H
#define SUST_SUST16_H

#include "sucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SUST-16 Serialization Transport
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
 *   sust16_encode_bytes()/sust16_decode_bytes()   -> BIG-ENDIAN  (canonical)
 *   sust16_encode_bytes_be()/sust16_decode_bytes_be() -> BIG-ENDIAN (explicit)
 *   sust16_encode_bytes_le()/sust16_decode_bytes_le() -> LITTLE-ENDIAN
 *
 * There is deliberately NO byte order mark: every word >= 0x8000 is a
 * framing marker, so no signature word can exist. The byte order is a
 * transport attribute fixed per stream by the sender, not self-describing.
 * Hand-rolled packing is the one way to corrupt a SUTF-16 stream silently —
 * a stream decoded with the wrong order either fails loudly (marker bit
 * flips) or silently decodes a different valid codepoint. Always pair an
 * encode_* with its matching decode_* order.
 */

/* Inline helper for SUST-16 serialized byte length (2 or 4) */
static inline size_t sust16_codepoint_bytes(sucs_char_t cp) {
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    if (cp <= 0x7FFFUL) {
        return 2;
    } else {
        return 4;
    }
}

/**
 * Encodes a SUCS codepoint into a canonical BIG-ENDIAN SUST-16 byte stream
 * (default / network order). Returns bytes written (2 or 4), or 0 on error.
 * Equivalent to sust16_encode_bytes_be(). Freestanding-safe: no allocation.
 */
size_t sust16_encode_bytes(sucs_char_t cp, uint8_t* out_bytes, size_t buf_bytes);

/**
 * Decodes one codepoint from a canonical BIG-ENDIAN SUST-16 byte stream.
 * Returns bytes read (2 or 4), or 0 on error (yielding
 * SUCS_INVALID_CODEPOINT). Equivalent to sust16_decode_bytes_be().
 * A LITTLE-ENDIAN stream fails loudly here: swapped bytes flip the marker
 * bit, so misframed data is rejected instead of silently decoding wrong
 * values. Freestanding-safe: no allocation.
 */
size_t sust16_decode_bytes(const uint8_t* in_bytes, size_t buf_bytes, sucs_char_t* out_cp);

/**
 * Encodes a SUCS codepoint into an explicitly BIG-ENDIAN SUST-16 byte
 * stream (high byte of each word first). Returns bytes written (2 or 4),
 * or 0 on error. Freestanding-safe: no allocation.
 */
size_t sust16_encode_bytes_be(sucs_char_t cp, uint8_t* out_bytes, size_t buf_bytes);

/**
 * Decodes one codepoint from an explicitly BIG-ENDIAN SUST-16 byte stream.
 * Returns bytes read (2 or 4), or 0 on error. Freestanding-safe: no
 * allocation.
 */
size_t sust16_decode_bytes_be(const uint8_t* in_bytes, size_t buf_bytes, sucs_char_t* out_cp);

/**
 * Encodes a SUCS codepoint into a LITTLE-ENDIAN SUST-16 byte stream (low
 * byte of each word first). Returns bytes written (2 or 4), or 0 on error.
 * The marker framing is byte-order-independent: word0 still carries bit 15.
 * Freestanding-safe: no allocation.
 */
size_t sust16_encode_bytes_le(sucs_char_t cp, uint8_t* out_bytes, size_t buf_bytes);

/**
 * Decodes one codepoint from a LITTLE-ENDIAN SUST-16 byte stream.
 * Returns bytes read (2 or 4), or 0 on error. A BIG-ENDIAN stream fails
 * loudly here (marker bit flips). Freestanding-safe: no allocation.
 */
size_t sust16_decode_bytes_le(const uint8_t* in_bytes, size_t buf_bytes, sucs_char_t* out_cp);

#ifdef __cplusplus
}
#endif

#endif /* SUST_SUST16_H */