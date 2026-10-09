/*
 * bancat.c - OpenWindows BANcode Crash Dump Decoder (.owx)
 *
 * Inspects and decodes .bdmp files, printing fault register frame,
 * failing BANcode, and sentinel execution history.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../Extensions/bdmp_format.h"

int bancat_decode(const bdmp_header_t *dump)
{
    if (!dump || !bdmp_header_valid(dump)) {
        return -1;
    }

    uint32_t panic_code = dump->primary_bancode;
    uint32_t core_id = dump->faulting_cpu_id;
    uint64_t tsc = dump->fault_tsc;

    (void)panic_code;
    (void)core_id;
    (void)tsc;

    return 0;
}
