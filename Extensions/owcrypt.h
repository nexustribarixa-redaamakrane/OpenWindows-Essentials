/*
 * owcrypt.h - owcrypt.owd ABI mirror (OpenWindows Encryption Engine)
 *
 * Freestanding C99 cryptographic suite for OpenWindows modules:
 *   - SHA-256, SHA-512, BLAKE2b, HMAC
 *   - ChaCha20 + Poly1305 + ChaCha20-Poly1305 AEAD (RFC 8439)
 *   - AES-128 / AES-256 + GCM AEAD (FIPS-197 / SP 800-38D)
 *   - X25519 key exchange (RFC 7748) and Ed25519 signatures (RFC 8032)
 *   - jitter/RDTSC-seeded ChaCha20 CSPRNG, constant-time compare,
 *     memory cleanse, and SUTF security-frame sealing with BANcode
 *     payload pre-scan linked to the banhammer security trap plane.
 *
 * All datums are big- or little-endian per their RFC. Zero allocation:
 * every routine writes into caller buffers. The CSPRNG holds one ChaCha20
 * state and reseeds itself from bare-metal entropy every N bytes.
 *
 * Conforms to OWD1 binary format (Extensions/owd_format.h).
 * C99 freestanding - <stdint.h>/<stdbool.h>/<stddef.h>/<limits.h> only.
 */

#ifndef OWE_OWCRYPT_H
#define OWE_OWCRYPT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Status Codes - BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A7FF): Fatal module faults                    */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal degradations                 */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                     */
/* ------------------------------------------------------------------ */

typedef uint32_t owcrypt_status_t;

#define OWCRYPT_OK                         0x00000000u  /* Success        */
/* B+ Fatal */
#define OWCRYPT_BAN_K64_BOOT               0x0011A320u  /* k64 API dead    */
#define OWCRYPT_BAN_ENTROPY                0x0011A321u  /* entropy fail    */
#define OWCRYPT_BAN_AUTH_FAIL              0x0011A322u  /* tag mismatch    */
#define OWCRYPT_BAN_BAD_KEY                0x0011A323u  /* invalid key     */
#define OWCRYPT_BAN_BANCODE                0x0011A324u  /* payload BANcode */
/* S+ Soft */
#define OWCRYPT_ERR_UNINITIALIZED          0x0011AEC0u  /* Not init yet   */
#define OWCRYPT_ERR_BAD_ARG                0x0011AEC1u  /* Bad argument   */
#define OWCRYPT_ERR_SHORT_OUT              0x0011AEC2u  /* Output too small*/

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

owcrypt_status_t owcrypt_module_init(void);
owcrypt_status_t owcrypt_module_shutdown(void);
uint32_t owcrypt_abi_version(void);
uint16_t owcrypt_abi_major(void);
uint16_t owcrypt_abi_minor(void);
const uint8_t *owcrypt_ident(void);

/* ------------------------------------------------------------------ */
/*  SHA-256 / SHA-512                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t h[8];
    uint64_t total[2];
    uint8_t  buf[128];
    size_t   buflen;
} owcrypt_sha_ctx_t;

void owcrypt_sha256_init(owcrypt_sha_ctx_t *c);
void owcrypt_sha256_update(owcrypt_sha_ctx_t *c, const void *data, size_t n);
void owcrypt_sha256_final(owcrypt_sha_ctx_t *c, uint8_t out[32]);

void owcrypt_sha512_init(owcrypt_sha_ctx_t *c);
void owcrypt_sha512_update(owcrypt_sha_ctx_t *c, const void *data, size_t n);
void owcrypt_sha512_final(owcrypt_sha_ctx_t *c, uint8_t out[64]);

void owcrypt_sha256(const void *data, size_t n, uint8_t out[32]);
void owcrypt_sha512(const void *data, size_t n, uint8_t out[64]);

/* ------------------------------------------------------------------ */
/*  BLAKE2b (RFC 7693), digest length 1..64 bytes                     */
/* ------------------------------------------------------------------ */

void owcrypt_blake2b(uint8_t *out, size_t outlen,
                     const void *in, size_t inlen);

/* ------------------------------------------------------------------ */
/*  HMAC-SHA256 / HMAC-SHA512                                          */
/* ------------------------------------------------------------------ */

void owcrypt_hmac_sha256(const uint8_t *key, size_t keylen,
                         const void *msg, size_t msglen, uint8_t out[32]);
void owcrypt_hmac_sha512(const uint8_t *key, size_t keylen,
                         const void *msg, size_t msglen, uint8_t out[64]);

/* ------------------------------------------------------------------ */
/*  ChaCha20 stream (RFC 8439)                                         */
/* ------------------------------------------------------------------ */

void owcrypt_chacha20_xor(const uint8_t key[32], const uint8_t nonce[12],
                          uint32_t counter, const uint8_t *in, size_t len,
                          uint8_t *out);

/* ------------------------------------------------------------------ */
/*  Poly1305 one-shot MAC (RFC 8439)                                   */
/* ------------------------------------------------------------------ */

void owcrypt_poly1305(const uint8_t key[32], const uint8_t *msg, size_t len,
                      uint8_t tag[16]);

/* ------------------------------------------------------------------ */
/*  ChaCha20-Poly1305 AEAD (RFC 8439 construction)                     */
/*  `out` may alias `in`. Tag is 16 bytes, appended by seal, compared   */
/*  by open.                                                           */
/* ------------------------------------------------------------------ */

owcrypt_status_t owcrypt_chacha_poly_seal(const uint8_t key[32],
                                          const uint8_t nonce[12],
                                          const uint8_t *aad, size_t aadlen,
                                          const uint8_t *plain, size_t plen,
                                          uint8_t *out, uint8_t tag[16]);

owcrypt_status_t owcrypt_chacha_poly_open(const uint8_t key[32],
                                          const uint8_t nonce[12],
                                          const uint8_t *aad, size_t aadlen,
                                          const uint8_t *cipher, size_t clen,
                                          const uint8_t tag[16],
                                          uint8_t *out);

/* ------------------------------------------------------------------ */
/*  AES-128 / AES-256 (FIPS-197)                                       */
/* ------------------------------------------------------------------ */

/* Single 16-byte block ECB (both directions). keybits is 128 or 256. */
owcrypt_status_t owcrypt_aes_ecb_encrypt(const uint8_t *key, uint32_t keybits,
                                         const uint8_t pt[16], uint8_t ct[16]);
owcrypt_status_t owcrypt_aes_ecb_decrypt(const uint8_t *key, uint32_t keybits,
                                         const uint8_t ct[16], uint8_t pt[16]);

/* ------------------------------------------------------------------ */
/*  AES-GCM AEAD (SP 800-38D), 96-bit nonce, 16-byte tag              */
/* `out` may alias `in`.                                              */
/* ------------------------------------------------------------------ */

owcrypt_status_t owcrypt_aes_gcm_seal(const uint8_t *key, uint32_t keybits,
                                      const uint8_t nonce[12],
                                      const uint8_t *aad, size_t aadlen,
                                      const uint8_t *plain, size_t plen,
                                      uint8_t *out, uint8_t tag[16]);

owcrypt_status_t owcrypt_aes_gcm_open(const uint8_t *key, uint32_t keybits,
                                      const uint8_t nonce[12],
                                      const uint8_t *aad, size_t aadlen,
                                      const uint8_t *cipher, size_t clen,
                                      const uint8_t tag[16], uint8_t *out);

/* ------------------------------------------------------------------ */
/*  X25519 (RFC 7748)                                                  */
/*  scalar_out may alias scalar_in (scalar mult). u == NULL selects the */
/*  standard base point u = 9.                                        */
/* ------------------------------------------------------------------ */

owcrypt_status_t owcrypt_x25519(uint8_t scalar_out[32],
                                const uint8_t scalar_in[32],
                                const uint8_t u[32]);

/* ------------------------------------------------------------------ */
/*  Ed25519 (RFC 8032)                                                 */
/* ------------------------------------------------------------------ */

owcrypt_status_t owcrypt_ed25519_keypair(const uint8_t seed[32],
                                         uint8_t public_key[32]);
owcrypt_status_t owcrypt_ed25519_sign(const uint8_t seed[32],
                                      const uint8_t public_key[32],
                                      const uint8_t *msg, size_t msglen,
                                      uint8_t sig[64]);
owcrypt_status_t owcrypt_ed25519_verify(const uint8_t public_key[32],
                                        const uint8_t *msg, size_t msglen,
                                        const uint8_t sig[64]);

/* ------------------------------------------------------------------ */
/*  CSPRNG (ChaCha20 DRBG over jitter + RDTSC entropy)                 */
/* ------------------------------------------------------------------ */

owcrypt_status_t owcrypt_csprng_init(void);
owcrypt_status_t owcrypt_csprng_generate(void *out, size_t n);

/* ------------------------------------------------------------------ */
/*  Constant-time helpers                                              */
/* ------------------------------------------------------------------ */

bool owcrypt_ct_memcmp(const void *a, const void *b, size_t n);
void owcrypt_cleanse(void *ptr, size_t n);

/* ------------------------------------------------------------------ */
/*  SUTF security frame                                                */
/*  Frame wire layout (all counts little-endian):                      */
/*      [magic 4: "OWSF"][aadlen 4][clen 4][aad][cipher][tag 16]      */
/*  open authenticates BEFORE decrypting in place and (optionally)     */
/*  scans the recovered plaintext for B+ BANcodes, calling the trap    */
/*  hook so banhammer damage control can sever the exchange.           */
/* ------------------------------------------------------------------ */

#define OWCRYPT_FRAME_MAGIC      0x4653574Fu       /* "OWSF" LE        */
#define OWCRYPT_FRAME_OVERHEAD   12u               /* magic+aadlen+clen */

typedef void (*owcrypt_ban_signal_t)(uint32_t bancode, uint64_t subcode,
                                     void *ctx);

owcrypt_status_t owcrypt_seal_frame(const uint8_t key[32],
                                    const uint8_t nonce[12],
                                    const uint8_t *aad, size_t aadlen,
                                    const uint8_t *plain, size_t plen,
                                    uint8_t *frame, size_t *frame_len);

/* Frame-overhead bytes at least OWCRYPT_FRAME_OVERHEAD + 16. */
owcrypt_status_t owcrypt_open_frame(const uint8_t key[32],
                                    const uint8_t nonce[12],
                                    const uint8_t *frame, size_t frame_len,
                                    uint8_t *plain_out, size_t *plain_len,
                                    const uint8_t **aad_out, size_t *aad_len_out,
                                    bool scan_ban,
                                    owcrypt_ban_signal_t ban_hook,
                                    void *ban_ctx);

/* Scan a raw byte stream for little-endian BANcode-registry codepoints
 * (0x0011A000-0x0011AEFF). Returns the first match or 0. */
uint32_t owcrypt_scan_ban_stream(const uint8_t *data, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* OWE_OWCRYPT_H */