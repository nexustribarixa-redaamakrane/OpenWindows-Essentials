/**
 * Ecosystem Compatibility Test: BANcode <-> SuperUnicode
 *
 * Single translation unit co-including:
 *   - sutf's sucs_types.h / sutf8.h              (libsutf transport layer)
 *   - superunicode's sucs_types.h / sucs_plane.h (BANcode registry & traps)
 *   - BANcode framework <bancode/bancode_all.h>
 *
 * Verifies that the BANcode Registry Plugin Range, the Kernel Security Trap
 * damage-control dispatch geometry, and the live handler dispatch table are
 * bit-exact across both frameworks. The guarded SUCS constants and the
 * bancode_t/sucs_char_t typedefs must coexist in one TU.
 *
 * Build mode: links the REAL BANcode kernel sources when ../BANcode is
 * checked out next to this workspace, otherwise the vendored port under
 * compat/BANcode/. COMPAT_BANCODE_REAL is defined by CMake accordingly.
 */

#include <stdio.h>
#include <assert.h>
#include "sutf8.h"                      /* libsutf (brings sutf/sucs_types.h) */
#include "superunicode/sucs_plane.h"    /* upstream module (brings sucs_types.h) */
#include "bancode/bancode_all.h"        /* BANcode framework */

static int g_checks;

#define CHECK(cond) \
    do { ++g_checks; assert(cond); } while (0)

/* Live damage-control dispatch handler (C99: no closures, use file-scope
 * statics to observe the callback). */
static int       g_fired;
static bancode_t g_fired_trap;
static bancode_t g_fired_code;

/* App-level crash handler observation (App-mode dispatch tests). */
static bancode_t g_app_crash_code;

static void test_dispatch_handler(bancode_t trap_cp, bancode_t bancode_cp, void* ctx) {
    (void)ctx;
    g_fired = 1;
    g_fired_trap = trap_cp;
    g_fired_code = bancode_cp;
}

static void app_crash_handler(bancode_t bancode_cp, void* context) {
    int* fired = (int*)context;
    if (fired) {
        *fired = 1;
    }
    g_app_crash_code = bancode_cp;
}

int main(void) {

#ifdef COMPAT_BANCODE_REAL
    printf("compat_bancode: using REAL ../BANcode kernel sources\n");
#else
    printf("compat_bancode: using vendored compat/BANcode port\n");
#endif

    /* --- 1. Kernel Security Trap range & sentinel parity ----------------- */
    CHECK(BANCODE_KERNEL_TRAP_MIN == 0x7FFFFFF0UL);
    CHECK(BANCODE_KERNEL_TRAP_MAX == 0x7FFFFFFEUL);
    CHECK(BANCODE_INVALID_CODEPOINT == 0x7FFFFFFFUL);
    CHECK(BANCODE_KERNEL_TRAP_MIN == SUCS_KERNEL_TRAP_MIN);
    CHECK(BANCODE_KERNEL_TRAP_MAX == SUCS_KERNEL_TRAP_MAX);
    CHECK(BANCODE_INVALID_CODEPOINT == SUCS_INVALID_CODEPOINT);
    CHECK(SUCS_TRAP_RANGE_MIN == SUCS_KERNEL_TRAP_MIN);
    CHECK(SUCS_TRAP_RANGE_MAX == SUCS_KERNEL_TRAP_MAX);

    /* --- 2. Dispatch geometry parity ------------------------------------- */
    CHECK(BANCODE_TRAP_SLOT_COUNT == SUCS_TRAP_SLOT_COUNT);
    CHECK(BANCODE_BANCODES_PER_TRAP == SUCS_BANCODES_PER_TRAP);
    CHECK(SUCS_TRAP_SLOT_COUNT == 15);
    CHECK(SUCS_BANCODES_PER_TRAP == 128);

    /* --- 3. Registry block boundary parity ------------------------------- */
    CHECK(BANCODE_BANCODE_START  == SUCS_BANCODE_RANGE_MIN);
    CHECK(BANCODE_BANCODE_END    == SUCS_BANCODE_RANGE_MAX);
    CHECK(BANCODE_WARNCODE_START == SUCS_WARNCODE_RANGE_MIN);
    CHECK(BANCODE_WARNCODE_END   == SUCS_WARNCODE_RANGE_MAX);
    CHECK(BANCODE_COMCODE_START  == SUCS_COMCODE_RANGE_MIN);
    CHECK(BANCODE_COMCODE_END    == SUCS_COMCODE_RANGE_MAX);
    CHECK(BANCODE_SOFTCODE_START == SUCS_SOFTCODE_RANGE_MIN);
    CHECK(BANCODE_SOFTCODE_END   == SUCS_SOFTCODE_RANGE_MAX);

    CHECK(SUCS_BANCODE_REGISTRY_MIN == BANCODE_BANCODE_START);
    CHECK(SUCS_BANCODE_REGISTRY_MAX == BANCODE_SOFTCODE_END);

    /* --- 4. Forward mapping parity: sucs_bancode_to_trap == bancode_to_trap
     *     over every trap-cluster boundary and edge sample --------------- */
    {
        static const uint32_t samples[] = {
            0x0011A000UL, 0x0011A001UL, 0x0011A07FUL,          /* slot 0 edges */
            0x0011A080UL, 0x0011A0FFUL,                        /* slot 1       */
            0x0011A100UL,                                      /* slot 2       */
            0x0011A180UL,                                      /* slot 3       */
            0x0011A3E6UL,                                      /* VIP BAN_ENTRY_CORRUPT neighborhood */
            0x0011A77FUL,                                      /* last mapped codepoint */
            0x0011A780UL, 0x0011A7FFUL,                        /* unmapped slot 15 */
            0x0011AB00UL, 0x0011AD00UL,                        /* W+ / C+       */
            0x00000000UL, 0x7FFFFFFFUL                         /* non-BANcode   */
        };
        size_t i;

        for (i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
            sucs_char_t t_sucs = sucs_bancode_to_trap(samples[i]);
            bancode_t   t_ban  = bancode_to_trap(samples[i]);
            CHECK(t_sucs == t_ban);
        }

        /* Explicit expected resolutions */
        CHECK(sucs_bancode_to_trap(0x0011A000UL) == 0x7FFFFFF0UL);
        CHECK(sucs_bancode_to_trap(0x0011A07FUL) == 0x7FFFFFF0UL);
        CHECK(sucs_bancode_to_trap(0x0011A080UL) == 0x7FFFFFF1UL);
        CHECK(sucs_bancode_to_trap(0x0011A3E6UL) == 0x7FFFFFF7UL);
        CHECK(sucs_bancode_to_trap(0x0011A77FUL) == 0x7FFFFFFEUL);
        CHECK(sucs_bancode_to_trap(0x0011A780UL) == SUCS_INVALID_CODEPOINT);
        CHECK(bancode_to_trap(0x0011A780UL)      == BANCODE_INVALID_CODEPOINT);
    }

    /* --- 5. Reverse mapping parity: trap -> B+ cluster range ------------- */
    {
        uint32_t slot;
        for (slot = 0; slot < 15U; ++slot) {
            bancode_t trap_cp = (bancode_t)(BANCODE_KERNEL_TRAP_MIN + slot);
            bancode_t b_min = 0, b_max = 0;
            sucs_char_t s_min = 0, s_max = 0;
            bool ok_ban = bancode_trap_to_bancode_range(trap_cp, &b_min, &b_max);
            bool ok_suc = sucs_trap_to_bancode_range(trap_cp, &s_min, &s_max);
            CHECK(ok_ban);
            CHECK(ok_suc);
            CHECK((uint32_t)s_min == (uint32_t)b_min);
            CHECK((uint32_t)s_max == (uint32_t)b_max);
            CHECK(b_min == (bancode_t)(BANCODE_BANCODE_START + slot * 128U));
            CHECK(b_max == (bancode_t)(b_min + 127U));
        }
        {
            bancode_t b_min = 0, b_max = 0;
            sucs_char_t s_min = 0, s_max = 0;
            CHECK(!bancode_trap_to_bancode_range(0x7FFFFFFFUL, &b_min, &b_max));
            CHECK(!sucs_trap_to_bancode_range(SUCS_INVALID_CODEPOINT, &s_min, &s_max));
        }
    }

    /* --- 6. Classification parity ----------------------------------------- */
    {
        static const uint32_t fatal_samples[] = {
            0x0011A000UL, 0x0011A400UL, 0x0011A7FFUL
        };
        size_t i;
        for (i = 0; i < sizeof(fatal_samples) / sizeof(fatal_samples[0]); ++i) {
            CHECK(bancode_is_bancode(fatal_samples[i]));
            CHECK(sucs_is_bancode(fatal_samples[i]));
            CHECK(sucs_classify_bancode(fatal_samples[i]) == SUCS_BANCODE_FATAL);
        }
        CHECK(sucs_classify_bancode(0x0011AC00UL) == SUCS_BANCODE_COM);
        CHECK(sucs_classify_bancode(0x0011AEFFUL) == SUCS_BANCODE_SOFT);
        CHECK(sucs_classify_bancode(0x0011AF00UL) == SUCS_BANCODE_NONE);
        CHECK(!sucs_is_bancode_registry(0x0011AF00UL));
    }

    /* --- 7. Validator agreement over the trap range ----------------------- */
    {
        uint32_t cp;
        for (cp = 0x7FFFFFEFUL; cp <= 0x7FFFFFFFUL; ++cp) {
            if (bancode_is_kernel_trap(cp)) {
                CHECK(!sucs_is_valid(cp));
                CHECK(sucs_classify_codepoint(cp) == SUCS_TYPE_INVALID);
            }
        }
        CHECK(sucs_is_valid(0x7FFFFFEFUL));
        CHECK(!sucs_is_valid(0x7FFFFFF0UL));
    }

    /* --- 8. Live damage-control dispatch through BANcode's table ---------- */
    bancode_trap_diagnostic_t diag;
    uint32_t slot;

    g_fired = 0;
    bancode_trap_clear_all();

    /* Resolve the governing slot via SuperUnicode, register through BANcode:
     * proves both sides agree on slot geometry at runtime. */
    {
        sucs_char_t trap_cp = sucs_bancode_to_trap(0x0011A042UL);
        CHECK(trap_cp == 0x7FFFFFF0UL);
        slot = (uint32_t)(trap_cp - SUCS_KERNEL_TRAP_MIN);
        CHECK(slot == 0U);
    }

    CHECK(bancode_trap_register_handler(slot, test_dispatch_handler, NULL));

    CHECK(bancode_trap_dispatch(0x0011A042UL));
    CHECK(g_fired == 1);
    CHECK(g_fired_trap == 0x7FFFFFF0UL);
    CHECK(g_fired_code == 0x0011A042UL);

    diag = bancode_trap_last_dispatch();
    CHECK(diag.fired);
    CHECK(diag.slot == 0U);
    CHECK(diag.trap_cp == (bancode_t)sucs_bancode_to_trap(diag.bancode_cp));

    /* Unmapped cluster must not fire any handler */
    CHECK(!bancode_trap_dispatch(0x0011A7FFUL));
    /* Non-fatal codes must not dispatch */
    CHECK(!bancode_trap_dispatch(0x0011A800UL)); /* W+ WARNcode */

    CHECK(bancode_trap_unregister_handler(slot));
    CHECK(!bancode_trap_handler_installed(slot, NULL));
    bancode_trap_clear_all();

    /* --- 9. BANcode System / App mode selection ---------------------------- */
    /* Both modes share the identical codepoint registry and trap geometry; the
     * mode only alters how fatal B+ BANcodes are handled at dispatch time. */
    {
        bancode_mode_t mode;

        /* Invalid mode rejected */
        CHECK(!bancode_set_mode((bancode_mode_t)99));

        /* Reset to a known state */
        CHECK(bancode_set_mode(BANCODE_MODE_SYSTEM));
        CHECK(bancode_get_mode() == BANCODE_MODE_SYSTEM);
        CHECK(bancode_is_system_mode());
        CHECK(!bancode_is_app_mode());

        CHECK(bancode_set_mode(BANCODE_MODE_APP));
        CHECK(bancode_get_mode() == BANCODE_MODE_APP);
        CHECK(bancode_is_app_mode());
        CHECK(!bancode_is_system_mode());

        /* Mode does not change registry boundaries or trap geometry */
        mode = bancode_get_mode();
        (void)mode;
        CHECK(BANCODE_BANCODE_START == SUCS_BANCODE_RANGE_MIN);
        CHECK(bancode_to_trap(0x0011A000UL) == 0x7FFFFFF0UL);
        CHECK(sucs_bancode_to_trap(0x0011A3E6UL) == 0x7FFFFFF7UL);
        CHECK(bancode_is_bancode(0x0011A042UL));
        CHECK(sucs_classify_bancode(0x0011A042UL) == SUCS_BANCODE_FATAL);
    }

    /* --- 10. App mode: fatal BANcodes bypass krnl trap dispatch ------------- */
    {
        int app_fired = 0;

        /* Register an App-level crash handler */
        CHECK(bancode_register_app_crash_handler(app_crash_handler, &app_fired));
        CHECK(bancode_app_crash_handler_installed(NULL));

        /* In App mode, a fatal BANcode MUST NOT touch the krnl trap table.
         * Even with a krnl handler installed for the governing slot, the
         * dispatch must route to the App crash handler instead. */
        {
            sucs_char_t trap_cp = sucs_bancode_to_trap(0x0011A042UL);
            uint32_t s = (uint32_t)(trap_cp - SUCS_KERNEL_TRAP_MIN);

            g_fired = 0;
            CHECK(bancode_trap_register_handler(s, test_dispatch_handler, NULL));

            app_fired = 0;
            CHECK(bancode_set_mode(BANCODE_MODE_APP));

            /* Dispatch must return true (a handler was invoked) but it must be
             * the App handler, NOT the krnl trap handler. */
            CHECK(bancode_trap_dispatch(0x0011A042UL));
            CHECK(app_fired == 1);
            CHECK(g_app_crash_code == 0x0011A042UL);
            CHECK(g_fired == 0);  /* krnl trap handler NOT invoked */

            /* Without an App handler installed, App-mode dispatch returns false
             * and never reaches the krnl table. */
            CHECK(bancode_unregister_app_crash_handler());
            app_fired = 0;
            CHECK(!bancode_trap_dispatch(0x0011A042UL));
            CHECK(app_fired == 0);
            CHECK(g_fired == 0);

            bancode_trap_unregister_handler(s);
        }

        /* Switch back to System mode for the remaining checks */
        CHECK(bancode_set_mode(BANCODE_MODE_SYSTEM));
        CHECK(!bancode_app_crash_handler_installed(NULL));
    }

    printf("compat_bancode: %d checks passed\n", g_checks);
    return 0;
}
