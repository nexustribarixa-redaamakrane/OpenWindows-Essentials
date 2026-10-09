/*
 * packmgr.h - OpenWindows Package (.owp) & Resource (.owr) Dynamic Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef PACKMGR_H
#define PACKMGR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "owp_format.h"
#include "owr_format.h"

bool packmgr_validate_owp(const owp_header_t *hdr);
bool packmgr_validate_owr(const owr_header_t *hdr);
int32_t packmgr_find_file(const owp_file_entry_t *entries, uint32_t count, const char *strings, const char *target_name);
const owr_entry_t *packmgr_find_resource(const owr_entry_t *entries, uint32_t count, uint32_t res_id);

#endif /* PACKMGR_H */
