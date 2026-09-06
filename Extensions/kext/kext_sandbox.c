/*
 * kext_sandbox.c - OpenWindows Process Sandbox Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_sandbox.h"

static bool kext_sandbox_loaded = false;

void kext_sandbox_register(const kext_sandbox_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_sandbox_loaded = true;
}

void kext_sandbox_unregister(const char *name)
{
    (void)name;
    kext_sandbox_loaded = false;
}

bool kext_sandbox_is_loaded(void)
{
    return kext_sandbox_loaded;
}
