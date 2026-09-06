/*
 * owrp.c - OpenWindows Ring Portal implementation
 *
 * C99 freestanding, zero heap allocation.
 * x86_64 syscall gate library.
 */

#include "owrp.h"

/* ------------------------------------------------------------------ */
/*  Compiler-portable inline asm macro                                 */
/* ------------------------------------------------------------------ */

#if defined(__GNUC__) || defined(__clang__)
#define OWRP_ASM_VOL    __asm__ volatile

/* Local register variables (used to pin syscall args to the fixed
 * x86_64 ABI registers r10/r8/r9) are a GNU extension. Silence the
 * pedantic diagnostic around the inline-asm blocks that use them. */
#define OWRP_ASM_PEDANTIC_PUSH                                              \
    _Pragma("GCC diagnostic push")                                          \
    _Pragma("GCC diagnostic ignored \"-Wpedantic\"")
#define OWRP_ASM_PEDANTIC_POP  _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define OWRP_ASM_VOL    __asm
#define OWRP_ASM_PEDANTIC_PUSH
#define OWRP_ASM_PEDANTIC_POP
#else
#define OWRP_ASM_VOL    /* unsupported compiler */
#define OWRP_ASM_PEDANTIC_PUSH
#define OWRP_ASM_PEDANTIC_POP
#endif

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static void owrp_log_call(
    owrp_context_t *ctx,
    uint32_t        syscall_nr,
    int64_t         result_val,
    int64_t         result_err
) {
    uint32_t idx = ctx->log_head;
    ctx->call_log[idx].syscall_nr = syscall_nr;
    ctx->call_log[idx].result     = result_val;
    ctx->call_log[idx].error      = result_err;
    ctx->call_log[idx].timestamp  = 0; /* no hosted clock available */

    ctx->log_head = (ctx->log_head + 1) % OWRP_CALL_LOG_SIZE;
    if (ctx->log_count < OWRP_CALL_LOG_SIZE) {
        ctx->log_count++;
    }
}

/* ------------------------------------------------------------------ */
/*  owrp_init                                                         */
/* ------------------------------------------------------------------ */

owrp_status_t owrp_init(owrp_context_t *ctx) {
    if (!ctx) return OWRP_ERR_INVALID_ARG;

    /* Zero the entire context with volatile writes */
    volatile uint8_t *dst = (volatile uint8_t *)ctx;
    for (size_t i = 0; i < sizeof(owrp_context_t); i++) {
        dst[i] = 0;
    }

    ctx->initialized  = true;
    ctx->current_ring = 3;
    ctx->boot_phase   = 0;

    return OWRP_OK;
}

/* ------------------------------------------------------------------ */
/*  owrp_invoke_gate                                                  */
/* ------------------------------------------------------------------ */

owrp_gate_result_t owrp_invoke_gate(
    owrp_context_t *ctx,
    uint64_t        sys_nr,
    uint64_t        arg1,
    uint64_t        arg2,
    uint64_t        arg3
) {
    owrp_gate_result_t res;
    res.value = 0;
    res.error = 0;
    res.r8    = 0;
    res.r9    = 0;

    /* Validate context */
    if (!ctx || !ctx->initialized) {
        if (ctx) ctx->total_errors++;
        res.error = OWRP_ERR_UNMAPPED;
        return res;
    }

    /* Validate syscall is registered */
    const owrp_gate_entry_t *gate = owrp_find_gate(ctx, (uint32_t)sys_nr);
    if (!gate) {
        ctx->total_errors++;
        res.error = OWRP_ERR_INVALID_SYSCALL;
        return res;
    }

    /* Permission check: ring 3 cannot call KERNEL-flagged gates */
    if (ctx->current_ring != 0 &&
        (gate->flags & OWRP_GATE_FLAG_KERNEL) != 0) {
        ctx->total_privilege_denials++;
        res.error = OWRP_ERR_PRIVILEGE;
        return res;
    }

    /* Boot phase check */
    if (ctx->boot_phase && !(gate->flags & OWRP_GATE_FLAG_BOOT)) {
        ctx->total_privilege_denials++;
        res.error = OWRP_ERR_PRIVILEGE;
        return res;
    }

    /* Execute syscall */
    OWRP_ASM_PEDANTIC_PUSH
    register uint64_t rax __asm__("rax") = sys_nr;
    register uint64_t rdi __asm__("rdi") = arg1;
    register uint64_t rsi __asm__("rsi") = arg2;
    register uint64_t rdx __asm__("rdx") = arg3;
    register uint64_t ret_rax __asm__("rax");
    register uint64_t ret_rdx __asm__("rdx");
    OWRP_ASM_VOL("syscall"
        : "=r"(ret_rax), "=r"(ret_rdx)
        : "r"(rax), "r"(rdi), "r"(rsi), "r"(rdx)
        : "rcx", "r11", "memory"
    );
    OWRP_ASM_PEDANTIC_POP

    res.value = (int64_t)ret_rax;
    res.error = (int64_t)ret_rdx;

    /* Log and update stats */
    owrp_log_call(ctx, (uint32_t)sys_nr, res.value, res.error);

    if (res.error != 0) {
        ctx->total_errors++;
    } else {
        ctx->total_calls++;
    }

    return res;
}

/* ------------------------------------------------------------------ */
/*  owrp_invoke_gate_full                                             */
/* ------------------------------------------------------------------ */

owrp_gate_result_t owrp_invoke_gate_full(
    owrp_context_t *ctx,
    uint64_t        sys_nr,
    uint64_t        arg1,
    uint64_t        arg2,
    uint64_t        arg3,
    uint64_t        arg4,
    uint64_t        arg5,
    uint64_t        arg6
) {
    owrp_gate_result_t res;
    res.value = 0;
    res.error = 0;
    res.r8    = 0;
    res.r9    = 0;

    /* Validate context */
    if (!ctx || !ctx->initialized) {
        if (ctx) ctx->total_errors++;
        res.error = OWRP_ERR_UNMAPPED;
        return res;
    }

    /* Validate syscall is registered */
    const owrp_gate_entry_t *gate = owrp_find_gate(ctx, (uint32_t)sys_nr);
    if (!gate) {
        ctx->total_errors++;
        res.error = OWRP_ERR_INVALID_SYSCALL;
        return res;
    }

    /* Permission check */
    if (ctx->current_ring != 0 &&
        (gate->flags & OWRP_GATE_FLAG_KERNEL) != 0) {
        ctx->total_privilege_denials++;
        res.error = OWRP_ERR_PRIVILEGE;
        return res;
    }

    /* Boot phase check */
    if (ctx->boot_phase && !(gate->flags & OWRP_GATE_FLAG_BOOT)) {
        ctx->total_privilege_denials++;
        res.error = OWRP_ERR_PRIVILEGE;
        return res;
    }

    /* Execute syscall with all 6 args */
    OWRP_ASM_PEDANTIC_PUSH
    register uint64_t rax __asm__("rax") = sys_nr;
    register uint64_t rdi __asm__("rdi") = arg1;
    register uint64_t rsi __asm__("rsi") = arg2;
    register uint64_t rdx __asm__("rdx") = arg3;
    register uint64_t r10 __asm__("r10") = arg4;
    register uint64_t r8  __asm__("r8")  = arg5;
    register uint64_t r9  __asm__("r9")  = arg6;
    register uint64_t ret_rax __asm__("rax");
    register uint64_t ret_rdx __asm__("rdx");
    OWRP_ASM_VOL("syscall"
        : "=r"(ret_rax), "=r"(ret_rdx)
        : "r"(rax), "r"(rdi), "r"(rsi), "r"(rdx),
          "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    OWRP_ASM_PEDANTIC_POP

    res.value = (int64_t)ret_rax;
    res.error = (int64_t)ret_rdx;

    /* Log and update stats */
    owrp_log_call(ctx, (uint32_t)sys_nr, res.value, res.error);

    if (res.error != 0) {
        ctx->total_errors++;
    } else {
        ctx->total_calls++;
    }

    return res;
}

/* ------------------------------------------------------------------ */
/*  owrp_register_gate                                                */
/* ------------------------------------------------------------------ */

owrp_status_t owrp_register_gate(
    owrp_context_t *ctx,
    uint32_t        syscall_nr,
    const char     *name,
    uint8_t         param_count,
    uint16_t        required_ring,
    uint8_t         flags
) {
    if (!ctx || !ctx->initialized) return OWRP_ERR_INVALID_ARG;
    if (!name) return OWRP_ERR_INVALID_ARG;

    /* Check for existing entry with same syscall_nr (update) */
    int32_t slot = -1;
    for (uint32_t i = 0; i < ctx->gate_count; i++) {
        if (ctx->gates[i].syscall_nr == syscall_nr &&
            (ctx->gates[i].flags & OWRP_GATE_FLAG_ACTIVE)) {
            slot = (int32_t)i;
            break;
        }
    }

    /* If no existing entry, allocate next slot */
    if (slot < 0) {
        if (ctx->gate_count >= OWRP_MAX_GATES) {
            return OWRP_ERR_INVALID_ARG;
        }
        slot = (int32_t)ctx->gate_count;
        ctx->gate_count++;
    }

    owrp_gate_entry_t *e = &ctx->gates[slot];
    e->syscall_nr    = syscall_nr;
    e->param_count   = param_count;
    e->required_ring = required_ring;
    e->flags         = flags | OWRP_GATE_FLAG_ACTIVE;

    /* Copy name bounded to OWRP_GATE_NAME_MAX - 1 */
    uint32_t i = 0;
    while (name[i] && i < OWRP_GATE_NAME_MAX - 1) {
        e->name[i] = name[i];
        i++;
    }
    e->name[i] = '\0';

    return OWRP_OK;
}

/* ------------------------------------------------------------------ */
/*  owrp_find_gate                                                    */
/* ------------------------------------------------------------------ */

const owrp_gate_entry_t *owrp_find_gate(
    const owrp_context_t *ctx,
    uint32_t              syscall_nr
) {
    if (!ctx || !ctx->initialized) return NULL;

    for (uint32_t i = 0; i < ctx->gate_count; i++) {
        if (ctx->gates[i].syscall_nr == syscall_nr &&
            (ctx->gates[i].flags & OWRP_GATE_FLAG_ACTIVE)) {
            return &ctx->gates[i];
        }
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/*  owrp_set_ring                                                     */
/* ------------------------------------------------------------------ */

owrp_status_t owrp_set_ring(owrp_context_t *ctx, uint8_t ring) {
    if (!ctx || !ctx->initialized) return OWRP_ERR_INVALID_ARG;

    if (ctx->current_ring != 0 && ring != ctx->current_ring) {
        return OWRP_ERR_PRIVILEGE;
    }

    ctx->current_ring = ring;
    return OWRP_OK;
}

/* ------------------------------------------------------------------ */
/*  owrp_enter / owrp_exit boot phase                                 */
/* ------------------------------------------------------------------ */

void owrp_enter_boot_phase(owrp_context_t *ctx) {
    if (ctx) ctx->boot_phase = 1;
}

void owrp_exit_boot_phase(owrp_context_t *ctx) {
    if (ctx) ctx->boot_phase = 0;
}

/* ------------------------------------------------------------------ */
/*  owrp_get_call_log                                                 */
/* ------------------------------------------------------------------ */

uint32_t owrp_get_call_log(
    const owrp_context_t *ctx,
    uint32_t             *out_syscall_nrs,
    int64_t              *out_results,
    uint32_t              max_entries
) {
    if (!ctx || !out_syscall_nrs || !out_results) return 0;

    uint32_t count = ctx->log_count;
    if (count > max_entries) count = max_entries;
    if (count > OWRP_CALL_LOG_SIZE) count = OWRP_CALL_LOG_SIZE;

    /* Copy most recent first */
    for (uint32_t i = 0; i < count; i++) {
        /* Most recent entry is at log_head - 1 (wrapping) */
        uint32_t src_idx = (ctx->log_head + OWRP_CALL_LOG_SIZE - 1 - i)
                           % OWRP_CALL_LOG_SIZE;
        out_syscall_nrs[i] = ctx->call_log[src_idx].syscall_nr;
        out_results[i]     = ctx->call_log[src_idx].result;
    }

    return count;
}

/* ------------------------------------------------------------------ */
/*  owrp_get_stats                                                    */
/* ------------------------------------------------------------------ */

void owrp_get_stats(
    const owrp_context_t *ctx,
    uint64_t *total_calls,
    uint64_t *total_errors,
    uint64_t *total_privilege_denials
) {
    if (!ctx) return;
    if (total_calls)            *total_calls            = ctx->total_calls;
    if (total_errors)           *total_errors           = ctx->total_errors;
    if (total_privilege_denials) *total_privilege_denials = ctx->total_privilege_denials;
}

/* ------------------------------------------------------------------ */
/*  owrp_reset_stats                                                  */
/* ------------------------------------------------------------------ */

void owrp_reset_stats(owrp_context_t *ctx) {
    if (!ctx) return;

    ctx->total_calls            = 0;
    ctx->total_errors           = 0;
    ctx->total_privilege_denials = 0;
    ctx->log_head  = 0;
    ctx->log_count = 0;

    /* Zero call_log with volatile writes */
    volatile uint8_t *dst = (volatile uint8_t *)ctx->call_log;
    size_t sz = sizeof(ctx->call_log);
    for (size_t i = 0; i < sz; i++) {
        dst[i] = 0;
    }
}
