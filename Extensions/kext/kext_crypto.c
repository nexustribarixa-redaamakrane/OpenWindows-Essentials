/*
 * kext_crypto.c - OpenWindows Cryptographic Extension
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kext_crypto.h"

static bool kext_crypto_loaded = false;

void kext_crypto_register(const kext_crypto_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    kext_crypto_loaded = true;
}

void kext_crypto_unregister(const char *name)
{
    (void)name;
    kext_crypto_loaded = false;
}

bool kext_crypto_is_loaded(void)
{
    return kext_crypto_loaded;
}
