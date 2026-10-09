/*
 * kext_netmon.c - OpenWindows Network Monitor Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_netmon.h"

static bool kext_netmon_loaded = false;

void kext_netmon_register(const kext_netmon_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_netmon_loaded = true;
}

void kext_netmon_unregister(const char *name)
{
    (void)name;
    kext_netmon_loaded = false;
}

bool kext_netmon_is_loaded(void)
{
    return kext_netmon_loaded;
}
