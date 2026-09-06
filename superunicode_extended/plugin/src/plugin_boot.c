/**
 * Plugin Boot Commit (Early Kernel Boot Phase)
 *
 * Consumes the staging table and applies the boot-time integrity gate:
 *   1. checksum verification (CRC32c + Fletcher-64)
 *   2. range policy validation (must extend past the base limit)
 *   3. collision check against active plugin ranges
 *   4. OWFS-only plugin partition mount
 *   5. range registration into the active ExtSUCS namespace
 *
 * Any failure at a gate quarantines the plugin — it is never mounted.
 *
 * Zero standard library dependencies.
 */

#include "superunicode_extended/plugin_boot.h"
#include "superunicode_extended/plugin_checksum.h"

typedef struct {
    bool            used;
    sucs_plugin_t   plugin;   /* ABI view; ranges pointer targets this entry */
    sucs_plugin_range_t ranges[SUCS_PLUGIN_MAX_RANGES];
} sucs_active_plugin_t;

typedef struct {
    sucs_plugin_range_t range;
} sucs_registered_range_t;

static sucs_active_plugin_t g_active[SUCS_PLUGIN_MAX_MOUNTED];
static uint32_t g_active_count;
static sucs_registered_range_t g_registered[SUCS_PLUGIN_MAX_REGISTERED_RANGES];
static uint32_t g_registered_count;
static uint32_t g_quarantined_count;

static bool ranges_overlap(const sucs_plugin_range_t* a, const sucs_plugin_range_t* b) {
    return (a->start <= b->end) && (b->start <= a->end);
}

static bool ranges_free(const sucs_plugin_range_t* ranges, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        for (uint32_t j = 0; j < g_registered_count; ++j) {
            if (ranges_overlap(&ranges[i], &g_registered[j].range)) {
                return false;
            }
        }
        /* Ranges within one plugin must not overlap each other. */
        for (uint32_t k = 0; k < i; ++k) {
            if (ranges_overlap(&ranges[i], &ranges[k])) {
                return false;
            }
        }
    }
    return true;
}

static void register_plugin_ranges(const sucs_plugin_range_t* ranges, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (g_registered_count < SUCS_PLUGIN_MAX_REGISTERED_RANGES) {
            g_registered[g_registered_count].range = ranges[i];
            g_registered_count++;
        }
    }
}

/* ============================================================================
 * Boot Commit
 * ============================================================================ */
bool sucs_plugin_commit_on_boot(sucs_plugin_boot_config_t* cfg) {
    if (cfg) {
        cfg->staged_count = 0;
        cfg->mounted_count = 0;
        cfg->quarantined_count = 0;
        cfg->reboot_required = false;
    }

    uint32_t pending = sucs_plugin_get_pending_count();
    if (cfg) {
        cfg->staged_count = pending;
    }

    uint32_t mounted_this_boot = 0;

    for (uint32_t i = 0; i < pending; ++i) {
        const uint8_t* blob = 0;
        size_t blob_size = 0;
        const sucs_plugin_blob_header_t* hdr = 0;
        char id[SUCS_PLUGIN_ID_MAX];
        sucs_plugin_range_t ranges[SUCS_PLUGIN_MAX_RANGES];
        uint32_t range_count = 0;
        bool ok = true;

        if (!sucs_plugin_get_pending_blob(i, &blob, &blob_size)) {
            ok = false;
        }

        /* GATE 1: boot-time checksum verification. */
        if (ok && !sucs_plugin_blob_verify(blob, blob_size)) {
            ok = false;
        }

        /* GATE 2: range policy (structure + above base limit). */
        if (ok) {
            if (sucs_plugin_parse_ranges(blob, blob_size, ranges,
                                         SUCS_PLUGIN_MAX_RANGES, &range_count) != SUCS_PLUGIN_OK) {
                ok = false;
            } else if (sucs_plugin_validate_ranges(ranges, range_count) != SUCS_PLUGIN_OK) {
                ok = false;
            }
        }

        /* GATE 3: collision against active plugin ranges. */
        if (ok && !ranges_free(ranges, range_count)) {
            ok = false;
        }

        /* GATE 4: OWFS-only plugin partition mount. */
        sucs_plugin_partition_t part;
        if (ok) {
            hdr = (const sucs_plugin_blob_header_t*)blob;
            for (size_t k = 0; k < SUCS_PLUGIN_ID_MAX; ++k) {
                id[k] = hdr->id[k];
            }
            if (sucs_plugin_partition_mount(&part, id, SUCS_PARTITION_FS_OWFS,
                                            ranges, range_count) != SUCS_PLUGIN_OK) {
                ok = false;
            }
        }

        if (!ok) {
            /* Quarantined: never mounted, never registered. */
            g_quarantined_count++;
            continue;
        }

        /* GATE 5: register ranges + activate the plugin. */
        if (g_active_count >= SUCS_PLUGIN_MAX_MOUNTED) {
            g_quarantined_count++;
            continue;
        }

        sucs_active_plugin_t* entry = &g_active[g_active_count];
        entry->used = true;
        for (uint32_t r = 0; r < range_count; ++r) {
            entry->ranges[r] = ranges[r];
        }
        for (size_t k = 0; k < SUCS_PLUGIN_ID_MAX; ++k) {
            entry->plugin.id[k] = id[k];
        }
        entry->plugin.ver_major = hdr->ver_major;
        entry->plugin.ver_minor = hdr->ver_minor;
        entry->plugin.ver_patch = hdr->ver_patch;
        entry->plugin.base_limit = SUCS_PLUGIN_BASE_LIMIT;
        entry->plugin.range_count = range_count;
        entry->plugin.ranges = entry->ranges;
        entry->plugin.data = 0;
        entry->plugin.data_count = 0;

        register_plugin_ranges(ranges, range_count);
        g_active_count++;
        mounted_this_boot++;
    }

    /* The staged set is consumed by this commit pass. */
    sucs_plugin_stage_reset();

    if (cfg) {
        cfg->mounted_count = mounted_this_boot;
        cfg->quarantined_count = g_quarantined_count;
        cfg->reboot_required = false;
    }
    return mounted_this_boot > 0;
}

uint32_t sucs_plugin_get_active_count(void) {
    return g_active_count;
}

uint32_t sucs_plugin_get_quarantined_count(void) {
    return g_quarantined_count;
}

bool sucs_plugin_is_range_registered(sucs_ex_char_t ex_cp) {
    for (uint32_t i = 0; i < g_registered_count; ++i) {
        if (ex_cp >= g_registered[i].range.start && ex_cp <= g_registered[i].range.end) {
            return true;
        }
    }
    return false;
}

bool sucs_plugin_lookup_codepoint(sucs_ex_char_t ex_cp, const sucs_plugin_t** out_plugin) {
    if (!out_plugin) {
        return false;
    }
    for (uint32_t i = 0; i < g_active_count; ++i) {
        for (uint32_t r = 0; r < g_active[i].plugin.range_count; ++r) {
            if (ex_cp >= g_active[i].plugin.ranges[r].start &&
                ex_cp <= g_active[i].plugin.ranges[r].end) {
                *out_plugin = &g_active[i].plugin;
                return true;
            }
        }
    }
    *out_plugin = 0;
    return false;
}

void sucs_plugin_boot_reset(void) {
    for (uint32_t i = 0; i < g_active_count; ++i) {
        g_active[i].used = false;
        g_active[i].plugin.range_count = 0;
    }
    g_active_count = 0;
    g_registered_count = 0;
    g_quarantined_count = 0;
    sucs_plugin_partition_reset();
}
