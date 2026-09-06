/*
 * htl.h - Hardware Translation Layer
 *
 * Unified hardware interface hiding board-specific logic. Wraps the
 * low-level htl_device_t callbacks with higher-level block operations:
 * multi-sector I/O, block cache, statistics, device probing, and
 * entropy gathering.
 *
 * Builds on top of the OpenWindows-Storage ow_htl.h types.
 * C99 freestanding - stdint.h / stdbool.h / stddef.h only, no heap.
 */

#ifndef HTL_H
#define HTL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ================================================================== */
/*  Cross-reference constants                                         */
/* ================================================================== */

#ifndef OWFS_BLOCK_SIZE
#define OWFS_BLOCK_SIZE         0x1000          /* 4096 bytes           */
#endif

#ifndef VIP_SECTOR_SIZE
#define VIP_SECTOR_SIZE         512             /* Sector addressing    */
#endif

/* ================================================================== */
/*  Low-level types — BANcode mapped                                   */
/*  B+ (0x0011A000-0x0011A77F): Fatal hardware faults                 */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal warnings                    */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                    */
/* ================================================================== */

typedef uint32_t htl_status_t;

#define HTL_OK                          0x00000000u  /* Success              */
/* B+ Fatal */
#define HTL_ERR_IO                      0x0011A000u  /* Hardware I/O fault   */
#define HTL_ERR_INVALID_BLOCK           0x0011A001u  /* Bad block address    */
/* W+ Warning */
#define HTL_ERR_TIMEOUT                 0x0011A800u  /* Device timed out     */
#define HTL_ERR_NOT_READY               0x0011A801u  /* Device not ready     */
#define HTL_ERR_WRITE_PROTECT           0x0011A802u  /* Write-protected      */
/* S+ Soft */
#define HTL_ERR_NO_DEVICE               0x0011AE00u  /* No device registered */
#define HTL_ERR_INVALID_PARAM           0x0011AE01u  /* Bad parameter        */

typedef enum {
    HTL_DEV_UNKNOWN         = 0,
    HTL_DEV_NVME            = 1,
    HTL_DEV_AHCI_SATA       = 2,
    HTL_DEV_USB_MASS        = 3
} htl_device_type_t;

typedef htl_status_t (*htl_read_block_fn)(void *ctx, uint32_t block_num,
                                          void *buf, uint32_t block_size);
typedef htl_status_t (*htl_write_block_fn)(void *ctx, uint32_t block_num,
                                           const void *buf, uint32_t block_size);
typedef htl_status_t (*htl_flush_cache_fn)(void *ctx);
typedef htl_status_t (*htl_entropy_fn)(void *ctx, uint8_t *out, size_t len);

#define HTL_NAME_LEN        32u

typedef struct {
    htl_read_block_fn    read_block;
    htl_write_block_fn   write_block;
    htl_flush_cache_fn   flush_cache;
    htl_entropy_fn       entropy;
    void                *driver_ctx;
    htl_device_type_t    device_type;
    uint32_t             block_size;
    uint32_t             total_blocks;
    uint8_t              write_protect;
    uint8_t              reserved[3];
    char                 name[HTL_NAME_LEN];
} htl_device_t;

/* ================================================================== */
/*  High-level HTL types                                              */
/* ================================================================== */

#define HTL_MAX_DEVICES     8u
#define HTL_CACHE_DATA_SIZE OWFS_BLOCK_SIZE

typedef struct {
    htl_device_type_t    device_type;
    uint32_t             block_size;
    uint32_t             total_blocks;
    uint8_t              write_protect;
    char                 name[HTL_NAME_LEN];
    uint64_t             sector_count;
} htl_device_info_t;

typedef struct {
    uint64_t             reads;
    uint64_t             writes;
    uint64_t             flushes;
    uint64_t             errors;
    uint64_t             bytes_read;
    uint64_t             bytes_written;
} htl_stats_t;

typedef struct {
    uint32_t             block_num;
    uint8_t              dirty;
    uint8_t              valid;
    uint8_t              device_idx;
    uint8_t              reserved;
    uint32_t             access_order;
    uint8_t              data[HTL_CACHE_DATA_SIZE];
} htl_cache_entry_t;

typedef struct {
    htl_cache_entry_t   *entries;
    uint32_t             count;
    uint32_t             capacity;
    uint64_t             hits;
    uint64_t             misses;
} htl_cache_t;

typedef struct {
    htl_device_t         devices[HTL_MAX_DEVICES];
    uint32_t             device_count;
    htl_cache_t          cache;
    htl_stats_t          stats[HTL_MAX_DEVICES];
    uint32_t             access_counter;
} htl_context_t;

/* ================================================================== */
/*  Functions                                                         */
/* ================================================================== */

/* Initialize the translation layer context. cache_buf points to caller-
 * provided storage for cache entries, cache_cap is the max number of
 * entries the buffer can hold. */
htl_status_t htl_init(htl_context_t *ctx,
                       htl_cache_entry_t *cache_buf,
                       uint32_t cache_cap);

/* Register a low-level device. Returns the assigned device index through
 * *out_idx. */
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
                                  uint32_t *out_idx);

/* Remove a device from the translation layer. */
htl_status_t htl_unregister_device(htl_context_t *ctx, uint32_t device_idx);

/* Multi-sector read. lba is the starting sector, count is the number
 * of sectors, buf must be at least count * VIP_SECTOR_SIZE bytes. */
htl_status_t htl_read_sectors(htl_context_t *ctx,
                               uint32_t device_idx,
                               uint64_t lba,
                               uint32_t count,
                               void *buf);

/* Multi-sector write. */
htl_status_t htl_write_sectors(htl_context_t *ctx,
                                uint32_t device_idx,
                                uint64_t lba,
                                uint32_t count,
                                const void *buf);

/* Cached block read. Checks the cache first; on miss, reads from
 * hardware and inserts into cache. */
htl_status_t htl_read_cached(htl_context_t *ctx,
                              uint32_t device_idx,
                              uint32_t block_num,
                              void *buf);

/* Cached block write. Writes into cache and marks the entry dirty.
 * If the cache is full, evicts the oldest clean entry (FIFO). */
htl_status_t htl_write_cached(htl_context_t *ctx,
                               uint32_t device_idx,
                               uint32_t block_num,
                               const void *buf);

/* Flush all dirty cache entries for a device back to hardware. */
htl_status_t htl_flush_device_cache(htl_context_t *ctx, uint32_t device_idx);

/* Invalidate (discard) all cache entries for a device. Does NOT flush. */
htl_status_t htl_invalidate_cache(htl_context_t *ctx, uint32_t device_idx);

/* Zero an entire block on the underlying device. */
htl_status_t htl_zero_block(htl_context_t *ctx,
                             uint32_t device_idx,
                             uint32_t block_num);

/* Query device info. */
htl_status_t htl_get_device_info(htl_context_t *ctx,
                                  uint32_t device_idx,
                                  htl_device_info_t *info_out);

/* Get per-device statistics. */
htl_status_t htl_get_stats(htl_context_t *ctx,
                            uint32_t device_idx,
                            htl_stats_t *stats_out);

/* Gather entropy from hardware RNG. Returns HTL_ERR_NOT_READY when no
 * entropy callback is registered. */
htl_status_t htl_get_entropy(htl_context_t *ctx,
                              uint32_t device_idx,
                              uint8_t *out,
                              size_t len);

#endif /* HTL_H */
