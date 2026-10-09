/*
 * kext_hotplug.c - OpenWindows Hot-plug Device Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_hotplug.h"

static bool kext_hotplug_loaded = false;

void kext_hotplug_register(const kext_hotplug_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_hotplug_loaded = true;
}

void kext_hotplug_unregister(const char *name)
{
    (void)name;
    kext_hotplug_loaded = false;
}

bool kext_hotplug_is_loaded(void)
{
    return kext_hotplug_loaded;
}
