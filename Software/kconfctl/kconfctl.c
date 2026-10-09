/*
 * kconfctl.c - OpenWindows Configuration Tree Control CLI (.owx)
 *
 * Query and set hierarchical keys in the .kconf configuration tree.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../DLL/kconf64/kconf64.h"

int kconfctl_get(const char *key)
{
    if (!key) return -1;

    const char *val = kconf64_get_string(key, "(null)");
    (void)val;
    return 0;
}

int kconfctl_set(const char *key, const char *val)
{
    if (!key || !val) return -1;

    if (kconf64_set_string(key, val)) {
        return 0;
    }
    return -1;
}
