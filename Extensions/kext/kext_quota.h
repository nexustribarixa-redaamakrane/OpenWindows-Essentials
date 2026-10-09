/*
 * kext_quota.h - OpenWindows Disk Quota Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_QUOTA_H
#define KEXT_QUOTA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_QUOTA_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_quota_ops_t;

void kext_quota_register(const kext_quota_ops_t *ops);
void kext_quota_unregister(const char *name);
bool kext_quota_is_loaded(void);

#endif /* KEXT_QUOTA_H */
