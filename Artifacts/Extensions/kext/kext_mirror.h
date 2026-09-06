/*
 * kext_mirror.h - OpenWindows Disk Mirror/RAID Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_MIRROR_H
#define KEXT_MIRROR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_MIRROR_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_mirror_ops_t;

void kext_mirror_register(const kext_mirror_ops_t *ops);
void kext_mirror_unregister(const char *name);
bool kext_mirror_is_loaded(void);

#endif /* KEXT_MIRROR_H */
