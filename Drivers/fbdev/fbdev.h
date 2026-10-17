/*
 * fbdev.h - OpenWindows Framebuffer Device Driver (.owc)
 *
 * Lightweight 2D software framebuffer: per-pixel plotting, rectangle
 * fill, clipped bitmap blit and clear over a caller-provided linear
 * framebuffer with explicit geometry.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef FBDEV_H
#define FBDEV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Pixel formats ────────────────────────────────────────────── */

typedef enum {
    FBDEV_FMT_INDEX8 = 0,   /* 1 byte  : palette index            */
    FBDEV_FMT_RGB565 = 1,   /* 2 bytes : 5-6-5 little-endian      */
    FBDEV_FMT_BGR888 = 2,   /* 3 bytes : B,G,R byte order         */
    FBDEV_FMT_XRGB8888 = 3  /* 4 bytes : B,G,R,X little-endian    */
} fbdev_format_t;

/* Colour value, always packed as 0x00RRGGBB regardless of format. */
typedef uint32_t fbdev_color_t;

#define FBDEV_RGB(r, g, b) \
    ((((uint32_t)(r) & 0xFFu) << 16) | (((uint32_t)(g) & 0xFFu) << 8) | \
      ((uint32_t)(b) & 0xFFu))

/* ── Geometry / descriptors ───────────────────────────────────── */

typedef struct {
    uint32_t width;    /* Visible width in pixels  */
    uint32_t height;   /* Visible height in pixels */
    uint32_t pitch;    /* Bytes per scan line      */
    uint16_t bpp;      /* Bits per pixel: 8/16/24/32 */
} fbdev_geometry_t;

/* Runtime info about the bound framebuffer. */
typedef struct {
    volatile uint8_t *base;      /* Native framebuffer base        */
    uint32_t          width;     /* Visible width in pixels        */
    uint32_t          height;    /* Visible height in pixels       */
    uint32_t          pitch;     /* Bytes per scan line            */
    uint16_t          bpp;       /* Bits per pixel                 */
    fbdev_format_t    format;    /* Derived pixel layout           */
    size_t            size_bytes;/* pitch * height                 */
} fbdev_info_t;

/* Axis-aligned rectangle; negative/overflowing rectangles are clipped. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} fbdev_rect_t;

/* Source bitmap for blitting. */
typedef struct {
    const uint8_t *pixels;  /* Tightly or pitch-strided source rows  */
    uint32_t       width;   /* Source width in pixels                */
    uint32_t       height;  /* Source height in pixels               */
    uint32_t       pitch;   /* Source bytes per scan line            */
    uint16_t       bpp;     /* Source bits per pixel                 */
} fbdev_bitmap_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Bind a framebuffer and validate its geometry. */
bool fbdev_init(volatile uint8_t *base, const fbdev_geometry_t *geo);

/* Plot a single bounds-checked pixel; returns false if out of range. */
bool fbdev_putpixel(int32_t x, int32_t y, fbdev_color_t color);

/* Fill a rectangle (clipped to the surface). Returns pixels written. */
uint32_t fbdev_fill_rect(const fbdev_rect_t *rect, fbdev_color_t color);

/* Blit a source bitmap at (dst_x, dst_y) with clipping. Returns pixels
 * actually written. */
uint32_t fbdev_blit(int32_t dst_x, int32_t dst_y, const fbdev_bitmap_t *src);

/* Fill the whole surface with one colour. */
void fbdev_clear(fbdev_color_t color);

/* Copy runtime framebuffer info into `out_info`. */
bool fbdev_get_info(fbdev_info_t *out_info);

#endif /* FBDEV_H */
