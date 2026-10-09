#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "superunicode/superunicode.h"
#include "superunicode/sucs_ucd_names.h"

/* The UCS/UCB (Unicode Compatibility Space / Bridge) data set shipped with
 * this module. Bump these together with SUCS_UNICODE_VERSION_* and the
 * block/name tables regenerated from unicode.org. */

#define EXPECT_UNICODE_MAJOR 18
#define EXPECT_UNICODE_MINOR 0
#define EXPECT_BLOCK_COUNT   353
#define EXPECT_NAME_COUNT    41232

void test_ucd_version_macros(void) {
    assert(SUCS_UNICODE_VERSION_MAJOR == EXPECT_UNICODE_MAJOR);
    assert(SUCS_UNICODE_VERSION_MINOR == EXPECT_UNICODE_MINOR);
    assert(SUCS_UCD_UNICODE_VERSION == EXPECT_UNICODE_MAJOR * 100 + EXPECT_UNICODE_MINOR);

    assert(SUCS_UCD_BLOCK_COUNT == EXPECT_BLOCK_COUNT);
    assert(SUCS_UCD_NAME_COUNT == EXPECT_NAME_COUNT);

    printf("[PASS] test_ucd_version_macros (Unicode %d.%d, %d blocks, %d names)\n",
           SUCS_UNICODE_VERSION_MAJOR, SUCS_UNICODE_VERSION_MINOR,
           SUCS_UCD_BLOCK_COUNT, SUCS_UCD_NAME_COUNT);
}

/* The block table is binary-searched, so it must stay strictly ascending
 * and non-overlapping. This is the invariant that silently breaks lookups
 * when a block is inserted out of order. */
void test_block_table_is_sorted(void) {
    for (int i = 1; i < SUCS_UCD_BLOCK_COUNT; i++) {
        assert(sucs_ucd_blocks[i].min > sucs_ucd_blocks[i - 1].max);
    }
    printf("[PASS] test_block_table_is_sorted\n");
}

void test_block_lookup(void) {
    /* Basic Latin (first block) and Supplementary Private Use Area-B (last) */
    int first = sucs_ucd_block_lookup(0x0000);
    assert(first == 0);
    assert(sucs_is_in_ucd_block(0x0041, first) == true);

    int last = sucs_ucd_block_lookup(0x10FFFF);
    assert(last == SUCS_UCD_BLOCK_COUNT - 1);
    assert(sucs_is_in_ucd_block(0x10FFFF, last) == true);

    /* Boundaries resolve to the containing block, not the neighbour */
    int ascii = sucs_ucd_block_lookup(0x007F);
    assert(sucs_ucd_blocks[ascii].min == 0x0000);
    assert(sucs_ucd_blocks[ascii].max == 0x007F);
    int latin1 = sucs_ucd_block_lookup(0x0080);
    assert(sucs_ucd_blocks[latin1].min == 0x0080);

    /* Native SUCS and SCP space is not part of the Unicode block table */
    assert(sucs_ucd_block_lookup(0x00110000) == -1);
    assert(sucs_ucd_block_lookup(0x00120000) == -1);

    /* Out-of-range indices rejected */
    assert(sucs_is_in_ucd_block(0x0041, -1) == false);
    assert(sucs_is_in_ucd_block(0x0041, SUCS_UCD_BLOCK_COUNT) == false);

    printf("[PASS] test_block_lookup\n");
}

/* Blocks introduced by Unicode 18.0 — the UCS/UCB must expose them. */
void test_unicode18_blocks(void) {
    assert(SUCS_UCD_BLOCK_BENGALI_SUPPLEMENT_MIN == 0x11DF0);
    assert(SUCS_UCD_BLOCK_BENGALI_SUPPLEMENT_MAX == 0x11DFF);
    assert(SUCS_UCD_BLOCK_ARCHAIC_CUNEIFORM_NUMERALS_MIN == 0x12550);
    assert(SUCS_UCD_BLOCK_ARCHAIC_CUNEIFORM_NUMERALS_MAX == 0x1268F);
    assert(SUCS_UCD_BLOCK_JURCHEN_MIN == 0x18E00);
    assert(SUCS_UCD_BLOCK_JURCHEN_MAX == 0x1919F);
    assert(SUCS_UCD_BLOCK_JURCHEN_RADICALS_MIN == 0x191A0);
    assert(SUCS_UCD_BLOCK_JURCHEN_RADICALS_MAX == 0x191DF);
    assert(SUCS_UCD_BLOCK_MUSICAL_SYMBOLS_SUPPLEMENT_MIN == 0x1D250);
    assert(SUCS_UCD_BLOCK_MUSICAL_SYMBOLS_SUPPLEMENT_MAX == 0x1D28F);
    assert(SUCS_UCD_BLOCK_MISCELLANEOUS_SYMBOLS_AND_ARROWS_EXTENDED_MIN == 0x1DB00);
    assert(SUCS_UCD_BLOCK_MISCELLANEOUS_SYMBOLS_AND_ARROWS_EXTENDED_MAX == 0x1DBFF);
    assert(SUCS_UCD_BLOCK_SEAL_MIN == 0x3D000);
    assert(SUCS_UCD_BLOCK_SEAL_MAX == 0x3FC3F);

    /* Each resolves through the same table the defines describe. */
    assert(sucs_ucd_block_lookup(SUCS_UCD_BLOCK_BENGALI_SUPPLEMENT_MIN) >= 0);
    assert(sucs_ucd_block_lookup(SUCS_UCD_BLOCK_ARCHAIC_CUNEIFORM_NUMERALS_MIN) >= 0);
    assert(sucs_ucd_block_lookup(SUCS_UCD_BLOCK_JURCHEN_MIN) >= 0);
    assert(sucs_ucd_block_lookup(SUCS_UCD_BLOCK_MUSICAL_SYMBOLS_SUPPLEMENT_MIN) >= 0);
    assert(sucs_ucd_block_lookup(SUCS_UCD_BLOCK_MISCELLANEOUS_SYMBOLS_AND_ARROWS_EXTENDED_MIN) >= 0);
    assert(sucs_ucd_block_lookup(SUCS_UCD_BLOCK_SEAL_MIN) >= 0);
    assert(sucs_is_in_ucd_block(SUCS_UCD_BLOCK_SEAL_MIN,
                                sucs_ucd_block_lookup(SUCS_UCD_BLOCK_SEAL_MIN)) == true);
    assert(sucs_is_in_ucd_block(SUCS_UCD_BLOCK_SEAL_MAX,
                                sucs_ucd_block_lookup(SUCS_UCD_BLOCK_SEAL_MAX)) == true);

    printf("[PASS] test_unicode18_blocks\n");
}

/* The name pool is packed without NUL terminators, so sucs_ucd_get_name()
 * returns a length-bounded slice — compare it with memcmp, never strcmp. */
static void assert_name(sucs_char_t cp, const char* expected) {
    const size_t expected_len = strlen(expected);

    const char* got = sucs_ucd_get_name(cp);
    assert(got != NULL);
    assert(sucs_ucd_name_length(cp) == (uint16_t)expected_len);
    assert(memcmp(got, expected, expected_len) == 0);

    /* The copy accessor NUL-terminates for callers that want a C string */
    char buf[128];
    uint16_t n = sucs_ucd_get_name_copy(cp, buf, sizeof(buf));
    assert(n == (uint16_t)expected_len);
    assert(strcmp(buf, expected) == 0);
}

void test_ucd_name_lookup(void) {
    /* Stable names are unchanged by the version bump */
    assert_name(0x0041, "LATIN CAPITAL LETTER A");
    assert_name(0x00E9, "LATIN SMALL LETTER E WITH ACUTE");
    assert_name(0x0301, "COMBINING ACUTE ACCENT");
    assert_name(0x1F600, "GRINNING FACE");

    /* Codepoints introduced by Unicode 18.0 */
    assert_name(0x11DF0, "BENGALI SIGN COMBINING ANUSVARA ABOVE");
    assert_name(0x12550, "CUNEIFORM NUMERIC SIGN ONE N01");
    assert_name(0x191A0, "JURCHEN RADICAL-01");
    assert_name(0x1D250, "MUSICAL SYMBOL COMBINING FLAG-6");
    assert_name(0x1DB00, "LEIBNIZIAN EQUALS SIGN");

    /* CJK Unified Ideographs and Han-internal ranges are named in
     * UnicodeData.txt with <CJK Ideograph, First>/<CJK Ideograph, Last>
     * range markers, which carry no per-codepoint name. */
    assert(sucs_ucd_get_name(0x4E00) == NULL);
    assert(sucs_ucd_get_name(0x9FFF) == NULL);

    /* Unassigned codepoints inside a Unicode 18.0 block have no name */
    assert(sucs_ucd_get_name(SUCS_UCD_BLOCK_JURCHEN_MIN) == NULL);

    /* Above the bridge: no name, and the lookup must not read out of bounds */
    assert(sucs_ucd_get_name(0x00110000) == NULL);
    assert(sucs_ucd_name_length(0x00120000) == 0);
    assert(sucs_ucd_get_name(SUCS_MAX_CODEPOINT) == NULL);

    /* Buffer too small copies nothing */
    char small[4];
    assert(sucs_ucd_get_name_copy(0x0041, small, sizeof(small)) == 0);

    printf("[PASS] test_ucd_name_lookup\n");
}

int main(void) {
    printf("--- Running SUCS UCD (Unicode Compatibility Space/Bridge) Tests ---\n");
    test_ucd_version_macros();
    test_block_table_is_sorted();
    test_block_lookup();
    test_unicode18_blocks();
    test_ucd_name_lookup();
    printf("--- ALL UCD TESTS PASSED ---\n");
    return 0;
}
