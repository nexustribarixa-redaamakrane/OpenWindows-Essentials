/*
 * kext_hotplug.h - OpenWindows Hot-plug Device Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_HOTPLUG_H
#define KEXT_HOTPLUG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_HOTPLUG_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_hotplug_ops_t;

void kext_hotplug_register(const kext_hotplug_ops_t *ops);
void kext_hotplug_unregister(const char *name);
bool kext_hotplug_is_loaded(void);

#endif /* KEXT_HOTPLUG_H */
