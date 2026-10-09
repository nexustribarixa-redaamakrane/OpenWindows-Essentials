#ifndef SUPERUNICODE_SUCS_TRAP_H
#define SUPERUNICODE_SUCS_TRAP_H

#include "sucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Kernel Security Trap Damage Control Dispatch
 *
 * Each of the 15 Kernel Security Trap slots (0x7FFFFFF0-0x7FFFFFFE) governs a
 * cluster of 128 B+ BANcodes (0x0011A000-0x0011A77F). A handler registered for
 * a slot is invoked by sucs_trap_dispatch() whenever a B+ BANcode in its
 * cluster is raised, enabling kernel crash damage control tailored to the
 * BANcode registry domain.
 *
 * Handlers run in the calling (crash) context and must be freestanding-safe:
 * zero libc dependencies, no allocation, no blocking.
 *
 * The dispatch table is a fixed-size static table (zero dynamic allocation).
 */

/* Damage-control handler signature:
 *   trap_cp    - the Kernel Security Trap codepoint (0x7FFFFFF0+slot)
 *   bancode_cp - the B+ BANcode that triggered the dispatch
 *   context    - the caller-supplied context registered for the slot
 */
typedef void (*sucs_trap_handler_t)(sucs_char_t trap_cp, sucs_char_t bancode_cp, void* context);

/**
 * Registers (or replaces) the damage-control handler for a trap slot.
 * slot must be 0..SUCS_TRAP_SLOT_COUNT-1 and handler must be non-NULL
 * (use sucs_trap_unregister_handler to clear a slot).
 * Returns true on success.
 */
bool sucs_trap_register_handler(uint32_t slot, sucs_trap_handler_t handler, void* context);

/**
 * Unregisters the handler for a trap slot.
 * Returns true if a handler was actually removed.
 */
bool sucs_trap_unregister_handler(uint32_t slot);

/**
 * Returns true if a handler is installed for the slot; optionally returns the
 * registered context via out_context (may be NULL).
 */
bool sucs_trap_handler_installed(uint32_t slot, void** out_context);

/**
 * Removes every registered handler (used for shutdown / recovery).
 */
void sucs_trap_clear_all(void);

/**
 * Dispatches a B+ BANcode to the handler governing its cluster. Returns true
 * if a handler was installed and invoked. Non-fatal BANcodes, unmapped BANcodes
 * (slot 15 / 0x0011A780-0x0011A7FF), and slots without a handler return false.
 */
bool sucs_trap_dispatch(sucs_char_t bancode_cp);

/* Freestanding-readable diagnostic record of the most recent dispatch. */
typedef struct {
    bool        fired;        /* true if the last dispatch invoked a handler */
    uint32_t    slot;         /* trap slot index (0..14) */
    sucs_char_t trap_cp;      /* Kernel Security Trap codepoint */
    sucs_char_t bancode_cp;   /* B+ BANcode that triggered the dispatch */
} sucs_trap_diagnostic_t;

/**
 * Returns the diagnostic record of the most recent dispatch attempt.
 */
sucs_trap_diagnostic_t sucs_trap_last_dispatch(void);

#ifdef __cplusplus
}
#endif

#endif /* SUPERUNICODE_SUCS_TRAP_H */
