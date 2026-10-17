/*
 * floppy.h - OpenWindows Floppy Disk Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef FLOPPY_H
#define FLOPPY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Legacy floppy controller port window (0x3F0..0x3F7). */
#define FLOPPY_BASE_PRIMARY   0x3F0u
/* 8237 DMA channel used by the floppy controller. */
#define FLOPPY_DMA_CHANNEL    2u
/* Standard 3.5" high-density geometry. */
#define FLOPPY_SECTOR_SIZE    512u
#define FLOPPY_SECTORS_TRACK  18u
#define FLOPPY_HEADS          2u
/* End-of-track and gap-length parameters for 1.44 MB media. */
#define FLOPPY_EOT_144        0x12u
#define FLOPPY_GPL_144        0x1Bu
/* Largest transfer accepted by the 16-bit DMA count register. */
#define FLOPPY_MAX_SECTORS    64u

/* Status codes returned by every floppy entry point. */
typedef enum {
    FLOPPY_OK = 0,
    FLOPPY_ERR_INVALID_PARAM,
    FLOPPY_ERR_NOT_INITIALIZED,
    FLOPPY_ERR_TIMEOUT,
    FLOPPY_ERR_NO_DRIVE,
    FLOPPY_ERR_SEEK,
    FLOPPY_ERR_IO,
    FLOPPY_ERR_DMA,
    FLOPPY_ERR_UNSUPPORTED,
    FLOPPY_ERR_BUFFER_TOO_SMALL
} floppy_status_t;

/* One drive plus its DOR/MSR controller state. */
typedef struct {
    uint16_t base;        /* controller register base (0x3F0) */
    uint8_t  drive;       /* 0 = A:, 1 = B: */
    uint8_t  head;        /* currently selected head */
    uint8_t  cylinder;    /* last known cylinder */
    uint8_t  eot;         /* sectors per track (end of track) */
    uint8_t  gpl;         /* gap length */
    uint8_t  data_rate;   /* configuration control register value */
    uint8_t  dor;         /* digital output register shadow */
    uint8_t  msr;         /* last main status register sample */
    bool     motor_on;    /* motor currently spinning */
    bool     present;     /* media/drive responded to recalibrate */
    bool     initialized;
} floppy_drive_t;

/* Reset the controller, program the drive and recalibrate to track 0. */
floppy_status_t floppy_init(floppy_drive_t *drv, uint16_t base, uint8_t drive);

/* Seek the selected head to `cylinder` and confirm via SENSE INTERRUPT. */
floppy_status_t floppy_seek(floppy_drive_t *drv, uint8_t cylinder);

/* Read `count` 512-byte sectors starting at `lba` through the DMA engine. */
floppy_status_t floppy_read(floppy_drive_t *drv, uint32_t lba, uint32_t count,
                            void *buf, size_t buf_size);

/* Write `count` 512-byte sectors starting at `lba` through the DMA engine. */
floppy_status_t floppy_write(floppy_drive_t *drv, uint32_t lba, uint32_t count,
                             const void *buf, size_t buf_size);

/* Assert and release the controller reset line, then drain interrupts. */
floppy_status_t floppy_reset(floppy_drive_t *drv);

#endif /* FLOPPY_H */
