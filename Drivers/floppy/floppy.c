/*
 * floppy.c - OpenWindows Floppy Disk Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation. The driver talks to the
 * 82077-class controller through its register window (0x3F0..0x3F7) and
 * moves sector payloads with the 8237 DMA channel 2.
 */

#include "floppy.h"

/* ── Port I/O helpers ─────────────────────────────────────────── */

static inline uint8_t ow_inb(uint16_t port)
{
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void ow_outb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint16_t ow_inw(uint16_t port)
{
    uint16_t v;
    __asm__ __volatile__("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void ow_outw(uint16_t port, uint16_t value)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint32_t ow_inl(uint16_t port)
{
    uint32_t v;
    __asm__ __volatile__("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void ow_outl(uint16_t port, uint32_t value)
{
    __asm__ __volatile__("outl %0, %1" : : "a"(value), "Nd"(port));
}

/* ── Tiny local helpers ───────────────────────────────────────── */

static void ow_memset(void *dst, int value, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < len; ++i) {
        d[i] = (uint8_t)value;
    }
}

/* Simple calibrated delay used for motor spin-up and command settle. */
static void floppy_stall(uint32_t loops)
{
    for (volatile uint32_t i = 0u; i < loops; ++i) {
        __asm__ __volatile__("" ::: "memory");
    }
}

/* ── Controller registers ─────────────────────────────────────── */

#define FLOPPY_SRA      0u  /* status register A (read) */
#define FLOPPY_SRB      1u  /* status register B (read) */
#define FLOPPY_DOR      2u  /* digital output register (write) */
#define FLOPPY_TDR      3u  /* tape drive / data rate select */
#define FLOPPY_MSR      4u  /* main status register (read) */
#define FLOPPY_FIFO     5u  /* data FIFO */
#define FLOPPY_DIR      7u  /* digital input register (read) */
#define FLOPPY_CCR      7u  /* configuration control register (write) */

#define FLOPPY_MSR_RQM  0x80u
#define FLOPPY_MSR_DIO  0x40u
#define FLOPPY_MSR_NDMA 0x20u
#define FLOPPY_MSR_CB   0x10u

#define FLOPPY_DOR_DS0   0x01u
#define FLOPPY_DOR_DS1   0x02u
#define FLOPPY_DOR_NRST  0x04u
#define FLOPPY_DOR_DMA   0x08u
#define FLOPPY_DOR_MOT0  0x10u
#define FLOPPY_DOR_MOT1  0x20u
#define FLOPPY_DOR_ENABLE (FLOPPY_DOR_NRST | FLOPPY_DOR_DMA)

/* Command opcodes (MFM, media transfer where applicable). */
#define FLOPPY_CMD_SPECIFY    0x03u
#define FLOPPY_CMD_SENSE_INT  0x08u
#define FLOPPY_CMD_RECAL      0x07u
#define FLOPPY_CMD_SEEK       0x0Fu
#define FLOPPY_CMD_CONFIGURE  0x13u
#define FLOPPY_CMD_READ       0x46u  /* MT | MFM */
#define FLOPPY_CMD_WRITE      0x45u  /* MT | MFM */

/* 8237 DMA controller programming ports (channel 2). */
#define DMA_CH2_ADDR    0x04u
#define DMA_CH2_COUNT   0x05u
#define DMA_MASK_REG    0x0Au
#define DMA_MODE_REG    0x0Bu
#define DMA_FLIPFLOP    0x0Cu
#define DMA_CH2_PAGE    0x81u

#define DMA_MODE_MEM_TO_DEV 0x48u  /* single, increment, memory -> device */
#define DMA_MODE_DEV_TO_MEM 0x44u  /* single, increment, device -> memory */

#define FLOPPY_SPIN_LIMIT    200000u
#define FLOPPY_SEEK_RETRIES  64u
#define FLOPPY_SETTLE        20000u
#define FLOPPY_RESET_STALL   200000u
#define FLOPPY_MOTOR_STALL   800000u

/* ── MSR state machine ────────────────────────────────────────── */

static floppy_status_t fdc_wait_msr(floppy_drive_t *drv, uint8_t mask,
                                    uint8_t value)
{
    for (uint32_t i = 0u; i < FLOPPY_SPIN_LIMIT; ++i) {
        drv->msr = ow_inb((uint16_t)(drv->base + FLOPPY_MSR));
        if ((drv->msr & mask) == value) {
            return FLOPPY_OK;
        }
    }
    return FLOPPY_ERR_TIMEOUT;
}

static void fdc_write_dor(floppy_drive_t *drv, uint8_t value)
{
    drv->dor = value;
    ow_outb((uint16_t)(drv->base + FLOPPY_DOR), value);
}

/* ── Command / result phases ──────────────────────────────────── */

static floppy_status_t fdc_write_cmd(floppy_drive_t *drv, const uint8_t *cmd,
                                     uint32_t len)
{
    for (uint32_t i = 0u; i < len; ++i) {
        floppy_status_t st = fdc_wait_msr(drv, FLOPPY_MSR_RQM, FLOPPY_MSR_RQM);
        if (st != FLOPPY_OK) {
            return st;
        }
        /* Some implementations set RQM before changing direction. */
        if ((drv->msr & FLOPPY_MSR_DIO) != 0u) {
            return FLOPPY_ERR_IO;
        }
        ow_outb((uint16_t)(drv->base + FLOPPY_FIFO), cmd[i]);
    }
    return FLOPPY_OK;
}

static floppy_status_t fdc_read_result(floppy_drive_t *drv, uint8_t *out,
                                       uint32_t max, uint32_t *got)
{
    uint32_t n = 0u;

    for (uint32_t guard = 0u;
         (guard < FLOPPY_SPIN_LIMIT) && (n < max); ++guard) {
        drv->msr = ow_inb((uint16_t)(drv->base + FLOPPY_MSR));
        if ((drv->msr & FLOPPY_MSR_RQM) == 0u) {
            continue;
        }
        if ((drv->msr & FLOPPY_MSR_DIO) == 0u) {
            break;   /* controller is done sending result bytes */
        }
        out[n] = ow_inb((uint16_t)(drv->base + FLOPPY_FIFO));
        n++;
    }
    *got = n;
    return (n > 0u) ? FLOPPY_OK : FLOPPY_ERR_TIMEOUT;
}

static floppy_status_t fdc_sense_interrupt(floppy_drive_t *drv, uint8_t *st0,
                                           uint8_t *pcn)
{
    uint8_t cmd = FLOPPY_CMD_SENSE_INT;
    uint8_t res[2];
    uint32_t got = 0u;
    floppy_status_t st;

    st = fdc_write_cmd(drv, &cmd, 1u);
    if (st != FLOPPY_OK) {
        return st;
    }
    st = fdc_read_result(drv, res, 2u, &got);
    if (st != FLOPPY_OK) {
        return st;
    }
    if (got != 2u) {
        return FLOPPY_ERR_IO;
    }
    *st0 = res[0];
    *pcn = res[1];
    return FLOPPY_OK;
}

/* Wait for a seek/recalibrate to finish by polling SENSE INTERRUPT STATUS. */
static floppy_status_t fdc_await_seek(floppy_drive_t *drv)
{
    for (uint32_t attempt = 0u; attempt < FLOPPY_SEEK_RETRIES; ++attempt) {
        uint8_t st0 = 0u;
        uint8_t pcn = 0u;

        floppy_stall(FLOPPY_SETTLE);
        if (fdc_sense_interrupt(drv, &st0, &pcn) != FLOPPY_OK) {
            continue;
        }
        if ((st0 & 0xC0u) != 0u) {
            continue;   /* no interrupt pending (0x80) or abnormal code */
        }
        if ((st0 & 0x20u) != 0u) {
            return FLOPPY_OK;   /* SE: seek completed */
        }
    }
    return FLOPPY_ERR_SEEK;
}

/* ── Motor and geometry helpers ───────────────────────────────── */

static void floppy_motor_on(floppy_drive_t *drv)
{
    uint8_t motor = (drv->drive == 0u) ? FLOPPY_DOR_MOT0 : FLOPPY_DOR_MOT1;
    uint8_t ds = (drv->drive == 0u) ? FLOPPY_DOR_DS0 : FLOPPY_DOR_DS1;

    if (drv->motor_on) {
        return;
    }
    fdc_write_dor(drv, (uint8_t)(FLOPPY_DOR_ENABLE | motor | ds));
    floppy_stall(FLOPPY_MOTOR_STALL);
    drv->motor_on = true;
}

static void floppy_lba_to_chs(uint32_t lba, uint8_t *cyl, uint8_t *head,
                              uint8_t *sect)
{
    *sect = (uint8_t)((lba % FLOPPY_SECTORS_TRACK) + 1u);
    uint32_t tmp = lba / FLOPPY_SECTORS_TRACK;
    *head = (uint8_t)(tmp % FLOPPY_HEADS);
    *cyl = (uint8_t)(tmp / FLOPPY_HEADS);
}

/* ── DMA programming ──────────────────────────────────────────── */

static void floppy_dma_program(const void *buf, uint32_t length, bool to_device)
{
    uintptr_t addr = (uintptr_t)buf;
    uint32_t count = length - 1u;
    uint8_t mode = to_device ? DMA_MODE_MEM_TO_DEV : DMA_MODE_DEV_TO_MEM;

    ow_outb(DMA_MASK_REG, (uint8_t)((FLOPPY_DMA_CHANNEL & 0x03u) | 0x04u));
    ow_outb(DMA_FLIPFLOP, 0x00u);
    ow_outb(DMA_MODE_REG, mode);
    ow_outb(DMA_CH2_ADDR, (uint8_t)(addr & 0xFFu));
    ow_outb(DMA_CH2_ADDR, (uint8_t)((addr >> 8) & 0xFFu));
    ow_outb(DMA_CH2_PAGE, (uint8_t)((addr >> 16) & 0xFFu));
    ow_outb(DMA_CH2_COUNT, (uint8_t)(count & 0xFFu));
    ow_outb(DMA_CH2_COUNT, (uint8_t)((count >> 8) & 0xFFu));
    ow_outb(DMA_MASK_REG, (uint8_t)(FLOPPY_DMA_CHANNEL & 0x03u));
}

/* ── Configure / specify ──────────────────────────────────────── */

static floppy_status_t floppy_configure(floppy_drive_t *drv)
{
    uint8_t cfg[4];

    cfg[0] = FLOPPY_CMD_CONFIGURE;
    cfg[1] = 0x00u;   /* EIS, EFIFO, POLL and FIFO-disable all clear */
    cfg[2] = 0x00u;   /* FIFO threshold */
    cfg[3] = 0x00u;   /* pre-compensation track */
    return fdc_write_cmd(drv, cfg, 4u);
}

static floppy_status_t floppy_specify(floppy_drive_t *drv)
{
    uint8_t sp[3];

    sp[0] = FLOPPY_CMD_SPECIFY;
    sp[1] = 0xDFu;   /* SRT = 13, HUT = 15 */
    sp[2] = 0x02u;   /* HLT = 0, NDMA = 0 (DMA mode) */
    return fdc_write_cmd(drv, sp, 3u);
}

static floppy_status_t floppy_recalibrate(floppy_drive_t *drv)
{
    uint8_t cmd[2];
    floppy_status_t st;

    cmd[0] = FLOPPY_CMD_RECAL;
    cmd[1] = (uint8_t)(drv->drive & 0x01u);
    st = fdc_write_cmd(drv, cmd, 2u);
    if (st != FLOPPY_OK) {
        return st;
    }
    st = fdc_await_seek(drv);
    if (st != FLOPPY_OK) {
        return st;
    }
    drv->cylinder = 0u;
    drv->head = 0u;
    return FLOPPY_OK;
}

/* ── Sector transfer ──────────────────────────────────────────── */

static floppy_status_t floppy_transfer(floppy_drive_t *drv, uint32_t lba,
                                       uint32_t count, void *buf, bool write)
{
    uint8_t cyl = 0u;
    uint8_t head = 0u;
    uint8_t sect = 0u;
    uint8_t cmd[9];
    uint8_t res[7];
    uint32_t got = 0u;
    floppy_status_t st;

    floppy_lba_to_chs(lba, &cyl, &head, &sect);
    drv->head = head;

    if (cyl != drv->cylinder) {
        st = floppy_seek(drv, cyl);
        if (st != FLOPPY_OK) {
            return st;
        }
    }

    floppy_dma_program(buf, count * FLOPPY_SECTOR_SIZE, write);

    cmd[0] = write ? FLOPPY_CMD_WRITE : FLOPPY_CMD_READ;
    cmd[1] = (uint8_t)((head << 2) | (drv->drive & 0x01u));
    cmd[2] = cyl;
    cmd[3] = head;
    cmd[4] = sect;
    cmd[5] = 0x02u;          /* N = 2 -> 512 bytes per sector */
    cmd[6] = drv->eot;
    cmd[7] = drv->gpl;
    cmd[8] = 0xFFu;          /* DTL */

    st = fdc_write_cmd(drv, cmd, 9u);
    if (st != FLOPPY_OK) {
        return st;
    }

    st = fdc_read_result(drv, res, 7u, &got);
    if (st != FLOPPY_OK) {
        return st;
    }
    if (got < 7u) {
        return FLOPPY_ERR_IO;
    }
    if (((res[0] & 0xC0u) != 0u) || ((res[1] & 0x20u) != 0u)) {
        return FLOPPY_ERR_IO;   /* abnormal termination or data error */
    }
    return FLOPPY_OK;
}

/* ── Public API ───────────────────────────────────────────────── */

floppy_status_t floppy_reset(floppy_drive_t *drv)
{
    if ((drv == NULL) || (drv->base == 0u)) {
        return FLOPPY_ERR_INVALID_PARAM;
    }

    /* Assert reset through DOR bit 2, hold it, then release. */
    ow_outb((uint16_t)(drv->base + FLOPPY_DOR), 0x00u);
    floppy_stall(FLOPPY_RESET_STALL);
    fdc_write_dor(drv, (uint8_t)(FLOPPY_DOR_ENABLE | (drv->drive & 0x01u)));
    floppy_stall(FLOPPY_RESET_STALL);

    drv->motor_on = false;
    drv->cylinder = 0u;
    drv->head = 0u;

    /* Drain up to four pending reset interrupts (one per drive). */
    for (uint32_t i = 0u; i < 4u; ++i) {
        uint8_t st0 = 0u;
        uint8_t pcn = 0u;
        floppy_stall(FLOPPY_SETTLE);
        (void)fdc_sense_interrupt(drv, &st0, &pcn);
    }
    return FLOPPY_OK;
}

floppy_status_t floppy_init(floppy_drive_t *drv, uint16_t base, uint8_t drive)
{
    floppy_status_t st;

    if ((drv == NULL) || (base == 0u) || (drive > 1u)) {
        return FLOPPY_ERR_INVALID_PARAM;
    }

    ow_memset(drv, 0, sizeof(*drv));
    drv->base = base;
    drv->drive = drive;
    drv->head = 0u;
    drv->cylinder = 0u;
    drv->eot = FLOPPY_EOT_144;
    drv->gpl = FLOPPY_GPL_144;
    drv->data_rate = 0x00u;   /* 500 kbps */

    ow_outb((uint16_t)(base + FLOPPY_TDR), 0x00u);
    ow_outb((uint16_t)(base + FLOPPY_CCR), drv->data_rate);

    st = floppy_reset(drv);
    if (st != FLOPPY_OK) {
        return st;
    }

    drv->initialized = true;
    floppy_motor_on(drv);

    (void)floppy_configure(drv);
    (void)floppy_specify(drv);

    st = floppy_recalibrate(drv);
    drv->present = (st == FLOPPY_OK);
    return drv->present ? FLOPPY_OK : FLOPPY_ERR_NO_DRIVE;
}

floppy_status_t floppy_seek(floppy_drive_t *drv, uint8_t cylinder)
{
    uint8_t cmd[3];
    floppy_status_t st;

    if ((drv == NULL) || !drv->initialized) {
        return FLOPPY_ERR_NOT_INITIALIZED;
    }
    if (!drv->present) {
        return FLOPPY_ERR_NO_DRIVE;
    }

    floppy_motor_on(drv);

    cmd[0] = FLOPPY_CMD_SEEK;
    cmd[1] = (uint8_t)((drv->head << 2) | (drv->drive & 0x01u));
    cmd[2] = cylinder;

    st = fdc_write_cmd(drv, cmd, 3u);
    if (st != FLOPPY_OK) {
        return st;
    }
    st = fdc_await_seek(drv);
    if (st != FLOPPY_OK) {
        return st;
    }
    drv->cylinder = cylinder;
    return FLOPPY_OK;
}

floppy_status_t floppy_read(floppy_drive_t *drv, uint32_t lba, uint32_t count,
                            void *buf, size_t buf_size)
{
    uint64_t bytes;

    if ((drv == NULL) || !drv->initialized) {
        return FLOPPY_ERR_NOT_INITIALIZED;
    }
    if (!drv->present) {
        return FLOPPY_ERR_NO_DRIVE;
    }
    if ((buf == NULL) || (count == 0u) || (count > FLOPPY_MAX_SECTORS)) {
        return FLOPPY_ERR_INVALID_PARAM;
    }

    bytes = (uint64_t)count * (uint64_t)FLOPPY_SECTOR_SIZE;
    if (bytes > (uint64_t)buf_size) {
        return FLOPPY_ERR_BUFFER_TOO_SMALL;
    }
    return floppy_transfer(drv, lba, count, buf, false);
}

floppy_status_t floppy_write(floppy_drive_t *drv, uint32_t lba, uint32_t count,
                             const void *buf, size_t buf_size)
{
    uint64_t bytes;

    if ((drv == NULL) || !drv->initialized) {
        return FLOPPY_ERR_NOT_INITIALIZED;
    }
    if (!drv->present) {
        return FLOPPY_ERR_NO_DRIVE;
    }
    if ((buf == NULL) || (count == 0u) || (count > FLOPPY_MAX_SECTORS)) {
        return FLOPPY_ERR_INVALID_PARAM;
    }

    bytes = (uint64_t)count * (uint64_t)FLOPPY_SECTOR_SIZE;
    if (bytes > (uint64_t)buf_size) {
        return FLOPPY_ERR_BUFFER_TOO_SMALL;
    }
    return floppy_transfer(drv, lba, count, (void *)(uintptr_t)buf, true);
}
