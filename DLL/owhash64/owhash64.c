/*
 * owhash64.c - OpenWindows Hash Functions Library (.owd)
 *
 * Real SHA-256, MD5, and CRC32C (Castagnoli) digests.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owhash64.h"

/* ── Byte-order helpers ───────────────────────────────────────── */

static uint32_t ow_hash_rotr32(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32u - n));
}

static uint32_t ow_hash_rotl32(uint32_t x, uint32_t n)
{
    return (x << n) | (x >> (32u - n));
}

static uint32_t ow_hash_rd_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static void ow_hash_wr_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t ow_hash_rd_le32(const uint8_t *p)
{
    return  (uint32_t)p[0]        | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void ow_hash_wr_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void ow_hash_memcpy(void *dst, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) {
        *d++ = *s++;
    }
}

static void ow_hash_memset(void *dst, uint8_t val, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    while (n--) {
        *d++ = val;
    }
}

/* ── SHA-256 round constants ──────────────────────────────────── */

static const uint32_t ow_sha256_k[64] = {
    0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u,
    0x3956C25Bu, 0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u,
    0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u,
    0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u,
    0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu,
    0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
    0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u,
    0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u,
    0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u,
    0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
    0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u,
    0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
    0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u,
    0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
    0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u,
    0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u
};

/* ── SHA-256 block compression ────────────────────────────────── */

static void ow_sha256_transform(owhash_sha256_ctx_t *ctx, const uint8_t *block)
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t i;

    for (i = 0; i < 16u; ++i) {
        w[i] = ow_hash_rd_be32(block + (i * 4u));
    }
    for (i = 16u; i < 64u; ++i) {
        uint32_t s0 = ow_hash_rotr32(w[i - 15u], 7u) ^
                      ow_hash_rotr32(w[i - 15u], 18u) ^
                      (w[i - 15u] >> 3);
        uint32_t s1 = ow_hash_rotr32(w[i - 2u], 17u) ^
                      ow_hash_rotr32(w[i - 2u], 19u) ^
                      (w[i - 2u] >> 10);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64u; ++i) {
        uint32_t s1 = ow_hash_rotr32(e, 6u) ^ ow_hash_rotr32(e, 11u) ^
                      ow_hash_rotr32(e, 25u);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + s1 + ch + ow_sha256_k[i] + w[i];
        uint32_t s0 = ow_hash_rotr32(a, 2u) ^ ow_hash_rotr32(a, 13u) ^
                      ow_hash_rotr32(a, 22u);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

/* ── SHA-256 public API ───────────────────────────────────────── */

void owhash_sha256_init(owhash_sha256_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    ctx->state[0] = 0x6A09E667u;
    ctx->state[1] = 0xBB67AE85u;
    ctx->state[2] = 0x3C6EF372u;
    ctx->state[3] = 0xA54FF53Au;
    ctx->state[4] = 0x510E527Fu;
    ctx->state[5] = 0x9B05688Cu;
    ctx->state[6] = 0x1F83D9ABu;
    ctx->state[7] = 0x5BE0CD19u;
    ctx->bitlen = 0u;
    ctx->buflen = 0u;
    ow_hash_memset(ctx->buffer, 0u, sizeof(ctx->buffer));
}

void owhash_sha256_update(owhash_sha256_ctx_t *ctx, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    if (!ctx || (!p && len != 0u)) {
        return;
    }

    ctx->bitlen += (uint64_t)len * 8u;

    while (len > 0u) {
        uint32_t space = OWHASH_SHA256_BLOCK_SIZE - ctx->buflen;
        uint32_t take;

        if (space == 0u) {
            ow_sha256_transform(ctx, ctx->buffer);
            ctx->buflen = 0u;
            space = OWHASH_SHA256_BLOCK_SIZE;
        }

        take = (len < (size_t)space) ? (uint32_t)len : space;
        ow_hash_memcpy(ctx->buffer + ctx->buflen, p, (size_t)take);
        ctx->buflen += take;
        p += take;
        len -= (size_t)take;

        if (ctx->buflen == OWHASH_SHA256_BLOCK_SIZE) {
            ow_sha256_transform(ctx, ctx->buffer);
            ctx->buflen = 0u;
        }
    }
}

void owhash_sha256_final(owhash_sha256_ctx_t *ctx,
                         uint8_t out[OWHASH_SHA256_DIGEST_SIZE])
{
    uint64_t bits;
    uint32_t i;

    if (!ctx || !out) {
        return;
    }

    bits = ctx->bitlen;
    ctx->buffer[ctx->buflen++] = 0x80u;

    if (ctx->buflen > 56u) {
        while (ctx->buflen < OWHASH_SHA256_BLOCK_SIZE) {
            ctx->buffer[ctx->buflen++] = 0x00u;
        }
        ow_sha256_transform(ctx, ctx->buffer);
        ctx->buflen = 0u;
    }
    while (ctx->buflen < 56u) {
        ctx->buffer[ctx->buflen++] = 0x00u;
    }

    /* Big-endian 64-bit message length in bits. */
    for (i = 0; i < 8u; ++i) {
        ctx->buffer[56u + i] = (uint8_t)(bits >> (56u - (i * 8u)));
    }
    ow_sha256_transform(ctx, ctx->buffer);

    for (i = 0; i < 8u; ++i) {
        ow_hash_wr_be32(out + (i * 4u), ctx->state[i]);
    }
}

/* ── MD5 ──────────────────────────────────────────────────────── */

typedef struct {
    uint32_t state[4];
    uint64_t bitlen;
    uint8_t  buffer[64];
    uint32_t buflen;
} ow_md5_ctx_t;

/* Per-round left-rotation amounts. */
static const uint32_t ow_md5_s[64] = {
    7u, 12u, 17u, 22u,  7u, 12u, 17u, 22u,  7u, 12u, 17u, 22u,  7u, 12u, 17u, 22u,
    5u,  9u, 14u, 20u,  5u,  9u, 14u, 20u,  5u,  9u, 14u, 20u,  5u,  9u, 14u, 20u,
    4u, 11u, 16u, 23u,  4u, 11u, 16u, 23u,  4u, 11u, 16u, 23u,  4u, 11u, 16u, 23u,
    6u, 10u, 15u, 21u,  6u, 10u, 15u, 21u,  6u, 10u, 15u, 21u,  6u, 10u, 15u, 21u
};

/* Per-round additive constants: floor(2^32 * |sin(i+1)|). */
static const uint32_t ow_md5_k[64] = {
    0xD76AA478u, 0xE8C7B756u, 0x242070DBu, 0xC1BDCEEEu,
    0xF57C0FAFu, 0x4787C62Au, 0xA8304613u, 0xFD469501u,
    0x698098D8u, 0x8B44F7AFu, 0xFFFF5BB1u, 0x895CD7BEu,
    0x6B901122u, 0xFD987193u, 0xA679438Eu, 0x49B40821u,
    0xF61E2562u, 0xC040B340u, 0x265E5A51u, 0xE9B6C7AAu,
    0xD62F105Du, 0x02441453u, 0xD8A1E681u, 0xE7D3FBC8u,
    0x21E1CDE6u, 0xC33707D6u, 0xF4D50D87u, 0x455A14EDu,
    0xA9E3E905u, 0xFCEFA3F8u, 0x676F02D9u, 0x8D2A4C8Au,
    0xFFFA3942u, 0x8771F681u, 0x6D9D6122u, 0xFDE5380Cu,
    0xA4BEEA44u, 0x4BDECFA9u, 0xF6BB4B60u, 0xBEBFBC70u,
    0x289B7EC6u, 0xEAA127FAu, 0xD4EF3085u, 0x04881D05u,
    0xD9D4D039u, 0xE6DB99E5u, 0x1FA27CF8u, 0xC4AC5665u,
    0xF4292244u, 0x432AFF97u, 0xAB9423A7u, 0xFC93A039u,
    0x655B59C3u, 0x8F0CCC92u, 0xFFEFF47Du, 0x85845DD1u,
    0x6FA87E4Fu, 0xFE2CE6E0u, 0xA3014314u, 0x4E0811A1u,
    0xF7537E82u, 0xBD3AF235u, 0x2AD7D2BBu, 0xEB86D391u
};

static void ow_md5_transform(ow_md5_ctx_t *ctx, const uint8_t *block)
{
    uint32_t m[16];
    uint32_t a, b, c, d;
    uint32_t i;

    for (i = 0; i < 16u; ++i) {
        m[i] = ow_hash_rd_le32(block + (i * 4u));
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];

    for (i = 0; i < 64u; ++i) {
        uint32_t f;
        uint32_t g;

        if (i < 16u) {
            f = (b & c) | ((~b) & d);
            g = i;
        } else if (i < 32u) {
            f = (d & b) | ((~d) & c);
            g = ((5u * i) + 1u) & 15u;
        } else if (i < 48u) {
            f = b ^ c ^ d;
            g = ((3u * i) + 5u) & 15u;
        } else {
            f = c ^ (b | (~d));
            g = (7u * i) & 15u;
        }

        f = f + a + ow_md5_k[i] + m[g];
        a = d;
        d = c;
        c = b;
        b = b + ow_hash_rotl32(f, ow_md5_s[i]);
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

static void ow_md5_init(ow_md5_ctx_t *ctx)
{
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu;
    ctx->state[3] = 0x10325476u;
    ctx->bitlen = 0u;
    ctx->buflen = 0u;
}

static void ow_md5_update(ow_md5_ctx_t *ctx, const uint8_t *p, size_t len)
{
    ctx->bitlen += (uint64_t)len * 8u;

    while (len > 0u) {
        uint32_t space = 64u - ctx->buflen;
        uint32_t take;

        if (space == 0u) {
            ow_md5_transform(ctx, ctx->buffer);
            ctx->buflen = 0u;
            space = 64u;
        }

        take = (len < (size_t)space) ? (uint32_t)len : space;
        ow_hash_memcpy(ctx->buffer + ctx->buflen, p, (size_t)take);
        ctx->buflen += take;
        p += take;
        len -= (size_t)take;

        if (ctx->buflen == 64u) {
            ow_md5_transform(ctx, ctx->buffer);
            ctx->buflen = 0u;
        }
    }
}

static void ow_md5_final(ow_md5_ctx_t *ctx, uint8_t out[16])
{
    uint64_t bits;
    uint32_t i;

    bits = ctx->bitlen;
    ctx->buffer[ctx->buflen++] = 0x80u;

    if (ctx->buflen > 56u) {
        while (ctx->buflen < 64u) {
            ctx->buffer[ctx->buflen++] = 0x00u;
        }
        ow_md5_transform(ctx, ctx->buffer);
        ctx->buflen = 0u;
    }
    while (ctx->buflen < 56u) {
        ctx->buffer[ctx->buflen++] = 0x00u;
    }

    /* Little-endian 64-bit message length in bits. */
    for (i = 0; i < 8u; ++i) {
        ctx->buffer[56u + i] = (uint8_t)(bits >> (i * 8u));
    }
    ow_md5_transform(ctx, ctx->buffer);

    for (i = 0; i < 4u; ++i) {
        ow_hash_wr_le32(out + (i * 4u), ctx->state[i]);
    }
}

void owhash_md5(const void *data, size_t len,
                uint8_t out[OWHASH_MD5_DIGEST_SIZE])
{
    ow_md5_ctx_t ctx;

    if (!out) {
        return;
    }
    ow_md5_init(&ctx);
    ow_md5_update(&ctx, (const uint8_t *)data, len);
    ow_md5_final(&ctx, out);
}

/* ── CRC32C (Castagnoli) ──────────────────────────────────────── */

uint32_t owhash_crc32c(uint32_t crc, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t c = crc ^ 0xFFFFFFFFu;
    size_t i;
    uint32_t bit;

    if (!p && len != 0u) {
        return crc;
    }

    for (i = 0; i < len; ++i) {
        c ^= (uint32_t)p[i];
        for (bit = 0u; bit < 8u; ++bit) {
            /* Reflected Castagnoli polynomial 0x1EDC6F41 => 0x82F63B78. */
            if (c & 1u) {
                c = (c >> 1) ^ 0x82F63B78u;
            } else {
                c >>= 1;
            }
        }
    }

    return c ^ 0xFFFFFFFFu;
}
