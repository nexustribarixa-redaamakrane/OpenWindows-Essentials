/*
 * kext_netmon.h - OpenWindows Network Monitor Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_NETMON_H
#define KEXT_NETMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_NETMON_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_netmon_ops_t;

void kext_netmon_register(const kext_netmon_ops_t *ops);
void kext_netmon_unregister(const char *name);
bool kext_netmon_is_loaded(void);

#endif /* KEXT_NETMON_H */
