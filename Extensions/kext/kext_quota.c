/*
 * kext_quota.c - OpenWindows Disk Quota Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_quota.h"

static bool kext_quota_loaded = false;

void kext_quota_register(const kext_quota_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_quota_loaded = true;
}

void kext_quota_unregister(const char *name)
{
    (void)name;
    kext_quota_loaded = false;
}

bool kext_quota_is_loaded(void)
{
    return kext_quota_loaded;
}
