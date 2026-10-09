#ifndef SUCS_MODE_H
#define SUCS_MODE_H

/**
 * OpenWindows Kernel Mode-Switching Subsystem
 *
 * Controls kernel transitions between:
 * 1. Base Mode (SUCS_MODE_BASE): 31-bit Base SUCS Character Encoding &
 *    Base SUTF Transformation Formats (SUTF-8, SUTF-16, SUTF-4, SUTF-2).
 * 2. Extended Mode (SUCS_MODE_EXTENDED): Unbounded ExtSUCS Character Encoding &
 *    extSUTF Transformation Formats (vSUTF) and SUST serialization transports
 *    (SUST-32/64/128/256/512/N, e-SUST).
 *
 * Any alteration between Base and Extended mode requires a mandatory system
 * restart. Mode changes are staged as 'pending' and committed during early
 * kernel boot initialization via sucs_commit_mode_on_boot().
 *
 * Zero standard library dependencies.
 */

#include "sucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SuperUnicode Kernel Encoding & Transformation Operating Modes */
typedef enum {
    SUCS_MODE_BASE     = 0,  /* Base SUCS (31-bit) & Base SUTF (SUTF-8/16/4/2) */
    SUCS_MODE_EXTENDED = 1   /* ExtSUCS (Unbounded 64-bit) & vSUTF + SUST transports */
} sucs_kernel_mode_t;

/* Kernel Mode Switch Status & Return Codes */
typedef enum {
    SUCS_SWITCH_SUCCESS               = 0,  /* Mode alteration committed */
    SUCS_SWITCH_ERR_INVALID_MODE      = 1,  /* Invalid mode specified */
    SUCS_SWITCH_ERR_ALREADY_ACTIVE    = 2,  /* Requested mode is already active */
    SUCS_SWITCH_REBOOT_REQUIRED       = 3   /* Mode switch staged; system reboot required */
} sucs_switch_status_t;

/* Kernel Boot Configuration Control Block */
typedef struct {
    sucs_kernel_mode_t active_mode;        /* Currently active kernel encoding & transformation mode */
    sucs_kernel_mode_t pending_mode;       /* Staged mode to apply on next system restart */
    bool               reboot_required;    /* System reboot required flag */
    uint32_t           mode_change_count;  /* Total committed mode alterations */
} sucs_kernel_boot_config_t;

/**
 * Returns the currently active kernel encoding & transformation mode.
 */
sucs_kernel_mode_t sucs_get_active_mode(void);

/**
 * Returns the pending kernel encoding & transformation mode.
 */
sucs_kernel_mode_t sucs_get_pending_mode(void);

/**
 * Returns true if a system reboot is required to apply a staged mode alteration.
 */
bool sucs_is_reboot_required(void);

/**
 * Requests a kernel mode alteration between SUCS_MODE_BASE and SUCS_MODE_EXTENDED.
 *
 * Stages the new mode as pending and sets the reboot_required flag.
 * Active kernel operating mode is NOT altered until system restart.
 *
 * Returns SUCS_SWITCH_REBOOT_REQUIRED on success, or an error status code.
 */
sucs_switch_status_t sucs_request_mode_switch(sucs_kernel_mode_t new_mode);

/**
 * Early kernel boot initialization entry point.
 * Checks for a pending mode switch, commits the alteration to active_mode,
 * clears the reboot_required flag, and increments mode_change_count.
 *
 * Returns true if a mode transition was committed during this boot.
 */
bool sucs_commit_mode_on_boot(sucs_kernel_boot_config_t* boot_cfg);

/**
 * Resets system boot config state (used for initialization or recovery).
 */
void sucs_init_boot_config(sucs_kernel_boot_config_t* boot_cfg, sucs_kernel_mode_t initial_mode);

#ifdef __cplusplus
}
#endif

#endif /* SUCS_MODE_H */
