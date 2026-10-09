/*
 * kext_trace.h - OpenWindows Execution Tracer Extension
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KEXT_TRACE_H
#define KEXT_TRACE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_TRACE_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} kext_trace_ops_t;

void kext_trace_register(const kext_trace_ops_t *ops);
void kext_trace_unregister(const char *name);
bool kext_trace_is_loaded(void);

#endif /* KEXT_TRACE_H */
