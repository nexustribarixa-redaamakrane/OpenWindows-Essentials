/**
 * SUAS-002 SGW — System Glyph Width & Monospace Grid
 *
 * Unit tests for the fixed-cell / monospace grid engine.
 *
 * Covers: width classification over the six classes, zone dispatch over the
 * 64-bit space, contextual Ambiguous resolution, the O(1) grid cell + column
 * advance contract, non-advancing control/trap handling, and the tailoring
 * override hook.
 */

#include <stdio.h>
#include <assert.h>
#include "suas/suas_sgw.h"

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_fail = 1; \
} } while (0)

static void test_defaults(void)
{
    suas_sgw_options_t o;
    suas_sgw_options_default(&o);
    CHECK(o.overrides == NULL);
    CHECK(o.count == 0);
    CHECK(suas_sgw_version_string() != NULL);
}

static void test_width_classes(void)
{
    suas_sgw_options_t o;
    suas_sgw_options_default(&o);

    /* Narrow (ASCII default) */
    CHECK(suas_sgw_resolve(0x0041ULL, &o) == SUAS_SGW_W_NARROW); /* A */
    CHECK(suas_sgw_resolve(0x00A5ULL, &o) == SUAS_SGW_W_NARROW); /* ¥ */

    /* Neutral: decomposition starts with a Narrow form */
    CHECK(suas_sgw_resolve(0x00C5ULL, &o) == SUAS_SGW_W_NEUTRAL); /* Å */
    CHECK(suas_sgw_resolve(0x01D3ULL, &o) == SUAS_SGW_W_NEUTRAL); /* Ǔ */

    /* Ambiguous */
    CHECK(suas_sgw_resolve(0x01D4ULL, &o) == SUAS_SGW_W_AMBIGUOUS); /* ǔ */
    CHECK(suas_sgw_resolve(0x212BULL, &o) == SUAS_SGW_W_AMBIGUOUS); /* Å */

    /* Halfwidth */
    CHECK(suas_sgw_resolve(0x20A9ULL, &o) == SUAS_SGW_W_HALFWIDTH); /* ₩ WON SIGN */
    CHECK(suas_sgw_resolve(0xFF61ULL, &o) == SUAS_SGW_W_HALFWIDTH); /* ｡ */

    /* Wide: Han, Hangul, Kana */
    CHECK(suas_sgw_resolve(0x4E00ULL, &o) == SUAS_SGW_W_WIDE); /* 一 */
    CHECK(suas_sgw_resolve(0x3400ULL, &o) == SUAS_SGW_W_WIDE); /* CJK Ext A */
    CHECK(suas_sgw_resolve(0xF900ULL, &o) == SUAS_SGW_W_WIDE); /* CJK Compat */
    CHECK(suas_sgw_resolve(0xAC00ULL, &o) == SUAS_SGW_W_WIDE); /* 한 */
    CHECK(suas_sgw_resolve(0x3041ULL, &o) == SUAS_SGW_W_WIDE); /* ぁ */
    CHECK(suas_sgw_resolve(0x3000ULL, &o) == SUAS_SGW_W_WIDE); /* ideographic space */
    /* Unassigned Han ranges are Wide */
    CHECK(suas_sgw_resolve(0x20000ULL, &o) == SUAS_SGW_W_WIDE);
    CHECK(suas_sgw_resolve(0x30000ULL, &o) == SUAS_SGW_W_WIDE);
    CHECK(suas_sgw_resolve(0x2FFFDULL, &o) == SUAS_SGW_W_WIDE);

    /* Fullwidth */
    CHECK(suas_sgw_resolve(0xFF01ULL, &o) == SUAS_SGW_W_FULLWIDTH); /* ！ */
    CHECK(suas_sgw_resolve(0xFFE0ULL, &o) == SUAS_SGW_W_FULLWIDTH); /* ￠ */

    /* Emoji_Presentation → Wide, but Regional_Indicator → not wide */
    CHECK(suas_sgw_resolve(0x1F600ULL, &o) == SUAS_SGW_W_WIDE);     /* 😀 */
    CHECK(suas_sgw_resolve(0x1F1E6ULL, &o) == SUAS_SGW_W_NARROW);   /* 🇦 RI not wide */
}

static void test_zone_dispatch(void)
{
    suas_sgw_options_t o;
    suas_sgw_options_default(&o);

    /* SCP / trap / sentinel are non-advancing control plane. */
    CHECK(suas_sgw_cells(0x00110001ULL, false, &o) == SUAS_SGW_GRID_NONE);
    CHECK(suas_sgw_cells(0x7FFFFFF6ULL, false, &o) == SUAS_SGW_GRID_NONE);
    CHECK(suas_sgw_cells(0x7FFFFFFFULL, false, &o) == SUAS_SGW_GRID_NONE);

    /* Native SUCS default is a single cell. */
    CHECK(suas_sgw_cells(0x00120000ULL, false, &o) == SUAS_SGW_GRID_ONE);
    CHECK(suas_sgw_resolve(0x00120000ULL, &o) == SUAS_SGW_W_NEUTRAL);

    /* ExtSUCS plugin space default is a single cell. */
    CHECK(suas_sgw_cells(0x80000000ULL, false, &o) == SUAS_SGW_GRID_ONE);
    CHECK(suas_sgw_resolve(0x80000000ULL, &o) == SUAS_SGW_W_NEUTRAL);
    CHECK(suas_sgw_cells(0xFFFFFFFFFFFFFFFFULL, false, &o) == SUAS_SGW_GRID_ONE);
}

static void test_grid_cells(void)
{
    suas_sgw_options_t o;
    suas_sgw_options_default(&o);

    /* Half/narrow/neutral → 1 cell */
    CHECK(suas_sgw_cells(0x0041ULL, false, &o) == SUAS_SGW_GRID_ONE);
    CHECK(suas_sgw_cells(0x20A9ULL, false, &o) == SUAS_SGW_GRID_ONE);
    CHECK(suas_sgw_cells(0x00C5ULL, false, &o) == SUAS_SGW_GRID_ONE);

    /* Wide/fullwidth → 2 cells */
    CHECK(suas_sgw_cells(0x4E00ULL, false, &o) == SUAS_SGW_GRID_TWO);
    CHECK(suas_sgw_cells(0xFF01ULL, false, &o) == SUAS_SGW_GRID_TWO);
    CHECK(suas_sgw_cells(0x1F600ULL, false, &o) == SUAS_SGW_GRID_TWO);

    /* Ambiguous depends on context */
    CHECK(suas_sgw_cells(0x01D4ULL, false, &o) == SUAS_SGW_GRID_ONE);
    CHECK(suas_sgw_cells(0x01D4ULL, true,  &o) == SUAS_SGW_GRID_TWO);
    CHECK(suas_sgw_resolve_ambiguous(0x01D4ULL, false, &o) == SUAS_SGW_GRID_ONE);
    CHECK(suas_sgw_resolve_ambiguous(0x01D4ULL, true,  &o) == SUAS_SGW_GRID_TWO);
}

static void test_column_advance(void)
{
    suas_sgw_options_t o;
    suas_sgw_options_default(&o);
    size_t col = 0;

    /* "A" then "一" then combining-mark control */
    CHECK(suas_sgw_column_advance(0x0041ULL, false, &o, &col) == SUAS_SGW_OK);
    CHECK(col == 1);
    CHECK(suas_sgw_column_advance(0x4E00ULL, false, &o, &col) == SUAS_SGW_OK);
    CHECK(col == 3);
    /* SCP control does not advance */
    CHECK(suas_sgw_column_advance(0x00110001ULL, false, &o, &col) == SUAS_SGW_OK);
    CHECK(col == 3);

    CHECK(suas_sgw_column_advance(0x0041ULL, false, &o, NULL) == SUAS_SGW_ERR_NULL_POINTER);
}

static void test_grid_batch(void)
{
    suas_sgw_options_t o;
    suas_sgw_options_default(&o);
    const sucs_ex_char_t cps[] = { 0x0041ULL, 0x4E00ULL, 0xFF61ULL, 0x3000ULL };
    suas_sgw_grid_t out[4];
    size_t cols[4];

    CHECK(suas_sgw_grid(cps, 4, false, &o, out, cols) == SUAS_SGW_OK);
    CHECK(out[0] == SUAS_SGW_GRID_ONE);
    CHECK(out[1] == SUAS_SGW_GRID_TWO);
    CHECK(out[2] == SUAS_SGW_GRID_ONE);   /* ｡ halfwidth */
    CHECK(out[3] == SUAS_SGW_GRID_TWO);   /* ideographic space wide */
    CHECK(cols[0] == 1);
    CHECK(cols[1] == 3);
    CHECK(cols[2] == 4);
    CHECK(cols[3] == 6);

    CHECK(suas_sgw_grid(NULL, 4, false, &o, out, cols) == SUAS_SGW_ERR_NULL_POINTER);
    CHECK(suas_sgw_grid(cps, 4, false, &o, NULL, cols) == SUAS_SGW_ERR_NULL_POINTER);
}

static void test_override_hook(void)
{
    /* Pin native SUCS range + override an ambiguous cp to wide.
     * Entries MUST be sorted ascending by lo for the binary search. */
    static const suas_sgw_override_t ovr[] = {
        { 0x01D4ULL, 0x01D4ULL, SUAS_SGW_W_FULLWIDTH },
        { 0x00120000ULL, 0x0012FFFFULL, SUAS_SGW_W_WIDE },
    };
    suas_sgw_options_t o;
    o.overrides = ovr;
    o.count = 2;

    CHECK(suas_sgw_resolve(0x00120000ULL, &o) == SUAS_SGW_W_WIDE);
    CHECK(suas_sgw_cells(0x00120000ULL, false, &o) == SUAS_SGW_GRID_TWO);
    /* Override of an ambiguous cp to fullwidth forces 2 cells even in narrow ctx */
    CHECK(suas_sgw_cells(0x01D4ULL, false, &o) == SUAS_SGW_GRID_TWO);
}

int main(void)
{
    test_defaults();
    test_width_classes();
    test_zone_dispatch();
    test_grid_cells();
    test_column_advance();
    test_grid_batch();
    test_override_hook();

    if (g_fail) {
        printf("SUAS-002 SGW: FAIL\n");
        return 1;
    }
    printf("[PASS] test_suas_sgw\n");
    return 0;
}
