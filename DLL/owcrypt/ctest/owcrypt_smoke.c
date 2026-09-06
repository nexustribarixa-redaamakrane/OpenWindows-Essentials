/*
 * owcrypt_smoke.c - hosted self-test for the owcrypt.owd crypto suite.
 *
 * Compiles DLL/owcrypt/owcrypt.c together with this file against the
 * host CRT (used only by the harness/facsimile). The k64 API is a local
 * facsimile. Runs known-answer vectors: SHA-256/512, BLAKE2b, HMAC,
 * ChaCha20, Poly1305, ChaCha20-Poly1305, AES-128/256 ECB + GCM,
 * X25519 RFC 7748, Ed25519 RFC 8032, CSPRNG, ct helpers, BAN scan and
 * SUTF seal/open frames.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "owcrypt.h"
#include "kernel64.h"

/* ------------------------------------------------------------------ */
/*  k64 facsimile                                                      */
/* ------------------------------------------------------------------ */

static bool fake_k64_ready = false;

k64_status_t k64_initialize_api(void)
{
    fake_k64_ready = true;
    return K64_OK;
}

bool k64_api_ready(void) { return fake_k64_ready; }

static int hex_u8(uint8_t *out, const char *s)
{
    int n = 0;
    while (s[0] && s[1]) {
        unsigned hi, lo;
        char c = s[0];
        hi = (c >= '0' && c <= '9') ? (unsigned)(c - '0')
           : (unsigned)(((c | 0x20) - 'a') + 10u);
        c = s[1];
        lo = (c >= '0' && c <= '9') ? (unsigned)(c - '0')
           : (unsigned)(((c | 0x20) - 'a') + 10u);
        out[n++] = (uint8_t)((hi << 4) | lo);
        s += 2;
    }
    return n;
}

static int hex_ok(const uint8_t *got, size_t n, const char *want)
{
    uint8_t w[256];
    int wn;
    assert(n <= 256u);
    wn = hex_u8(w, want);
    if ((size_t)wn != n) return 0;
    return memcmp(got, w, n) == 0;
}

static void dump512(const char *tag, const uint8_t *d)
{
    size_t zi;
    fprintf(stderr, "%s: ", tag);
    for (zi = 0u; zi < 64u; zi++) fprintf(stderr, "%02x", d[zi]);
    fprintf(stderr, "\n");
}

int main(void)
{
    owcrypt_status_t st = owcrypt_module_init();
    assert(st == OWCRYPT_OK);
    assert(owcrypt_abi_major() == 1u);

    /* ---- SHA-256("abc") ---- */
    {
        uint8_t d[32];
        owcrypt_sha256("abc", 3u, d);
        assert(hex_ok(d, 32,
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad"));
    }

    /* ---- SHA-512("abc") ---- */
    {
        uint8_t d[64];
        owcrypt_sha512("abc", 3u, d);
        assert(hex_ok(d, 64,
            "ddaf35a193617abacc417349ae204131"
            "12e6fa4e89a97ea20a9eeee64b55d39a"
            "2192992a274fc1a836ba3c23a3feebbd"
            "454d4423643ce80e2a9ac94fa54ca49f"));
    }

    /* ---- BLAKE2b-512("abc") ---- */
    {
        uint8_t d[64];
        owcrypt_blake2b(d, 64u, "abc", 3u);
        assert(hex_ok(d, 64,
            "ba80a53f981c4d0d6a2797b69f12f6e9"
            "4c212f14685ac4b74b12bb6fdbffa2d1"
            "7d87c5392aab792dc252d5de4533cc95"
            "18d38aa8dbf1925ab92386edd4009923"));
    }

    /* ---- HMAC-SHA256 RFC 4231 case 2 (0x0b*20, "Hi There") ---- */
    {
        uint8_t key[20], d[32];
        int i;
        for (i = 0; i < 20; i++) key[i] = 0x0b;
        owcrypt_hmac_sha256(key, 20u, "Hi There", 8u, d);
        assert(hex_ok(d, 32,
            "b0344c61d8db38535ca8afceaf0bf12b"
            "881dc200c9833da726e9376c2e32cff7"));
    }

    /* ---- ChaCha20 (RFC 8439) zero-state first block ---- */
    {
        uint8_t k[32], n[12], out[64];
        int i;
        for (i = 0; i < 32; i++) k[i] = 0;
        for (i = 0; i < 12; i++) n[i] = 0;
        owcrypt_chacha20_xor(k, n, 0u, NULL, 64u, out);
        assert(hex_ok(out, 32,
            "76b8e0ada0f13d90405d6ae55386bd28"
            "bdd219b8a08ded1aa836efcc8b770dc7"));
    }

    /* ---- Poly1305 (RFC 8439 2.5.2) ---- */
    {
        static const uint8_t msg[] = "Cryptographic Forum Research Group";
        static const uint8_t key[32] = {
            0x85,0xd6,0xbe,0x78,0x57,0x55,0x6d,0x33,
            0x7f,0x44,0x52,0xfe,0x42,0xd5,0x06,0xa8,
            0x01,0x03,0x80,0x8a,0xfb,0x0d,0xb2,0xfd,
            0x4a,0xbf,0xf6,0xaf,0x41,0x49,0xf5,0x1b
        };
        uint8_t tag[16];
        owcrypt_poly1305(key, msg, sizeof(msg) - 1u, tag);
        assert(hex_ok(tag, 16, "a8061dc1305136c6c22b8baf0c0127a9"));
    }

    /* ---- ChaCha20-Poly1305 AEAD (RFC 8439 2.8.2) ---- */
    {
        static const uint8_t aead_key[32] = {
            0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
            0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
            0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
            0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f
        };
        static const uint8_t aead_nonce[12] = {
            0x07,0x00,0x00,0x00,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47
        };
        static const uint8_t aead_aad[12] = {
            0x50,0x51,0x52,0x53,0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7
        };
        static const char aead_pt[] =
            "Ladies and Gentlemen of the class of '99: "
            "If I could offer you only one tip for the future, "
            "sunscreen would be it.";
        uint8_t ct[128], tag[16], back[128];
        assert(sizeof(aead_pt) - 1u <= 128u);
        st = owcrypt_chacha_poly_seal(aead_key, aead_nonce, aead_aad,
                                      sizeof(aead_aad), (const uint8_t *)aead_pt,
                                      sizeof(aead_pt) - 1u, ct, tag);
        assert(st == OWCRYPT_OK);
        assert(hex_ok(ct, sizeof(aead_pt) - 1u,
            "d31a8d34648e60db7b86afbc53ef7ec2"
            "a4aded51296e08fea9e2b5a736ee62d6"
            "3dbea45e8ca9671282fafb69da92728b"
            "1a71de0a9e060b2905d6a5b67ecd3b36"
            "92ddbd7f2d778b8c9803aee328091b58"
            "fab324e4fad675945585808b4831d7bc"
            "3ff4def08e4b7a9de576d26586cec64b"
            "6116"));
        assert(hex_ok(tag, 16, "1ae10b594f09e26a7e902ecbd0600691"));

        st = owcrypt_chacha_poly_open(aead_key, aead_nonce, aead_aad,
                                      sizeof(aead_aad), ct,
                                      sizeof(aead_pt) - 1u, tag, back);
        assert(st == OWCRYPT_OK);
        assert(memcmp(back, aead_pt, sizeof(aead_pt) - 1u) == 0);
        tag[0] ^= 1u;
        /* auth failure must not decrypt */
        st = owcrypt_chacha_poly_open(aead_key, aead_nonce, aead_aad,
                                      sizeof(aead_aad), ct,
                                      sizeof(aead_pt) - 1u, tag, back);
        assert(st == OWCRYPT_BAN_AUTH_FAIL);
    }

    /* ---- AES-128 ECB (FIPS-197 C.1) + round-trip ---- */
    {
        static const uint8_t k[16] = {
            0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
            0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
        };
        static const uint8_t pt[16] = {
            0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
            0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
        };
        uint8_t c[16], p[16];
        st = owcrypt_aes_ecb_encrypt(k, 128u, pt, c);
        assert(st == OWCRYPT_OK);
        assert(hex_ok(c, 16, "69c4e0d86a7b0430d8cdb78070b4c55a"));
        st = owcrypt_aes_ecb_decrypt(k, 128u, c, p);
        assert(st == OWCRYPT_OK && memcmp(p, pt, 16) == 0);
        assert(owcrypt_aes_ecb_encrypt(k, 64u, pt, c) == OWCRYPT_ERR_BAD_ARG);
    }

    /* ---- AES-256 ECB (FIPS-197 C.3) ---- */
    {
        static const uint8_t k[32] = {
            0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
            0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
            0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
            0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
        };
        static const uint8_t pt[16] = {
            0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
            0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
        };
        uint8_t c[16], p[16];
        st = owcrypt_aes_ecb_encrypt(k, 256u, pt, c);
        assert(st == OWCRYPT_OK);
        assert(hex_ok(c, 16, "8ea2b7ca516745bfeafc49904b496089"));
        st = owcrypt_aes_ecb_decrypt(k, 256u, c, p);
        assert(st == OWCRYPT_OK && memcmp(p, pt, 16) == 0);
    }

    /* ---- AES-128-GCM (SP 800-38D): zero key/nonce, empty pt ---- */
    {
        uint8_t k[16], n[12], tag[16];
        uint8_t ct[128];
        memset(k, 0, sizeof(k));
        memset(n, 0, sizeof(n));
        st = owcrypt_aes_gcm_seal(k, 128u, n, NULL, 0u, NULL, 0u, ct, tag);
        assert(st == OWCRYPT_OK);
        assert(hex_ok(tag, 16, "58e2fccefa7e3061367f1d57a4e7455a"));

        st = owcrypt_aes_gcm_open(k, 128u, n, NULL, 0u, ct, 0u, tag, ct);
        assert(st == OWCRYPT_OK);
    }

    /* ---- AES-128-GCM NIST case 3 ---- */
    {
        static const uint8_t k[16] = {
            0xfe,0xff,0xe9,0x92,0x86,0x65,0x73,0x1c,
            0x6d,0x6a,0x8f,0x94,0x67,0x30,0x83,0x08
        };
        static const uint8_t n[12] = {
            0xca,0xfe,0xba,0xbe,0xfa,0xce,0xdb,0xad,0xde,0xca,0xf8,0x88
        };
        static const uint8_t pt[64] = {
            0xd9,0x31,0x32,0x25,0xf8,0x84,0x06,0xe5,
            0xa5,0x59,0x09,0xc5,0xaf,0xf5,0x26,0x9a,
            0x86,0xa7,0xa9,0x53,0x15,0x34,0xf7,0xda,
            0x2e,0x4c,0x30,0x3d,0x8a,0x31,0x8a,0x72,
            0x1c,0x3c,0x0c,0x95,0x95,0x68,0x09,0x53,
            0x2f,0xcf,0x0e,0x24,0x49,0xa6,0xb5,0x25,
            0xb1,0x6a,0xed,0xf5,0xaa,0x0d,0xe6,0x57,
            0xba,0x63,0x7b,0x39,0x1a,0xaf,0xd2,0x55
        };
        uint8_t ct[64], tag[16], back[64];
        memset(ct, 0, sizeof(ct));
        st = owcrypt_aes_gcm_seal(k, 128u, n, NULL, 0u, pt, sizeof(pt), ct, tag);
        assert(st == OWCRYPT_OK);
        assert(hex_ok(ct, sizeof(pt),
            "42831ec2217774244b7221b784d0d49c"
            "e3aa212f2c02a4e035c17e2329aca12e"
            "21d514b25466931c7d8f6a5aac84aa05"
            "1ba30b396a0aac973d58e091473f5985"));
        assert(hex_ok(tag, 16, "4d5c2af327cd64a62cf35abd2ba6fab4"));
        st = owcrypt_aes_gcm_open(k, 128u, n, NULL, 0u, ct, sizeof(ct), tag,
                                  back);
        assert(st == OWCRYPT_OK && memcmp(back, pt, sizeof(pt)) == 0);
    }

    /* ---- X25519 (RFC 7748 6.1) ---- */
    {
        static const uint8_t a_scalar[32] = {
            0x77,0x07,0x6d,0x0a,0x73,0x18,0xa5,0x7d,
            0x3c,0x16,0xc1,0x72,0x51,0xb2,0x66,0x45,
            0xdf,0x4c,0x2f,0x87,0xeb,0xc0,0x99,0x2a,
            0xb1,0x77,0xfb,0xa5,0x1d,0xb9,0x2c,0x2a
        };
        static const uint8_t b_pub[32] = {
            0xde,0x9e,0xdb,0x7d,0x7b,0x7d,0xc1,0xb4,
            0xd3,0x5b,0x61,0xc2,0xec,0xe4,0x35,0x37,
            0x3f,0x83,0x43,0xc8,0x5b,0x78,0x67,0x4d,
            0xad,0xfc,0x7e,0x14,0x6f,0x88,0x2b,0x4f
        };
        uint8_t shared[32];
        st = owcrypt_x25519(shared, a_scalar, b_pub);
        assert(st == OWCRYPT_OK);
        assert(hex_ok(shared, 32,
            "4a5d9d5ba4ce2de1728e3bf480350f25"
            "e07e21c947d19e3376f09b3c1e161742"));
    }

    /* ---- Ed25519 (RFC 8032 7.1) ---- */
    {
        static const uint8_t seed[32] = {
            0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,
            0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,
            0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,
            0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60
        };
        uint8_t pub[32], sig[64];
        st = owcrypt_ed25519_keypair(seed, pub);
        assert(st == OWCRYPT_OK);
        assert(hex_ok(pub, 32,
            "d75a980182b10ab7d54bfed3c964073a"
            "0ee172f3daa62325af021a68f707511a"));
        st = owcrypt_ed25519_sign(seed, pub, (const uint8_t *)"", 0u, sig);
        assert(st == OWCRYPT_OK);
        assert(hex_ok(sig, 64,
            "e5564300c360ac729086e2cc806e828a"
            "84877f1eb8e5d974d873e06522490155"
            "5fb8821590a33bacc61e39701cf9b46b"
            "d25bf5f0595bbe24655141438e7a100b"));
        assert(owcrypt_ed25519_verify(pub, (const uint8_t *)"", 0u, sig)
               == OWCRYPT_OK);
        sig[10] ^= 1u;
        assert(owcrypt_ed25519_verify(pub, (const uint8_t *)"", 0u, sig)
               == OWCRYPT_BAN_AUTH_FAIL);
        st = owcrypt_ed25519_keypair(seed, pub);
        assert(st == OWCRYPT_OK);
        /* sign/verify round trip over a longer message */
        static const char *msgs[] = {
            "",
            "a",
            "Hello, world!",
            "Four score and seven years ago our fathers brought forth "
            "on this continent, a new nation, conceived in Liberty, and "
            "dedicated to the proposition that all men are created equal."
        };
        {
            size_t mi;
            for (mi = 0u; mi < 4u; mi++) {
                st = owcrypt_ed25519_sign(seed, pub, (const uint8_t *)msgs[mi],
                                          strlen(msgs[mi]), sig);
                assert(st == OWCRYPT_OK);
                assert(owcrypt_ed25519_verify(pub, (const uint8_t *)msgs[mi],
                                              strlen(msgs[mi]), sig)
                       == OWCRYPT_OK);
            }
        }
    }

    /* ---- CSPRNG ---- */
    {
        uint8_t r1[64], r2[64];
        owcrypt_status_t rs;
        size_t i;
        rs = owcrypt_csprng_init();
        assert(rs == OWCRYPT_OK);
        assert(owcrypt_csprng_generate(r1, sizeof(r1)) == OWCRYPT_OK);
        assert(owcrypt_csprng_generate(r2, sizeof(r2)) == OWCRYPT_OK);
        assert(memcmp(r1, r2, sizeof(r1)) != 0);
        /* rough entropy sanity: not all zero, not all equal */
        {
            unsigned sum = 0u;
            for (i = 0u; i < sizeof(r1); i++) sum |= r1[i];
            assert(sum != 0u);
        }
        assert(owcrypt_csprng_generate(NULL, 1u) == OWCRYPT_ERR_BAD_ARG);
        (void)rs;
    }

    /* ---- ct helpers / cleanse ---- */
    {
        uint8_t a[64], b[64];
        size_t i;
        for (i = 0u; i < 64u; i++) { a[i] = (uint8_t)i; b[i] = a[i]; }
        assert(owcrypt_ct_memcmp(a, b, 64u) == true);
        b[37] ^= 0x80u;
        assert(owcrypt_ct_memcmp(a, b, 64u) == false);
        owcrypt_cleanse(a, sizeof(a));
        for (i = 0u; i < 64u; i++) assert(a[i] == 0);
    }

    /* ---- BAN stream scan ---- */
    {
        static const uint8_t data[] = {
            0x01, 0x02, 0x20, 0xa3, 0x11, 0x00, /* LE 0x0011A320 (K64 boot) */
            0xff, 0xff, 0xff, 0xff              /* noise <nothing> */
        };
        assert(owcrypt_scan_ban_stream(data, sizeof(data))
               == OWCRYPT_BAN_K64_BOOT);
        assert(owcrypt_scan_ban_stream(data + 2, 4u) == OWCRYPT_BAN_K64_BOOT);
        assert(owcrypt_scan_ban_stream(data, 1u) == 0u);
    }

    /* ---- SUTF frame + ban scan on open ---- */
    {
        static const uint8_t fkey[32] = {
            0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
            0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
            0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
            0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
        };
        static const uint8_t fnonce[12] = {
            0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c
        };
        static const uint8_t faad[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        static const char fplain[] = "payload with signed content";
        uint8_t frame[512];
        size_t  frame_len = sizeof(frame);
        uint8_t plain[64];
        size_t  plen;
        const uint8_t *aad_out;
        size_t  aad_len_out;

        st = owcrypt_seal_frame(fkey, fnonce, faad, sizeof(faad),
                                (const uint8_t *)fplain, sizeof(fplain) - 1u,
                                frame, &frame_len);
        assert(st == OWCRYPT_OK);
        assert(frame_len == OWCRYPT_FRAME_OVERHEAD + sizeof(faad)
                           + sizeof(fplain) - 1u + 16u);

        plen = 0u;
        st = owcrypt_open_frame(fkey, fnonce, frame, frame_len, plain, &plen,
                                &aad_out, &aad_len_out, false, NULL, NULL);
        assert(st == OWCRYPT_OK);
        assert(plen == sizeof(fplain) - 1u);
        assert(aad_len_out == sizeof(faad));
        assert(aad_out != NULL && memcmp(aad_out, faad, sizeof(faad)) == 0);
        assert(memcmp(plain, fplain, plen) == 0);

        /* tampered frame must fail auth */
        frame[frame_len - 5u] ^= 0x01u;
        st = owcrypt_open_frame(fkey, fnonce, frame, frame_len, plain, &plen,
                                &aad_out, &aad_len_out, false, NULL, NULL);
        assert(st == OWCRYPT_BAN_AUTH_FAIL);

        /* ban-inducing payload caught pre-use */
        {
            static const uint8_t evil[] = {
                0xaa, 0x20, 0xa3, 0x11, 0x00, 0xbb
            };
            uint8_t buf[64];
            size_t buflen = sizeof(buf);
            st = owcrypt_seal_frame(fkey, fnonce, NULL, 0u, evil,
                                    sizeof(evil), buf, &buflen);
            assert(st == OWCRYPT_OK);
            plen = 0u;
            st = owcrypt_open_frame(fkey, fnonce, buf, buflen, frame, &plen,
                                    &aad_out, &aad_len_out, true,
                                    (owcrypt_ban_signal_t)0, NULL);
            assert(st == OWCRYPT_BAN_BANCODE);
        }
    }

    printf("owcrypt_smoke: PASS\n");
    return 0;
}