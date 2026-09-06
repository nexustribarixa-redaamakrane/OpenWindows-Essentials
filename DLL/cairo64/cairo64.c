/*
 * cairo64.c - Genuine Cairo core renderer (.owd) module core
 *
 * The drawing surface of this module is the genuine cairo-1.18.5 core,
 * linked verbatim from vendor/cairo (libcairo64_vendor.a) over pixman
 * (libpixman_vendor.a) on top of the owrt freestanding runtime. Those
 * archives resolve libc/math against owrt and memory against k64.
 *
 * This translation unit provides only the module facade: identity, ABI
 * version commitment, and lifecycle (heap bootstrap through k64).
 *
 * C99 freestanding, strict profile (-Wall -Wextra -Werror -Wpedantic).
 */

#include "cairo64.h"
#include "kernel64.h"

static bool cairo64_initialized = false;

cairo64_status_t cairo64_module_init(void)
{
    if (cairo64_initialized) {
        return CAIRO64_OK;
    }
    if (k64_initialize_api() != K64_OK) {
        return CAIRO64_BAN_K64_BOOT;
    }
    cairo64_initialized = true;
    return CAIRO64_OK;
}

cairo64_status_t cairo64_module_shutdown(void)
{
    if (!cairo64_initialized) {
        return CAIRO64_ERR_UNINITIALIZED;
    }
    cairo64_initialized = false;
    return CAIRO64_OK;
}

uint32_t cairo64_abi_version(void)
{
    return (uint32_t)((CAIRO_VERSION_MAJOR * 10000u) +
                      (CAIRO_VERSION_MINOR *    100u) +
                      (CAIRO_VERSION_MICRO *      1u));
}

uint16_t cairo64_abi_major(void)
{
    return (uint16_t)CAIRO_VERSION_MAJOR;
}

uint16_t cairo64_abi_minor(void)
{
    return (uint16_t)CAIRO_VERSION_MINOR;
}

const uint8_t *cairo64_ident(void)
{
    static const uint8_t id[] = "cairo64.owd 1.18.5";
    return id;
}

/*
 * PE DLL entry for the .owd (GNU ld needs an entry address). The module
 * ships no startup/CRT; leaving a real entry that may be invoked by the
 * loader could touch state before the host bootstraps k64, so this is a
 * pure "attach OK" stub that runs nothing.
 */
#if defined(_WIN32)
unsigned long __stdcall owd64_dll_ep(void)
{
    return 1ul;
}
#endif