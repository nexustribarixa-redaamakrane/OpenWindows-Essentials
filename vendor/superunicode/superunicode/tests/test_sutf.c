#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "superunicode/superunicode.h"

void test_sutf_1byte(void) {
    char buf[8];
    size_t written = 0;
    sucs_char_t decoded = 0;
    size_t read_bytes = 0;

    int status = sutf_encode_char(0x41, buf, sizeof(buf), &written);
    assert(status == SUES_SUCCESS);
    assert(written == 1);
    assert((unsigned char)buf[0] == 0x41);

    status = sutf_decode_char(buf, written, &decoded, &read_bytes);
    assert(status == SUES_SUCCESS);
    assert(read_bytes == 1);
    assert(decoded == 0x41);
    printf("[PASS] test_sutf_1byte (0x41)\n");
}

void test_sutf_2bytes(void) {
    char buf[8];
    size_t written = 0;
    sucs_char_t decoded = 0;
    size_t read_bytes = 0;

    sucs_char_t cp = 0x7FF;
    int status = sutf_encode_char(cp, buf, sizeof(buf), &written);
    assert(status == SUES_SUCCESS);
    assert(written == 2);
    assert(((unsigned char)buf[0] & 0xE0) == 0xC0);
    assert(((unsigned char)buf[1] & 0xC0) == 0x80);

    status = sutf_decode_char(buf, written, &decoded, &read_bytes);
    assert(status == SUES_SUCCESS);
    assert(read_bytes == 2);
    assert(decoded == cp);
    printf("[PASS] test_sutf_2bytes (0x7FF)\n");
}

void test_sutf_3bytes(void) {
    char buf[8];
    size_t written = 0;
    sucs_char_t decoded = 0;
    size_t read_bytes = 0;

    sucs_char_t cp = 0xABCD;
    int status = sutf_encode_char(cp, buf, sizeof(buf), &written);
    assert(status == SUES_SUCCESS);
    assert(written == 3);
    assert(((unsigned char)buf[0] & 0xF0) == 0xE0);

    status = sutf_decode_char(buf, written, &decoded, &read_bytes);
    assert(status == SUES_SUCCESS);
    assert(read_bytes == 3);
    assert(decoded == cp);
    printf("[PASS] test_sutf_3bytes (0xABCD)\n");
}

void test_sutf_4bytes_unicode_max(void) {
    char buf[8];
    size_t written = 0;
    sucs_char_t decoded = 0;
    size_t read_bytes = 0;

    sucs_char_t cp = 0x10FFFF; /* Standard Unicode Max */
    int status = sutf_encode_char(cp, buf, sizeof(buf), &written);
    assert(status == SUES_SUCCESS);
    assert(written == 4);
    assert(((unsigned char)buf[0] & 0xF8) == 0xF0);

    status = sutf_decode_char(buf, written, &decoded, &read_bytes);
    assert(status == SUES_SUCCESS);
    assert(read_bytes == 4);
    assert(decoded == cp);
    printf("[PASS] test_sutf_4bytes_unicode_max (0x10FFFF)\n");
}

void test_sutf_5bytes_native_extended(void) {
    char buf[8];
    size_t written = 0;
    sucs_char_t decoded = 0;
    size_t read_bytes = 0;

    /* 0x110000: System Control Plane start */
    sucs_char_t cp1 = 0x110000;
    int status = sutf_encode_char(cp1, buf, sizeof(buf), &written);
    assert(status == SUES_SUCCESS);
    assert(written == 5);
    assert(((unsigned char)buf[0] & 0xFC) == 0xF8);
    assert(((unsigned char)buf[1] & 0xC0) == 0x80);

    status = sutf_decode_char(buf, written, &decoded, &read_bytes);
    assert(status == SUES_SUCCESS);
    assert(read_bytes == 5);
    assert(decoded == cp1);

    /* 0x3FFFFFF: Max 5-byte codepoint */
    sucs_char_t cp2 = 0x3FFFFFF;
    status = sutf_encode_char(cp2, buf, sizeof(buf), &written);
    assert(status == SUES_SUCCESS);
    assert(written == 5);

    status = sutf_decode_char(buf, written, &decoded, &read_bytes);
    assert(status == SUES_SUCCESS);
    assert(read_bytes == 5);
    assert(decoded == cp2);

    printf("[PASS] test_sutf_5bytes_native_extended (0x110000, 0x3FFFFFF)\n");
}

void test_sutf_6bytes_31bit_max(void) {
    char buf[8];
    size_t written = 0;
    sucs_char_t decoded = 0;
    size_t read_bytes = 0;

    /* 0x04000000: Min 6-byte codepoint */
    sucs_char_t cp1 = 0x04000000;
    int status = sutf_encode_char(cp1, buf, sizeof(buf), &written);
    assert(status == SUES_SUCCESS);
    assert(written == 6);
    assert(((unsigned char)buf[0] & 0xFE) == 0xFC);

    status = sutf_decode_char(buf, written, &decoded, &read_bytes);
    assert(status == SUES_SUCCESS);
    assert(read_bytes == 6);
    assert(decoded == cp1);

    /* 0x7FFFFFEF: Max valid codepoint before the trap range (still 6 bytes) */
    sucs_char_t cp2 = 0x7FFFFFEF;
    status = sutf_encode_char(cp2, buf, sizeof(buf), &written);
    assert(status == SUES_SUCCESS);
    assert(written == 6);

    status = sutf_decode_char(buf, written, &decoded, &read_bytes);
    assert(status == SUES_SUCCESS);
    assert(read_bytes == 6);
    assert(decoded == cp2);

    /* Sentinel 0x7FFFFFFF and trap range are reserved, not encodable */
    status = sutf_encode_char(0x7FFFFFFF, buf, sizeof(buf), &written);
    assert(status == SUES_ERR_INVALID_CODEPOINT);

    status = sutf_encode_char(0x7FFFFFF5, buf, sizeof(buf), &written);
    assert(status == SUES_ERR_INVALID_CODEPOINT);

    printf("[PASS] test_sutf_6bytes_31bit_max (0x04000000, 0x7FFFFFEF)\n");
}

void test_sutf_overlong_rejection(void) {
    char buf[8];
    sucs_char_t cp = 0;
    size_t read_bytes = 0;

    /* 2-byte overlong: 0xC0 0x80 encodes 0x00 (must be 1 byte) */
    buf[0] = (char)0xC0;
    buf[1] = (char)0x80;
    assert(sutf_decode_char(buf, 2, &cp, &read_bytes) == SUES_ERR_INVALID_BYTE);

    /* 3-byte overlong: 0xE0 0x80 0x80 encodes 0x00 */
    buf[0] = (char)0xE0; buf[1] = (char)0x80; buf[2] = (char)0x80;
    assert(sutf_decode_char(buf, 3, &cp, &read_bytes) == SUES_ERR_INVALID_BYTE);

    /* 4-byte below Unicode range: 0xF0 0x80 0x80 0x80 encodes 0x00 */
    buf[0] = (char)0xF0; buf[1] = (char)0x80; buf[2] = (char)0x80; buf[3] = (char)0x80;
    assert(sutf_decode_char(buf, 4, &cp, &read_bytes) == SUES_ERR_INVALID_BYTE);

    /* 4-byte above Unicode max: 0xF4 0x90 0x80 0x80 encodes 0x110000 (must be 5-byte) */
    buf[0] = (char)0xF4; buf[1] = (char)0x90; buf[2] = (char)0x80; buf[3] = (char)0x80;
    assert(sutf_decode_char(buf, 4, &cp, &read_bytes) == SUES_ERR_INVALID_BYTE);

    /* 5-byte below native extended space: 0xF8 0x80 0x80 0x80 0x80 encodes 0x00 */
    buf[0] = (char)0xF8; buf[1] = (char)0x80; buf[2] = (char)0x80; buf[3] = (char)0x80; buf[4] = (char)0x80;
    assert(sutf_decode_char(buf, 5, &cp, &read_bytes) == SUES_ERR_INVALID_BYTE);

    /* 6-byte below 6-byte range: 0xFC 0x80 0x80 0x80 0x80 0x80 encodes 0x00 */
    buf[0] = (char)0xFC; buf[1] = (char)0x80; buf[2] = (char)0x80; buf[3] = (char)0x80;
    buf[4] = (char)0x80; buf[5] = (char)0x80;
    assert(sutf_decode_char(buf, 6, &cp, &read_bytes) == SUES_ERR_INVALID_BYTE);

    printf("[PASS] test_sutf_overlong_rejection\n");
}

void test_sutf_error_cases(void) {
    char buf[8];
    size_t written = 0;

    /* Out of bounds codepoint > 0x7FFFFFFF */
    int status = sutf_encode_char(0x80000000UL, buf, sizeof(buf), &written);
    assert(status == SUES_ERR_OUT_OF_BOUNDS);

    /* Buffer too small (valid 6-byte codepoint into a 4-byte buffer) */
    status = sutf_encode_char(0x04000000, buf, 4, &written);
    assert(status == SUES_ERR_BUFFER_TOO_SMALL);

    /* Invalid continuation byte */
    buf[0] = (char)0xF8;
    buf[1] = (char)0x20; /* Invalid (missing 0x80 prefix) */
    sucs_char_t cp = 0;
    size_t read_bytes = 0;
    status = sutf_decode_char(buf, 5, &cp, &read_bytes);
    assert(status == SUES_ERR_INVALID_BYTE);

    printf("[PASS] test_sutf_error_cases\n");
}

int main(void) {
    printf("--- Running SUTF Serialization Unit Tests ---\n");
    test_sutf_1byte();
    test_sutf_2bytes();
    test_sutf_3bytes();
    test_sutf_4bytes_unicode_max();
    test_sutf_5bytes_native_extended();
    test_sutf_6bytes_31bit_max();
    test_sutf_overlong_rejection();
    test_sutf_error_cases();
    printf("--- ALL SUTF UNIT TESTS PASSED ---\n");
    return 0;
}
