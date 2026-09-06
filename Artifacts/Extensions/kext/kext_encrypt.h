/*
 * kext_encrypt.h - OpenWindows Full Disk Encryption Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_ENCRYPT_H
#define KEXT_ENCRYPT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_ENCRYPT_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_encrypt_ops_t;

void kext_encrypt_register(const kext_encrypt_ops_t *ops);
void kext_encrypt_unregister(const char *name);
bool kext_encrypt_is_loaded(void);

#endif /* KEXT_ENCRYPT_H */
