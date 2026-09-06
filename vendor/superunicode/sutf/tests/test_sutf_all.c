#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "sucs_types.h"
#include "sutf8.h"
#include "sutf16.h"
#include "sutf4.h"
#include "sutf2.h"
#include "sucs_mode.h"

void test_validator(void) {
    assert(sucs_is_valid(0x00000000) == true);
    assert(sucs_is_valid(0x00000041) == true);
    assert(sucs_is_valid(0x0010FFFF) == true);
    assert(sucs_is_valid(0x7FFFFFFE) == false); /* Trap range max */
    assert(sucs_is_valid(0x7FFFFFF0) == false); /* Trap range min */
    assert(sucs_is_valid(SUCS_INVALID_CODEPOINT) == false);
    assert(sucs_is_valid(0x80000000UL) == false);
    printf("[PASS] test_validator\n");
}

void test_sutf8(void) {
    uint8_t buf[16];
    sucs_char_t decoded = 0;

    /* 1 Byte ASCII */
    size_t w = sutf8_encode_char(0x41, buf, sizeof(buf));
    assert(w == 1);
    size_t r = sutf8_decode_char(buf, w, &decoded);
    assert(r == 1 && decoded == 0x41);

    /* 3 Bytes (PUA 0xD800 direct) */
    w = sutf8_encode_char(0xD800, buf, sizeof(buf));
    assert(w == 3);
    r = sutf8_decode_char(buf, w, &decoded);
    assert(r == 3 && decoded == 0xD800);

    /* 4 Bytes (SCP boundary 0x10000) */
    w = sutf8_encode_char(0x10000, buf, sizeof(buf));
    assert(w == 4);
    r = sutf8_decode_char(buf, w, &decoded);
    assert(r == 4 && decoded == 0x10000);

    /* 5 Bytes (0x200000) */
    w = sutf8_encode_char(0x200000, buf, sizeof(buf));
    assert(w == 5);
    r = sutf8_decode_char(buf, w, &decoded);
    assert(r == 5 && decoded == 0x200000);

    /* Length-table boundary regression: every value up to 0x7FFFFFFF must
     * report a 5- or 6-byte length (old 0x1FFFFF cutoff mis-sized 0x200000+). */
    assert(sutf8_codepoint_length(0x1FFFFF) == 5);
    assert(sutf8_codepoint_length(0x200000) == 5);
    assert(sutf8_codepoint_length(0x3FFFFFF) == 5);
    assert(sutf8_codepoint_length(0x4000000) == 6);
    assert(sutf8_codepoint_length(0x7FFFFFEF) == 6);

    /* 6 Bytes (0x7FFFFFEF - max valid codepoint before trap range) */
    w = sutf8_encode_char(0x7FFFFFEF, buf, sizeof(buf));
    assert(w == 6);
    r = sutf8_decode_char(buf, w, &decoded);
    assert(r == 6 && decoded == 0x7FFFFFEF);

    /* Trap range rejection */
    w = sutf8_encode_char(0x7FFFFFF5UL, buf, sizeof(buf));
    assert(w == 0);

    /* Sentinel rejection */
    w = sutf8_encode_char(SUCS_INVALID_CODEPOINT, buf, sizeof(buf));
    assert(w == 0);

    printf("[PASS] test_sutf8 (1..6 Bytes)\n");
}

void test_sutf16(void) {
    uint16_t words[4];
    sucs_char_t decoded = 0;

    /* 1 Word (literal; bit 15 clear) */
    size_t w = sutf16_encode_char(0x1234, words, 4);
    assert(w == 1 && words[0] == 0x1234);
    size_t r = sutf16_decode_char(words, w, &decoded);
    assert(r == 1 && decoded == 0x1234);

    /* 1 Word upper boundary: 0x7FFF (max single-word literal) */
    w = sutf16_encode_char(0x7FFF, words, 4);
    assert(w == 1 && words[0] == 0x7FFF);
    r = sutf16_decode_char(words, w, &decoded);
    assert(r == 1 && decoded == 0x7FFF);
    assert(sutf16_codepoint_length(0x7FFF) == 1);

    /* 0x8000-0xFFFF now use the 2-word form: marker word (0x8000) + literal.
     * A lone marker word is never a literal, so this framing is unambiguous. */
    w = sutf16_encode_char(0xD800, words, 4);
    assert(w == 2 && words[0] == 0x8000 && words[1] == 0xD800);
    r = sutf16_decode_char(words, w, &decoded);
    assert(r == 2 && decoded == 0xD800);

    /* The old ambiguous stream {0xD800, 0x0041} is now unambiguously one
     * codepoint 0x58000041 — 0xD800 can no longer be a literal 1-word value. */
    words[0] = 0xD800; words[1] = 0x0041;
    r = sutf16_decode_char(words, 2, &decoded);
    assert(r == 2 && decoded == 0x58000041);

    /* 2 Words (0x10000) */
    w = sutf16_encode_char(0x10000, words, 4);
    assert(w == 2 && words[0] == 0x8001 && words[1] == 0x0000);
    r = sutf16_decode_char(words, w, &decoded);
    assert(r == 2 && decoded == 0x10000);

    /* 2 Words (0x7FFFFFEF - highest valid codepoint before trap range) */
    w = sutf16_encode_char(0x7FFFFFEF, words, 4);
    assert(w == 2 && words[0] == 0xFFFF && words[1] == 0xFFEF);
    r = sutf16_decode_char(words, w, &decoded);
    assert(r == 2 && decoded == 0x7FFFFFEF);

    /* Sentinel SUCS_INVALID_CODEPOINT (0x7FFFFFFF) returns 0 */
    w = sutf16_encode_char(0x7FFFFFFF, words, 4);
    assert(w == 0);

    /* A lone marker word is a truncated 2-word sequence — rejected */
    words[0] = 0xD800;
    assert(sutf16_decode_char(words, 1, &decoded) == 0);

    /* Overlong 2-word sequence encoding a 1-word-range value (<= 0x7FFF) */
    words[0] = 0x8000; words[1] = 0x1234;
    assert(sutf16_decode_char(words, 2, &decoded) == 0);

    printf("[PASS] test_sutf16 (1..2 Words)\n");
}

void test_sutf4(void) {
    uint8_t buf[8];
    sucs_char_t decoded = 0;

    sucs_char_t cp = 0x12345678;
    size_t w = sutf4_encode_char(cp, buf, sizeof(buf));
    assert(w == 4);
    size_t r = sutf4_decode_char(buf, w, &decoded);
    assert(r == 4 && decoded == cp);

    printf("[PASS] test_sutf4 (Packed 4-Bit Nibbles)\n");
}

void test_sutf2(void) {
    uint8_t buf[8];
    sucs_char_t decoded = 0;

    sucs_char_t cp = 0x3ABCDEF0;
    size_t w = sutf2_encode_char(cp, buf, sizeof(buf));
    assert(w == 4);
    size_t r = sutf2_decode_char(buf, w, &decoded);
    assert(r == 4 && decoded == cp);

    printf("[PASS] test_sutf2 (2-Bit Symbol Bitstream)\n");
}

void test_overlong_rejection(void) {
    uint8_t buf[8];
    sucs_char_t decoded = 0;

    /* 2-byte overlong: 0xC0 0x80 encodes 0x00 (must be 1 byte) */
    buf[0] = 0xC0; buf[1] = 0x80;
    assert(sutf8_decode_char(buf, 2, &decoded) == 0);

    /* 3-byte overlong: 0xE0 0x80 0x80 encodes 0x00 */
    buf[0] = 0xE0; buf[1] = 0x80; buf[2] = 0x80;
    assert(sutf8_decode_char(buf, 3, &decoded) == 0);

    /* 4-byte below Unicode range: 0xF0 0x80 0x80 0x80 encodes 0x00 */
    buf[0] = 0xF0; buf[1] = 0x80; buf[2] = 0x80; buf[3] = 0x80;
    assert(sutf8_decode_char(buf, 4, &decoded) == 0);

    /* 4-byte above Unicode max: 0xF4 0x90 0x80 0x80 encodes 0x110000 (must be 5-byte) */
    buf[0] = 0xF4; buf[1] = 0x90; buf[2] = 0x80; buf[3] = 0x80;
    assert(sutf8_decode_char(buf, 4, &decoded) == 0);

    /* 5-byte below native extended space: 0xF8 0x80 0x80 0x80 0x80 encodes 0x00 */
    buf[0] = 0xF8; buf[1] = 0x80; buf[2] = 0x80; buf[3] = 0x80; buf[4] = 0x80;
    assert(sutf8_decode_char(buf, 5, &decoded) == 0);

    /* 6-byte below 6-byte range: 0xFC 0x80 0x80 0x80 0x80 0x80 encodes 0x00 */
    buf[0] = 0xFC; buf[1] = 0x80; buf[2] = 0x80; buf[3] = 0x80;
    buf[4] = 0x80; buf[5] = 0x80;
    assert(sutf8_decode_char(buf, 6, &decoded) == 0);

    printf("[PASS] test_overlong_rejection\n");
}

void test_kernel_mode_switch(void) {
    sucs_kernel_boot_config_t cfg;
    sucs_init_boot_config(&cfg, SUCS_MODE_BASE);

    assert(cfg.active_mode == SUCS_MODE_BASE);
    assert(cfg.pending_mode == SUCS_MODE_BASE);
    assert(cfg.reboot_required == false);

    /* Request switch to ExtSUCS mode */
    sucs_switch_status_t status = sucs_request_mode_switch(SUCS_MODE_EXTENDED);
    assert(status == SUCS_SWITCH_REBOOT_REQUIRED);
    assert(sucs_get_pending_mode() == SUCS_MODE_EXTENDED);
    assert(sucs_is_reboot_required() == true);
    /* Active mode remains Base until system restart commit */
    assert(sucs_get_active_mode() == SUCS_MODE_BASE);

    /* Simulate early kernel boot commit */
    bool committed = sucs_commit_mode_on_boot(NULL);
    assert(committed == true);
    assert(sucs_get_active_mode() == SUCS_MODE_EXTENDED);
    assert(sucs_is_reboot_required() == false);

    /* Requesting same mode when already active returns error */
    status = sucs_request_mode_switch(SUCS_MODE_EXTENDED);
    assert(status == SUCS_SWITCH_ERR_ALREADY_ACTIVE);

    printf("[PASS] test_kernel_mode_switch (Restart & Boot Commit)\n");
}

int main(void) {
    printf("=========================================\n");
    printf(" RUNNING ALL SUTF SERIALIZATION TESTS   \n");
    printf("=========================================\n");
    test_validator();
    test_sutf8();
    test_sutf16();
    test_sutf4();
    test_sutf2();
    test_overlong_rejection();
    test_kernel_mode_switch();
    printf("=========================================\n");
    printf(" ALL SUTF SERIALIZATION TESTS PASSED!   \n");
    printf("=========================================\n");
    return 0;
}
