/*
 * engines_smoke.c - engines.owd KAT harness (hosted smoke)
 *
 * Covers: census keys for every-known-language names, engine registry &
 * dispatch, the poco kernel (assembler + executor), managednet bytecode
 * front-end, manageddl nested + hot-registration, managedcom component
 * calls, and the managedsh shell subset, including the error paths.
 */

#include <stdio.h>
#include <string.h>

#include "managed_engines.h"

static int g_fails = 0;
static int g_passed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (cond) { g_passed++; }                                       \
        else {                                                          \
            g_fails++;                                                  \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
        }                                                               \
    } while (0)

#define CHECK_ST(got, want)                                             \
    do {                                                                \
        if ((got) == (want)) { g_passed++; }                            \
        else {                                                          \
            g_fails++;                                                  \
            printf("FAIL %s:%d  got=%s want=%s\n",                      \
                   __FILE__, __LINE__,                                  \
                   meng_status_text(got), meng_status_text(want));      \
        }                                                               \
    } while (0)

#define EXPECT_OUT(m, s)                                                \
    do {                                                                \
        size_t _n = strlen(s);                                          \
        if ((m)->out_len == _n && memcmp((m)->out, (s), _n) == 0) {     \
            g_passed++;                                                 \
        } else {                                                        \
            g_fails++;                                                  \
            printf("FAIL %s:%d  out=%.*s want=%s\n", __FILE__, __LINE__, \
                   (int)(m)->out_len, (m)->out, (s));                   \
        }                                                               \
    } while (0)

#define KEXECS(pg)                                                      \
    meng_kernel_exec(&mach, (const uint8_t *)(pg), sizeof(pg) - 1u,     \
                     NULL, 0u, NULL)

#define KGEXECS(pg)                                                     \
    meng_kernel_exec(&mach, (const uint8_t *)(pg), sizeof(pg) - 1u,     \
                     globals, MENG_K_GLOBALS, NULL)

#define DISPS(k, pg)                                                    \
    meng_engine_dispatch(&mach, (k), (const uint8_t *)(pg),             \
                         sizeof(pg) - 1u)

static uint8_t outbuf[256];

static int64_t globals[MENG_K_GLOBALS];

static meng_machine_t mach;

/* ------------------------------------------------------------------ */
/*  managedcom helpers                                                 */
/* ------------------------------------------------------------------ */

static int64_t gsum;

static meng_status_t com_add(meng_com_instance_t *inst, const int64_t *a,
                             uint32_t n, int64_t *r)
{
    (void)inst;
    *r = (n >= 2u) ? (a[0] + a[1]) : 0;
    return MENG_OK;
}

static meng_status_t com_sub(meng_com_instance_t *inst, const int64_t *a,
                             uint32_t n, int64_t *r)
{
    (void)inst;
    *r = (n >= 2u) ? (a[0] - a[1]) : 0;
    return MENG_OK;
}

static meng_status_t com_mul(meng_com_instance_t *inst, const int64_t *a,
                             uint32_t n, int64_t *r)
{
    (void)inst;
    *r = (n >= 2u) ? (a[0] * a[1]) : 0;
    return MENG_OK;
}

static meng_status_t com_factory(const uint8_t ccc[4],
                                 meng_com_instance_t *inst, uint32_t id)
{
    uint32_t iid = 0u;
    meng_status_t st;

    (void)id;
    gsum = 0;
    st = meng_census_key("math", &iid);
    if (st != MENG_OK) return st;

    inst->iface_count = 1u;
    inst->ifaces[0].iid = iid;
    inst->ifaces[0].mslots[0].fn = com_add;
    inst->ifaces[0].mslots[1].fn = com_sub;
    inst->ifaces[0].mslots[2].fn = com_mul;
    inst->data = &gsum;
    (void)ccc;
    return MENG_OK;
}

/* ------------------------------------------------------------------ */
/*  manageddl hot-registration helper                                  */
/* ------------------------------------------------------------------ */

static meng_status_t fake_engine_run(meng_machine_t *m,
                                     const uint8_t *manifest,
                                     size_t manifest_len)
{
    (void)manifest;
    (void)manifest_len;
    return meng_machine_out_cstr(m, "TL");
}

static meng_engine_t fake_engine;

/* ------------------------------------------------------------------ */

static void mach_reset(void)
{
    meng_machine_reset(&mach);
    mach.out = outbuf;
    mach.out_cap = (uint32_t)sizeof(outbuf);
    mach.step_budget = 1u << 20;
}

int main(void)
{
    uint32_t key;
    meng_status_t st;
    uint32_t i;
    char namebuf[64];

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* ---- lifecycle -------------------------------------------------- */
    fprintf(stderr, "[s1]\n");
    CHECK(meng_module_init() == MENG_OK);
    CHECK(meng_abi_version() == 0x00010000u);
    CHECK(meng_engine_count() == 8u);
    CHECK(meng_census_count() == 8u);

    /* ---- census ----------------------------------------------------- */
    fprintf(stderr, "[s2]\n");
    CHECK(meng_census_key("managednet", &key) == MENG_OK);
    CHECK(key == MENG_KEY_MANAGEDNET);
    CHECK(meng_census_key("poco", &key) == MENG_OK);
    CHECK(key == MENG_KEY_POCO);
    CHECK(meng_census_key("managedpy", &key) == MENG_OK);
    CHECK(key == MENG_KEY_MANAGEDPY);

    /* case folding: "Python" and "python" are the same census key */
    {
        uint32_t k1 = 0u, k2 = 0u;
        CHECK(meng_census_key("Python", &k1) == MENG_OK);
        CHECK(meng_census_key("python", &k2) == MENG_OK);
        CHECK(k1 == k2);
        CHECK(k1 == MENG_KEY_MANAGEDPY);
    }

    /* every known language has a deterministic, distinct key */
    {
        const char *langs[] = { "c", "cobol", "fortran", "ada", "pascal",
                                "brainfuck", "apl", "lisp", "prolog",
                                "erlang", "rust", "swift", "go", "c++",
                                "c#", "f#", "vb.net", "sql", "r", "tsql",
                                "x86 asm", "malbolge", "befunge", "j",
                                "k", "scheme", "clojure", "elixir",
                                "haskell", "ocaml", "zig", "v", "dart",
                                "kotlin", "groovy", "perl", "php",
                                "ruby", "lua", "forth", "smalltalk",
                                "simula", "modula-2", "delphi",
                                "visual basic" };
        size_t li, lj;

        for (li = 0u; li < sizeof(langs) / sizeof(langs[0]); li++) {
            uint32_t k;
            CHECK(meng_census_key(langs[li], &k) == MENG_OK);
            for (lj = 0u; lj < li; lj++) {
                uint32_t other;
                CHECK(meng_census_key(langs[lj], &other) == MENG_OK);
                CHECK(k != other);
            }
        }
    }

    /* canonical name round-trip */
    st = meng_census_name(0u, namebuf, sizeof(namebuf));
    CHECK(st == MENG_ERR_NOT_FOUND);
    st = meng_census_name(MENG_KEY_MANAGEDNET, namebuf, sizeof(namebuf));
    CHECK(st == MENG_OK);
    CHECK(strcmp(namebuf, "managednet") == 0);

    /* bad args */
    CHECK(meng_census_key("", &key) == MENG_ERR_BAD_ARG);
    CHECK(meng_census_key(NULL, &key) == MENG_ERR_BAD_ARG);

    /* ---- poco kernel: arithmetic ------------------------------------ */
    fprintf(stderr, "[s3]\n");
    mach_reset();
    st = KEXECS("pushi 5\npushi 3\nimul\npushi 2\niadd\nprint\nhalt");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "17");

    /* string + concat */
    mach_reset();
    st = KEXECS("pushs \"OW\"\npushs \"EN\"\nconcat\nprint\nhalt");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "OWEN");

    /* globals + while-loop via goto */
    mach_reset();
    memset(globals, 0, sizeof(globals));
    st = KGEXECS("pushi 0 gstore 0\n"
                "top:\n"
                "gload 0\nprint\n"
                "pushi 1\ngload 0\niadd\ngstore 0\n"
                "gload 0\npushi 5\ncmplt\njnz top\n"
                "halt\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "01234");

    /* functions via CALL/RET (args passed on the stack) */
    mach_reset();
    st = KEXECS("pushi 6\npushi 7\ncall addf\nprint\nhalt\n"
                "addf:\niadd\nret\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "13");

    /* negative immediate */
    mach_reset();
    st = KEXECS("pushi 7\npushi -2\niadd\nprint\nhalt");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "5");

    /* ---- kernel error paths ----------------------------------------- */
    fprintf(stderr, "[s4]\n");
    mach_reset();
    st = KEXECS("pushi 1\npushs \"a\"\niadd");
    CHECK_ST(st, MENG_ERR_TYPE);

    mach_reset();
    st = KEXECS("pushi 1\npushi 0\nidiv");
    CHECK_ST(st, MENG_ERR_RUNTIME);

    KEXECS("pushi");
    {
        /* assemble-only error checks */
        meng_word_t words[8];
        uint32_t    cnt = 0u;

        st = meng_kernel_assemble((const uint8_t *)"pushi", 5u, words, 8u,
                                  &cnt);
        CHECK_ST(st, MENG_ERR_SYNTAX);
        st = meng_kernel_assemble((const uint8_t *)"jmp nowhere", 11u,
                                  words, 8u, &cnt);
        CHECK_ST(st, MENG_ERR_SYNTAX);
        st = meng_kernel_assemble((const uint8_t *)"bogus", 5u, words, 8u,
                                  &cnt);
        CHECK_ST(st, MENG_ERR_SYNTAX);
    }

    /* stack overflow */
    {
        char big[1024];
        size_t p = 0u;
        size_t k;
        memset(big, 0, sizeof(big));
        for (k = 0u; k < 70u; k++) {
            p += (size_t)sprintf(big + p, "pushi 1 dup2\n");
        }
        mach_reset();
        st = meng_kernel_exec(&mach, (const uint8_t *)big, p, NULL, 0u,
                              NULL);
        CHECK_ST(st, MENG_ERR_STACK_OVERFLOW);
    }

    /* watchdog */
    mach_reset();
    mach.step_budget = 200u;
    st = KEXECS("h0:\njmp h0\n");
    CHECK_ST(st, MENG_ERR_TRAP);

    /* ---- managednet engine dispatch --------------------------------- */
    fprintf(stderr, "[s5]\n");
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDNET,
               "pushi 16\npushi 4\nidiv\nprint\nhalt");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "4");

    /* same program through the poco engine */
    mach_reset();
    st = DISPS(MENG_KEY_POCO, "pushi 1\npushi 2\niadd\nprint\nhalt");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "3");

    /* ---- managedpy interpreter KATs --------------------------------- */
    fprintf(stderr, "[s5b]\n");

    /* assignment + arithmetic */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDPY, "x = 2 + 3\nprint x\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "5");

    /* integer division truncates */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDPY, "print 7/2\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "3");

    /* comparison yields 0/1 */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDPY, "print 4 >= 4\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "1");
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDPY, "print 2 < 1\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "0");

    /* unary minus */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDPY, "print -4 + 1\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "-3");

    /* string literal print */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDPY, "print \"hello\"\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "hello");

    /* while loop: 0 1 2 */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDPY,
               "i = 0\nwhile i < 3:\n    print i\n    i = i + 1\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "012");

    /* if/else */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDPY,
               "x = 5\nif x > 3:\n    print \"big\"\nelse:\n    print \"small\"\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "big");

    /* NameError -> RUNTIME */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDPY, "print zzz\n");
    CHECK_ST(st, MENG_ERR_RUNTIME);

    /* syntax error */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDPY, "x = \n");
    CHECK_ST(st, MENG_ERR_SYNTAX);

    /* empty program is a no-op */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDPY, "");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "");

    /* ---- managedjs interpreter KATs --------------------------------- */
    fprintf(stderr, "[s5c]\n");

    /* let assignment + print(...) */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDJS, "let x = 2 + 3;\nprint(x);\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "5");

    /* integer division + var declaration */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDJS, "var y = 7;\nprint(y / 2);\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "3");

    /* while loop with brace-block and plain re-assignment */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDJS,
               "var i = 0;\nwhile (i < 3) {\n    print(i);\n    i = i + 1;\n}\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "012");

    /* if/else */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDJS,
               "var x = 5;\nif (x > 3) {\n    print(\"big\");\n} else {\n    print(\"small\");\n}\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "big");

    /* // comment is ignored */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDJS,
               "let z = 1; // trailing comment\nprint(z);\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "1");

    /* comparison */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDJS, "print(2 < 1);\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "0");

    /* NameError -> RUNTIME */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDJS, "print(qqq);\n");
    CHECK_ST(st, MENG_ERR_RUNTIME);

    /* ---- managedlua interpreter KATs -------------------------------- */
    fprintf(stderr, "[s5d]\n");

    /* plain NAME = expr assignment (no keyword, Lua-style) */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDLUA, "x = 2 + 3\nprint(x)\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "5");

    /* integer division */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDLUA, "print(7 / 2)\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "3");

    /* while ... do ... end */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDLUA,
               "i = 0\nwhile i < 3 do\n    print(i)\n    i = i + 1\nend\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "012");

    /* if ... then ... else ... end */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDLUA,
               "x = 5\nif x > 3 then\n    print(\"big\")\nelse\n    print(\"small\")\nend\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "big");

    /* -- comment is ignored */
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDLUA,
               "z = 1 -- trailing comment\nprint(z)\n");
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "1");

    /* unregistered census key has no handler */
    mach_reset();
    st = meng_engine_dispatch(&mach, 0xDEADBEEFu, (const uint8_t *)"x", 1u);
    CHECK_ST(st, MENG_ERR_DEPENDENCY);

    /* ---- manageddl meta engine -------------------------------------- */
    fprintf(stderr, "[s6]\n");
    {
        /* 4-byte LE key = MENG_KEY_MANAGEDNET, then a bytecode program */
        uint8_t nested[64];
        size_t p = 0u;
        uint32_t nk = MENG_KEY_MANAGEDNET;
        const char *prog = "pushi 20\npushi 2\nimul\nprint\nhalt";

        nested[p++] = (uint8_t)(nk & 0xFFu);
        nested[p++] = (uint8_t)((nk >> 8) & 0xFFu);
        nested[p++] = (uint8_t)((nk >> 16) & 0xFFu);
        nested[p++] = (uint8_t)((nk >> 24) & 0xFFu);
        for (i = 0u; prog[i] != '\0'; i++) nested[p++] = (uint8_t)prog[i];
        mach_reset();
        st = meng_engine_dispatch(&mach, MENG_KEY_MANAGEDDL, nested, p);
        CHECK_ST(st, MENG_OK);
        EXPECT_OUT(&mach, "40");
    }

    /* nested load of an unserved language -> DEPENDENCY */
    {
        uint8_t nested[4];
        uint32_t nk = 0xDEADBEEFu;
        nested[0] = (uint8_t)(nk & 0xFFu);
        nested[1] = (uint8_t)((nk >> 8) & 0xFFu);
        nested[2] = (uint8_t)((nk >> 16) & 0xFFu);
        nested[3] = (uint8_t)((nk >> 24) & 0xFFu);
        mach_reset();
        st = meng_engine_dispatch(&mach, MENG_KEY_MANAGEDDL, nested, 4u);
        CHECK_ST(st, MENG_ERR_DEPENDENCY);
    }

    /* hot-register a third-party engine under a foreign census key */
    CHECK(meng_census_key("testlang92", &key) == MENG_OK);
    fake_engine.key = key;
    fake_engine.name = "testlang92";
    fake_engine.abi_minor = 1u;
    fake_engine.generation = 0u;
    fake_engine.run = fake_engine_run;
    CHECK(meng_engine_register(&fake_engine) == MENG_OK);
    CHECK(meng_engine_register(&fake_engine) == MENG_ERR_DUPLICATE);
    CHECK(meng_engine_count() == 9u);
    mach_reset();
    st = meng_engine_dispatch(&mach, key, (const uint8_t *)"x", 1u);
    CHECK_ST(st, MENG_OK);
    EXPECT_OUT(&mach, "TL");

    /* ---- managedcom -------------------------------------------------- */
    fprintf(stderr, "[s7]\n");
    {
        uint32_t gkey = (uint32_t)'g' | ((uint32_t)'e' << 8) |
                        ((uint32_t)'m' << 16) | ((uint32_t)'t' << 24);
        uint32_t iid = 0u;
        uint32_t id = 0u;
        int64_t  r = 0;
        int64_t  args[2];

        CHECK(meng_census_key("math", &iid) == MENG_OK);
        CHECK(meng_com_factory_register(gkey, com_factory, NULL) == MENG_OK);
        CHECK(meng_com_factory_register(gkey, com_factory, NULL) ==
              MENG_ERR_DUPLICATE);
        CHECK(meng_com_create(gkey, (const uint8_t *)"gemt", &id) == MENG_OK);
        CHECK(id != 0u);
        CHECK(meng_com_instance_count() == 1u);

        args[0] = 3; args[1] = 4;
        CHECK(meng_com_invoke(id, iid, 0u, args, 2u, &r) == MENG_OK);
        CHECK(r == 7);
        CHECK(meng_com_invoke(id, iid, 1u, args, 2u, &r) == MENG_OK);
        CHECK(r == -1);
        CHECK(meng_com_invoke(id, iid, 2u, args, 2u, &r) == MENG_OK);
        CHECK(r == 12);

        /* unknown method slot / interface */
        CHECK(meng_com_invoke(id, iid, 7u, args, 2u, &r) == MENG_ERR_BAD_ARG);
        CHECK(meng_com_invoke(id, 0x1234u, 0u, args, 2u, &r) ==
              MENG_ERR_NOT_FOUND);

        /* the managedcom engine runs a manifest against the registry */
        {
            const char *man =
                "create gemt\n"   /* current = gemt instance          */
                "resolv math\n"   /* current_iid = hash("math")        */
                "call 0 7 2\n"    /* add -> 9                          */
                "call 1 7 2\n"    /* sub -> 5                          */
                "call 2 7 2\n";   /* mul -> 14                         */
            mach_reset();
            st = meng_engine_dispatch(&mach, MENG_KEY_MANAGEDCOM,
                                      (const uint8_t *)man, strlen(man));
            CHECK_ST(st, MENG_OK);
        }

        /* rejected iid in manifest */
        {
            const char *man = "create gemt\nresolv nope\ncall 0 1 2\n";
            mach_reset();
            st = meng_engine_dispatch(&mach, MENG_KEY_MANAGEDCOM,
                                      (const uint8_t *)man, strlen(man));
            CHECK_ST(st, MENG_ERR_NOT_FOUND);
        }

        CHECK(meng_com_destroy(id) == MENG_OK);
        {
            uint32_t di;
            for (di = 1u; di <= MENG_COM_MAX_INSTANCES; di++) {
                if (di != id) meng_com_destroy(di);
            }
        }
        CHECK(meng_com_instance_count() == 0u);
    }

    /* ---- managedsh --------------------------------------------------- */
    fprintf(stderr, "[s8]\n");
    {
        const char *script =
            "# loop {0,1,2} then a string\n"
            "set i 0\n"
            "set msg hi\n"
            "top:\n"
            "echo $i\n"
            "inc i\n"
            "goto top IF $i lt 3\n"
            "echo $msg\n"
            "halt\n";
        mach_reset();
        st = meng_engine_dispatch(&mach, MENG_KEY_MANAGEDSH,
                                  (const uint8_t *)script, strlen(script));
        CHECK_ST(st, MENG_OK);
        EXPECT_OUT(&mach, "012hi");
    }

    /* unknown command */
    {
        const char *script = "set a 1\nboguscmd\n";
        mach_reset();
        st = meng_engine_dispatch(&mach, MENG_KEY_MANAGEDSH,
                                  (const uint8_t *)script, strlen(script));
        CHECK_ST(st, MENG_ERR_SYNTAX);
    }

    /* output cap: truncation is detected but the run still succeeds */
    {
        const char *script = "echo abcdef\n";
        mach_reset();
        mach.out_cap = 4u;
        st = meng_engine_dispatch(&mach, MENG_KEY_MANAGEDSH,
                                  (const uint8_t *)script, strlen(script));
        CHECK_ST(st, MENG_OK);
        CHECK(mach.out_len == 4u);
        CHECK(mach.out_total == 6u);
        CHECK((mach.flags & MENG_MFLAG_TRUNC) != 0u);
        CHECK(memcmp(outbuf, "abcd", 4u) == 0);
    }

    /* ---- machine / uninit paths -------------------------------------- */
    fprintf(stderr, "[s9]\n");
    CHECK(strcmp(meng_status_text(MENG_ERR_SYNTAX), "program parse failed")
          == 0);
    CHECK(strcmp(meng_status_text(MENG_OK), "OK") == 0);

    CHECK(meng_module_shutdown() == MENG_OK);
    mach_reset();
    st = DISPS(MENG_KEY_MANAGEDNET, "halt");
    CHECK_ST(st, MENG_ERR_UNINITIALIZED);
    CHECK(meng_module_shutdown() == MENG_ERR_UNINITIALIZED);

    /* re-init is clean */
    CHECK(meng_module_init() == MENG_OK);
    CHECK(meng_engine_count() == 8u);
    CHECK(meng_module_shutdown() == MENG_OK);

    printf("engines_smoke: %d passed, %d failed\n", g_passed, g_fails);
    fflush(stdout);
    return g_fails == 0 ? 0 : 1;
}