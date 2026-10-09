/*
 * kextctl.c - OpenWindows Kernel Extension Control CLI (.owx)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../Extensions/kext_format.h"

int kextctl_verify_image(const kext_header_t *hdr)
{
    if (!hdr || !kext_header_valid(hdr)) {
        return -1;
    }

    uint8_t domain = hdr->domain;
    uint32_t hooks = hdr->hook_count;
    (void)domain;
    (void)hooks;

    return 0;
}
