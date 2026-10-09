/*
 * kext_encrypt.c - OpenWindows Full Disk Encryption Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_encrypt.h"

static bool kext_encrypt_loaded = false;

void kext_encrypt_register(const kext_encrypt_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_encrypt_loaded = true;
}

void kext_encrypt_unregister(const char *name)
{
    (void)name;
    kext_encrypt_loaded = false;
}

bool kext_encrypt_is_loaded(void)
{
    return kext_encrypt_loaded;
}
