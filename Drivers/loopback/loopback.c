/*
 * loopback.c - OpenWindows Loopback Block Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation: the device table lives in
 * the caller-owned manager and buffers are caller-provided.
 */

#include "loopback.h"

/* ── Tiny local helpers ───────────────────────────────────────── */

static void ow_memset(void *dst, int value, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < len; ++i) {
        d[i] = (uint8_t)value;
    }
}

static void ow_memcpy(void *dst, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

/* ── Device table lookup ──────────────────────────────────────── */

static loop_device_t *loop_find(loop_manager_t *mgr, uint32_t id)
{
    for (uint32_t i = 0u; i < LOOP_MAX_DEVICES; ++i) {
        if (mgr->devices[i].used && (mgr->devices[i].id == id)) {
            return &mgr->devices[i];
        }
    }
    return NULL;
}

/* Claim a free slot and assign it a fresh identifier. */
static loop_device_t *loop_alloc(loop_manager_t *mgr)
{
    for (uint32_t i = 0u; i < LOOP_MAX_DEVICES; ++i) {
        loop_device_t *dev = &mgr->devices[i];

        if (!dev->used) {
            ow_memset(dev, 0, sizeof(*dev));
            dev->used = true;
            dev->id = mgr->next_id;
            mgr->next_id++;
            if (mgr->next_id == LOOP_INVALID_ID) {
                mgr->next_id = 1u;
            }
            return dev;
        }
    }
    return NULL;
}

/* ── Public API ───────────────────────────────────────────────── */

loop_status_t loop_init(loop_manager_t *mgr)
{
    if (mgr == NULL) {
        return LOOP_ERR_INVALID_PARAM;
    }
    ow_memset(mgr, 0, sizeof(*mgr));
    mgr->next_id = 1u;
    mgr->initialized = true;
    return LOOP_OK;
}

loop_status_t loop_attach(loop_manager_t *mgr, void *mem, uint64_t sector_count,
                          bool writable, uint32_t *out_id)
{
    loop_device_t *dev;

    if ((mgr == NULL) || !mgr->initialized) {
        return LOOP_ERR_NOT_INITIALIZED;
    }
    if ((mem == NULL) || (sector_count == 0u)) {
        return LOOP_ERR_INVALID_PARAM;
    }

    dev = loop_alloc(mgr);
    if (dev == NULL) {
        return LOOP_ERR_NO_SPACE;
    }

    dev->mem = mem;
    dev->has_backend = false;
    dev->writable = writable;
    dev->sector_count = sector_count;

    if (out_id != NULL) {
        *out_id = dev->id;
    }
    return LOOP_OK;
}

loop_status_t loop_attach_backend(loop_manager_t *mgr,
                                  const loop_backend_t *backend,
                                  uint64_t sector_count, bool writable,
                                  uint32_t *out_id)
{
    loop_device_t *dev;

    if ((mgr == NULL) || !mgr->initialized) {
        return LOOP_ERR_NOT_INITIALIZED;
    }
    if ((backend == NULL) || (sector_count == 0u)) {
        return LOOP_ERR_INVALID_PARAM;
    }
    if ((backend->read == NULL) && (backend->write == NULL)) {
        return LOOP_ERR_INVALID_PARAM;
    }

    dev = loop_alloc(mgr);
    if (dev == NULL) {
        return LOOP_ERR_NO_SPACE;
    }

    dev->backend = *backend;
    dev->has_backend = true;
    dev->writable = writable;
    dev->sector_count = sector_count;

    if (out_id != NULL) {
        *out_id = dev->id;
    }
    return LOOP_OK;
}

loop_status_t loop_detach(loop_manager_t *mgr, uint32_t id)
{
    loop_device_t *dev;

    if ((mgr == NULL) || !mgr->initialized) {
        return LOOP_ERR_NOT_INITIALIZED;
    }
    dev = loop_find(mgr, id);
    if (dev == NULL) {
        return LOOP_ERR_NO_DEVICE;
    }
    ow_memset(dev, 0, sizeof(*dev));
    return LOOP_OK;
}

loop_status_t loop_read(loop_manager_t *mgr, uint32_t id, uint64_t lba,
                        uint32_t count, void *buf, size_t buf_size)
{
    loop_device_t *dev;
    uint64_t bytes;

    if ((mgr == NULL) || !mgr->initialized) {
        return LOOP_ERR_NOT_INITIALIZED;
    }
    if ((buf == NULL) || (count == 0u)) {
        return LOOP_ERR_INVALID_PARAM;
    }
    dev = loop_find(mgr, id);
    if (dev == NULL) {
        return LOOP_ERR_NO_DEVICE;
    }

    if ((lba >= dev->sector_count)
        || ((uint64_t)count > (dev->sector_count - lba))) {
        return LOOP_ERR_OUT_OF_RANGE;
    }
    bytes = (uint64_t)count * (uint64_t)LOOP_SECTOR_SIZE;
    if (bytes > (uint64_t)buf_size) {
        return LOOP_ERR_INVALID_PARAM;
    }

    if (dev->has_backend) {
        if (dev->backend.read == NULL) {
            return LOOP_ERR_IO;
        }
        if (dev->backend.read(dev->backend.ctx, lba, count, buf) != 0) {
            return LOOP_ERR_IO;
        }
        return LOOP_OK;
    }

    ow_memcpy(buf, (const uint8_t *)dev->mem + (lba * LOOP_SECTOR_SIZE),
              (size_t)bytes);
    return LOOP_OK;
}

loop_status_t loop_write(loop_manager_t *mgr, uint32_t id, uint64_t lba,
                         uint32_t count, const void *buf, size_t buf_size)
{
    loop_device_t *dev;
    uint64_t bytes;

    if ((mgr == NULL) || !mgr->initialized) {
        return LOOP_ERR_NOT_INITIALIZED;
    }
    if ((buf == NULL) || (count == 0u)) {
        return LOOP_ERR_INVALID_PARAM;
    }
    dev = loop_find(mgr, id);
    if (dev == NULL) {
        return LOOP_ERR_NO_DEVICE;
    }
    if (!dev->writable) {
        return LOOP_ERR_READ_ONLY;
    }

    if ((lba >= dev->sector_count)
        || ((uint64_t)count > (dev->sector_count - lba))) {
        return LOOP_ERR_OUT_OF_RANGE;
    }
    bytes = (uint64_t)count * (uint64_t)LOOP_SECTOR_SIZE;
    if (bytes > (uint64_t)buf_size) {
        return LOOP_ERR_INVALID_PARAM;
    }

    if (dev->has_backend) {
        if (dev->backend.write == NULL) {
            return LOOP_ERR_IO;
        }
        if (dev->backend.write(dev->backend.ctx, lba, count, buf) != 0) {
            return LOOP_ERR_IO;
        }
        return LOOP_OK;
    }

    ow_memcpy((uint8_t *)dev->mem + (lba * LOOP_SECTOR_SIZE), buf,
              (size_t)bytes);
    return LOOP_OK;
}
