/*
 * sucs_trap.h - Kernel Security Trap Damage Control Dispatch
 *
 * Self-contained freestanding compatibility layer. Mirrors the canonical
 * superunicode/superunicode/sucs_plane.h mapping helpers and
 * superunicode/sucs_trap.h dispatch semantics exactly.
 *
 * Each of the 15 Kernel Security Trap slots (0x7FFFFFF0-0x7FFFFFFE) governs
 * a cluster of 128 B+ BANcodes (0x0011A000-0x0011A77F). A handler registered
 * for a slot is invoked whenever a B+ BANcode in its cluster is dispatched,
 * enabling kernel crash damage control tailored to the BANcode registry
 * domain.
 *
 * The dispatch table is a fixed-size static table (zero dynamic allocation).
 * NOTE: being a self-contained header, the state is file-scope static and is
 * thus per-translation-unit. Link the canonical libsutf.a / .owd integration
 * (see vendor/superunicode) when share-across-TU dispatch state is required.
 *
 * C99 freestanding - stdint.h / stdbool.h / stddef.h only, zero allocation.
 */

#ifndef OWE_SUCS_TRAP_H
#define OWE_SUCS_TRAP_H

#include "sucs_types.h"

/* ------------------------------------------------------------------ */
/*  BANcode Registry Plugin Range (inside the System Control Plane)    */
/*  Guarded so identical constants can coexist with the canonical      */
/*  superunicode headers in a single translation unit.                 */
/* ------------------------------------------------------------------ */

#ifndef SUCS_SCP_MIN
#define SUCS_SCP_MIN                 0x00110000UL
#endif
#ifndef SUCS_SCP_MAX
#define SUCS_SCP_MAX                 0x0011FFFFUL
#endif
#ifndef SUCS_KERNEL_TRAP_MIN
#define SUCS_KERNEL_TRAP_MIN         0x7FFFFFF0UL
#endif
#ifndef SUCS_KERNEL_TRAP_MAX
#define SUCS_KERNEL_TRAP_MAX         0x7FFFFFFEUL
#endif
#ifndef SUCS_BANCODE_REGISTRY_MIN
#define SUCS_BANCODE_REGISTRY_MIN    0x0011A000UL
#endif
#ifndef SUCS_BANCODE_REGISTRY_MAX
#define SUCS_BANCODE_REGISTRY_MAX    0x0011AEFFUL
#endif
#ifndef SUCS_BANCODE_RANGE_MIN
#define SUCS_BANCODE_RANGE_MIN       0x0011A000UL
#endif
#ifndef SUCS_BANCODE_RANGE_MAX
#define SUCS_BANCODE_RANGE_MAX       0x0011A7FFUL
#endif
#ifndef SUCS_WARNCODE_RANGE_MIN
#define SUCS_WARNCODE_RANGE_MIN      0x0011A800UL
#endif
#ifndef SUCS_WARNCODE_RANGE_MAX
#define SUCS_WARNCODE_RANGE_MAX      0x0011ABFFUL
#endif
#ifndef SUCS_COMCODE_RANGE_MIN
#define SUCS_COMCODE_RANGE_MIN       0x0011AC00UL
#endif
#ifndef SUCS_COMCODE_RANGE_MAX
#define SUCS_COMCODE_RANGE_MAX       0x0011ADFFUL
#endif
#ifndef SUCS_SOFTCODE_RANGE_MIN
#define SUCS_SOFTCODE_RANGE_MIN      0x0011AE00UL
#endif
#ifndef SUCS_SOFTCODE_RANGE_MAX
#define SUCS_SOFTCODE_RANGE_MAX      0x0011AEFFUL
#endif
#ifndef SUCS_TRAP_SLOT_COUNT
#define SUCS_TRAP_SLOT_COUNT         15
#endif
#ifndef SUCS_BANCODES_PER_TRAP
#define SUCS_BANCODES_PER_TRAP       128
#endif

/* ------------------------------------------------------------------ */
/*  BANcode Registry Classification                                    */
/* ------------------------------------------------------------------ */

#ifndef SUCS_BANCODE_TYPE_T_DEFINED
#define SUCS_BANCODE_TYPE_T_DEFINED
typedef enum {
    SUCS_BANCODE_NONE  = 0, /* Not in the BANcode Registry                  */
    SUCS_BANCODE_FATAL = 1, /* B+ 0x0011A000-0x0011A7FF: unrecoverable       */
    SUCS_BANCODE_WARN  = 2, /* W+ 0x0011A800-0x0011ABFF: non-fatal telemetry */
    SUCS_BANCODE_COM   = 3, /* C+ 0x0011AC00-0x0011ADFF: IPC / handshakes    */
    SUCS_BANCODE_SOFT  = 4  /* S+ 0x0011AE00-0x0011AEFF: soft recovery       */
} sucs_bancode_type_t;
#endif

static inline bool sucs_is_scp_plane(sucs_char_t cp)
{
    return (cp >= SUCS_SCP_MIN && cp <= SUCS_SCP_MAX);
}

static inline bool sucs_is_kernel_trap(sucs_char_t cp)
{
    return (cp >= SUCS_KERNEL_TRAP_MIN && cp <= SUCS_KERNEL_TRAP_MAX);
}

static inline bool sucs_is_bancode_registry(sucs_char_t cp)
{
    return (cp >= SUCS_BANCODE_REGISTRY_MIN && cp <= SUCS_BANCODE_REGISTRY_MAX);
}

static inline bool sucs_is_bancode(sucs_char_t cp)
{
    return (cp >= SUCS_BANCODE_RANGE_MIN && cp <= SUCS_BANCODE_RANGE_MAX);
}

static inline bool sucs_is_warncode(sucs_char_t cp)
{
    return (cp >= SUCS_WARNCODE_RANGE_MIN && cp <= SUCS_WARNCODE_RANGE_MAX);
}

static inline bool sucs_is_comcode(sucs_char_t cp)
{
    return (cp >= SUCS_COMCODE_RANGE_MIN && cp <= SUCS_COMCODE_RANGE_MAX);
}

static inline bool sucs_is_softcode(sucs_char_t cp)
{
    return (cp >= SUCS_SOFTCODE_RANGE_MIN && cp <= SUCS_SOFTCODE_RANGE_MAX);
}

static inline sucs_bancode_type_t sucs_classify_bancode(sucs_char_t cp)
{
    if (cp >= SUCS_BANCODE_RANGE_MIN && cp <= SUCS_BANCODE_RANGE_MAX) {
        return SUCS_BANCODE_FATAL;
    } else if (cp >= SUCS_WARNCODE_RANGE_MIN && cp <= SUCS_WARNCODE_RANGE_MAX) {
        return SUCS_BANCODE_WARN;
    } else if (cp >= SUCS_COMCODE_RANGE_MIN && cp <= SUCS_COMCODE_RANGE_MAX) {
        return SUCS_BANCODE_COM;
    } else if (cp >= SUCS_SOFTCODE_RANGE_MIN && cp <= SUCS_SOFTCODE_RANGE_MAX) {
        return SUCS_BANCODE_SOFT;
    }
    return SUCS_BANCODE_NONE;
}

/* ------------------------------------------------------------------ */
/*  BANcode <-> Kernel Security Trap Mapping                           */
/* ------------------------------------------------------------------ */

/* Resolves the Kernel Security Trap codepoint governing a B+ BANcode for
 * damage-control dispatch. Returns SUCS_INVALID_CODEPOINT if the input is
 * not a B+ BANcode or falls beyond the 15 assigned trap slots. */
static inline sucs_char_t sucs_bancode_to_trap(sucs_char_t bancode_cp)
{
    if (!sucs_is_bancode(bancode_cp)) {
        return SUCS_INVALID_CODEPOINT;
    }
    uint32_t slot = (uint32_t)((bancode_cp - (sucs_char_t)SUCS_BANCODE_RANGE_MIN) /
                               (sucs_char_t)SUCS_BANCODES_PER_TRAP);
    if (slot >= SUCS_TRAP_SLOT_COUNT) {
        return SUCS_INVALID_CODEPOINT;
    }
    return (sucs_char_t)((sucs_char_t)SUCS_KERNEL_TRAP_MIN + (sucs_char_t)slot);
}

/* Returns the inclusive B+ BANcode cluster range governed by a Kernel
 * Security Trap handler. Returns false for non-trap codepoints or NULL
 * output pointers. */
static inline bool sucs_trap_to_bancode_range(
    sucs_char_t  trap_cp,
    sucs_char_t *out_min,
    sucs_char_t *out_max)
{
    if (!sucs_is_kernel_trap(trap_cp) || out_min == NULL || out_max == NULL) {
        return false;
    }
    uint32_t slot = (uint32_t)(trap_cp - (sucs_char_t)SUCS_KERNEL_TRAP_MIN);
    *out_min = (sucs_char_t)((sucs_char_t)SUCS_BANCODE_RANGE_MIN +
                             (sucs_char_t)(slot * (uint32_t)SUCS_BANCODES_PER_TRAP));
    *out_max = (sucs_char_t)(*out_min + (sucs_char_t)(SUCS_BANCODES_PER_TRAP - 1u));
    return true;
}

/* ------------------------------------------------------------------ */
/*  BANcode Operating Modes (BANcode v1.1.0)                            */
/*  System mode (default): fatal B+ BANcodes dispatch to the Kernel     */
/*  Security Trap handlers (sucs_trap_dispatch), the krnl path.         */
/*  App mode: fatal B+ BANcodes bypass Kernel Security Trap dispatch    */
/*  entirely and are delivered to the registered App-level crash        */
/*  handler. Both modes share the identical codepoint registry.         */
/*  Mirrors the canonical bancode_mode_t surface; guarded so the        */
/*  upstream superunicode/compat/BANcode constants coexist in one TU.   */
/* ------------------------------------------------------------------ */

#ifndef SUCS_BANCODE_MODE_T_DEFINED
#define SUCS_BANCODE_MODE_T_DEFINED
typedef enum {
    SUCS_BANCODE_MODE_SYSTEM = 0, /* Kernel mode: fatal BANcodes use krnl trap dispatch */
    SUCS_BANCODE_MODE_APP    = 1  /* App mode: fatal BANcodes use app-level crash handler */
} sucs_bancode_mode_t;
#endif

#ifndef SUCS_BANCODE_DEFAULT_MODE
#define SUCS_BANCODE_DEFAULT_MODE SUCS_BANCODE_MODE_SYSTEM
#endif

/* App-level crash handler signature (App mode path): a fatal B+ BANcode in
 * App mode is delivered here - never to the krnl trap table. Mirrors
 * bancode_app_crash_handler_t. */
typedef void (*sucs_bancode_app_crash_handler_t)(sucs_char_t bancode_cp, void *context);

/* ------------------------------------------------------------------ */
/*  Damage-Control Dispatch Table                                      */
/* ------------------------------------------------------------------ */

#if defined(__GNUC__) || defined(__clang__)
#define OWE_SUCS_UNUSED __attribute__((unused))
#else
#define OWE_SUCS_UNUSED
#endif

/* Handler signature:
 *    trap_cp    - Kernel Security Trap codepoint (0x7FFFFFF0+slot)
 *    bancode_cp - B+ BANcode that triggered the dispatch
 *    context    - caller-supplied context registered for the slot
 * Handlers run in the calling (crash) context and must be freestanding-safe:
 * zero libc dependencies, no allocation, no blocking. */
typedef void (*sucs_trap_handler_t)(sucs_char_t trap_cp, sucs_char_t bancode_cp,
                                    void *context);

/* Freestanding-readable diagnostic record of the most recent dispatch. */
typedef struct {
    bool        fired;        /* true if the last dispatch invoked a handler */
    uint32_t    slot;         /* trap slot index (0..14)                    */
    sucs_char_t trap_cp;      /* Kernel Security Trap codepoint             */
    sucs_char_t bancode_cp;   /* B+ BANcode that triggered the dispatch     */
} sucs_trap_diagnostic_t;

#define SUCS_TRAP_SLOTS_SUPPORTED  SUCS_TRAP_SLOT_COUNT

#ifndef SUCS_TRAP_TABLE_DEFINED
#define SUCS_TRAP_TABLE_DEFINED
static OWE_SUCS_UNUSED sucs_trap_handler_t  g_sucs_trap_handlers[SUCS_TRAP_SLOTS_SUPPORTED];
static OWE_SUCS_UNUSED void               *g_sucs_trap_contexts[SUCS_TRAP_SLOTS_SUPPORTED];
static OWE_SUCS_UNUSED bool                g_sucs_trap_installed[SUCS_TRAP_SLOTS_SUPPORTED];
static OWE_SUCS_UNUSED sucs_trap_diagnostic_t g_sucs_trap_diag;
static OWE_SUCS_UNUSED sucs_bancode_mode_t g_sucs_bancode_mode = SUCS_BANCODE_DEFAULT_MODE;
static OWE_SUCS_UNUSED sucs_bancode_app_crash_handler_t g_sucs_app_crash_handler;
static OWE_SUCS_UNUSED void *g_sucs_app_crash_context;
#endif

/* Returns the current BANcode operating mode. */
static inline sucs_bancode_mode_t sucs_bancode_get_mode(void)
{
    return g_sucs_bancode_mode;
}

/* Sets the BANcode operating mode at runtime. Returns true on success,
 * false if an invalid mode was specified. */
static inline bool sucs_bancode_set_mode(sucs_bancode_mode_t mode)
{
    if (mode != SUCS_BANCODE_MODE_SYSTEM && mode != SUCS_BANCODE_MODE_APP) {
        return false;
    }
    g_sucs_bancode_mode = mode;
    return true;
}

/* Returns true if the framework is in System mode (krnl trap dispatch). */
static inline bool sucs_bancode_is_system_mode(void)
{
    return g_sucs_bancode_mode == SUCS_BANCODE_MODE_SYSTEM;
}

/* Returns true if the framework is in App mode (app crash handler). */
static inline bool sucs_bancode_is_app_mode(void)
{
    return g_sucs_bancode_mode == SUCS_BANCODE_MODE_APP;
}

/* Registers (or replaces) the App-level crash handler used when in App mode.
 * Returns true on success. */
static inline bool sucs_bancode_register_app_crash_handler(
    sucs_bancode_app_crash_handler_t handler, void *context)
{
    if (handler == NULL) {
        return false;
    }
    g_sucs_app_crash_handler = handler;
    g_sucs_app_crash_context = context;
    return true;
}

/* Unregisters the App-level crash handler. Returns true if one was active. */
static inline bool sucs_bancode_unregister_app_crash_handler(void)
{
    if (g_sucs_app_crash_handler == NULL) {
        return false;
    }
    g_sucs_app_crash_handler = NULL;
    g_sucs_app_crash_context = NULL;
    return true;
}

/* Registers (or replaces) the damage-control handler for a trap slot.
 * slot must be 0..SUCS_TRAP_SLOT_COUNT-1 and handler must be non-NULL (use
 * sucs_trap_unregister_handler to clear a slot). Returns true on success. */
static inline bool sucs_trap_register_handler(uint32_t slot,
                                              sucs_trap_handler_t handler,
                                              void *context)
{
    if (slot >= SUCS_TRAP_SLOT_COUNT || handler == NULL) {
        return false;
    }
    g_sucs_trap_handlers[slot]  = handler;
    g_sucs_trap_contexts[slot]  = context;
    g_sucs_trap_installed[slot] = true;
    return true;
}

/* Unregisters the handler for a trap slot. Returns true if a handler was
 * actually removed. */
static inline bool sucs_trap_unregister_handler(uint32_t slot)
{
    if (slot >= SUCS_TRAP_SLOT_COUNT) {
        return false;
    }
    if (!g_sucs_trap_installed[slot]) {
        return false;
    }
    g_sucs_trap_handlers[slot]  = NULL;
    g_sucs_trap_contexts[slot]  = NULL;
    g_sucs_trap_installed[slot] = false;
    return true;
}

/* Returns true if a handler is installed for the slot; optionally returns the
 * registered context via out_context (may be NULL). */
static inline bool sucs_trap_handler_installed(uint32_t slot, void **out_context)
{
    if (slot >= SUCS_TRAP_SLOT_COUNT) {
        return false;
    }
    if (out_context != NULL) {
        *out_context = g_sucs_trap_contexts[slot];
    }
    return g_sucs_trap_installed[slot];
}

/* Removes every registered handler (used for shutdown / recovery). */
static inline void sucs_trap_clear_all(void)
{
    for (uint32_t i = 0u; i < SUCS_TRAP_SLOT_COUNT; i++) {
        g_sucs_trap_handlers[i]  = NULL;
        g_sucs_trap_contexts[i]  = NULL;
        g_sucs_trap_installed[i] = false;
    }
}

/* Dispatches a B+ BANcode to the handler governing its cluster. Returns true
 * if a handler was installed and invoked. Non-fatal BANcodes, unmapped
 * BANcodes (0x0011A780-0x0011A7FF), and slots without a handler return false.
 *
 * Dispatch routing depends on the operating mode (BANcode v1.1.0):
 *  - System mode (default): routes to the Kernel Security Trap handler for
 *    the slot, exactly as described above.
 *  - App mode: fatal BANcodes bypass the krnl trap table and are delivered to
 *    the registered App-level crash handler. A true return means the app
 *    crash handler was registered and invoked. */
static inline bool sucs_trap_dispatch(sucs_char_t bancode_cp)
{
    if (g_sucs_bancode_mode == SUCS_BANCODE_MODE_APP &&
        sucs_is_bancode(bancode_cp)) {
        if (g_sucs_app_crash_handler != NULL) {
            g_sucs_app_crash_handler(bancode_cp, g_sucs_app_crash_context);
            return true;
        }
        return false;
    }
    sucs_char_t trap_cp = sucs_bancode_to_trap(bancode_cp);
    if (trap_cp == SUCS_INVALID_CODEPOINT) {
        g_sucs_trap_diag.fired      = false;
        g_sucs_trap_diag.slot       = 0u;
        g_sucs_trap_diag.trap_cp    = SUCS_INVALID_CODEPOINT;
        g_sucs_trap_diag.bancode_cp = bancode_cp;
        return false;
    }
    uint32_t slot = (uint32_t)(trap_cp - (sucs_char_t)SUCS_KERNEL_TRAP_MIN);
    if (!g_sucs_trap_installed[slot]) {
        g_sucs_trap_diag.fired      = false;
        g_sucs_trap_diag.slot       = slot;
        g_sucs_trap_diag.trap_cp    = trap_cp;
        g_sucs_trap_diag.bancode_cp = bancode_cp;
        return false;
    }
    g_sucs_trap_diag.fired      = true;
    g_sucs_trap_diag.slot       = slot;
    g_sucs_trap_diag.trap_cp    = trap_cp;
    g_sucs_trap_diag.bancode_cp = bancode_cp;
    g_sucs_trap_handlers[slot](trap_cp, bancode_cp, g_sucs_trap_contexts[slot]);
    return true;
}

/* Returns the diagnostic record of the most recent dispatch attempt. */
static inline sucs_trap_diagnostic_t sucs_trap_last_dispatch(void)
{
    return g_sucs_trap_diag;
}

/* Initialize the dispatch table (call once during early boot before any
 * handlers are registered). */
static inline void sucs_trap_init(void)
{
    g_sucs_trap_diag.fired      = false;
    g_sucs_trap_diag.slot       = 0u;
    g_sucs_trap_diag.trap_cp    = SUCS_INVALID_CODEPOINT;
    g_sucs_trap_diag.bancode_cp = SUCS_INVALID_CODEPOINT;
    g_sucs_bancode_mode         = SUCS_BANCODE_DEFAULT_MODE;
    g_sucs_app_crash_handler    = NULL;
    g_sucs_app_crash_context    = NULL;
    sucs_trap_clear_all();
}

#endif /* OWE_SUCS_TRAP_H */