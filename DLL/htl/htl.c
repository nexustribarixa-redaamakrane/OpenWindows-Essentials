/*
 * htl.c - Hardware Translation Layer implementation
 *
 * C99 freestanding. No heap allocation; all storage is caller-provided.
 */

#include "htl.h"

/* ================================================================== */
/*  Helpers                                                           */
/* ================================================================== */

static htl_status_t validate_device_idx(const htl_context_t *ctx,
                                        uint32_t device_idx)
{
    if (!ctx) return HTL_ERR_INVALID_PARAM;
    if (device_idx >= ctx->device_count) return HTL_ERR_NO_DEVICE;
    return HTL_OK;
}

static bool device_valid(const htl_device_t *dev)
{
    if (!dev->read_block || !dev->write_block) return false;
    if (dev->block_size == 0 || dev->total_blocks == 0) return false;
    return true;
}

/* Saturating add for uint64_t. */
static uint64_t sat_add_u64(uint64_t a, uint64_t b)
{
    uint64_t r = a + b;
    if (r < a) return UINT64_MAX;
    return r;
}

/* Linear scan cache lookup. Returns index or -1. */
static int32_t cache_find(const htl_cache_t *cache,
                           uint32_t device_idx,
                           uint32_t block_num)
{
    for (uint32_t i = 0; i < cache->count; ++i) {
        htl_cache_entry_t *e = &cache->entries[i];
        if (e->valid && e->device_idx == (uint8_t)device_idx &&
            e->block_num == block_num) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* Insert a new entry into the cache. Evicts oldest clean entry if full. */
static htl_status_t cache_insert(htl_cache_t *cache,
                                  uint32_t device_idx,
                                  uint32_t block_num,
                                  const void *data,
                                  uint32_t access_counter)
{
    if (cache->capacity == 0) return HTL_ERR_INVALID_PARAM;

    /* Find free slot first. */
    for (uint32_t i = 0; i < cache->capacity; ++i) {
        if (!cache->entries[i].valid) {
            cache->entries[i].device_idx    = (uint8_t)device_idx;
            cache->entries[i].block_num     = block_num;
            cache->entries[i].dirty         = 0;
            cache->entries[i].valid         = 1;
            cache->entries[i].access_order  = access_counter;
            if (data) {
                for (uint32_t j = 0; j < HTL_CACHE_DATA_SIZE; ++j) {
                    cache->entries[i].data[j] = ((const uint8_t *)data)[j];
                }
            }
            if (cache->count < cache->capacity) cache->count++;
            return HTL_OK;
        }
    }

    /* No free slot - evict oldest non-dirty entry. */
    uint32_t victim_idx = 0;
    uint32_t oldest_order = UINT32_MAX;
    bool found_clean = false;

    for (uint32_t i = 0; i < cache->capacity; ++i) {
        if (!cache->entries[i].dirty &&
            cache->entries[i].access_order < oldest_order) {
            oldest_order = cache->entries[i].access_order;
            victim_idx = i;
            found_clean = true;
        }
    }

    if (!found_clean) return HTL_ERR_IO;

    cache->entries[victim_idx].device_idx    = (uint8_t)device_idx;
    cache->entries[victim_idx].block_num     = block_num;
    cache->entries[victim_idx].dirty         = 0;
    cache->entries[victim_idx].valid         = 1;
    cache->entries[victim_idx].access_order  = access_counter;
    if (data) {
        for (uint32_t j = 0; j < HTL_CACHE_DATA_SIZE; ++j) {
            cache->entries[victim_idx].data[j] = ((const uint8_t *)data)[j];
        }
    }
    return HTL_OK;
}

/* ================================================================== */
/*  htl_init                                                          */
/* ================================================================== */

htl_status_t htl_init(htl_context_t *ctx,
                       htl_cache_entry_t *cache_buf,
                       uint32_t cache_cap)
{
    if (!ctx) return HTL_ERR_INVALID_PARAM;

    /* Zero the entire context. */
    uint8_t *dst = (uint8_t *)ctx;
    for (size_t i = 0; i < sizeof(htl_context_t); ++i) dst[i] = 0;

    ctx->device_count  = 0;
    ctx->access_counter = 0;

    ctx->cache.entries  = cache_buf;
    ctx->cache.capacity = cache_cap;
    ctx->cache.count    = 0;
    ctx->cache.hits     = 0;
    ctx->cache.misses   = 0;

    /* Zero the cache buffer. */
    if (cache_buf && cache_cap > 0) {
        size_t buf_bytes = (size_t)cache_cap * sizeof(htl_cache_entry_t);
        uint8_t *cstart = (uint8_t *)cache_buf;
        for (size_t i = 0; i < buf_bytes; ++i) cstart[i] = 0;
    }

    return HTL_OK;
}

/* ================================================================== */
/*  htl_register_device                                               */
/* ================================================================== */

htl_status_t htl_register_device(htl_context_t *ctx,
                                  htl_device_type_t device_type,
                                  htl_read_block_fn read_fn,
                                  htl_write_block_fn write_fn,
                                  htl_flush_cache_fn flush_fn,
                                  htl_entropy_fn entropy_fn,
                                  void *driver_ctx,
                                  uint32_t block_size,
                                  uint32_t total_blocks,
                                  const char *name,
                                  uint32_t *out_idx)
{
    if (!ctx) return HTL_ERR_INVALID_PARAM;
    if (!read_fn || !write_fn) return HTL_ERR_INVALID_PARAM;
    if (block_size == 0 || total_blocks == 0) return HTL_ERR_INVALID_PARAM;
    if (ctx->device_count >= HTL_MAX_DEVICES) return HTL_ERR_INVALID_PARAM;

    uint32_t idx = ctx->device_count;

    htl_device_t *dev = &ctx->devices[idx];
    dev->read_block   = read_fn;
    dev->write_block  = write_fn;
    dev->flush_cache  = flush_fn;
    dev->entropy      = entropy_fn;
    dev->driver_ctx   = driver_ctx;
    dev->device_type  = device_type;
    dev->block_size   = block_size;
    dev->total_blocks = total_blocks;
    dev->write_protect = 0;
    dev->reserved[0] = 0;
    dev->reserved[1] = 0;
    dev->reserved[2] = 0;

    ctx->device_count++;

    /* Store the device name (truncated) so get_device_info can report
     * the caller-supplied identity instead of a type-derived label. */
    uint32_t ni;
    for (ni = 0; ni < HTL_NAME_LEN - 1 && name && name[ni] != '\0'; ++ni) {
        dev->name[ni] = name[ni];
    }
    dev->name[ni] = '\0';

    if (out_idx) *out_idx = idx;
    return HTL_OK;
}

/* ================================================================== */
/*  htl_unregister_device                                             */
/* ================================================================== */

htl_status_t htl_unregister_device(htl_context_t *ctx, uint32_t device_idx)
{
    htl_status_t st = validate_device_idx(ctx, device_idx);
    if (st != HTL_OK) return st;

    /* Invalidate all cache entries for this device. */
    htl_invalidate_cache(ctx, device_idx);

    /* Zero out the device slot. */
    uint8_t *dst = (uint8_t *)&ctx->devices[device_idx];
    for (size_t i = 0; i < sizeof(htl_device_t); ++i) dst[i] = 0;

    /* Zero stats for this slot. */
    dst = (uint8_t *)&ctx->stats[device_idx];
    for (size_t i = 0; i < sizeof(htl_stats_t); ++i) dst[i] = 0;

    /* Compact: shift remaining devices down. */
    if (device_idx < ctx->device_count - 1) {
        uint32_t remaining = ctx->device_count - device_idx - 1;
        for (uint32_t i = 0; i < remaining; ++i) {
            uint32_t src_idx = device_idx + 1 + i;
            ctx->devices[device_idx + i] = ctx->devices[src_idx];
            ctx->stats[device_idx + i]   = ctx->stats[src_idx];

            /* Update cache entries referencing the shifted device. */
            for (uint32_t c = 0; c < ctx->cache.count; ++c) {
                if (ctx->cache.entries[c].valid &&
                    ctx->cache.entries[c].device_idx == (uint8_t)src_idx) {
                    ctx->cache.entries[c].device_idx = (uint8_t)(device_idx + i);
                }
            }
        }
    }

    /* Clear the last slot. */
    uint32_t last = ctx->device_count - 1;
    uint8_t *last_dst = (uint8_t *)&ctx->devices[last];
    for (size_t i = 0; i < sizeof(htl_device_t); ++i) last_dst[i] = 0;
    last_dst = (uint8_t *)&ctx->stats[last];
    for (size_t i = 0; i < sizeof(htl_stats_t); ++i) last_dst[i] = 0;

    ctx->device_count--;
    return HTL_OK;
}

/* ================================================================== */
/*  htl_read_sectors                                                  */
/* ================================================================== */

htl_status_t htl_read_sectors(htl_context_t *ctx,
                               uint32_t device_idx,
                               uint64_t lba,
                               uint32_t count,
                               void *buf)
{
    htl_status_t st = validate_device_idx(ctx, device_idx);
    if (st != HTL_OK) return st;
    if (!buf || count == 0) return HTL_ERR_INVALID_PARAM;

    htl_device_t *dev = &ctx->devices[device_idx];
    if (!device_valid(dev)) return HTL_ERR_NOT_READY;

    /* Boundary check: lba + count must not exceed sector count. */
    uint64_t end_sector = lba + (uint64_t)count;
    uint64_t dev_sectors = (uint64_t)dev->total_blocks *
                           ((uint64_t)dev->block_size / VIP_SECTOR_SIZE);
    if (end_sector > dev_sectors) return HTL_ERR_INVALID_BLOCK;

    /* Convert sector addressing to block addressing. */
    uint32_t sectors_per_block = dev->block_size / VIP_SECTOR_SIZE;
    uint8_t *out = (uint8_t *)buf;
    uint64_t sectors_remaining = count;
    uint64_t current_lba = lba;

    while (sectors_remaining > 0) {
        uint32_t block_num = (uint32_t)(current_lba / sectors_per_block);
        uint32_t offset_in_block = (uint32_t)(current_lba % sectors_per_block);
        uint32_t sectors_left_in_block = sectors_per_block - offset_in_block;
        uint32_t to_read = (uint32_t)sectors_remaining;
        if (to_read > sectors_left_in_block) {
            to_read = sectors_left_in_block;
        }

        /* If partial block read, we need a temporary buffer. */
        if (offset_in_block != 0 || to_read < sectors_per_block) {
            uint8_t block_buf[HTL_CACHE_DATA_SIZE];
            st = dev->read_block(dev->driver_ctx, block_num,
                                 block_buf, dev->block_size);
            if (st != HTL_OK) {
                ctx->stats[device_idx].errors =
                    sat_add_u64(ctx->stats[device_idx].errors, 1);
                return st;
            }
            uint32_t byte_offset = offset_in_block * VIP_SECTOR_SIZE;
            uint32_t byte_count  = to_read * VIP_SECTOR_SIZE;
            for (uint32_t i = 0; i < byte_count; ++i) {
                out[i] = block_buf[byte_offset + i];
            }
        } else {
            /* Full block read, no intermediate copy needed. */
            st = dev->read_block(dev->driver_ctx, block_num,
                                 out, dev->block_size);
            if (st != HTL_OK) {
                ctx->stats[device_idx].errors =
                    sat_add_u64(ctx->stats[device_idx].errors, 1);
                return st;
            }
        }

        out += to_read * VIP_SECTOR_SIZE;
        current_lba   += to_read;
        sectors_remaining -= to_read;
    }

    /* Accumulate stats. */
    ctx->stats[device_idx].reads =
        sat_add_u64(ctx->stats[device_idx].reads, 1);
    ctx->stats[device_idx].bytes_read =
        sat_add_u64(ctx->stats[device_idx].bytes_read,
                     (uint64_t)count * VIP_SECTOR_SIZE);

    return HTL_OK;
}

/* ================================================================== */
/*  htl_write_sectors                                                 */
/* ================================================================== */

htl_status_t htl_write_sectors(htl_context_t *ctx,
                                uint32_t device_idx,
                                uint64_t lba,
                                uint32_t count,
                                const void *buf)
{
    htl_status_t st = validate_device_idx(ctx, device_idx);
    if (st != HTL_OK) return st;
    if (!buf || count == 0) return HTL_ERR_INVALID_PARAM;

    htl_device_t *dev = &ctx->devices[device_idx];
    if (!device_valid(dev)) return HTL_ERR_NOT_READY;
    if (dev->write_protect) return HTL_ERR_WRITE_PROTECT;

    uint64_t end_sector = lba + (uint64_t)count;
    uint64_t dev_sectors = (uint64_t)dev->total_blocks *
                           ((uint64_t)dev->block_size / VIP_SECTOR_SIZE);
    if (end_sector > dev_sectors) return HTL_ERR_INVALID_BLOCK;

    uint32_t sectors_per_block = dev->block_size / VIP_SECTOR_SIZE;
    const uint8_t *src = (const uint8_t *)buf;
    uint64_t sectors_remaining = count;
    uint64_t current_lba = lba;

    while (sectors_remaining > 0) {
        uint32_t block_num = (uint32_t)(current_lba / sectors_per_block);
        uint32_t offset_in_block = (uint32_t)(current_lba % sectors_per_block);
        uint32_t sectors_left_in_block = sectors_per_block - offset_in_block;
        uint32_t to_write = (uint32_t)sectors_remaining;
        if (to_write > sectors_left_in_block) {
            to_write = sectors_left_in_block;
        }

        if (offset_in_block != 0 || to_write < sectors_per_block) {
            /* Read-modify-write for partial block. */
            uint8_t block_buf[HTL_CACHE_DATA_SIZE];
            st = dev->read_block(dev->driver_ctx, block_num,
                                 block_buf, dev->block_size);
            if (st != HTL_OK) {
                ctx->stats[device_idx].errors =
                    sat_add_u64(ctx->stats[device_idx].errors, 1);
                return st;
            }
            uint32_t byte_offset = offset_in_block * VIP_SECTOR_SIZE;
            uint32_t byte_count  = to_write * VIP_SECTOR_SIZE;
            for (uint32_t i = 0; i < byte_count; ++i) {
                block_buf[byte_offset + i] = src[i];
            }
            st = dev->write_block(dev->driver_ctx, block_num,
                                  block_buf, dev->block_size);
        } else {
            /* Full block write. */
            st = dev->write_block(dev->driver_ctx, block_num,
                                  src, dev->block_size);
        }

        if (st != HTL_OK) {
            ctx->stats[device_idx].errors =
                sat_add_u64(ctx->stats[device_idx].errors, 1);
            return st;
        }

        src += to_write * VIP_SECTOR_SIZE;
        current_lba   += to_write;
        sectors_remaining -= to_write;
    }

    ctx->stats[device_idx].writes =
        sat_add_u64(ctx->stats[device_idx].writes, 1);
    ctx->stats[device_idx].bytes_written =
        sat_add_u64(ctx->stats[device_idx].bytes_written,
                     (uint64_t)count * VIP_SECTOR_SIZE);

    return HTL_OK;
}

/* ================================================================== */
/*  htl_read_cached                                                   */
/* ================================================================== */

htl_status_t htl_read_cached(htl_context_t *ctx,
                              uint32_t device_idx,
                              uint32_t block_num,
                              void *buf)
{
    htl_status_t st = validate_device_idx(ctx, device_idx);
    if (st != HTL_OK) return st;
    if (!buf) return HTL_ERR_INVALID_PARAM;

    htl_device_t *dev = &ctx->devices[device_idx];
    if (!device_valid(dev)) return HTL_ERR_NOT_READY;
    if (block_num >= dev->total_blocks) return HTL_ERR_INVALID_BLOCK;

    /* Check cache. */
    int32_t idx = cache_find(&ctx->cache, device_idx, block_num);
    if (idx >= 0) {
        /* Cache hit. */
        ctx->cache.hits = sat_add_u64(ctx->cache.hits, 1);
        for (uint32_t i = 0; i < HTL_CACHE_DATA_SIZE; ++i) {
            ((uint8_t *)buf)[i] = ctx->cache.entries[idx].data[i];
        }
        return HTL_OK;
    }

    /* Cache miss - read from device. */
    ctx->cache.misses = sat_add_u64(ctx->cache.misses, 1);
    st = dev->read_block(dev->driver_ctx, block_num, buf, dev->block_size);
    if (st != HTL_OK) {
        ctx->stats[device_idx].errors =
            sat_add_u64(ctx->stats[device_idx].errors, 1);
        return st;
    }

    /* Insert into cache. */
    ctx->access_counter++;
    cache_insert(&ctx->cache, device_idx, block_num, buf, ctx->access_counter);

    ctx->stats[device_idx].reads =
        sat_add_u64(ctx->stats[device_idx].reads, 1);
    ctx->stats[device_idx].bytes_read =
        sat_add_u64(ctx->stats[device_idx].bytes_read, dev->block_size);

    return HTL_OK;
}

/* ================================================================== */
/*  htl_write_cached                                                  */
/* ================================================================== */

htl_status_t htl_write_cached(htl_context_t *ctx,
                               uint32_t device_idx,
                               uint32_t block_num,
                               const void *buf)
{
    htl_status_t st = validate_device_idx(ctx, device_idx);
    if (st != HTL_OK) return st;
    if (!buf) return HTL_ERR_INVALID_PARAM;

    htl_device_t *dev = &ctx->devices[device_idx];
    if (!device_valid(dev)) return HTL_ERR_NOT_READY;
    if (dev->write_protect) return HTL_ERR_WRITE_PROTECT;
    if (block_num >= dev->total_blocks) return HTL_ERR_INVALID_BLOCK;

    /* Check if block is already in cache. */
    int32_t idx = cache_find(&ctx->cache, device_idx, block_num);
    if (idx >= 0) {
        /* Update existing entry. */
        for (uint32_t i = 0; i < HTL_CACHE_DATA_SIZE; ++i) {
            ctx->cache.entries[idx].data[i] = ((const uint8_t *)buf)[i];
        }
        ctx->cache.entries[idx].dirty = 1;
        ctx->cache.entries[idx].access_order = ctx->access_counter++;
    } else {
        /* Insert new entry. */
        ctx->access_counter++;
        st = cache_insert(&ctx->cache, device_idx, block_num, buf,
                          ctx->access_counter);
        if (st != HTL_OK) return st;
        /* Mark the newly inserted entry dirty. */
        int32_t new_idx = cache_find(&ctx->cache, device_idx, block_num);
        if (new_idx >= 0) {
            ctx->cache.entries[new_idx].dirty = 1;
        }
    }

    ctx->stats[device_idx].writes =
        sat_add_u64(ctx->stats[device_idx].writes, 1);
    ctx->stats[device_idx].bytes_written =
        sat_add_u64(ctx->stats[device_idx].bytes_written, dev->block_size);

    return HTL_OK;
}

/* ================================================================== */
/*  htl_flush_device_cache                                            */
/* ================================================================== */

htl_status_t htl_flush_device_cache(htl_context_t *ctx, uint32_t device_idx)
{
    htl_status_t st = validate_device_idx(ctx, device_idx);
    if (st != HTL_OK) return st;

    htl_device_t *dev = &ctx->devices[device_idx];
    if (!device_valid(dev)) return HTL_ERR_NOT_READY;

    htl_status_t last_err = HTL_OK;

    for (uint32_t i = 0; i < ctx->cache.capacity; ++i) {
        htl_cache_entry_t *e = &ctx->cache.entries[i];
        if (e->valid && e->dirty && e->device_idx == (uint8_t)device_idx) {
            st = dev->write_block(dev->driver_ctx, e->block_num,
                                  e->data, dev->block_size);
            if (st != HTL_OK) {
                last_err = st;
                ctx->stats[device_idx].errors =
                    sat_add_u64(ctx->stats[device_idx].errors, 1);
            } else {
                e->dirty = 0;
            }
        }
    }

    /* Also call the low-level flush if available. */
    if (dev->flush_cache) {
        st = dev->flush_cache(dev->driver_ctx);
        if (st != HTL_OK) {
            last_err = st;
            ctx->stats[device_idx].errors =
                sat_add_u64(ctx->stats[device_idx].errors, 1);
        }
    }

    ctx->stats[device_idx].flushes =
        sat_add_u64(ctx->stats[device_idx].flushes, 1);

    return last_err;
}

/* ================================================================== */
/*  htl_invalidate_cache                                              */
/* ================================================================== */

htl_status_t htl_invalidate_cache(htl_context_t *ctx, uint32_t device_idx)
{
    htl_status_t st = validate_device_idx(ctx, device_idx);
    if (st != HTL_OK) return st;

    for (uint32_t i = 0; i < ctx->cache.capacity; ++i) {
        htl_cache_entry_t *e = &ctx->cache.entries[i];
        if (e->valid && e->device_idx == (uint8_t)device_idx) {
            e->valid = 0;
            e->dirty = 0;
        }
    }

    return HTL_OK;
}

/* ================================================================== */
/*  htl_zero_block                                                    */
/* ================================================================== */

htl_status_t htl_zero_block(htl_context_t *ctx,
                             uint32_t device_idx,
                             uint32_t block_num)
{
    htl_status_t st = validate_device_idx(ctx, device_idx);
    if (st != HTL_OK) return st;

    htl_device_t *dev = &ctx->devices[device_idx];
    if (!device_valid(dev)) return HTL_ERR_NOT_READY;
    if (dev->write_protect) return HTL_ERR_WRITE_PROTECT;
    if (block_num >= dev->total_blocks) return HTL_ERR_INVALID_BLOCK;

    uint8_t zeros[HTL_CACHE_DATA_SIZE];
    for (uint32_t i = 0; i < HTL_CACHE_DATA_SIZE; ++i) zeros[i] = 0;

    st = dev->write_block(dev->driver_ctx, block_num,
                          zeros, dev->block_size);
    if (st != HTL_OK) {
        ctx->stats[device_idx].errors =
            sat_add_u64(ctx->stats[device_idx].errors, 1);
    }
    return st;
}

/* ================================================================== */
/*  htl_get_device_info                                               */
/* ================================================================== */

htl_status_t htl_get_device_info(htl_context_t *ctx,
                                  uint32_t device_idx,
                                  htl_device_info_t *info_out)
{
    htl_status_t st = validate_device_idx(ctx, device_idx);
    if (st != HTL_OK) return st;
    if (!info_out) return HTL_ERR_INVALID_PARAM;

    htl_device_t *dev = &ctx->devices[device_idx];

    info_out->device_type   = dev->device_type;
    info_out->block_size    = dev->block_size;
    info_out->total_blocks  = dev->total_blocks;
    info_out->write_protect = dev->write_protect;
    info_out->sector_count  = (uint64_t)dev->total_blocks *
                              ((uint64_t)dev->block_size / VIP_SECTOR_SIZE);

    /* Report the caller-supplied name if one was stored; otherwise fall
     * back to a type-derived label. */
    for (uint32_t i = 0; i < HTL_NAME_LEN; ++i)
        info_out->name[i] = '\0';

    const char *label = NULL;
    switch (dev->device_type) {
        case HTL_DEV_NVME:      label = "nvme";      break;
        case HTL_DEV_AHCI_SATA: label = "ahci_sata";  break;
        case HTL_DEV_USB_MASS:  label = "usb_mass";   break;
        default:                label = "unknown";     break;
    }
    for (uint32_t i = 0; i < HTL_NAME_LEN - 1 && dev->name[i]; ++i) {
        info_out->name[i] = dev->name[i];
    }
    if (dev->name[0] == '\0') {
        for (uint32_t i = 0; i < HTL_NAME_LEN - 1 && label[i]; ++i) {
            info_out->name[i] = label[i];
        }
    }

    return HTL_OK;
}

/* ================================================================== */
/*  htl_get_stats                                                     */
/* ================================================================== */

htl_status_t htl_get_stats(htl_context_t *ctx,
                            uint32_t device_idx,
                            htl_stats_t *stats_out)
{
    htl_status_t st = validate_device_idx(ctx, device_idx);
    if (st != HTL_OK) return st;
    if (!stats_out) return HTL_ERR_INVALID_PARAM;

    *stats_out = ctx->stats[device_idx];
    return HTL_OK;
}

/* ================================================================== */
/*  htl_get_entropy                                                   */
/* ================================================================== */

htl_status_t htl_get_entropy(htl_context_t *ctx,
                              uint32_t device_idx,
                              uint8_t *out,
                              size_t len)
{
    htl_status_t st = validate_device_idx(ctx, device_idx);
    if (st != HTL_OK) return st;
    if (!out || len == 0) return HTL_ERR_INVALID_PARAM;

    htl_device_t *dev = &ctx->devices[device_idx];
    if (!device_valid(dev)) return HTL_ERR_NOT_READY;
    if (!dev->entropy) return HTL_ERR_NOT_READY;

    return dev->entropy(dev->driver_ctx, out, len);
}
