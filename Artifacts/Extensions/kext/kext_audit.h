/*
 * kext_audit.h - OpenWindows Security Audit Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_AUDIT_H
#define KEXT_AUDIT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_AUDIT_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_audit_ops_t;

void kext_audit_register(const kext_audit_ops_t *ops);
void kext_audit_unregister(const char *name);
bool kext_audit_is_loaded(void);

#endif /* KEXT_AUDIT_H */
