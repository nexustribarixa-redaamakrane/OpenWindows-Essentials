/*
 * config.h — OpenWindows freestanding cairo-1.18.5 build configuration.
 *
 * Hand-built for the genuine cairo subtree (vendor/cairo) on x86-64
 * Windows with GCC (w64devkit). Mirrors the subset meson would emit for an
 * image-surface-only freestanding build: native uint64 wideints, GCC
 * __atomic builtins, no mutexes, no hosted libc features, debug off.
 */
#ifndef CAIRO_CONFIG_H
#define CAIRO_CONFIG_H

#define PACKAGE "cairo"
#define VERSION "1.18.5"

/* Native 64-bit wideints instead of the struct-pair fallback. */
#define HAVE_UINT64_T 1

/* GCC __atomic builtins (x86-64, lock-free by construction). */
#define HAVE_CXX11_ATOMIC_PRIMITIVES 1

/* Single-threaded by design: no pthreads, no Win32 CRITICAL_SECTION. */
#define CAIRO_NO_MUTEX 1

/* Freestanding sizes (x86-64 Windows LLP64: long is 32-bit). */
#define SIZEOF_VOID_P 8
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_SIZE_T 8

#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_IO_H 1

/* Release build; all CAIRO_DEBUG paths compile out. */
#define NDEBUG 1
#define CAIRO_DEBUG 0

/* No valgrind */
#define HAVE_VALGRIND 0

#endif /* CAIRO_CONFIG_H */