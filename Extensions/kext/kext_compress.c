/*
 * kext_compress.c - OpenWindows Compression Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_compress.h"

static bool kext_compress_loaded = false;

void kext_compress_register(const kext_compress_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_compress_loaded = true;
}

void kext_compress_unregister(const char *name)
{
    (void)name;
    kext_compress_loaded = false;
}

bool kext_compress_is_loaded(void)
{
    return kext_compress_loaded;
}
