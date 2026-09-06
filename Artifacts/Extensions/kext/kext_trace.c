/*
 * kext_trace.c - OpenWindows Execution Tracer Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_trace.h"

static bool kext_trace_loaded = false;

void kext_trace_register(const kext_trace_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_trace_loaded = true;
}

void kext_trace_unregister(const char *name)
{
    (void)name;
    kext_trace_loaded = false;
}

bool kext_trace_is_loaded(void)
{
    return kext_trace_loaded;
}
