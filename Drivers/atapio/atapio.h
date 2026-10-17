/*
 * atapio.h - OpenWindows ATA PIO Mode Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef ATAPIO_H
#define ATAPIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Primary bus task-file window (0x1F0..0x1F7) and alt-status (0x3F6). */
#define ATA_PRIMARY_IO       0x1F0u
#define ATA_PRIMARY_CTRL     0x3F6u
/* Secondary bus task-file window (0x170..0x177) and alt-status (0x376). */
#define ATA_SECONDARY_IO     0x170u
#define ATA_SECONDARY_CTRL   0x376u

/* Legacy PIO logical sector size. */
#define ATA_SECTOR_SIZE      512u
/* Model string exposed by IDENTIFY DEVICE (40 chars + NUL). */
#define ATA_MODEL_LEN        41u
/* Serial number string exposed by IDENTIFY DEVICE (20 chars + NUL). */
#define ATA_SERIAL_LEN       21u
/* Firmware revision string exposed by IDENTIFY DEVICE (8 chars + NUL). */
#define ATA_FIRMWARE_LEN     9u

/* Status codes returned by every ATA entry point. */
typedef enum {
    ATA_OK = 0,
    ATA_ERR_INVALID_PARAM,
    ATA_ERR_NOT_INITIALIZED,
    ATA_ERR_NO_DEVICE,
    ATA_ERR_TIMEOUT,
    ATA_ERR_DEVICE_FAULT,
    ATA_ERR_UNSUPPORTED,
    ATA_ERR_BUFFER_TOO_SMALL
} ata_status_t;

/* One drive on a channel (index 0 = master, 1 = slave). */
typedef struct {
    bool     present;      /* IDENTIFY DEVICE completed */
    bool     atapi;        /* word 0 bit 15 set (packet device) */
    bool     lba_supported;/* word 49 bit 9 */
    bool     lba48;        /* word 83 bit 10 and word 86 bit 10 */
    uint32_t sector_size;  /* bytes per logical sector */
    uint64_t sector_count; /* addressable logical sectors */
    char     model[ATA_MODEL_LEN];
    char     serial[ATA_SERIAL_LEN];
    char     firmware[ATA_FIRMWARE_LEN];
} ata_device_t;

/* ATA channel instance. Caller owns the storage; no heap is used. */
typedef struct {
    uint16_t     io_base;      /* task-file base (0x1F0 / 0x170) */
    uint16_t     ctrl_base;    /* device-control / alt-status base */
    bool         initialized;
    ata_device_t devices[2];   /* master, slave */
} ata_channel_t;

/* Soft-reset the channel and probe both drives via IDENTIFY DEVICE. */
ata_status_t ata_init(ata_channel_t *chan, uint16_t io_base, uint16_t ctrl_base);

/* Issue IDENTIFY DEVICE (0xEC) to `device` and parse the result. */
ata_status_t ata_identify(ata_channel_t *chan, uint32_t device);

/* Read `count` sectors from `lba` using PIO READ SECTORS (0x20/0x24). */
ata_status_t ata_read_sectors(ata_channel_t *chan, uint32_t device,
                              uint64_t lba, uint32_t count,
                              void *buf, size_t buf_size);

/* Write `count` sectors at `lba` using PIO WRITE SECTORS (0x30/0x34). */
ata_status_t ata_write_sectors(ata_channel_t *chan, uint32_t device,
                               uint64_t lba, uint32_t count,
                               const void *buf, size_t buf_size);

#endif /* ATAPIO_H */
