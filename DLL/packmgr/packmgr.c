/*
 * packmgr.c - OpenWindows Package & Resource Library Implementation (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "packmgr.h"

bool packmgr_validate_owp(const owp_header_t *hdr)
{
    return owp_header_valid(hdr);
}

bool packmgr_validate_owr(const owr_header_t *hdr)
{
    return owr_header_valid(hdr);
}

static bool str_equals(const char *a, const char *b)
{
    size_t i = 0u;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        i++;
    }
    return (a[i] == b[i]);
}

int32_t packmgr_find_file(const owp_file_entry_t *entries, uint32_t count, const char *strings, const char *target_name)
{
    if (!entries || !strings || !target_name) return -1;

    for (uint32_t i = 0u; i < count; ++i) {
        const char *name = strings + entries[i].name_offset;
        if (str_equals(name, target_name)) {
            return (int32_t)i;
        }
    }
    return -1;
}

const owr_entry_t *packmgr_find_resource(const owr_entry_t *entries, uint32_t count, uint32_t res_id)
{
    if (!entries) return NULL;

    for (uint32_t i = 0u; i < count; ++i) {
        if (entries[i].resource_id == res_id) {
            return &entries[i];
        }
    }
    return NULL;
}
