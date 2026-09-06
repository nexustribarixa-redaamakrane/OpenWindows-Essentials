/*
 * cairo64.h - Genuine Cairo core renderer (.owd)
 *
 * Subsystem library delivering the genuine cairo-1.18.2 core as a
 * freestanding OpenWindows module: ARGB32/RGB24 image surfaces, recording
 * surfaces, the software compositor (fill/stroke/paint/mask), toy fonts and
 * user fonts. The public drawing API is the vendor-free ABI mirror in
 * Extensions/cairo64.h (exact copy of the genuine cairo.h family, includes
 * rewritten to Extensions mirrors). The k64-backed allocator and libc/libm
 * interposition shims live in owrt/ (ow_runtime).
 *
 * Conforms to OWD1 binary layout (Extensions/owd_format.h).
 * C99 freestanding - stdint/stdbool/stddef only, zero heap of its own.
 */

#ifndef OWE_CAIRO64_H
#define OWE_CAIRO64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "owd_format.h"
#include "owc_format.h"
#include "cairo64-version.h"

/* ------------------------------------------------------------------ */
/*  OWD1 Binary Header Metadata                                        */
/* ------------------------------------------------------------------ */

#define CAIRO64_LIB_NAME        "cairo64.owd"
#define CAIRO64_LIB_TYPE        OWD_LIBTYPE_HYBRID
#define CAIRO64_TARGET_ARCH     0x02u
#define CAIRO64_ALIGNMENT_LOG2  4u
#define CAIRO64_INIT_FLAGS      (OWC_INIT_REQUIRES_OWRP | \
                                 OWC_INIT_REQUIRES_BANC)

/* ------------------------------------------------------------------ */
/*  Status Codes - BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A7FF): Fatal module faults                    */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal degradations                 */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                     */
/* ------------------------------------------------------------------ */

typedef uint32_t cairo64_status_t;

#define CAIRO64_OK                         0x00000000u  /* Success        */
/* B+ Fatal */
#define CAIRO64_BAN_K64_BOOT               0x0011A200u  /* k64 API dead   */
#define CAIRO64_BAN_ALLOC_CORRUPT          0x0011A201u  /* heap corrupted */
#define CAIRO64_BAN_SURFACE_FAULT          0x0011A202u  /* renderer fault */
/* W+ Warning */
#define CAIRO64_ERR_RENDER_DEGRADED        0x0011AA20u  /* clipped/limit  */
/* S+ Soft */
#define CAIRO64_ERR_UNINITIALIZED          0x0011AE20u  /* Not init yet   */
#define CAIRO64_ERR_CONTRADICTORY_STATE    0x0011AE21u  /* Bad lifecycle  */

/* ------------------------------------------------------------------ */
/*  Module Lifecycle                                                   */
/* ------------------------------------------------------------------ */

/*
 * Initialize the cairo64 runtime: bring up the k64 base API (owning the
 * owrt allocator's slab supply) and mark the module initialized.
 * Idempotent; returns CAIRO64_OK once initialized.
 */
cairo64_status_t cairo64_module_init(void);

/*
 * Tear down module state (idempotent). The owrt allocator keeps its idle
 * slab cache warm across shutdown for fast library reloads; no backing
 * memory is handed back until k64 decides to reclaim it.
 */
cairo64_status_t cairo64_module_shutdown(void);

/* ------------------------------------------------------------------ */
/*  Version / Identity                                                 */
/* ------------------------------------------------------------------ */

/* CAIRO64_VERSION_ENCODE of this build. */
uint32_t cairo64_abi_version(void);

/* Major part of the cairo ABI mirror (1). */
uint16_t cairo64_abi_major(void);

/* Minor part of the cairo ABI mirror (18). */
uint16_t cairo64_abi_minor(void);

/* "cairo64.owd <major>.<minor>.<micro>" identifier. */
const uint8_t *cairo64_ident(void);

#endif /* OWE_CAIRO64_H */