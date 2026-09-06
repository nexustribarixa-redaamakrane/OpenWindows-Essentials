/**
 * SuperUnicode Partition Policy & Plugin Partition Mounting
 *
 * Enforces the filesystem policy for plugin partitions:
 *   - SuperUnicode Plugin Partitions MUST be OWFS (OpenWindows File System).
 *   - USFS is a permitted SuperUnicode Partition (base) format only.
 *   - Mounted plugin partitions are ALWAYS read-only.
 *
 * The concrete block-I/O binding to libowfs.a (htl_device_t) is injected by
 * the OpenWindows kernel at integration time; this module tracks partition
 * mount state in a fully freestanding manner.
 *
 * Zero standard library dependencies.
 */

#include "superunicode_extended/plugin_partition.h"
#include "superunicode_extended/plugin_checksum.h"

static sucs_plugin_partition_t g_mounted[SUCS_PLUGIN_MAX_MOUNTED];
static uint32_t g_mounted_count;

static bool bytes_equal(const char* a, const char* b, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

bool sucs_plugin_partition_fs_is_valid(sucs_partition_fs_t fs) {
    return (fs == SUCS_PARTITION_FS_OWFS) || (fs == SUCS_PARTITION_FS_USFS);
}

sucs_plugin_status_t sucs_plugin_partition_mount(sucs_plugin_partition_t* part,
                                                 const char* plugin_id,
                                                 sucs_partition_fs_t fs,
                                                 const sucs_plugin_range_t* ranges,
                                                 uint32_t range_count) {
    if (!part || !plugin_id || plugin_id[0] == 0) {
        return SUCS_PLUGIN_ERR_MOUNT_FAILED;
    }

    /* POLICY: plugin partitions must be formatted EXCLUSIVELY as OWFS. */
    if (fs != SUCS_PARTITION_FS_OWFS) {
        return SUCS_PLUGIN_ERR_NOT_OWFS;
    }

    if (sucs_plugin_validate_ranges(ranges, range_count) != SUCS_PLUGIN_OK) {
        return SUCS_PLUGIN_ERR_INVALID_RANGE;
    }

    /* Duplicate partition guard. */
    for (uint32_t i = 0; i < g_mounted_count; ++i) {
        if (bytes_equal(g_mounted[i].plugin_id, plugin_id, SUCS_PLUGIN_ID_MAX)) {
            return SUCS_PLUGIN_ERR_DUPLICATE_ID;
        }
    }

    if (g_mounted_count >= SUCS_PLUGIN_MAX_MOUNTED) {
        return SUCS_PLUGIN_ERR_MOUNT_FAILED;
    }

    for (size_t k = 0; k < SUCS_PLUGIN_ID_MAX; ++k) {
        part->plugin_id[k] = plugin_id[k];
        if (plugin_id[k] == 0) break;
    }
    part->fs_type = SUCS_PARTITION_FS_OWFS;
    part->mounted = true;
    part->read_only = true;
    part->checksum_ok = true;
    part->range_count = range_count;
    for (uint32_t r = 0; r < range_count; ++r) {
        part->ranges[r] = ranges[r];
    }

    g_mounted[g_mounted_count] = *part;
    g_mounted_count++;
    return SUCS_PLUGIN_OK;
}

sucs_plugin_status_t sucs_plugin_partition_unmount(sucs_plugin_partition_t* part) {
    if (!part || !part->mounted) {
        return SUCS_PLUGIN_ERR_MOUNT_FAILED;
    }
    /* Match by plugin id (mount stores its own copy of the descriptor). */
    for (uint32_t i = 0; i < g_mounted_count; ++i) {
        if (bytes_equal(g_mounted[i].plugin_id, part->plugin_id, SUCS_PLUGIN_ID_MAX)) {
            /* Shift down to close the hole. */
            for (uint32_t j = i; j + 1 < g_mounted_count; ++j) {
                g_mounted[j] = g_mounted[j + 1];
            }
            g_mounted_count--;
            break;
        }
    }
    part->mounted = false;
    part->read_only = false;
    part->checksum_ok = false;
    part->range_count = 0;
    return SUCS_PLUGIN_OK;
}

uint32_t sucs_plugin_partition_get_mounted_count(void) {
    return g_mounted_count;
}

void sucs_plugin_partition_reset(void) {
    for (uint32_t i = 0; i < g_mounted_count; ++i) {
        g_mounted[i].mounted = false;
        g_mounted[i].read_only = false;
        g_mounted[i].checksum_ok = false;
        g_mounted[i].range_count = 0;
        g_mounted[i].plugin_id[0] = 0;
    }
    g_mounted_count = 0;
}
