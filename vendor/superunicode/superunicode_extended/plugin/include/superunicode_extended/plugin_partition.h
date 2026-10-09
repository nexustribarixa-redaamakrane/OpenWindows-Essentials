#ifndef SUCS_PLUGIN_PARTITION_H
#define SUCS_PLUGIN_PARTITION_H

/**
 * SuperUnicode Partition Policy & Plugin Partition Mounting
 *
 * Two distinct partition concepts:
 *
 *   1. SuperUnicode Partition (Base SUCS + inherited by ExtSUCS):
 *        Format: OWFS or USFS.
 *        Purpose: used by Base SUCS ONLY for bugfix + rescue payloads.
 *
 *   2. SuperUnicode Plugin Partition (ExtSUCS-only feature):
 *        Format: OWFS EXCLUSIVELY.
 *        Purpose: hosts one mounted plugin's codepoint data (names, props,
 *        mappings, sources) and is read-only after the boot checksum pass.
 *
 * The binding to libowfs.a (htl_device_t block I/O) is injected by the
 * OpenWindows kernel at integration time; this module enforces the
 * filesystem policy and tracks mount state in a freestanding manner.
 */

#include "plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * SuperUnicode Partition Filesystem Types
 * ============================================================================ */
typedef enum {
    SUCS_PARTITION_FS_NONE = 0,  /* Not formatted / unknown */
    SUCS_PARTITION_FS_OWFS = 1,  /* OpenWindows File System (native) */
    SUCS_PARTITION_FS_USFS = 2   /* Universal Secured File System (portable) */
} sucs_partition_fs_t;

/* ============================================================================
 * Mounted Plugin Partition Descriptor
 * ============================================================================ */
typedef struct {
    char                     plugin_id[SUCS_PLUGIN_ID_MAX];
    sucs_partition_fs_t      fs_type;      /* MUST be OWFS for plugin partitions */
    bool                     mounted;      /* Mounted into the active namespace */
    bool                     read_only;    /* Always true (integrity protection) */
    bool                     checksum_ok;  /* Boot-time checksum pass result */
    uint32_t                 range_count;
    sucs_plugin_range_t      ranges[SUCS_PLUGIN_MAX_RANGES];
} sucs_plugin_partition_t;

/**
 * Returns true when `fs` is a permitted SuperUnicode Partition format
 * (OWFS or USFS). Plugin partitions further restrict this to OWFS.
 */
bool sucs_plugin_partition_fs_is_valid(sucs_partition_fs_t fs);

/**
 * Mounts a plugin partition.
 *
 * Policy: plugin partitions MUST be formatted as OWFS. A USFS (or any
 * non-OWFS) plugin partition is rejected with SUCS_PLUGIN_ERR_NOT_OWFS.
 * The mounted partition is always read-only and its ranges must be valid.
 */
sucs_plugin_status_t sucs_plugin_partition_mount(sucs_plugin_partition_t* part,
                                                 const char* plugin_id,
                                                 sucs_partition_fs_t fs,
                                                 const sucs_plugin_range_t* ranges,
                                                 uint32_t range_count);

/**
 * Unmounts a plugin partition and clears its state.
 */
sucs_plugin_status_t sucs_plugin_partition_unmount(sucs_plugin_partition_t* part);

/**
 * Returns the number of plugin partitions currently mounted.
 */
uint32_t sucs_plugin_partition_get_mounted_count(void);

/**
 * Resets the mounted partition table. Used for initialization and recovery.
 */
void sucs_plugin_partition_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* SUCS_PLUGIN_PARTITION_H */
