/**
 * OpenWindows Kernel Mode-Switching Subsystem
 *
 * Implements kernel transitions between Base SUCS (31-bit character encoding &
 * SUTF-8/16/4/2 text formatting transports) and ExtSUCS (unbounded character encoding
 * & extSUTF text formatting transports).
 *
 * Mode alterations require a system restart. Mode switches are staged as pending
 * and committed during early kernel boot initialization.
 * Zero standard library dependencies.
 */

#include "sucs_mode.h"

/* Default kernel boot configuration state */
static sucs_kernel_boot_config_t g_kernel_boot_cfg = {
    .active_mode        = SUCS_MODE_BASE,
    .pending_mode       = SUCS_MODE_BASE,
    .reboot_required    = false,
    .mode_change_count  = 0
};

void sucs_init_boot_config(sucs_kernel_boot_config_t* boot_cfg, sucs_kernel_mode_t initial_mode) {
    sucs_kernel_boot_config_t* target = boot_cfg ? boot_cfg : &g_kernel_boot_cfg;
    if (initial_mode != SUCS_MODE_BASE && initial_mode != SUCS_MODE_EXTENDED) {
        initial_mode = SUCS_MODE_BASE;
    }
    target->active_mode       = initial_mode;
    target->pending_mode      = initial_mode;
    target->reboot_required   = false;
    target->mode_change_count = 0;
}

sucs_kernel_mode_t sucs_get_active_mode(void) {
    return g_kernel_boot_cfg.active_mode;
}

sucs_kernel_mode_t sucs_get_pending_mode(void) {
    return g_kernel_boot_cfg.pending_mode;
}

bool sucs_is_reboot_required(void) {
    return g_kernel_boot_cfg.reboot_required;
}

sucs_switch_status_t sucs_request_mode_switch(sucs_kernel_mode_t new_mode) {
    if (new_mode != SUCS_MODE_BASE && new_mode != SUCS_MODE_EXTENDED) {
        return SUCS_SWITCH_ERR_INVALID_MODE;
    }

    if (new_mode == g_kernel_boot_cfg.active_mode && !g_kernel_boot_cfg.reboot_required) {
        return SUCS_SWITCH_ERR_ALREADY_ACTIVE;
    }

    /* Stage pending mode and signal required system reboot */
    g_kernel_boot_cfg.pending_mode    = new_mode;
    g_kernel_boot_cfg.reboot_required = true;

    return SUCS_SWITCH_REBOOT_REQUIRED;
}

bool sucs_commit_mode_on_boot(sucs_kernel_boot_config_t* boot_cfg) {
    sucs_kernel_boot_config_t* target = boot_cfg ? boot_cfg : &g_kernel_boot_cfg;

    if (target->reboot_required && (target->pending_mode != target->active_mode)) {
        /* Commit pending mode alteration to active kernel state */
        target->active_mode       = target->pending_mode;
        target->reboot_required   = false;
        target->mode_change_count++;
        return true;
    }

    target->reboot_required = false;
    return false;
}
