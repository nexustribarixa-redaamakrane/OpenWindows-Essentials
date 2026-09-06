/**
 * Test Suite: SuperUnicode Serialization Transports (SUST)
 *
 * Verifies:
 *  - SUST-16 canonical BIG-ENDIAN byte serialization of the SUTF-16 word stream
 *  - SUST-32/64/128/256/512/N fixed-width vector slot serialization
 *  - e-SUST hypervisor page-mapped virtual IPC transport
 *
 * NOTE: This test file uses host CRT (<stdio.h>, <assert.h>) for verification.
 * The library code under test remains strictly freestanding.
 */

#include <stdio.h>
#include <assert.h>
#include "sust16.h"
#include "sustfixed.h"
#include "esust.h"

/* ============================================================================
 * Test: SUST-16 Byte Serialization (canonical big-endian)
 * ============================================================================ */
void test_sust16(void) {
    sucs_char_t decoded = 0;
    uint8_t bytes[8];

    /* 1-word form: literal 0x1234 -> bytes 12 34 (high byte first). */
    size_t nb = sust16_encode_bytes(0x1234, bytes, sizeof(bytes));
    assert(nb == 2 && bytes[0] == 0x12 && bytes[1] == 0x34);
    size_t rb = sust16_decode_bytes(bytes, nb, &decoded);
    assert(rb == 2 && decoded == 0x1234);

    /* Boundary: max single-word literal 0x7FFF -> 7F FF. */
    nb = sust16_encode_bytes(0x7FFF, bytes, sizeof(bytes));
    assert(nb == 2 && bytes[0] == 0x7F && bytes[1] == 0xFF);
    rb = sust16_decode_bytes(bytes, nb, &decoded);
    assert(rb == 2 && decoded == 0x7FFF);
    assert(sust16_codepoint_bytes(0x7FFF) == 2);

    /* 2-word form: 0xD800 (PUA) -> marker 80 00 + D8 00. */
    nb = sust16_encode_bytes(0xD800, bytes, sizeof(bytes));
    assert(nb == 4 && bytes[0] == 0x80 && bytes[1] == 0x00 &&
           bytes[2] == 0xD8 && bytes[3] == 0x00);
    rb = sust16_decode_bytes(bytes, nb, &decoded);
    assert(rb == 4 && decoded == 0xD800);
    assert(sust16_codepoint_bytes(0xD800) == 4);

    /* Top of range before trap/sentinel: 0x7FFFFFEF -> FF FF FF EF. */
    nb = sust16_encode_bytes(0x7FFFFFEF, bytes, sizeof(bytes));
    assert(nb == 4 && bytes[0] == 0xFF && bytes[1] == 0xFF &&
           bytes[2] == 0xFF && bytes[3] == 0xEF);
    rb = sust16_decode_bytes(bytes, nb, &decoded);
    assert(rb == 4 && decoded == 0x7FFFFFEF);

    /* Sentinel SUCS_INVALID_CODEPOINT rejected on the byte path too. */
    assert(sust16_encode_bytes(0x7FFFFFFF, bytes, sizeof(bytes)) == 0);

    /* Truncation: lone marker word in a byte stream is rejected. */
    bytes[0] = 0x80; bytes[1] = 0x00;
    assert(sust16_decode_bytes(bytes, 2, &decoded) == 0);

    /* Byte-swapped streams: why big-endian is mandatory.
     * (a) LOUD failure: literal 0x0080 serializes as 00 80. A little-endian
     *     reader/writer produces 80 00, which the canonical decoder sees as
     *     a MARKER word with only one word present -> rejected outright,
     *     never misdecoded. */
    {
        uint8_t le_swapped[2] = { 0x80, 0x00 };
        sucs_char_t got = 0;
        assert(sust16_decode_bytes(le_swapped, 2, &got) == 0);

        /* (b) WRONG-VALUE hazard: swapping can also stay inside the literal
         * range (e.g. 0x1234 <-> 0x3412), yielding a DIFFERENT valid
         * codepoint instead of an error. This is precisely why hand-rolled
         * packing is forbidden and encode_bytes/decode_bytes are the only
         * sanctioned byte-level interface. */
        uint8_t be[2];
        assert(sust16_encode_bytes(0x1234, be, 2) == 2);
        uint8_t swapped[2] = { be[1], be[0] };
        sucs_char_t wrong = 0;
        rb = sust16_decode_bytes(swapped, 2, &wrong);
        assert(rb == 2 && wrong == 0x3412 && wrong != 0x1234);
    }

    /* Buffer-too-small rejections on the byte path. */
    assert(sust16_encode_bytes(0x1234, bytes, 1) == 0);
    assert(sust16_encode_bytes(0x10000, bytes, 3) == 0);
    assert(sust16_decode_bytes(bytes, 1, &decoded) == 0);

    /* Overlong 2-word byte stream encoding a 1-word-range value. */
    bytes[0] = 0x80; bytes[1] = 0x00; bytes[2] = 0x12; bytes[3] = 0x34;
    assert(sust16_decode_bytes(bytes, 4, &decoded) == 0);

    printf("[PASS] test_sust16 (Big-Endian Byte Serialization)\n");
}

/* ============================================================================
 * Test: SUST-16 Little-Endian Byte Serialization + explicit byte-order variants
 * ============================================================================ */
void test_sust16_le(void) {
    sucs_char_t decoded = 0;
    uint8_t bytes[8];

    /* 1-word form: literal 0x1234 -> LE bytes 34 12 (low byte first). */
    size_t nb = sust16_encode_bytes_le(0x1234, bytes, sizeof(bytes));
    assert(nb == 2 && bytes[0] == 0x34 && bytes[1] == 0x12);
    size_t rb = sust16_decode_bytes_le(bytes, nb, &decoded);
    assert(rb == 2 && decoded == 0x1234);

    /* Boundary: max single-word literal 0x7FFF -> FF 7F. */
    nb = sust16_encode_bytes_le(0x7FFF, bytes, sizeof(bytes));
    assert(nb == 2 && bytes[0] == 0xFF && bytes[1] == 0x7F);
    rb = sust16_decode_bytes_le(bytes, nb, &decoded);
    assert(rb == 2 && decoded == 0x7FFF);
    assert(sust16_codepoint_bytes(0x7FFF) == 2);

    /* 2-word form: 0xD800 (PUA) -> words {0x8000, 0xD800} -> LE 00 80 00 D8. */
    nb = sust16_encode_bytes_le(0xD800, bytes, sizeof(bytes));
    assert(nb == 4 && bytes[0] == 0x00 && bytes[1] == 0x80 &&
           bytes[2] == 0x00 && bytes[3] == 0xD8);
    rb = sust16_decode_bytes_le(bytes, nb, &decoded);
    assert(rb == 4 && decoded == 0xD800);

    /* Top of range before trap/sentinel: 0x7FFFFFEF -> words {0xFFFF,0xFFEF} */
    nb = sust16_encode_bytes_le(0x7FFFFFEF, bytes, sizeof(bytes));
    assert(nb == 4 && bytes[0] == 0xFF && bytes[1] == 0xFF &&
           bytes[2] == 0xEF && bytes[3] == 0xFF);
    rb = sust16_decode_bytes_le(bytes, nb, &decoded);
    assert(rb == 4 && decoded == 0x7FFFFFEF);

    /* Sentinel SUCS_INVALID_CODEPOINT rejected on the LE byte path too. */
    assert(sust16_encode_bytes_le(0x7FFFFFFF, bytes, sizeof(bytes)) == 0);

    /* Truncation: lone marker word in an LE byte stream is rejected. */
    bytes[0] = 0x00; bytes[1] = 0x80;
    assert(sust16_decode_bytes_le(bytes, 2, &decoded) == 0);

    /* Buffer-too-small rejections on the LE byte path. */
    assert(sust16_encode_bytes_le(0x1234, bytes, 1) == 0);
    assert(sust16_encode_bytes_le(0x10000, bytes, 3) == 0);
    assert(sust16_decode_bytes_le(bytes, 1, &decoded) == 0);

    /* Overlong 2-word LE stream encoding a 1-word-range value. */
    bytes[0] = 0x00; bytes[1] = 0x80; bytes[2] = 0x34; bytes[3] = 0x12;
    assert(sust16_decode_bytes_le(bytes, 4, &decoded) == 0);

    /* Explicit _be functions are the same canonical big-endian bytes. */
    nb = sust16_encode_bytes_be(0x1234, bytes, sizeof(bytes));
    assert(nb == 2 && bytes[0] == 0x12 && bytes[1] == 0x34);
    rb = sust16_decode_bytes_be(bytes, nb, &decoded);
    assert(rb == 2 && decoded == 0x1234);

    /* Canonical unqualified == big-endian == explicit _be. */
    nb = sust16_encode_bytes(0x1234, bytes, sizeof(bytes));
    assert(nb == 2 && bytes[0] == 0x12 && bytes[1] == 0x34);
    nb = sust16_encode_bytes_be(0x5A5A, bytes, sizeof(bytes));
    assert(nb == 2 && bytes[0] == 0x5A && bytes[1] == 0x5A);

    /* Round-trips agree between orders (values, not bytes). */
    uint8_t le_buf[4];
    uint8_t be_buf[4];
    sucs_char_t from_le = 0, from_be = 0;
    assert(sust16_encode_bytes_le(0x1234567, le_buf, 4) == 4);
    assert(sust16_encode_bytes_be(0x1234567, be_buf, 4) == 4);
    assert(sust16_decode_bytes_le(le_buf, 4, &from_le) == 4);
    assert(sust16_decode_bytes_be(be_buf, 4, &from_be) == 4);
    assert(from_le == 0x1234567 && from_be == 0x1234567);

    printf("[PASS] test_sust16_le (Little-Endian Byte Serialization)\n");
}

/* ============================================================================
 * Test: SUST-32 Fixed-Width Transport (4 bytes)
 * ============================================================================ */
void test_sust32(void) {
    uint8_t buf[16];
    sucs_ex_char_t decoded = 0;

    /* Encode/decode 0x41 */
    assert(sust32_encode(0x41ULL, buf, sizeof(buf)) == 4);
    assert(sust32_decode(buf, 4, &decoded) == 4);
    assert(decoded == 0x41ULL);

    /* Encode/decode max 32-bit value */
    assert(sust32_encode(0xFFFFFFFFULL, buf, sizeof(buf)) == 4);
    assert(sust32_decode(buf, 4, &decoded) == 4);
    assert(decoded == 0xFFFFFFFFULL);

    /* Reject >32-bit codepoints */
    assert(sust32_encode(0x100000000ULL, buf, sizeof(buf)) == 0);

    /* Reject trap range */
    assert(sust32_encode(0x7FFFFFF5ULL, buf, sizeof(buf)) == 0);

    printf("[PASS] test_sust32 (4-Byte Fixed)\n");
}

/* ============================================================================
 * Test: SUST-64 Fixed-Width Transport (8 bytes)
 * ============================================================================ */
void test_sust64(void) {
    uint8_t buf[16];
    sucs_ex_char_t decoded = 0;

    /* Full 64-bit roundtrip */
    assert(sust64_encode(0xDEADBEEFCAFEULL, buf, sizeof(buf)) == 8);
    assert(sust64_decode(buf, 8, &decoded) == 8);
    assert(decoded == 0xDEADBEEFCAFEULL);

    /* Max 64-bit value */
    assert(sust64_encode(0xFFFFFFFFFFFFFFFFULL, buf, sizeof(buf)) == 8);
    assert(sust64_decode(buf, 8, &decoded) == 8);
    assert(decoded == 0xFFFFFFFFFFFFFFFFULL);

    /* 0x7FFFFFFF is valid in ExtSUCS */
    assert(sust64_encode(0x7FFFFFFFULL, buf, sizeof(buf)) == 8);
    assert(sust64_decode(buf, 8, &decoded) == 8);
    assert(decoded == 0x7FFFFFFFULL);

    printf("[PASS] test_sust64 (8-Byte Fixed)\n");
}

/* ============================================================================
 * Test: SUST-128/256/512 Fixed-Width Transports
 * ============================================================================ */
void test_sust_wide(void) {
    uint8_t buf128[16];
    uint8_t buf256[32];
    uint8_t buf512[64];
    sucs_ex_char_t decoded = 0;

    sucs_ex_char_t test_cp = 0xABCDEF0123456789ULL;

    /* SUST-128 */
    assert(sust128_encode(test_cp, buf128, sizeof(buf128)) == 16);
    assert(sust128_decode(buf128, 16, &decoded) == 16);
    assert(decoded == test_cp);

    /* SUST-256 */
    assert(sust256_encode(test_cp, buf256, sizeof(buf256)) == 32);
    assert(sust256_decode(buf256, 32, &decoded) == 32);
    assert(decoded == test_cp);

    /* SUST-512 */
    assert(sust512_encode(test_cp, buf512, sizeof(buf512)) == 64);
    assert(sust512_decode(buf512, 64, &decoded) == 64);
    assert(decoded == test_cp);

    printf("[PASS] test_sust_wide (128/256/512-Byte Fixed)\n");
}

/* ============================================================================
 * Test: SUST-N Arbitrary Fixed-Width Transport
 * ============================================================================ */
void test_sustn(void) {
    uint8_t buf[128];
    sucs_ex_char_t decoded = 0;

    sucs_ex_char_t test_cp = 0x123456789ABCDEF0ULL;

    /* 24-byte arbitrary slot */
    assert(sustn_encode(test_cp, buf, 24) == 24);
    assert(sustn_decode(buf, 24, &decoded) == 24);
    assert(decoded == test_cp);

    /* Minimum 8-byte slot */
    assert(sustn_encode(test_cp, buf, 8) == 8);
    assert(sustn_decode(buf, 8, &decoded) == 8);
    assert(decoded == test_cp);

    /* Reject slot < 8 bytes */
    assert(sustn_encode(test_cp, buf, 4) == 0);

    printf("[PASS] test_sustn (N-Byte Arbitrary Fixed)\n");
}

/* ============================================================================
 * Test: e-SUST Hypervisor Page-Mapped IPC Transport
 * ============================================================================ */
void test_esust(void) {
    uint32_t page_index = 0;
    uint16_t offset = 0;
    sucs_ex_char_t decoded = 0;
    uint8_t frame[8];
    uint32_t flags = 0;

    /* Start clean */
    esust_unmap_all();
    assert(esust_is_page_mapped(0, NULL) == false);

    /* Map the guest pages the test will reference (real page table) */
    assert(esust_map_page(0, 0x00000000ULL, ESUST_PAGE_READ) == true);
    assert(esust_map_page(1, 0x00001000ULL, ESUST_PAGE_READ) == true);
    assert(esust_map_page(0x12345, 0x12345000ULL, ESUST_PAGE_READ | ESUST_PAGE_WRITE) == true);

    assert(esust_is_page_mapped(0, &flags) == true);
    assert((flags & ESUST_PAGE_PRESENT) != 0);
    assert(esust_is_page_mapped(2, NULL) == false);

    /* Page 0, offset 0x41 = codepoint 0x41 */
    assert(esust_translate_to_guest(0x41ULL, &page_index, &offset) == true);
    assert(page_index == 0);
    assert(offset == 0x41);

    /* Reconstruct back to host */
    assert(esust_translate_to_host(0, 0x41, &decoded) == true);
    assert(decoded == 0x41ULL);

    /* Page boundary: codepoint 4096 = page 1, offset 0 */
    assert(esust_translate_to_guest(4096ULL, &page_index, &offset) == true);
    assert(page_index == 1);
    assert(offset == 0);

    /* Unmapped host address is rejected (data-driven, not arithmetic) */
    assert(esust_translate_to_guest(8192ULL, &page_index, &offset) == false);

    /* Large codepoint: 0x12345678 */
    assert(esust_translate_to_guest(0x12345678ULL, &page_index, &offset) == true);
    assert(page_index == 0x12345);
    assert(offset == 0x678);
    assert(esust_translate_to_host(page_index, offset, &decoded) == true);
    assert(decoded == 0x12345678ULL);

    /* IPC frame roundtrip */
    assert(esust_encode_ipc(0x12345678ULL, frame, sizeof(frame)) == 6);
    assert(esust_decode_ipc(frame, 6, &decoded) == 6);
    assert(decoded == 0x12345678ULL);

    /* Trap range rejection in IPC */
    assert(esust_encode_ipc(0x7FFFFFF5ULL, frame, sizeof(frame)) == 0);

    /* Offset out of bounds rejection */
    assert(esust_translate_to_host(0, 5000, &decoded) == false);

    /* Unmapped page rejection in IPC decode */
    frame[0] = 0x00; frame[1] = 0x00; frame[2] = 0x00; frame[3] = 0x02; /* page 2 */
    frame[4] = 0x00; frame[5] = 0x41;
    assert(esust_decode_ipc(frame, 6, &decoded) == 0);

    /* Page-table rules: base must be page-aligned, and the mapped page must
     * not intersect the inherited Kernel Security Trap range */
    assert(esust_map_page(100, 0x00000123ULL, 0) == false);   /* not aligned */
    assert(esust_map_page(101, 0x7FFFF000ULL, 0) == false);   /* page 0x7FFFF000-0x7FFFFFFF overlaps trap range */
    assert(esust_map_page(102, 0x7FFFE000ULL, 0) == true);    /* page-aligned, below trap range */
    assert(esust_map_page(103, 0x80000000ULL, 0) == true);    /* page-aligned, above trap range */

    /* Unmap (including swap-remove) and verify no translation */
    assert(esust_unmap_page(103) == true);
    assert(esust_unmap_page(102) == true);
    assert(esust_unmap_page(0) == true);
    assert(esust_translate_to_guest(0x41ULL, &page_index, &offset) == false);

    /* Unmap of a non-mapped page fails */
    assert(esust_unmap_page(0) == false);

    printf("[PASS] test_esust (Hypervisor Page-Mapped IPC)\n");
}

/* ============================================================================
 * Main
 * ============================================================================ */
int main(void) {
    printf("===============================================\n");
    printf(" RUNNING ALL SUST SERIALIZATION TRANSPORT TESTS \n");
    printf("===============================================\n");
    test_sust16();
    test_sust16_le();
    test_sust32();
    test_sust64();
    test_sust_wide();
    test_sustn();
    test_esust();
    printf("===============================================\n");
    printf(" ALL SUST SERIALIZATION TRANSPORT TESTS PASSED!\n");
    printf("===============================================\n");
    return 0;
}