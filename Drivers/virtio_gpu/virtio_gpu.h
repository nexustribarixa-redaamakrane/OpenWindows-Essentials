/*
 * virtio_gpu.h - OpenWindows VirtIO GPU Device (.owc)
 *
 * 2D command model for a VirtIO-GPU device driven through a pluggable
 * transport vtable. The driver owns resource bookkeeping and argument
 * validation; the transport performs the actual virtqueue hand-off.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef VIRTIO_GPU_H
#define VIRTIO_GPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t virtgpu_status_t;

#define VIRTGPU_OK                  0x00000000u  /* Success           */
#define VIRTGPU_ERR_INVALID_PARAM   0x0011C000u  /* Bad pointer/args  */
#define VIRTGPU_ERR_NOT_INITIALIZED 0x0011C001u  /* init() not run    */
#define VIRTGPU_ERR_NO_TRANSPORT    0x0011C002u  /* Missing vtable fn */
#define VIRTGPU_ERR_TABLE_FULL      0x0011C003u  /* Resource table    */
#define VIRTGPU_ERR_NO_RESOURCE     0x0011C004u  /* Unknown id        */
#define VIRTGPU_ERR_ALREADY_EXISTS  0x0011C005u  /* Duplicate id      */
#define VIRTGPU_ERR_TOO_SMALL       0x0011C006u  /* Backing too small */
#define VIRTGPU_ERR_DEVICE          0x0011C007u  /* Transport failed  */

/* ── Capacity limits ──────────────────────────────────────────── */

#define VIRTGPU_MAX_RESOURCES       16u          /* Static table size */

/* ── Pixel formats (virtio-gpu half of the DRM fourcc space) ──── */

typedef enum {
    VIRTGPU_FORMAT_B8G8R8A8_UNORM = 1,  /* 32bpp BGRA                */
    VIRTGPU_FORMAT_B8G8R8X8_UNORM = 2,  /* 32bpp BGRX (alpha pad)    */
    VIRTGPU_FORMAT_A8R8G8B8_UNORM = 3,  /* 32bpp ARGB                */
    VIRTGPU_FORMAT_X8R8G8B8_UNORM = 4,  /* 32bpp XRGB                */
    VIRTGPU_FORMAT_R8G8B8A8_UNORM = 67, /* 32bpp RGBA                */
    VIRTGPU_FORMAT_B5G6R5_UNORM   = 131 /* 16bpp 5-6-5               */
} virtgpu_format_t;

/* ── Geometry ─────────────────────────────────────────────────── */

/* Axis-aligned box in host pixel coordinates. */
typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} virtgpu_box_t;

/* Current output (scanout) geometry reported by the device. */
typedef struct {
    uint32_t width;     /* Active scanout width in pixels  */
    uint32_t height;    /* Active scanout height in pixels */
    uint32_t enabled;   /* Non-zero when a scanout is live */
} virtgpu_display_info_t;

/* ── 2D resource descriptor ───────────────────────────────────── */

typedef struct {
    uint32_t   resource_id;   /* Guest-assigned 2D resource id        */
    uint32_t   format;        /* virtgpu_format_t value               */
    uint32_t   width;         /* Resource width in pixels             */
    uint32_t   height;        /* Resource height in pixels            */
    uint32_t   bytes_per_pixel;/* Derived from format                 */
    uint32_t   row_stride;    /* width * bytes_per_pixel              */
    void      *backing;       /* Guest backing store for the resource  */
    size_t     backing_size;  /* Backing store size in bytes           */
    bool       attached;      /* Backing has been bound                */
} virtgpu_2d_resource_t;

/* ── Transport vtable ─────────────────────────────────────────── */

/* Callbacks implemented by the virtio transport (PCI/MMIO/virtqueue). */
typedef struct {
    void *ctx;   /* Opaque transport context                          */

    /* Fetch current display geometry. */
    bool (*get_display_info)(void *ctx, virtgpu_display_info_t *out);

    /* Create a 2D host resource. */
    bool (*resource_create_2d)(void *ctx, uint32_t resource_id,
                               uint32_t format, uint32_t width, uint32_t height);

    /* Transfer guest backing content to the host for a box. */
    bool (*transfer_to_host_2d)(void *ctx, uint32_t resource_id,
                                const virtgpu_box_t *box);

    /* Flush a dirty box of a resource to the scanout. */
    bool (*resource_flush)(void *ctx, uint32_t resource_id,
                           const virtgpu_box_t *box);
} virtgpu_transport_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Bind a transport, reset the resource table and read display info. */
virtgpu_status_t virtgpu_init(const virtgpu_transport_t *transport);

/* Create and register a 2D resource backed by caller memory. */
virtgpu_status_t virtgpu_create_resource(uint32_t resource_id, uint32_t format,
                                         uint32_t width, uint32_t height,
                                         void *backing, size_t backing_size);

/* Attach (if needed) the backing store and transfer a box to the host. */
virtgpu_status_t virtgpu_transfer(uint32_t resource_id,
                                  const virtgpu_box_t *box);

/* Flush a dirty box of a resource to the scanout. */
virtgpu_status_t virtgpu_flush(uint32_t resource_id,
                               const virtgpu_box_t *box);

/* ── Introspection helpers ────────────────────────────────────── */

bool virtgpu_get_display_info(virtgpu_display_info_t *out_info);

bool virtgpu_get_resource(uint32_t resource_id, virtgpu_2d_resource_t *out);

uint32_t virtgpu_resource_count(void);

#endif /* VIRTIO_GPU_H */
