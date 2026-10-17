/*
 * ahci.h - OpenWindows Advanced Host Controller Interface (SATA) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Maximum number of ports tracked by one controller instance. */
#define AHCI_MAX_PORTS       32u
/* Maximum command slots per port (CAP.NCS is at most 32). */
#define AHCI_MAX_SLOTS       32u
/* Physical region descriptor entries used per transfer. */
#define AHCI_MAX_PRDT        8u
/* Largest single PRDT data run (bytes). */
#define AHCI_PRDT_CHUNK      0x100000u
/* Native logical sector size for SATA block devices. */
#define AHCI_SECTOR_SIZE     512u
/* Model string exposed by IDENTIFY DEVICE (40 chars + NUL). */
#define AHCI_MODEL_LEN       41u

/* Status codes returned by every AHCI entry point. */
typedef enum {
    AHCI_OK = 0,
    AHCI_ERR_INVALID_PARAM,
    AHCI_ERR_NOT_INITIALIZED,
    AHCI_ERR_NO_CONTROLLER,
    AHCI_ERR_NO_PORT,
    AHCI_ERR_TIMEOUT,
    AHCI_ERR_DEVICE_FAULT,
    AHCI_ERR_UNSUPPORTED,
    AHCI_ERR_BUFFER_TOO_SMALL
} ahci_status_t;

/* Port classification derived from the PxSIG signature register. */
typedef enum {
    AHCI_PORT_NONE = 0,
    AHCI_PORT_SATA,
    AHCI_PORT_SATAPI,
    AHCI_PORT_SEMB,
    AHCI_PORT_PM,
    AHCI_PORT_UNKNOWN
} ahci_port_type_t;

/* One implemented HBA port and the device behind it. */
typedef struct {
    uint32_t         port_index;      /* HBA port number (0..31)             */
    bool             implemented;     /* PI bit set for this port            */
    bool             present;         /* PxSSTS.DET == 3 (device attached)   */
    ahci_port_type_t type;            /* PxSIG classification                 */
    uint8_t          interface_speed; /* 1 = Gen1, 2 = Gen2, 3 = Gen3        */
    uint64_t         sector_count;    /* capacity in 512-byte sectors        */
    uint32_t         sector_size;     /* bytes per logical sector            */
    char             model[AHCI_MODEL_LEN]; /* IDENTIFY model string         */
} ahci_port_info_t;

/* Controller instance. Caller owns the storage; no heap is used. */
typedef struct {
    uintptr_t        abar;            /* ABAR (BAR5) mapped base address     */
    uint32_t         version;         /* VS register                         */
    uint32_t         cap;             /* CAP register                        */
    uint32_t         implemented;     /* PI register                         */
    uint32_t         port_count;      /* entries built in ports[]            */
    ahci_port_info_t ports[AHCI_MAX_PORTS];
    bool             initialized;
} ahci_controller_t;

/* Reset the HBA via GHC and probe every implemented port (PI). */
ahci_status_t ahci_init(ahci_controller_t *ctrl, uintptr_t abar);

/* Rebuild the implemented-port list from PI; returns the port count. */
uint32_t ahci_probe_ports(ahci_controller_t *ctrl);

/* Return the first present port, or NULL when no device is attached. */
const ahci_port_info_t *ahci_first_port(const ahci_controller_t *ctrl);

/* Read `count` sectors from `lba` using READ DMA EXT (0x25). */
ahci_status_t ahci_read_sectors(ahci_controller_t *ctrl, uint32_t port,
                                uint64_t lba, uint32_t count,
                                void *buf, size_t buf_size);

/* Write `count` sectors at `lba` using WRITE DMA EXT (0x35). */
ahci_status_t ahci_write_sectors(ahci_controller_t *ctrl, uint32_t port,
                                 uint64_t lba, uint32_t count,
                                 const void *buf, size_t buf_size);

#endif /* AHCI_H */
