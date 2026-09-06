#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include "suas/suas_sdf.h"

#define SUAS_SUCD_MASK(cp) (suas_sucd_bidi((uint32_t)(cp)))

/* ── SUCD BiDi lookup ──────────────────────────────────────────── */

static void test_sucd_bidi_masks(void)
{
    /* Strong LTR */
    assert(suas_sucd_bidi(0x0041) == SUCD_BIDI_LTR);     /* 'A' */
    assert(suas_sucd_bidi(0x0061) == SUCD_BIDI_LTR);     /* 'a' */
    assert(suas_sucd_bidi(0x0416) == SUCD_BIDI_LTR);     /* Cyrillic Ж */

    /* Arabic letter (RTL) */
    assert((suas_sucd_bidi(0x0627) & SUCD_BIDI_ARABIC_AL) != 0); /* ا */
    assert((suas_sucd_bidi(0x0645) & SUCD_BIDI_ARABIC_AL) != 0); /* م */

    /* Hebrew (strong RTL) */
    assert((suas_sucd_bidi(0x05D0) & SUCD_BIDI_RTL) != 0); /* א */

    /* Mirrorable punctuation */
    assert(suas_sucd_bidi(0x0028) == SUCD_BIDI_MIRRORED); /* ( */
    assert(suas_sucd_bidi(0x0029) == SUCD_BIDI_MIRRORED); /* ) */
    assert(suas_sucd_bidi(0x005B) == SUCD_BIDI_MIRRORED); /* [ */
    assert(suas_sucd_bidi(0x007D) == SUCD_BIDI_MIRRORED); /* } */

    /* Whitespace */
    assert(suas_sucd_bidi(0x0020) == SUCD_BIDI_WHITESPACE);

    /* Unmapped defaults to neutral; outside bridge also neutral */
    assert(suas_sucd_bidi(0xE000) == SUCD_BIDI_NEUTRAL);  /* private use (unmapped) */
    assert(suas_sucd_bidi(0x00120000) == SUCD_BIDI_NEUTRAL); /* native, out of bridge */

    printf("[PASS] test_sucd_bidi_masks\n");
}

/* ── Classification ────────────────────────────────────────────── */

static void test_classify(void)
{
    suas_dir_type_t dir;
    int mirror;

    /* Latin 'A' under LTR */
    assert(suas_sdf_classify(0x0041, SUAS_DIR_LTR, SUAS_SUCD_MASK(0x0041),
                             &dir, &mirror) == SUAS_SDF_OK);
    assert(dir == SUAS_DIR_LTR && mirror == 0);

    /* Arabic letter under any resolved direction -> RTL */
    assert(suas_sdf_classify(0x0627, SUAS_DIR_LTR, suas_sucd_bidi(0x0627),
                             &dir, &mirror) == SUAS_SDF_OK);
    assert(dir == SUAS_DIR_RTL && mirror == 0);

    /* Mirrorable ')' under LTR -> MIRRORED_LTR, mirrored=1 */
    assert(suas_sdf_classify(0x0029, SUAS_DIR_LTR, suas_sucd_bidi(0x0029),
                             &dir, &mirror) == SUAS_SDF_OK);
    assert(dir == SUAS_DIR_MIRRORED_LTR && mirror == 1);

    /* Mirrorable '(' under RTL -> MIRRORED_RTL, mirrored=1 */
    assert(suas_sdf_classify(0x0028, SUAS_DIR_RTL, suas_sucd_bidi(0x0028),
                             &dir, &mirror) == SUAS_SDF_OK);
    assert(dir == SUAS_DIR_MIRRORED_RTL && mirror == 1);

    /* Neutral digit under RTL -> NEUTRAL, no mirror */
    assert(suas_sdf_classify(0x0030, SUAS_DIR_RTL, suas_sucd_bidi(0x0030),
                             &dir, &mirror) == SUAS_SDF_OK);
    assert(dir == SUAS_DIR_NEUTRAL && mirror == 0);

    /* Bad arguments rejected */
    assert(suas_sdf_classify(0x0041, SUAS_DIR_LTR, 0, NULL, &mirror) == SUAS_SDF_ERR_INVALID_ARG);
    assert(suas_sdf_classify(0x0041, SUAS_DIR_LTR, 0, &dir, NULL) == SUAS_SDF_ERR_INVALID_ARG);

    printf("[PASS] test_classify\n");
}

/* ── State machine / Dual-Mode resolver ───────────────────────── */


static void test_explicit_directives(void)
{
    suas_sdf_state_t st;
    suts32_framed_t w;
    size_t count = 0;

    /* Root isolate starts LTR */
    suas_sdf_init(&st, SUAS_DIR_LTR);
    assert(suas_sdf_depth(&st) == 1);
    assert(suas_sdf_current_dir(&st) == SUAS_DIR_LTR);
    assert(suas_sdf_runtime(&st) == SUAS_SDF_RUNTIME_ACTIVE);

    /* RTL directive switches active direction, emits nothing */
    count = 0;
    assert(suas_sdf_process_codepoint(&st, SCP_DIR_RTL, &w, &count) == SUAS_SDF_OK);
    assert(count == 0);
    assert(suas_sdf_current_dir(&st) == SUAS_DIR_RTL);

    /* Push isolate inherits RTL */
    assert(suas_sdf_process_codepoint(&st, SCP_DIR_ISOLATE_PUSH, &w, &count) == SUAS_SDF_OK);
    assert(count == 0);
    assert(suas_sdf_depth(&st) == 2);
    assert(suas_sdf_current_dir(&st) == SUAS_DIR_RTL);

    /* Inside isolate, switch to LTR */
    assert(suas_sdf_process_codepoint(&st, SCP_DIR_LTR, &w, &count) == SUAS_SDF_OK);
    assert(suas_sdf_current_dir(&st) == SUAS_DIR_LTR);

    /* Pop returns to parent RTL */
    assert(suas_sdf_process_codepoint(&st, SCP_DIR_ISOLATE_POP, &w, &count) == SUAS_SDF_OK);
    assert(count == 0);
    assert(suas_sdf_depth(&st) == 1);
    assert(suas_sdf_current_dir(&st) == SUAS_DIR_RTL);

    printf("[PASS] test_explicit_directives\n");
}

static void test_unicode_bridge_implicit(void)
{
    suas_sdf_state_t st;
    suts32_framed_t w;
    size_t count = 0;

    suas_sdf_init(&st, SUAS_DIR_LTR);

    /* Latin 'A' in the Unicode Bridge frames LTR, no mirror */
    count = 0;
    assert(suas_sdf_process_codepoint(&st, 0x0041, &w, &count) == SUAS_SDF_OK);
    assert(count == 1);
    assert(SUAS_SDF_FRAMED_CP(w) == 0x0041);
    assert(SUAS_SDF_FRAMED_DIR(w) == SUAS_DIR_LTR);
    assert(SUAS_SDF_FRAMED_MIRROR(w) == 0);

    /* Arabic letter frames RTL regardless of LTR context */
    assert(suas_sdf_process_codepoint(&st, 0x0627, &w, &count) == SUAS_SDF_OK);
    assert(SUAS_SDF_FRAMED_DIR(w) == SUAS_DIR_RTL);
    assert(SUAS_SDF_FRAMED_MIRROR(w) == 0);

    /* Mirrorable ')' in neutral-linked set -> mirrors in LTR context */
    st.runtime = SUAS_SDF_RUNTIME_ACTIVE;
    assert(suas_sdf_process_codepoint(&st, 0x0029, &w, &count) == SUAS_SDF_OK);
    assert(SUAS_SDF_FRAMED_DIR(w) == SUAS_DIR_MIRRORED_LTR);
    assert(SUAS_SDF_FRAMED_MIRROR(w) == 1);

    printf("[PASS] test_unicode_bridge_implicit\n");
}

static void test_native_inherit(void)
{
    suas_sdf_state_t st;
    suts32_framed_t w;
    size_t count = 0;

    /* Native SUCS codepoint under root LTR inherits (frames NEUTRAL) */
    suas_sdf_init(&st, SUAS_DIR_LTR);
    count = 0;
    assert(suas_sdf_process_codepoint(&st, 0x00120000, &w, &count) == SUAS_SDF_OK);
    assert(count == 1);
    assert(SUAS_SDF_FRAMED_CP(w) == 0x00120000);
    assert(SUAS_SDF_FRAMED_MIRROR(w) == 0);

    /* Under an RTL isolate, native codepoint still inherits RTL scope:
     * dir_type is NEUTRAL but resolves at render from current_dir. */
    assert(suas_sdf_process_codepoint(&st, SCP_DIR_RTL, &w, &count) == SUAS_SDF_OK);
    assert(suas_sdf_current_dir(&st) == SUAS_DIR_RTL);
    assert(suas_sdf_process_codepoint(&st, 0x0012FFFF, &w, &count) == SUAS_SDF_OK);
    assert(SUAS_SDF_FRAMED_CP(w) == 0x0012FFFF);
    assert(SUAS_SDF_FRAMED_DIR(w) == SUAS_DIR_NEUTRAL);

    printf("[PASS] test_native_inherit\n");
}

static void test_structural_errors(void)
{
    suas_sdf_state_t st;
    suts32_framed_t w;
    size_t count = 0;

    /* Popping the root isolate is a structural underflow error */
    suas_sdf_init(&st, SUAS_DIR_LTR);
    assert(suas_sdf_process_codepoint(&st, SCP_DIR_ISOLATE_POP, &w, &count) == SUAS_SDF_ERR_STACK_UNDERFLOW);
    assert(suas_sdf_runtime(&st) == SUAS_SDF_RUNTIME_ERROR);

    /* Stack overflow: push past SUAS_SDF_STACK_DEPTH */
    suas_sdf_init(&st, SUAS_DIR_LTR);
    for (int i = 0; i < SUAS_SDF_STACK_DEPTH - 1; ++i) {
        assert(suas_sdf_process_codepoint(&st, SCP_DIR_ISOLATE_PUSH, &w, &count) == SUAS_SDF_OK);
    }
    /* One more push exceeds depth */
    assert(suas_sdf_process_codepoint(&st, SCP_DIR_ISOLATE_PUSH, &w, &count) == SUAS_SDF_ERR_STACK_OVERFLOW);
    assert(suas_sdf_runtime(&st) == SUAS_SDF_RUNTIME_ERROR);

    /* Input after error is rejected */
    assert(suas_sdf_process_codepoint(&st, 0x0041, &w, &count) == SUAS_SDF_ERR_STATE);

    /* Input after finish() is rejected */
    suas_sdf_init(&st, SUAS_DIR_LTR);
    assert(suas_sdf_finish(&st) == SUAS_SDF_OK);
    assert(suas_sdf_runtime(&st) == SUAS_SDF_RUNTIME_ENDED);
    assert(suas_sdf_process_codepoint(&st, 0x0041, &w, &count) == SUAS_SDF_ERR_STATE);

    printf("[PASS] test_structural_errors\n");
}

static void test_one_shot(void)
{
    /* "A(ב" <- Latin A, '(' , Hebrew bet under root LTR */
    uint32_t cps[] = { 0x0041, 0x0028, 0x05D1 };
    suts32_framed_t out[8];
    size_t count = 0;

    suas_sdf_status_t rc = suas_sdf_frame(cps, 3, out, 8, &count);
    assert(rc == SUAS_SDF_OK);
    assert(count == 3);

    assert(SUAS_SDF_FRAMED_CP(out[0]) == 0x0041);
    assert(SUAS_SDF_FRAMED_DIR(out[0]) == SUAS_DIR_LTR);
    assert(SUAS_SDF_FRAMED_MIRROR(out[0]) == 0);

    assert(SUAS_SDF_FRAMED_CP(out[1]) == 0x0028);
    assert(SUAS_SDF_FRAMED_DIR(out[1]) == SUAS_DIR_MIRRORED_LTR);
    assert(SUAS_SDF_FRAMED_MIRROR(out[1]) == 1);

    assert(SUAS_SDF_FRAMED_CP(out[2]) == 0x05D1);
    assert(SUAS_SDF_FRAMED_DIR(out[2]) == SUAS_DIR_RTL);
    assert(SUAS_SDF_FRAMED_MIRROR(out[2]) == 0);

    /* Buffer-too-small */
    count = 0;
    rc = suas_sdf_frame(cps, 3, out, 2, &count);
    assert(rc == SUAS_SDF_ERR_BUFFER_TOO_SMALL);

    /* Bad args */
    assert(suas_sdf_frame(cps, 3, NULL, 8, &count) == SUAS_SDF_ERR_INVALID_ARG);
    assert(suas_sdf_frame(NULL, 3, out, 8, &count) == SUAS_SDF_ERR_INVALID_ARG);
    assert(suas_sdf_frame(cps, 3, out, 0, &count) == SUAS_SDF_ERR_INVALID_ARG);

    printf("[PASS] test_one_shot\n");
}

/* ── Full bidirectional processing model (UAX #9) ────────────── */

static void test_bidi_classify(void)
{
    assert(suas_sdf_bidi_classify(0x41)    == SUAS_SDF_BIDI_L);   /* A   */
    assert(suas_sdf_bidi_classify(0x05D0)  == SUAS_SDF_BIDI_R);   /* alef */
    assert(suas_sdf_bidi_classify(0x0627)  == SUAS_SDF_BIDI_AL);  /* alef-arabic */
    assert(suas_sdf_bidi_classify(0x0030)  == SUAS_SDF_BIDI_EN);  /* 0 */
    assert(suas_sdf_bidi_classify(0x0660)  == SUAS_SDF_BIDI_AN);  /* arabic-indic digit */
    assert(suas_sdf_bidi_classify(0x0028)  == SUAS_SDF_BIDI_ON);  /* ( */
    assert(suas_sdf_bidi_classify(0x0020)  == SUAS_SDF_BIDI_WS);  /* space */
    assert(suas_sdf_bidi_classify(0x202A)  == SUAS_SDF_BIDI_LRE);
    assert(suas_sdf_bidi_classify(0x202E)  == SUAS_SDF_BIDI_RLO);
    assert(suas_sdf_bidi_classify(0x2066)  == SUAS_SDF_BIDI_LRI);
    assert(suas_sdf_bidi_classify(0x2069)  == SUAS_SDF_BIDI_PDI);
    /* SCP / native default L */
    assert(suas_sdf_bidi_classify(SCP_DIR_LTR) == SUAS_SDF_BIDI_L);
    assert(suas_sdf_bidi_classify(0x00120000)  == SUAS_SDF_BIDI_L);

    printf("[PASS] test_bidi_classify\n");
}

static void test_bidi_paragraph_direction(void)
{
    uint32_t lat[] = { 0x61, 0x62, 0x63 };
    uint32_t heb[] = { 0x05D0, 0x05D1, 0x05D2 };
    suas_sdf_run_t out[8];
    uint8_t pl = 0xFF;

    assert(suas_sdf_resolve_paragraph(lat, 3, SUAS_SDF_PARA_AUTO, out, NULL, &pl) == SUAS_SDF_BIDI_OK);
    assert(pl == 0);                    /* first strong L -> LTR */
    assert(suas_sdf_resolve_paragraph(heb, 3, SUAS_SDF_PARA_AUTO, out, NULL, &pl) == SUAS_SDF_BIDI_OK);
    assert(pl == 1);                    /* first strong R -> RTL */
    assert(suas_sdf_resolve_paragraph(lat, 3, SUAS_SDF_PARA_RTL, out, NULL, &pl) == SUAS_SDF_BIDI_OK);
    assert(pl == 1);                    /* forced RTL */
    assert(suas_sdf_resolve_paragraph(heb, 3, SUAS_SDF_PARA_LTR, out, NULL, &pl) == SUAS_SDF_BIDI_OK);
    assert(pl == 0);                    /* forced LTR */

    printf("[PASS] test_bidi_paragraph_direction\n");
}

static void test_bidi_basic_ltr(void)
{
    uint32_t cps[] = { 0x41, 0x42, 0x43 };
    suas_sdf_run_t out[8];
    int visual[8];
    uint8_t pl;

    assert(suas_sdf_resolve_paragraph(cps, 3, SUAS_SDF_PARA_AUTO, out, visual, &pl) == SUAS_SDF_BIDI_OK);
    assert(pl == 0);
    assert(out[0].level == 0 && out[1].level == 0 && out[2].level == 0);
    assert(out[0].cls == SUAS_SDF_BIDI_L);
    assert(visual[0] == 0 && visual[1] == 1 && visual[2] == 2);
    assert(out[0].mirrored == 0 && out[0].removed == 0);

    printf("[PASS] test_bidi_basic_ltr\n");
}

static void test_bidi_mixed_reorder(void)
{
    /* A alef bet B : the Hebrew run must reverse (RTL) at level 1 */
    uint32_t cps[] = { 0x41, 0x05D0, 0x05D1, 0x42 };
    suas_sdf_run_t out[8];
    int visual[8];
    uint8_t pl;

    assert(suas_sdf_resolve_paragraph(cps, 4, SUAS_SDF_PARA_AUTO, out, visual, &pl) == SUAS_SDF_BIDI_OK);
    assert(pl == 0);
    assert(out[1].level == 1);          /* alef resolved R -> odd */
    assert(out[2].level == 1);          /* bet  resolved R -> odd */
    assert(out[0].level == 0 && out[3].level == 0);
    /* visual order: A, bet, alef, B */
    assert(visual[0] == 0 && visual[1] == 2 && visual[2] == 1 && visual[3] == 3);

    printf("[PASS] test_bidi_mixed_reorder\n");
}

static void test_bidi_embedding(void)
{
    /* A RLE alef PDF B : RLE raises alef to level 1; RLE/PDF removed */
    uint32_t cps[] = { 0x41, SUAS_SDF_CP_RLE, 0x05D0, SUAS_SDF_CP_PDF, 0x42 };
    suas_sdf_run_t out[8];
    uint8_t pl;

    assert(suas_sdf_resolve_paragraph(cps, 5, SUAS_SDF_PARA_LTR, out, NULL, &pl) == SUAS_SDF_BIDI_OK);
    assert(pl == 0);
    assert(out[1].removed == 1);        /* RLE removed */
    assert(out[3].removed == 1);        /* PDF removed */
    assert(out[2].level == 1);          /* alef inside RLE */
    assert(out[0].level == 0 && out[4].level == 0);

    printf("[PASS] test_bidi_embedding\n");
}

static void test_bidi_override(void)
{
    /* A RLO B PDF B : RLO forces following 'B' to R at level 1 */
    uint32_t cps[] = { 0x41, SUAS_SDF_CP_RLO, 0x42, SUAS_SDF_CP_PDF, 0x42 };
    suas_sdf_run_t out[8];
    uint8_t pl;

    assert(suas_sdf_resolve_paragraph(cps, 5, SUAS_SDF_PARA_LTR, out, NULL, &pl) == SUAS_SDF_BIDI_OK);
    assert(out[2].cls == SUAS_SDF_BIDI_R);   /* overridden to R */
    assert(out[2].level == 1);
    assert(out[4].cls == SUAS_SDF_BIDI_L);   /* after PDF, back to L */
    assert(out[4].level == 0);

    printf("[PASS] test_bidi_override\n");
}

static void test_bidi_isolate(void)
{
    /* LRI alef PDI : isolate initiator/PDI are retained (not removed),
     * and the contained alef is embedded at an even base level (then
     * R resolves to odd). Align with root: para forced LTR. */
    uint32_t cps[] = { SUAS_SDF_CP_LRI, 0x05D0, SUAS_SDF_CP_PDI };
    suas_sdf_run_t out[8];
    uint8_t pl;

    assert(suas_sdf_resolve_paragraph(cps, 3, SUAS_SDF_PARA_LTR, out, NULL, &pl) == SUAS_SDF_BIDI_OK);
    assert(pl == 0);
    assert(out[0].removed == 0);        /* LRI kept */
    assert(out[2].removed == 0);        /* PDI kept */
    assert(out[0].level == 0);          /* isolate initiator at enclosing level */
    assert(out[2].level == 0);          /* PDI returns to enclosing level */
    assert(out[1].level == 3);          /* alef: base 2, R -> odd 3 */
    assert(out[1].cls == SUAS_SDF_BIDI_R);

    printf("[PASS] test_bidi_isolate\n");
}

static void test_bidi_bracket_mirror(void)
{
    /* alef ( bet ) in forced RTL paragraph: both parens at odd level get
     * mirrored by L4. */
    uint32_t cps[] = { 0x05D0, 0x0028, 0x05D1, 0x0029 };
    suas_sdf_run_t out[8];
    uint8_t pl;

    assert(suas_sdf_resolve_paragraph(cps, 4, SUAS_SDF_PARA_RTL, out, NULL, &pl) == SUAS_SDF_BIDI_OK);
    assert(pl == 1);
    assert(SUAS_SDF_FRAMED_MIRROR(out[1]) == 1 || out[1].mirrored == 1);
    assert(out[1].cp == 0x0029);        /* '(' mirrored to ')' */
    assert(out[3].cp == 0x0028);        /* ')' mirrored to '(' */
    assert(out[1].level == 1);          /* odd level triggers mirror */

    printf("[PASS] test_bidi_bracket_mirror\n");
}

static void test_bidi_mirror_helper(void)
{
    uint32_t paired;
    int mirrored;

    suas_sdf_bidi_mirror(0x0028, &paired, &mirrored);
    assert(paired == 0x0029 && mirrored == 1);
    suas_sdf_bidi_mirror(0x005B, &paired, &mirrored);
    assert(paired == 0x005D && mirrored == 1);
    suas_sdf_bidi_mirror(0x0041, &paired, &mirrored);  /* not mirrorable */
    assert(paired == 0x0041 && mirrored == 0);
    suas_sdf_bidi_mirror(0x00120000, &paired, &mirrored); /* native */
    assert(paired == 0x00120000 && mirrored == 0);

    printf("[PASS] test_bidi_mirror_helper\n");
}

static void test_bidi_errors(void)
{
    uint32_t cps[] = { 0x41 };
    suas_sdf_run_t out[8];
    uint32_t big[SUAS_SDF_BIDI_MAX_LEN + 8];
    size_t i;
    for (i = 0; i < SUAS_SDF_BIDI_MAX_LEN + 8; ++i) big[i] = 0x41;

    assert(suas_sdf_resolve_paragraph(NULL, 1, SUAS_SDF_PARA_AUTO, out, NULL, NULL) == SUAS_SDF_BIDI_ERR_INVALID_ARG);
    assert(suas_sdf_resolve_paragraph(cps, 1, SUAS_SDF_PARA_AUTO, NULL, NULL, NULL) == SUAS_SDF_BIDI_ERR_INVALID_ARG);
    assert(suas_sdf_resolve_paragraph(big, SUAS_SDF_BIDI_MAX_LEN + 1, SUAS_SDF_PARA_AUTO, out, NULL, NULL) == SUAS_SDF_BIDI_ERR_TOO_LONG);
    /* empty run is OK */
    assert(suas_sdf_resolve_paragraph(cps, 0, SUAS_SDF_PARA_AUTO, out, NULL, NULL) == SUAS_SDF_BIDI_OK);

    printf("[PASS] test_bidi_errors\n");
}

int main(void)
{
    printf("--- Running SUAS-001 SDF Unit Tests ---\n");
    test_sucd_bidi_masks();
    test_classify();
    test_explicit_directives();
    test_unicode_bridge_implicit();
    test_native_inherit();
    test_structural_errors();
    test_one_shot();
    test_bidi_classify();
    test_bidi_paragraph_direction();
    test_bidi_basic_ltr();
    test_bidi_mixed_reorder();
    test_bidi_embedding();
    test_bidi_override();
    test_bidi_isolate();
    test_bidi_bracket_mirror();
    test_bidi_mirror_helper();
    test_bidi_errors();
    printf("--- ALL SDF TESTS PASSED ---\n");
    return 0;
}
