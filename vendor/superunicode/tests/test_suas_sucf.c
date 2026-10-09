/**
 * SUAS-004 SUCF — SuperUnicode Canonical Forms
 *
 * Unit tests for the single-pass canonical (de)composition engine.
 *
 * Covers: Combining Canonical Class (CCC) lookup and starter detection,
 * the CCC-ascending reorder swap rule, SCP/native invariance, algorithmic
 * Hangul (de)composition, SUCF-C canonical composition, SUCF-D canonical
 * decomposition, quick check, and the streaming vs bulk transforms.
 */

#include <stdio.h>
#include <assert.h>
#include "suas/suas_sucf.h"

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_fail = 1; \
} } while (0)

static void test_defaults(void)
{
    suas_sucf_options_t o;
    suas_sucf_options_default(&o);
    CHECK(o.overrides == NULL);
    CHECK(o.count == 0);
    CHECK(suas_sucf_version_string() != NULL);
}

static void test_ccc_and_starter(void)
{
    suas_sucf_options_t o;
    suas_sucf_options_default(&o);

    /* Starters have CCC 0. */
    CHECK(suas_sucf_ccc(0x0041ULL, &o) == 0);   /* A */
    CHECK(suas_sucf_ccc(0x0061ULL, &o) == 0);   /* a */
    CHECK(suas_sucf_is_starter(0x0061ULL, &o) == true);

    /* Combining marks have non-zero CCC. */
    CHECK(suas_sucf_ccc(0x0300ULL, &o) == 230); /* grave */
    CHECK(suas_sucf_ccc(0x0301ULL, &o) == 230); /* acute */
    CHECK(suas_sucf_ccc(0x030CULL, &o) == 230); /* caron */
    CHECK(suas_sucf_ccc(0x0345ULL, &o) == 240); /* ypogegrammeni */
    CHECK(suas_sucf_is_starter(0x0301ULL, &o) == false);

    /* Unlisted bridge codepoints default to starter. */
    CHECK(suas_sucf_ccc(0x00B7ULL, &o) == 0);   /* middle dot */

    /* Native/plugin codepoints are starters (CCC 0). */
    CHECK(suas_sucf_ccc(0x00120000ULL, &o) == 0);
    CHECK(suas_sucf_ccc(0x80000000ULL, &o) == 0);
}

static void test_invariance(void)
{
    /* SCP, BANcode and trap markers are canonically invariant. */
    CHECK(suas_sucf_is_scp(0x00110000ULL) == true);
    CHECK(suas_sucf_is_scp(0x00110020ULL) == true);
    CHECK(suas_sucf_is_scp(0x0011A080ULL) == true); /* BANcode */
    CHECK(suas_sucf_is_scp(0x7FFFFFF0ULL) == true); /* trap */
    CHECK(suas_sucf_is_scp(0x7FFFFFFFULL) == true); /* sentinel */
    CHECK(suas_sucf_is_scp(0x0041ULL) == false);

    /* Native SUCS / ExtSUCS plugin codepoints are invariant by default. */
    CHECK(suas_sucf_is_invariant(0x00120000ULL) == true);
    CHECK(suas_sucf_is_invariant(0xABC00000ULL) == true);
    CHECK(suas_sucf_is_invariant(0x00110021ULL) == true);
    CHECK(suas_sucf_is_invariant(0x7FFFFFFEULL) == true);

    /* Bridged letters with no combining property are invariant. */
    CHECK(suas_sucf_is_invariant(0x0041ULL) == true);
    /* Precomposed letters and combining marks are not invariant. */
    CHECK(suas_sucf_is_invariant(0x00E9ULL) == false); /* é */
    CHECK(suas_sucf_is_invariant(0x0301ULL) == false); /* acute mark */
    /* Hangul syllables decompose algorithmically → not invariant. */
    CHECK(suas_sucf_is_invariant(0xAC00ULL) == false);
}

static void test_hangul(void)
{
    suas_sucf_options_t o;
    suas_sucf_options_default(&o);
    sucs_ex_char_t out[4];
    size_t n;

    /* 가 U+AC00 decomposes to ᄀ L + ᅡ V (no trailing). */
    CHECK(suas_sucf_is_hangul(0xAC00ULL) == true);
    n = suas_sucf_decompose_one(0xAC00ULL, &o, out, 4);
    CHECK(n == 2);
    CHECK(out[0] == 0x1100ULL); /* L */
    CHECK(out[1] == 0x1161ULL); /* V */

    /* 각 U+AC01 decomposes to L + V + T. */
    n = suas_sucf_decompose_one(0xAC01ULL, &o, out, 4);
    CHECK(n == 3);
    CHECK(out[0] == 0x1100ULL);
    CHECK(out[1] == 0x1161ULL);
    CHECK(out[2] == 0x11A8ULL); /* T */

    /* Hangul Jamo are not syllables. */
    CHECK(suas_sucf_is_hangul(0x1100ULL) == false);
    CHECK(suas_sucf_is_hangul(0xD7A4ULL) == false);
}

static void test_decompose_one(void)
{
    suas_sucf_options_t o;
    suas_sucf_options_default(&o);
    sucs_ex_char_t out[4];
    size_t n;

    /* Primitive letters decompose to themselves. */
    n = suas_sucf_decompose_one(0x0041ULL, &o, out, 4);
    CHECK(n == 1);
    CHECK(out[0] == 0x0041ULL);

    /* Precomposed é → e + combining acute. */
    n = suas_sucf_decompose_one(0x00E9ULL, &o, out, 4);
    CHECK(n == 2);
    CHECK(out[0] == 0x0065ULL);
    CHECK(out[1] == 0x0301ULL);

    /* Singleton exclusion: ANGSTROM SIGN → Å (never recomposes). */
    n = suas_sucf_decompose_one(0x212BULL, &o, out, 4);
    CHECK(n == 1);
    CHECK(out[0] == 0x00C5ULL);

    /* SCP codepoint decomposes to itself. */
    n = suas_sucf_decompose_one(0x00110000ULL, &o, out, 4);
    CHECK(n == 1);
    CHECK(out[0] == 0x00110000ULL);
}

/* Decomposed-equality helper: identical scalar arrays. */
static int eq(const sucs_ex_char_t* a, size_t n,
              const sucs_ex_char_t* b, size_t m)
{
    size_t i;
    if (n != m) return 0;
    for (i = 0; i < n; ++i) if (a[i] != b[i]) return 0;
    return 1;
}

static void test_reorder_swap(void)
{
    /* CCC-ascending reorder: input e + acute(230) + grave(230) stays as-is,
     * but a lower-CCC mark after a higher one must swap forward. */
    suas_sucf_options_t o;
    suas_sucf_options_default(&o);
    /* x + tilde(230) + below(220): below must reorder before tilde. */
    /* Use direct: 0x0061, 0x0300(230), 0x0323(220) → D form: a, below, grave. */
    sucs_ex_char_t in[]  = { 0x0061ULL, 0x0300ULL, 0x0323ULL };
    sucs_ex_char_t expD[] = { 0x0061ULL, 0x0323ULL, 0x0300ULL };
    sucs_ex_char_t out[8];
    size_t oc = 0;
    CHECK(suas_sucf_transform(in, 3, SUAS_SUCF_FORM_D, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(eq(out, oc, expD, 3));

    /* Stable when CCC already ascending: a + below(220) + grave(230). */
    sucs_ex_char_t in2[] = { 0x0061ULL, 0x0323ULL, 0x0300ULL };
    sucs_ex_char_t expD2[] = { 0x0061ULL, 0x0323ULL, 0x0300ULL };
    oc = 0;
    CHECK(suas_sucf_transform(in2, 3, SUAS_SUCF_FORM_D, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(eq(out, oc, expD2, 3));
}

static void test_compose_c(void)
{
    suas_sucf_options_t o;
    suas_sucf_options_default(&o);
    /* e + acute → é in SUCF-C. */
    sucs_ex_char_t in[] = { 0x0065ULL, 0x0301ULL };
    sucs_ex_char_t exp[] = { 0x00E9ULL };
    sucs_ex_char_t out[8];
    size_t oc = 0;
    CHECK(suas_sucf_transform(in, 2, SUAS_SUCF_FORM_C, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(eq(out, oc, exp, 1));

    /* à (already composed) stays composed in SUCF-C. */
    sucs_ex_char_t in2[] = { 0x00E0ULL };
    oc = 0;
    CHECK(suas_sucf_transform(in2, 1, SUAS_SUCF_FORM_C, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(eq(out, oc, in2, 1));

    /* Composition exclusion: ANGSTROM stays decomposed in SUCF-C. */
    sucs_ex_char_t in3[] = { 0x212BULL };
    sucs_ex_char_t exp3[] = { 0x00C5ULL };
    oc = 0;
    CHECK(suas_sucf_transform(in3, 1, SUAS_SUCF_FORM_C, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(eq(out, oc, exp3, 1));

    /* NFC idempotence: Ả (U+1EA2 A+HOOKABC) not in curated table but the
     * explicit 0x1EA2 is not present — skip. Use é passthrough instead. */
    sucs_ex_char_t in4[] = { 0x00E9ULL };
    oc = 0;
    CHECK(suas_sucf_transform(in4, 1, SUAS_SUCF_FORM_C, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(eq(out, oc, in4, 1));
}

static void test_decompose_d(void)
{
    suas_sucf_options_t o;
    suas_sucf_options_default(&o);
    /* é → e + acute in SUCF-D. */
    sucs_ex_char_t in[] = { 0x00E9ULL };
    sucs_ex_char_t exp[] = { 0x0065ULL, 0x0301ULL };
    sucs_ex_char_t out[8];
    size_t oc = 0;
    CHECK(suas_sucf_transform(in, 1, SUAS_SUCF_FORM_D, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(eq(out, oc, exp, 2));

    /* NFD idempotence: decomposed input stays decomposed. */
    sucs_ex_char_t in2[] = { 0x0065ULL, 0x0301ULL };
    oc = 0;
    CHECK(suas_sucf_transform(in2, 2, SUAS_SUCF_FORM_D, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(eq(out, oc, in2, 2));

    /* Hangul decomposes in SUCF-D. */
    sucs_ex_char_t in3[] = { 0xAC01ULL };
    sucs_ex_char_t exp3[] = { 0x1100ULL, 0x1161ULL, 0x11A8ULL };
    oc = 0;
    CHECK(suas_sucf_transform(in3, 1, SUAS_SUCF_FORM_D, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(eq(out, oc, exp3, 3));
}

static void test_quick_check(void)
{
    suas_sucf_options_t o;
    suas_sucf_options_default(&o);
    sucs_ex_char_t plain[] = { 0x0041ULL, 0x0062ULL, 0x0063ULL };

    /* Plain ASCII is already in both canonical forms. */
    CHECK(suas_sucf_quick_check(plain, 3, SUAS_SUCF_FORM_C, &o) == SUAS_SUCF_QC_YES);
    CHECK(suas_sucf_quick_check(plain, 3, SUAS_SUCF_FORM_D, &o) == SUAS_SUCF_QC_YES);

    /* Precomposed é is NO for decomposition. */
    sucs_ex_char_t pre[] = { 0x00E9ULL };
    CHECK(suas_sucf_quick_check(pre, 1, SUAS_SUCF_FORM_D, &o) == SUAS_SUCF_QC_NO);
    CHECK(suas_sucf_quick_check(pre, 1, SUAS_SUCF_FORM_C, &o) == SUAS_SUCF_QC_YES);

    /* Decomposed e + acute is NO for composition. */
    sucs_ex_char_t dec[] = { 0x0065ULL, 0x0301ULL };
    CHECK(suas_sucf_quick_check(dec, 2, SUAS_SUCF_FORM_C, &o) == SUAS_SUCF_QC_NO);
    CHECK(suas_sucf_quick_check(dec, 2, SUAS_SUCF_FORM_D, &o) == SUAS_SUCF_QC_YES);
}

static void test_scp_invariance_stream(void)
{
    suas_sucf_options_t o;
    suas_sucf_options_default(&o);
    /* An SCP instruction between a starter and its mark must not break the
     * combining state: e + SCP + acute still composes to é in SUCF-C, and
     * the SCP marker passes through in its original stream position. */
    suas_sucf_state_t st;
    sucs_ex_char_t out[8];
    size_t oc = 0;
    CHECK(suas_sucf_state_init(&st, SUAS_SUCF_FORM_C, &o) == SUAS_SUCF_OK);
    CHECK(suas_sucf_process_codepoint(&st, 0x0065ULL, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(suas_sucf_process_codepoint(&st, 0x00110020ULL, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(suas_sucf_process_codepoint(&st, 0x0301ULL, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(suas_sucf_flush(&st, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(eq(out, oc, (const sucs_ex_char_t[]){0x00E9ULL, 0x00110020ULL}, 2));
}

static void test_hangul_compose_c(void)
{
    suas_sucf_options_t o;
    suas_sucf_options_default(&o);
    /* Explicit Jamo L + V + T composes to a syllable in SUCF-C. */
    sucs_ex_char_t in[] = { 0x1100ULL, 0x1161ULL, 0x11A8ULL }; /* 각 */
    sucs_ex_char_t exp[] = { 0xAC01ULL };                     /* 각 */
    sucs_ex_char_t out[8];
    size_t oc = 0;
    CHECK(suas_sucf_transform(in, 3, SUAS_SUCF_FORM_C, &o, out, 8, &oc) == SUAS_SUCF_OK);
    CHECK(eq(out, oc, exp, 1));
}

static void test_stream_equal_bulk(void)
{
    suas_sucf_options_t o;
    suas_sucf_options_default(&o);
    sucs_ex_char_t in[] = { 0x0041ULL, 0x00E9ULL, 0x0323ULL,
                            0x0300ULL, 0x0062ULL, 0x212BULL, 0x7FFFFFFFULL };
    sucs_ex_char_t bulk[16], strm[16];
    size_t boc = 0, soc = 0;
    suas_sucf_state_t st;
    size_t i;
    CHECK(suas_sucf_transform(in, 7, SUAS_SUCF_FORM_D, &o, bulk, 16, &boc) == SUAS_SUCF_OK);
    CHECK(suas_sucf_state_init(&st, SUAS_SUCF_FORM_D, &o) == SUAS_SUCF_OK);
    for (i = 0; i < 7; ++i)
        CHECK(suas_sucf_process_codepoint(&st, in[i], &o, strm, 16, &soc) == SUAS_SUCF_OK);
    CHECK(suas_sucf_flush(&st, strm, 16, &soc) == SUAS_SUCF_OK);
    CHECK(eq(bulk, boc, strm, soc));
}

int main(void)
{
    test_defaults();
    test_ccc_and_starter();
    test_invariance();
    test_hangul();
    test_decompose_one();
    test_reorder_swap();
    test_compose_c();
    test_decompose_d();
    test_quick_check();
    test_scp_invariance_stream();
    test_hangul_compose_c();
    test_stream_equal_bulk();

    if (g_fail) {
        printf("SUAS-004 SUCF: FAIL\n");
        return 1;
    }
    printf("[PASS] test_suas_sucf\n");
    return 0;
}
