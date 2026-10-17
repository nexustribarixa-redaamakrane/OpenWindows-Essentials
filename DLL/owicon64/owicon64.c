/*
 * owicon64.c - OpenWindows Icon/Bitmap Resource Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owicon64.h"

/* ── Internal state ───────────────────────────────────────────── */

typedef struct {
    uint32_t       id;
    char           name[OWICON_NAME_MAX];
    uint32_t       width;
    uint32_t       height;
    uint8_t        bpp;
    uint8_t        flags;
    uint16_t       reserved;
    uint32_t       stride;
    const uint8_t *pixels;
    bool           owned;   /* pixels point into the static pool */
    bool           used;
} owicon_resource_t;

static bool owicon64_ready = false;
static owicon_resource_t owicon64_table[OWICON_MAX_RESOURCES];

/* Backing store for icons produced by owicon_create(). */
static uint8_t owicon64_pool[OWICON_MAX_RESOURCES][OWICON_POOL_BYTES];

/* ── Internal helpers ─────────────────────────────────────────── */

static size_t owicon_bpp_bytes(uint8_t bpp)
{
    switch (bpp) {
    case 8:  return 1;
    case 16: return 2;
    case 24: return 3;
    case 32: return 4;
    default: return 0;
    }
}

static bool owicon_surface_valid(const owicon_surface_t *fb)
{
    if (!owicon64_ready || !fb || !fb->pixels) return false;
    if (fb->width <= 0 || fb->height <= 0 || fb->stride <= 0) return false;
    if (owicon_bpp_bytes(fb->bpp) == 0) return false;
    return true;
}

static void owicon_store_pixel(uint8_t *pixels, int32_t stride, uint8_t bpp,
                               int32_t x, int32_t y, uint32_t color)
{
    uint8_t *p = pixels + ((size_t)y * (size_t)stride) +
                 ((size_t)x * owicon_bpp_bytes(bpp));

    switch (bpp) {
    case 8:
        p[0] = (uint8_t)(color & 0xFFu);
        break;
    case 16: {
        uint16_t v = (uint16_t)((((color >> 19) & 0x001Fu) << 11) |
                                (((color >> 10) & 0x003Fu) << 5)  |
                                 ((color >> 3)  & 0x001Fu));
        p[0] = (uint8_t)(v & 0x00FFu);
        p[1] = (uint8_t)((v >> 8) & 0x00FFu);
        break;
    }
    case 24:
        p[0] = (uint8_t)(color & 0xFFu);
        p[1] = (uint8_t)((color >> 8) & 0xFFu);
        p[2] = (uint8_t)((color >> 16) & 0xFFu);
        break;
    case 32:
        p[0] = (uint8_t)(color & 0xFFu);
        p[1] = (uint8_t)((color >> 8) & 0xFFu);
        p[2] = (uint8_t)((color >> 16) & 0xFFu);
        p[3] = 0xFFu;
        break;
    default:
        break;
    }
}

static uint32_t owicon_read_pixel(const uint8_t *pixels, int32_t stride,
                                  uint8_t bpp, int32_t x, int32_t y)
{
    const uint8_t *p = pixels + ((size_t)y * (size_t)stride) +
                       ((size_t)x * owicon_bpp_bytes(bpp));

    switch (bpp) {
    case 8:
        return (uint32_t)p[0];
    case 16: {
        uint16_t v = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
        uint32_t r = ((uint32_t)((v >> 11) & 0x001Fu) * 255u) / 31u;
        uint32_t g = ((uint32_t)((v >> 5)  & 0x003Fu) * 255u) / 63u;
        uint32_t b = ((uint32_t)(v & 0x001Fu) * 255u) / 31u;
        return (r << 16) | (g << 8) | b;
    }
    case 24:
    case 32:
        return ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[0];
    default:
        return 0;
    }
}

static void owicon_copy_name(char *dst, const char *src)
{
    uint32_t i = 0;

    if (src) {
        for (i = 0; i < OWICON_NAME_MAX - 1u && src[i] != '\0'; ++i) {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
}

/* Locate a resource slot by id. Returns index or -1. */
static int32_t owicon_find(uint32_t id)
{
    for (uint32_t i = 0; i < OWICON_MAX_RESOURCES; ++i) {
        if (owicon64_table[i].used && owicon64_table[i].id == id) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* Find a free slot, or -1 when the table is full. */
static int32_t owicon_free_slot(void)
{
    for (uint32_t i = 0; i < OWICON_MAX_RESOURCES; ++i) {
        if (!owicon64_table[i].used) return (int32_t)i;
    }
    return -1;
}

/* ── Implementation ──────────────────────────────────────────── */

bool owicon_init(void)
{
    for (uint32_t i = 0; i < OWICON_MAX_RESOURCES; ++i) {
        owicon64_table[i].used = false;
        owicon64_table[i].owned = false;
        owicon64_table[i].id = 0u;
        owicon64_table[i].name[0] = '\0';
    }
    owicon64_ready = true;
    return owicon64_ready;
}

int32_t owicon_load(const owicon_descriptor_t *table, uint32_t count)
{
    if (!owicon64_ready) return -1;
    if (!table || count == 0u || count > OWICON_MAX_RESOURCES) return -1;

    /* Replacing the table drops any previously registered resources. */
    for (uint32_t i = 0; i < OWICON_MAX_RESOURCES; ++i) {
        owicon64_table[i].used = false;
        owicon64_table[i].owned = false;
    }

    uint32_t loaded = 0u;

    for (uint32_t i = 0; i < count; ++i) {
        const owicon_descriptor_t *d = &table[i];
        size_t bytes_per_pixel = owicon_bpp_bytes(d->bpp);

        if (!d->pixels) continue;
        if (d->width == 0u || d->height == 0u) continue;
        if (bytes_per_pixel == 0u) continue;
        if (owicon_find(d->id) >= 0) continue;   /* duplicate id */

        int32_t slot = owicon_free_slot();
        if (slot < 0) break;

        uint32_t min_stride = d->width * (uint32_t)bytes_per_pixel;
        if (d->stride < min_stride) continue;

        owicon_resource_t *r = &owicon64_table[slot];
        r->id       = d->id;
        r->width    = d->width;
        r->height   = d->height;
        r->bpp      = d->bpp;
        r->flags    = 0u;
        r->reserved = 0u;
        r->stride   = d->stride;
        r->pixels   = d->pixels;
        r->owned    = false;
        r->used     = true;
        owicon_copy_name(r->name, d->name);

        ++loaded;
    }

    return (int32_t)loaded;
}

bool owicon_create(uint32_t id, const char *name, uint32_t width,
                   uint32_t height, uint8_t bpp, const uint8_t *pixels)
{
    if (!owicon64_ready) return false;
    if (width == 0u || height == 0u) return false;
    if (width > OWICON_MAX_DIM || height > OWICON_MAX_DIM) return false;
    if (owicon_find(id) >= 0) return false;

    size_t bytes_per_pixel = owicon_bpp_bytes(bpp);
    if (bytes_per_pixel == 0u) return false;

    size_t bytes = (size_t)width * (size_t)height * bytes_per_pixel;
    if (bytes > (size_t)OWICON_POOL_BYTES) return false;

    int32_t slot = owicon_free_slot();
    if (slot < 0) return false;

    uint8_t *dst = owicon64_pool[slot];
    if (pixels) {
        for (size_t i = 0; i < bytes; ++i) dst[i] = pixels[i];
    } else {
        for (size_t i = 0; i < bytes; ++i) dst[i] = 0u;
    }

    owicon_resource_t *r = &owicon64_table[slot];
    r->id       = id;
    r->width    = width;
    r->height   = height;
    r->bpp      = bpp;
    r->flags    = 0u;
    r->reserved = 0u;
    r->stride   = (uint32_t)((size_t)width * bytes_per_pixel);
    r->pixels   = dst;
    r->owned    = true;
    r->used     = true;
    owicon_copy_name(r->name, name);

    return true;
}

bool owicon_unload(uint32_t id)
{
    int32_t slot = owicon_find(id);
    if (slot < 0) return false;

    owicon64_table[slot].used = false;
    owicon64_table[slot].owned = false;
    owicon64_table[slot].pixels = NULL;
    return true;
}

bool owicon_get_size(uint32_t id, uint32_t *out_width, uint32_t *out_height)
{
    int32_t slot = owicon_find(id);
    if (slot < 0 || !out_width || !out_height) return false;

    *out_width  = owicon64_table[slot].width;
    *out_height = owicon64_table[slot].height;
    return true;
}

bool owicon_draw(uint32_t id, owicon_surface_t *dst, int32_t x, int32_t y)
{
    if (!owicon_surface_valid(dst)) return false;

    int32_t slot = owicon_find(id);
    if (slot < 0) return false;

    const owicon_resource_t *r = &owicon64_table[slot];

    for (uint32_t sy = 0; sy < r->height; ++sy) {
        int32_t dy = y + (int32_t)sy;
        if (dy < 0 || dy >= dst->height) continue;

        for (uint32_t sx = 0; sx < r->width; ++sx) {
            int32_t dx = x + (int32_t)sx;
            if (dx < 0 || dx >= dst->width) continue;

            uint32_t color = owicon_read_pixel(r->pixels, (int32_t)r->stride,
                                               r->bpp, (int32_t)sx, (int32_t)sy);
            owicon_store_pixel(dst->pixels, dst->stride, dst->bpp, dx, dy,
                               color);
        }
    }

    return true;
}
