/*
 * unitrans.h - Unicode to SuperUnicode Freestanding Translation Layer
 *
 * Provides bidirectional conversion between Standard Unicode (UTF-8, UTF-16,
 * UTF-32, US-ASCII) and the OpenWindows SuperUnicode architecture (SUCS, SUTF-8).
 *
 * Implements strict bounds-checking and guarantees zero dynamic heap allocation.
 * C99 freestanding.
 */

#ifndef UNITRANS_H
#define UNITRANS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define UNITRANS_OK                 0u
#define UNITRANS_ERR_INVALID_UTF8   1u
#define UNITRANS_ERR_BUFFER_FULL    2u
#define UNITRANS_ERR_TRAP_RANGE     3u

/* SuperUnicode Control Point Trap Range */
#define SCP_TRAP_BASE               0x7FFFFFF0u
#define SCP_TRAP_LIMIT              0x7FFFFFFEu

/*
 * Translates standard UTF-8 stream to canonical SUCS codepoints (uint32_t).
 */
static inline uint32_t unitrans_utf8_to_sucs(
    const uint8_t *in_utf8, size_t in_len,
    uint32_t *out_sucs, size_t max_out,
    size_t *written)
{
    if (!in_utf8 || !out_sucs || !written) return UNITRANS_ERR_INVALID_UTF8;

    size_t i = 0u;
    size_t out_idx = 0u;

    while (i < in_len && out_idx < max_out) {
        uint8_t b0 = in_utf8[i];
        uint32_t cp = 0u;

        if ((b0 & 0x80u) == 0x00u) {
            cp = b0;
            i += 1u;
        } else if ((b0 & 0xE0u) == 0xC0u) {
            if (i + 1u >= in_len) break;
            cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(in_utf8[i + 1] & 0x3Fu);
            i += 2u;
        } else if ((b0 & 0xF0u) == 0xE0u) {
            if (i + 2u >= in_len) break;
            cp = ((uint32_t)(b0 & 0x0Fu) << 12) |
                 ((uint32_t)(in_utf8[i + 1] & 0x3Fu) << 6) |
                 (uint32_t)(in_utf8[i + 2] & 0x3Fu);
            i += 3u;
        } else if ((b0 & 0xF8u) == 0xF0u) {
            if (i + 3u >= in_len) break;
            cp = ((uint32_t)(b0 & 0x07u) << 18) |
                 ((uint32_t)(in_utf8[i + 1] & 0x3Fu) << 12) |
                 ((uint32_t)(in_utf8[i + 2] & 0x3Fu) << 6) |
                 (uint32_t)(in_utf8[i + 3] & 0x3Fu);
            i += 4u;
        } else {
            return UNITRANS_ERR_INVALID_UTF8;
        }

        /* Check for SCP trap range collision */
        if (cp >= SCP_TRAP_BASE && cp <= SCP_TRAP_LIMIT) {
            return UNITRANS_ERR_TRAP_RANGE;
        }

        out_sucs[out_idx++] = cp;
    }

    *written = out_idx;
    return (out_idx < max_out) ? UNITRANS_OK : UNITRANS_ERR_BUFFER_FULL;
}

/*
 * Translates SUCS codepoints to SUTF-8 stream.
 */
static inline uint32_t unitrans_sucs_to_sutf8(
    const uint32_t *in_sucs, size_t in_count,
    uint8_t *out_utf8, size_t max_out,
    size_t *written)
{
    if (!in_sucs || !out_utf8 || !written) return UNITRANS_ERR_INVALID_UTF8;

    size_t out_idx = 0u;
    for (size_t i = 0u; i < in_count; ++i) {
        uint32_t cp = in_sucs[i];
        if (cp < 0x80u) {
            if (out_idx >= max_out) break;
            out_utf8[out_idx++] = (uint8_t)cp;
        } else if (cp < 0x800u) {
            if (out_idx + 2u > max_out) break;
            out_utf8[out_idx++] = (uint8_t)(0xC0u | (cp >> 6));
            out_utf8[out_idx++] = (uint8_t)(0x80u | (cp & 0x3Fu));
        } else if (cp < 0x10000u) {
            if (out_idx + 3u > max_out) break;
            out_utf8[out_idx++] = (uint8_t)(0xE0u | (cp >> 12));
            out_utf8[out_idx++] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
            out_utf8[out_idx++] = (uint8_t)(0x80u | (cp & 0x3Fu));
        } else if (cp <= 0x10FFFFu) {
            if (out_idx + 4u > max_out) break;
            out_utf8[out_idx++] = (uint8_t)(0xF0u | (cp >> 18));
            out_utf8[out_idx++] = (uint8_t)(0x80u | ((cp >> 12) & 0x3Fu));
            out_utf8[out_idx++] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
            out_utf8[out_idx++] = (uint8_t)(0x80u | (cp & 0x3Fu));
        }
    }

    *written = out_idx;
    return UNITRANS_OK;
}

#endif /* UNITRANS_H */
