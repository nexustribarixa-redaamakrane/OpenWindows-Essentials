/*
 * kext_profile.h - OpenWindows Performance Profiler Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_PROFILE_H
#define KEXT_PROFILE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_PROFILE_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_profile_ops_t;

void kext_profile_register(const kext_profile_ops_t *ops);
void kext_profile_unregister(const char *name);
bool kext_profile_is_loaded(void);

#endif /* KEXT_PROFILE_H */
