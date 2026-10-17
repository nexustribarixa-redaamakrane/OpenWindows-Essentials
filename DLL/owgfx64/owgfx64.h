/*
 * owgfx64.h - OpenWindows Graphics Primitives Library (.owd)
 *
 * CPU-side framebuffer rasterizer. Draws Bresenham lines, rectangle
 * outlines, midpoint circles, solid fills and clipped bitmap blits into a
 * caller-provided 8/16/24/32-bpp surface. No floating point, no heap.
 *
 * Colors are packed 0x00RRGGBB; the rasterizer converts to the surface
 * depth on write.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWGFX64_H
#define OWGFX64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Public types ─────────────────────────────────────────────── */

/* Destination surface. `stride` is the byte distance between the first
 * pixel of consecutive rows and may exceed width * bytes-per-pixel. */
typedef struct {
    int32_t   width;
    int32_t   height;
    int32_t   stride;
    uint8_t   bpp;          /* 8, 16, 24 or 32 bits per pixel            */
    uint8_t   reserved[3];
    uint8_t  *pixels;
} owgfx_surface_t;

/* Read-only source bitmap for blits; layout matches owgfx_surface_t. */
typedef struct {
    int32_t        width;
    int32_t        height;
    int32_t        stride;
    uint8_t        bpp;
    uint8_t        reserved[3];
    const uint8_t *pixels;
} owgfx_bitmap_t;

/* Packed 0x00RRGGBB color. */
typedef uint32_t owgfx_color_t;

/* Build a packed color from 8-bit components. */
#define OWGFX_RGB(r, g, b) \
    ((owgfx_color_t)((((uint32_t)(r) & 0xFFu) << 16) | \
                     (((uint32_t)(g) & 0xFFu) << 8)  | \
                      ((uint32_t)(b) & 0xFFu)))

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize the rasterizer. Returns true on success. Every drawing call
 * is a no-op until this succeeds. */
bool owgfx_init(void);

/* Write one clipped pixel. Returns true when the pixel was inside. */
bool owgfx_put_pixel(owgfx_surface_t *fb, int32_t x, int32_t y,
                     owgfx_color_t color);

/* Read one pixel as 0x00RRGGBB; out-of-range reads return 0. */
owgfx_color_t owgfx_get_pixel(const owgfx_surface_t *fb, int32_t x, int32_t y);

/* Draw a 1-pixel-wide Bresenham line from (x0,y0) to (x1,y1). */
void owgfx_draw_line(owgfx_surface_t *fb, int32_t x0, int32_t y0,
                     int32_t x1, int32_t y1, owgfx_color_t color);

/* Draw a 1-pixel-wide rectangle outline. w/h are the outer dimensions. */
void owgfx_draw_rect(owgfx_surface_t *fb, int32_t x, int32_t y,
                     int32_t w, int32_t h, owgfx_color_t color);

/* Draw a midpoint circle outline. */
void owgfx_draw_circle(owgfx_surface_t *fb, int32_t cx, int32_t cy,
                       int32_t radius, owgfx_color_t color);

/* Fill the rectangle [x, x+w) x [y, y+h) with a solid color. */
void owgfx_fill_rect(owgfx_surface_t *fb, int32_t x, int32_t y,
                     int32_t w, int32_t h, owgfx_color_t color);

/* Copy a bitmap with the top-left corner at (x, y). Source pixels of any
 * supported depth are converted to the destination depth and clipped. */
void owgfx_blit_bitmap(owgfx_surface_t *fb, int32_t x, int32_t y,
                       const owgfx_bitmap_t *src);

/* Color-keyed blit: source pixels equal to `key` are skipped. */
void owgfx_blit_bitmap_keyed(owgfx_surface_t *fb, int32_t x, int32_t y,
                             const owgfx_bitmap_t *src, owgfx_color_t key);

#endif /* OWGFX64_H */
