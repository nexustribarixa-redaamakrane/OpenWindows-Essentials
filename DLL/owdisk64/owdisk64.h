/*
 * owdisk64.h - OpenWindows Disk I/O Abstraction (.owd)
 *
 * Block-device abstraction over a pluggable backend vtable. A backend
 * exposes raw LBA read/write/flush callbacks; this layer adds a static
 * device table, descriptor validation, range checking and sector-count
 * based transfer helpers.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWDISK64_H
#define OWDISK64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Limits ───────────────────────────────────────────────────── */

#define OWDISK64_MAX_DEVICES    8u
#define OWDISK64_NAME_MAX       32u
#define OWDISK64_SECTOR_SIZE    512u    /* conventional logical sector */

/* ── Status codes ─────────────────────────────────────────────── */

typedef enum {
    OWDISK_OK           = 0,    /* success                            */
    OWDISK_ERR_PARAM    = -1,   /* bad parameter                      */
    OWDISK_ERR_NO_DEV   = -2,   /* unknown device index               */
    OWDISK_ERR_IO       = -3,   /* backend reported an I/O fault      */
    OWDISK_ERR_READONLY = -4,   /* write attempted on read-only media */
    OWDISK_ERR_RANGE    = -5,   /* LBA range outside the device       */
    OWDISK_ERR_FULL     = -6    /* device table is full               */
} owdisk_status_t;

/* ── Backend vtable ───────────────────────────────────────────── */

/* Read count blocks starting at lba into buf. Returns 0 on success. */
typedef int32_t (*owdisk_read_fn)(void *ctx, uint64_t lba, uint32_t count,
                                  void *buf);

/* Write count blocks starting at lba from buf. Returns 0 on success. */
typedef int32_t (*owdisk_write_fn)(void *ctx, uint64_t lba, uint32_t count,
                                   const void *buf);

/* Flush any backend write cache. Returns 0 on success. */
typedef int32_t (*owdisk_flush_fn)(void *ctx);

typedef struct {
    owdisk_read_fn  read;
    owdisk_write_fn write;
    owdisk_flush_fn flush;
} owdisk_backend_t;

/* ── Device descriptor returned to callers ────────────────────── */

typedef struct {
    char     name[OWDISK64_NAME_MAX];   /* human-readable device name */
    uint32_t block_size;                /* bytes per logical block    */
    uint64_t total_blocks;              /* addressable block count    */
    uint64_t total_bytes;               /* total_blocks * block_size  */
    bool     read_only;                 /* writes are rejected        */
    bool     present;                   /* slot holds a device        */
} owdisk_info_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Clear the device table. */
void owdisk_init(void);

/* Register a backend as a block device. Returns a non-negative device
 * index, or a negative owdisk_status_t on failure. */
int32_t owdisk_register(const char *name, const owdisk_backend_t *backend,
                        void *backend_ctx, uint32_t block_size,
                        uint64_t total_blocks, bool read_only);

/* Read count blocks from device starting at lba. */
int32_t owdisk_read(uint32_t device, uint64_t lba, uint32_t count, void *buf);

/* Write count blocks to device starting at lba. */
int32_t owdisk_write(uint32_t device, uint64_t lba, uint32_t count,
                     const void *buf);

/* Flush the device write cache. */
int32_t owdisk_flush(uint32_t device);

/* Fetch the descriptor for a device index. */
bool owdisk_get_info(uint32_t device, owdisk_info_t *out);

/* Copy up to capacity present descriptors into out; returns the count. */
uint32_t owdisk_enumerate(owdisk_info_t *out, uint32_t capacity);

#endif /* OWDISK64_H */
