#ifndef SUST_FIXED_H
#define SUST_FIXED_H

/**
 * SUST Fixed-Width Serialization Transports
 *
 * These are strictly SERIALIZATION TRANSPORTS defining fixed-width
 * byte-packing and memory layout rules for storing and transmitting
 * SUCS and ExtSUCS codepoints in aligned vector register slots.
 *
 * SUST-32:  4-byte  (32-bit)  alignment — Base SUCS fast-path container
 * SUST-64:  8-byte  (64-bit)  alignment — SIMD / AI tensor slot
 * SUST-128: 16-byte (128-bit) alignment — SSE / NEON vector register slot
 * SUST-256: 32-byte (256-bit) alignment — AVX-256 vector register slot
 * SUST-512: 64-byte (512-bit) alignment — AVX-512 vector register slot
 * SUST-N:   N-word  arbitrary  alignment — Multi-word block container
 *
 * All formats use big-endian byte order with zero-padding in upper bytes.
 * Error handling is strictly out-of-band via return values.
 */

#include "extsucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * SUST-32 Fixed-Width Serialization Transport (4 bytes)
 *
 * Serializes ExtSUCS codepoints into 4-byte big-endian containers.
 * Can only represent codepoints 0x00000000 to 0xFFFFFFFF (32-bit range).
 * Returns false / 0 if the codepoint exceeds the 32-bit addressable range.
 * ============================================================================ */
#define SUST32_BYTES 4

size_t sust32_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size);
size_t sust32_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp);

/* ============================================================================
 * SUST-64 Fixed-Width Serialization Transport (8 bytes)
 *
 * Serializes ExtSUCS codepoints into 8-byte big-endian containers for
 * SIMD / AI tensor alignment and 64-bit bus-width transports.
 * Full 64-bit ExtSUCS range supported.
 * ============================================================================ */
#define SUST64_BYTES 8

size_t sust64_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size);
size_t sust64_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp);

/* ============================================================================
 * SUST-128 Fixed-Width Serialization Transport (16 bytes)
 *
 * Serializes ExtSUCS codepoints into 16-byte zero-padded, big-endian aligned
 * vector register slots (SSE / NEON width).
 * ============================================================================ */
#define SUST128_BYTES 16

size_t sust128_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size);
size_t sust128_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp);

/* ============================================================================
 * SUST-256 Fixed-Width Serialization Transport (32 bytes)
 *
 * Serializes ExtSUCS codepoints into 32-byte zero-padded, big-endian aligned
 * vector register slots (AVX-256 width).
 * ============================================================================ */
#define SUST256_BYTES 32

size_t sust256_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size);
size_t sust256_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp);

/* ============================================================================
 * SUST-512 Fixed-Width Serialization Transport (64 bytes)
 *
 * Serializes ExtSUCS codepoints into 64-byte zero-padded, big-endian aligned
 * vector register slots (AVX-512 width).
 * ============================================================================ */
#define SUST512_BYTES 64

size_t sust512_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size);
size_t sust512_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp);

/* ============================================================================
 * SUST-N Arbitrary Fixed-Width Serialization Transport
 *
 * Serializes an ExtSUCS codepoint into an N-byte zero-padded, big-endian
 * aligned multi-word block container. The caller specifies slot_bytes.
 * Minimum slot_bytes is 8 (for full 64-bit coverage).
 * ============================================================================ */
size_t sustn_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t slot_bytes);
size_t sustn_decode(const uint8_t* in_buf, size_t slot_bytes, sucs_ex_char_t* out_cp);

#ifdef __cplusplus
}
#endif

#endif /* SUST_FIXED_H */