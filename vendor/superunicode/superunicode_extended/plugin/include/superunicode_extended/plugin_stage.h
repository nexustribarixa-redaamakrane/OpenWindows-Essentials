#ifndef SUCS_PLUGIN_STAGE_H
#define SUCS_PLUGIN_STAGE_H

/**
 * Plugin Staging (Runtime / Pre-Restart Phase)
 *
 * Installing a SuperUnicode plugin requires a MANDATORY system restart,
 * exactly like switching between Base SUCS and ExtSUCS operating modes.
 *
 *   sucs_plugin_stage_install(blob, size)
 *     -> validates the blob structure
 *     -> copies it into the staging table
 *     -> returns SUCS_PLUGIN_REBOOT_REQUIRED
 *
 * The staged plugin is consumed at boot by sucs_plugin_commit_on_boot()
 * (see plugin_boot.h). The boot-time checksum gate decides whether the
 * plugin is mounted or quarantined.
 */

#include "plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Stages a plugin blob for installation.
 *
 * Structural validation only (magic, version, id, blob size, range records).
 * The authoritative integrity gate is the boot-time checksum verification.
 *
 * Returns SUCS_PLUGIN_REBOOT_REQUIRED on success. Plugin activation occurs
 * only after a system restart.
 */
sucs_plugin_status_t sucs_plugin_stage_install(const uint8_t* blob, size_t blob_size);

/**
 * Returns the number of staged plugins awaiting the next boot commit.
 */
uint32_t sucs_plugin_get_pending_count(void);

/**
 * Returns true when staged plugins exist and a reboot is required.
 */
bool sucs_plugin_is_reboot_required(void);

/**
 * Copies the id of the staged plugin at `index` into `out_id`.
 * Returns false when the index is out of range.
 */
bool sucs_plugin_get_pending_id(uint32_t index, char* out_id, size_t out_capacity);

/**
 * Returns the staged blob at `index` (read-only). Returns false when the
 * index is out of range.
 */
bool sucs_plugin_get_pending_blob(uint32_t index, const uint8_t** out_blob, size_t* out_size);

/**
 * Clears the staging table and the reboot-required flag.
 */
void sucs_plugin_stage_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* SUCS_PLUGIN_STAGE_H */
