/**
 * Plugin Staging (Runtime / Pre-Restart Phase)
 *
 * Validates a plugin blob's structure, copies it into the fixed staging
 * table, and flags a mandatory reboot — the exact contract used by the
 * Base <-> Extended mode-switch subsystem (sucs_request_mode_switch()).
 *
 * Zero standard library dependencies.
 */

#include "superunicode_extended/plugin_stage.h"
#include "superunicode_extended/plugin_checksum.h"

typedef struct {
    bool     used;
    char     id[SUCS_PLUGIN_ID_MAX];
    uint8_t  blob[SUCS_PLUGIN_STAGE_BUF_SIZE];
    uint32_t blob_size;
} sucs_pending_plugin_t;

static sucs_pending_plugin_t g_pending[SUCS_PLUGIN_MAX_PENDING];
static uint32_t g_pending_count;
static bool g_reboot_required;

static bool bytes_equal(const char* a, const char* b, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

/* ============================================================================
 * Stage Install: structural validation + staged copy
 * ============================================================================ */
sucs_plugin_status_t sucs_plugin_stage_install(const uint8_t* blob, size_t blob_size) {
    if (!blob || blob_size < sizeof(sucs_plugin_blob_header_t)) {
        return SUCS_PLUGIN_ERR_INVALID_BLOB;
    }
    if (blob_size > SUCS_PLUGIN_STAGE_BUF_SIZE) {
        return SUCS_PLUGIN_ERR_BUFFER_TOO_SMALL;
    }

    const sucs_plugin_blob_header_t* hdr = (const sucs_plugin_blob_header_t*)blob;
    if (hdr->magic != SUCS_PLUGIN_BLOB_MAGIC) {
        return SUCS_PLUGIN_ERR_INVALID_BLOB;
    }
    if (hdr->blob_version != SUCS_PLUGIN_BLOB_VERSION) {
        return SUCS_PLUGIN_ERR_UNSUPPORTED_VERSION;
    }
    if ((size_t)hdr->blob_size != blob_size) {
        return SUCS_PLUGIN_ERR_INVALID_BLOB;
    }
    if (hdr->id[0] == 0 || hdr->id[SUCS_PLUGIN_ID_MAX - 1] != 0) {
        return SUCS_PLUGIN_ERR_INVALID_ID;
    }

    /* Structural range checks (checksum gate runs at boot). */
    sucs_plugin_range_t ranges[SUCS_PLUGIN_MAX_RANGES];
    uint32_t range_count = 0;
    sucs_plugin_status_t status = sucs_plugin_parse_ranges(
        blob, blob_size, ranges, SUCS_PLUGIN_MAX_RANGES, &range_count);
    if (status != SUCS_PLUGIN_OK) {
        return status;
    }
    status = sucs_plugin_validate_ranges(ranges, range_count);
    if (status != SUCS_PLUGIN_OK) {
        return status;
    }

    /* Reject duplicate ids across the staging table. */
    for (uint32_t i = 0; i < g_pending_count; ++i) {
        if (bytes_equal(g_pending[i].id, hdr->id, SUCS_PLUGIN_ID_MAX)) {
            return SUCS_PLUGIN_ERR_DUPLICATE_ID;
        }
    }

    if (g_pending_count >= SUCS_PLUGIN_MAX_PENDING) {
        return SUCS_PLUGIN_ERR_STAGING_FULL;
    }

    sucs_pending_plugin_t* slot = &g_pending[g_pending_count];
    for (size_t i = 0; i < SUCS_PLUGIN_ID_MAX; ++i) {
        slot->id[i] = hdr->id[i];
    }
    for (size_t i = 0; i < blob_size; ++i) {
        slot->blob[i] = blob[i];
    }
    slot->blob_size = (uint32_t)blob_size;
    slot->used = true;
    g_pending_count++;

    /* Plugin installation always requires a mandatory restart. */
    g_reboot_required = true;
    return SUCS_PLUGIN_REBOOT_REQUIRED;
}

uint32_t sucs_plugin_get_pending_count(void) {
    return g_pending_count;
}

bool sucs_plugin_is_reboot_required(void) {
    return g_reboot_required;
}

bool sucs_plugin_get_pending_id(uint32_t index, char* out_id, size_t out_capacity) {
    if (index >= g_pending_count || !out_id) {
        return false;
    }
    const char* src = g_pending[index].id;
    for (size_t i = 0; i + 1 < out_capacity && i < SUCS_PLUGIN_ID_MAX; ++i) {
        out_id[i] = src[i];
        if (src[i] == 0) break;
    }
    out_id[out_capacity - 1] = 0;
    return true;
}

bool sucs_plugin_get_pending_blob(uint32_t index, const uint8_t** out_blob, size_t* out_size) {
    if (index >= g_pending_count || !out_blob || !out_size) {
        return false;
    }
    *out_blob = g_pending[index].blob;
    *out_size = (size_t)g_pending[index].blob_size;
    return true;
}

void sucs_plugin_stage_reset(void) {
    for (uint32_t i = 0; i < g_pending_count; ++i) {
        g_pending[i].used = false;
        g_pending[i].blob_size = 0;
        g_pending[i].id[0] = 0;
    }
    g_pending_count = 0;
    g_reboot_required = false;
}
