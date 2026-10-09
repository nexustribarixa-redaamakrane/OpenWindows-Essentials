#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "extsucs_compat.h"
#include "extsucs_ucd_names.h"

/* ExtSUCS has no UCD data of its own: it delegates to the base library's
 * single shared database. These expectations must therefore stay identical
 * to superunicode/tests/test_sucs_ucd.c. If they diverge, the two modules
 * have drifted apart and the bridge is no longer 1:1 across both. */

#define EXPECT_UNICODE_MAJOR 18
#define EXPECT_UNICODE_MINOR 0
#define EXPECT_BLOCK_COUNT   353
#define EXPECT_NAME_COUNT    41232

void test_ext_ucd_version(void) {
    assert(EXTSUCS_UNICODE_VERSION_MAJOR == EXPECT_UNICODE_MAJOR);
    assert(EXTSUCS_UNICODE_VERSION_MINOR == EXPECT_UNICODE_MINOR);
    assert(EXTSUCS_UCD_UNICODE_VERSION == EXPECT_UNICODE_MAJOR * 100 + EXPECT_UNICODE_MINOR);
    assert(EXTSUCS_UCD_BLOCK_COUNT == EXPECT_BLOCK_COUNT);
    assert(EXTSUCS_UCD_NAME_COUNT == EXPECT_NAME_COUNT);

    /* The aliases must not silently diverge from the base macros */
    assert(EXTSUCS_UCD_BLOCK_COUNT == SUCS_UCD_BLOCK_COUNT);
    assert(EXTSUCS_UCD_NAME_COUNT == SUCS_UCD_NAME_COUNT);
    assert(EXTSUCS_UCD_UNICODE_VERSION == SUCS_UCD_UNICODE_VERSION);

    printf("[PASS] test_ext_ucd_version (Unicode %d.%d, %d blocks, %d names)\n",
           EXTSUCS_UNICODE_VERSION_MAJOR, EXTSUCS_UNICODE_VERSION_MINOR,
           EXTSUCS_UCD_BLOCK_COUNT, EXTSUCS_UCD_NAME_COUNT);
}

/* The 64-bit wrappers must reject codepoints above the bridge rather than
 * truncating them into the Unicode range. */
void test_ext_range_guards(void) {
    assert(extsucs_is_unicode_compat(0x0010FFFF) == true);
    assert(extsucs_is_unicode_compat(0x00110000) == false);
    assert(extsucs_is_native_extended(0x00110000) == true);
    assert(extsucs_is_native_extended(0x0010FFFF) == false);

    /* Values that would alias to a valid Unicode codepoint if truncated */
    assert(extsucs_ucd_block_lookup(0x1000011DF0ULL) == -1);
    assert(extsucs_ucd_get_name(0x1000011DF0ULL) == NULL);
    assert(extsucs_ucd_name_length(0x20000041ULL) == 0);

    assert(extsucs_ucd_block_lookup(0x00110000) == -1);
    assert(extsucs_ucd_get_name(0x00110000) == NULL);

    printf("[PASS] test_ext_range_guards\n");
}

static void assert_ext_name(sucs_ex_char_t cp, const char* expected) {
    char buf[128];
    uint16_t n = extsucs_ucd_get_name_copy(cp, buf, sizeof(buf));
    assert(n == (uint16_t)strlen(expected));
    assert(strcmp(buf, expected) == 0);
}

void test_ext_unicode18_names(void) {
    assert_ext_name(0x0041, "LATIN CAPITAL LETTER A");

    /* Codepoints introduced by Unicode 18.0, resolved through the ExtSUCS API */
    assert_ext_name(0x11DF0, "BENGALI SIGN COMBINING ANUSVARA ABOVE");
    assert_ext_name(0x12550, "CUNEIFORM NUMERIC SIGN ONE N01");
    assert_ext_name(0x191A0, "JURCHEN RADICAL-01");
    assert_ext_name(0x1D250, "MUSICAL SYMBOL COMBINING FLAG-6");
    assert_ext_name(0x1DB00, "LEIBNIZIAN EQUALS SIGN");

    printf("[PASS] test_ext_unicode18_names\n");
}

/* The base and extended lookups must agree on every Unicode 18.0 block. */
void test_ext_unicode18_blocks(void) {
    static const sucs_char_t probes[] = {
        SUCS_UCD_BLOCK_BENGALI_SUPPLEMENT_MIN,
        SUCS_UCD_BLOCK_ARCHAIC_CUNEIFORM_NUMERALS_MIN,
        SUCS_UCD_BLOCK_JURCHEN_MIN,
        SUCS_UCD_BLOCK_JURCHEN_RADICALS_MIN,
        SUCS_UCD_BLOCK_MUSICAL_SYMBOLS_SUPPLEMENT_MIN,
        SUCS_UCD_BLOCK_MISCELLANEOUS_SYMBOLS_AND_ARROWS_EXTENDED_MIN,
        SUCS_UCD_BLOCK_SEAL_MIN,
        SUCS_UCD_BLOCK_SEAL_MAX,
    };

    for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        int base = sucs_ucd_block_lookup(probes[i]);
        int ext = extsucs_ucd_block_lookup((sucs_ex_char_t)probes[i]);
        assert(base >= 0);
        assert(base == ext);
        assert(extsucs_is_in_ucd_block((sucs_ex_char_t)probes[i], ext) == true);
    }

    assert(extsucs_ucd_block_lookup(0x0010FFFF) == SUCS_UCD_BLOCK_COUNT - 1);

    printf("[PASS] test_ext_unicode18_blocks\n");
}

int main(void) {
    printf("--- Running ExtSUCS UCD (shared UCS/UCB) Tests ---\n");
    test_ext_ucd_version();
    test_ext_range_guards();
    test_ext_unicode18_names();
    test_ext_unicode18_blocks();
    printf("--- ALL EXTSUCS UCD TESTS PASSED ---\n");
    return 0;
}
