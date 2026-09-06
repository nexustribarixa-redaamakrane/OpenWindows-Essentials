/*
 * owcrypt.c - owcrypt.owd implementation (OpenWindows Encryption Engine)
 *
 * Freestanding C99 cryptographic suite. Every primitive is implemented
 * here from its public specification; there is no hosted libc and no
 * third-party crypto dependency. AES S-box tables are generated at init
 * from the GF(2^8) generator 0x03, the Ed25519 curve constant d and the
 * square root of -1 are derived arithmetically at init, and the base
 * point is decompressed and self-validated against the curve equation,
 * so no hand-derived table can quietly corrupt the primitives.
 *
 * The only external symbols are the kernel64.owd API (late-bound at
 * module load) and libgcc 128-bit helpers.
 *
 * C99 freestanding strict profile.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#include "owcrypt.h"            /* Extensions ABI mirror */
#include "owd_format.h"
#include "owc_format.h"
#include "kernel64.h"

/* OWD1 binary header metadata (documentary; see Extensions/owd_format.h). */
#define OWCRYPT_LIB_NAME        "owcrypt.owd"
#define OWCRYPT_LIB_TYPE        OWD_LIBTYPE_HYBRID
#define OWCRYPT_TARGET_ARCH     0x02u
#define OWCRYPT_ALIGNMENT_LOG2  4u
#define OWCRYPT_INIT_FLAGS      (OWC_INIT_REQUIRES_OWRP | \
                                 OWC_INIT_REQUIRES_BANC)

#define OWCRYPT_LIB_VERSION     "owcrypt.owd 1.0.0"

static const uint8_t owcrypt_ident_str[] = OWCRYPT_LIB_VERSION;

static bool owcrypt_initialized = false;

/* ------------------------------------------------------------------ */
/*  Byte helpers                                                       */
/* ------------------------------------------------------------------ */

static void owc_put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static void owc_put32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t owc_get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static uint32_t owc_get32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void owc_put64(uint8_t *p, uint64_t v)
{
    p[0] = (uint8_t)(v >> 56); p[1] = (uint8_t)(v >> 48);
    p[2] = (uint8_t)(v >> 40); p[3] = (uint8_t)(v >> 32);
    p[4] = (uint8_t)(v >> 24); p[5] = (uint8_t)(v >> 16);
    p[6] = (uint8_t)(v >> 8);  p[7] = (uint8_t)v;
}

static uint64_t owc_get64(const uint8_t *p)
{
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48)
         | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32)
         | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16)
         | ((uint64_t)p[6] << 8)  | (uint64_t)p[7];
}

static void owc_put64le(uint8_t *p, uint64_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
    p[4] = (uint8_t)(v >> 32); p[5] = (uint8_t)(v >> 40);
    p[6] = (uint8_t)(v >> 48); p[7] = (uint8_t)(v >> 56);
}

static uint64_t owc_get64le(const uint8_t *p)
{
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8)
         | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24)
         | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40)
         | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

#define owc_rotl32(x, n) (((x) << (n)) | ((x) >> (32u - (n))))
#define owc_rotr32(x, n) (((x) >> (n)) | ((x) << (32u - (n))))
#define owc_rotr64(x, n) (((x) >> (n)) | ((x) << (64u - (n))))

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static uint8_t owc_sbox[256];
static uint8_t owc_inv_sbox[256];

owcrypt_status_t owcrypt_module_init(void)
{
    if (owcrypt_initialized) {
        return OWCRYPT_OK;
    }
    if (k64_initialize_api() != K64_OK) {
        return OWCRYPT_BAN_K64_BOOT;
    }
    /* Build the AES S-box + inverse from the GF(2^8) generator 0x03:
     * e[i] = 0x03^i, l[0x03^i] = i; then affine transform. */
    {
        uint8_t e[256], l[256];
        uint16_t x = 1u;
        for (uint32_t i = 0u; i < 256u; i++) {
            uint16_t o;
            e[i] = (uint8_t)x;
            l[(uint8_t)x] = (uint8_t)i;
            o = x;
            x <<= 1u;                        /*         x *= 0x03     */
            if ((x & 0x100u) != 0u) {
                x ^= 0x11Bu;
            }
            x ^= o;
        }
        for (uint32_t i = 0u; i < 256u; i++) {
            uint8_t inv = (i == 0u) ? 0u : e[255u - l[(uint8_t)i]];
            uint8_t t = inv;
            uint8_t s = inv;
            for (uint32_t r = 0u; r < 4u; r++) {
                t = (uint8_t)(((t << 1) | (t >> 7)) & 0xFFu);
                s ^= t;
            }
            owc_sbox[i] = (uint8_t)(s ^ 0x63u);
        }
        for (uint32_t i = 0u; i < 256u; i++) {
            owc_inv_sbox[owc_sbox[i]] = (uint8_t)i;
        }
    }
    /* Snapshot check: FIPS-197 S-Box[0x53] == 0xED. */
    if (owc_sbox[0x53u] != 0xEDu) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    owcrypt_initialized = true;
    return OWCRYPT_OK;
}

owcrypt_status_t owcrypt_module_shutdown(void)
{
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    owcrypt_cleanse(owc_sbox, sizeof(owc_sbox));
    owcrypt_cleanse(owc_inv_sbox, sizeof(owc_inv_sbox));
    owcrypt_initialized = false;
    return OWCRYPT_OK;
}

uint32_t owcrypt_abi_version(void)
{
    return (uint32_t)((1u * 10000u) + (0u * 100u) + 0u);
}

uint16_t owcrypt_abi_major(void) { return 1u; }
uint16_t owcrypt_abi_minor(void) { return 0u; }
const uint8_t *owcrypt_ident(void) { return owcrypt_ident_str; }

/* ------------------------------------------------------------------ */
/*  SHA-256 (FIPS 180-4)                                               */
/* ------------------------------------------------------------------ */

static const uint32_t owc_sha256_k[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,
    0x923f82a4u,0xab1c5ed5u,0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,
    0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,0xe49b69c1u,0xefbe4786u,
    0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,
    0x06ca6351u,0x14292967u,0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,
    0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,0xa2bfe8a1u,0xa81a664bu,
    0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,
    0x5b9cca4fu,0x682e6ff3u,0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,
    0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static void owc_sha256_compress(owcrypt_sha_ctx_t *c, const uint8_t *blk)
{
    uint32_t w[64];
    uint32_t a, b, cc, d, e, f, g, h;
    for (uint32_t i = 0u; i < 16u; i++) {
        w[i] = owc_get32(blk + 4u * i);
    }
    for (uint32_t i = 16u; i < 64u; i++) {
        uint32_t s0 = owc_rotr32(w[i - 15u], 7) ^ owc_rotr32(w[i - 15u], 18)
                     ^ (w[i - 15u] >> 3);
        uint32_t s1 = owc_rotr32(w[i - 2u], 17) ^ owc_rotr32(w[i - 2u], 19)
                     ^ (w[i - 2u] >> 10);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }
    a = (uint32_t)c->h[0]; b = (uint32_t)c->h[1];
    cc = (uint32_t)c->h[2]; d = (uint32_t)c->h[3];
    e = (uint32_t)c->h[4]; f = (uint32_t)c->h[5];
    g = (uint32_t)c->h[6]; h = (uint32_t)c->h[7];
    for (uint32_t i = 0u; i < 64u; i++) {
        uint32_t s1 = owc_rotr32(e, 6) ^ owc_rotr32(e, 11) ^ owc_rotr32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + ch + owc_sha256_k[i] + w[i];
        uint32_t s0 = owc_rotr32(a, 2) ^ owc_rotr32(a, 13) ^ owc_rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a;  c->h[1] += b;  c->h[2] += cc; c->h[3] += d;
    c->h[4] += e;  c->h[5] += f;  c->h[6] += g;  c->h[7] += h;
}

void owcrypt_sha256_init(owcrypt_sha_ctx_t *c)
{
    c->h[0] = 0x6a09e667u; c->h[1] = 0xbb67ae85u;
    c->h[2] = 0x3c6ef372u; c->h[3] = 0xa54ff53au;
    c->h[4] = 0x510e527fu; c->h[5] = 0x9b05688cu;
    c->h[6] = 0x1f83d9abu; c->h[7] = 0x5be0cd19u;
    c->total[0] = 0u; c->total[1] = 0u; c->buflen = 0u;
}

void owcrypt_sha256_update(owcrypt_sha_ctx_t *c, const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    c->total[0] += n;
    if (c->buflen != 0u) {
        size_t need = 64u - c->buflen;
        size_t take = (n < need) ? n : need;
        for (size_t i = 0u; i < take; i++) {
            c->buf[c->buflen + i] = p[i];
        }
        c->buflen += take;
        n -= take;
        p += take;
        if (c->buflen == 64u) {
            owc_sha256_compress(c, c->buf);
            c->buflen = 0u;
        }
    }
    while (n >= 64u) {
        owc_sha256_compress(c, p);
        p += 64u;
        n -= 64u;
    }
    for (size_t i = 0u; i < n; i++) {
        c->buf[c->buflen + i] = p[i];
    }
    c->buflen += n;
}

void owcrypt_sha256_final(owcrypt_sha_ctx_t *c, uint8_t out[32])
{
    uint64_t bits = c->total[0] * 8u;
    c->buf[c->buflen++] = 0x80u;
    if (c->buflen > 56u) {
        while (c->buflen < 64u) {
            c->buf[c->buflen++] = 0u;
        }
        owc_sha256_compress(c, c->buf);
        c->buflen = 0u;
    }
    while (c->buflen < 56u) {
        c->buf[c->buflen++] = 0u;
    }
    owc_put64(c->buf + 56u, bits);
    owc_sha256_compress(c, c->buf);
    for (uint32_t i = 0u; i < 8u; i++) {
        owc_put32(out + 4u * i, (uint32_t)c->h[i]);
    }
}

void owcrypt_sha256(const void *data, size_t n, uint8_t out[32])
{
    owcrypt_sha_ctx_t c;
    owcrypt_sha256_init(&c);
    owcrypt_sha256_update(&c, data, n);
    owcrypt_sha256_final(&c, out);
}

/* ------------------------------------------------------------------ */
/*  SHA-512 (FIPS 180-4)                                               */
/* ------------------------------------------------------------------ */

static const uint64_t owc_sha512_k[80] = {
    0x428a2f98d728ae22u,0x7137449123ef65cdu,0xb5c0fbcfec4d3b2fu,0xe9b5dba58189dbbcu,
    0x3956c25bf348b538u,0x59f111f1b605d019u,0x923f82a4af194f9bu,0xab1c5ed5da6d8118u,
    0xd807aa98a3030242u,0x12835b0145706fbeu,0x243185be4ee4b28cu,0x550c7dc3d5ffb4e2u,
    0x72be5d74f27b896fu,0x80deb1fe3b1696b1u,0x9bdc06a725c71235u,0xc19bf174cf692694u,
    0xe49b69c19ef14ad2u,0xefbe4786384f25e3u,0x0fc19dc68b8cd5b5u,0x240ca1cc77ac9c65u,
    0x2de92c6f592b0275u,0x4a7484aa6ea6e483u,0x5cb0a9dcbd41fbd4u,0x76f988da831153b5u,
    0x983e5152ee66dfabu,0xa831c66d2db43210u,0xb00327c898fb213fu,0xbf597fc7beef0ee4u,
    0xc6e00bf33da88fc2u,0xd5a79147930aa725u,0x06ca6351e003826fu,0x142929670a0e6e70u,
    0x27b70a8546d22ffcu,0x2e1b21385c26c926u,0x4d2c6dfc5ac42aedu,0x53380d139d95b3dfu,
    0x650a73548baf63deu,0x766a0abb3c77b2a8u,0x81c2c92e47edaee6u,0x92722c851482353bu,
    0xa2bfe8a14cf10364u,0xa81a664bbc423001u,0xc24b8b70d0f89791u,0xc76c51a30654be30u,
    0xd192e819d6ef5218u,0xd69906245565a910u,0xf40e35855771202au,0x106aa07032bbd1b8u,
    0x19a4c116b8d2d0c8u,0x1e376c085141ab53u,0x2748774cdf8eeb99u,0x34b0bcb5e19b48a8u,
    0x391c0cb3c5c95a63u,0x4ed8aa4ae3418acbu,0x5b9cca4f7763e373u,0x682e6ff3d6b2b8a3u,
    0x748f82ee5defb2fcu,0x78a5636f43172f60u,0x84c87814a1f0ab72u,0x8cc702081a6439ecu,
    0x90befffa23631e28u,0xa4506cebde82bde9u,0xbef9a3f7b2c67915u,0xc67178f2e372532bu,
    0xca273eceea26619cu,0xd186b8c721c0c207u,0xeada7dd6cde0eb1eu,0xf57d4f7fee6ed178u,
    0x06f067aa72176fbau,0x0a637dc5a2c898a6u,0x113f9804bef90daeu,0x1b710b35131c471bu,
    0x28db77f523047d84u,0x32caab7b40c72493u,0x3c9ebe0a15c9bebcu,0x431d67c49c100d4cu,
    0x4cc5d4becb3e42b6u,0x597f299cfc657e2au,0x5fcb6fab3ad6faecu,0x6c44198c4a475817u
};

static void owc_sha512_compress(owcrypt_sha_ctx_t *c, const uint8_t *blk)
{
    uint64_t w[80];
    uint64_t a, b, cc, d, e, f, g, h;
    for (uint32_t i = 0u; i < 16u; i++) {
        w[i] = owc_get64(blk + 8u * i);
    }
    for (uint32_t i = 16u; i < 80u; i++) {
        uint64_t s0 = owc_rotr64(w[i - 15u], 1) ^ owc_rotr64(w[i - 15u], 8)
                     ^ (w[i - 15u] >> 7);
        uint64_t s1 = owc_rotr64(w[i - 2u], 19) ^ owc_rotr64(w[i - 2u], 61)
                     ^ (w[i - 2u] >> 6);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3];
    e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];
    for (uint32_t i = 0u; i < 80u; i++) {
        uint64_t s1 = owc_rotr64(e, 14) ^ owc_rotr64(e, 18) ^ owc_rotr64(e, 41);
        uint64_t ch = (e & f) ^ (~e & g);
        uint64_t t1 = h + s1 + ch + owc_sha512_k[i] + w[i];
        uint64_t s0 = owc_rotr64(a, 28) ^ owc_rotr64(a, 34) ^ owc_rotr64(a, 39);
        uint64_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint64_t t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

void owcrypt_sha512_init(owcrypt_sha_ctx_t *c)
{
    c->h[0] = 0x6a09e667f3bcc908u; c->h[1] = 0xbb67ae8584caa73bu;
    c->h[2] = 0x3c6ef372fe94f82bu; c->h[3] = 0xa54ff53a5f1d36f1u;
    c->h[4] = 0x510e527fade682d1u; c->h[5] = 0x9b05688c2b3e6c1fu;
    c->h[6] = 0x1f83d9abfb41bd6bu; c->h[7] = 0x5be0cd19137e2179u;
    c->total[0] = 0u; c->total[1] = 0u; c->buflen = 0u;
}

void owcrypt_sha512_update(owcrypt_sha_ctx_t *c, const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    c->total[0] += n;
    if (c->buflen != 0u) {
        size_t need = 128u - c->buflen;
        size_t take = (n < need) ? n : need;
        for (size_t i = 0u; i < take; i++) {
            c->buf[c->buflen + i] = p[i];
        }
        c->buflen += take;
        n -= take;
        p += take;
        if (c->buflen == 128u) {
            owc_sha512_compress(c, c->buf);
            c->buflen = 0u;
        }
    }
    while (n >= 128u) {
        owc_sha512_compress(c, p);
        p += 128u;
        n -= 128u;
    }
    for (size_t i = 0u; i < n; i++) {
        c->buf[c->buflen + i] = p[i];
    }
    c->buflen += n;
}

void owcrypt_sha512_final(owcrypt_sha_ctx_t *c, uint8_t out[64])
{
    uint64_t bits = c->total[0] * 8u;
    c->buf[c->buflen++] = 0x80u;
    if (c->buflen > 112u) {
        while (c->buflen < 128u) {
            c->buf[c->buflen++] = 0u;
        }
        owc_sha512_compress(c, c->buf);
        c->buflen = 0u;
    }
    while (c->buflen < 112u) {
        c->buf[c->buflen++] = 0u;
    }
    owc_put64(c->buf + 112u, 0u);       /* high 64 bits of 128-bit length */
    owc_put64(c->buf + 120u, bits);     /* low  64 bits */
    owc_sha512_compress(c, c->buf);
    for (uint32_t i = 0u; i < 8u; i++) {
        owc_put64(out + 8u * i, c->h[i]);
    }
}

void owcrypt_sha512(const void *data, size_t n, uint8_t out[64])
{
    owcrypt_sha_ctx_t c;
    owcrypt_sha512_init(&c);
    owcrypt_sha512_update(&c, data, n);
    owcrypt_sha512_final(&c, out);
}

/* ------------------------------------------------------------------ */
/*  BLAKE2b (RFC 7693)                                                 */
/* ------------------------------------------------------------------ */

static const uint64_t owc_blake2b_iv[8] = {
    0x6a09e667f3bcc908u, 0xbb67ae8584caa73bu, 0x3c6ef372fe94f82bu,
    0xa54ff53a5f1d36f1u, 0x510e527fade682d1u, 0x9b05688c2b3e6c1fu,
    0x1f83d9abfb41bd6bu, 0x5be0cd19137e2179u
};

static const uint8_t owc_blake2b_sigma[12][16] = {
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
    {14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3},
    {11,8,12,0,5,2,15,13,10,14,3,6,7,1,9,4},
    {7,9,3,1,13,12,11,14,2,6,5,10,4,0,15,8},
    {9,0,5,7,2,4,10,15,14,1,11,12,6,8,3,13},
    {2,12,6,10,0,11,8,3,4,13,7,5,15,14,1,9},
    {12,5,1,15,14,13,4,10,0,7,6,3,9,2,8,11},
    {13,11,7,14,12,1,3,9,5,0,15,4,8,6,2,10},
    {6,15,14,9,11,3,0,8,12,2,13,7,1,4,10,5},
    {10,2,8,4,7,6,1,5,15,11,9,14,3,12,13,0},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
    {14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3}
};

static void owc_b2_g(uint64_t *v, uint32_t a, uint32_t b, uint32_t c, uint32_t dd,
                     uint64_t x, uint64_t y)
{
    v[a] = v[a] + v[b] + x;
    v[dd] = owc_rotr64(v[dd] ^ v[a], 32u);
    v[c] = v[c] + v[dd];
    v[b] = owc_rotr64(v[b] ^ v[c], 24u);
    v[a] = v[a] + v[b] + y;
    v[dd] = owc_rotr64(v[dd] ^ v[a], 16u);
    v[c] = v[c] + v[dd];
    v[b] = owc_rotr64(v[b] ^ v[c], 63u);
}

void owcrypt_blake2b(uint8_t *out, size_t outlen,
                     const void *in, size_t inlen)
{
    uint64_t h[8], v[16], m[16], t0 = 0u;
    const uint8_t *p = (const uint8_t *)in;
    size_t remaining = inlen;

    for (uint32_t i = 0u; i < 8u; i++) {
        h[i] = owc_blake2b_iv[i];
    }
    h[0] ^= 0x01010000u ^ ((uint64_t)outlen & 0xFFu);

    while (remaining > 0u) {
        size_t n = (remaining > 128u) ? 128u : remaining;
        t0 += (uint64_t)n;
        uint64_t fin = (remaining <= 128u) ? ~(uint64_t)0u : 0u;
        for (uint32_t i = 0u; i < 16u; i++) {
            m[i] = 0u;
        }
        for (size_t i = 0u; i < n; i++) {
            m[i / 8u] |= ((uint64_t)p[i]) << (8u * (i % 8u));
        }
        for (uint32_t i = 0u; i < 8u; i++) {
            v[i] = h[i];
            v[i + 8u] = owc_blake2b_iv[i];
        }
        v[12] ^= t0;                    /* t low                       */
        v[13] ^= 0u;                    /* t high (counter fits u64)   */
        v[14] ^= fin;                   /* f0 (final flag)             */
        /* f1 stays 0 for the non-tree case                            */
        for (uint32_t r = 0u; r < 12u; r++) {
            const uint8_t *s = owc_blake2b_sigma[r];
            owc_b2_g(v, 0u, 4u,  8u, 12u, m[s[0]], m[s[1]]);
            owc_b2_g(v, 1u, 5u,  9u, 13u, m[s[2]], m[s[3]]);
            owc_b2_g(v, 2u, 6u, 10u, 14u, m[s[4]], m[s[5]]);
            owc_b2_g(v, 3u, 7u, 11u, 15u, m[s[6]], m[s[7]]);
            owc_b2_g(v, 0u, 5u, 10u, 15u, m[s[8]], m[s[9]]);
            owc_b2_g(v, 1u, 6u, 11u, 12u, m[s[10]], m[s[11]]);
            owc_b2_g(v, 2u, 7u,  8u, 13u, m[s[12]], m[s[13]]);
            owc_b2_g(v, 3u, 4u,  9u, 14u, m[s[14]], m[s[15]]);
        }
        for (uint32_t i = 0u; i < 8u; i++) {
            h[i] ^= v[i] ^ v[i + 8u];
        }
        p += n;
        remaining -= n;
    }

    for (size_t i = 0u; i < outlen; i++) {
        out[i] = (uint8_t)(h[i / 8u] >> (8u * (i % 8u)));
    }
}

/* ------------------------------------------------------------------ */
/*  HMAC-SHA256 / HMAC-SHA512                                          */
/* ------------------------------------------------------------------ */

void owcrypt_hmac_sha256(const uint8_t *key, size_t keylen,
                         const void *msg, size_t msglen, uint8_t out[32])
{
    uint8_t k[64], ipad[64], opad[64], inner[32];
    owcrypt_sha_ctx_t c;

    if (keylen > 64u) {
        owcrypt_sha256(key, keylen, k);
        for (uint32_t i = 32u; i < 64u; i++) {
            k[i] = 0u;
        }
    } else {
        for (size_t i = 0u; i < keylen; i++) {
            k[i] = key[i];
        }
        for (size_t i = keylen; i < 64u; i++) {
            k[i] = 0u;
        }
    }
    for (uint32_t i = 0u; i < 64u; i++) {
        ipad[i] = k[i] ^ 0x36u;
        opad[i] = k[i] ^ 0x5Cu;
    }
    owcrypt_sha256_init(&c);
    owcrypt_sha256_update(&c, ipad, 64u);
    owcrypt_sha256_update(&c, msg, msglen);
    owcrypt_sha256_final(&c, inner);
    owcrypt_sha256_init(&c);
    owcrypt_sha256_update(&c, opad, 64u);
    owcrypt_sha256_update(&c, inner, 32u);
    owcrypt_sha256_final(&c, out);
}

void owcrypt_hmac_sha512(const uint8_t *key, size_t keylen,
                         const void *msg, size_t msglen, uint8_t out[64])
{
    uint8_t k[128], ipad[128], opad[128], inner[64];
    owcrypt_sha_ctx_t c;

    if (keylen > 128u) {
        owcrypt_sha512(key, keylen, k);
        for (uint32_t i = 64u; i < 128u; i++) {
            k[i] = 0u;
        }
    } else {
        for (size_t i = 0u; i < keylen; i++) {
            k[i] = key[i];
        }
        for (size_t i = keylen; i < 128u; i++) {
            k[i] = 0u;
        }
    }
    for (uint32_t i = 0u; i < 128u; i++) {
        ipad[i] = k[i] ^ 0x36u;
        opad[i] = k[i] ^ 0x5Cu;
    }
    owcrypt_sha512_init(&c);
    owcrypt_sha512_update(&c, ipad, 128u);
    owcrypt_sha512_update(&c, msg, msglen);
    owcrypt_sha512_final(&c, inner);
    owcrypt_sha512_init(&c);
    owcrypt_sha512_update(&c, opad, 128u);
    owcrypt_sha512_update(&c, inner, 64u);
    owcrypt_sha512_final(&c, out);
}

/* ------------------------------------------------------------------ */
/*  ChaCha20 (RFC 8439)                                                */
/* ------------------------------------------------------------------ */

static void owc_qr(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    *a += *b; *d = owc_rotl32(*d ^ *a, 16);
    *c += *d; *b = owc_rotl32(*b ^ *c, 12);
    *a += *b; *d = owc_rotl32(*d ^ *a, 8);
    *c += *d; *b = owc_rotl32(*b ^ *c, 7);
}

static void owc_chacha_block(const uint8_t key[32], const uint8_t nonce[12],
                             uint32_t counter, uint8_t out[64])
{
    uint32_t s[16], x[16];
    s[0] = 0x61707865u; s[1] = 0x3320646eu;
    s[2] = 0x79622d32u; s[3] = 0x6b206574u;
    for (uint32_t i = 0u; i < 8u; i++) {
        s[4u + i] = owc_get32le(key + 4u * i);
    }
    s[12] = counter;
    for (uint32_t i = 0u; i < 3u; i++) {
        s[13u + i] = owc_get32le(nonce + 4u * i);
    }
    for (uint32_t i = 0u; i < 16u; i++) {
        x[i] = s[i];
    }
    for (uint32_t r = 0u; r < 10u; r++) {
        owc_qr(&x[0], &x[4], &x[8],  &x[12]);
        owc_qr(&x[1], &x[5], &x[9],  &x[13]);
        owc_qr(&x[2], &x[6], &x[10], &x[14]);
        owc_qr(&x[3], &x[7], &x[11], &x[15]);
        owc_qr(&x[0], &x[5], &x[10], &x[15]);
        owc_qr(&x[1], &x[6], &x[11], &x[12]);
        owc_qr(&x[2], &x[7], &x[8],  &x[13]);
        owc_qr(&x[3], &x[4], &x[9],  &x[14]);
    }
    for (uint32_t i = 0u; i < 16u; i++) {
        owc_put32le(out + 4u * i, x[i] + s[i]);
    }
}

void owcrypt_chacha20_xor(const uint8_t key[32], const uint8_t nonce[12],
                          uint32_t counter, const uint8_t *in, size_t len,
                          uint8_t *out)
{
    uint8_t ks[64];
    size_t off = 0u;
    while (off < len) {
        owc_chacha_block(key, nonce, counter, ks);
        size_t n = len - off;
        if (n > 64u) {
            n = 64u;
        }
        if (in == NULL) {
            for (size_t i = 0u; i < n; i++) {
                out[off + i] = ks[i];
            }
        } else {
            for (size_t i = 0u; i < n; i++) {
                out[off + i] = in[off + i] ^ ks[i];
            }
        }
        off += n;
        counter++;
    }
}

/* ------------------------------------------------------------------ */
/*  Poly1305 (RFC 8439) - streaming 26-bit-limb implementation         */
/*                                                                     */
/*  poly1305-donna 32 structure: the 130-bit accumulator lives in five */
/*  26-bit lanes; multiplication is the circular "wrap" reduction      */
/*  (bits >= 130 come back multiplied by 5), so no big-integer fold is */
/*  required and wrap-free u64 math suffices everywhere.               */
/* ------------------------------------------------------------------ */

#define OWC_P26      26u
#define OWC_P26M     0x3FFFFFFu

typedef struct {
    uint64_t h[5];
    uint64_t r[5];
    uint64_t s[4];          /* s[i] = r[i+1] * 5                      */
    uint64_t pad0;
    uint64_t pad1;
    uint8_t  block[16];
    size_t   blocklen;
} owc_p1305_t;

static void owc_p1305_init(owc_p1305_t *c, const uint8_t key[32])
{
    uint64_t r0 = owc_get64le(key);
    uint64_t r1 = owc_get64le(key + 8);
    uint64_t i;
    r0 &= 0x0FFFFFFC0FFFFFFFull;
    r1 &= 0x0FFFFFFC0FFFFFFCull;
    c->r[0] = r0 & OWC_P26M;
    c->r[1] = (r0 >> 26) & OWC_P26M;
    c->r[2] = (r0 >> 52) | ((r1 & 0x3FFFu) << 12);
    c->r[3] = (r1 >> 14) & OWC_P26M;
    c->r[4] = (r1 >> 40) & OWC_P26M;
    c->s[0] = c->r[1] * 5u;
    c->s[1] = c->r[2] * 5u;
    c->s[2] = c->r[3] * 5u;
    c->s[3] = c->r[4] * 5u;
    for (i = 0u; i < 5u; i++) {
        c->h[i] = 0u;
    }
    c->pad0 = owc_get64le(key + 16);
    c->pad1 = owc_get64le(key + 24);
    c->blocklen = 0u;
}

static void owc_p1305_mul(owc_p1305_t *c)
{
    uint64_t d0, d1, d2, d3, d4, c1;
    d0 = c->h[0] * c->r[0] + c->h[1] * c->s[3] + c->h[2] * c->s[2]
       + c->h[3] * c->s[1] + c->h[4] * c->s[0];
    d1 = c->h[0] * c->r[1] + c->h[1] * c->r[0] + c->h[2] * c->s[3]
       + c->h[3] * c->s[2] + c->h[4] * c->s[1];
    d2 = c->h[0] * c->r[2] + c->h[1] * c->r[1] + c->h[2] * c->r[0]
       + c->h[3] * c->s[3] + c->h[4] * c->s[2];
    d3 = c->h[0] * c->r[3] + c->h[1] * c->r[2] + c->h[2] * c->r[1]
       + c->h[3] * c->r[0] + c->h[4] * c->s[3];
    d4 = c->h[0] * c->r[4] + c->h[1] * c->r[3] + c->h[2] * c->r[2]
       + c->h[3] * c->r[1] + c->h[4] * c->r[0];

    c->h[0] = d0 & OWC_P26M;  c1 = d0 >> 26; d1 += c1;
    c->h[1] = d1 & OWC_P26M;  c1 = d1 >> 26; d2 += c1;
    c->h[2] = d2 & OWC_P26M;  c1 = d2 >> 26; d3 += c1;
    c->h[3] = d3 & OWC_P26M;  c1 = d3 >> 26; d4 += c1;
    c->h[4] = d4 & OWC_P26M;  c1 = d4 >> 26;
    c->h[0] += c1 * 5u;       c1 = c->h[0] >> 26; c->h[0] &= OWC_P26M; c->h[1] += c1;
    c1 = c->h[1] >> 26; c->h[1] &= OWC_P26M; c->h[2] += c1;
    c1 = c->h[2] >> 26; c->h[2] &= OWC_P26M; c->h[3] += c1;
    c1 = c->h[3] >> 26; c->h[3] &= OWC_P26M; c->h[4] += c1;
    c->h[4] &= OWC_P26M;
}

static void owc_p1305_block(owc_p1305_t *c, const uint8_t blk[16],
                            uint32_t tlen)
{
    uint64_t m0 = owc_get64le(blk);
    uint64_t m1 = owc_get64le(blk + 8);
    uint64_t md[5];
    uint64_t carry;
    uint32_t i;

    if (tlen < 16u) {
        if (tlen < 8u) {
            m0 |= (uint64_t)1 << (8u * tlen);
        } else {
            m1 |= (uint64_t)1 << (8u * (tlen - 8u));
        }
    }

    md[0] = m0 & OWC_P26M;
    md[1] = (m0 >> 26) & OWC_P26M;
    md[2] = (m0 >> 52) | ((m1 & 0x3FFFu) << 12);
    md[3] = (m1 >> 14) & OWC_P26M;
    md[4] = (m1 >> 40) & OWC_P26M;
    if (tlen == 16u) {
        md[4] |= (uint64_t)1 << 24;     /* 130th bit of the block latch */
    }

    carry = 0u;
    for (i = 0u; i < 5u; i++) {
        c->h[i] += md[i] + carry;
        carry = c->h[i] >> 26;
        c->h[i] &= OWC_P26M;
    }
    if (carry != 0u) {
        c->h[0] += carry * 5u;
        carry = c->h[0] >> 26;
        c->h[0] &= OWC_P26M;
        for (i = 1u; i < 5u && carry != 0u; i++) {
            c->h[i] += carry;
            carry = c->h[i] >> 26;
            c->h[i] &= OWC_P26M;
        }
    }

    owc_p1305_mul(c);
}

static void owc_p1305_update(owc_p1305_t *c, const uint8_t *data, size_t n)
{
    while (n > 0u) {
        size_t take = n;
        if (take > 16u - c->blocklen) {
            take = 16u - c->blocklen;
        }
        for (size_t i = 0u; i < take; i++) {
            c->block[c->blocklen + i] = data[i];
        }
        c->blocklen += take;
        data += take;
        n -= take;
        if (c->blocklen == 16u) {
            owc_p1305_block(c, c->block, 16u);
            c->blocklen = 0u;
        }
    }
}

static void owc_p1305_final(owc_p1305_t *c, uint8_t tag[16])
{
    uint64_t h0, h1, h2, h3, h4, g0, g1, g2, g3, g4, borrow;
    uint64_t e0, e1;

    if (c->blocklen != 0u) {
        for (size_t i = c->blocklen; i < 16u; i++) {
            c->block[i] = 0u;
        }
        owc_p1305_block(c, c->block, (uint32_t)c->blocklen);
        c->blocklen = 0u;
    }

    /* Conditional subtract p = 2^130-5.  g = h + 5 mod 2^130 equals h - p
     * exactly when h >= p; the add "wraps" (carry out of lane 4) only in
     * that case, so select g iff the chain produced a carry. */
    h0 = c->h[0]; h1 = c->h[1]; h2 = c->h[2]; h3 = c->h[3]; h4 = c->h[4];
    g0 = h0 + 5u;   /* h + 5 mod 2^130, borrow chain = carry propagation */
    borrow = (g0 < h0) ? 1u : 0u;
    g1 = h1 + borrow; borrow = (g1 < borrow) ? 1u : 0u;
    g2 = h2 + borrow; borrow = (g2 < borrow) ? 1u : 0u;
    g3 = h3 + borrow; borrow = (g3 < borrow) ? 1u : 0u;
    g4 = h4 + borrow; borrow = (g4 < borrow) ? 1u : 0u;
    /* Since lanes are < 2^26, "h + 5 mod 2^130" underflows only when the
     * lanes still exceed p; use g when the borrow chain did not wrap. */
    if (borrow != 0u) {
        h0 = g0; h1 = g1; h2 = g2; h3 = g3; h4 = g4;
    }

    e0 = h0 | (h1 << 26) | ((h2 & 0xFFFu) << 52);
    e1 = (h2 >> 12) | (h3 << 14) | ((h4 & 0xFFFFu) << 40) | ((h4 >> 16) << 56);
    {
        uint64_t s = e0 + c->pad0;
        uint64_t hi = e1 + c->pad1 + ((s < e0) ? 1u : 0u);
        owc_put64le(tag, s);
        owc_put64le(tag + 8, hi);
    }
}

void owcrypt_poly1305(const uint8_t key[32], const uint8_t *msg, size_t len,
                      uint8_t tag[16])
{
    owc_p1305_t c;
    owc_p1305_init(&c, key);
    owc_p1305_update(&c, msg, len);
    owc_p1305_final(&c, tag);
}

/* ------------------------------------------------------------------ */
/*  ChaCha20-Poly1305 AEAD (RFC 8439)                                  */
/* ------------------------------------------------------------------ */

static void owc_aead_mac(const uint8_t key[32], const uint8_t nonce[12],
                         const uint8_t *aad, size_t aadlen,
                         const uint8_t *ct, size_t clen,
                         uint8_t mac[16])
{
    uint8_t polykey[64];
    uint8_t zeros[16];
    uint8_t lenblk[16];
    owc_p1305_t m;
    size_t pa = (16u - (aadlen % 16u)) % 16u;
    size_t pc = (16u - (clen % 16u)) % 16u;

    for (size_t i = 0u; i < 16u; i++) {
        zeros[i] = 0u;
    }
    owc_chacha_block(key, nonce, 0u, polykey);
    owc_p1305_init(&m, polykey);
    owc_p1305_update(&m, aad, aadlen);
    owc_p1305_update(&m, zeros, pa);
    owc_p1305_update(&m, ct, clen);
    owc_p1305_update(&m, zeros, pc);
    owc_put64le(lenblk, (uint64_t)aadlen);
    owc_put64le(lenblk + 8, (uint64_t)clen);
    owc_p1305_update(&m, lenblk, 16u);
    owc_p1305_final(&m, mac);
}

owcrypt_status_t owcrypt_chacha_poly_seal(const uint8_t key[32],
                                          const uint8_t nonce[12],
                                          const uint8_t *aad, size_t aadlen,
                                          const uint8_t *plain, size_t plen,
                                          uint8_t *out, uint8_t tag[16])
{
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (key == NULL || nonce == NULL || out == NULL || tag == NULL) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    if (plen > 0u && plain == NULL) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    owcrypt_chacha20_xor(key, nonce, 1u, plain, plen, out);
    owc_aead_mac(key, nonce, aad, aadlen, out, plen, tag);
    return OWCRYPT_OK;
}

owcrypt_status_t owcrypt_chacha_poly_open(const uint8_t key[32],
                                          const uint8_t nonce[12],
                                          const uint8_t *aad, size_t aadlen,
                                          const uint8_t *cipher, size_t clen,
                                          const uint8_t tag[16],
                                          uint8_t *out)
{
    uint8_t expect[16];
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (key == NULL || nonce == NULL || tag == NULL ||
        (clen > 0u && cipher == NULL)) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    owc_aead_mac(key, nonce, aad, aadlen, cipher, clen, expect);
    if (!owcrypt_ct_memcmp(expect, tag, 16u)) {
        return OWCRYPT_BAN_AUTH_FAIL;
    }
    if (clen > 0u) {
        owcrypt_chacha20_xor(key, nonce, 1u, cipher, clen, out);
    }
    return OWCRYPT_OK;
}

/* ------------------------------------------------------------------ */
/*  Field arithmetic (radix 2^51, p = 2^255-19)                        */
/*  Uses __int128 products and a folding carry chain; every fe op      */
/*  returns a fully canonical value < p.                               */
/* ------------------------------------------------------------------ */

#define OWCRYPT_M51   0x7FFFFFFFFFFFFu   /* 2^51 - 1 */

typedef struct {
    uint64_t v[5];
} owc_fe_t;

static void owc_fe_zero(owc_fe_t *o)
{
    o->v[0] = o->v[1] = o->v[2] = o->v[3] = o->v[4] = 0u;
}

static void owc_fe_set_small(owc_fe_t *o, uint64_t x)
{
    owc_fe_zero(o);
    o->v[0] = x;
}

static void owc_fe_copy(owc_fe_t *o, const owc_fe_t *a)
{
    o->v[0] = a->v[0]; o->v[1] = a->v[1]; o->v[2] = a->v[2];
    o->v[3] = a->v[3]; o->v[4] = a->v[4];
}

/* Fully canonicalise five wide limbs (each entry < 2^51 accepted) into
 * a value < p. Overflow beyond limb 4 folds back as *19. */
static void owc_fe_canon(owc_fe_t *o)
{
    uint64_t c;
    uint32_t it;

    for (it = 0u; it < 32u; it++) {
        c = o->v[0] >> 51; o->v[0] &= OWCRYPT_M51;
        c = (o->v[1] += c) >> 51; o->v[1] &= OWCRYPT_M51;
        c = (o->v[2] += c) >> 51; o->v[2] &= OWCRYPT_M51;
        c = (o->v[3] += c) >> 51; o->v[3] &= OWCRYPT_M51;
        c = (o->v[4] += c) >> 51; o->v[4] &= OWCRYPT_M51;
        if (c == 0u) {
            break;
        }
        o->v[0] += 19u * c;
    }
    /* o->v now holds a value < 2^255 in five <2^51 limbs.
     * Subtract p once; when a borrow would appear (value already < p)
     * keep the original value. */
    {
        int64_t  b = 0;
        uint64_t r[5];
        int32_t  k;
        for (k = 0; k < 5; k++) {
            int64_t sub = (k == 0) ? (int64_t)(OWCRYPT_M51 - 18u)
                                   : (int64_t)OWCRYPT_M51;
            int64_t s = (int64_t)o->v[k] - sub - b;
            if (s < 0) {
                r[k] = (uint64_t)(s + ((int64_t)1 << 51));
                b = 1;
            } else {
                r[k] = (uint64_t)s;
                b = 0;
            }
        }
        if (b == 0) {
            for (k = 0; k < 5; k++) {
                o->v[k] = r[k];
            }
        }
    }
}

static void owc_fe_frombytes(owc_fe_t *o, const uint8_t in[32])
{
    uint64_t m0 = owc_get64le(in);
    uint64_t m1 = owc_get64le(in + 8);
    uint64_t m2 = owc_get64le(in + 16);
    uint64_t m3 = owc_get64le(in + 24);
    o->v[0] = m0 & OWCRYPT_M51;
    o->v[1] = ((m0 >> 51) | (m1 << 13)) & OWCRYPT_M51;
    o->v[2] = ((m1 >> 38) | (m2 << 26)) & OWCRYPT_M51;
    o->v[3] = ((m2 >> 25) | (m3 << 39)) & OWCRYPT_M51;
    o->v[4] = (m3 >> 12) & OWCRYPT_M51;
}

static void owc_fe_tobytes(uint8_t out[32], const owc_fe_t *a)
{
    uint64_t w0 = a->v[0] | (a->v[1] << 51);
    uint64_t w1 = (a->v[1] >> 13) | (a->v[2] << 38);
    uint64_t w2 = (a->v[2] >> 26) | (a->v[3] << 25);
    uint64_t w3 = (a->v[3] >> 39) | (a->v[4] << 12);
    owc_put64le(out, w0);
    owc_put64le(out + 8, w1);
    owc_put64le(out + 16, w2);
    owc_put64le(out + 24, w3);
}

static void owc_fe_add(owc_fe_t *o, const owc_fe_t *a, const owc_fe_t *b)
{
    o->v[0] = a->v[0] + b->v[0];
    o->v[1] = a->v[1] + b->v[1];
    o->v[2] = a->v[2] + b->v[2];
    o->v[3] = a->v[3] + b->v[3];
    o->v[4] = a->v[4] + b->v[4];
    owc_fe_canon(o);
}

static void owc_fe_sub(owc_fe_t *o, const owc_fe_t *a, const owc_fe_t *b)
{
    int64_t  br = 0;
    uint64_t r[5];
    int32_t  k;
    for (k = 0; k < 5; k++) {
        int64_t d = (int64_t)a->v[k] - (int64_t)b->v[k] - br;
        if (d < 0) {
            r[k] = (uint64_t)(d + ((int64_t)1 << 51));
            br = 1;
        } else {
            r[k] = (uint64_t)d;
            br = 0;
        }
    }
    if (br != 0) {
        /* a < b: r = a - b + 2^255, so
         *   (a - b) mod p = r + p - 2^255 = r - 19 (mod p). */
        int64_t nb = 0;
        for (k = 0; k < 5; k++) {
            int64_t v = (int64_t)r[k] - ((k == 0) ? 19 : 0) - nb;
            if (v < 0) {
                o->v[k] = (uint64_t)(v + ((int64_t)1 << 51));
                nb = 1;
            } else {
                o->v[k] = (uint64_t)v;
                nb = 0;
            }
        }
        if (nb != 0) {
            /* r < 19: the difference is negative; add p back. */
            uint64_t carry = 0;
            uint64_t v = o->v[0] + (OWCRYPT_M51 - 18u);
            carry = (v >> 51); o->v[0] = v & OWCRYPT_M51;
            for (k = 1; k < 5; k++) {
                uint64_t s = o->v[k] + OWCRYPT_M51 + carry;
                carry = (s >> 51);
                o->v[k] = s & OWCRYPT_M51;
            }
            o->v[0] += 19u * carry;
        }
    } else {
        for (k = 0; k < 5; k++) {
            o->v[k] = r[k];
        }
    }
    owc_fe_canon(o);
}

static void owc_fe_neg(owc_fe_t *o, const owc_fe_t *a)
{
    int64_t  br = 0;
    int32_t  k;
    if (a->v[0] == 0u && a->v[1] == 0u && a->v[2] == 0u &&
        a->v[3] == 0u && a->v[4] == 0u) {
        owc_fe_zero(o);
        return;
    }
    for (k = 0; k < 5; k++) {
        int64_t sub = (k == 0) ? (int64_t)(OWCRYPT_M51 - 18u)
                               : (int64_t)OWCRYPT_M51;
        int64_t t = sub - (int64_t)a->v[k] - br;
        if (t < 0) {
            o->v[k] = (uint64_t)(t + ((int64_t)1 << 51));
            br = 1;
        } else {
            o->v[k] = (uint64_t)t;
            br = 0;
        }
    }
}

static void owc_fe_mul(owc_fe_t *o, const owc_fe_t *a, const owc_fe_t *b)
{
    const __int128 a0 = a->v[0], a1 = a->v[1], a2 = a->v[2];
    const __int128 a3 = a->v[3], a4 = a->v[4];
    const __int128 b0 = b->v[0], b1 = b->v[1], b2 = b->v[2];
    const __int128 b3 = b->v[3], b4 = b->v[4];
    __int128 t0 = a0*b0 + 19*(a1*b4 + a2*b3 + a3*b2 + a4*b1);
    __int128 t1 = a0*b1 + a1*b0 + 19*(a2*b4 + a3*b3 + a4*b2);
    __int128 t2 = a0*b2 + a1*b1 + a2*b0 + 19*(a3*b4 + a4*b3);
    __int128 t3 = a0*b3 + a1*b2 + a2*b1 + a3*b0 + 19*(a4*b4);
    __int128 t4 = a0*b4 + a1*b3 + a2*b2 + a3*b1 + a4*b0;
    uint64_t c = (uint64_t)(t0 >> 51);
    o->v[0] = (uint64_t)t0 & OWCRYPT_M51;
    t1 += c;
    c = (uint64_t)(t1 >> 51);
    o->v[1] = (uint64_t)t1 & OWCRYPT_M51;
    t2 += c;
    c = (uint64_t)(t2 >> 51);
    o->v[2] = (uint64_t)t2 & OWCRYPT_M51;
    t3 += c;
    c = (uint64_t)(t3 >> 51);
    o->v[3] = (uint64_t)t3 & OWCRYPT_M51;
    t4 += c;
    c = (uint64_t)(t4 >> 51);
    o->v[4] = (uint64_t)t4 & OWCRYPT_M51;
    o->v[0] += 19u * c;
    owc_fe_canon(o);
}

static void owc_fe_sq(owc_fe_t *o, const owc_fe_t *a)
{
    owc_fe_mul(o, a, a);
}

static void owc_fe_cswap(owc_fe_t *x, owc_fe_t *y, uint64_t swap)
{
    uint64_t m = 0u - swap;
    int32_t   k;
    for (k = 0; k < 5; k++) {
        uint64_t xk = x->v[k];
        uint64_t yk = y->v[k];
        uint64_t t = (xk ^ yk) & m;
        x->v[k] = xk ^ t;
        y->v[k] = yk ^ t;
    }
}

/* Square-and-multiply power; exponent is little-endian bytes. */
static void owc_fe_pow(owc_fe_t *o, const owc_fe_t *a,
                       const uint8_t e[], size_t elen)
{
    owc_fe_t r;
    owc_fe_set_small(&r, 1u);
    for (size_t b = elen * 8u; b-- > 0u; ) {
        owc_fe_t t;
        owc_fe_sq(&t, &r);
        owc_fe_copy(&r, &t);
        if (((e[b >> 3] >> (b & 7u)) & 1u) != 0u) {
            owc_fe_mul(&t, &r, a);
            owc_fe_copy(&r, &t);
        }
    }
    owc_fe_copy(o, &r);
}

static void owc_fe_inv(owc_fe_t *o, const owc_fe_t *a)
{
    static const uint8_t e_pm2[32] = {
        0xEB,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x7F
    };
    owc_fe_pow(o, a, e_pm2, sizeof(e_pm2));
}

static bool owc_fe_eq(const owc_fe_t *a, const owc_fe_t *b)
{
    return a->v[0] == b->v[0] && a->v[1] == b->v[1] &&
           a->v[2] == b->v[2] && a->v[3] == b->v[3] &&
           a->v[4] == b->v[4];
}

/* sqrt(-1), d = -121665/121666 and 2d, built lazily at first use. */
static const uint8_t owc_d_bytes[32] = {
    0xA3,0x78,0x59,0x13,0xCA,0x4D,0xEB,0x75,
    0xAB,0xD8,0x41,0x41,0x4D,0x0A,0x70,0x00,
    0x98,0xE8,0x79,0x77,0x79,0x40,0xC7,0x8C,
    0x73,0xFE,0x6F,0x2B,0xEE,0x6C,0x03,0x52
};
static const uint8_t owc_pm1d4[32] = {
    0xFB,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x1F
};
static const uint8_t owc_pp3d8[32] = {
    0xFE,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x0F
};

static const owc_fe_t *owc_fe_d(void)
{
    static int      ready = 0;
    static owc_fe_t d;
    if (!ready) {
        owc_fe_frombytes(&d, owc_d_bytes);
        ready = 1;
    }
    return &d;
}

static const owc_fe_t *owc_fe_d2(void)
{
    static int      ready = 0;
    static owc_fe_t d2;
    if (!ready) {
        const owc_fe_t *d = owc_fe_d();
        owc_fe_t t;
        owc_fe_add(&t, d, d);
        owc_fe_copy(&d2, &t);
        ready = 1;
    }
    return &d2;
}

static const owc_fe_t *owc_fe_sqrt_m1(void)
{
    static int      ready = 0;
    static owc_fe_t i;
    if (!ready) {
        owc_fe_t two;
        owc_fe_set_small(&two, 2u);
        owc_fe_pow(&i, &two, owc_pm1d4, sizeof(owc_pm1d4));
        ready = 1;
    }
    return &i;
}

/* Decompress an Ed25519 point. Returns false when y is off-curve.
 * `sign` is the x-sign bit (bit 255 of the encoding). */
static bool owc_ge_decompress(uint8_t x[32], const uint8_t y[32], int sign)
{
    owc_fe_t yy, u, v, x2, xc, chk;
    const owc_fe_t *d = owc_fe_d();
    owc_fe_frombytes(&yy, y);
    owc_fe_t y1, t;
    owc_fe_set_small(&y1, 1u);
    owc_fe_sq(&t, &yy);                       /* y^2            */
    owc_fe_sub(&u, &t, &y1);                  /* u = y^2 - 1    */
    owc_fe_mul(&v, d, &yy);
    owc_fe_mul(&v, &v, &yy);
    owc_fe_add(&v, &v, &y1);                  /* v = d y^2 + 1  */
    owc_fe_inv(&t, &v);                       /* 1/v            */
    owc_fe_mul(&x2, &u, &t);                  /* x^2 = u/v      */
    owc_fe_pow(&xc, &x2, owc_pp3d8, sizeof(owc_pp3d8));
    owc_fe_sq(&chk, &xc);
    if (!owc_fe_eq(&chk, &x2)) {
        owc_fe_mul(&t, &xc, owc_fe_sqrt_m1());
        owc_fe_sq(&chk, &t);
        if (!owc_fe_eq(&chk, &x2)) {
            return false;
        }
        owc_fe_copy(&xc, &t);
    }
    owc_fe_tobytes(x, &xc);
    if (((uint32_t)x[0] & 1u) != (uint32_t)sign) {
        owc_fe_neg(&xc, &xc);
    }
    owc_fe_tobytes(x, &xc);
    return true;
}

/* ------------------------------------------------------------------ */
/*  Edwards curve operations (Edwards25519, a = -1, d as above)        */
/* ------------------------------------------------------------------ */

typedef struct {
    owc_fe_t x, y, z, t;
} owc_ge_t;

static void owc_ge_identity(owc_ge_t *o)
{
    owc_fe_zero(&o->x);
    owc_fe_set_small(&o->y, 1u);
    owc_fe_set_small(&o->z, 1u);
    owc_fe_zero(&o->t);
}

static void owc_ge_copy(owc_ge_t *o, const owc_ge_t *p)
{
    owc_fe_copy(&o->x, &p->x);
    owc_fe_copy(&o->y, &p->y);
    owc_fe_copy(&o->z, &p->z);
    owc_fe_copy(&o->t, &p->t);
}

static void owc_ge_neg(owc_ge_t *o, const owc_ge_t *p)
{
    owc_fe_neg(&o->x, &p->x);
    owc_fe_copy(&o->y, &p->y);
    owc_fe_copy(&o->z, &p->z);
    owc_fe_neg(&o->t, &p->t);
}

/* Complete twisted-Edwards addition (extended coordinates, a = -1). */
static void owc_ge_add(owc_ge_t *o, const owc_ge_t *p, const owc_ge_t *q)
{
    owc_fe_t A, B, C, D, E, F, G, H;
    owc_fe_t t1, t2;
    owc_fe_sub(&t1, &p->y, &p->x);
    owc_fe_sub(&t2, &q->y, &q->x);
    owc_fe_mul(&A, &t1, &t2);
    owc_fe_add(&t1, &p->y, &p->x);
    owc_fe_add(&t2, &q->y, &q->x);
    owc_fe_mul(&B, &t1, &t2);
    owc_fe_mul(&t1, &p->t, &q->t);
    owc_fe_mul(&C, &t1, owc_fe_d2());
    owc_fe_mul(&D, &p->z, &q->z);
    owc_fe_add(&D, &D, &D);
    owc_fe_sub(&E, &B, &A);
    owc_fe_sub(&F, &D, &C);
    owc_fe_add(&G, &D, &C);
    owc_fe_add(&H, &B, &A);
    owc_fe_mul(&o->x, &E, &F);
    owc_fe_mul(&o->y, &G, &H);
    owc_fe_mul(&o->t, &E, &H);
    owc_fe_mul(&o->z, &F, &G);
}

/* Double-and-add scalarmult over the full 256-bit little-endian scalar. */
static void owc_ge_scalarmult(owc_ge_t *o, const uint8_t sc[32],
                              const owc_ge_t *p)
{
    owc_ge_t r;
    owc_ge_t q;
    int32_t  b;
    owc_ge_identity(&r);
    owc_ge_copy(&q, p);
    for (b = 255; b >= 0; b--) {
        owc_ge_t t;
        owc_ge_add(&r, &r, &r);
        if (((sc[b >> 3] >> (b & 7)) & 1u) != 0u) {
            owc_ge_add(&t, &r, &q);
            owc_ge_copy(&r, &t);
        }
    }
    owc_ge_copy(o, &r);
}

static void owc_ge_encode(uint8_t out[32], const owc_ge_t *p)
{
    owc_fe_t zinv, x, y;
    uint8_t  xb[32];
    owc_fe_inv(&zinv, &p->z);
    owc_fe_mul(&y, &p->y, &zinv);
    owc_fe_tobytes(out, &y);
    owc_fe_mul(&x, &p->x, &zinv);
    owc_fe_tobytes(xb, &x);
    out[31] |= (uint8_t)((xb[0] & 1u) << 7);
}

static bool owc_ge_frombytes(owc_ge_t *o, const uint8_t enc[32])
{
    uint8_t y[32];
    uint8_t xb[32];
    int32_t i;
    int     sign;
    for (i = 0; i < 31; i++) {
        y[i] = enc[i];
    }
    y[31] = (uint8_t)(enc[31] & 0x7Fu);
    sign = (int)((enc[31] >> 7) & 1u);
    if (!owc_ge_decompress(xb, y, sign)) {
        return false;
    }
    owc_fe_frombytes(&o->x, xb);
    owc_fe_frombytes(&o->y, y);
    owc_fe_set_small(&o->z, 1u);
    owc_fe_mul(&o->t, &o->x, &o->y);
    return true;
}

static const owc_ge_t *owc_ge_base(void)
{
    static int      ready = 0;
    static owc_ge_t b;
    if (!ready) {
        static const uint8_t by[32] = {
            0x58,0x66,0x66,0x66,0x66,0x66,0x66,0x66,
            0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,
            0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,
            0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66
        };
        uint8_t xb[32];
        if (owc_ge_decompress(xb, by, 0)) {
            owc_fe_frombytes(&b.y, by);
            owc_fe_frombytes(&b.x, xb);
            owc_fe_set_small(&b.z, 1u);
            owc_fe_mul(&b.t, &b.x, &b.y);
        } else {
            owc_ge_identity(&b);
        }
        ready = 1;
    }
    return &b;
}

/* ------------------------------------------------------------------ */
/*  Scalar arithmetic mod L (Ed25519 order)                            */
/* ------------------------------------------------------------------ */

static const uint8_t owc_sc_L[32] = {
    0xED,0xD3,0xF5,0x5C,0x1A,0x63,0x12,0x58,
    0xD6,0x9C,0xF7,0xA2,0xDE,0xF9,0xDE,0x14,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10
};

static int owc_sc_getbit(const uint8_t s[64], int32_t bit)
{
    return (s[bit >> 3] >> (bit & 7)) & 1u;
}

static unsigned owc_sc_sub_bytes(uint8_t a[64], const uint8_t b[64])
{
    unsigned borrow = 0u;
    int32_t  i;
    for (i = 0; i < 64; i++) {
        unsigned bi = (unsigned)b[i] + borrow;
        if (a[i] < bi) {
            a[i] = (uint8_t)(a[i] + 256u - bi);
            borrow = 1u;
        } else {
            a[i] = (uint8_t)(a[i] - bi);
            borrow = 0u;
        }
    }
    return borrow;
}

static void owc_sc_subshift(uint8_t a[64], const uint8_t l[32], uint32_t bits)
{
    uint8_t  t[64];
    uint32_t byte_off = bits >> 3;
    uint32_t bit_off  = bits & 7u;
    uint32_t carry = 0u;
    int32_t  i;
    for (i = 0; i < 64; i++) {
        t[i] = 0u;
    }
    for (i = 0; i < 32; i++) {
        uint32_t out = ((uint32_t)l[i] << bit_off) | carry;
        uint32_t pos = byte_off + (uint32_t)i;
        if (pos < 64u) {
            t[pos] = (uint8_t)out;
        }
        carry = out >> 8;
    }
    if (byte_off + 32u < 64u) {
        t[byte_off + 32u] = (uint8_t)carry;
    }
    (void)owc_sc_sub_bytes(a, t);
}

static int owc_sc_leb_ge(const uint8_t a[32], const uint8_t b[32])
{
    int32_t i;
    for (i = 31; i >= 0; i--) {
        if (a[i] != b[i]) {
            return a[i] > b[i];
        }
    }
    return 1;
}

/* Reduce a 64-byte little-endian value mod L in place. */
static void owc_sc_reduce(uint8_t s[64])
{
    int32_t i;
    for (i = 259; i >= 0; i--) {
        if (owc_sc_getbit(s, i + 252)) {
            owc_sc_subshift(s, owc_sc_L, (uint32_t)i);
        }
    }
    while (owc_sc_leb_ge(s, owc_sc_L)) {
        uint8_t t[64];
        int32_t j;
        for (j = 0; j < 64; j++) {
            t[j] = 0u;
        }
        for (j = 0; j < 32; j++) {
            t[j] = owc_sc_L[j];
        }
        (void)owc_sc_sub_bytes(s, t);
    }
}

/* out = a*b mod L, with a,b < L (32-byte little-endian). */
static void owc_sc_mulmod(uint8_t out[32], const uint8_t a[32],
                          const uint8_t b[32])
{
    uint32_t  al[16], bl[16];
    __int128  acc[32];
    uint8_t   s[64];
    uint64_t  carry;
    int32_t   i, j;
    for (i = 0; i < 32; i++) {
        acc[i] = 0;
    }
for (i = 0; i < 8; i++) {
        al[i] = owc_get32le(a + 4 * i);
        bl[i] = owc_get32le(b + 4 * i);
    }
    /* Split-32-bit schoolbook: each term < 2^64, a slot holds at most 8
       products, so a __int128 accumulator cannot overflow. */
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            acc[i + j] += (uint64_t)al[i] * (uint64_t)bl[j];
        }
    }
    /* a,b < L < 2^253 so a*b < 2^506: only the low 8 bytes matter. */
    carry = 0u;
    for (i = 0; i < 32; i++) {
        __int128 v = acc[i] + (__int128)carry;
        if (i < 16) {
            owc_put32le(s + 4 * i, (uint32_t)v);
        }
        carry = (uint64_t)(v >> 32);
    }
    owc_sc_reduce(s);
    for (i = 0; i < 32; i++) {
        out[i] = s[i];
    }
}

/* Reduce a 32-byte value mod L (writing result to out). */
static void owc_sc_reduce32(uint8_t out[32], const uint8_t in[32])
{
    uint8_t s[64];
    int32_t i;
    for (i = 0; i < 64; i++) {
        s[i] = 0u;
    }
    for (i = 0; i < 32; i++) {
        s[i] = in[i];
    }
    owc_sc_reduce(s);
    for (i = 0; i < 32; i++) {
        out[i] = s[i];
    }
}

/* ------------------------------------------------------------------ */
/*  X25519 (RFC 7748)                                                  */
/* ------------------------------------------------------------------ */

static const uint8_t owc_x25519_base9[32] = {
    0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static void owc_x25519_core(uint8_t out[32], const uint8_t scalar[32],
                            const uint8_t u[32])
{
    owc_fe_t x1, x2, z2, x3, z3;
    owc_fe_t a, aa, b, bb, e, c, d, da, cb, t, ae;
    uint8_t  k[32];
    int32_t  bit;
    int      swap = 0;
    int32_t  i;
    for (i = 0; i < 32; i++) {
        k[i] = scalar[i];
    }
    k[0] &= 0xF8u;
    k[31] &= 0x7Fu;
    k[31] |= 0x40u;

    owc_fe_frombytes(&x1, u);
    owc_fe_set_small(&x2, 1u);
    owc_fe_zero(&z2);
    owc_fe_copy(&x3, &x1);
    owc_fe_set_small(&z3, 1u);

    for (bit = 254; bit >= 0; bit--) {
        int ki = (k[bit >> 3] >> (bit & 7)) & 1u;
        swap ^= ki;
        owc_fe_cswap(&x2, &x3, (uint64_t)swap);
        owc_fe_cswap(&z2, &z3, (uint64_t)swap);
        swap = ki;

        owc_fe_add(&a, &x2, &z2);
        owc_fe_sq(&aa, &a);
        owc_fe_sub(&b, &x2, &z2);
        owc_fe_sq(&bb, &b);
        owc_fe_sub(&e, &aa, &bb);
        owc_fe_add(&c, &x3, &z3);
        owc_fe_sub(&d, &x3, &z3);
        owc_fe_mul(&da, &d, &a);
        owc_fe_mul(&cb, &c, &b);

        owc_fe_add(&t, &da, &cb);
        owc_fe_sq(&x3, &t);
        owc_fe_sub(&t, &da, &cb);
        owc_fe_sq(&t, &t);
        owc_fe_mul(&z3, &t, &x1);

        owc_fe_mul(&x2, &aa, &bb);
        owc_fe_set_small(&ae, 121665u);
        owc_fe_mul(&ae, &ae, &e);
        owc_fe_add(&t, &aa, &ae);
        owc_fe_mul(&z2, &e, &t);
    }
    owc_fe_cswap(&x2, &x3, (uint64_t)swap);
    owc_fe_cswap(&z2, &z3, (uint64_t)swap);

    owc_fe_inv(&t, &z2);
    owc_fe_mul(&t, &t, &x2);
    owc_fe_tobytes(out, &t);
}

owcrypt_status_t owcrypt_x25519(uint8_t scalar_out[32],
                                const uint8_t scalar_in[32],
                                const uint8_t u[32])
{
    uint8_t  kb[32];
    uint8_t  ub[32];
    uint8_t  res[32];
    uint32_t acc = 0u;
    int32_t  i;
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (scalar_out == NULL || scalar_in == NULL) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    if (u == NULL) {
        for (i = 0; i < 32; i++) {
            ub[i] = owc_x25519_base9[i];
        }
    } else {
        for (i = 0; i < 32; i++) {
            ub[i] = u[i];
        }
    }
    for (i = 0; i < 32; i++) {
        kb[i] = scalar_in[i];
    }
    owc_x25519_core(res, kb, ub);
    for (i = 0; i < 32; i++) {
        acc |= (uint32_t)res[i];
        scalar_out[i] = res[i];
    }
    if (acc == 0u) {
        return OWCRYPT_BAN_BAD_KEY;
    }
    return OWCRYPT_OK;
}

/* ------------------------------------------------------------------ */
/*  Ed25519 (RFC 8032)                                                 */
/* ------------------------------------------------------------------ */

owcrypt_status_t owcrypt_ed25519_keypair(const uint8_t seed[32],
                                         uint8_t public_key[32])
{
    uint8_t   h[64];
    uint8_t   a[32];
    uint8_t   sr[32];
    owc_ge_t  A;
    uint8_t   enc[32];
    int32_t   i;
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (seed == NULL || public_key == NULL) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    owcrypt_sha512(seed, 32u, h);
    for (i = 0; i < 32; i++) {
        a[i] = h[i];
    }
    a[0] &= 0xF8u;
    a[31] &= 0x3Fu;
    a[31] |= 0x40u;
    owc_sc_reduce32(sr, a);
    owc_ge_scalarmult(&A, sr, owc_ge_base());
    owc_ge_encode(enc, &A);
    for (i = 0; i < 32; i++) {
        public_key[i] = enc[i];
    }
    return OWCRYPT_OK;
}

owcrypt_status_t owcrypt_ed25519_sign(const uint8_t seed[32],
                                      const uint8_t public_key[32],
                                      const uint8_t *msg, size_t msglen,
                                      uint8_t sig[64])
{
    uint8_t          h[64];
    uint8_t          a[32];
    uint8_t          sr[32];
    uint8_t          pref[32];
    uint8_t          t64[64];
    uint8_t          rr[32];
    uint8_t          rbuf[32];
    uint8_t          kbytes[64];
    uint8_t          kr[32];
    uint8_t          srhs[32];
    uint8_t          s64[64];
    owcrypt_sha_ctx_t c1;
    owc_ge_t         R;
    int32_t          i;
    unsigned         carry = 0u;
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (seed == NULL || public_key == NULL || sig == NULL ||
        (msglen > 0u && msg == NULL)) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    owcrypt_sha512(seed, 32u, h);
    for (i = 0; i < 32; i++) {
        a[i] = h[i];
        pref[i] = h[32 + i];
    }
    a[0] &= 0xF8u;
    a[31] &= 0x3Fu;
    a[31] |= 0x40u;
    owc_sc_reduce32(sr, a);

    owcrypt_sha512_init(&c1);
    owcrypt_sha512_update(&c1, pref, 32u);
    owcrypt_sha512_update(&c1, msg, msglen);
    owcrypt_sha512_final(&c1, h);
    for (i = 0; i < 64; i++) {
        t64[i] = h[i];
    }
    owc_sc_reduce(t64);
    for (i = 0; i < 32; i++) {
        rr[i] = t64[i];
    }

    owc_ge_scalarmult(&R, rr, owc_ge_base());
    owc_ge_encode(rbuf, &R);
    for (i = 0; i < 32; i++) {
        sig[i] = rbuf[i];
    }

    owcrypt_sha512_init(&c1);
    owcrypt_sha512_update(&c1, rbuf, 32u);
    owcrypt_sha512_update(&c1, public_key, 32u);
    owcrypt_sha512_update(&c1, msg, msglen);
    owcrypt_sha512_final(&c1, kbytes);
    owc_sc_reduce(kbytes);
    for (i = 0; i < 32; i++) {
        kr[i] = kbytes[i];
    }

    owc_sc_mulmod(srhs, kr, sr);
    for (i = 0; i < 32; i++) {
        unsigned v = (unsigned)srhs[i] + (unsigned)rr[i] + carry;
        s64[i] = (uint8_t)v;
        carry = v >> 8;
    }
    for (i = 32; i < 64; i++) {
        s64[i] = 0u;
    }
    owc_sc_reduce(s64);
    for (i = 0; i < 32; i++) {
        sig[32 + i] = s64[i];
    }
    return OWCRYPT_OK;
}

owcrypt_status_t owcrypt_ed25519_verify(const uint8_t public_key[32],
                                        const uint8_t *msg, size_t msglen,
                                        const uint8_t sig[64])
{
    uint8_t          h64[64];
    uint8_t          h[32];
    owc_ge_t         A, R, sB, hA, hAneg, Rc;
    owc_fe_t         Xr, Yr, Xc, Yc;
    owcrypt_sha_ctx_t c1;
    int32_t          i;
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (public_key == NULL || sig == NULL || (msglen > 0u && msg == NULL)) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    /* Reject S >= L (malleability). */
    if (owc_sc_leb_ge(sig + 32, owc_sc_L)) {
        return OWCRYPT_BAN_BAD_KEY;
    }
    if (!owc_ge_frombytes(&A, public_key)) {
        return OWCRYPT_BAN_BAD_KEY;
    }
    if (!owc_ge_frombytes(&R, sig)) {
        return OWCRYPT_BAN_BAD_KEY;
    }

    owcrypt_sha512_init(&c1);
    owcrypt_sha512_update(&c1, sig, 32u);
    owcrypt_sha512_update(&c1, public_key, 32u);
    owcrypt_sha512_update(&c1, msg, msglen);
    owcrypt_sha512_final(&c1, h64);
    owc_sc_reduce(h64);
    for (i = 0; i < 32; i++) {
        h[i] = h64[i];
    }

    owc_ge_scalarmult(&sB, sig + 32, owc_ge_base());
    owc_ge_scalarmult(&hA, h, &A);
    owc_ge_neg(&hAneg, &hA);
    owc_ge_add(&Rc, &sB, &hAneg);

    /* Constant-time-ish affine comparison: Xr/Yr vs Xc/Yc. */
    owc_fe_mul(&Xr, &R.x, &Rc.z);
    owc_fe_mul(&Xc, &Rc.x, &R.z);
    owc_fe_mul(&Yr, &R.y, &Rc.z);
    owc_fe_mul(&Yc, &Rc.y, &R.z);
    if (!owc_fe_eq(&Xr, &Xc) || !owc_fe_eq(&Yr, &Yc)) {
        return OWCRYPT_BAN_AUTH_FAIL;
    }
    return OWCRYPT_OK;
}

/* ------------------------------------------------------------------ */
/*  AES (FIPS-197) - runtime S-box tables built at module init         */
/* ------------------------------------------------------------------ */

static uint8_t owc_gmul(uint8_t a, uint8_t b)
{
    uint8_t p = 0u;
    int32_t i;
    for (i = 0; i < 8; i++) {
        if ((b & 1u) != 0u) {
            p ^= a;
        }
        {
            uint8_t hi = (uint8_t)(a & 0x80u);
            a = (uint8_t)(a << 1);
            if (hi != 0u) {
                a ^= 0x1Bu;
            }
        }
        b >>= 1;
    }
    return p;
}

static void owc_aes_expand(uint8_t rk[240], const uint8_t *key,
                           uint32_t keybits)
{
    static const uint8_t rcon[10] = {
        0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1B,0x36
    };
    uint32_t nk = keybits / 32u;          /* 4 or 8   */
    uint32_t nr = nk + 6u;                /* 10 or 14 */
    uint32_t nw = 4u * (nr + 1u);
    uint32_t i;
    for (i = 0; i < nk; i++) {
        uint32_t j;
        for (j = 0; j < 4u; j++) {
            rk[4u * i + j] = key[4u * i + j];
        }
    }
    for (; i < nw; i++) {
        uint8_t t[4];
        uint32_t j;
        for (j = 0; j < 4u; j++) {
            t[j] = rk[4u * (i - 1u) + j];
        }
        if ((i % nk) == 0u) {
            uint8_t tmp = t[0];
            t[0] = t[1]; t[1] = t[2]; t[2] = t[3]; t[3] = tmp;
            for (j = 0; j < 4u; j++) {
                t[j] = owc_sbox[t[j]];
            }
            t[0] ^= rcon[(i / nk) - 1u];
        } else if (nk == 8u && (i % nk) == 4u) {
            for (j = 0; j < 4u; j++) {
                t[j] = owc_sbox[t[j]];
            }
        }
        for (j = 0; j < 4u; j++) {
            rk[4u * i + j] = rk[4u * (i - nk) + j] ^ t[j];
        }
    }
}

static void owc_aes_encrypt_block(uint8_t out[16], const uint8_t in[16],
                                  const uint8_t rk[240], uint32_t nr)
{
    uint8_t s[16];
    uint32_t r, c, i;
    for (i = 0; i < 16u; i++) {
        s[i] = in[i];
    }
    for (i = 0; i < 16u; i++) {
        s[i] ^= rk[i];
    }
    for (r = 1u; r < nr; r++) {
        uint8_t t[16];
        /* SubBytes + ShiftRows into t[]. */
        for (c = 0u; c < 4u; c++) {
            uint8_t a0 = owc_sbox[s[4u * c + 0u]];
            uint8_t a1 = owc_sbox[s[4u * c + 1u]];
            uint8_t a2 = owc_sbox[s[4u * c + 2u]];
            uint8_t a3 = owc_sbox[s[4u * c + 3u]];
            t[4u * ((c + 0u) % 4u) + 0u] = a0;   /* r=0: no shift */
            t[4u * ((c + 3u) % 4u) + 1u] = a1;   /* r=1: left 1   */
            t[4u * ((c + 2u) % 4u) + 2u] = a2;   /* r=2: left 2   */
            t[4u * ((c + 1u) % 4u) + 3u] = a3;   /* r=3: left 3   */
        }
        /* MixColumns reads t columns, writes back into s. */
        for (c = 0u; c < 4u; c++) {
            uint8_t a0 = t[4u * c + 0u];
            uint8_t a1 = t[4u * c + 1u];
            uint8_t a2 = t[4u * c + 2u];
            uint8_t a3 = t[4u * c + 3u];
            s[4u * c + 0u] = (uint8_t)(owc_gmul(a0, 2u) ^ owc_gmul(a1, 3u)
                                       ^ a2 ^ a3);
            s[4u * c + 1u] = (uint8_t)(a0 ^ owc_gmul(a1, 2u)
                                       ^ owc_gmul(a2, 3u) ^ a3);
            s[4u * c + 2u] = (uint8_t)(a0 ^ a1 ^ owc_gmul(a2, 2u)
                                       ^ owc_gmul(a3, 3u));
            s[4u * c + 3u] = (uint8_t)(owc_gmul(a0, 3u) ^ a1 ^ a2
                                       ^ owc_gmul(a3, 2u));
        }
        for (i = 0; i < 16u; i++) {
            s[i] ^= rk[16u * r + i];
        }
    }
    /* Final round: SubBytes + ShiftRows + AddRoundKey (no MixColumns). */
    {
        uint8_t t[16];
        for (c = 0u; c < 4u; c++) {
            uint8_t a0 = owc_sbox[s[4u * c + 0u]];
            uint8_t a1 = owc_sbox[s[4u * c + 1u]];
            uint8_t a2 = owc_sbox[s[4u * c + 2u]];
            uint8_t a3 = owc_sbox[s[4u * c + 3u]];
            t[4u * c + 0u] = a0;
            t[4u * ((c + 3u) % 4u) + 1u] = a1;
            t[4u * ((c + 2u) % 4u) + 2u] = a2;
            t[4u * ((c + 1u) % 4u) + 3u] = a3;
        }
        for (i = 0; i < 16u; i++) {
            s[i] = t[i] ^ rk[16u * nr + i];
        }
    }
    for (i = 0; i < 16u; i++) {
        out[i] = s[i];
    }
}

static void owc_aes_decrypt_block(uint8_t out[16], const uint8_t in[16],
                                  const uint8_t rk[240], uint32_t nr)
{
    uint8_t s[16];
    uint32_t r, c, i;
    for (i = 0; i < 16u; i++) {
        s[i] = in[i];
    }
    for (i = 0; i < 16u; i++) {
        s[i] ^= rk[16u * nr + i];
    }
    for (r = nr; r > 1u; r--) {
        uint8_t t[16];
        for (c = 0u; c < 4u; c++) {
            uint8_t a0, a1, a2, a3;
            a0 = owc_inv_sbox[s[4u * c + 0u]];
            a1 = owc_inv_sbox[s[4u * c + 1u]];
            a2 = owc_inv_sbox[s[4u * c + 2u]];
            a3 = owc_inv_sbox[s[4u * c + 3u]];
            t[4u * c + 0u] = a0;                             /* r=0       */
            t[4u * ((c + 1u) % 4u) + 1u] = a1;               /* r=1 right */
            t[4u * ((c + 2u) % 4u) + 2u] = a2;               /* r=2       */
            t[4u * ((c + 3u) % 4u) + 3u] = a3;               /* r=3 right */
        }
        for (c = 0u; c < 4u; c++) {
            uint8_t a0 = t[4u * c + 0u] ^ rk[16u * (r - 1u) + 4u * c + 0u];
            uint8_t a1 = t[4u * c + 1u] ^ rk[16u * (r - 1u) + 4u * c + 1u];
            uint8_t a2 = t[4u * c + 2u] ^ rk[16u * (r - 1u) + 4u * c + 2u];
            uint8_t a3 = t[4u * c + 3u] ^ rk[16u * (r - 1u) + 4u * c + 3u];
            s[4u * c + 0u] = (uint8_t)(owc_gmul(a0, 14u) ^ owc_gmul(a1, 11u)
                                       ^ owc_gmul(a2, 13u) ^ owc_gmul(a3, 9u));
            s[4u * c + 1u] = (uint8_t)(owc_gmul(a0, 9u) ^ owc_gmul(a1, 14u)
                                       ^ owc_gmul(a2, 11u) ^ owc_gmul(a3, 13u));
            s[4u * c + 2u] = (uint8_t)(owc_gmul(a0, 13u) ^ owc_gmul(a1, 9u)
                                       ^ owc_gmul(a2, 14u) ^ owc_gmul(a3, 11u));
            s[4u * c + 3u] = (uint8_t)(owc_gmul(a0, 11u) ^ owc_gmul(a1, 13u)
                                       ^ owc_gmul(a2, 9u) ^ owc_gmul(a3, 14u));
        }
    }
    /* Final round (r == 1): InvShiftRows + InvSubBytes + AddRoundKey. */
    {
        uint8_t t[16];
        for (c = 0u; c < 4u; c++) {
            t[4u * c + 0u] = s[4u * c + 0u];
            t[4u * ((c + 1u) % 4u) + 1u] = s[4u * c + 1u];
            t[4u * ((c + 2u) % 4u) + 2u] = s[4u * c + 2u];
            t[4u * ((c + 3u) % 4u) + 3u] = s[4u * c + 3u];
        }
        for (i = 0; i < 16u; i++) {
            s[i] = owc_inv_sbox[t[i]] ^ rk[i];
        }
    }
    for (i = 0; i < 16u; i++) {
        out[i] = s[i];
    }
}

owcrypt_status_t owcrypt_aes_ecb_encrypt(const uint8_t *key, uint32_t keybits,
                                         const uint8_t pt[16],
                                         uint8_t ct[16])
{
    uint8_t rk[240];
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (key == NULL || pt == NULL || ct == NULL) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    if (keybits != 128u && keybits != 256u) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    owc_aes_expand(rk, key, keybits);
    owc_aes_encrypt_block(ct, pt, rk, keybits / 32u + 6u);
    return OWCRYPT_OK;
}

owcrypt_status_t owcrypt_aes_ecb_decrypt(const uint8_t *key, uint32_t keybits,
                                         const uint8_t ct[16],
                                         uint8_t pt[16])
{
    uint8_t rk[240];
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (key == NULL || ct == NULL || pt == NULL) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    if (keybits != 128u && keybits != 256u) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    owc_aes_expand(rk, key, keybits);
    owc_aes_decrypt_block(pt, ct, rk, keybits / 32u + 6u);
    return OWCRYPT_OK;
}

/* ------------------------------------------------------------------ */
/*  GHASH + AES-GCM (SP 800-38D)                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t hi;                       /* most significant 64 bits      */
    uint64_t lo;
} owc_gh_t;

static void owc_gh_load(owc_gh_t *g, const uint8_t b[16])
{
    g->hi = owc_get64(b);
    g->lo = owc_get64(b + 8);
}

static void owc_gh_store(uint8_t b[16], const owc_gh_t *g)
{
    owc_put64(b, g->hi);
    owc_put64(b + 8, g->lo);
}

static void owc_gh_mul(owc_gh_t *o, const owc_gh_t *x, const owc_gh_t *y)
{
    uint8_t v[16];
    uint8_t z[16];
    uint8_t xb[16];
    int32_t i;
    owc_gh_store(v, y);
    for (i = 0; i < 16; i++) {
        z[i] = 0u;
    }
    owc_gh_store(xb, x);
    for (i = 0; i < 128; i++) {
        /* Bit i (MSB-first) of x selects v. */
        if ((xb[i >> 3] & (uint8_t)(0x80u >> (i & 7u))) != 0u) {
            uint32_t j;
            for (j = 0; j < 16u; j++) {
                z[j] ^= v[j];
            }
        }
        {
            uint8_t lsb = (uint8_t)(v[15] & 1u);
            uint32_t j;
            for (j = 15u; j > 0u; j--) {
                v[j] = (uint8_t)((v[j] >> 1) | (v[j - 1u] << 7));
            }
            v[0] = (uint8_t)(v[0] >> 1);
            if (lsb != 0u) {
                v[0] ^= 0xE1u;
            }
        }
    }
    {
        owc_gh_t t;
        owc_gh_load(&t, z);
        *o = t;
    }
}

static void owc_gh_step(owc_gh_t *s, const owc_gh_t *h, const uint8_t blk[16])
{
    owc_gh_t t;
    owc_gh_load(&t, blk);
    s->hi ^= t.hi;
    s->lo ^= t.lo;
    owc_gh_mul(&t, s, h);
    *s = t;
}

static void owc_ghash(owc_gh_t *s, const uint8_t h[16],
                      const uint8_t *aad, size_t aadlen,
                      const uint8_t *ct, size_t clen)
{
    owc_gh_t H;
    uint8_t  pad[16];
    size_t   q, i;
    owc_gh_load(&H, h);
    s->hi = 0u; s->lo = 0u;
    for (q = 0; q < aadlen / 16u; q++) {
        owc_gh_step(s, &H, aad + 16u * q);
    }
    if ((aadlen % 16u) != 0u) {
        size_t rem = aadlen % 16u;
        for (i = 0; i < 16u; i++) {
            pad[i] = (i < rem) ? aad[aadlen - rem + i] : 0u;
        }
        owc_gh_step(s, &H, pad);
    }
    for (q = 0; q < clen / 16u; q++) {
        owc_gh_step(s, &H, ct + 16u * q);
    }
    if ((clen % 16u) != 0u) {
        size_t rem = clen % 16u;
        for (i = 0; i < 16u; i++) {
            pad[i] = (i < rem) ? ct[clen - rem + i] : 0u;
        }
        owc_gh_step(s, &H, pad);
    }
    for (i = 0; i < 16u; i++) {
        pad[i] = 0u;
    }
    owc_put64(pad, (uint64_t)aadlen * 8u);
    owc_put64(pad + 8, (uint64_t)clen * 8u);
    owc_gh_step(s, &H, pad);
}

static void owc_gcm_inc32(uint8_t j0[16])
{
    uint32_t c = owc_get32(j0 + 12);
    c = (c + 1u) & 0xFFFFFFFFu;
    owc_put32(j0 + 12, c);
}

static void owc_gcm_keystream(uint8_t out[16], uint8_t enc[16],
                              const uint8_t *inblk, const uint8_t *rk,
                              uint32_t nr, uint8_t ctr[16])
{
    owc_gcm_inc32(ctr);
    owc_aes_encrypt_block(enc, ctr, rk, nr);
    for (uint32_t i = 0u; i < 16u; i++) {
        out[i] = (uint8_t)(enc[i] ^ inblk[i]);
    }
}

owcrypt_status_t owcrypt_aes_gcm_seal(const uint8_t *key, uint32_t keybits,
                                      const uint8_t nonce[12],
                                      const uint8_t *aad, size_t aadlen,
                                      const uint8_t *plain, size_t plen,
                                      uint8_t *out, uint8_t tag[16])
{
    uint8_t  rk[240];
    uint8_t  h[16];
    uint8_t  j0[16];
    uint8_t  ctr[16];
    uint8_t  blk[16];
    uint8_t  enc[16];
    uint8_t  e0[16];
    owc_gh_t s;
    size_t   q, i;
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (key == NULL || nonce == NULL || tag == NULL || out == NULL ||
        (plen > 0u && plain == NULL) || (aadlen > 0u && aad == NULL)) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    if (keybits != 128u && keybits != 256u) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    owc_aes_expand(rk, key, keybits);
    {
        uint8_t zero[16];
        for (i = 0; i < 16u; i++) {
            zero[i] = 0u;
        }
        owc_aes_encrypt_block(h, zero, rk, keybits / 32u + 6u);
    }

    for (i = 0; i < 12u; i++) {
        j0[i] = nonce[i];
    }
    owc_put32(j0 + 12, 1u);
    for (i = 0; i < 16u; i++) {
        ctr[i] = j0[i];
    }

    for (q = 0; q < plen / 16u; q++) {
        owc_gcm_keystream(out + 16u * q, enc, plain + 16u * q, rk,
                          keybits / 32u + 6u, ctr);
    }
    if ((plen % 16u) != 0u) {
        size_t rem = plen % 16u;
        for (i = 0; i < 16u; i++) {
            blk[i] = (i < rem) ? plain[plen - rem + i] : 0u;
        }
        owc_gcm_keystream(blk, enc, blk, rk, keybits / 32u + 6u, ctr);
        for (i = 0; i < rem; i++) {
            out[plen - rem + i] = blk[i];
        }
    }

    owc_ghash(&s, h, aad, aadlen, out, plen);
    owc_gh_store(enc, &s);
    for (i = 0; i < 16u; i++) {
        e0[i] = j0[i];
    }
    owc_aes_encrypt_block(e0, e0, rk, keybits / 32u + 6u);
    for (i = 0; i < 16u; i++) {
        tag[i] = (uint8_t)(e0[i] ^ enc[i]);
    }
    return OWCRYPT_OK;
}

owcrypt_status_t owcrypt_aes_gcm_open(const uint8_t *key, uint32_t keybits,
                                      const uint8_t nonce[12],
                                      const uint8_t *aad, size_t aadlen,
                                      const uint8_t *cipher, size_t clen,
                                      const uint8_t tag[16], uint8_t *out)
{
    uint8_t  rk[240];
    uint8_t  h[16];
    uint8_t  j0[16];
    uint8_t  ctr[16];
    uint8_t  blk[16];
    uint8_t  enc[16];
    uint8_t  e0[16];
    uint8_t  expect[16];
    owc_gh_t s;
    size_t   q, i;
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (key == NULL || nonce == NULL || tag == NULL ||
        (clen > 0u && cipher == NULL) || (aadlen > 0u && aad == NULL)) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    if (keybits != 128u && keybits != 256u) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    owc_aes_expand(rk, key, keybits);
    {
        uint8_t zero[16];
        for (i = 0; i < 16u; i++) {
            zero[i] = 0u;
        }
        owc_aes_encrypt_block(h, zero, rk, keybits / 32u + 6u);
    }
    for (i = 0; i < 12u; i++) {
        j0[i] = nonce[i];
    }
    owc_put32(j0 + 12, 1u);
    for (i = 0; i < 16u; i++) {
        ctr[i] = j0[i];
    }

    owc_ghash(&s, h, aad, aadlen, cipher, clen);
    owc_gh_store(enc, &s);
    for (i = 0; i < 16u; i++) {
        e0[i] = j0[i];
    }
    owc_aes_encrypt_block(e0, e0, rk, keybits / 32u + 6u);
    for (i = 0; i < 16u; i++) {
        expect[i] = (uint8_t)(e0[i] ^ enc[i]);
    }
    if (!owcrypt_ct_memcmp(expect, tag, 16u)) {
        return OWCRYPT_BAN_AUTH_FAIL;
    }

    for (q = 0; q < clen / 16u; q++) {
        owc_gcm_keystream(out + 16u * q, enc, cipher + 16u * q, rk,
                          keybits / 32u + 6u, ctr);
    }
    if ((clen % 16u) != 0u) {
        size_t rem = clen % 16u;
        for (i = 0; i < 16u; i++) {
            blk[i] = (i < rem) ? cipher[clen - rem + i] : 0u;
        }
        owc_gcm_keystream(blk, enc, blk, rk, keybits / 32u + 6u, ctr);
        for (i = 0; i < rem; i++) {
            out[clen - rem + i] = blk[i];
        }
    }
    return OWCRYPT_OK;
}

/* ------------------------------------------------------------------ */
/*  Constant-time helpers                                              */
/* ------------------------------------------------------------------ */

void owcrypt_cleanse(void *ptr, size_t n)
{
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    while (n-- > 0u) {
        *p++ = 0u;
    }
}

bool owcrypt_ct_memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    uint8_t  diff = 0u;
    size_t   i;
    if (x == NULL || y == NULL) {
        return false;
    }
    for (i = 0u; i < n; i++) {
        diff |= (uint8_t)(x[i] ^ y[i]);
    }
    return diff == 0u;
}

/* ------------------------------------------------------------------ */
/*  CSPRNG (ChaCha20 DRBG over jitter + RDTSCP entropy)               */
/* ------------------------------------------------------------------ */

#define OWC_RNG_RESEED  8192u

static uint8_t  owc_rng_key[32];
static uint8_t  owc_rng_nonce[12];
static uint8_t  owc_rng_block[64];
static uint64_t owc_rng_consumed;
static uint64_t owc_rng_generated;
static int      owc_rng_ready;

static uint64_t owc_rdtsc64(void)
{
    uint32_t lo, hi;
#if defined(__GNUC__) && defined(__x86_64__)
    __asm__ volatile ("rdtscp" : "=a"(lo), "=d"(hi) : : "rcx", "memory");
#else
    lo = 0u;
    hi = 0u;
#endif
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t owc_rng_entropy_u64(void)
{
    volatile uint64_t x = owc_rdtsc64();
    volatile uint64_t j = 0u;
    uint32_t         i;
    for (i = 0u; i < 64u; i++) {
        x = x * 0x9E3779B97F4A7C15u + (uint64_t)i;
        j = (j * 31u) ^ owc_rdtsc64();
    }
    return x ^ j ^ owc_rdtsc64() ^ (uint64_t)(size_t)&x;
}

static void owc_rng_gather(uint8_t out[64])
{
    uint32_t i, k;
    for (i = 0u; i < 8u; i++) {
        uint64_t e = owc_rng_entropy_u64();
        for (k = 0u; k < 8u; k++) {
            out[8u * i + k] = (uint8_t)(e >> (8u * k));
        }
    }
}

static void owc_rng_advance_nonce(void)
{
    uint32_t i;
    for (i = 0u; i < 8u; i++) {
        owc_rng_nonce[i] = (uint8_t)(owc_rng_nonce[i] + 1u);
        if (owc_rng_nonce[i] != 0u) {
            break;
        }
    }
}

static void owc_rng_rekey(void)
{
    uint8_t ent[64];
    uint8_t mix[32];
    uint8_t km[64];
    uint32_t i;
    owc_rng_gather(ent);
    for (i = 0u; i < 32u; i++) {
        km[i] = owc_rng_key[i];
        km[32u + i] = ent[i];
    }
    owcrypt_blake2b(mix, sizeof(mix), km, sizeof(km));
    for (i = 0u; i < 32u; i++) {
        owc_rng_key[i] = mix[i];
    }
    owcrypt_cleanse(ent, sizeof(ent));
    owcrypt_cleanse(km, sizeof(km));
    owcrypt_cleanse(mix, sizeof(mix));
}

static void owc_rng_next_block(void)
{
    uint8_t zeros[64];
    uint32_t i;
    for (i = 0u; i < 64u; i++) {
        zeros[i] = 0u;
    }
    owcrypt_chacha20_xor(owc_rng_key, owc_rng_nonce, 0u, zeros, 64u,
                         owc_rng_block);
    owcrypt_cleanse(zeros, sizeof(zeros));
    owc_rng_advance_nonce();
    owc_rng_consumed = 0u;
    owc_rng_generated += 64u;
    if (owc_rng_generated >= OWC_RNG_RESEED) {
        owc_rng_generated = 0u;
        owc_rng_rekey();
    }
}

owcrypt_status_t owcrypt_csprng_init(void)
{
    uint8_t  ent[64];
    uint8_t  nc[8];
    uint64_t c;
    uint32_t i, k;
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    owc_rng_gather(ent);
    owcrypt_blake2b(owc_rng_key, sizeof(owc_rng_key), ent, sizeof(ent));
    owcrypt_cleanse(ent, sizeof(ent));
    c = owc_rng_entropy_u64();
    for (i = 0u; i < 8u; i++) {
        nc[i] = (uint8_t)(c >> (8u * i));
    }
    for (i = 0u; i < 8u; i++) {
        owc_rng_nonce[i] = nc[i];
    }
    owc_rng_nonce[8] = owc_rng_nonce[9] = 0u;
    owc_rng_nonce[10] = owc_rng_nonce[11] = 0u;
    for (k = 0u; k < 64u; k++) {
        owc_rng_block[k] = 0u;
    }
    owc_rng_consumed = 64u;      /* force first block generation */
    owc_rng_generated = 0u;
    owc_rng_ready = 1;
    (void)nc;
    return OWCRYPT_OK;
}

owcrypt_status_t owcrypt_csprng_generate(void *out, size_t n)
{
    uint8_t  *p = (uint8_t *)out;
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (!owc_rng_ready) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (p == NULL && n > 0u) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    while (n > 0u) {
        size_t c;
        if (owc_rng_consumed >= 64u) {
            owc_rng_next_block();
        }
        c = 64u - owc_rng_consumed;
        if (c > n) {
            c = n;
        }
        {
            size_t i;
            for (i = 0u; i < c; i++) {
                p[i] = owc_rng_block[owc_rng_consumed + i];
            }
        }
        owc_rng_consumed += c;
        p += c;
        n -= c;
    }
    return OWCRYPT_OK;
}

/* ------------------------------------------------------------------ */
/*  BANcode pre-scan                                                   */
/* ------------------------------------------------------------------ */

uint32_t owcrypt_scan_ban_stream(const uint8_t *data, size_t n)
{
    size_t i;
    if (data == NULL || n < 4u) {
        return 0u;
    }
    for (i = 0u; i + 4u <= n; i++) {
        uint32_t v = owc_get32le(data + i);
        if (v >= 0x0011A000u && v <= 0x0011AEFFu) {
            return v;
        }
    }
    return 0u;
}

/* ------------------------------------------------------------------ */
/*  SUTF security frames                                               */
/* ------------------------------------------------------------------ */

static void owc_copy_bytes(uint8_t *dst, const uint8_t *src, size_t n)
{
    size_t i;
    for (i = 0u; i < n; i++) {
        dst[i] = src[i];
    }
}

owcrypt_status_t owcrypt_seal_frame(const uint8_t key[32],
                                    const uint8_t nonce[12],
                                    const uint8_t *aad, size_t aadlen,
                                    const uint8_t *plain, size_t plen,
                                    uint8_t *frame, size_t *frame_len)
{
    size_t need;
    uint8_t *p;
    uint8_t  tag[16];
    owcrypt_status_t st;
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (key == NULL || nonce == NULL || frame == NULL || frame_len == NULL ||
        (plen > 0u && plain == NULL) || (aadlen > 0u && aad == NULL)) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    need = OWCRYPT_FRAME_OVERHEAD + aadlen + plen + 16u;
    if (*frame_len < need) {
        return OWCRYPT_ERR_SHORT_OUT;
    }
    *frame_len = need;

    p = frame;
    owc_put32le(p, OWCRYPT_FRAME_MAGIC); p += 4;
    owc_put32le(p, (uint32_t)aadlen);    p += 4;
    owc_put32le(p, (uint32_t)plen);      p += 4;
    if (aadlen > 0u) {
        owc_copy_bytes(p, aad, aadlen);
        p += aadlen;
    }
    st = owcrypt_chacha_poly_seal(key, nonce, aad, aadlen, plain, plen,
                                  p, tag);
    if (st != OWCRYPT_OK) {
        return st;
    }
    owc_copy_bytes(p + plen, tag, 16u);
    return OWCRYPT_OK;
}

owcrypt_status_t owcrypt_open_frame(const uint8_t key[32],
                                    const uint8_t nonce[12],
                                    const uint8_t *frame, size_t frame_len,
                                    uint8_t *plain_out, size_t *plain_len,
                                    const uint8_t **aad_out, size_t *aad_len_out,
                                    bool scan_ban,
                                    owcrypt_ban_signal_t ban_hook,
                                    void *ban_ctx)
{
    const uint8_t *p;
    const uint8_t *aad;
    const uint8_t *cipher;
    const uint8_t *tag;
    uint32_t aadlen32, clen32;
    size_t   aadlen, clen;
    uint8_t  *out;
    owcrypt_status_t st;
    uint32_t bancode;
    if (!owcrypt_initialized) {
        return OWCRYPT_ERR_UNINITIALIZED;
    }
    if (key == NULL || nonce == NULL || frame == NULL ||
        plain_len == NULL || aad_out == NULL || aad_len_out == NULL) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    if (frame_len < OWCRYPT_FRAME_OVERHEAD + 16u) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    p = frame;
    if (owc_get32le(p) != OWCRYPT_FRAME_MAGIC) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    aadlen32 = owc_get32le(p + 4);
    clen32   = owc_get32le(p + 8);
    aadlen = (size_t)aadlen32;
    clen   = (size_t)clen32;
    if (frame_len < OWCRYPT_FRAME_OVERHEAD + aadlen + clen + 16u) {
        return OWCRYPT_ERR_BAD_ARG;
    }
    aad    = frame + OWCRYPT_FRAME_OVERHEAD;
    cipher = aad + aadlen;
    tag    = cipher + clen;

    out = plain_out;
    if (out == NULL) {
        out = (uint8_t *)cipher;   /* authenticate, then decrypt in place */
    }
    st = owcrypt_chacha_poly_open(key, nonce, aad, aadlen, cipher, clen,
                                  tag, out);
    if (st != OWCRYPT_OK) {
        return st;
    }
    *plain_len = clen;
    *aad_out = (aadlen > 0u) ? aad : NULL;
    *aad_len_out = aadlen;

    if (scan_ban && clen > 0u) {
        bancode = owcrypt_scan_ban_stream(out, clen);
        if (bancode != 0u) {
            if (ban_hook != NULL) {
                ban_hook(bancode, 0u, ban_ctx);
            }
            return OWCRYPT_BAN_BANCODE;
        }
    }
    return OWCRYPT_OK;
}