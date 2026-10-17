/*
 * loopback.h - OpenWindows Loopback Block Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef LOOPBACK_H
#define LOOPBACK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Logical sector size presented by every loopback device. */
#define LOOP_SECTOR_SIZE     512u
/* Number of concurrent loopback devices in the static table. */
#define LOOP_MAX_DEVICES     8u
/* Default device identifier returned to callers. */
#define LOOP_INVALID_ID      0xFFFFFFFFu

/* Status codes returned by every loopback entry point. */
typedef enum {
    LOOP_OK = 0,
    LOOP_ERR_INVALID_PARAM,
    LOOP_ERR_NOT_INITIALIZED,
    LOOP_ERR_NO_DEVICE,
    LOOP_ERR_NO_SPACE,
    LOOP_ERR_OUT_OF_RANGE,
    LOOP_ERR_READ_ONLY,
    LOOP_ERR_IO
} loop_status_t;

/* Pluggable backend vtable: either callback may be NULL for unsupported
 * operations. Both return 0 on success and nonzero on failure. */
typedef struct {
    int32_t (*read)(void *ctx, uint64_t lba, uint32_t count, void *buf);
    int32_t (*write)(void *ctx, uint64_t lba, uint32_t count, const void *buf);
    void    *ctx;
} loop_backend_t;

/* One entry in the static device table. */
typedef struct {
    bool           used;
    uint32_t       id;
    bool           writable;
    uint64_t       sector_count;
    void          *mem;         /* memory-backed storage (NULL for backends) */
    loop_backend_t backend;     /* pluggable callbacks                       */
    bool           has_backend;
} loop_device_t;

/* Loopback manager holding the static device table. */
typedef struct {
    loop_device_t devices[LOOP_MAX_DEVICES];
    uint32_t      next_id;
    bool          initialized;
} loop_manager_t;

/* Initialise the manager and clear the device table. */
loop_status_t loop_init(loop_manager_t *mgr);

/* Bind a caller-owned memory buffer as a read/write block device. */
loop_status_t loop_attach(loop_manager_t *mgr, void *mem, uint64_t sector_count,
                          bool writable, uint32_t *out_id);

/* Bind a pluggable backend vtable as a block device. */
loop_status_t loop_attach_backend(loop_manager_t *mgr,
                                  const loop_backend_t *backend,
                                  uint64_t sector_count, bool writable,
                                  uint32_t *out_id);

/* Release a device slot by identifier. */
loop_status_t loop_detach(loop_manager_t *mgr, uint32_t id);

/* Read `count` sectors from device `id` into `buf`. */
loop_status_t loop_read(loop_manager_t *mgr, uint32_t id, uint64_t lba,
                        uint32_t count, void *buf, size_t buf_size);

/* Write `count` sectors to device `id` from `buf`. */
loop_status_t loop_write(loop_manager_t *mgr, uint32_t id, uint64_t lba,
                         uint32_t count, const void *buf, size_t buf_size);

#endif /* LOOPBACK_H */
