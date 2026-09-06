/*
 * nullzero.c - OpenWindows Pseudo-Devices Driver Implementation (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "nullzero.h"

size_t null_read(void *buf, size_t len)
{
    (void)buf;
    (void)len;
    return 0u; /* EOF */
}

size_t null_write(const void *buf, size_t len)
{
    (void)buf;
    return len; /* Discarded */
}

size_t zero_read(void *buf, size_t len)
{
    if (!buf) return 0u;
    uint8_t *p = (uint8_t *)buf;
    for (size_t i = 0u; i < len; ++i) {
        p[i] = 0u;
    }
    return len;
}

size_t zero_write(const void *buf, size_t len)
{
    (void)buf;
    return len;
}
