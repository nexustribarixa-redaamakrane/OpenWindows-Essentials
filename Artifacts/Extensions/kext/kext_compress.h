/*
 * kext_compress.h - OpenWindows Compression Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_COMPRESS_H
#define KEXT_COMPRESS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_COMPRESS_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_compress_ops_t;

void kext_compress_register(const kext_compress_ops_t *ops);
void kext_compress_unregister(const char *name);
bool kext_compress_is_loaded(void);

#endif /* KEXT_COMPRESS_H */
