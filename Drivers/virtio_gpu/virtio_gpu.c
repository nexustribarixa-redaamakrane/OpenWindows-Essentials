/*
 * virtio_gpu.c - OpenWindows VirtIO GPU Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "virtio_gpu.h"

/* ── Internal state ───────────────────────────────────────────── */

static const virtgpu_transport_t *virtgpu_transport = NULL;
static virtgpu_display_info_t     virtgpu_display;
static virtgpu_2d_resource_t      virtgpu_resources[VIRTGPU_MAX_RESOURCES];
static uint32_t                   virtgpu_res_count = 0u;
static bool                       virtgpu_initialized = false;

/* ── Helpers ──────────────────────────────────────────────────── */

/* Byte-wise clear of a caller buffer. */
static void ow_memset(void *dst, uint8_t value, size_t len)
{
    uint8_t *p = (uint8_t *)dst;
    size_t i;
    for (i = 0u; i < len; ++i) {
        p[i] = value;
    }
}

/* Bytes per pixel for a supported virtio-gpu format, or 0. */
static uint32_t virtgpu_format_bpp(uint32_t format)
{
    switch (format) {
    case VIRTGPU_FORMAT_B8G8R8A8_UNORM:
    case VIRTGPU_FORMAT_B8G8R8X8_UNORM:
    case VIRTGPU_FORMAT_A8R8G8B8_UNORM:
    case VIRTGPU_FORMAT_X8R8G8B8_UNORM:
    case VIRTGPU_FORMAT_R8G8B8A8_UNORM:
        return 4u;
    case VIRTGPU_FORMAT_B5G6R5_UNORM:
        return 2u;
    default:
        return 0u;
    }
}

/* Locate a registered resource index by id. Returns index or -1. */
static int32_t virtgpu_find(uint32_t resource_id)
{
    uint32_t i;
    for (i = 0u; i < virtgpu_res_count; ++i) {
        if (virtgpu_resources[i].resource_id == resource_id) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* Clamp a box to the extent of a resource. Returns false if empty. */
static bool virtgpu_clip_box(const virtgpu_2d_resource_t *res,
                             virtgpu_box_t *box)
{
    uint32_t x1 = box->x + box->width;
    uint32_t y1 = box->y + box->height;

    if (box->width == 0u || box->height == 0u) {
        return false;
    }
    if (x1 > res->width)  { x1 = res->width; }
    if (y1 > res->height) { y1 = res->height; }
    if (x1 <= box->x || y1 <= box->y) {
        return false;
    }
    box->width  = x1 - box->x;
    box->height = y1 - box->y;
    return true;
}

/* ── Implementation ───────────────────────────────────────────── */

virtgpu_status_t virtgpu_init(const virtgpu_transport_t *transport)
{
    if (transport == NULL) {
        return VIRTGPU_ERR_INVALID_PARAM;
    }
    if (transport->get_display_info == NULL ||
        transport->resource_create_2d == NULL ||
        transport->transfer_to_host_2d == NULL ||
        transport->resource_flush == NULL) {
        return VIRTGPU_ERR_NO_TRANSPORT;
    }

    ow_memset(&virtgpu_display, 0, sizeof(virtgpu_display));
    ow_memset(virtgpu_resources, 0, sizeof(virtgpu_resources));
    virtgpu_res_count = 0u;
    virtgpu_transport = transport;
    virtgpu_initialized = true;

    if (!virtgpu_transport->get_display_info(virtgpu_transport->ctx,
                                             &virtgpu_display)) {
        return VIRTGPU_ERR_DEVICE;
    }
    return VIRTGPU_OK;
}

virtgpu_status_t virtgpu_create_resource(uint32_t resource_id, uint32_t format,
                                         uint32_t width, uint32_t height,
                                         void *backing, size_t backing_size)
{
    virtgpu_2d_resource_t *res;
    uint32_t bpp;
    size_t required;

    if (!virtgpu_initialized) {
        return VIRTGPU_ERR_NOT_INITIALIZED;
    }
    if (width == 0u || height == 0u) {
        return VIRTGPU_ERR_INVALID_PARAM;
    }
    if (backing == NULL) {
        return VIRTGPU_ERR_INVALID_PARAM;
    }
    if (virtgpu_find(resource_id) >= 0) {
        return VIRTGPU_ERR_ALREADY_EXISTS;
    }
    if (virtgpu_res_count >= VIRTGPU_MAX_RESOURCES) {
        return VIRTGPU_ERR_TABLE_FULL;
    }

    bpp = virtgpu_format_bpp(format);
    if (bpp == 0u) {
        return VIRTGPU_ERR_INVALID_PARAM;
    }

    required = (size_t)width * (size_t)bpp * (size_t)height;
    if (backing_size < required) {
        return VIRTGPU_ERR_TOO_SMALL;
    }

    /* Ask the host to allocate the 2D resource before recording it. */
    if (!virtgpu_transport->resource_create_2d(virtgpu_transport->ctx,
                                               resource_id, format,
                                               width, height)) {
        return VIRTGPU_ERR_DEVICE;
    }

    res = &virtgpu_resources[virtgpu_res_count];
    res->resource_id     = resource_id;
    res->format          = format;
    res->width           = width;
    res->height          = height;
    res->bytes_per_pixel = bpp;
    res->row_stride      = width * bpp;
    res->backing         = backing;
    res->backing_size    = backing_size;
    res->attached        = true;
    virtgpu_res_count++;
    return VIRTGPU_OK;
}

virtgpu_status_t virtgpu_transfer(uint32_t resource_id,
                                  const virtgpu_box_t *box)
{
    virtgpu_box_t clipped;
    int32_t idx;
    virtgpu_2d_resource_t *res;

    if (!virtgpu_initialized) {
        return VIRTGPU_ERR_NOT_INITIALIZED;
    }
    if (box == NULL) {
        return VIRTGPU_ERR_INVALID_PARAM;
    }

    idx = virtgpu_find(resource_id);
    if (idx < 0) {
        return VIRTGPU_ERR_NO_RESOURCE;
    }
    res = &virtgpu_resources[(uint32_t)idx];

    /* The backing store is attached at creation time; it must be live. */
    if (!res->attached || res->backing == NULL) {
        return VIRTGPU_ERR_INVALID_PARAM;
    }

    clipped = *box;
    if (!virtgpu_clip_box(res, &clipped)) {
        return VIRTGPU_ERR_INVALID_PARAM;
    }

    if (!virtgpu_transport->transfer_to_host_2d(virtgpu_transport->ctx,
                                                resource_id, &clipped)) {
        return VIRTGPU_ERR_DEVICE;
    }
    return VIRTGPU_OK;
}

virtgpu_status_t virtgpu_flush(uint32_t resource_id,
                               const virtgpu_box_t *box)
{
    virtgpu_box_t clipped;
    int32_t idx;
    virtgpu_2d_resource_t *res;

    if (!virtgpu_initialized) {
        return VIRTGPU_ERR_NOT_INITIALIZED;
    }
    if (box == NULL) {
        return VIRTGPU_ERR_INVALID_PARAM;
    }

    idx = virtgpu_find(resource_id);
    if (idx < 0) {
        return VIRTGPU_ERR_NO_RESOURCE;
    }
    res = &virtgpu_resources[(uint32_t)idx];

    clipped = *box;
    if (!virtgpu_clip_box(res, &clipped)) {
        return VIRTGPU_ERR_INVALID_PARAM;
    }

    if (!virtgpu_transport->resource_flush(virtgpu_transport->ctx,
                                           resource_id, &clipped)) {
        return VIRTGPU_ERR_DEVICE;
    }
    return VIRTGPU_OK;
}

bool virtgpu_get_display_info(virtgpu_display_info_t *out_info)
{
    if (out_info == NULL || !virtgpu_initialized) {
        return false;
    }
    *out_info = virtgpu_display;
    return true;
}

bool virtgpu_get_resource(uint32_t resource_id, virtgpu_2d_resource_t *out)
{
    int32_t idx;

    if (out == NULL || !virtgpu_initialized) {
        return false;
    }
    idx = virtgpu_find(resource_id);
    if (idx < 0) {
        return false;
    }
    *out = virtgpu_resources[(uint32_t)idx];
    return true;
}

uint32_t virtgpu_resource_count(void)
{
    return virtgpu_res_count;
}
