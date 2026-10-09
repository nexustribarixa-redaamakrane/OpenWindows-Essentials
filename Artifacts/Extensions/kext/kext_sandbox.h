/*
 * kext_sandbox.h - OpenWindows Process Sandbox Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_SANDBOX_H
#define KEXT_SANDBOX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_SANDBOX_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_sandbox_ops_t;

void kext_sandbox_register(const kext_sandbox_ops_t *ops);
void kext_sandbox_unregister(const char *name);
bool kext_sandbox_is_loaded(void);

#endif /* KEXT_SANDBOX_H */
