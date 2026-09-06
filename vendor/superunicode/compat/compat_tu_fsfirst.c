/* compat_tu_fsfirst.c - Include-order coexistence proof.
 *
 * Canonical OpenWindows-storage headers are included FIRST and the VIP
 * header SECOND. Proves the VIP header never redefines the unguarded
 * OWFS / USFS macros and adopts their canonical values via the
 * VIP_STORAGE_* aliases. Compiled as a separate translation unit so the
 * opposite order can be proven in the main smoke test.
 */
#include "owfs_types.h"
#include "usfs_types.h"
#include "univip_fvip.h"

int compat_check_fs_macros(void) {
    int ok = 1;

    /* Block geometry adopted from the canonical headers */
    if (VIP_STORAGE_BLOCK_SIZE != OWFS_BLOCK_SIZE) { ok = 0; }
    if (VIP_STORAGE_BLOCK_SIZE != USFS_BLOCK_SIZE) { ok = 0; }
    if (VIP_STORAGE_BLOCK_SIZE != 0x1000U) { ok = 0; }
    if (VIP_LBAS_PER_STORAGE_BLOCK != 8U) { ok = 0; }

    /* Entry-type bits shared across both filesystem families */
    if (USFS_ENTRY_FILE != OWFS_ENTRY_FILE) { ok = 0; }
    if (USFS_ENTRY_CATALOG != OWFS_ENTRY_CATALOG) { ok = 0; }
    if (USFS_ENTRY_DELETED != OWFS_ENTRY_DELETED) { ok = 0; }

    /* Security bits share positions across both filesystem families */
    if (USFS_SEC_ENCRYPTED != OWFS_SEC_ENCRYPTED) { ok = 0; }
    if (USFS_SEC_READONLY != OWFS_SEC_READONLY) { ok = 0; }
    if (USFS_SEC_HIDDEN != OWFS_SEC_HIDDEN) { ok = 0; }

    /* Flag conversion against the REAL on-disk constants */
    {
        uint32_t f;
        uint8_t type;
        uint32_t sec;

        f = fvip_flags_from_storage_entry(OWFS_ENTRY_FILE,
                                          OWFS_SEC_READONLY | OWFS_SEC_ENCRYPTED);
        if (f != (FVIP_FLAG_FILE | FVIP_FLAG_READONLY | FVIP_FLAG_ENCRYPTED)) { ok = 0; }

        f = fvip_flags_from_storage_entry((uint8_t)(OWFS_ENTRY_CATALOG | OWFS_ENTRY_DELETED),
                                          OWFS_SEC_HIDDEN);
        if (f != (FVIP_FLAG_CATALOG | FVIP_FLAG_DELETED | FVIP_FLAG_HIDDEN)) { ok = 0; }

        fvip_flags_to_storage_entry(FVIP_FLAG_FILE | FVIP_FLAG_ENCRYPTED,
                                    &type, &sec);
        if (type != OWFS_ENTRY_FILE) { ok = 0; }
        if (sec != OWFS_SEC_ENCRYPTED) { ok = 0; }

        fvip_flags_to_storage_entry(FVIP_FLAG_CATALOG | FVIP_FLAG_HIDDEN |
                                        FVIP_FLAG_DELETED,
                                    &type, &sec);
        if (type != (uint8_t)(OWFS_ENTRY_CATALOG | OWFS_ENTRY_DELETED)) { ok = 0; }
        if (sec != OWFS_SEC_HIDDEN) { ok = 0; }

        /* Neither FILE nor CATALOG defaults to FILE */
        fvip_flags_to_storage_entry(FVIP_FLAG_SYSTEM, &type, &sec);
        if (type != OWFS_ENTRY_FILE) { ok = 0; }
        if (sec != 0U) { ok = 0; }
    }

    return ok;
}
