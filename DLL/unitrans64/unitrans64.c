/*
 * unitrans64.c - OpenWindows Unicode Translation Dynamic Library Implementation (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "unitrans64.h"
#include "unitrans.h"

uint32_t unitrans64_to_sucs(const uint8_t *utf8, size_t len, uint32_t *out_sucs, size_t max, size_t *out_len)
{
    return unitrans_utf8_to_sucs(utf8, len, out_sucs, max, out_len);
}

uint32_t unitrans64_to_sutf8(const uint32_t *sucs, size_t count, uint8_t *out_utf8, size_t max, size_t *out_len)
{
    return unitrans_sucs_to_sutf8(sucs, count, out_utf8, max, out_len);
}

bool unitrans64_validate_sucs(uint32_t cp)
{
    if (cp >= SCP_TRAP_BASE && cp <= SCP_TRAP_LIMIT) {
        return false; /* Trap range violation */
    }
    return true;
}
