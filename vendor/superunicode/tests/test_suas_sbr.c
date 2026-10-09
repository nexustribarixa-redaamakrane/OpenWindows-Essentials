/**
 * SUAS-003 SBR — System Boundary & Line Break Rules
 *
 * Unit tests for the single-pass deterministic line-break engine.
 *
 * Covers: break-status classification (MUST/CAN/NO/ALPHANUM), zone dispatch
 * over the 64-bit space, the O(1) transition-matrix pair contract, WJ/ZWSP
 * invisible formatting, Explicit SCP Break Markers, the Native SUCS high-bit
 * range bitmask, the streaming engine, and the tailoring override hook.
 */

#include <stdio.h>
#include <assert.h>
#include "suas/suas_sbr.h"

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_fail = 1; \
} } while (0)

static void test_defaults(void)
{
    suas_sbr_options_t o;
    suas_sbr_options_default(&o);
    CHECK(o.overrides == NULL);
    CHECK(o.count == 0);
    CHECK(suas_sbr_version_string() != NULL);

    suas_sbr_state_t st;
    suas_sbr_state_init(&st);
    CHECK(st.seeded == 0);
    CHECK(st.prev == SUAS_SBR_CLS_XX);
}

static void test_classify_zones(void)
{
    suas_sbr_options_t o;
    suas_sbr_options_default(&o);

    /* Bridge: alphabetic → AL, numeric → NU, space → SP, IDEOGRAPHIC. */
    CHECK(suas_sbr_classify(0x0041ULL, &o) == SUAS_SBR_CLS_AL);   /* A */
    CHECK(suas_sbr_classify(0x0061ULL, &o) == SUAS_SBR_CLS_AL);   /* a */
    CHECK(suas_sbr_classify(0x0031ULL, &o) == SUAS_SBR_CLS_NU);   /* 1 */
    CHECK(suas_sbr_classify(0x0020ULL, &o) == SUAS_SBR_CLS_SP);   /* sp */
    CHECK(suas_sbr_classify(0x4E00ULL, &o) == SUAS_SBR_CLS_ID);   /* 一 */
    CHECK(suas_sbr_classify(0x200BULL, &o) == SUAS_SBR_CLS_ZW);   /* ZWSP */
    CHECK(suas_sbr_classify(0x2060ULL, &o) == SUAS_SBR_CLS_WJ);   /* WJ */

    /* SCP break markers classify as controls. */
    CHECK(suas_sbr_classify(SCP_BRK_MANDATORY, &o) == SUAS_SBR_CLS_BK);
    CHECK(suas_sbr_classify(SCP_BRK_PROHIBITED, &o) == SUAS_SBR_CLS_GL);
    CHECK(suas_sbr_classify(SCP_BRK_OPPORTUNISTIC, &o) == SUAS_SBR_CLS_BA);
    CHECK(suas_sbr_classify(0x00110001ULL, &o) == SUAS_SBR_CLS_BK);

    /* Native SUCS: low blocks are words, high/plugin are neutral. */
    CHECK(suas_sbr_classify(0x00120000ULL, &o) == SUAS_SBR_CLS_ID);
    CHECK(suas_sbr_classify(0x00123456ULL, &o) == SUAS_SBR_CLS_ID);
    CHECK(suas_sbr_classify(0x40000000ULL, &o) == SUAS_SBR_CLS_XX);

    /* ExtSUCS plugin space: neutral gap. */
    CHECK(suas_sbr_classify(0x80000000ULL, &o) == SUAS_SBR_CLS_XX);
    CHECK(suas_sbr_classify(0xFFFFFFFFFFFFFFFFULL, &o) == SUAS_SBR_CLS_XX);
}

static void test_native_word_bitmask(void)
{
    CHECK(suas_sbr_is_native_word(0x00120000ULL) == true);
    CHECK(suas_sbr_is_native_word(0x00123456ULL) == true);
    CHECK(suas_sbr_is_native_word(0x00FFFFFFULL) == true);
    CHECK(suas_sbr_is_native_word(0x01000000ULL) == false);
    CHECK(suas_sbr_is_native_word(0x40000000ULL) == false);
    CHECK(suas_sbr_is_native_word(0x80000000ULL) == false);
    CHECK(suas_sbr_is_native_word(0xFFFFFFFFFFFFFFFFULL) == false);
}

static void test_pair_status(void)
{
    suas_sbr_options_t o;
    suas_sbr_options_default(&o);

    /* Mandatory control after a char → MUST_BREAK. */
    CHECK(suas_sbr_pair(0x0041ULL, 0x000AULL, &o) == SUAS_BRK_MUST_BREAK);

    /* WJ forbids a break on either side. */
    CHECK(suas_sbr_pair(0x0041ULL, 0x2060ULL, &o) == SUAS_BRK_NO_BREAK); /* WJ */
    CHECK(suas_sbr_pair(0x2060ULL, 0x0041ULL, &o) == SUAS_BRK_NO_BREAK);

    /* Inside a word (alpha-alpha) → NO_BREAK. */
    CHECK(suas_sbr_pair(0x0041ULL, 0x0062ULL, &o) == SUAS_BRK_NO_BREAK);
    /* Numeric-numeric inside a number → NO_BREAK. */
    CHECK(suas_sbr_pair(0x0031ULL, 0x0032ULL, &o) == SUAS_BRK_NO_BREAK);
    /* Alpha-numeric → NO_BREAK (a1 / word). */
    CHECK(suas_sbr_pair(0x0041ULL, 0x0031ULL, &o) == SUAS_BRK_NO_BREAK);

    /* Space after a word → CAN_BREAK (soft opportunity). */
    CHECK(suas_sbr_pair(0x0041ULL, 0x0020ULL, &o) == SUAS_BRK_CAN_BREAK);

    /* Open punctuation does not break before. */
    CHECK(suas_sbr_pair(0x0028ULL, 0x0041ULL, &o) == SUAS_BRK_NO_BREAK);
    /* Close punctuation does not break before. */
    CHECK(suas_sbr_pair(0x0041ULL, 0x0029ULL, &o) == SUAS_BRK_NO_BREAK);

    /* CJK numeric rule: alphabetic then currency prefix → ALPHANUM_BREAK. */
    CHECK(suas_sbr_pair(0x0041ULL, 0x0024ULL, &o) == SUAS_BRK_ALPHANUM_BREAK);
}

static void test_streaming_engine(void)
{
    suas_sbr_options_t o;
    suas_sbr_options_default(&o);
    suas_sbr_state_t st;
    suas_sbr_state_init(&st);

    /* "A" seeds (MUST_BREAK at start), then "b" is the same word → NO, then
     * " " after the word is the soft break spot → CAN. */
    CHECK(suas_sbr_process_codepoint(&st, 0x0041ULL, &o) == SUAS_BRK_MUST_BREAK);
    CHECK(suas_sbr_process_codepoint(&st, 0x0062ULL, &o) == SUAS_BRK_NO_BREAK);
    CHECK(suas_sbr_process_codepoint(&st, 0x0020ULL, &o) == SUAS_BRK_CAN_BREAK);
    /* The space's break opportunity lands before the next word. */
    CHECK(suas_sbr_process_codepoint(&st, 0x0063ULL, &o) == SUAS_BRK_CAN_BREAK);

    /* Global state: CJK then LF → hard break. */
    suas_sbr_state_init(&st);
    CHECK(suas_sbr_process_codepoint(&st, 0x4E00ULL, &o) == SUAS_BRK_MUST_BREAK);
    CHECK(suas_sbr_process_codepoint(&st, 0x000AULL, &o) == SUAS_BRK_MUST_BREAK);

    /* NULL guard. */
    CHECK(suas_sbr_process_codepoint(NULL, 0x0041ULL, &o)
          == SUAS_SBR_ERR_NULL_POINTER);
}

static void test_scp_markers(void)
{
    suas_sbr_options_t o;
    suas_sbr_options_default(&o);

    /* MANDATORY between two no-break word chars forces a break. */
    CHECK(suas_sbr_pair(0x0041ULL, SCP_BRK_MANDATORY, &o)
          == SUAS_BRK_MUST_BREAK);
    CHECK(suas_sbr_pair(SCP_BRK_MANDATORY, 0x0062ULL, &o)
          == SUAS_BRK_MUST_BREAK);

    /* PROHIBITED forces NO_BREAK even across a soft space. */
    CHECK(suas_sbr_pair(0x0041ULL, SCP_BRK_PROHIBITED, &o)
          == SUAS_BRK_NO_BREAK);
    CHECK(suas_sbr_pair(SCP_BRK_PROHIBITED, 0x0062ULL, &o)
          == SUAS_BRK_NO_BREAK);

    /* OPPORTUNISTIC forces CAN_BREAK. */
    CHECK(suas_sbr_pair(0x0041ULL, SCP_BRK_OPPORTUNISTIC, &o)
          == SUAS_BRK_CAN_BREAK);
    CHECK(suas_sbr_pair(SCP_BRK_OPPORTUNISTIC, 0x0062ULL, &o)
          == SUAS_BRK_CAN_BREAK);
}

static void test_override_hook(void)
{
    /* Pin a native range to NO_BREAK-heavy ideographic word class and force
     * a normally-unknown plugin cp to AL. Entries MUST be sorted by lo. */
    static const suas_sbr_override_t ovr[] = {
        { 0x80000000ULL, 0x800000FFULL, SUAS_SBR_CLS_AL },
    };
    suas_sbr_options_t o;
    o.overrides = ovr;
    o.count = 1;

    CHECK(suas_sbr_classify(0x80000000ULL, &o) == SUAS_SBR_CLS_AL);
    /* Two overridden AL cps → inside word → NO_BREAK. */
    CHECK(suas_sbr_pair(0x80000000ULL, 0x80000001ULL, &o)
          == SUAS_BRK_NO_BREAK);
}

int main(void)
{
    test_defaults();
    test_classify_zones();
    test_native_word_bitmask();
    test_pair_status();
    test_streaming_engine();
    test_scp_markers();
    test_override_hook();

    if (g_fail) {
        printf("SUAS-003 SBR: FAIL\n");
        return 1;
    }
    printf("[PASS] test_suas_sbr\n");
    return 0;
}
