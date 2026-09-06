/**
 * @file test_extconv.c
 * @brief Unit tests for ExtSUCS / vSUTF <-> Unicode / UTF-8 conversion functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "extsucs_conv.h"

static void test_utf8_to_vsutf_basic(void) {
    const uint8_t src[] = "ExtSUCS Test \xc3\xa9 \xe2\x9c\x93 \xf0\x9f\x98\x80";
    uint8_t vsutf[128];
    size_t vsutf_len = 0;
    extsucs_conv_status_t st = extsucs_conv_utf8_to_vsutf(src, sizeof(src) - 1, vsutf, sizeof(vsutf), &vsutf_len);
    assert(st == EXTSUCS_CONV_OK);
    assert(vsutf_len == sizeof(src) - 1);
    assert(memcmp(src, vsutf, vsutf_len) == 0);

    uint8_t utf[128];
    size_t utf_len = 0;
    st = extsucs_conv_vsutf_to_utf8(vsutf, vsutf_len, utf, sizeof(utf), &utf_len, true);
    assert(st == EXTSUCS_CONV_OK);
    assert(utf_len == sizeof(src) - 1);
    assert(memcmp(src, utf, utf_len) == 0);
    printf("  [PASS] test_utf8_to_vsutf_basic\n");
}

static void test_vsutf_64bit_extended(void) {
    /* 64-bit ExtSUCS codepoint: 0x100000000ULL (requires 0xFE prefix + 8 bytes in vSUTF) */
    uint8_t vsutf_buf[16];
    size_t written = vsutf_encode(0x100000000ULL, vsutf_buf, sizeof(vsutf_buf));
    assert(written == 9);
    assert(vsutf_buf[0] == 0xFE);

    /* In strict mode, should fail */
    uint8_t utf[32];
    size_t utf_len = 0;
    extsucs_conv_status_t st = extsucs_conv_vsutf_to_utf8(vsutf_buf, written, utf, sizeof(utf), &utf_len, true);
    assert(st == EXTSUCS_CONV_ERR_OUT_OF_RANGE);

    /* In lenient mode, should emit U+FFFD (0xEF 0xBF 0xBD) */
    utf_len = 0;
    st = extsucs_conv_vsutf_to_utf8(vsutf_buf, written, utf, sizeof(utf), &utf_len, false);
    assert(st == EXTSUCS_CONV_OK);
    assert(utf_len == 3);
    assert(utf[0] == 0xEF && utf[1] == 0xBF && utf[2] == 0xBD);
    printf("  [PASS] test_vsutf_64bit_extended\n");
}

static void test_extsucs_array_conversion(void) {
    uint32_t unicode_cps[] = { 0x0041, 0x00E9, 0x2713, 0x1F600 };
    sucs_ex_char_t ex_cps[4];
    size_t out_count = 0;

    extsucs_conv_status_t st = extsucs_conv_unicode_to_extsucs(unicode_cps, 4, ex_cps, &out_count);
    assert(st == EXTSUCS_CONV_OK);
    assert(out_count == 4);
    assert(ex_cps[0] == 0x0041);
    assert(ex_cps[1] == 0x00E9);
    assert(ex_cps[2] == 0x2713);
    assert(ex_cps[3] == 0x1F600);

    uint32_t roundtrip[4];
    st = extsucs_conv_extsucs_to_unicode(ex_cps, 4, roundtrip, &out_count, true);
    assert(st == EXTSUCS_CONV_OK);
    assert(out_count == 4);
    assert(memcmp(unicode_cps, roundtrip, sizeof(unicode_cps)) == 0);
    printf("  [PASS] test_extsucs_array_conversion\n");
}

int main(void) {
    printf("=== ExtSUCS / vSUTF Conversion Unit Tests ===\n");
    test_utf8_to_vsutf_basic();
    test_vsutf_64bit_extended();
    test_extsucs_array_conversion();
    printf("All ExtSUCS conversion tests passed!\n");
    return 0;
}
