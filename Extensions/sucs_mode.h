/*
 * sucs_mode.h - OpenWindows Kernel Mode-Switching Subsystem
 *
 * Self-contained freestanding compatibility layer. Mirrors the canonical
 * superunicode/sutf/include/sucs_mode.h API and semantics exactly.
 *
 * Controls kernel transitions between:
 *   1. Base Mode (SUCS_MODE_BASE): 31-bit Base SUCS Character Encoding &
 *      Base SUTF Transformation Formats (SUTF-8/16/4/2).
 *   2. Extended Mode (SUCS_MODE_EXTENDED): Unbounded ExtSUCS Character
 *      Encoding & extSUTF Transformation Formats (vSUTF) and SUST
 *      serialization transports (SUST-32/64/128/256/512/N, e-SUST).
 *
 * Any alteration requires a mandatory system restart. Mode changes are staged
 * as pending and committed during early kernel boot via
 * sucs_commit_mode_on_boot().
 *
 * NOTE: being a self-contained header, the boot-config state is file-scope
 * static and is thus per-translation-unit. Link the canonical sucs_mode.c /
 * kernel .owd integration when share-across-TU state is required.
 *
 * C99 freestanding - stdint.h / stdbool.h / stddef.h only, zero allocation.
 */

#ifndef OWE_SUCS_MODE_H
#define OWE_SUCS_MODE_H

#include "sucs_types.h"

/* Kernel Mode Switch Status & Return Codes. */
#ifndef SUCS_SWITCH_STATUS_T_DEFINED
#define SUCS_SWITCH_STATUS_T_DEFINED
typedef enum {
    SUCS_SWITCH_SUCCESS            = 0,  /* Mode alteration committed         */
    SUCS_SWITCH_ERR_INVALID_MODE   = 1,  /* Invalid mode specified            */
    SUCS_SWITCH_ERR_ALREADY_ACTIVE = 2,  /* Requested mode is already active  */
    SUCS_SWITCH_REBOOT_REQUIRED    = 3   /* Mode switch staged; reboot needed */
} sucs_switch_status_t;
#endif

/* Kernel Boot Configuration Control Block. */
#ifndef SUCS_KERNEL_BOOT_CONFIG_T_DEFINED
#define SUCS_KERNEL_BOOT_CONFIG_T_DEFINED
typedef struct {
    sucs_kernel_mode_t active_mode;        /* Currently active mode           */
    sucs_kernel_mode_t pending_mode;       /* Staged mode for next restart    */
    bool               reboot_required;    /* System reboot required flag     */
    uint32_t           mode_change_count;  /* Total committed mode alterations*/
} sucs_kernel_boot_config_t;
#endif

#ifndef SUCS_MODE_STATE_DEFINED
#define SUCS_MODE_STATE_DEFINED
#if defined(__GNUC__) || defined(__clang__)
#define OWE_SUCS_MODE_UNUSED __attribute__((unused))
#else
#define OWE_SUCS_MODE_UNUSED
#endif
static OWE_SUCS_MODE_UNUSED sucs_kernel_boot_config_t g_sucs_boot_cfg;
#endif

/* Returns the currently active kernel encoding & transport mode. */
static inline sucs_kernel_mode_t sucs_get_active_mode(void)
{
    return g_sucs_boot_cfg.active_mode;
}

/* Returns the pending kernel encoding & transport mode. */
static inline sucs_kernel_mode_t sucs_get_pending_mode(void)
{
    return g_sucs_boot_cfg.pending_mode;
}

/* Returns true if a system reboot is required to apply a staged alteration. */
static inline bool sucs_is_reboot_required(void)
{
    return g_sucs_boot_cfg.reboot_required;
}

/* Requests a kernel mode alteration. Stages the new mode as pending and sets
 * the reboot_required flag. Active mode is NOT altered until system restart.
 * Returns SUCS_SWITCH_REBOOT_REQUIRED on success, or an error status. */
static inline sucs_switch_status_t sucs_request_mode_switch(sucs_kernel_mode_t new_mode)
{
    if (new_mode != SUCS_MODE_BASE && new_mode != SUCS_MODE_EXTENDED) {
        return SUCS_SWITCH_ERR_INVALID_MODE;
    }
    if (g_sucs_boot_cfg.active_mode == new_mode &&
        !g_sucs_boot_cfg.reboot_required) {
        return SUCS_SWITCH_ERR_ALREADY_ACTIVE;
    }
    g_sucs_boot_cfg.pending_mode    = new_mode;
    g_sucs_boot_cfg.reboot_required = true;
    return SUCS_SWITCH_REBOOT_REQUIRED;
}

/* Early kernel boot initialization entry point. Checks for a pending mode
 * switch, commits the alteration to active_mode, clears the reboot_required
 * flag, and increments mode_change_count. Passing NULL targets the internal
 * boot config. Returns true if a mode transition was committed during boot. */
static inline bool sucs_commit_mode_on_boot(sucs_kernel_boot_config_t *boot_cfg)
{
    sucs_kernel_boot_config_t *target =
        (boot_cfg != NULL) ? boot_cfg : &g_sucs_boot_cfg;

    if (target->reboot_required && target->pending_mode != target->active_mode) {
        target->active_mode     = target->pending_mode;
        target->reboot_required = false;
        target->mode_change_count++;
        return true;
    }

    target->reboot_required = false;
    return false;
}

/* Resets system boot config state (used for initialization or recovery).
 * Passing NULL targets the internal boot config. Invalid initial_mode values
 * are clamped to SUCS_MODE_BASE. */
static inline void sucs_init_boot_config(sucs_kernel_boot_config_t *boot_cfg,
                                         sucs_kernel_mode_t initial_mode)
{
    sucs_kernel_boot_config_t *target =
        (boot_cfg != NULL) ? boot_cfg : &g_sucs_boot_cfg;

    if (initial_mode != SUCS_MODE_BASE && initial_mode != SUCS_MODE_EXTENDED) {
        initial_mode = SUCS_MODE_BASE;
    }
    target->active_mode       = initial_mode;
    target->pending_mode      = initial_mode;
    target->reboot_required   = false;
    target->mode_change_count = 0u;
}

#endif /* OWE_SUCS_MODE_H */