/*
 * kext_snap.c - OpenWindows Filesystem Snapshot Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_snap.h"

static bool kext_snap_loaded = false;

void kext_snap_register(const kext_snap_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_snap_loaded = true;
}

void kext_snap_unregister(const char *name)
{
    (void)name;
    kext_snap_loaded = false;
}

bool kext_snap_is_loaded(void)
{
    return kext_snap_loaded;
}
