#ifndef SUCS_PLUGIN_BOOT_H
#define SUCS_PLUGIN_BOOT_H

/**
 * Plugin Boot Commit (Early Kernel Boot Phase)
 *
 * Runs during early kernel boot initialization, mirroring the contract of
 * sucs_commit_mode_on_boot() in the mode-switching subsystem.
 *
 *   sucs_plugin_commit_on_boot(cfg)
 *     -> for EVERY staged plugin:
 *        1. VERIFY CHECKSUM (CRC32c + Fletcher-64)          <-- first gate
 *        2. valid   -> mount as a SuperUnicode PLUGIN Partition (OWFS ONLY)
 *        3. valid   -> register the plugin's ranges into the active ExtSUCS
 *           namespace
 *        4. invalid -> QUARANTINE. The plugin is never mounted.
 *
 * Returns true when at least one plugin was mounted during this boot.
 */

#include "plugin.h"
#include "plugin_stage.h"
#include "plugin_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Plugin Boot Configuration Control Block
 * ============================================================================ */
typedef struct {
    uint32_t staged_count;        /* Staged plugins consumed by this commit */
    uint32_t mounted_count;       /* Plugins mounted + registered */
    uint32_t quarantined_count;   /* Plugins rejected (checksum / policy) */
    bool     reboot_required;     /* Cleared after the commit pass */
} sucs_plugin_boot_config_t;

/**
 * Early kernel boot initialization entry point for the plugin subsystem.
 * Consumes the staging table, verifies checksums, mounts valid plugins as
 * OWFS-only plugin partitions, registers their codepoint ranges, and
 * quarantines invalid ones.
 *
 * Returns true if at least one plugin was mounted.
 */
bool sucs_plugin_commit_on_boot(sucs_plugin_boot_config_t* cfg);

/**
 * Returns the number of plugins currently mounted and active.
 */
uint32_t sucs_plugin_get_active_count(void);

/**
 * Returns the number of plugins quarantined by the last commit.
 */
uint32_t sucs_plugin_get_quarantined_count(void);

/**
 * Returns true when `ex_cp` falls inside the registered ranges of any
 * active plugin (i.e. the codepoint is provided by a mounted plugin).
 */
bool sucs_plugin_is_range_registered(sucs_ex_char_t ex_cp);

/**
 * Looks up the active plugin that owns `ex_cp` (if any).
 * Returns true and stores the plugin ABI pointer, or false when unowned.
 */
bool sucs_plugin_lookup_codepoint(sucs_ex_char_t ex_cp, const sucs_plugin_t** out_plugin);

/**
 * Resets the active plugin table, registered ranges, quarantine counter,
 * and mounted partition table. Used for initialization and recovery.
 */
void sucs_plugin_boot_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* SUCS_PLUGIN_BOOT_H */
