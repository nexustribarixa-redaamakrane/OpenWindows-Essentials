/* test_compat_vip.c - Ecosystem compatibility test: vip <-> SuperUnicode.
 *
 * Vendored into the workspace compat suite from the vip sibling project's
 * own test_compat_smoke.c (source-level integration). Runtime compatibility
 * smoke test for the VIP Volume Indexing Protocol framework.
 *
 * Covers all four ecosystem integration surfaces:
 *   1. BANcode      - every return code inside its canonical C+/S+/B+
 *                     block; classification helpers; Kernel Security Trap
 *                     mapping parity with bancode_all.h's bancode_to_trap().
 *   2. SuperUnicode - SUTF-8 codec parity against the real libsutf
 *                     implementation (lengths, round-trips, rejection).
 *   3. OpenWindows-storage - OWFS/USFS flag conversion, absolute byte/LBA
 *                     addressing, registry semantics.
 *   4. Modular-Bootloader - MBL reserved-region + GPT partition LBA
 *                     layout constants and 512-byte LBA addressing units.
 *
 * This TU includes the VIP header FIRST (reverse order of
 * compat_tu_fsfirst.c), then co-includes <bancode/bancode_all.h>,
 * libsutf's <sutf8.h>, and <owfs_types.h> in one translation unit.
 *
 * Hosted build (uses stdio); links the freestanding libvip objects.
 */
#include <stdio.h>
#include <string.h>

#include "univip_fvip.h"            /* VIP first */
#include "bancode/bancode_all.h"    /* BANcode framework */
#include "sutf8.h"                  /* libsutf reference codec */
#include "owfs_types.h"             /* canonical storage constants */

extern int compat_check_fs_macros(void);

static int g_pass;
static int g_fail;

#define CHECK(cond) \
    do { \
        if (cond) { \
            ++g_pass; \
        } else { \
            ++g_fail; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

/* ------------------------------------------------------------------ */
/* 1. BANcode block alignment, classification, trap dispatch parity   */
/* ------------------------------------------------------------------ */

static void test_bancode_alignment(void) {
    static const bancode_t success_codes[] = {
        VIP_OK, VIP_INIT_OK, VIP_REGISTER_OK, VIP_RESOLVE_OK,
        VIP_INSERT_OK, VIP_LOOKUP_OK, VIP_REMOVE_OK, VIP_INTEGRITY_OK
    };
    static const bancode_t soft_codes[] = {
        VIP_ERR_INVALID_PARAM, VIP_ERR_NULL_POINTER, VIP_ERR_NOT_FOUND,
        VIP_ERR_ALREADY_EXISTS, VIP_ERR_TABLE_FULL,
        VIP_ERR_VOLUME_NOT_REGISTERED, VIP_ERR_VOLUME_LIMIT,
        VIP_ERR_LABEL_TOO_LONG, VIP_ERR_PATH_TOO_LONG,
        VIP_ERR_NOT_INITIALIZED, VIP_ERR_SYSTEM_NOT_READY,
        VIP_ERR_INTEGRITY_FAIL, VIP_ERR_INVALID_SUTF8
    };
    static const bancode_t fatal_codes[] = {
        VIP_BAN_TABLE_CORRUPT, VIP_BAN_NODE_CORRUPT, VIP_BAN_INDEX_OVERRUN,
        VIP_BAN_POOL_EXHAUSTED, VIP_BAN_TRIE_DEPTH_EXCEEDED,
        VIP_BAN_SYSTEM_NOT_INITIALIZED, VIP_BAN_ENTRY_CORRUPT
    };
    /* Trap-cluster boundaries across the B+ block */
    static const bancode_t trap_samples[] = {
        0x0011A000U, 0x0011A07FU, 0x0011A080U, 0x0011A0FFU,
        0x0011A100U, 0x0011A300U, 0x0011A3E6U, 0x0011A600U,
        0x0011A700U, 0x0011A77FU
    };
    static const bancode_t non_trap_samples[] = {
        0x0011A780U, 0x0011A7FFU, 0x0011AB00U, VIP_OK,
        VIP_ERR_NOT_FOUND, 0x0011ADFFU, VIP_ERR_INVALID_SUTF8, 0x00000000U
    };
    size_t i;

    for (i = 0; i < sizeof(success_codes) / sizeof(success_codes[0]); ++i) {
        CHECK(vip_code_is_success(success_codes[i]));
        CHECK(!vip_code_is_soft(success_codes[i]));
        CHECK(!vip_code_is_fatal(success_codes[i]));
        CHECK(!vip_code_is_warning(success_codes[i]));
    }

    for (i = 0; i < sizeof(soft_codes) / sizeof(soft_codes[0]); ++i) {
        CHECK(vip_code_is_soft(soft_codes[i]));
        CHECK(!vip_code_is_success(soft_codes[i]));
        CHECK(!vip_code_is_fatal(soft_codes[i]));
        CHECK(!vip_code_is_warning(soft_codes[i]));
    }

    for (i = 0; i < sizeof(fatal_codes) / sizeof(fatal_codes[0]); ++i) {
        CHECK(vip_code_is_fatal(fatal_codes[i]));
        CHECK(!vip_code_is_success(fatal_codes[i]));
        CHECK(!vip_code_is_soft(fatal_codes[i]));
        CHECK(!vip_code_is_warning(fatal_codes[i]));
        /* Must sit inside a dispatchable cluster (< unmapped A780) */
        CHECK(fatal_codes[i] < 0x0011A780U);
    }

    /* Trap mapping parity with the official BANcode framework header */
    for (i = 0; i < sizeof(trap_samples) / sizeof(trap_samples[0]); ++i) {
        CHECK(vip_bancode_to_trap(trap_samples[i]) ==
              bancode_to_trap(trap_samples[i]));
    }
    CHECK(vip_bancode_to_trap(0x0011A01AU) == 0x7FFFFFF0U);
    CHECK(vip_bancode_to_trap(VIP_BAN_ENTRY_CORRUPT) == 0x7FFFFFF7U);

    for (i = 0; i < sizeof(non_trap_samples) / sizeof(non_trap_samples[0]); ++i) {
        CHECK(vip_bancode_to_trap(non_trap_samples[i]) == BANCODE_INVALID_CODEPOINT);
        CHECK(vip_bancode_to_trap(non_trap_samples[i]) == SUCS_INVALID_CODEPOINT);
    }
}

/* ------------------------------------------------------------------ */
/* 2. SuperUnicode / SUTF-8 codec parity with libsutf                 */
/* ------------------------------------------------------------------ */

static void test_sutf8_parity(void) {
    static const uint32_t cps[] = {
        0x00000041U, 0x0000007FU, 0x00000080U, 0x000007FFU,
        0x00000800U, 0x0000FFFFU, 0x00010000U, 0x0010FFFFU,
        0x00200000U, 0x03FFFFFFU, 0x04000000U, 0x40000000U,
        0x7FFFFFFEU
    };
    /* [bytes, length] vectors - expected rejected by BOTH codecs */
    static const uint8_t malformed[][8] = {
        { 0xC0, 0x80 },                          /* overlong 2-byte   */
        { 0xE0, 0x80, 0x80 },                    /* overlong 3-byte   */
        { 0xF8, 0x88, 0x80, 0x80, 0x80 },        /* overlong 5-byte   */
        { 0xC3 },                                /* truncated         */
        { 0xE2, 0x82 },                          /* truncated         */
        { 0xFF },                                /* invalid lead      */
        { 0xF8, 0x88, 0x80, 0x80, 0x40 },        /* bad continuation  */
        { 0xFD, 0xBF, 0xBF, 0xBF, 0xBF, 0xBF }   /* sentinel cp       */
    };
    uint8_t buf[16];
    size_t i;

    for (i = 0; i < sizeof(cps) / sizeof(cps[0]); ++i) {
        size_t ref_len;
        size_t my_len;
        sucs_char_t cp = cps[i];
        int enc;
        uint32_t decoded = SUCS_INVALID_CODEPOINT;
        size_t consumed;

        ref_len = sutf8_codepoint_length(cp);
        my_len = fvip_sutf8_codepoint_length(cp);
        if (ref_len != my_len) {
            CHECK(0 && "length parity");
            continue;
        }
        if (my_len == 0) {
            /* Invalid codepoint (e.g. trap range): neither codec may
             * transport it - encode() must refuse as well */
            CHECK(sutf8_encode_char(cp, buf, sizeof(buf)) == 0);
            continue;
        }

        enc = (int)sutf8_encode_char(cp, buf, sizeof(buf));
        CHECK(enc > 0 && (size_t)enc == my_len);

        consumed = fvip_sutf8_decode_char(buf, sizeof(buf), &decoded);
        CHECK(consumed == my_len);
        CHECK(decoded == cp);
    }

    /* Invalid codepoints: both report zero-length */
    CHECK(sutf8_codepoint_length(SUCS_INVALID_CODEPOINT) == 0);
    CHECK(fvip_sutf8_codepoint_length(SUCS_INVALID_CODEPOINT) == 0);

    /* Malformed streams: identical rejection behavior */
    for (i = 0; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
        sucs_char_t ref_cp = 0;
        uint32_t my_cp = 0;
        size_t ref_n;
        size_t my_n;

        ref_n = sutf8_decode_char(malformed[i], 8, &ref_cp);
        my_n = fvip_sutf8_decode_char(malformed[i], 8, &my_cp);
        CHECK((ref_n == 0) == (my_n == 0));
        if (my_n == 0) {
            CHECK(my_cp == SUCS_INVALID_CODEPOINT);
        }
    }

    /* String-level helpers */
    CHECK(fvip_str_is_sutf8("/boot/kernel.bin"));
    CHECK(fvip_str_is_sutf8(""));
    CHECK(!fvip_str_is_sutf8((const char *)0));
    {
        static const char bad_overlong[] = { '/', (char)0xC0, (char)0x80, 0 };
        static const char bad_trunc[] = { '/', (char)0xE2, (char)0x82, 0 };
        CHECK(!fvip_str_is_sutf8(bad_overlong));
        CHECK(!fvip_str_is_sutf8(bad_trunc));
    }
    {
        size_t n = 0;
        CHECK(fvip_str_codepoint_count("/bin/sh", &n) == VIP_OK);
        CHECK(n == 7);
    }
}

/* ------------------------------------------------------------------ */
/* 3+4. Storage / MBL runtime flow                                    */
/* ------------------------------------------------------------------ */

static void test_storage_mbl_flow(void) {
    fvip_table_t table;
    fvip_table_t orphan;
    fvip_entry_t entry;
    uint64_t addr = 0;
    size_t i;
    uint32_t flags;

    /* Layout constants match the MBL disk image spec */
    CHECK(VIP_SECTOR_SIZE == 512U);
    CHECK(VIP_MBL_RESERVED_LBA_COUNT == 128U);
    CHECK((uint64_t)VIP_MBL_RESERVED_LBA_COUNT * VIP_SECTOR_SIZE == 0x10000ULL);
    CHECK(VIP_OWFS_PARTITION_LBA == 131200U);

    /* Registry lifecycle */
    CHECK(univip_register_volume(7, VIP_OWFS_PARTITION_LBA, "early") ==
          VIP_ERR_SYSTEM_NOT_READY);
    CHECK(univip_init_system() == VIP_INIT_OK);
    CHECK(univip_register_volume(7, VIP_OWFS_PARTITION_LBA, "sysdisk") ==
          VIP_REGISTER_OK);
    CHECK(univip_register_volume(7, VIP_OWFS_PARTITION_LBA, "dup") ==
          VIP_ERR_ALREADY_EXISTS);
    {
        static const char bad_label[] = { 'v', (char)0xC0, (char)0x80, 'l', 0 };
        CHECK(univip_register_volume(8, 0, bad_label) == VIP_ERR_INVALID_SUTF8);
    }
    CHECK(univip_resolve_volume(7, &addr) == VIP_RESOLVE_OK);
    CHECK(addr == VIP_OWFS_PARTITION_LBA);
    CHECK(univip_resolve_volume(99, &addr) == VIP_ERR_VOLUME_NOT_REGISTERED);

    /* FVIP indexing on the registered volume */
    CHECK(fvip_init_volume_index(7, &table) == VIP_OK);

    flags = fvip_flags_from_storage_entry(OWFS_ENTRY_FILE,
                                          OWFS_SEC_READONLY |
                                          OWFS_SEC_ENCRYPTED);
    CHECK(flags == (FVIP_FLAG_FILE | FVIP_FLAG_READONLY | FVIP_FLAG_ENCRYPTED));
    CHECK(fvip_insert_entry(&table, "/kernel.bin", 0x200000ULL, flags) ==
          VIP_INSERT_OK);
    CHECK(fvip_insert_entry(&table, "/kernel.bin", 0, 0) ==
          VIP_ERR_ALREADY_EXISTS);
    {
        static const char bad_path[] = { '/', (char)0xE0, (char)0x80, (char)0x80, 0 };
        CHECK(fvip_insert_entry(&table, bad_path, 0, 0) ==
              VIP_ERR_INVALID_SUTF8);
    }
    CHECK(fvip_insert_entry(&table, "", 0, 0) == VIP_ERR_PATH_TOO_LONG);

    CHECK(fvip_lookup_entry(&table, "/kernel.bin", &entry) == VIP_LOOKUP_OK);
    CHECK(entry.sector_offset == 0x200000ULL);
    CHECK(entry.flags == flags);
    CHECK(entry.codepoint_metadata == strlen("/kernel.bin"));
    CHECK((entry.flags & FVIP_FLAG_CATALOG) == 0);

    /* Absolute addressing: MBL Block I/O LBA + raw drive byte offset */
    CHECK(fvip_entry_absolute_byte(&table, &entry, &addr) == VIP_OK);
    CHECK(addr == (uint64_t)VIP_OWFS_PARTITION_LBA * VIP_SECTOR_SIZE +
                        0x200000ULL);
    CHECK(fvip_entry_absolute_lba(&table, &entry, &addr) == VIP_OK);
    CHECK(addr == VIP_OWFS_PARTITION_LBA + (0x200000ULL / VIP_SECTOR_SIZE));

    /* Catalog entry with hidden security bit */
    flags = fvip_flags_from_storage_entry(OWFS_ENTRY_CATALOG, OWFS_SEC_HIDDEN);
    CHECK(fvip_insert_entry(&table, "/etc", 0x1000ULL, flags) == VIP_INSERT_OK);
    CHECK(fvip_lookup_entry(&table, "/etc", &entry) == VIP_LOOKUP_OK);
    CHECK((entry.flags & FVIP_FLAG_CATALOG) != 0);
    CHECK((entry.flags & FVIP_FLAG_FILE) == 0);
    CHECK((entry.flags & FVIP_FLAG_HIDDEN) != 0);

    /* Unregistered volume cannot resolve addresses */
    CHECK(fvip_init_volume_index(999, &orphan) == VIP_OK);
    CHECK(orphan.volume_id == 999);
    CHECK(fvip_lookup_entry(&orphan, "/kernel.bin", &entry) == VIP_ERR_NOT_FOUND);
    CHECK(fvip_insert_entry(&orphan, "/x", 0, FVIP_FLAG_FILE) == VIP_INSERT_OK);
    CHECK(fvip_lookup_entry(&orphan, "/x", &entry) == VIP_LOOKUP_OK);
    CHECK(fvip_entry_absolute_lba(&orphan, &entry, &addr) ==
          VIP_ERR_VOLUME_NOT_REGISTERED);

    /* Removal + integrity */
    CHECK(fvip_remove_entry(&table, "/kernel.bin") == VIP_REMOVE_OK);
    CHECK(fvip_lookup_entry(&table, "/kernel.bin", &entry) == VIP_ERR_NOT_FOUND);
    CHECK(fvip_remove_entry(&table, "/kernel.bin") == VIP_ERR_NOT_FOUND);
    CHECK(fvip_verify_integrity(&table) == VIP_INTEGRITY_OK);

    /* Tampered codepoint metadata is a fatal B+ diagnostic */
    for (i = 0; i < FVIP_MAX_ENTRIES; ++i) {
        if (table.entries[i].occupied && table.entries[i].path[1] == 'e') {
            table.entries[i].codepoint_metadata += 1;
        }
    }
    CHECK(vip_code_is_fatal(fvip_verify_integrity(&table)));

    /* Uninitialized table rejects operations */
    {
        fvip_table_t cold;
        memset(&cold, 0, sizeof(cold));
        CHECK(fvip_insert_entry(&cold, "/a", 0, 0) == VIP_ERR_NOT_INITIALIZED);
        CHECK(fvip_verify_integrity(&cold) == VIP_ERR_NOT_INITIALIZED);
    }
}

int main(void) {
    test_bancode_alignment();
    test_sutf8_parity();
    test_storage_mbl_flow();

    printf("fs-first include-order checks: %s\n",
           compat_check_fs_macros() ? "PASS" : "FAIL");
    if (!compat_check_fs_macros()) {
        ++g_fail;
    }

    printf("\ncompat_vip: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
