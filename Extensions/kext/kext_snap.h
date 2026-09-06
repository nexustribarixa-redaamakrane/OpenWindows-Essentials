/*
 * kext_snap.h - OpenWindows Filesystem Snapshot Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_SNAP_H
#define KEXT_SNAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_SNAP_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_snap_ops_t;

void kext_snap_register(const kext_snap_ops_t *ops);
void kext_snap_unregister(const char *name);
bool kext_snap_is_loaded(void);

#endif /* KEXT_SNAP_H */
