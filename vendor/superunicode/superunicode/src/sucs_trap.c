/**
 * Kernel Security Trap Damage Control Dispatch
 *
 * Fixed-size dispatch table for the 15 Kernel Security Trap slots
 * (0x7FFFFFF0-0x7FFFFFFE), each governing a cluster of 128 B+ BANcodes.
 *
 * Zero standard library dependencies; no dynamic allocation.
 */

#include "superunicode/sucs_trap.h"
#include "superunicode/sucs_plane.h"

static sucs_trap_handler_t g_handlers[SUCS_TRAP_SLOT_COUNT];
static void*               g_contexts[SUCS_TRAP_SLOT_COUNT];
static bool                g_installed[SUCS_TRAP_SLOT_COUNT];
static sucs_trap_diagnostic_t g_last_dispatch = { false, 0, 0, 0 };

static bool slot_valid(uint32_t slot) {
    return slot < (uint32_t)SUCS_TRAP_SLOT_COUNT;
}

bool sucs_trap_register_handler(uint32_t slot, sucs_trap_handler_t handler, void* context) {
    if (!slot_valid(slot) || handler == NULL) {
        return false;
    }
    g_handlers[slot]  = handler;
    g_contexts[slot]  = context;
    g_installed[slot] = true;
    return true;
}

bool sucs_trap_unregister_handler(uint32_t slot) {
    if (!slot_valid(slot) || !g_installed[slot]) {
        return false;
    }
    g_installed[slot]  = false;
    g_handlers[slot]   = NULL;
    g_contexts[slot]   = NULL;
    return true;
}

bool sucs_trap_handler_installed(uint32_t slot, void** out_context) {
    if (!slot_valid(slot)) {
        return false;
    }
    if (out_context) {
        *out_context = g_contexts[slot];
    }
    return g_installed[slot];
}

void sucs_trap_clear_all(void) {
    for (uint32_t i = 0; i < (uint32_t)SUCS_TRAP_SLOT_COUNT; i++) {
        g_installed[i] = false;
        g_handlers[i]  = NULL;
        g_contexts[i]  = NULL;
    }
    g_last_dispatch.fired = false;
}

bool sucs_trap_dispatch(sucs_char_t bancode_cp) {
    g_last_dispatch.fired = false;

    /* Only fatal B+ BANcodes route to Kernel Security Traps */
    if (!sucs_is_bancode(bancode_cp)) {
        return false;
    }

    uint32_t slot = (uint32_t)((bancode_cp - SUCS_BANCODE_RANGE_MIN) / SUCS_BANCODES_PER_TRAP);
    if (!slot_valid(slot)) {
        return false; /* slot 15 (0x0011A780-0x0011A7FF) has no trap handler */
    }
    if (!g_installed[slot]) {
        return false;
    }

    sucs_char_t trap_cp = (sucs_char_t)(SUCS_KERNEL_TRAP_MIN + slot);

    g_last_dispatch.fired      = true;
    g_last_dispatch.slot       = slot;
    g_last_dispatch.trap_cp    = trap_cp;
    g_last_dispatch.bancode_cp = bancode_cp;

    g_handlers[slot](trap_cp, bancode_cp, g_contexts[slot]);
    return true;
}

sucs_trap_diagnostic_t sucs_trap_last_dispatch(void) {
    return g_last_dispatch;
}
