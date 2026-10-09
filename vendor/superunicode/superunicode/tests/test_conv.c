/**
 * @file test_conv.c
 * @brief Unit tests for SuperUnicode <-> Unicode conversion functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "superunicode/superunicode.h"

static void test_utf8_to_sutf8_basic(void) {
    /* ASCII string */
    const uint8_t ascii[] = "Hello World!";
    uint8_t out[64];
    size_t written = 0;
    sucs_conv_status_t st = sucs_conv_utf8_to_sutf8(ascii, sizeof(ascii) - 1, out, sizeof(out), &written);
    assert(st == SUCS_CONV_OK);
    assert(written == sizeof(ascii) - 1);
    assert(memcmp(ascii, out, written) == 0);

    /* Multibyte UTF-8 string: "Hello \xc3\xa9 \xe2\x9c\x93 \xf0\x9f\x98\x80" (Hello é ✓ 😀) */
    const uint8_t utf8_multi[] = "Hello \xc3\xa9 \xe2\x9c\x93 \xf0\x9f\x98\x80";
    written = 0;
    st = sucs_conv_utf8_to_sutf8(utf8_multi, sizeof(utf8_multi) - 1, out, sizeof(out), &written);
    assert(st == SUCS_CONV_OK);
    assert(written == sizeof(utf8_multi) - 1);
    assert(memcmp(utf8_multi, out, written) == 0);
    printf("  [PASS] test_utf8_to_sutf8_basic\n");
}

static void test_sutf8_to_utf8_roundtrip(void) {
    const uint8_t src[] = "Testing 1, 2, 3... \xc3\xb1 \xe2\x98\x85 \xf0\x9f\x9a\x80";
    uint8_t sutf[128];
    size_t sutf_len = 0;
    sucs_conv_status_t st = sucs_conv_utf8_to_sutf8(src, sizeof(src) - 1, sutf, sizeof(sutf), &sutf_len);
    assert(st == SUCS_CONV_OK);

    uint8_t utf[128];
    size_t utf_len = 0;
    st = sucs_conv_sutf8_to_utf8(sutf, sutf_len, utf, sizeof(utf), &utf_len, true);
    assert(st == SUCS_CONV_OK);
    assert(utf_len == sizeof(src) - 1);
    assert(memcmp(src, utf, utf_len) == 0);
    printf("  [PASS] test_sutf8_to_utf8_roundtrip\n");
}

static void test_sutf8_to_utf8_extended_lenient(void) {
    /* Create a 5-byte native SUCS SUTF-8 sequence (codepoint 0x00123456) */
    char sutf_buf[8];
    size_t written = 0;
    int est = sutf_encode_char(0x00123456UL, sutf_buf, sizeof(sutf_buf), &written);
    assert(est == SUES_SUCCESS);
    assert(written == 5);

    /* In strict mode, should fail */
    uint8_t utf[32];
    size_t utf_len = 0;
    sucs_conv_status_t st = sucs_conv_sutf8_to_utf8((const uint8_t *)sutf_buf, written, utf, sizeof(utf), &utf_len, true);
    assert(st == SUCS_CONV_ERR_OUT_OF_RANGE);

    /* In lenient mode, should emit U+FFFD (0xEF 0xBF 0xBD) */
    utf_len = 0;
    st = sucs_conv_sutf8_to_utf8((const uint8_t *)sutf_buf, written, utf, sizeof(utf), &utf_len, false);
    assert(st == SUCS_CONV_OK);
    assert(utf_len == 3);
    assert(utf[0] == 0xEF && utf[1] == 0xBF && utf[2] == 0xBD);
    printf("  [PASS] test_sutf8_to_utf8_extended_lenient\n");
}

static void test_codepoint_array_conversion(void) {
    uint32_t unicode_cps[] = { 0x0041, 0x00E9, 0x2713, 0x1F600 };
    sucs_char_t sucs_cps[4];
    size_t out_count = 0;

    sucs_conv_status_t st = sucs_conv_unicode_to_sucs(unicode_cps, 4, sucs_cps, &out_count);
    assert(st == SUCS_CONV_OK);
    assert(out_count == 4);
    assert(sucs_cps[0] == 0x0041);
    assert(sucs_cps[1] == 0x00E9);
    assert(sucs_cps[2] == 0x2713);
    assert(sucs_cps[3] == 0x1F600);

    uint32_t roundtrip[4];
    st = sucs_conv_sucs_to_unicode(sucs_cps, 4, roundtrip, &out_count, true);
    assert(st == SUCS_CONV_OK);
    assert(out_count == 4);
    assert(memcmp(unicode_cps, roundtrip, sizeof(unicode_cps)) == 0);
    printf("  [PASS] test_codepoint_array_conversion\n");
}

int main(void) {
    printf("=== SuperUnicode Conversion Unit Tests ===\n");
    test_utf8_to_sutf8_basic();
    test_sutf8_to_utf8_roundtrip();
    test_sutf8_to_utf8_extended_lenient();
    test_codepoint_array_conversion();
    printf("All SuperUnicode conversion tests passed!\n");
    return 0;
}
