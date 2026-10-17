/*
 * owhash64.h - OpenWindows Hash Functions Library (.owd)
 *
 * Provides self-contained cryptographic and integrity digests:
 *   - SHA-256 (FIPS 180-4)
 *   - MD5    (RFC 1321)
 *   - CRC32C (Castagnoli, reflected polynomial 0x82F63B78)
 *
 * C99 freestanding. Zero dynamic heap allocation; callers own all storage.
 */

#ifndef OWHASH64_H
#define OWHASH64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Digest sizes in bytes. */
#define OWHASH_SHA256_DIGEST_SIZE 32u
#define OWHASH_MD5_DIGEST_SIZE    16u

/* SHA-256 block size in bytes (internal buffer width). */
#define OWHASH_SHA256_BLOCK_SIZE  64u

/* ------------------------------------------------------------------ */
/*  SHA-256 streaming context                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t state[8];                         /* Working hash words    */
    uint64_t bitlen;                           /* Total length in bits  */
    uint8_t  buffer[OWHASH_SHA256_BLOCK_SIZE]; /* Partial block buffer  */
    uint32_t buflen;                           /* Bytes held in buffer  */
} owhash_sha256_ctx_t;

/* Initialise a SHA-256 context to the standard IV. */
void owhash_sha256_init(owhash_sha256_ctx_t *ctx);

/* Absorb `len` bytes of message material into the context. */
void owhash_sha256_update(owhash_sha256_ctx_t *ctx, const void *data, size_t len);

/* Finalise and write OWHASH_SHA256_DIGEST_SIZE bytes into `out`. */
void owhash_sha256_final(owhash_sha256_ctx_t *ctx,
                         uint8_t out[OWHASH_SHA256_DIGEST_SIZE]);

/* ------------------------------------------------------------------ */
/*  MD5 one-shot digest                                               */
/* ------------------------------------------------------------------ */

/* Compute the MD5 of `len` bytes at `data`; writes 16 bytes into `out`. */
void owhash_md5(const void *data, size_t len,
                uint8_t out[OWHASH_MD5_DIGEST_SIZE]);

/* ------------------------------------------------------------------ */
/*  CRC32C (Castagnoli) running checksum                              */
/* ------------------------------------------------------------------ */

/*
 * Update a running CRC32C. Pass 0 for the first call to obtain the
 * standard IEEE-behaved Castagnoli checksum; pass the previous return
 * value to continue over a split buffer.
 */
uint32_t owhash_crc32c(uint32_t crc, const void *data, size_t len);

#endif /* OWHASH64_H */
