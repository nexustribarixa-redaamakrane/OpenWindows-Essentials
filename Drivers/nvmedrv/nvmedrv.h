/*
 * nvmedrv.h - OpenWindows NVMe Storage Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef NVMEDRV_H
#define NVMEDRV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Default logical block size when the namespace has not been queried. */
#define NVME_DEFAULT_LBA_SIZE   512u
/* Baseline memory page size advertised through CC.MPS (log2 = 12). */
#define NVME_PAGE_SIZE          4096u
/* Largest single-PRP transfer (bytes) supported by this driver. */
#define NVME_MAX_XFER           NVME_PAGE_SIZE
/* Capacity of the fixed admin queue pair. */
#define NVME_ADMIN_Q_DEPTH      8u
/* Capacity of the fixed I/O queue pair. */
#define NVME_IO_Q_DEPTH         8u
/* Model string exposed by IDENTIFY (40 chars + NUL). */
#define NVME_MODEL_LEN          41u
/* Serial number string exposed by IDENTIFY (20 chars + NUL). */
#define NVME_SERIAL_LEN         21u
/* Firmware revision string exposed by IDENTIFY (8 chars + NUL). */
#define NVME_FIRMWARE_LEN       9u

/* Status codes returned by every NVMe entry point. */
typedef enum {
    NVME_OK = 0,
    NVME_ERR_INVALID_PARAM,
    NVME_ERR_NOT_INITIALIZED,
    NVME_ERR_TIMEOUT,
    NVME_ERR_CONTROLLER_FAULT,
    NVME_ERR_COMMAND,
    NVME_ERR_UNSUPPORTED,
    NVME_ERR_BUFFER_TOO_SMALL
} nvme_status_t;

/* Result of the IDENTIFY controller/namespace pair. */
typedef struct {
    bool     valid;                       /* identify completed successfully */
    char     model[NVME_MODEL_LEN];       /* controller model number          */
    char     serial[NVME_SERIAL_LEN];     /* controller serial number         */
    char     firmware[NVME_FIRMWARE_LEN]; /* firmware revision                */
    uint64_t nsze;                        /* namespace size (logical blocks)  */
    uint64_t ncap;                        /* namespace capacity (logical blocks) */
    uint32_t lba_size;                    /* bytes per logical block          */
    uint8_t  lba_shift;                   /* log2(bytes per logical block)    */
} nvme_id_t;

/* Controller instance. Caller owns the storage; no heap is used. */
typedef struct {
    uintptr_t bar0;             /* BAR0 mapped register base               */
    uint32_t  version;          /* VS register                             */
    uint32_t  doorbell_stride;  /* CAP.DSTRD                               */
    uint32_t  page_size;        /* bytes per memory page                   */
    uint32_t  lba_size;         /* namespace logical block size            */
    uint16_t  admin_q_depth;    /* admin queue entries actually used       */
    uint16_t  io_q_depth;       /* I/O queue entries actually used         */
    uint32_t  admin_sq_tail;    /* admin submission queue tail index       */
    uint32_t  admin_cq_head;    /* admin completion queue head index       */
    uint32_t  io_sq_tail;       /* I/O submission queue tail index         */
    uint32_t  io_cq_head;       /* I/O completion queue head index         */
    uint8_t   admin_cq_phase;   /* admin completion queue expected phase   */
    uint8_t   io_cq_phase;      /* I/O completion queue expected phase     */
    bool      io_queues_ready;  /* I/O queue pair created                  */
    bool      initialized;
} nvme_controller_t;

/* Enable the controller (CC.EN) and bring up the admin queue pair. */
nvme_status_t nvme_init(nvme_controller_t *ctrl, uintptr_t bar0);

/* Issue IDENTIFY controller and namespace, filling `id`. */
nvme_status_t nvme_identify(nvme_controller_t *ctrl, nvme_id_t *id);

/* Read `count` logical blocks from `lba` into `buf` (NVM Read 0x02). */
nvme_status_t nvme_read(nvme_controller_t *ctrl, uint64_t lba, uint32_t count,
                        void *buf, size_t buf_size);

/* Write `count` logical blocks at `lba` from `buf` (NVM Write 0x01). */
nvme_status_t nvme_write(nvme_controller_t *ctrl, uint64_t lba, uint32_t count,
                         const void *buf, size_t buf_size);

/* Flush volatile write caches to the medium (NVM Flush 0x00). */
nvme_status_t nvme_flush(nvme_controller_t *ctrl);

#endif /* NVMEDRV_H */
