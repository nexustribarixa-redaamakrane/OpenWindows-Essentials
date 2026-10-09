/*
 * kext_audit.c - OpenWindows Security Audit Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_audit.h"

static bool kext_audit_loaded = false;

void kext_audit_register(const kext_audit_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_audit_loaded = true;
}

void kext_audit_unregister(const char *name)
{
    (void)name;
    kext_audit_loaded = false;
}

bool kext_audit_is_loaded(void)
{
    return kext_audit_loaded;
}
