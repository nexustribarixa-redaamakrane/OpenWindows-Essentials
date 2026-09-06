/*
 * kext_mirror.c - OpenWindows Disk Mirror/RAID Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_mirror.h"

static bool kext_mirror_loaded = false;

void kext_mirror_register(const kext_mirror_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_mirror_loaded = true;
}

void kext_mirror_unregister(const char *name)
{
    (void)name;
    kext_mirror_loaded = false;
}

bool kext_mirror_is_loaded(void)
{
    return kext_mirror_loaded;
}
