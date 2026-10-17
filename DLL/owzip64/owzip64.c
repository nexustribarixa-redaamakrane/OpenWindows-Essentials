/*
 * owzip64.c - OpenWindows Data Compression Library (.owd)
 *
 * LZSS codec and CRC32/Adler-32 checksums.
 *
 * Stream grammar (OWZIP1):
 *   The output is a sequence of groups. Each group begins with one control
 *   byte whose bit N (LSB first) describes token N:
 *       bit = 1 -> literal: one following byte is copied verbatim.
 *       bit = 0 -> match:   two following bytes encode the back-reference.
 *   A match word is little-endian:
 *       bits 0..11 : distance - 1        (1..4096)
 *       bits 12..15: length - 3          (3..18)
 *   Tokens are packed 8 per control byte; the final control byte may
 *   describe fewer than eight tokens, detected by input exhaustion.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owzip64.h"

/* ── Encoder output writer ────────────────────────────────────── */

typedef struct {
    uint8_t *out;
    size_t   cap;
    size_t   len;
    size_t   flag_index;   /* Index of the active control byte */
    uint32_t nflags;       /* Tokens recorded in active control byte */
    bool     overflow;
} ow_zip_writer_t;

static void ow_zip_writer_init(ow_zip_writer_t *w,
                               uint8_t *out, size_t cap)
{
    w->out = out;
    w->cap = cap;
    w->len = 0u;
    w->flag_index = 0u;
    w->nflags = 0u;
    w->overflow = false;
}

/* Append one token (literal byte or 16-bit match word) to the stream. */
static void ow_zip_writer_token(ow_zip_writer_t *w, bool literal, uint16_t word)
{
    if (w->overflow) {
        return;
    }

    if (w->nflags == 0u) {
        if (w->len >= w->cap) {
            w->overflow = true;
            return;
        }
        w->flag_index = w->len;
        w->out[w->len++] = 0x00u;
    }

    if (literal) {
        if (w->len >= w->cap) {
            w->overflow = true;
            return;
        }
        w->out[w->flag_index] |= (uint8_t)(1u << w->nflags);
        w->out[w->len++] = (uint8_t)word;
    } else {
        if (w->len + 2u > w->cap) {
            w->overflow = true;
            return;
        }
        w->out[w->len++] = (uint8_t)(word & 0xFFu);
        w->out[w->len++] = (uint8_t)((word >> 8) & 0xFFu);
    }

    w->nflags = (w->nflags + 1u) & 7u;
}

/* ── Encoder match search ─────────────────────────────────────── */

/*
 * Find the longest match for the string starting at `pos` within the
 * previous OWZIP_WINDOW_SIZE bytes. Returns the match length (0 if none)
 * and writes the distance (1-based) to `*out_dist`.
 */
static uint32_t ow_zip_find_match(const uint8_t *src, size_t src_len,
                                  size_t pos, size_t *out_dist)
{
    size_t window_start;
    size_t max_len;
    size_t best_len = 0u;
    size_t best_dist = 0u;
    size_t j;

    if (pos < 1u || pos >= src_len) {
        *out_dist = 0u;
        return 0u;
    }

    max_len = src_len - pos;
    if (max_len > (size_t)OWZIP_MAX_MATCH) {
        max_len = (size_t)OWZIP_MAX_MATCH;
    }
    if (max_len < (size_t)OWZIP_MIN_MATCH) {
        *out_dist = 0u;
        return 0u;
    }

    window_start = (pos > (size_t)OWZIP_WINDOW_SIZE)
                       ? (pos - (size_t)OWZIP_WINDOW_SIZE)
                       : 0u;

    for (j = window_start; j < pos; ++j) {
        size_t l = 0u;

        while (l < max_len && src[j + l] == src[pos + l]) {
            l++;
        }

        if (l > best_len) {
            best_len = l;
            best_dist = pos - j;
            if (l == max_len) {
                break;
            }
        }
    }

    if (best_len < (size_t)OWZIP_MIN_MATCH) {
        *out_dist = 0u;
        return 0u;
    }

    *out_dist = best_dist;
    return (uint32_t)best_len;
}

/* ── Compression ──────────────────────────────────────────────── */

bool owzip_compress(const uint8_t *src, size_t src_len,
                    uint8_t *dst, size_t dst_cap, size_t *out_len)
{
    ow_zip_writer_t w;
    size_t pos;

    if (!out_len) {
        return false;
    }
    *out_len = 0u;

    if ((!src && src_len != 0u) || (!dst && dst_cap != 0u)) {
        return false;
    }

    ow_zip_writer_init(&w, dst, dst_cap);

    pos = 0u;
    while (pos < src_len) {
        size_t dist = 0u;
        uint32_t mlen = ow_zip_find_match(src, src_len, pos, &dist);

        if (mlen >= (uint32_t)OWZIP_MIN_MATCH && dist >= 1u) {
            uint16_t word = (uint16_t)(((dist - 1u) & 0x0FFFu) |
                                       ((size_t)(mlen - 3u) << 12));
            ow_zip_writer_token(&w, false, word);
            pos += (size_t)mlen;
        } else {
            ow_zip_writer_token(&w, true, (uint16_t)src[pos]);
            pos++;
        }

        if (w.overflow) {
            return false;
        }
    }

    *out_len = w.len;
    return true;
}

/* ── Decompression ────────────────────────────────────────────── */

bool owzip_decompress(const uint8_t *src, size_t src_len,
                      uint8_t *dst, size_t dst_cap, size_t *out_len)
{
    size_t ip = 0u;
    size_t op = 0u;

    if (!out_len) {
        return false;
    }
    *out_len = 0u;

    if ((!src && src_len != 0u) || (!dst && dst_cap != 0u)) {
        return false;
    }

    while (ip < src_len) {
        uint8_t flags = src[ip++];
        uint32_t bit;

        for (bit = 0u; bit < 8u; ++bit) {
            if (ip >= src_len) {
                break;
            }

            if ((flags >> bit) & 1u) {
                /* Literal byte. */
                if (op >= dst_cap) {
                    return false;
                }
                dst[op++] = src[ip++];
            } else {
                /* Two-byte back-reference. */
                size_t off;
                size_t len;
                size_t k;

                if (ip + 2u > src_len) {
                    return false;
                }
                {
                    uint16_t v = (uint16_t)((uint16_t)src[ip] |
                                            ((uint16_t)src[ip + 1u] << 8));
                    ip += 2u;
                    off = (size_t)(v & 0x0FFFu) + 1u;
                    len = (size_t)((v >> 12) & 0x0Fu) + (size_t)OWZIP_MIN_MATCH;
                }

                if (off > op) {
                    return false;   /* Reference before start of output. */
                }
                if (op + len > dst_cap) {
                    return false;
                }

                for (k = 0u; k < len; ++k) {
                    dst[op] = dst[op - off];
                    op++;
                }
            }
        }
    }

    *out_len = op;
    return true;
}

/* ── CRC32 (IEEE 802.3) ───────────────────────────────────────── */

uint32_t owzip_crc32(uint32_t crc, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t c = crc ^ 0xFFFFFFFFu;
    size_t i;
    uint32_t bit;

    if (!p && len != 0u) {
        return crc;
    }

    for (i = 0u; i < len; ++i) {
        c ^= (uint32_t)p[i];
        for (bit = 0u; bit < 8u; ++bit) {
            if (c & 1u) {
                c = (c >> 1) ^ 0xEDB88320u;
            } else {
                c >>= 1;
            }
        }
    }

    return c ^ 0xFFFFFFFFu;
}

/* ── Adler-32 (RFC 1950) ──────────────────────────────────────── */

uint32_t owzip_adler32(uint32_t adler, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t a = adler & 0xFFFFu;
    uint32_t b = (adler >> 16) & 0xFFFFu;
    size_t i;

    if (!p && len != 0u) {
        return adler;
    }

    for (i = 0u; i < len; ++i) {
        a = (a + (uint32_t)p[i]) % 65521u;
        b = (b + a) % 65521u;
    }

    return (b << 16) | a;
}
