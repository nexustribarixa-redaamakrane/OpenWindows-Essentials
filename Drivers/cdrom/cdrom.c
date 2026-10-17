/*
 * cdrom.c - OpenWindows ATAPI CD-ROM Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation: the IDENTIFY PACKET
 * DEVICE buffer is file-scope static storage. Commands travel through the
 * ATAPI PACKET (0xA0) interface on the legacy task file.
 */

#include "cdrom.h"

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

#define REG_DATA        0u  /* data (16-bit) */
#define REG_ERROR       1u
#define REG_FEATURES    1u
#define REG_SECCOUNT    2u
#define REG_LBA0        3u
#define REG_LBA1        4u  /* ATAPI byte count limit low */
#define REG_LBA2        5u  /* ATAPI byte count limit high */
#define REG_DEVSEL      6u
#define REG_STATUS      7u
#define REG_COMMAND     7u
#define REG_ALTSTATUS   0u  /* relative to ctrl_base */

#define SR_BSY          0x80u
#define SR_DRDY         0x40u
#define SR_DF           0x20u
#define SR_DRQ          0x08u
#define SR_ERR          0x01u

#define ATAPI_CMD_PACKET          0xA0u
#define ATAPI_CMD_IDENTIFY_PACKET 0xA1u

#define SCSI_READ_12        0xA8u
#define SCSI_READ_CAPACITY  0x25u
#define SCSI_START_STOP     0x1Bu

#define CDROM_WORDS         256u
#define CDROM_SPIN_LIMIT    2000000u

/* ── Static IDENTIFY buffer ───────────────────────────────────── */

static uint16_t g_cdrom_identify[CDROM_WORDS] __attribute__((aligned(2)));

/* ── Register primitives ──────────────────────────────────────── */

static uint8_t atapi_status(const cdrom_device_t *cd)
{
    return ow_inb((uint16_t)(cd->io_base + REG_STATUS));
}

static void atapi_delay_400ns(const cdrom_device_t *cd)
{
    (void)ow_inb((uint16_t)(cd->ctrl_base + REG_ALTSTATUS));
    (void)ow_inb((uint16_t)(cd->ctrl_base + REG_ALTSTATUS));
    (void)ow_inb((uint16_t)(cd->ctrl_base + REG_ALTSTATUS));
    (void)ow_inb((uint16_t)(cd->ctrl_base + REG_ALTSTATUS));
}

static cdrom_status_t atapi_wait_not_busy(const cdrom_device_t *cd)
{
    for (uint32_t i = 0u; i < CDROM_SPIN_LIMIT; ++i) {
        if ((atapi_status(cd) & SR_BSY) == 0u) {
            return CDROM_OK;
        }
    }
    return CDROM_ERR_TIMEOUT;
}

static cdrom_status_t atapi_wait_drq(const cdrom_device_t *cd)
{
    for (uint32_t i = 0u; i < CDROM_SPIN_LIMIT; ++i) {
        uint8_t st = atapi_status(cd);

        if ((st & SR_BSY) != 0u) {
            continue;
        }
        if ((st & (SR_DF | SR_ERR)) != 0u) {
            return CDROM_ERR_DEVICE_FAULT;
        }
        if ((st & SR_DRQ) != 0u) {
            return CDROM_OK;
        }
    }
    return CDROM_ERR_TIMEOUT;
}

static cdrom_status_t atapi_wait_complete(const cdrom_device_t *cd)
{
    for (uint32_t i = 0u; i < CDROM_SPIN_LIMIT; ++i) {
        uint8_t st = atapi_status(cd);

        if ((st & SR_BSY) != 0u) {
            continue;
        }
        if ((st & SR_DRQ) != 0u) {
            continue;
        }
        if ((st & (SR_DF | SR_ERR)) != 0u) {
            return CDROM_ERR_DEVICE_FAULT;
        }
        return CDROM_OK;
    }
    return CDROM_ERR_TIMEOUT;
}

static void atapi_select(const cdrom_device_t *cd)
{
    ow_outb((uint16_t)(cd->io_base + REG_DEVSEL),
            (uint8_t)(0xA0u | ((cd->device & 1u) << 4)));
    atapi_delay_400ns(cd);
}

/* ── PACKET interface ─────────────────────────────────────────── */

static void atapi_send_packet(const cdrom_device_t *cd, const uint8_t *cdb)
{
    uint16_t data = (uint16_t)(cd->io_base + REG_DATA);

    for (uint32_t i = 0u; i < 6u; ++i) {
        uint16_t w = (uint16_t)cdb[i * 2u]
                   | (uint16_t)((uint16_t)cdb[(i * 2u) + 1u] << 8);
        ow_outw(data, w);
    }
}

static void atapi_read_data(const cdrom_device_t *cd, uint8_t *buf,
                            uint32_t bytes)
{
    uint16_t data = (uint16_t)(cd->io_base + REG_DATA);
    uint32_t words = bytes / 2u;

    for (uint32_t i = 0u; i < words; ++i) {
        uint16_t w = ow_inw(data);
        buf[i * 2u] = (uint8_t)(w & 0xFFu);
        buf[(i * 2u) + 1u] = (uint8_t)(w >> 8);
    }
    if ((bytes & 1u) != 0u) {
        uint16_t w = ow_inw(data);
        buf[bytes - 1u] = (uint8_t)(w & 0xFFu);
    }
}

/* Issue one 12-byte packet command and optionally capture its data-in. */
static cdrom_status_t cdrom_packet(cdrom_device_t *cd, const uint8_t *cdb,
                                   void *buf, uint32_t bytes, bool data_in)
{
    uint16_t io = cd->io_base;
    cdrom_status_t st;

    atapi_select(cd);

    st = atapi_wait_not_busy(cd);
    if (st != CDROM_OK) {
        return st;
    }

    /* Byte count limit lives in the cylinder low/high registers. */
    ow_outb((uint16_t)(io + REG_LBA1), (uint8_t)(bytes & 0xFFu));
    ow_outb((uint16_t)(io + REG_LBA2), (uint8_t)((bytes >> 8) & 0xFFu));
    ow_outb((uint16_t)(io + REG_FEATURES), 0x00u);
    ow_outb((uint16_t)(io + REG_SECCOUNT), 0x00u);

    ow_outb((uint16_t)(io + REG_COMMAND), ATAPI_CMD_PACKET);
    atapi_delay_400ns(cd);

    /* The device raises DRQ when it is ready for the command packet. */
    st = atapi_wait_drq(cd);
    if (st != CDROM_OK) {
        return st;
    }
    atapi_send_packet(cd, cdb);
    atapi_delay_400ns(cd);

    if (data_in && (bytes > 0u)) {
        st = atapi_wait_drq(cd);
        if (st != CDROM_OK) {
            return st;
        }
        atapi_read_data(cd, (uint8_t *)buf, bytes);
        atapi_delay_400ns(cd);
    }

    return atapi_wait_complete(cd);
}

/* ── IDENTIFY PACKET DEVICE parsing ───────────────────────────── */

static char atapi_char(uint16_t word, bool high)
{
    uint8_t byte = high ? (uint8_t)(word >> 8) : (uint8_t)(word & 0xFFu);
    if ((byte < 0x20u) || (byte > 0x7Eu)) {
        return ' ';
    }
    return (char)byte;
}

static cdrom_status_t atapi_identify(cdrom_device_t *cd)
{
    uint16_t io = cd->io_base;
    uint8_t st;

    atapi_select(cd);
    if (atapi_wait_not_busy(cd) != CDROM_OK) {
        return CDROM_ERR_TIMEOUT;
    }

    ow_outb((uint16_t)(io + REG_FEATURES), 0x00u);
    ow_outb((uint16_t)(io + REG_SECCOUNT), 0x00u);
    ow_outb((uint16_t)(io + REG_LBA0), 0x00u);
    ow_outb((uint16_t)(io + REG_LBA1), 0x00u);
    ow_outb((uint16_t)(io + REG_LBA2), 0x00u);
    ow_outb((uint16_t)(io + REG_COMMAND), ATAPI_CMD_IDENTIFY_PACKET);
    atapi_delay_400ns(cd);

    st = atapi_status(cd);
    if (st == 0u) {
        return CDROM_ERR_NO_DEVICE;
    }
    if (atapi_wait_drq(cd) != CDROM_OK) {
        return CDROM_ERR_DEVICE_FAULT;
    }

    for (uint32_t i = 0u; i < CDROM_WORDS; ++i) {
        g_cdrom_identify[i] = ow_inw((uint16_t)(io + REG_DATA));
    }
    atapi_delay_400ns(cd);

    cd->present = (g_cdrom_identify[0] & 0x8000u) != 0u;
    cd->removable = (g_cdrom_identify[0] & 0x0080u) != 0u;

    if ((uint32_t)((g_cdrom_identify[0] >> 8) & 0x1Fu) != CDROM_TYPE_CDROM) {
        cd->present = false;
        return CDROM_ERR_UNSUPPORTED;
    }

    for (uint32_t i = 0u; i < 20u; ++i) {
        cd->model[i * 2u] = atapi_char(g_cdrom_identify[27u + i], true);
        cd->model[(i * 2u) + 1u] = atapi_char(g_cdrom_identify[27u + i], false);
    }
    cd->model[40] = '\0';
    for (uint32_t i = 40u; i > 0u; --i) {
        if (cd->model[i - 1u] != ' ') {
            cd->model[i] = '\0';
            break;
        }
        cd->model[i - 1u] = '\0';
    }

    cd->lba_supported = (g_cdrom_identify[49] & (1u << 9)) != 0u;
    return cd->present ? CDROM_OK : CDROM_ERR_NO_DEVICE;
}

/* ── Public API ───────────────────────────────────────────────── */

cdrom_status_t cdrom_init(cdrom_device_t *cd, uint16_t io_base,
                          uint16_t ctrl_base, uint8_t device)
{
    cdrom_status_t status;

    if ((cd == NULL) || (io_base == 0u) || (device > 1u)) {
        return CDROM_ERR_INVALID_PARAM;
    }

    ow_memset(cd, 0, sizeof(*cd));
    cd->io_base = io_base;
    cd->ctrl_base = ctrl_base;
    cd->device = device;
    cd->sector_size = CDROM_SECTOR_SIZE;

    status = atapi_identify(cd);
    if (status != CDROM_OK) {
        return status;
    }

    cd->initialized = true;

    /* Cache the medium size; a failure only means the tray is empty. */
    (void)cdrom_capacity(cd, NULL);
    return CDROM_OK;
}

cdrom_status_t cdrom_read_sector(cdrom_device_t *cd, uint32_t lba,
                                 void *buf, size_t buf_size)
{
    uint8_t cdb[12];
    uint32_t size;

    if ((cd == NULL) || !cd->initialized) {
        return CDROM_ERR_NOT_INITIALIZED;
    }
    if (!cd->present || (buf == NULL)) {
        return CDROM_ERR_INVALID_PARAM;
    }

    size = (cd->sector_size != 0u) ? cd->sector_size : CDROM_SECTOR_SIZE;
    if ((size_t)size > buf_size) {
        return CDROM_ERR_BUFFER_TOO_SMALL;
    }

    ow_memset(cdb, 0, sizeof(cdb));
    cdb[0] = SCSI_READ_12;
    cdb[2] = (uint8_t)((lba >> 24) & 0xFFu);
    cdb[3] = (uint8_t)((lba >> 16) & 0xFFu);
    cdb[4] = (uint8_t)((lba >> 8) & 0xFFu);
    cdb[5] = (uint8_t)(lba & 0xFFu);
    cdb[8] = 0x01u;   /* transfer length = 1 block */

    return cdrom_packet(cd, cdb, buf, size, true);
}

cdrom_status_t cdrom_eject(cdrom_device_t *cd, bool eject)
{
    uint8_t cdb[12];

    if ((cd == NULL) || !cd->initialized) {
        return CDROM_ERR_NOT_INITIALIZED;
    }
    if (!cd->present) {
        return CDROM_ERR_NO_DEVICE;
    }

    ow_memset(cdb, 0, sizeof(cdb));
    cdb[0] = SCSI_START_STOP;
    /* LoEj = 1; Start = 1 loads the medium, 0 unloads it. */
    cdb[4] = (uint8_t)(0x02u | (eject ? 0x00u : 0x01u));

    return cdrom_packet(cd, cdb, NULL, 0u, false);
}

cdrom_status_t cdrom_capacity(cdrom_device_t *cd, uint32_t *last_lba)
{
    uint8_t cdb[12];
    uint8_t resp[8];
    uint32_t last;
    uint32_t block;
    cdrom_status_t status;

    if ((cd == NULL) || !cd->initialized) {
        return CDROM_ERR_NOT_INITIALIZED;
    }
    if (!cd->present) {
        return CDROM_ERR_NO_DEVICE;
    }

    ow_memset(cdb, 0, sizeof(cdb));
    cdb[0] = SCSI_READ_CAPACITY;

    status = cdrom_packet(cd, cdb, resp, sizeof(resp), true);
    if (status != CDROM_OK) {
        return status;
    }

    last = ((uint32_t)resp[0] << 24) | ((uint32_t)resp[1] << 16)
         | ((uint32_t)resp[2] << 8) | (uint32_t)resp[3];
    block = ((uint32_t)resp[4] << 24) | ((uint32_t)resp[5] << 16)
          | ((uint32_t)resp[6] << 8) | (uint32_t)resp[7];

    cd->last_lba = last;
    if (block != 0u) {
        cd->sector_size = block;
    }
    if (last_lba != NULL) {
        *last_lba = last;
    }
    return CDROM_OK;
}
