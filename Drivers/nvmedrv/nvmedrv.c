/*
 * nvmedrv.c - OpenWindows NVMe Storage Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation: the admin and I/O queue
 * pairs plus the IDENTIFY payload live in file-scope static, page-aligned
 * storage. The boot environment identity-maps low physical memory, so the
 * virtual addresses of those buffers are used directly as PRP addresses.
 */

#include "nvmedrv.h"

/* ── Tiny local helpers ───────────────────────────────────────── */

static void ow_memset(void *dst, int value, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < len; ++i) {
        d[i] = (uint8_t)value;
    }
}

/* Compiler barrier: preserves queue/doorbell ordering. */
static void nvme_mb(void)
{
    __asm__ __volatile__("" ::: "memory");
}

static uint32_t nvme_lo(const void *p)
{
    return (uint32_t)(uintptr_t)p;
}

static uint32_t nvme_hi(const void *p)
{
    return (uint32_t)(((uint64_t)(uintptr_t)p) >> 32);
}

/* ── Register map (BAR0 relative) ─────────────────────────────── */

#define NVME_CAP             0x00u
#define NVME_VS              0x08u
#define NVME_INTMS           0x0Cu
#define NVME_INTMC           0x10u
#define NVME_CC              0x14u
#define NVME_CSTS            0x1Cu
#define NVME_NSSR            0x20u
#define NVME_AQA             0x24u
#define NVME_ASQ             0x28u
#define NVME_ACQ             0x30u
#define NVME_DOORBELL_BASE   0x1000u

#define NVME_CC_EN           0x00000001u
#define NVME_CSTS_RDY        0x00000001u
#define NVME_CSTS_CFS        0x00000002u

#define NVME_ADMIN_CREATE_SQ 0x01u
#define NVME_ADMIN_CREATE_CQ 0x05u
#define NVME_ADMIN_IDENTIFY  0x06u

#define NVME_IO_FLUSH        0x00u
#define NVME_IO_WRITE        0x01u
#define NVME_IO_READ         0x02u

#define NVME_SPIN_LIMIT      2000000u

#define NVME_ID_CNS_NAMESPACE 0u
#define NVME_ID_CNS_CONTROLLER 1u

/* ── MMIO accessors ───────────────────────────────────────────── */

static uint32_t nvme_rd32(uintptr_t bar0, uint32_t off)
{
    return *(volatile uint32_t *)(bar0 + (uintptr_t)off);
}

static void nvme_wr32(uintptr_t bar0, uint32_t off, uint32_t value)
{
    *(volatile uint32_t *)(bar0 + (uintptr_t)off) = value;
}

static uint64_t nvme_rd64(uintptr_t bar0, uint32_t off)
{
    uint64_t lo = nvme_rd32(bar0, off);
    uint64_t hi = nvme_rd32(bar0, off + 4u);
    return lo | (hi << 32);
}

static void nvme_wr64(uintptr_t bar0, uint32_t off, uint64_t value)
{
    nvme_wr32(bar0, off, (uint32_t)(value & 0xFFFFFFFFu));
    nvme_wr32(bar0, off + 4u, (uint32_t)(value >> 32));
}

/* ── Queue elements ───────────────────────────────────────────── */

/* Submission queue entry (64 bytes). */
typedef struct {
    uint32_t cdw0;    /* opcode (7:0), fuse (9:8), command id (31:16) */
    uint32_t nsid;
    uint32_t cdw2;
    uint32_t cdw3;
    uint32_t mptr_lo;
    uint32_t mptr_hi;
    uint32_t prp1_lo;
    uint32_t prp1_hi;
    uint32_t prp2_lo;
    uint32_t prp2_hi;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} nvme_sqe_t;

/* Completion queue entry (16 bytes). */
typedef struct {
    uint32_t result;
    uint32_t rsvd;
    uint32_t sq_head;  /* 15:0  = submission queue head pointer */
    uint32_t status;   /* 15:0 = command id, 16 = phase, 24:17 = status */
} nvme_cqe_t;

/* ── Static DMA-backed storage ────────────────────────────────── */

static nvme_sqe_t g_nvme_asq[NVME_ADMIN_Q_DEPTH] __attribute__((aligned(4096)));
static nvme_cqe_t g_nvme_acq[NVME_ADMIN_Q_DEPTH] __attribute__((aligned(4096)));
static nvme_sqe_t g_nvme_iosq[NVME_IO_Q_DEPTH] __attribute__((aligned(4096)));
static nvme_cqe_t g_nvme_iocq[NVME_IO_Q_DEPTH] __attribute__((aligned(4096)));
static uint8_t    g_nvme_id[4096] __attribute__((aligned(4096)));

/* ── Little-endian field readers over a byte buffer ───────────── */

static uint32_t nvme_buf_rd32(const uint8_t *b, uint32_t off)
{
    uint32_t v = 0u;
    for (uint32_t i = 0u; i < 4u; ++i) {
        v |= (uint32_t)b[off + i] << (8u * i);
    }
    return v;
}

static uint64_t nvme_buf_rd64(const uint8_t *b, uint32_t off)
{
    uint64_t v = 0u;
    for (uint32_t i = 0u; i < 8u; ++i) {
        v |= (uint64_t)b[off + i] << (8u * i);
    }
    return v;
}

static void nvme_copy_str(char *dst, const uint8_t *src, uint32_t len,
                          uint32_t cap)
{
    uint32_t n = (len < (cap - 1u)) ? len : (cap - 1u);

    for (uint32_t i = 0u; i < n; ++i) {
        dst[i] = (char)src[i];
    }
    dst[n] = '\0';

    /* Trim trailing blanks from the fixed-width ASCII field. */
    while ((n > 0u) && (dst[n - 1u] == ' ')) {
        dst[n - 1u] = '\0';
        n--;
    }
}

/* ── Controller ready handshake ───────────────────────────────── */

static nvme_status_t nvme_wait_rdy(uintptr_t bar0, bool want)
{
    for (uint32_t i = 0u; i < NVME_SPIN_LIMIT; ++i) {
        uint32_t csts = nvme_rd32(bar0, NVME_CSTS);
        if ((csts & NVME_CSTS_CFS) != 0u) {
            return NVME_ERR_CONTROLLER_FAULT;
        }
        if (((csts & NVME_CSTS_RDY) != 0u) == want) {
            return NVME_OK;
        }
    }
    return NVME_ERR_TIMEOUT;
}

/* ── Doorbell addressing ──────────────────────────────────────── */

static uint32_t nvme_doorbell_off(const nvme_controller_t *ctrl, uint32_t qid,
                                  bool completion)
{
    uint32_t stride = 4u << ctrl->doorbell_stride;
    uint32_t index = (qid * 2u) + (completion ? 1u : 0u);
    return NVME_DOORBELL_BASE + (index * stride);
}

/* ── Admin command submission ─────────────────────────────────── */

static nvme_status_t nvme_admin_submit(nvme_controller_t *ctrl,
                                       const nvme_sqe_t *req, uint32_t *result)
{
    uintptr_t bar0 = ctrl->bar0;
    uint16_t tail = (uint16_t)ctrl->admin_sq_tail;
    nvme_sqe_t *slot = &g_nvme_asq[tail];

    ow_memset(slot, 0, sizeof(*slot));
    *slot = *req;
    slot->cdw0 = (slot->cdw0 & 0x0000FFFFu) | ((uint32_t)tail << 16);

    ctrl->admin_sq_tail = ((uint32_t)tail + 1u) % (uint32_t)ctrl->admin_q_depth;
    nvme_mb();
    nvme_wr32(bar0, nvme_doorbell_off(ctrl, 0u, false), ctrl->admin_sq_tail);

    for (uint32_t i = 0u; i < NVME_SPIN_LIMIT; ++i) {
        volatile nvme_cqe_t *cqe = &g_nvme_acq[ctrl->admin_cq_head];
        uint32_t status = cqe->status;

        if ((uint8_t)((status >> 16) & 1u) == ctrl->admin_cq_phase) {
            uint8_t sc = (uint8_t)((status >> 17) & 0xFFu);

            if (result != NULL) {
                *result = cqe->result;
            }
            ctrl->admin_cq_head++;
            if (ctrl->admin_cq_head >= (uint32_t)ctrl->admin_q_depth) {
                ctrl->admin_cq_head = 0u;
                ctrl->admin_cq_phase ^= 1u;
            }
            nvme_wr32(bar0, nvme_doorbell_off(ctrl, 0u, true),
                      ctrl->admin_cq_head);
            return (sc == 0u) ? NVME_OK : NVME_ERR_COMMAND;
        }
    }
    return NVME_ERR_TIMEOUT;
}

/* ── I/O queue pair creation ──────────────────────────────────── */

static nvme_status_t nvme_create_io_queues(nvme_controller_t *ctrl)
{
    nvme_sqe_t sqe;
    nvme_status_t status;
    uint32_t qsize = (uint32_t)ctrl->io_q_depth - 1u;

    /* Create I/O completion queue 1 (physically contiguous, polling mode). */
    ow_memset(&sqe, 0, sizeof(sqe));
    sqe.cdw0 = NVME_ADMIN_CREATE_CQ;
    sqe.prp1_lo = nvme_lo(g_nvme_iocq);
    sqe.prp1_hi = nvme_hi(g_nvme_iocq);
    sqe.cdw10 = 1u | (qsize << 16);
    sqe.cdw11 = 1u;                       /* PC = 1 */
    status = nvme_admin_submit(ctrl, &sqe, NULL);
    if (status != NVME_OK) {
        return status;
    }

    /* Create I/O submission queue 1 bound to completion queue 1. */
    ow_memset(&sqe, 0, sizeof(sqe));
    sqe.cdw0 = NVME_ADMIN_CREATE_SQ;
    sqe.prp1_lo = nvme_lo(g_nvme_iosq);
    sqe.prp1_hi = nvme_hi(g_nvme_iosq);
    sqe.cdw10 = 1u | (qsize << 16);
    sqe.cdw11 = 1u | (1u << 16);          /* PC = 1, CQID = 1 */
    status = nvme_admin_submit(ctrl, &sqe, NULL);
    if (status != NVME_OK) {
        return status;
    }

    ctrl->io_queues_ready = true;
    return NVME_OK;
}

/* ── I/O command submission ───────────────────────────────────── */

static nvme_status_t nvme_io_submit(nvme_controller_t *ctrl, uint32_t opcode,
                                    uint64_t lba, uint32_t nlb,
                                    const void *buf)
{
    uintptr_t bar0 = ctrl->bar0;
    uint16_t tail = (uint16_t)ctrl->io_sq_tail;
    nvme_sqe_t *slot = &g_nvme_iosq[tail];

    ow_memset(slot, 0, sizeof(*slot));
    slot->cdw0 = opcode | ((uint32_t)tail << 16);
    slot->nsid = 1u;
    if (buf != NULL) {
        slot->prp1_lo = nvme_lo(buf);
        slot->prp1_hi = nvme_hi(buf);
    }
    slot->cdw10 = (uint32_t)(lba & 0xFFFFFFFFu);
    slot->cdw11 = (uint32_t)(lba >> 32);
    slot->cdw12 = nlb & 0xFFFFu;

    ctrl->io_sq_tail = ((uint32_t)tail + 1u) % (uint32_t)ctrl->io_q_depth;
    nvme_mb();
    nvme_wr32(bar0, nvme_doorbell_off(ctrl, 1u, false), ctrl->io_sq_tail);

    for (uint32_t i = 0u; i < NVME_SPIN_LIMIT; ++i) {
        volatile nvme_cqe_t *cqe = &g_nvme_iocq[ctrl->io_cq_head];
        uint32_t status = cqe->status;

        if ((uint8_t)((status >> 16) & 1u) == ctrl->io_cq_phase) {
            uint8_t sc = (uint8_t)((status >> 17) & 0xFFu);

            ctrl->io_cq_head++;
            if (ctrl->io_cq_head >= (uint32_t)ctrl->io_q_depth) {
                ctrl->io_cq_head = 0u;
                ctrl->io_cq_phase ^= 1u;
            }
            nvme_wr32(bar0, nvme_doorbell_off(ctrl, 1u, true),
                      ctrl->io_cq_head);
            return (sc == 0u) ? NVME_OK : NVME_ERR_COMMAND;
        }
    }
    return NVME_ERR_TIMEOUT;
}

/* ── Public API ───────────────────────────────────────────────── */

nvme_status_t nvme_init(nvme_controller_t *ctrl, uintptr_t bar0)
{
    uint64_t cap;
    uint32_t cc;
    uint32_t mqes;
    uint32_t depth;
    nvme_status_t status;

    if ((ctrl == NULL) || (bar0 == 0u)) {
        return NVME_ERR_INVALID_PARAM;
    }

    ow_memset(ctrl, 0, sizeof(*ctrl));
    ctrl->bar0 = bar0;
    ctrl->page_size = NVME_PAGE_SIZE;
    ctrl->lba_size = NVME_DEFAULT_LBA_SIZE;

    cap = nvme_rd64(bar0, NVME_CAP);
    ctrl->doorbell_stride = (uint32_t)((cap >> 32) & 0x0Fu);
    mqes = (uint32_t)(cap & 0xFFFFu);

    /* Disable the controller first if firmware left it enabled. */
    cc = nvme_rd32(bar0, NVME_CC);
    if ((cc & NVME_CC_EN) != 0u) {
        nvme_wr32(bar0, NVME_CC, cc & ~NVME_CC_EN);
        status = nvme_wait_rdy(bar0, false);
        if (status != NVME_OK) {
            return status;
        }
    }

    depth = mqes + 1u;
    if (depth > NVME_ADMIN_Q_DEPTH) {
        depth = NVME_ADMIN_Q_DEPTH;
    }
    if (depth < 2u) {
        return NVME_ERR_UNSUPPORTED;
    }
    ctrl->admin_q_depth = (uint16_t)depth;

    depth = mqes + 1u;
    if (depth > NVME_IO_Q_DEPTH) {
        depth = NVME_IO_Q_DEPTH;
    }
    if (depth < 2u) {
        return NVME_ERR_UNSUPPORTED;
    }
    ctrl->io_q_depth = (uint16_t)depth;

    ow_memset(g_nvme_asq, 0, sizeof(g_nvme_asq));
    ow_memset(g_nvme_acq, 0, sizeof(g_nvme_acq));
    ow_memset(g_nvme_iosq, 0, sizeof(g_nvme_iosq));
    ow_memset(g_nvme_iocq, 0, sizeof(g_nvme_iocq));

    ctrl->admin_sq_tail = 0u;
    ctrl->admin_cq_head = 0u;
    ctrl->io_sq_tail = 0u;
    ctrl->io_cq_head = 0u;
    ctrl->admin_cq_phase = 1u;
    ctrl->io_cq_phase = 1u;

    /* Admin queue attributes and base addresses. */
    nvme_wr32(bar0, NVME_AQA,
              (uint32_t)(ctrl->admin_q_depth - 1u) |
              ((uint32_t)(ctrl->admin_q_depth - 1u) << 16));
    nvme_wr64(bar0, NVME_ASQ, (uint64_t)(uintptr_t)g_nvme_asq);
    nvme_wr64(bar0, NVME_ACQ, (uint64_t)(uintptr_t)g_nvme_acq);

    /* CC: NVM command set, 4 KiB pages, 64-byte SQE, 16-byte CQE, enable. */
    cc = NVME_CC_EN | (6u << 16) | (4u << 20);
    nvme_wr32(bar0, NVME_INTMS, 0xFFFFFFFFu);
    nvme_wr32(bar0, NVME_CC, cc);

    status = nvme_wait_rdy(bar0, true);
    if (status != NVME_OK) {
        return status;
    }

    ctrl->version = nvme_rd32(bar0, NVME_VS);
    ctrl->initialized = true;

    /* I/O queues are best-effort: the admin queue still works without them. */
    (void)nvme_create_io_queues(ctrl);
    return NVME_OK;
}

nvme_status_t nvme_identify(nvme_controller_t *ctrl, nvme_id_t *id)
{
    nvme_sqe_t sqe;
    nvme_status_t status;
    uint32_t flbas;
    uint32_t lbaf;
    uint8_t lbads;

    if ((ctrl == NULL) || !ctrl->initialized) {
        return NVME_ERR_NOT_INITIALIZED;
    }
    if (id == NULL) {
        return NVME_ERR_INVALID_PARAM;
    }

    ow_memset(id, 0, sizeof(*id));
    ow_memset(g_nvme_id, 0, sizeof(g_nvme_id));

    /* IDENTIFY controller (CNS = 1). */
    ow_memset(&sqe, 0, sizeof(sqe));
    sqe.cdw0 = NVME_ADMIN_IDENTIFY;
    sqe.prp1_lo = nvme_lo(g_nvme_id);
    sqe.prp1_hi = nvme_hi(g_nvme_id);
    sqe.cdw10 = NVME_ID_CNS_CONTROLLER;
    status = nvme_admin_submit(ctrl, &sqe, NULL);
    if (status != NVME_OK) {
        return status;
    }
    nvme_copy_str(id->serial, &g_nvme_id[4], 20u, NVME_SERIAL_LEN);
    nvme_copy_str(id->model, &g_nvme_id[24], 40u, NVME_MODEL_LEN);
    nvme_copy_str(id->firmware, &g_nvme_id[64], 8u, NVME_FIRMWARE_LEN);

    /* IDENTIFY namespace 1 (CNS = 0). */
    ow_memset(g_nvme_id, 0, sizeof(g_nvme_id));
    ow_memset(&sqe, 0, sizeof(sqe));
    sqe.cdw0 = NVME_ADMIN_IDENTIFY;
    sqe.nsid = 1u;
    sqe.prp1_lo = nvme_lo(g_nvme_id);
    sqe.prp1_hi = nvme_hi(g_nvme_id);
    sqe.cdw10 = NVME_ID_CNS_NAMESPACE;
    status = nvme_admin_submit(ctrl, &sqe, NULL);
    if (status != NVME_OK) {
        return status;
    }

    id->nsze = nvme_buf_rd64(g_nvme_id, 0u);
    id->ncap = nvme_buf_rd64(g_nvme_id, 8u);

    flbas = nvme_buf_rd32(g_nvme_id, 24u);
    flbas &= 0x0Fu;
    lbaf = nvme_buf_rd32(g_nvme_id, 128u + (flbas * 4u));
    lbads = (uint8_t)((lbaf >> 16) & 0xFFu);
    if (lbads > 31u) {
        lbads = 9u;   /* sanity fallback: 512-byte blocks */
    }

    id->lba_shift = lbads;
    id->lba_size = 1u << lbads;
    ctrl->lba_size = id->lba_size;
    id->valid = true;
    return NVME_OK;
}

nvme_status_t nvme_read(nvme_controller_t *ctrl, uint64_t lba, uint32_t count,
                        void *buf, size_t buf_size)
{
    uint64_t bytes;

    if ((ctrl == NULL) || !ctrl->initialized) {
        return NVME_ERR_NOT_INITIALIZED;
    }
    if (!ctrl->io_queues_ready) {
        return NVME_ERR_UNSUPPORTED;
    }
    if ((buf == NULL) || (count == 0u) || (count > 0x10000u)) {
        return NVME_ERR_INVALID_PARAM;
    }

    bytes = (uint64_t)count * (uint64_t)ctrl->lba_size;
    if (bytes > (uint64_t)buf_size) {
        return NVME_ERR_BUFFER_TOO_SMALL;
    }
    if (bytes > (uint64_t)ctrl->page_size) {
        return NVME_ERR_UNSUPPORTED;   /* single-PRP transfers only */
    }
    return nvme_io_submit(ctrl, NVME_IO_READ, lba, count - 1u, buf);
}

nvme_status_t nvme_write(nvme_controller_t *ctrl, uint64_t lba, uint32_t count,
                         const void *buf, size_t buf_size)
{
    uint64_t bytes;

    if ((ctrl == NULL) || !ctrl->initialized) {
        return NVME_ERR_NOT_INITIALIZED;
    }
    if (!ctrl->io_queues_ready) {
        return NVME_ERR_UNSUPPORTED;
    }
    if ((buf == NULL) || (count == 0u) || (count > 0x10000u)) {
        return NVME_ERR_INVALID_PARAM;
    }

    bytes = (uint64_t)count * (uint64_t)ctrl->lba_size;
    if (bytes > (uint64_t)buf_size) {
        return NVME_ERR_BUFFER_TOO_SMALL;
    }
    if (bytes > (uint64_t)ctrl->page_size) {
        return NVME_ERR_UNSUPPORTED;   /* single-PRP transfers only */
    }
    return nvme_io_submit(ctrl, NVME_IO_WRITE, lba, count - 1u, buf);
}

nvme_status_t nvme_flush(nvme_controller_t *ctrl)
{
    if ((ctrl == NULL) || !ctrl->initialized) {
        return NVME_ERR_NOT_INITIALIZED;
    }
    if (!ctrl->io_queues_ready) {
        return NVME_ERR_UNSUPPORTED;
    }
    return nvme_io_submit(ctrl, NVME_IO_FLUSH, 0u, 0u, NULL);
}
