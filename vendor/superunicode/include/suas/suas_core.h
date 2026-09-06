#ifndef SUAS_SUAS_CORE_H
#define SUAS_SUAS_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── SUAS Version & Identification ─────────────────────────────── */

#ifndef SUAS_VERSION_MAJOR
#define SUAS_VERSION_MAJOR 0
#endif
#ifndef SUAS_VERSION_MINOR
#define SUAS_VERSION_MINOR 1
#endif
#ifndef SUAS_VERSION_PATCH
#define SUAS_VERSION_PATCH 0
#endif

/* ── Freestanding Compatibility ─────────────────────────────────── */

/* SUAS targets freestanding C99 environments.
 * All headers used here are part of the C99 freestanding subset:
 *   <stdint.h>, <stdbool.h>, <stddef.h> */

#ifndef SUAS_FREESTANDING
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0
#define SUAS_FREESTANDING 1
#else
#define SUAS_FREESTANDING 0
#endif
#endif

/* ── Architecture Invariant Constants ───────────────────────────── */

#ifndef SUAS_CODEPOINT_BITS
#define SUAS_CODEPOINT_BITS 31
#endif

#ifndef SUAS_CODEPOINT_MAX
#define SUAS_CODEPOINT_MAX 0x7FFFFFFFUL
#endif

#ifndef SUAS_ZONE_UNICODE_MIN
#define SUAS_ZONE_UNICODE_MIN 0x00000000UL
#endif
#ifndef SUAS_ZONE_UNICODE_MAX
#define SUAS_ZONE_UNICODE_MAX 0x0010FFFFUL
#endif

#ifndef SUAS_ZONE_SCP_MIN
#define SUAS_ZONE_SCP_MIN 0x00110000UL
#endif
#ifndef SUAS_ZONE_SCP_MAX
#define SUAS_ZONE_SCP_MAX 0x0011FFFFUL
#endif

#ifndef SUAS_ZONE_NATIVE_MIN
#define SUAS_ZONE_NATIVE_MIN 0x00120000UL
#endif
#ifndef SUAS_ZONE_NATIVE_MAX
#define SUAS_ZONE_NATIVE_MAX 0x7FFFFFFEUL
#endif

/* ── Status Codes ───────────────────────────────────────────────── */

typedef enum {
    SUAS_OK                      =  0,
    SUAS_ERR_NOT_INITIALIZED     = -1,
    SUAS_ERR_INVALID_ARGUMENT    = -2,
    SUAS_ERR_OUT_OF_MEMORY       = -3,
    SUAS_ERR_UNSUPPORTED         = -4
} suas_status_t;

/* ── Placeholder Structures ─────────────────────────────────────── */

/**
 * Opaque kernel context descriptor.
 * Actual fields are defined by conforming implementations.
 */
typedef struct suas_kernel suas_kernel_t;

/* ── Module Interface Stubs ─────────────────────────────────────── */

/**
 * Initializes a SUAS kernel context.
 * Stub — implementation pending SUAS-001.
 */
suas_status_t suas_kernel_init(suas_kernel_t* kernel);

/**
 * Returns the SUAS specification version.
 * Stub — implementation pending SUAS-001.
 */
suas_status_t suas_kernel_version(uint32_t* out_major,
                                  uint32_t* out_minor,
                                  uint32_t* out_patch);

#ifdef __cplusplus
}
#endif

#endif /* SUAS_SUAS_CORE_H */
