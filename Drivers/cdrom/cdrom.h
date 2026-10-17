/*
 * cdrom.h - OpenWindows ATAPI CD-ROM Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef CDROM_H
#define CDROM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Primary ATAPI bus task-file window (0x1F0..0x1F7) and alt-status (0x3F6). */
#define CDROM_PRIMARY_IO     0x1F0u
#define CDROM_PRIMARY_CTRL   0x3F6u
/* Secondary ATAPI bus task-file window (0x170..0x177) and alt-status (0x376). */
#define CDROM_SECONDARY_IO   0x170u
#define CDROM_SECONDARY_CTRL 0x376u

/* 2048-byte Mode 1 / Mode 2 Form 1 data sector. */
#define CDROM_SECTOR_SIZE    2048u
/* Model string exposed by IDENTIFY PACKET DEVICE (40 chars + NUL). */
#define CDROM_MODEL_LEN      41u

/* ATAPI device type code found in IDENTIFY word 0 bits 12:8. */
#define CDROM_TYPE_CDROM     0x05u

/* Status codes returned by every CD-ROM entry point. */
typedef enum {
    CDROM_OK = 0,
    CDROM_ERR_INVALID_PARAM,
    CDROM_ERR_NOT_INITIALIZED,
    CDROM_ERR_NO_DEVICE,
    CDROM_ERR_TIMEOUT,
    CDROM_ERR_DEVICE_FAULT,
    CDROM_ERR_UNSUPPORTED,
    CDROM_ERR_BUFFER_TOO_SMALL,
    CDROM_ERR_MEDIA
} cdrom_status_t;

/* ATAPI CD-ROM instance. Caller owns the storage; no heap is used. */
typedef struct {
    uint16_t io_base;      /* task-file base (0x1F0 / 0x170) */
    uint16_t ctrl_base;    /* device-control / alt-status base */
    uint8_t  device;       /* 0 = master, 1 = slave */
    bool     present;      /* IDENTIFY PACKET DEVICE succeeded */
    bool     removable;    /* IDENTIFY word 0 bit 7 */
    bool     lba_supported;
    uint32_t sector_size;  /* bytes per logical block */
    uint32_t last_lba;     /* highest addressable LBA from READ CAPACITY */
    char     model[CDROM_MODEL_LEN];
    bool     initialized;
} cdrom_device_t;

/* Probe an ATAPI packet device with IDENTIFY PACKET DEVICE (0xA1). */
cdrom_status_t cdrom_init(cdrom_device_t *cd, uint16_t io_base,
                          uint16_t ctrl_base, uint8_t device);

/* Read one 2048-byte data sector at `lba` using READ(12) (0xA8). */
cdrom_status_t cdrom_read_sector(cdrom_device_t *cd, uint32_t lba,
                                 void *buf, size_t buf_size);

/* Load (`eject` false) or unload (`eject` true) the tray via START STOP UNIT. */
cdrom_status_t cdrom_eject(cdrom_device_t *cd, bool eject);

/* Query the last addressable LBA using READ CAPACITY (0x25). */
cdrom_status_t cdrom_capacity(cdrom_device_t *cd, uint32_t *last_lba);

#endif /* CDROM_H */
