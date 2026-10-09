/*
 * ramdisk.c - OpenWindows Volatile RAM Disk Driver (.owc)
 *
 * Provides a memory-backed block device for temporary storage.
 * Uses caller-provided memory; zero dynamic allocation.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ramdisk.h"

/* ── Internal helpers ─────────────────────────────────────────── */

static void rd_memcpy(void *dst, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) { *d++ = *s++; }
}

static void rd_memset(void *dst, uint8_t val, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    while (n--) { *d++ = val; }
}

/* ── Public API ───────────────────────────────────────────────── */

void ramdisk_init(ramdisk_state_t *rd, uint8_t *buffer, size_t sector_count)
{
    if (!rd || !buffer || sector_count == 0) { return; }
    if (sector_count > RAMDISK_MAX_SECTORS) {
        sector_count = RAMDISK_MAX_SECTORS;
    }
    rd->base          = buffer;
    rd->total_sectors = sector_count;
    rd->sector_size   = RAMDISK_SECTOR_SIZE;
    rd->is_mounted    = true;
    rd_memset(buffer, 0, sector_count * RAMDISK_SECTOR_SIZE);
}

bool ramdisk_read(const ramdisk_state_t *rd, uint32_t lba, void *out, size_t count)
{
    if (!rd || !rd->is_mounted || !out) { return false; }
    if ((size_t)lba + count > rd->total_sectors) { return false; }
    size_t offset = (size_t)lba * rd->sector_size;
    rd_memcpy(out, rd->base + offset, count * rd->sector_size);
    return true;
}

bool ramdisk_write(ramdisk_state_t *rd, uint32_t lba, const void *in, size_t count)
{
    if (!rd || !rd->is_mounted || !in) { return false; }
    if ((size_t)lba + count > rd->total_sectors) { return false; }
    size_t offset = (size_t)lba * rd->sector_size;
    rd_memcpy(rd->base + offset, in, count * rd->sector_size);
    return true;
}
