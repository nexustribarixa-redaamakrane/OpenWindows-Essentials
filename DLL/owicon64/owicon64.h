/*
 * owicon64.h - OpenWindows Icon/Bitmap Resource Library (.owd)
 *
 * Maintains a fixed-size table of named icon resources. Resources may be
 * loaded from an in-memory descriptor table (referencing caller pixels) or
 * created into an internal static pool. Icons can be drawn onto any
 * compatible surface with clipped 8/16/24/32-bpp conversion.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWICON64_H
#define OWICON64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Public types ─────────────────────────────────────────────── */

#define OWICON_MAX_RESOURCES 16u  /* icon table slots                  */
#define OWICON_MAX_DIM       64u  /* largest icon edge in pixels       */
#define OWICON_NAME_MAX      32u  /* stored icon name length           */

/* Internal pool bytes reserved per created icon (max dimension at 32bpp). */
#define OWICON_POOL_BYTES (OWICON_MAX_DIM * OWICON_MAX_DIM * 4u)

/* Destination surface (same layout as the graphics library). */
typedef struct {
    int32_t  width;
    int32_t  height;
    int32_t  stride;
    uint8_t  bpp;
    uint8_t  reserved[3];
    uint8_t *pixels;
} owicon_surface_t;

/* Icon resource description supplied by the caller when loading a table.
 * `pixels` must stay valid for as long as the resource is used; `stride`
 * is the byte distance between consecutive rows. */
typedef struct {
    uint32_t       id;
    const char    *name;
    uint32_t       width;
    uint32_t       height;
    uint8_t        bpp;
    uint8_t        reserved[3];
    uint32_t       stride;
    const uint8_t *pixels;
} owicon_descriptor_t;

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize the library and clear the icon table. */
bool owicon_init(void);

/* Replace the icon table with `count` descriptors. Returns the number of
 * resources registered, or -1 when the arguments are invalid. */
int32_t owicon_load(const owicon_descriptor_t *table, uint32_t count);

/* Create an icon by copying `pixels` into the internal static pool. When
 * `pixels` is NULL a zero-filled icon is produced. Returns false when the
 * id already exists, the slot table is full, or the dimensions overflow
 * the pool. */
bool owicon_create(uint32_t id, const char *name, uint32_t width,
                   uint32_t height, uint8_t bpp, const uint8_t *pixels);

/* Remove an icon from the table. */
bool owicon_unload(uint32_t id);

/* Draw an icon with its top-left corner at (x, y), clipped to the target.
 * Returns false when the id is unknown or the surface is invalid. */
bool owicon_draw(uint32_t id, owicon_surface_t *dst, int32_t x, int32_t y);

/* Query an icon's pixel dimensions. */
bool owicon_get_size(uint32_t id, uint32_t *out_width, uint32_t *out_height);

#endif /* OWICON64_H */
