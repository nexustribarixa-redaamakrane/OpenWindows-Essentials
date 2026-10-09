/*
 * kext_profile.c - OpenWindows Performance Profiler Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_profile.h"

static bool kext_profile_loaded = false;

void kext_profile_register(const kext_profile_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_profile_loaded = true;
}

void kext_profile_unregister(const char *name)
{
    (void)name;
    kext_profile_loaded = false;
}

bool kext_profile_is_loaded(void)
{
    return kext_profile_loaded;
}
