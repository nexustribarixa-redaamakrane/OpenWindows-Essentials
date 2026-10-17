/*
 * atapio.c - OpenWindows ATA PIO Mode Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation: the 512-byte IDENTIFY
 * buffer is file-scope static storage. All transfers use the legacy task
 * file (0x1F0..0x1F7 / 0x170..0x177) plus the alternate status register.
 */

#include "atapio.h"

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

/* ── Task-file register offsets ───────────────────────────────── */

#define ATA_REG_DATA        0u  /* data (16-bit) */
#define ATA_REG_ERROR       1u  /* error / features */
#define ATA_REG_FEATURES    1u
#define ATA_REG_SECCOUNT    2u  /* sector count */
#define ATA_REG_LBA0        3u
#define ATA_REG_LBA1        4u
#define ATA_REG_LBA2        5u
#define ATA_REG_DEVSEL      6u  /* drive / head */
#define ATA_REG_STATUS      7u
#define ATA_REG_COMMAND     7u
#define ATA_REG_ALTSTATUS   0u  /* relative to ctrl_base */

#define ATA_SR_BSY          0x80u
#define ATA_SR_DRDY         0x40u
#define ATA_SR_DF           0x20u
#define ATA_SR_DRQ          0x08u
#define ATA_SR_ERR          0x01u

#define ATA_CMD_READ         0x20u  /* READ SECTORS (LBA28) */
#define ATA_CMD_READ_EXT     0x24u  /* READ SECTORS EXT (LBA48) */
#define ATA_CMD_WRITE        0x30u  /* WRITE SECTORS (LBA28) */
#define ATA_CMD_WRITE_EXT    0x34u  /* WRITE SECTORS EXT (LBA48) */
#define ATA_CMD_IDENTIFY     0xECu

#define ATA_WORDS_PER_SECTOR 256u
#define ATA_SPIN_LIMIT       2000000u
#define ATA_PIO_MAX_SECTORS  256u

/* ── Static IDENTIFY buffer ───────────────────────────────────── */

static uint16_t g_ata_identify[ATA_WORDS_PER_SECTOR] __attribute__((aligned(2)));

/* ── Register access primitives ───────────────────────────────── */

static uint8_t ata_read_status(const ata_channel_t *chan)
{
    return ow_inb((uint16_t)(chan->io_base + ATA_REG_STATUS));
}

/* Reading the alternate status four times yields the required 400 ns. */
static void ata_delay_400ns(const ata_channel_t *chan)
{
    (void)ow_inb((uint16_t)(chan->ctrl_base + ATA_REG_ALTSTATUS));
    (void)ow_inb((uint16_t)(chan->ctrl_base + ATA_REG_ALTSTATUS));
    (void)ow_inb((uint16_t)(chan->ctrl_base + ATA_REG_ALTSTATUS));
    (void)ow_inb((uint16_t)(chan->ctrl_base + ATA_REG_ALTSTATUS));
}

static ata_status_t ata_wait_not_busy(const ata_channel_t *chan)
{
    for (uint32_t i = 0u; i < ATA_SPIN_LIMIT; ++i) {
        if ((ata_read_status(chan) & ATA_SR_BSY) == 0u) {
            return ATA_OK;
        }
    }
    return ATA_ERR_TIMEOUT;
}

static ata_status_t ata_poll_ready(const ata_channel_t *chan)
{
    for (uint32_t i = 0u; i < ATA_SPIN_LIMIT; ++i) {
        uint8_t st = ata_read_status(chan);

        if ((st & ATA_SR_BSY) != 0u) {
            continue;
        }
        if ((st & (ATA_SR_DF | ATA_SR_ERR)) != 0u) {
            return ATA_ERR_DEVICE_FAULT;
        }
        return ATA_OK;
    }
    return ATA_ERR_TIMEOUT;
}

static ata_status_t ata_poll_drq(const ata_channel_t *chan)
{
    for (uint32_t i = 0u; i < ATA_SPIN_LIMIT; ++i) {
        uint8_t st = ata_read_status(chan);

        if ((st & ATA_SR_BSY) != 0u) {
            continue;
        }
        if ((st & (ATA_SR_DF | ATA_SR_ERR)) != 0u) {
            return ATA_ERR_DEVICE_FAULT;
        }
        if ((st & ATA_SR_DRQ) != 0u) {
            return ATA_OK;
        }
    }
    return ATA_ERR_TIMEOUT;
}

static void ata_write_devsel(const ata_channel_t *chan, uint8_t devsel)
{
    ow_outb((uint16_t)(chan->io_base + ATA_REG_DEVSEL), devsel);
    ata_delay_400ns(chan);
}

/* ── IDENTIFY parsing ─────────────────────────────────────────── */

static char ata_fixed_char(uint16_t word, bool high)
{
    uint8_t byte = high ? (uint8_t)(word >> 8) : (uint8_t)(word & 0xFFu);
    if ((byte < 0x20u) || (byte > 0x7Eu)) {
        return ' ';
    }
    return (char)byte;
}

static void ata_copy_words(char *dst, const uint16_t *id, uint32_t first,
                           uint32_t count, uint32_t cap)
{
    uint32_t chars = count * 2u;
    uint32_t n = (chars < (cap - 1u)) ? chars : (cap - 1u);

    for (uint32_t i = 0u; i < n; ++i) {
        uint16_t word = id[first + (i / 2u)];
        dst[i] = ata_fixed_char(word, (i % 2u) == 0u);
    }
    dst[n] = '\0';

    while ((n > 0u) && (dst[n - 1u] == ' ')) {
        dst[n - 1u] = '\0';
        n--;
    }
}

static void ata_parse_identify(ata_device_t *dev, const uint16_t *id)
{
    dev->atapi = (id[0] & 0x8000u) != 0u;
    dev->lba_supported = (id[49] & (1u << 9)) != 0u;
    dev->lba48 = (id[83] & (1u << 10)) != 0u;

    ata_copy_words(dev->serial, id, 10u, 10u, ATA_SERIAL_LEN);
    ata_copy_words(dev->firmware, id, 23u, 4u, ATA_FIRMWARE_LEN);
    ata_copy_words(dev->model, id, 27u, 20u, ATA_MODEL_LEN);

    /* Logical sector size: word 106 valid (bit 14) and >512 (bit 12). */
    if (((id[106] & 0x4000u) != 0u) && ((id[106] & 0x1000u) != 0u)) {
        uint32_t shift = (uint32_t)(id[106] & 0x0Fu);
        dev->sector_size = (shift < 31u) ? (1u << shift) : ATA_SECTOR_SIZE;
    } else {
        dev->sector_size = ATA_SECTOR_SIZE;
    }

    if (dev->lba48) {
        dev->sector_count = (uint64_t)id[100]
                          | ((uint64_t)id[101] << 16)
                          | ((uint64_t)id[102] << 32)
                          | ((uint64_t)id[103] << 48);
    } else {
        dev->sector_count = ((uint32_t)id[61] << 16) | (uint32_t)id[60];
    }
}

/* ── PIO data transfer ────────────────────────────────────────── */

static ata_status_t ata_pio_transfer(const ata_channel_t *chan, uint8_t *buf,
                                     uint32_t sectors, bool write)
{
    uint16_t data_port = (uint16_t)(chan->io_base + ATA_REG_DATA);

    for (uint32_t s = 0u; s < sectors; ++s) {
        ata_status_t st = ata_poll_drq(chan);
        uint32_t base = s * (ATA_SECTOR_SIZE);

        if (st != ATA_OK) {
            return st;
        }

        if (write) {
            for (uint32_t i = 0u; i < ATA_WORDS_PER_SECTOR; ++i) {
                uint16_t w = (uint16_t)buf[base + (i * 2u)]
                           | (uint16_t)((uint16_t)buf[base + (i * 2u) + 1u] << 8);
                ow_outw(data_port, w);
            }
        } else {
            for (uint32_t i = 0u; i < ATA_WORDS_PER_SECTOR; ++i) {
                uint16_t w = ow_inw(data_port);
                buf[base + (i * 2u)] = (uint8_t)(w & 0xFFu);
                buf[base + (i * 2u) + 1u] = (uint8_t)(w >> 8);
            }
        }
        ata_delay_400ns(chan);
    }
    return ata_poll_ready(chan);
}

/* ── Command issue ────────────────────────────────────────────── */

static ata_status_t ata_issue_rw(const ata_channel_t *chan, uint32_t device,
                                 bool lba48, uint64_t lba, uint16_t count,
                                 bool write, uint8_t *buf)
{
    uint16_t io = chan->io_base;
    uint8_t devsel;
    ata_status_t st;

    devsel = lba48 ? (uint8_t)(0x40u | ((device & 1u) << 4))
                   : (uint8_t)(0xE0u | ((device & 1u) << 4)
                               | (uint8_t)((lba >> 24) & 0x0Fu));
    ata_write_devsel(chan, devsel);

    st = ata_wait_not_busy(chan);
    if (st != ATA_OK) {
        return st;
    }

    if (lba48) {
        ow_outb((uint16_t)(io + ATA_REG_SECCOUNT), (uint8_t)(count >> 8));
        ow_outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)((lba >> 24) & 0xFFu));
        ow_outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 32) & 0xFFu));
        ow_outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 40) & 0xFFu));
    }

    ow_outb((uint16_t)(io + ATA_REG_SECCOUNT), (uint8_t)(count & 0xFFu));
    ow_outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)(lba & 0xFFu));
    ow_outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFFu));
    ow_outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFFu));

    if (write) {
        ow_outb((uint16_t)(io + ATA_REG_COMMAND),
                lba48 ? ATA_CMD_WRITE_EXT : ATA_CMD_WRITE);
    } else {
        ow_outb((uint16_t)(io + ATA_REG_COMMAND),
                lba48 ? ATA_CMD_READ_EXT : ATA_CMD_READ);
    }
    ata_delay_400ns(chan);

    return ata_pio_transfer(chan, buf, count, write);
}

/* ── Public API ───────────────────────────────────────────────── */

ata_status_t ata_init(ata_channel_t *chan, uint16_t io_base, uint16_t ctrl_base)
{
    if ((chan == NULL) || (io_base == 0u)) {
        return ATA_ERR_INVALID_PARAM;
    }

    ow_memset(chan, 0, sizeof(*chan));
    chan->io_base = io_base;
    chan->ctrl_base = ctrl_base;

    /* Soft reset: assert SRST on the device control register, then clear it. */
    ow_outb((uint16_t)(ctrl_base + ATA_REG_ALTSTATUS), 0x04u);
    ata_delay_400ns(chan);
    ow_outb((uint16_t)(ctrl_base + ATA_REG_ALTSTATUS), 0x00u);
    ata_delay_400ns(chan);
    (void)ata_wait_not_busy(chan);

    chan->initialized = true;

    for (uint32_t device = 0u; device < 2u; ++device) {
        (void)ata_identify(chan, device);
    }
    return ATA_OK;
}

ata_status_t ata_identify(ata_channel_t *chan, uint32_t device)
{
    ata_device_t *dev;
    uint16_t io;
    uint8_t st;
    ata_status_t status;

    if ((chan == NULL) || !chan->initialized) {
        return ATA_ERR_NOT_INITIALIZED;
    }
    if (device > 1u) {
        return ATA_ERR_INVALID_PARAM;
    }

    dev = &chan->devices[device];
    ow_memset(dev, 0, sizeof(*dev));
    dev->sector_size = ATA_SECTOR_SIZE;
    io = chan->io_base;

    ata_write_devsel(chan, (uint8_t)(0xA0u | ((device & 1u) << 4)));
    if (ata_wait_not_busy(chan) != ATA_OK) {
        return ATA_ERR_TIMEOUT;
    }

    ow_outb((uint16_t)(io + ATA_REG_SECCOUNT), 0u);
    ow_outb((uint16_t)(io + ATA_REG_LBA0), 0u);
    ow_outb((uint16_t)(io + ATA_REG_LBA1), 0u);
    ow_outb((uint16_t)(io + ATA_REG_LBA2), 0u);
    ow_outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_IDENTIFY);
    ata_delay_400ns(chan);

    st = ata_read_status(chan);
    if (st == 0u) {
        /* Floating bus: nothing is attached to this drive position. */
        return ATA_ERR_NO_DEVICE;
    }

    status = ata_poll_drq(chan);
    if (status != ATA_OK) {
        /* ATAPI and other packet devices abort IDENTIFY DEVICE. */
        return ATA_ERR_UNSUPPORTED;
    }

    for (uint32_t i = 0u; i < ATA_WORDS_PER_SECTOR; ++i) {
        g_ata_identify[i] = ow_inw((uint16_t)(io + ATA_REG_DATA));
    }
    ata_delay_400ns(chan);

    ata_parse_identify(dev, g_ata_identify);
    dev->present = !dev->atapi;
    return dev->present ? ATA_OK : ATA_ERR_UNSUPPORTED;
}

ata_status_t ata_read_sectors(ata_channel_t *chan, uint32_t device,
                              uint64_t lba, uint32_t count,
                              void *buf, size_t buf_size)
{
    const ata_device_t *dev;
    uint64_t bytes;
    bool lba48;

    if ((chan == NULL) || !chan->initialized) {
        return ATA_ERR_NOT_INITIALIZED;
    }
    if ((device > 1u) || (buf == NULL)) {
        return ATA_ERR_INVALID_PARAM;
    }
    dev = &chan->devices[device];
    if (!dev->present) {
        return ATA_ERR_NO_DEVICE;
    }
    if ((count == 0u) || (count > ATA_PIO_MAX_SECTORS)) {
        return ATA_ERR_INVALID_PARAM;
    }

    bytes = (uint64_t)count * (uint64_t)dev->sector_size;
    if (bytes > (uint64_t)buf_size) {
        return ATA_ERR_BUFFER_TOO_SMALL;
    }

    lba48 = dev->lba48;
    if (!lba48 && ((lba + count) > 0x10000000u)) {
        return ATA_ERR_UNSUPPORTED;   /* beyond the 28-bit LBA range */
    }
    return ata_issue_rw(chan, device, lba48, lba, (uint16_t)count, false,
                        (uint8_t *)buf);
}

ata_status_t ata_write_sectors(ata_channel_t *chan, uint32_t device,
                               uint64_t lba, uint32_t count,
                               const void *buf, size_t buf_size)
{
    const ata_device_t *dev;
    uint64_t bytes;
    bool lba48;

    if ((chan == NULL) || !chan->initialized) {
        return ATA_ERR_NOT_INITIALIZED;
    }
    if ((device > 1u) || (buf == NULL)) {
        return ATA_ERR_INVALID_PARAM;
    }
    dev = &chan->devices[device];
    if (!dev->present) {
        return ATA_ERR_NO_DEVICE;
    }
    if ((count == 0u) || (count > ATA_PIO_MAX_SECTORS)) {
        return ATA_ERR_INVALID_PARAM;
    }

    bytes = (uint64_t)count * (uint64_t)dev->sector_size;
    if (bytes > (uint64_t)buf_size) {
        return ATA_ERR_BUFFER_TOO_SMALL;
    }

    lba48 = dev->lba48;
    if (!lba48 && ((lba + count) > 0x10000000u)) {
        return ATA_ERR_UNSUPPORTED;
    }
    return ata_issue_rw(chan, device, lba48, lba, (uint16_t)count, true,
                        (uint8_t *)(uintptr_t)buf);
}
