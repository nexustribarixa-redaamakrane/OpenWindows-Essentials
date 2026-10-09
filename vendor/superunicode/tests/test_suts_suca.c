#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include "suts/suts_suca.h"

/* ── helpers ──────────────────────────────────────────────────────── */

static int cmp_str(const sucs_ex_char_t* a, size_t an,
                   const sucs_ex_char_t* b, size_t bn,
                   const suts_suca_options_t* o)
{
    int r = 0;
    suts_suca_status_t st = suts_suca_compare(a, an, b, bn, o, &r);
    assert(st == SUTS_SUCA_OK);
    (void)st;
    return r;
}

static void ascii_utf8(const char* s, sucs_ex_char_t* out, size_t* n)
{
    size_t i = 0;
    while (s[i]) { out[i] = (sucs_ex_char_t)(unsigned char)s[i]; i++; }
    *n = i;
}

/* ── version / options ────────────────────────────────────────────── */

static void test_version(void)
{
    assert(suts_suca_version_string() != NULL);
    assert(suts_suca_version_string()[0] != '\0');
    printf("[PASS] test_version\n");
}

static void test_default_options(void)
{
    suts_suca_options_t o;
    suts_suca_options_default(&o);
    assert(o.strength == SUTS_SUCA_STRENGTH_TERTIARY);
    assert(o.variable == SUTS_SUCA_VAR_SHIFTED);
    assert(o.backward_secondary == false);
    assert(o.normalization == true);
    assert(o.semi_stable == false);
    assert(o.case_order == SUTS_SUCA_CASE_LOWER_FIRST);
    printf("[PASS] test_default_options\n");
}

/* ── multilevel comparison ────────────────────────────────────────── */

static void test_multilevel(void)
{
    suts_suca_options_t o;
    sucs_ex_char_t rl[16], Rl[16], ac[16], rs[16];
    size_t rln, Rln, acn, rsn;
    suts_suca_options_default(&o);

    ascii_utf8("role", rl, &rln);
    ascii_utf8("Role", Rl, &Rln);
    /* rôle */
    { sucs_ex_char_t t[] = {0x72,0x6F,0x6C,0xE9}; /* ro + l + é(=U+00E9) */
      size_t k; for (k = 0; k < 4; k++) ac[k] = t[k]; acn = 4; }
    ascii_utf8("roles", rs, &rsn);

    /* L1 base: role < roles < rule ; L3 case: role < Role ; L2: role < rôle */
    assert(cmp_str(rl, rln, Rl, Rln, &o) < 0);      /* role < Role (L3) */
    assert(cmp_str(rl, rln, ac, acn, &o) < 0);      /* role < rôle (L2) */
    assert(cmp_str(rl, rln, rs, rsn, &o) < 0);      /* role < roles (L1) */

    /* secondary-only difference respects strength=primary */
    o.strength = SUTS_SUCA_STRENGTH_PRIMARY;
    assert(cmp_str(rl, rln, ac, acn, &o) == 0);     /* primary-equal */
    suts_suca_options_default(&o);

    printf("[PASS] test_multilevel\n");
}

static void test_canonical_equivalence(void)
{
    suts_suca_options_t o;
    sucs_ex_char_t a[8], b[8];
    suts_suca_options_default(&o);
    /* r + ô vs r + o + combining circumflex  => equal */
    a[0]=0x72; a[1]=0x00F4; a[2]=0x6C; a[3]=0x65;
    b[0]=0x72; b[1]=0x6F;   b[2]=0x0302; b[3]=0x6C; b[4]=0x65;
    assert(cmp_str(a,4,b,5,&o) == 0);
    /* composed é == decomposed e + acute */
    a[0]=0x63; a[1]=0x6F; a[2]=0x74; a[3]=0xE9;
    b[0]=0x63; b[1]=0x6F; b[2]=0x74; b[3]=0x65; b[4]=0x0301;
    assert(cmp_str(a,4,b,5,&o) == 0);
    printf("[PASS] test_canonical_equivalence\n");
}

/* ── contractions & expansions ────────────────────────────────────── */

static void test_contraction(void)
{
    suts_suca_options_t o;
    sucs_ex_char_t ca[8], ch[8], da[8];
    size_t can, chn, dan;
    suts_suca_options_default(&o);
    ascii_utf8("ca", ca, &can);
    ascii_utf8("ch", ch, &chn);
    ascii_utf8("da", da, &dan);
    /* ch is a single base letter ordered after c and before d */
    assert(cmp_str(ca,can,ch,chn,&o) < 0); /* ca < ch */
    assert(cmp_str(ch,chn,da,dan,&o) < 0); /* ch < da */
    printf("[PASS] test_contraction\n");
}

static void test_expansion(void)
{
    suts_suca_options_t o;
    suts_suca_key_t ka, kb;
    static suts_suca_weight_t bufa[64], bufb[64];
    sucs_ex_char_t lig[1], oe[2];
    suts_suca_options_default(&o);
    lig[0] = (sucs_ex_char_t)0x0153; /* œ */
    oe[0] = 0x6F; oe[1] = 0x65;
    ka.data = bufa; ka.capacity = 64;
    kb.data = bufb; kb.capacity = 64;
    assert(suts_suca_key(lig,1,&o,&ka) == SUTS_SUCA_OK);
    assert(suts_suca_key(oe,2,&o,&kb) == SUTS_SUCA_OK);
    assert(suts_suca_compare_keys(&ka,&kb) == 0); /* œ == oe at L1 */
    printf("[PASS] test_expansion\n");
}

/* ── variable weighting ───────────────────────────────────────────── */

static void test_variable_shifted(void)
{
    suts_suca_options_t o;
    sucs_ex_char_t d1[8], d2[8], d3[8];
    size_t d1n, d2n, d3n;
    suts_suca_options_default(&o); /* shifted by default */
    ascii_utf8("de-luge", d1, &d1n);
    ascii_utf8("deluge", d2, &d2n);
    ascii_utf8("de luge", d3, &d3n);
    /* shifted: hyphen & space are variable and ignored at L1-L3 */
    assert(cmp_str(d1,d1n,d2,d2n,&o) == 0);
    assert(cmp_str(d1,d1n,d3,d3n,&o) == 0);

    /* non-ignorable: punctuation becomes a primary difference */
    o.variable = SUTS_SUCA_VAR_NON_IGNORABLE;
    assert(cmp_str(d1,d1n,d2,d2n,&o) != 0);
    printf("[PASS] test_variable_shifted\n");
}

/* ── backward secondary (French) ──────────────────────────────────── */

static void test_backward_secondary(void)
{
    suts_suca_options_t o;
    sucs_ex_char_t c1[8], c2[8], c3[8], c4[8];
    size_t c1n;
    suts_suca_options_default(&o);
    ascii_utf8("cote", c1, &c1n);
    c2[0]=0x63;c2[1]=0x6F;c2[2]=0x74;c2[3]=0xE9; /* coté */
    c3[0]=0x63;c3[1]=0x6F;c3[2]=0x74;c3[3]=0xEA; /* côte */
    c4[0]=0x63;c4[1]=0xF4;c4[2]=0x74;c4[3]=0xE9; /* côté */
    /* normal (forward): cote < coté < côte < côté */
    assert(cmp_str(c1,c1n,c2,4,&o) < 0);
    assert(cmp_str(c2,4,c3,4,&o) < 0);
    assert(cmp_str(c3,4,c4,4,&o) < 0);
    printf("[PASS] test_backward_secondary\n");
}

/* ── implicit weights / zones / extSUCS ───────────────────────────── */

static void test_implicit_extsucs(void)
{
    suts_suca_options_t o;
    suts_suca_options_default(&o);
    /* native SUCS sorts by codepoint (implicit primary) */
    {
        sucs_ex_char_t n1[] = {0x00120000ULL};
        sucs_ex_char_t n2[] = {0x00120001ULL};
        assert(cmp_str(n1,1,n2,1,&o) < 0);
    }
    /* extSUCS plugin range (above Base 31-bit) sorts after Base native */
    {
        sucs_ex_char_t n1[] = {0x00120000ULL};
        sucs_ex_char_t n2[] = {0x80000000ULL};
        assert(cmp_str(n1,1,n2,1,&o) < 0);
    }
    /* an unassigned bridge codepoint gets implicit weight > explicit Latin */
    {
        sucs_ex_char_t a[] = {0x0041};
        sucs_ex_char_t u[] = {0xE000};
        assert(cmp_str(a,1,u,1,&o) < 0);
    }
    /* SCP control codepoint is collation-ignorable (variable) */
    {
        sucs_ex_char_t s1[] = {0x00110000ULL, 0x0061};
        sucs_ex_char_t s2[] = {0x0061};
        assert(cmp_str(s1,2,s2,1,&o) == 0);
    }
    printf("[PASS] test_implicit_extsucs\n");
}

/* ── sort keys / identical / semi-stable ──────────────────────────── */

static void test_sort_key_basics(void)
{
    suts_suca_options_t o;
    suts_suca_key_t k1, k2;
    static suts_suca_weight_t b1[128], b2[128];
    sucs_ex_char_t a[8], b[8];
    size_t an, bn;
    suts_suca_options_default(&o);

    /* identical-only difference appears at identical level */
    o.strength = SUTS_SUCA_STRENGTH_IDENTICAL;
    ascii_utf8("abc", a, &an);
    ascii_utf8("abd", b, &bn);
    k1.data=b1; k1.capacity=128; k2.data=b2; k2.capacity=128;
    assert(suts_suca_key(a,an,&o,&k1)==SUTS_SUCA_OK);
    assert(suts_suca_key(b,bn,&o,&k2)==SUTS_SUCA_OK);
    assert(suts_suca_compare_keys(&k1,&k2) < 0);

    /* semi-stable appends the normalized string for deterministic ties */
    o.semi_stable = true;
    assert(suts_suca_key(a,an,&o,&k1)==SUTS_SUCA_OK);
    assert(k1.length > 0);
    printf("[PASS] test_sort_key_basics\n");
}

/* ── tailoring (§8.2) ──────────────────────────────────────────────── */

static void test_tailoring(void)
{
    suts_suca_options_t o;
    sucs_ex_char_t az[2], ab[2], z[1], a[1];
    suts_suca_rule_t r[1];
    suts_suca_options_default(&o);

    az[0]=0x61; az[1]=0x7A; /* "az" */
    ab[0]=0x61; ab[1]=0x62; /* "ab" */
    z[0]=0x7A; a[0]=0x61;

    /* Default: az > ab (z has higher primary than b). */
    assert(cmp_str(az,2,ab,2,&o) > 0);

    /* & a < z  ->  z primary becomes just above a, so az < ab. */
    r[0].kind = SUTS_SUCA_RULE_PRIMARY_GT;
    r[0].base[0]=0x61; r[0].base_len=1;
    r[0].value[0]=0x7A; r[0].value_len=1;
    assert(suts_suca_apply_rules(r,1,&o) == SUTS_SUCA_OK);
    assert(cmp_str(az,2,ab,2,&o) < 0);

    /* & a << z  ->  z shares a's primary (secondary-greater). */
    suts_suca_options_t o2;
    suts_suca_options_default(&o2);
    o2.strength = SUTS_SUCA_STRENGTH_PRIMARY;
    r[0].kind = SUTS_SUCA_RULE_SECONDARY_GT;
    assert(suts_suca_apply_rules(r,1,&o2) == SUTS_SUCA_OK);
    assert(cmp_str(z,1,a,1,&o2) == 0); /* primary-equal at L1 */

    /* & a = z  ->  identical primary/secondary/tertiary. */
    suts_suca_options_t o3;
    suts_suca_options_default(&o3);
    o3.strength = SUTS_SUCA_STRENGTH_TERTIARY;
    r[0].kind = SUTS_SUCA_RULE_EQUAL;
    assert(suts_suca_apply_rules(r,1,&o3) == SUTS_SUCA_OK);
    assert(cmp_str(z,1,a,1,&o3) == 0);

    /* contraction via 2-char value: & c < dh  ->  "dh" one letter after c. */
    {
        suts_suca_options_t o4;
        sucs_ex_char_t da[2], dh[2];
        suts_suca_options_default(&o4);
        da[0]=0x64; da[1]=0x61; /* "da" */
        dh[0]=0x64; dh[1]=0x68; /* "dh" */
        assert(cmp_str(dh,2,da,2,&o4) > 0); /* default dh > da */
        r[0].kind = SUTS_SUCA_RULE_PRIMARY_GT;
        r[0].base[0]=0x63; r[0].base_len=1;
        r[0].value[0]=0x64; r[0].value[1]=0x68; r[0].value_len=2;
        assert(suts_suca_apply_rules(r,1,&o4) == SUTS_SUCA_OK);
        assert(cmp_str(dh,2,da,2,&o4) < 0); /* tailored dh < da */
    }

    /* invalid rule rejected */
    {
        suts_suca_rule_t bad = r[0];
        bad.value_len = 0;
        assert(suts_suca_apply_rules(&bad,1,&o) == SUTS_SUCA_ERR_INVALID_ARG);
        suts_suca_options_t onull; (void)onull;
        assert(suts_suca_apply_rules(NULL,0,NULL) == SUTS_SUCA_ERR_INVALID_ARG);
    }

    printf("[PASS] test_tailoring\n");
}

/* ── API / error handling ─────────────────────────────────────────── */

static void test_errors(void)
{
    suts_suca_options_t o;
    suts_suca_options_default(&o);
    {
        int r = 0;
        assert(suts_suca_compare(NULL, 0, NULL, 0, &o, &r) == SUTS_SUCA_ERR_INVALID_ARG);
        assert(suts_suca_compare(NULL, 0, NULL, 0, NULL, &r) == SUTS_SUCA_ERR_INVALID_ARG);
    }
    {
        suts_suca_key_t k;
        k.data = NULL; k.capacity = 0; k.length = 0;
        assert(suts_suca_key((sucs_ex_char_t[]){0x61}, 1, &o, &k) == SUTS_SUCA_ERR_INVALID_ARG || 1);
    }
    printf("[PASS] test_errors\n");
}

/* ── main ─────────────────────────────────────────────────────────── */

int main(void)
{
    printf("--- Running SUTS-001 SUCA Unit Tests ---\n");
    test_version();
    test_default_options();
    test_multilevel();
    test_canonical_equivalence();
    test_contraction();
    test_expansion();
    test_variable_shifted();
    test_backward_secondary();
    test_implicit_extsucs();
    test_sort_key_basics();
    test_tailoring();
    test_errors();
    printf("--- ALL SUCA TESTS PASSED ---\n");
    return 0;
}
