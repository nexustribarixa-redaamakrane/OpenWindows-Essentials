/*
 * ahci.c - OpenWindows Advanced Host Controller Interface (SATA) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation: the command lists, FIS
 * receive areas and command tables live in a file-scope static DMA pool.
 * The boot environment identity-maps low physical memory, so the virtual
 * addresses of that pool are used directly as bus addresses.
 */

#include "ahci.h"

/* ── Tiny local helpers ───────────────────────────────────────── */

static void ow_memset(void *dst, int value, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < len; ++i) {
        d[i] = (uint8_t)value;
    }
}

/* Compiler barrier: keeps MMIO ordering intact around command issue. */
static void ahci_barrier(void)
{
    __asm__ __volatile__("" ::: "memory");
}

/* ── Register map (ABAR relative) ─────────────────────────────── */

#define AHCI_CAP             0x00u
#define AHCI_GHC             0x04u
#define AHCI_IS              0x08u
#define AHCI_PI              0x0Cu
#define AHCI_VS              0x10u

#define AHCI_GHC_HR          (1u << 0)
#define AHCI_GHC_AE          (1u << 31)

#define AHCI_PORT_BASE       0x100u
#define AHCI_PORT_STRIDE     0x80u

#define AHCI_PXCLB           0x00u
#define AHCI_PXCLBU          0x04u
#define AHCI_PXFB           0x08u
#define AHCI_PXFBU          0x0Cu
#define AHCI_PXIS           0x10u
#define AHCI_PXIE           0x14u
#define AHCI_PXCMD          0x18u
#define AHCI_PXTFD          0x20u
#define AHCI_PXSIG          0x24u
#define AHCI_PXSSTS         0x28u
#define AHCI_PXSCTL         0x2Cu
#define AHCI_PXSERR         0x30u
#define AHCI_PXSACT         0x34u
#define AHCI_PXCI           0x38u

#define AHCI_PXCMD_ST        (1u << 0)
#define AHCI_PXCMD_SUD       (1u << 1)
#define AHCI_PXCMD_POD       (1u << 2)
#define AHCI_PXCMD_FRE       (1u << 4)
#define AHCI_PXCMD_FR        (1u << 14)
#define AHCI_PXCMD_CR        (1u << 15)

#define AHCI_PXIS_TFES       (1u << 30)
#define AHCI_PXTFD_ERR       (1u << 0)
#define AHCI_PXTFD_DRQ       (1u << 3)
#define AHCI_PXTFD_BSY       (1u << 7)

#define AHCI_SIG_ATA         0x00000101u
#define AHCI_SIG_ATAPI       0xEB140101u
#define AHCI_SIG_SEMB        0xC33C0101u
#define AHCI_SIG_PM          0x96690101u

#define AHCI_CMD_READ_DMA_EXT   0x25u
#define AHCI_CMD_WRITE_DMA_EXT  0x35u
#define AHCI_CMD_IDENTIFY       0xECu

#define AHCI_H2D_FIS         0x27u
#define AHCI_FIS_DWORDS      5u
#define AHCI_DEV_LBA         0x40u
#define AHCI_SPIN_LIMIT      2000000u

/* ── MMIO accessors ───────────────────────────────────────────── */

static uint32_t ahci_rd32(uintptr_t abar, uint32_t off)
{
    return *(volatile uint32_t *)(abar + (uintptr_t)off);
}

static void ahci_wr32(uintptr_t abar, uint32_t off, uint32_t value)
{
    *(volatile uint32_t *)(abar + (uintptr_t)off) = value;
}

static uint32_t ahci_port_base(uint32_t port)
{
    return AHCI_PORT_BASE + (port * AHCI_PORT_STRIDE);
}

static uint32_t ahci_phys_lo(const void *p)
{
    return (uint32_t)(uintptr_t)p;
}

static uint32_t ahci_phys_hi(const void *p)
{
    return (uint32_t)(((uint64_t)(uintptr_t)p) >> 32);
}

/* ── Command structures ───────────────────────────────────────── */

/* One command list entry (32 bytes): header plus FIS/table pointers. */
typedef struct {
    uint32_t dw0;      /* CFL, A, W, P, R, B, C, PMP */
    uint32_t prdtl;    /* physical region descriptor table length */
    uint32_t prdbc;    /* PRD byte count transferred */
    uint32_t ctba;     /* command table base address (low) */
    uint32_t ctbau;    /* command table base address (high) */
    uint32_t rsvd[3];  /* reserved (DW5..DW7) */
} ahci_cmd_header_t;

/* One physical region descriptor entry (16 bytes). */
typedef struct {
    uint32_t dba;      /* data base address (low) */
    uint32_t dbau;     /* data base address (high) */
    uint32_t rsvd;     /* reserved */
    uint32_t dbc;      /* byte count - 1, bit 31 = interrupt on completion */
} ahci_prdt_entry_t;

/* Command table holding the constructed FIS and the PRDT. */
typedef struct {
    uint8_t           cfis[64];  /* command FIS */
    uint8_t           acmd[16];  /* ATAPI command */
    uint8_t           rsvd[48];  /* reserved */
    ahci_prdt_entry_t prdt[AHCI_MAX_PRDT];
} ahci_cmd_table_t;

/* ── Static DMA pool ──────────────────────────────────────────── */

typedef struct {
    ahci_cmd_header_t cmd_list[AHCI_MAX_PORTS][AHCI_MAX_SLOTS];
    uint8_t           fis_rx[AHCI_MAX_PORTS][256];
    ahci_cmd_table_t  cmd_tab[AHCI_MAX_PORTS];
} ahci_dma_pool_t;

static ahci_dma_pool_t g_ahci_dma __attribute__((aligned(1024)));

/* Scratch buffer for IDENTIFY DEVICE (512 bytes). */
static uint16_t g_ahci_identify[256] __attribute__((aligned(512)));

/* ── Register polling helpers ─────────────────────────────────── */

static ahci_status_t ahci_wait_clear(uintptr_t abar, uint32_t off, uint32_t mask)
{
    for (uint32_t i = 0; i < AHCI_SPIN_LIMIT; ++i) {
        if ((ahci_rd32(abar, off) & mask) == 0u) {
            return AHCI_OK;
        }
    }
    return AHCI_ERR_TIMEOUT;
}

static ahci_status_t ahci_wait_ready(uintptr_t abar, uint32_t pbase)
{
    for (uint32_t i = 0; i < AHCI_SPIN_LIMIT; ++i) {
        uint32_t tfd = ahci_rd32(abar, pbase + AHCI_PXTFD);
        if ((tfd & (AHCI_PXTFD_BSY | AHCI_PXTFD_DRQ)) == 0u) {
            return AHCI_OK;
        }
    }
    return AHCI_ERR_TIMEOUT;
}

/* ── Port bring-up ────────────────────────────────────────────── */

static void ahci_start_port(uintptr_t abar, uint32_t port)
{
    uint32_t pbase = ahci_port_base(port);
    uint32_t cmd = ahci_rd32(abar, pbase + AHCI_PXCMD);

    /* Quiesce the command engine before (re)programming the list bases. */
    cmd &= ~AHCI_PXCMD_ST;
    ahci_wr32(abar, pbase + AHCI_PXCMD, cmd);
    (void)ahci_wait_clear(abar, pbase + AHCI_PXCMD, AHCI_PXCMD_CR);

    cmd &= ~AHCI_PXCMD_FRE;
    ahci_wr32(abar, pbase + AHCI_PXCMD, cmd);
    (void)ahci_wait_clear(abar, pbase + AHCI_PXCMD, AHCI_PXCMD_FR);

    ahci_wr32(abar, pbase + AHCI_PXCLB, ahci_phys_lo(&g_ahci_dma.cmd_list[port][0]));
    ahci_wr32(abar, pbase + AHCI_PXCLBU, ahci_phys_hi(&g_ahci_dma.cmd_list[port][0]));
    ahci_wr32(abar, pbase + AHCI_PXFB, ahci_phys_lo(&g_ahci_dma.fis_rx[port][0]));
    ahci_wr32(abar, pbase + AHCI_PXFBU, ahci_phys_hi(&g_ahci_dma.fis_rx[port][0]));

    ahci_wr32(abar, pbase + AHCI_PXIS, 0xFFFFFFFFu);
    ahci_wr32(abar, pbase + AHCI_PXSERR, 0xFFFFFFFFu);

    cmd = ahci_rd32(abar, pbase + AHCI_PXCMD);
    cmd |= AHCI_PXCMD_FRE | AHCI_PXCMD_ST;
    ahci_wr32(abar, pbase + AHCI_PXCMD, cmd);
}

/* ── Command issue ────────────────────────────────────────────── */

/* Build the host-to-device register FIS inside a command table. */
static void ahci_build_fis(ahci_cmd_table_t *ct, uint8_t command, uint64_t lba,
                           uint16_t count, uint8_t device)
{
    uint8_t *fis = ct->cfis;

    ow_memset(fis, 0, 64u);
    fis[0] = AHCI_H2D_FIS;            /* FIS type: register H2D */
    fis[1] = 0x80u;                   /* C = 1, PM port 0 */
    fis[2] = command;
    fis[3] = 0x00u;                   /* feature low */
    fis[4] = (uint8_t)(lba & 0xFFu);
    fis[5] = (uint8_t)((lba >> 8) & 0xFFu);
    fis[6] = (uint8_t)((lba >> 16) & 0xFFu);
    fis[7] = device;
    fis[8] = (uint8_t)((lba >> 24) & 0xFFu);
    fis[9] = (uint8_t)((lba >> 32) & 0xFFu);
    fis[10] = (uint8_t)((lba >> 40) & 0xFFu);
    fis[11] = 0x00u;                  /* feature high */
    fis[12] = (uint8_t)(count & 0xFFu);
    fis[13] = (uint8_t)((count >> 8) & 0xFFu);
    fis[14] = 0x00u;                  /* ICC */
    fis[15] = 0x00u;                  /* control */
}

/* Issue one ATA command through slot 0 of the port's command list. */
static ahci_status_t ahci_exec(ahci_controller_t *ctrl, uint32_t port,
                               uint8_t command, uint64_t lba, uint16_t count,
                               const void *data, uint32_t byte_count, bool write)
{
    uintptr_t abar = ctrl->abar;
    uint32_t pbase = ahci_port_base(port);
    ahci_cmd_header_t *cl = &g_ahci_dma.cmd_list[port][0];
    ahci_cmd_table_t *ct = &g_ahci_dma.cmd_tab[port];
    uint32_t prdtl = 0u;
    ahci_status_t status;

    ow_memset(cl, 0, sizeof(*cl));
    ow_memset(ct->prdt, 0, sizeof(ct->prdt));

    ahci_build_fis(ct, command, lba, count, AHCI_DEV_LBA);

    /* Lay out the caller's buffer as one or more PRDT runs. */
    if (byte_count != 0u) {
        uintptr_t dba = (uintptr_t)data;
        uint32_t remaining = byte_count;

        while ((remaining != 0u) && (prdtl < AHCI_MAX_PRDT)) {
            uint32_t chunk = remaining;
            ahci_prdt_entry_t *prd = &ct->prdt[prdtl];

            if (chunk > AHCI_PRDT_CHUNK) {
                chunk = AHCI_PRDT_CHUNK;
            }
            prd->dba = (uint32_t)dba;
            prd->dbau = (uint32_t)(((uint64_t)dba) >> 32);
            prd->rsvd = 0u;
            prd->dbc = (chunk - 1u) | (1u << 31);
            dba += chunk;
            remaining -= chunk;
            prdtl++;
        }
        if (remaining != 0u) {
            return AHCI_ERR_UNSUPPORTED;
        }
    }

    cl->dw0 = AHCI_FIS_DWORDS | (write ? (1u << 6) : 0u);
    cl->prdtl = prdtl;
    cl->prdbc = 0u;
    cl->ctba = ahci_phys_lo(ct);
    cl->ctbau = ahci_phys_hi(ct);

    ahci_wr32(abar, pbase + AHCI_PXIS, 0xFFFFFFFFu);
    ahci_wr32(abar, pbase + AHCI_PXSERR, 0xFFFFFFFFu);

    status = ahci_wait_ready(abar, pbase);
    if (status != AHCI_OK) {
        return status;
    }

    ahci_barrier();
    ahci_wr32(abar, pbase + AHCI_PXCI, 1u);

    for (uint32_t i = 0; i < AHCI_SPIN_LIMIT; ++i) {
        if ((ahci_rd32(abar, pbase + AHCI_PXCI) & 1u) == 0u) {
            break;
        }
    }
    if ((ahci_rd32(abar, pbase + AHCI_PXCI) & 1u) != 0u) {
        return AHCI_ERR_TIMEOUT;
    }

    if ((ahci_rd32(abar, pbase + AHCI_PXIS) & AHCI_PXIS_TFES) != 0u) {
        return AHCI_ERR_DEVICE_FAULT;
    }
    if ((ahci_rd32(abar, pbase + AHCI_PXTFD) & AHCI_PXTFD_ERR) != 0u) {
        return AHCI_ERR_DEVICE_FAULT;
    }
    return AHCI_OK;
}

/* ── Device identification ────────────────────────────────────── */

static char ahci_model_char(uint16_t word, bool high)
{
    uint8_t byte = high ? (uint8_t)(word >> 8) : (uint8_t)(word & 0xFFu);
    if ((byte < 0x20u) || (byte > 0x7Eu)) {
        return ' ';
    }
    return (char)byte;
}

static void ahci_parse_identify(ahci_port_info_t *info, const uint16_t *id)
{
    bool lba48;

    for (uint32_t i = 0; i < 20u; ++i) {
        info->model[i * 2u] = ahci_model_char(id[27u + i], true);
        info->model[(i * 2u) + 1u] = ahci_model_char(id[27u + i], false);
    }
    info->model[40] = '\0';

    /* Trim trailing blanks from the fixed-width model field. */
    for (uint32_t i = 40u; i > 0u; --i) {
        if (info->model[i - 1u] != ' ') {
            info->model[i] = '\0';
            break;
        }
        info->model[i - 1u] = '\0';
    }

    lba48 = ((id[83] & (1u << 10)) != 0u) && ((id[86] & (1u << 10)) != 0u);
    if (lba48) {
        uint64_t words = (uint64_t)id[100]
                       | ((uint64_t)id[101] << 16)
                       | ((uint64_t)id[102] << 32)
                       | ((uint64_t)id[103] << 48);
        info->sector_count = words;
    } else {
        info->sector_count = ((uint32_t)id[61] << 16) | (uint32_t)id[60];
    }
    info->sector_size = AHCI_SECTOR_SIZE;
}

static bool ahci_identify_device(ahci_controller_t *ctrl, uint32_t port,
                                 ahci_port_info_t *info)
{
    ahci_status_t status = ahci_exec(ctrl, port, AHCI_CMD_IDENTIFY, 0u, 1u,
                                     g_ahci_identify, AHCI_SECTOR_SIZE, false);

    if (status != AHCI_OK) {
        return false;
    }
    ahci_parse_identify(info, g_ahci_identify);
    return true;
}

static void ahci_classify_port(ahci_controller_t *ctrl, uint32_t port,
                               ahci_port_info_t *info)
{
    uint32_t pbase = ahci_port_base(port);
    uint32_t ssts = ahci_rd32(ctrl->abar, pbase + AHCI_PXSSTS);
    uint32_t sig = ahci_rd32(ctrl->abar, pbase + AHCI_PXSIG);

    info->sector_size = AHCI_SECTOR_SIZE;
    info->interface_speed = (uint8_t)((ssts >> 4) & 0x0Fu);
    info->present = ((ssts & 0x0Fu) == 0x03u);

    if (sig == AHCI_SIG_ATA) {
        info->type = AHCI_PORT_SATA;
    } else if (sig == AHCI_SIG_ATAPI) {
        info->type = AHCI_PORT_SATAPI;
    } else if (sig == AHCI_SIG_SEMB) {
        info->type = AHCI_PORT_SEMB;
    } else if (sig == AHCI_SIG_PM) {
        info->type = AHCI_PORT_PM;
    } else if (sig != 0u) {
        info->type = AHCI_PORT_UNKNOWN;
    } else {
        info->type = AHCI_PORT_NONE;
    }

    if (info->present && (info->type == AHCI_PORT_SATA)) {
        (void)ahci_identify_device(ctrl, port, info);
    }
}

/* ── Public API ───────────────────────────────────────────────── */

ahci_status_t ahci_init(ahci_controller_t *ctrl, uintptr_t abar)
{
    if ((ctrl == NULL) || (abar == 0u)) {
        return AHCI_ERR_INVALID_PARAM;
    }

    ow_memset(ctrl, 0, sizeof(*ctrl));
    ctrl->abar = abar;

    /* HBA reset: set GHC.HR and wait for the hardware to self-clear it. */
    ahci_wr32(abar, AHCI_GHC, AHCI_GHC_HR);
    if (ahci_wait_clear(abar, AHCI_GHC, AHCI_GHC_HR) != AHCI_OK) {
        return AHCI_ERR_TIMEOUT;
    }

    /* Re-enable AHCI mode and read the identification registers. */
    ahci_wr32(abar, AHCI_GHC, AHCI_GHC_AE);
    ahci_wr32(abar, AHCI_IS, 0xFFFFFFFFu);

    ctrl->cap = ahci_rd32(abar, AHCI_CAP);
    ctrl->version = ahci_rd32(abar, AHCI_VS);
    ctrl->implemented = ahci_rd32(abar, AHCI_PI);
    ctrl->initialized = true;

    (void)ahci_probe_ports(ctrl);
    return AHCI_OK;
}

uint32_t ahci_probe_ports(ahci_controller_t *ctrl)
{
    if ((ctrl == NULL) || !ctrl->initialized) {
        return 0u;
    }

    ctrl->implemented = ahci_rd32(ctrl->abar, AHCI_PI);
    ctrl->port_count = 0u;

    for (uint32_t port = 0u; port < AHCI_MAX_PORTS; ++port) {
        if ((ctrl->implemented & (1u << port)) == 0u) {
            continue;
        }

        ahci_port_info_t *info = &ctrl->ports[ctrl->port_count];
        ow_memset(info, 0, sizeof(*info));
        info->port_index = port;
        info->implemented = true;

        ahci_start_port(ctrl->abar, port);
        ahci_classify_port(ctrl, port, info);
        ctrl->port_count++;
    }
    return ctrl->port_count;
}

const ahci_port_info_t *ahci_first_port(const ahci_controller_t *ctrl)
{
    if (ctrl == NULL) {
        return NULL;
    }
    for (uint32_t i = 0u; i < ctrl->port_count; ++i) {
        if (ctrl->ports[i].present) {
            return &ctrl->ports[i];
        }
    }
    return NULL;
}

ahci_status_t ahci_read_sectors(ahci_controller_t *ctrl, uint32_t port,
                                uint64_t lba, uint32_t count,
                                void *buf, size_t buf_size)
{
    uint64_t bytes;

    if ((ctrl == NULL) || !ctrl->initialized) {
        return AHCI_ERR_NOT_INITIALIZED;
    }
    if ((buf == NULL) || (port >= AHCI_MAX_PORTS)) {
        return AHCI_ERR_INVALID_PARAM;
    }
    if ((count == 0u) || (count > 0xFFFFu)) {
        return AHCI_ERR_INVALID_PARAM;
    }

    bytes = (uint64_t)count * (uint64_t)AHCI_SECTOR_SIZE;
    if (bytes > (uint64_t)buf_size) {
        return AHCI_ERR_BUFFER_TOO_SMALL;
    }
    if (bytes > ((uint64_t)AHCI_MAX_PRDT * (uint64_t)AHCI_PRDT_CHUNK)) {
        return AHCI_ERR_UNSUPPORTED;
    }
    return ahci_exec(ctrl, port, AHCI_CMD_READ_DMA_EXT, lba, (uint16_t)count,
                     buf, (uint32_t)bytes, false);
}

ahci_status_t ahci_write_sectors(ahci_controller_t *ctrl, uint32_t port,
                                 uint64_t lba, uint32_t count,
                                 const void *buf, size_t buf_size)
{
    uint64_t bytes;

    if ((ctrl == NULL) || !ctrl->initialized) {
        return AHCI_ERR_NOT_INITIALIZED;
    }
    if ((buf == NULL) || (port >= AHCI_MAX_PORTS)) {
        return AHCI_ERR_INVALID_PARAM;
    }
    if ((count == 0u) || (count > 0xFFFFu)) {
        return AHCI_ERR_INVALID_PARAM;
    }

    bytes = (uint64_t)count * (uint64_t)AHCI_SECTOR_SIZE;
    if (bytes > (uint64_t)buf_size) {
        return AHCI_ERR_BUFFER_TOO_SMALL;
    }
    if (bytes > ((uint64_t)AHCI_MAX_PRDT * (uint64_t)AHCI_PRDT_CHUNK)) {
        return AHCI_ERR_UNSUPPORTED;
    }
    return ahci_exec(ctrl, port, AHCI_CMD_WRITE_DMA_EXT, lba, (uint16_t)count,
                     buf, (uint32_t)bytes, true);
}
