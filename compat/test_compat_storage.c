/**
 * Ecosystem Compatibility Test: OpenWindows-Storage <-> SuperUnicode
 *
 * Single translation unit co-including:
 *   - sutf's sutf8.h                     (canonical libsutf SUTF-8 codec)
 *   - OpenWindows-storage <owfs_types.h> / <usfs_types.h> / <ow_string.h>
 *
 * Verifies:
 *   1. OWFS/USFS layout constants agree with the shared storage contract.
 *   2. The ow_sutf8_validate() codec accepts EXACTLY the streams the
 *      canonical libsutf sutf8_encode_char()/sutf8_decode_char() accept
 *      (1-6 byte canonical forms, overlong rejection, surrogate PUA
 *      codepoints 0xD800-0xDFFF valid, Kernel Security Trap range and
 *      sentinel unencodable).
 *   3. Names written for OWFS/USFS round-trip through the libsutf codec.
 *
 * Build mode: compiles the REAL ../OpenWindows-Storage common sources when
 * present, otherwise the vendored port under compat/OpenWindows-storage/.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "sutf8.h"
#include "owfs_types.h"
#include "usfs_types.h"
#include "ow_string.h"

static int g_checks;

#define CHECK(cond) \
    do { ++g_checks; assert(cond); } while (0)

/* Boundary & edge codepoints for the codec contract sweep */
static const sucs_char_t k_probe_cps[] = {
    0x00000000UL, 0x00000041UL, 0x0000007FUL,
    0x00000080UL, 0x000007FFUL,
    0x00000800UL, 0x00000FFFUL,
    0x0000D7FFUL, 0x0000D800UL, 0x0000DBFFUL, 0x0000DFFFUL, /* SUCS PUA */
    0x0000E000UL, 0x0000FFFFUL,
    0x00010000UL, 0x0010FFFFUL,
    0x00110000UL,
    0x0011A000UL, 0x0011A3E6UL, 0x0011AEFFUL,               /* BANcode registry */
    0x00120000UL, 0x00FFFFFFUL,
    0x01000000UL, 0x03FFFFFFUL,
    0x04000000UL, 0x40000000UL,
    0x7FFFFFEFUL                                            /* last valid */
};

/* Codepoints that must be UNENCODEABLE (reserved kernel space) */
static const sucs_char_t k_invalid_cps[] = {
    0x7FFFFFF0UL, 0x7FFFFFF5UL, 0x7FFFFFFEUL, 0x7FFFFFFFUL
};

/* Malformed byte vectors: rejected by both codecs */
static const uint8_t k_malformed[][8] = {
    { 0xC0, 0x80 },                          /* overlong 2-byte NUL       */
    { 0xC1, 0xBF },                          /* overlong 2-byte           */
    { 0xE0, 0x80, 0x80 },                    /* overlong 3-byte           */
    { 0xF0, 0x80, 0x80, 0x80 },              /* overlong 4-byte           */
    { 0xF8, 0x80, 0x80, 0x80, 0x80 },        /* overlong 5-byte           */
    { 0xFC, 0x80, 0x80, 0x80, 0x80, 0x80 },  /* overlong 6-byte NUL      */
    { 0xF8, 0x80, 0x80, 0x80, 0x01 },        /* overlong 5-byte (cp=1)   */
    { 0xFC, 0x83, 0xBF, 0xBF, 0xBF, 0xBF },  /* overlong (5-byte max cp) */
    { 0xC3 },                                /* truncated                 */
    { 0xE1, 0x80 },                          /* truncated                 */
    { 0xF2, 0x85, 0x80, 0xC0 },              /* bad continuation          */
    { 0xFE },                                /* invalid lead              */
    { 0xFF },                                /* invalid lead              */
    { 0x80 }                                 /* bare continuation         */
};

/* Hand-built canonical 6-byte encodings of reserved kernel codepoints
 * (libsutf's encoder refuses them, so they must be constructed by hand):
 * both codecs must reject them. */
static void test_reserved_stream_rejection(void) {
    static const uint8_t reserved_streams[][8] = {
        { 0xFD, 0xBF, 0xBF, 0xBF, 0xBF, 0xB0 }, /* 0x7FFFFFF0 (trap min) */
        { 0xFD, 0xBF, 0xBF, 0xBF, 0xBF, 0xBE }, /* 0x7FFFFFFE (trap max) */
        { 0xFD, 0xBF, 0xBF, 0xBF, 0xBF, 0xBF }  /* 0x7FFFFFFF (sentinel)*/
    };
    size_t i;
    sucs_char_t cp = 0;
    uint8_t probe[16];

    for (i = 0; i < sizeof(reserved_streams) / sizeof(reserved_streams[0]); ++i) {
        CHECK(sutf8_decode_char(reserved_streams[i], 6, &cp) == 0);
        CHECK(cp == SUCS_INVALID_CODEPOINT);
        CHECK(!ow_sutf8_validate(reserved_streams[i], 6));
    }

    /* And libsutf must refuse to encode them either */
    for (i = 0; i < sizeof(k_invalid_cps) / sizeof(k_invalid_cps[0]); ++i) {
        CHECK(sutf8_encode_char(k_invalid_cps[i], probe, sizeof(probe)) == 0);
        CHECK(sutf8_codepoint_length(k_invalid_cps[i]) == 0);
    }
}

int main(void) {

#ifdef COMPAT_STORAGE_REAL
    printf("compat_storage: using REAL ../OpenWindows-Storage common sources\n");
#else
    printf("compat_storage: using vendored compat/OpenWindows-storage port\n");
#endif

    /* --- 1. Layout constant parity --------------------------------------- */
    CHECK(OWFS_BLOCK_SIZE == 0x1000);
    CHECK(USFS_BLOCK_SIZE == OWFS_BLOCK_SIZE);
    CHECK(OWFS_BLOCK_SHIFT == 12);
    CHECK(USFS_BLOCK_SHIFT == OWFS_BLOCK_SHIFT);
    CHECK((OWFS_BLOCK_SIZE >> OWFS_BLOCK_SHIFT) == 1); /* power-of-two check */

    CHECK(OWFS_NAME_MAX_BYTES == 128);
    CHECK(USFS_NAME_MAX_BYTES == OWFS_NAME_MAX_BYTES);

    CHECK(OWFS_SUPERBLOCK_BLOCK == 16);
    CHECK(OWFS_PARTITION_OFFSET == 0x10000UL); /* 64 KiB MBL-reserved region */
    CHECK(OWFS_PARTITION_OFFSET == ((size_t)OWFS_SUPERBLOCK_BLOCK << OWFS_BLOCK_SHIFT));

    CHECK(OWFS_ENTRY_FILE    == 0x01);
    CHECK(OWFS_ENTRY_CATALOG == 0x02);
    CHECK(OWFS_ENTRY_DELETED == 0x80);
    CHECK(USFS_ENTRY_FILE    == OWFS_ENTRY_FILE);
    CHECK(USFS_ENTRY_CATALOG == OWFS_ENTRY_CATALOG);
    CHECK(USFS_ENTRY_DELETED == OWFS_ENTRY_DELETED);

    CHECK(OWFS_SEC_ENCRYPTED == USFS_SEC_ENCRYPTED);
    CHECK(OWFS_SEC_READONLY  == USFS_SEC_READONLY);
    CHECK(OWFS_SEC_HIDDEN    == USFS_SEC_HIDDEN);
    CHECK(OWFS_SEC_ENCRYPTED == (1U << 0));
    CHECK(OWFS_SEC_READONLY  == (1U << 1));
    CHECK(OWFS_SEC_HIDDEN    == (1U << 2));

    CHECK(OWFS_MAGIC == 0x4F574653UL); /* 'OWFS' */
    CHECK(USFS_MAGIC == 0x55534653UL); /* 'USFS' */
    CHECK(OWFS_ROOT_INODE == 0);

    /* --- 2. Codec contract sweep: encode(libsutf) -> validate(storage) ---- */
    {
        size_t i;
        uint8_t buf[16];

        for (i = 0; i < sizeof(k_probe_cps) / sizeof(k_probe_cps[0]); ++i) {
            sucs_char_t cp = k_probe_cps[i];
            size_t n = sutf8_encode_char(cp, buf, sizeof(buf));
            sucs_char_t back = 0;

            CHECK(n > 0);
            CHECK(n == sutf8_codepoint_length(cp));
            CHECK(ow_sutf8_validate(buf, n));           /* storage accepts  */
            CHECK(sutf8_decode_char(buf, n, &back) == n);
            CHECK(back == cp);                           /* round-trip      */
        }

        for (i = 0; i < sizeof(k_invalid_cps) / sizeof(k_invalid_cps[0]); ++i) {
            CHECK(sutf8_codepoint_length(k_invalid_cps[i]) == 0);
        }
    }

    /* --- 3. Reserved stream rejection by BOTH sides ----------------------- */
    test_reserved_stream_rejection();

    /* --- 4. Malformed vector rejection by BOTH sides ---------------------- */
    {
        size_t i, len;
        sucs_char_t cp = 0;

        for (i = 0; i < sizeof(k_malformed) / sizeof(k_malformed[0]); ++i) {
            len = 0;
            while (len < 8 && k_malformed[i][len] != 0) {
                ++len;
            }
            if (len == 0) {
                len = 8; /* vector with embedded zeros */
            }
            CHECK(sutf8_decode_char(k_malformed[i], len, &cp) == 0);
            CHECK(cp == SUCS_INVALID_CODEPOINT);
            CHECK(!ow_sutf8_validate(k_malformed[i], len));
        }
    }

    /* --- 5. OWFS name pipeline round-trip through libsutf ------------------ */
    {
        static const char* k_names[] = {
            "kernel.bin",
            "System Catalog",
            "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E",         /* 3-byte UTF-8   */
            "\xF0\x9F\x92\xA1",                              /* 4-byte         */
            "\xF8\x87\xBF\xBF\xBF",                          /* 5-byte native  */
            "\xFC\x84\xBF\xBF\xBF\xBF"                       /* 6-byte native  */
        };
        size_t i;

        for (i = 0; i < sizeof(k_names) / sizeof(k_names[0]); ++i) {
            size_t len = strlen(k_names[i]);
            CHECK(len < OWFS_NAME_MAX_BYTES);
            CHECK(len < USFS_NAME_MAX_BYTES);
            CHECK(ow_sutf8_validate((const uint8_t*)k_names[i], len));

            /* every codepoint decodes canonically via libsutf */
            {
                size_t pos = 0;
                while (pos < len) {
                    sucs_char_t cp = 0;
                    size_t n = sutf8_decode_char(
                        (const uint8_t*)k_names[i] + pos, len - pos, &cp);
                    CHECK(n > 0);
                    CHECK(sucs_is_valid(cp));
                    pos += n;
                }
            }

            /* name_copy truncation respects the on-disk field limit */
            {
                uint8_t out[OWFS_NAME_MAX_BYTES];
                size_t copied = ow_sutf8_name_copy(out, sizeof(out),
                                                   (const uint8_t*)k_names[i], len);
                CHECK(copied == len);
                CHECK(out[copied] == 0);
                CHECK(ow_sutf8_name_cmp(out, copied,
                                        (const uint8_t*)k_names[i], len) == 0);
            }
        }
    }

    printf("compat_storage: %d checks passed\n", g_checks);
    return 0;
}
