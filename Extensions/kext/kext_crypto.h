/*
 * kext_crypto.h - OpenWindows Cryptographic Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_CRYPTO_H
#define KEXT_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_CRYPTO_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_crypto_ops_t;

void kext_crypto_register(const kext_crypto_ops_t *ops);
void kext_crypto_unregister(const char *name);
bool kext_crypto_is_loaded(void);

#endif /* KEXT_CRYPTO_H */
