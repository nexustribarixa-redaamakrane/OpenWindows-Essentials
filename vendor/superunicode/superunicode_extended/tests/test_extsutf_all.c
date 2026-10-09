/**
 * Test Suite: ExtSUCS Character Encoding & vSUTF Transformation Format
 *
 * Verifies: ExtSUCS types/validators, upcast/downcast, and vSUTF variable
 * streaming transformation.
 *
 * NOTE: Fixed-width (SUST-32/64/128/256/512/N) and page-mapped IPC (e-SUST)
 * serialization transports were relocated to the SUST library — see
 * sust/tests/test_sust_all.c.
 *
 * NOTE: This test file uses host CRT (<stdio.h>, <assert.h>) for verification.
 * The library code under test remains strictly freestanding.
 */

#include <stdio.h>
#include <assert.h>
#include "extsucs_types.h"
#include "vsutf.h"

/* ============================================================================
 * Test: ExtSUCS Character Encoding Validators
 * ============================================================================ */
void test_extsucs_validators(void) {
    /* All 64-bit values are valid EXCEPT the inherited trap range */
    assert(extsucs_is_valid(0x00000000ULL) == true);
    assert(extsucs_is_valid(0x00000041ULL) == true);
    assert(extsucs_is_valid(0x0010FFFFULL) == true);

    /* 0x7FFFFFFF IS valid in ExtSUCS (unbounded encoding, no sentinel) */
    assert(extsucs_is_valid(0x7FFFFFFFULL) == true);

    /* Inherited Kernel Security Trap Range — invalid in both encodings */
    assert(extsucs_is_valid(0x7FFFFFF0ULL) == false);
    assert(extsucs_is_valid(0x7FFFFFF5ULL) == false);
    assert(extsucs_is_valid(0x7FFFFFFEULL) == false);

    /* Extended range beyond Base SUCS — all valid */
    assert(extsucs_is_valid(0x80000000ULL) == true);
    assert(extsucs_is_valid(0xFFFFFFFFULL) == true);
    assert(extsucs_is_valid(0x100000000ULL) == true);
    assert(extsucs_is_valid(0xFFFFFFFFFFFFFFFFULL) == true);

    /* Base SUCS range detection */
    assert(extsucs_is_base_sucs(0x00000041ULL) == true);
    assert(extsucs_is_base_sucs(0x7FFFFFFFULL) == true);
    assert(extsucs_is_base_sucs(0x80000000ULL) == false);

    printf("[PASS] test_extsucs_validators\n");
}

/* ============================================================================
 * Test: System Control Plane (SCP) & BANcode Registry Ranges
 * ============================================================================ */
void test_scp_bancode_ranges(void) {
    /* SCP boundaries (Zone 0, District 0x11) — inherited identically */
    assert(extsucs_is_scp_plane(0x00110000ULL) == true);
    assert(extsucs_is_scp_plane(0x0011FFFFULL) == true);
    assert(extsucs_is_scp_plane(0x0010FFFFULL) == false);
    assert(extsucs_is_scp_plane(0x00120000ULL) == false);

    /* SCP codepoints remain valid ExtSUCS addresses (not rejected) */
    assert(extsucs_is_valid(0x00110000ULL) == true);
    assert(extsucs_is_valid(0x0011A005ULL) == true);

    /* BANcode Registry plugin range (inside SCP) */
    assert(extsucs_is_bancode_registry(0x0011A000ULL) == true);
    assert(extsucs_is_bancode_registry(0x0011AEFFULL) == true);
    assert(extsucs_is_bancode_registry(0x0011A7FFULL) == true);
    assert(extsucs_is_bancode_registry(0x0011AFFFULL) == false);
    assert(extsucs_is_bancode_registry(0x00110000ULL) == false);

    /* B+ BANcode (Fatal kernel errors) */
    assert(extsucs_is_bancode(0x0011A000ULL) == true);
    assert(extsucs_is_bancode(0x0011A7FFULL) == true);
    assert(extsucs_is_bancode(0x0011A800ULL) == false);

    /* W+ WARNcode */
    assert(extsucs_is_warncode(0x0011A800ULL) == true);
    assert(extsucs_is_warncode(0x0011ABFFULL) == true);
    assert(extsucs_is_warncode(0x0011AC00ULL) == false);

    /* C+ COMcode */
    assert(extsucs_is_comcode(0x0011AC00ULL) == true);
    assert(extsucs_is_comcode(0x0011ADFFULL) == true);
    assert(extsucs_is_comcode(0x0011AE00ULL) == false);

    /* S+ SOFTcode */
    assert(extsucs_is_softcode(0x0011AE00ULL) == true);
    assert(extsucs_is_softcode(0x0011AEFFULL) == true);
    assert(extsucs_is_softcode(0x0011AF00ULL) == false);

    /* Classification */
    assert(extsucs_classify_bancode(0x0011A005ULL) == SUCS_BANCODE_FATAL);
    assert(extsucs_classify_bancode(0x0011A850ULL) == SUCS_BANCODE_WARN);
    assert(extsucs_classify_bancode(0x0011AC20ULL) == SUCS_BANCODE_COM);
    assert(extsucs_classify_bancode(0x0011AE10ULL) == SUCS_BANCODE_SOFT);
    assert(extsucs_classify_bancode(0x0011AFFFULL) == SUCS_BANCODE_NONE);

    /* Trap range is NOT part of the SCP and remains rejected */
    assert(extsucs_is_scp_plane(0x7FFFFFF0ULL) == false);
    assert(extsucs_is_valid(0x7FFFFFF5ULL) == false);

    printf("[PASS] test_scp_bancode_ranges\n");
}

/* ============================================================================
 * Test: Zero-Cost Upcast & Safe Downcast
 * ============================================================================ */
void test_upcast_downcast(void) {
    sucs_char_t base_cp = 0x41;
    sucs_ex_char_t ex_cp = sucs_upcast(base_cp);
    assert(ex_cp == 0x41ULL);

    /* Downcast valid Base SUCS codepoint */
    sucs_char_t out_cp = 0;
    assert(sucs_downcast(0x41ULL, &out_cp) == true);
    assert(out_cp == 0x41);

    /* Downcast extended range — fails (exceeds Base SUCS boundary) */
    assert(sucs_downcast(0x80000000ULL, &out_cp) == false);
    assert(out_cp == SUCS_INVALID_CODEPOINT);

    /* Downcast 0x7FFFFFFF — fails (Base SUCS sentinel, valid in ExtSUCS but not Base) */
    assert(sucs_downcast(0x7FFFFFFFULL, &out_cp) == false);

    /* Downcast trap range — fails */
    assert(sucs_downcast(0x7FFFFFF5ULL, &out_cp) == false);

    printf("[PASS] test_upcast_downcast\n");
}

/* ============================================================================
 * Test: vSUTF Variable Streaming Transformation
 * ============================================================================ */
void test_vsutf(void) {
    uint8_t buf[16];
    sucs_ex_char_t decoded = 0;

    /* 1-byte ASCII fast-path */
    assert(vsutf_encode(0x41ULL, buf, sizeof(buf)) == 1);
    assert(vsutf_decode(buf, 1, &decoded) == 1);
    assert(decoded == 0x41ULL);

    /* 3-byte (0xD800 — valid PUA in SUCS, no surrogates) */
    assert(vsutf_encode(0xD800ULL, buf, sizeof(buf)) == 3);
    assert(vsutf_decode(buf, 3, &decoded) == 3);
    assert(decoded == 0xD800ULL);

    /* 6-byte max Base SUCS (0x7FFFFFFF — valid in ExtSUCS) */
    assert(vsutf_encode(0x7FFFFFFFULL, buf, sizeof(buf)) == 6);
    assert(vsutf_decode(buf, 6, &decoded) == 6);
    assert(decoded == 0x7FFFFFFFULL);

    /* 9-byte extended (0xFE prefix + 8 bytes) */
    assert(vsutf_encode(0x80000000ULL, buf, sizeof(buf)) == 9);
    assert(buf[0] == 0xFE);
    assert(vsutf_decode(buf, 9, &decoded) == 9);
    assert(decoded == 0x80000000ULL);

    /* 9-byte extended (max 64-bit value) */
    assert(vsutf_encode(0xFFFFFFFFFFFFFFFFULL, buf, sizeof(buf)) == 9);
    assert(buf[0] == 0xFE);
    assert(vsutf_decode(buf, 9, &decoded) == 9);
    assert(decoded == 0xFFFFFFFFFFFFFFFFULL);

    /* Trap range rejection */
    assert(vsutf_encode(0x7FFFFFF5ULL, buf, sizeof(buf)) == 0);

    /* Overlong extended frame: 0xFE + 8 bytes encoding a base-range value
     * (must use 1-6 byte base framing instead) */
    buf[0] = 0xFE; buf[1] = 0x00; buf[2] = 0x00; buf[3] = 0x00; buf[4] = 0x00;
    buf[5] = 0x00; buf[6] = 0x00; buf[7] = 0x00; buf[8] = 0x41;
    assert(vsutf_decode(buf, 9, &decoded) == 0);

    /* Truncated extended frame (fewer than 9 bytes) */
    assert(vsutf_decode(buf, 8, &decoded) == 0);

    /* 0xFF reserved prefix rejection on decode */
    buf[0] = 0xFF;
    assert(vsutf_decode(buf, 9, &decoded) == 0);

    printf("[PASS] test_vsutf (Variable Streaming Transformation)\n");
}

/* ============================================================================
 * Main
 * ============================================================================ */
int main(void) {
    printf("=====================================================\n");
    printf(" RUNNING ALL EXTSUTF TRANSFORMATION FORMAT TESTS     \n");
    printf("=====================================================\n");
    test_extsucs_validators();
    test_scp_bancode_ranges();
    test_upcast_downcast();
    test_vsutf();
    printf("=====================================================\n");
    printf(" ALL EXTSUTF TRANSFORMATION FORMAT TESTS PASSED!     \n");
    printf("=====================================================\n");
    return 0;
}
