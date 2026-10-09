/*
 * classpnp.h - PnP Arbiter / Class Driver Bus Negotiator
 *
 * Enumerates devices on buses (PCI, USB, ISA, ACPI, Platform),
 * assigns resources (IRQs, MMIO ranges, DMA channels, I/O ports),
 * and negotiates class-level driver binding for the OpenWindows
 * bare-metal subsystem.
 *
 * Builds as classpnp.owc (OpenWindows Component).
 * C99 freestanding - caller-provided context, zero heap allocation.
 */

#ifndef CLASSPNP_H
#define CLASSPNP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  OWFS Cross-References                                              */
/* ------------------------------------------------------------------ */

#define CLASSPNP_OWFS_MAGIC         0x4F574653u  /* "OWFS"             */
#define CLASSPNP_OWFS_BLOCK_SIZE    0x1000u       /* 4 KiB block        */

/* ------------------------------------------------------------------ */
/*  Limits                                                             */
/* ------------------------------------------------------------------ */

#define CLASSPNP_MAX_DEVICES        64u
#define CLASSPNP_MAX_BUSES          16u
#define CLASSPNP_MAX_RESOURCES      32u
#define CLASSPNP_MAX_RESOURCE_PER_DEV 8u
#define CLASSPNP_BUS_ID_LEN         16u
#define CLASSPNP_NAME_LEN           32u

/* ------------------------------------------------------------------ */
/*  Device States                                                      */
/* ------------------------------------------------------------------ */

typedef enum {
    CLASSPNP_STATE_UNKNOWN            = 0,
    CLASSPNP_STATE_DETECTED           = 1,
    CLASSPNP_STATE_RESOURCES_ASSIGNED = 2,
    CLASSPNP_STATE_DRIVER_BOUND       = 3,
    CLASSPNP_STATE_FAILED             = 4,
    CLASSPNP_STATE_DISABLED           = 5
} classpnp_device_state_t;

/* ------------------------------------------------------------------ */
/*  Bus Types                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    CLASSPNP_BUS_PCI      = 0,
    CLASSPNP_BUS_USB      = 1,
    CLASSPNP_BUS_ISA      = 2,
    CLASSPNP_BUS_ACPIC    = 3,
    CLASSPNP_BUS_PLATFORM = 4
} classpnp_bus_type_t;

/* ------------------------------------------------------------------ */
/*  Resource Types                                                     */
/* ------------------------------------------------------------------ */

typedef enum {
    CLASSPNP_RESOURCE_IRQ          = 0,
    CLASSPNP_RESOURCE_MMIO         = 1,
    CLASSPNP_RESOURCE_DMA_CHANNEL  = 2,
    CLASSPNP_RESOURCE_IO_PORT      = 3,
    CLASSPNP_RESOURCE_MEMORY       = 4
} classpnp_resource_type_t;

/* ------------------------------------------------------------------ */
/*  Resource Flags                                                     */
/* ------------------------------------------------------------------ */

#define CLASSPNP_RES_FLAG_SHARABLE      0x01u
#define CLASSPNP_RES_FLAG_PREFETCHABLE  0x02u
#define CLASSPNP_RES_FLAG_READ_ONLY     0x04u
#define CLASSPNP_RES_FLAG_WRITE_ONLY    0x08u
#define CLASSPNP_RES_FLAG_OWFS_MAPPED   0x10u  /* Backed by OWFS block */

/* ------------------------------------------------------------------ */
/*  Status Codes — BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A77F): Fatal faults                          */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal warnings                    */
/*  C+ (0x0011AC00-0x0011ADFF): Communication / protocol              */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                    */
/* ------------------------------------------------------------------ */

typedef uint32_t classpnp_status_t;

#define CLASSPNP_OK                      0x00000000u  /* Success              */
/* B+ Fatal */
#define CLASSPNP_ERR_NO_CAPACITY         0x0011A000u  /* Device table full    */
#define CLASSPNP_ERR_NO_RESOURCES        0x0011A001u  /* Resource exhaustion  */
/* W+ Warning */
#define CLASSPNP_ERR_INVALID_STATE       0x0011A800u  /* State mismatch       */
#define CLASSPNP_ERR_DRIVER_NOT_BOUND    0x0011A801u  /* No driver attached   */
/* C+ Communication */
#define CLASSPNP_ERR_BUS_NOT_FOUND       0x0011AC00u  /* Bus enumeration miss */
/* S+ Soft */
#define CLASSPNP_ERR_INVALID_CTX         0x0011AE00u  /* Bad context pointer  */
#define CLASSPNP_ERR_INVALID_INDEX       0x0011AE01u  /* Bad device index     */
#define CLASSPNP_ERR_DRIVER_BOUND        0x0011AE02u  /* Already driver-bound */
#define CLASSPNP_ERR_NAME_TOO_LONG       0x0011AE03u  /* Name truncated       */
#define CLASSPNP_ERR_DEVICE_NOT_FOUND    0x0011AE04u  /* Device not found     */

/* ------------------------------------------------------------------ */
/*  Resource Descriptor                                                */
/*  24 bytes, naturally aligned                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    /* 0x00 */ uint8_t  type;          /* classpnp_resource_type_t     */
    /* 0x01 */ uint8_t  flags;         /* CLASSPNP_RES_FLAG_*          */
    /* 0x02 */ uint8_t  _pad[2];       /* alignment                   */
    /* 0x04 */ uint32_t base_lo;       /* Base address (low 32 bits)   */
    /* 0x08 */ uint32_t base_hi;       /* Base address (high 32 bits)  */
    /* 0x0C */ uint32_t length;        /* Length in bytes              */
    /* 0x10 */ uint32_t irq_number;    /* IRQ line (IRQ type only)     */
    /* 0x14 */ uint32_t reserved;      /* Must be zero                 */
} classpnp_resource_t;                 /* sizeof = 24                  */

/* ------------------------------------------------------------------ */
/*  Device Entry                                                       */
/*  ~128 bytes                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    /* 0x00 */ uint8_t  bus_idx;           /* Index into bus array     */
    /* 0x01 */ uint8_t  parent_bus;        /* Parent bus index         */
    /* 0x02 */ uint8_t  state;             /* classpnp_device_state_t  */
    /* 0x03 */ uint8_t  resource_count;    /* Assigned resource count   */
    /* 0x04 */ uint16_t vendor_id;         /* Vendor ID (PCI etc.)     */
    /* 0x06 */ uint16_t device_id;         /* Device ID                */
    /* 0x08 */ uint32_t device_class;      /* Class/subclass/prog_if   */
    /* 0x0C */ uint32_t flags;             /* Device flags             */
    /* 0x10 */ uint8_t  resource_indices[CLASSPNP_MAX_RESOURCE_PER_DEV];
    /* 0x18 */ uint32_t mmio_base;         /* Assigned MMIO base       */
    /* 0x1C */ uint32_t mmio_length;       /* Assigned MMIO length     */
    /* 0x20 */ uint32_t irq;               /* Assigned IRQ number      */
    /* 0x24 */ uint32_t dma_channel;        /* Assigned DMA channel     */
    /* 0x28 */ uint32_t io_port_base;       /* Assigned I/O port base  */
    /* 0x2C */ uint32_t io_port_length;     /* Assigned I/O port len   */
    /* 0x30 */ char     name[CLASSPNP_NAME_LEN];
    /* 0x50 */ char     driver_name[CLASSPNP_NAME_LEN];
    /* 0x70 */ uint8_t  _pad[4];
} classpnp_device_t;                       /* sizeof = 116 -> pad to 120 */

/* ------------------------------------------------------------------ */
/*  Bus Entry                                                          */
/*  ~80 bytes                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    /* 0x00 */ uint8_t  bus_type;          /* classpnp_bus_type_t      */
    /* 0x01 */ uint8_t  device_count;      /* Devices on this bus       */
    /* 0x02 */ uint8_t  _pad[2];           /* alignment                */
    /* 0x04 */ uint32_t bus_flags;         /* Bus capability flags     */
    /* 0x08 */ char     bus_id[CLASSPNP_BUS_ID_LEN]; /* Bus identifier */
    /* 0x18 */ uint8_t  device_indices[CLASSPNP_MAX_DEVICES];
    /* 0x58 */ uint32_t mmio_base;         /* Bus MMIO aperture base   */
    /* 0x5C */ uint32_t mmio_length;       /* Bus MMIO aperture length */
    /* 0x60 */ uint32_t reserved[4];       /* Must be zero             */
} classpnp_bus_t;                          /* sizeof = 72 -> pad to 80 */

/* ------------------------------------------------------------------ */
/*  Statistics                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t total_enumerations;
    uint32_t total_resource_assignments;
    uint32_t total_driver_binds;
    uint32_t total_driver_unbinds;
    uint32_t total_devices_disabled;
    uint32_t total_devices_reenabled;
    uint32_t total_resets;
} classpnp_stats_t;

/* ------------------------------------------------------------------ */
/*  Context (opaque to callers after init)                             */
/* ------------------------------------------------------------------ */

typedef struct {
    classpnp_device_t devices[CLASSPNP_MAX_DEVICES];
    classpnp_bus_t    buses[CLASSPNP_MAX_BUSES];
    classpnp_resource_t resources[CLASSPNP_MAX_RESOURCES];
    uint32_t          device_count;
    uint32_t          bus_count;
    uint32_t          resource_count;
    uint32_t          next_device_id;
    classpnp_stats_t  stats;
} classpnp_context_t;

/* ------------------------------------------------------------------ */
/*  Forward Declarations                                               */
/* ------------------------------------------------------------------ */

typedef struct classpnp_context classpnp_context_internal_t;

/* ------------------------------------------------------------------ */
/*  Initialization & Lifecycle                                         */
/* ------------------------------------------------------------------ */

/*
 * Initialize a PnP arbiter context. Zeros all state and sets
 * next_device_id to 1. Returns CLASSPNP_OK on success.
 */
classpnp_status_t classpnp_init(classpnp_context_t *ctx);

/*
 * Reset all state in the context to zero. After reset, the
 * context must be re-initialized with classpnp_init() before use.
 */
void classpnp_reset(classpnp_context_t *ctx);

/* ------------------------------------------------------------------ */
/*  Bus Enumeration                                                   */
/* ------------------------------------------------------------------ */

/*
 * Enumerate devices on a bus of the given type. If the bus is not
 * already registered, it is added to the bus array. Devices are
 * detected and added in DETECTED state. For PCI buses, creates
 * device entries with bus-specific vendor/device/class values.
 * Returns CLASSPNP_OK on success.
 */
classpnp_status_t classpnp_enumerate_bus(
    classpnp_context_t   *ctx,
    classpnp_bus_type_t   bus_type,
    const char           *bus_id
);

/* ------------------------------------------------------------------ */
/*  Resource Assignment                                               */
/* ------------------------------------------------------------------ */

/*
 * Assign hardware resources to a device. PCI devices receive MMIO
 * ranges from the MMIO pool; ISA devices receive IRQs from the IRQ
 * pool. Resources are drawn from predefined static pools.
 * Transitions device to RESOURCES_ASSIGNED on success.
 */
classpnp_status_t classpnp_assign_resources(
    classpnp_context_t *ctx,
    uint32_t            device_idx
);

/* ------------------------------------------------------------------ */
/*  Driver Binding                                                    */
/* ------------------------------------------------------------------ */

/*
 * Bind a class-level driver to a device. The driver_name string is
 * copied (max CLASSPNP_NAME_LEN - 1 characters). Device must be in
 * RESOURCES_ASSIGNED state. Transitions to DRIVER_BOUND on success.
 */
classpnp_status_t classpnp_bind_driver(
    classpnp_context_t *ctx,
    uint32_t            device_idx,
    const char         *driver_name
);

/*
 * Unbind the currently bound driver from a device. Device must be
 * in DRIVER_BOUND state. Transitions back to RESOURCES_ASSIGNED.
 */
classpnp_status_t classpnp_unbind_driver(
    classpnp_context_t *ctx,
    uint32_t            device_idx
);

/* ------------------------------------------------------------------ */
/*  Device Enable/Disable                                             */
/* ------------------------------------------------------------------ */

/*
 * Disable a device. Saves current state and transitions to DISABLED.
 */
classpnp_status_t classpnp_disable_device(
    classpnp_context_t *ctx,
    uint32_t            device_idx
);

/*
 * Re-enable a previously disabled device. Restores the saved state
 * (RESOURCES_ASSIGNED or DRIVER_BOUND).
 */
classpnp_status_t classpnp_reenable_device(
    classpnp_context_t *ctx,
    uint32_t            device_idx
);

/* ------------------------------------------------------------------ */
/*  Query Operations                                                  */
/* ------------------------------------------------------------------ */

/*
 * Get a pointer to a device entry by index. Returns NULL if the
 * index is out of range.
 */
const classpnp_device_t *classpnp_get_device(
    const classpnp_context_t *ctx,
    uint32_t                  device_idx
);

/*
 * Get a pointer to a bus entry by index. Returns NULL if the
 * index is out of range.
 */
const classpnp_bus_t *classpnp_get_bus(
    const classpnp_context_t *ctx,
    uint32_t                  bus_idx
);

/*
 * Find a device by vendor_id and device_id. Returns the index of
 * the first matching device, or -1 if not found.
 */
int32_t classpnp_find_device(
    const classpnp_context_t *ctx,
    uint16_t                  vendor_id,
    uint16_t                  device_id
);

#endif /* CLASSPNP_H */
