/*
 * classpnp.c - PnP Arbiter / Class Driver Bus Negotiator
 *
 * C99 freestanding implementation. Enumerates devices on buses,
 * assigns resources (IRQ, MMIO, DMA, I/O port), and negotiates
 * class-level driver binding for OpenWindows bare-metal subsystems.
 *
 * Builds as classpnp.owc (OpenWindows Component).
 * Build: cc -std=c99 -ffreestanding -nostdlib -c classpnp.c
 * Zero heap allocation. All memory is caller-provided.
 */

#include "classpnp.h"

/* ------------------------------------------------------------------ */
/*  Internal Constants                                                 */
/* ------------------------------------------------------------------ */

#define CLASSPNP_DRV_NAME_MAX  (CLASSPNP_NAME_LEN - 1)

/*
 * Flag bit within device.flags that encodes the previous state
 * before a disable operation. Bits [4:0] hold the 5-bit state
 * value; bit 7 indicates "was disabled" for re-enable logic.
 */
#define CLASSPNP_FLAG_PREV_STATE_MASK   0x1Fu
#define CLASSPNP_FLAG_DISABLED          0x80u

/* ------------------------------------------------------------------ */
/*  Static Resource Pools                                              */
/* ------------------------------------------------------------------ */

/*
 * IRQ pool: standard ISA IRQs excluding reserved/legacy lines.
 *   Reserved: 0 (timer), 1 (keyboard), 2 (cascade), 8 (RTC),
 *             13 (math coprocessor), 15 (secondary IDE).
 * Available: 3, 4, 5, 6, 7, 9, 10, 11, 12, 14.
 */
static const uint8_t classpnp_irq_pool[] = {
    3u, 4u, 5u, 6u, 7u, 9u, 10u, 11u, 12u, 14u
};

#define CLASSPNP_IRQ_POOL_SIZE \
    (sizeof(classpnp_irq_pool) / sizeof(classpnp_irq_pool[0]))

/*
 * MMIO pool: 1 MiB aligned ranges in the 0xF0000000 region.
 * Each entry is a base address; length is 0x100000 (1 MiB).
 * These correspond to the upper MMIO hole above OWFS block
 * storage (cross-ref: CLASSPNP_OWFS_BLOCK_SIZE = 0x1000).
 */
#define CLASSPNP_MMIO_BASE      0xF0000000u
#define CLASSPNP_MMIO_LENGTH    0x00100000u  /* 1 MiB per range       */
#define CLASSPNP_MMIO_STEP      0x00100000u  /* Aligned spacing       */
#define CLASSPNP_MMIO_POOL_SIZE 10u

/*
 * DMA channel pool: ISA DMA channels 3, 5, 6, 7
 * (channels 0-3 are 8-bit, 5-7 are 16-bit).
 */
static const uint8_t classpnp_dma_pool[] = {
    3u, 5u, 6u, 7u
};

#define CLASSPNP_DMA_POOL_SIZE \
    (sizeof(classpnp_dma_pool) / sizeof(classpnp_dma_pool[0]))

/*
 * I/O port pool: standard free I/O port ranges.
 * Each entry is a base; length is 0x100 (256 bytes).
 * Ranges avoid legacy devices (0x000-0x0FF, 0x200-0x2FF,
 * 0x3F8-0x3FF, 0x2F8-0x2FF, 0x378-0x37F).
 */
#define CLASSPNP_IOPORT_BASE    0x0400u
#define CLASSPNP_IOPORT_LENGTH  0x0100u
#define CLASSPNP_IOPORT_STEP    0x0100u
#define CLASSPNP_IOPORT_POOL_SIZE 10u

/* ------------------------------------------------------------------ */
/*  Usage Tracking (static, zero-initialized)                          */
/* ------------------------------------------------------------------ */

static bool classpnp_irq_used[CLASSPNP_IRQ_POOL_SIZE];
static bool classpnp_dma_used[CLASSPNP_DMA_POOL_SIZE];

/* ------------------------------------------------------------------ */
/*  Internal Utility Functions                                         */
/* ------------------------------------------------------------------ */

static void classpnp_memset(uint8_t *dst, uint8_t val, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        dst[i] = val;
}

static void classpnp_memcpy(uint8_t *dst, const uint8_t *src, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        dst[i] = src[i];
}

static uint32_t classpnp_strlen(const char *s)
{
    uint32_t len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

static void classpnp_strncpy(char *dst, const char *src, uint32_t max_len)
{
    uint32_t i;
    for (i = 0; i < max_len && src[i] != '\0'; i++)
        dst[i] = src[i];
    for (; i < max_len; i++)
        dst[i] = '\0';
}

static bool classpnp_str_equal(const char *a, const char *b, uint32_t max_len)
{
    for (uint32_t i = 0; i < max_len; i++) {
        if (a[i] != b[i])
            return false;
        if (a[i] == '\0')
            return true;
    }
    return true;
}

/*
 * Encode a simulated PCI vendor/device/class triple from a bus
 * index and device slot. This produces deterministic but unique
 * identifiers for simulated enumeration.
 */
static void classpnp_simulate_pci_ids(
    uint32_t bus_idx,
    uint32_t slot,
    uint16_t *vendor_out,
    uint16_t *device_out,
    uint32_t *class_out
)
{
    /* Vendor: 0x1234 for bus 0, 0x5678 for bus 1, etc. */
    *vendor_out = (uint16_t)(0x1234u + (bus_idx * 0x4444u));

    /* Device: incorporates slot index */
    *device_out = (uint16_t)(0x1000u + (bus_idx * 0x0100u) + slot);

    /* Class: 0x01 (mass storage) | subclass 0x06 (ATA) | prog_if 0x01 */
    *class_out = 0x010601u;
}

static void classpnp_simulate_usb_ids(
    uint32_t bus_idx,
    uint32_t slot,
    uint16_t *vendor_out,
    uint16_t *device_out,
    uint32_t *class_out
)
{
    *vendor_out = (uint16_t)(0x5678u + (bus_idx * 0x1111u));
    *device_out = (uint16_t)(0x2000u + (bus_idx * 0x0100u) + slot);
    /* Class: 0x09 (hub) | subclass 0x00 | prog_if 0x00 */
    *class_out = 0x090000u;
}

static void classpnp_simulate_isa_ids(
    uint32_t bus_idx,
    uint32_t slot,
    uint16_t *vendor_out,
    uint16_t *device_out,
    uint32_t *class_out
)
{
    (void)bus_idx;
    *vendor_out = 0x0001u;
    *device_out = (uint16_t)(0x3000u + slot);
    /* Class: 0x07 (simple comms) | subclass 0x01 (serial) */
    *class_out = 0x070100u;
}

static void classpnp_simulate_acpi_ids(
    uint32_t bus_idx,
    uint32_t slot,
    uint16_t *vendor_out,
    uint16_t *device_out,
    uint32_t *class_out
)
{
    (void)bus_idx;
    *vendor_out = 0x0006u;
    *device_out = (uint16_t)(0x4000u + slot);
    /* Class: 0x08 (base system) | subclass 0x05 (ACPI) */
    *class_out = 0x080500u;
}

static void classpnp_simulate_platform_ids(
    uint32_t bus_idx,
    uint32_t slot,
    uint16_t *vendor_out,
    uint16_t *device_out,
    uint32_t *class_out
)
{
    (void)bus_idx;
    *vendor_out = 0x000Fu;
    *device_out = (uint16_t)(0x5000u + slot);
    /* Class: 0x08 (base system) | subclass 0x09 (other) */
    *class_out = 0x080900u;
}

/* ------------------------------------------------------------------ */
/*  Initialization & Lifecycle                                         */
/* ------------------------------------------------------------------ */

classpnp_status_t classpnp_init(classpnp_context_t *ctx)
{
    if (!ctx)
        return CLASSPNP_ERR_INVALID_CTX;

    classpnp_memset((uint8_t *)ctx, 0, sizeof(classpnp_context_t));

    ctx->next_device_id = 1u;

    for (uint32_t i = 0; i < CLASSPNP_IRQ_POOL_SIZE; i++)
        classpnp_irq_used[i] = false;

    for (uint32_t i = 0; i < CLASSPNP_DMA_POOL_SIZE; i++)
        classpnp_dma_used[i] = false;

    return CLASSPNP_OK;
}

void classpnp_reset(classpnp_context_t *ctx)
{
    if (!ctx)
        return;

    classpnp_memset((uint8_t *)ctx, 0, sizeof(classpnp_context_t));

    for (uint32_t i = 0; i < CLASSPNP_IRQ_POOL_SIZE; i++)
        classpnp_irq_used[i] = false;

    for (uint32_t i = 0; i < CLASSPNP_DMA_POOL_SIZE; i++)
        classpnp_dma_used[i] = false;
}

/* ------------------------------------------------------------------ */
/*  Bus Enumeration                                                   */
/* ------------------------------------------------------------------ */

classpnp_status_t classpnp_enumerate_bus(
    classpnp_context_t   *ctx,
    classpnp_bus_type_t   bus_type,
    const char           *bus_id
)
{
    if (!ctx)
        return CLASSPNP_ERR_INVALID_CTX;

    if (!bus_id)
        return CLASSPNP_ERR_INVALID_CTX;

    if (ctx->bus_count >= CLASSPNP_MAX_BUSES)
        return CLASSPNP_ERR_NO_CAPACITY;

    /*
     * Check if bus already exists. If so, return its index
     * without adding a duplicate.
     */
    uint32_t existing_idx = 0;
    bool found = false;

    for (uint32_t i = 0; i < ctx->bus_count; i++) {
        classpnp_bus_t *b = &ctx->buses[i];
        if (b->bus_type == (uint8_t)bus_type &&
            classpnp_str_equal(b->bus_id, bus_id, CLASSPNP_BUS_ID_LEN)) {
            existing_idx = i;
            found = true;
            break;
        }
    }

    if (found) {
        (void)existing_idx;
        /* Bus already registered; enumeration is a no-op for duplicates */
        return CLASSPNP_OK;
    }

    /*
     * Register new bus.
     */
    uint32_t bus_idx = ctx->bus_count;
    classpnp_bus_t *bus = &ctx->buses[bus_idx];

    bus->bus_type = (uint8_t)bus_type;
    bus->device_count = 0;
    classpnp_memset((uint8_t *)bus->bus_id, 0, CLASSPNP_BUS_ID_LEN);
    classpnp_strncpy(bus->bus_id, bus_id, CLASSPNP_BUS_ID_LEN);

    /*
     * Simulate device detection. The number of devices depends
     * on the bus type. For PCI, enumerate up to 4 devices.
     * For USB, up to 4 endpoints. ISA/ACPI/Platform get 1-2.
     */
    uint32_t sim_count = 0;

    switch (bus_type) {
    case CLASSPNP_BUS_PCI:
        sim_count = 4u;
        break;
    case CLASSPNP_BUS_USB:
        sim_count = 4u;
        break;
    case CLASSPNP_BUS_ISA:
        sim_count = 2u;
        break;
    case CLASSPNP_BUS_ACPIC:
        sim_count = 1u;
        break;
    case CLASSPNP_BUS_PLATFORM:
        sim_count = 1u;
        break;
    default:
        sim_count = 1u;
        break;
    }

    for (uint32_t slot = 0; slot < sim_count; slot++) {
        if (ctx->device_count >= CLASSPNP_MAX_DEVICES)
            break;

        uint32_t dev_idx = ctx->device_count;
        classpnp_device_t *dev = &ctx->devices[dev_idx];

        /* Zero the device entry */
        classpnp_memset((uint8_t *)dev, 0, sizeof(classpnp_device_t));

        dev->bus_idx = (uint8_t)bus_idx;
        dev->parent_bus = (uint8_t)bus_idx;
        dev->state = (uint8_t)CLASSPNP_STATE_DETECTED;

        /* Generate bus-type-specific IDs */
        switch (bus_type) {
        case CLASSPNP_BUS_PCI:
            classpnp_simulate_pci_ids(bus_idx, slot,
                                      &dev->vendor_id,
                                      &dev->device_id,
                                      &dev->device_class);
            break;
        case CLASSPNP_BUS_USB:
            classpnp_simulate_usb_ids(bus_idx, slot,
                                      &dev->vendor_id,
                                      &dev->device_id,
                                      &dev->device_class);
            break;
        case CLASSPNP_BUS_ISA:
            classpnp_simulate_isa_ids(bus_idx, slot,
                                      &dev->vendor_id,
                                      &dev->device_id,
                                      &dev->device_class);
            break;
        case CLASSPNP_BUS_ACPIC:
            classpnp_simulate_acpi_ids(bus_idx, slot,
                                       &dev->vendor_id,
                                       &dev->device_id,
                                       &dev->device_class);
            break;
        case CLASSPNP_BUS_PLATFORM:
            classpnp_simulate_platform_ids(bus_idx, slot,
                                           &dev->vendor_id,
                                           &dev->device_id,
                                           &dev->device_class);
            break;
        default:
            dev->vendor_id = 0xFFFFu;
            dev->device_id = 0xFFFFu;
            dev->device_class = 0xFFFFFFu;
            break;
        }

        /* Assign a unique device name */
        classpnp_memset((uint8_t *)dev->name, 0, CLASSPNP_NAME_LEN);
        dev->name[0] = 'd';
        dev->name[1] = 'e';
        dev->name[2] = 'v';
        /* Encode bus_idx and slot as simple ASCII digits */
        dev->name[3] = (char)('0' + (bus_idx % 10));
        dev->name[4] = '_';
        dev->name[5] = (char)('0' + (slot % 10));
        dev->name[6] = '\0';

        /* Link bus -> device */
        bus->device_indices[bus->device_count] = (uint8_t)dev_idx;
        bus->device_count++;

        ctx->device_count++;
    }

    ctx->stats.total_enumerations++;

    return CLASSPNP_OK;
}

/* ------------------------------------------------------------------ */
/*  Resource Assignment                                               */
/* ------------------------------------------------------------------ */

classpnp_status_t classpnp_assign_resources(
    classpnp_context_t *ctx,
    uint32_t            device_idx
)
{
    if (!ctx)
        return CLASSPNP_ERR_INVALID_CTX;

    if (device_idx >= ctx->device_count)
        return CLASSPNP_ERR_INVALID_INDEX;

    classpnp_device_t *dev = &ctx->devices[device_idx];

    if (dev->state != (uint8_t)CLASSPNP_STATE_DETECTED)
        return CLASSPNP_ERR_INVALID_STATE;

    if (dev->resource_count >= CLASSPNP_MAX_RESOURCE_PER_DEV)
        return CLASSPNP_ERR_NO_RESOURCES;

    /*
     * Determine bus type for resource policy.
     * PCI devices receive MMIO ranges.
     * ISA devices receive IRQs.
     * USB/ACPI/Platform receive MMIO + IRQ.
     */
    uint8_t bus_type = ctx->buses[dev->bus_idx].bus_type;

    bool assigned = false;

    /* Assign MMIO for PCI, USB, ACPI, Platform */
    if (bus_type == CLASSPNP_BUS_PCI ||
        bus_type == CLASSPNP_BUS_USB ||
        bus_type == CLASSPNP_BUS_ACPIC ||
        bus_type == CLASSPNP_BUS_PLATFORM) {

        if (ctx->resource_count < CLASSPNP_MAX_RESOURCES) {
            uint32_t res_idx = ctx->resource_count;
            classpnp_resource_t *res = &ctx->resources[res_idx];

            classpnp_memset((uint8_t *)res, 0, sizeof(classpnp_resource_t));

            res->type = (uint8_t)CLASSPNP_RESOURCE_MMIO;
            res->flags = CLASSPNP_RES_FLAG_PREFETCHABLE;
            res->base_lo = CLASSPNP_MMIO_BASE +
                           (device_idx * CLASSPNP_MMIO_STEP);
            res->base_hi = 0u;
            res->length = CLASSPNP_MMIO_LENGTH;

            /* Cross-ref OWFS: mark MMIO as OWFS-backed if in block range */
            if ((res->base_lo & ~(CLASSPNP_OWFS_BLOCK_SIZE - 1u)) ==
                (CLASSPNP_MMIO_BASE & ~(CLASSPNP_OWFS_BLOCK_SIZE - 1u))) {
                res->flags |= CLASSPNP_RES_FLAG_OWFS_MAPPED;
            }

            dev->mmio_base = res->base_lo;
            dev->mmio_length = res->length;
            dev->resource_indices[dev->resource_count] = (uint8_t)res_idx;
            dev->resource_count++;
            ctx->resource_count++;
            assigned = true;
        }
    }

    /* Assign IRQ for ISA; also assign IRQ for USB/ACPI/Platform */
    if (bus_type == CLASSPNP_BUS_ISA ||
        bus_type == CLASSPNP_BUS_USB ||
        bus_type == CLASSPNP_BUS_ACPIC ||
        bus_type == CLASSPNP_BUS_PLATFORM) {

        /* Find a free IRQ from the pool */
        for (uint32_t i = 0; i < CLASSPNP_IRQ_POOL_SIZE; i++) {
            if (!classpnp_irq_used[i]) {
                if (ctx->resource_count >= CLASSPNP_MAX_RESOURCES)
                    break;

                classpnp_irq_used[i] = true;

                uint32_t res_idx = ctx->resource_count;
                classpnp_resource_t *res = &ctx->resources[res_idx];

                classpnp_memset((uint8_t *)res, 0,
                                sizeof(classpnp_resource_t));

                res->type = (uint8_t)CLASSPNP_RESOURCE_IRQ;
                res->flags = CLASSPNP_RES_FLAG_SHARABLE;
                res->irq_number = classpnp_irq_pool[i];
                res->base_lo = 0u;
                res->base_hi = 0u;
                res->length = 0u;

                dev->irq = res->irq_number;
                dev->resource_indices[dev->resource_count] =
                    (uint8_t)res_idx;
                dev->resource_count++;
                ctx->resource_count++;
                assigned = true;
                break;
            }
        }
    }

    /* Assign DMA for ISA bus devices */
    if (bus_type == CLASSPNP_BUS_ISA) {
        for (uint32_t i = 0; i < CLASSPNP_DMA_POOL_SIZE; i++) {
            if (!classpnp_dma_used[i]) {
                if (ctx->resource_count >= CLASSPNP_MAX_RESOURCES)
                    break;

                classpnp_dma_used[i] = true;

                uint32_t res_idx = ctx->resource_count;
                classpnp_resource_t *res = &ctx->resources[res_idx];

                classpnp_memset((uint8_t *)res, 0,
                                sizeof(classpnp_resource_t));

                res->type = (uint8_t)CLASSPNP_RESOURCE_DMA_CHANNEL;
                res->flags = 0u;
                res->base_lo = classpnp_dma_pool[i];
                res->length = 0u;

                dev->dma_channel = classpnp_dma_pool[i];
                dev->resource_indices[dev->resource_count] =
                    (uint8_t)res_idx;
                dev->resource_count++;
                ctx->resource_count++;
                assigned = true;
                break;
            }
        }
    }

    if (!assigned)
        return CLASSPNP_ERR_NO_RESOURCES;

    dev->state = (uint8_t)CLASSPNP_STATE_RESOURCES_ASSIGNED;
    ctx->stats.total_resource_assignments++;

    return CLASSPNP_OK;
}

/* ------------------------------------------------------------------ */
/*  Driver Binding                                                    */
/* ------------------------------------------------------------------ */

classpnp_status_t classpnp_bind_driver(
    classpnp_context_t *ctx,
    uint32_t            device_idx,
    const char         *driver_name)
{
    if (!ctx)
        return CLASSPNP_ERR_INVALID_CTX;

    if (device_idx >= ctx->device_count)
        return CLASSPNP_ERR_INVALID_INDEX;

    if (!driver_name)
        return CLASSPNP_ERR_INVALID_CTX;

    classpnp_device_t *dev = &ctx->devices[device_idx];

    if (dev->state != (uint8_t)CLASSPNP_STATE_RESOURCES_ASSIGNED)
        return CLASSPNP_ERR_INVALID_STATE;

    uint32_t name_len = classpnp_strlen(driver_name);
    if (name_len > CLASSPNP_DRV_NAME_MAX)
        return CLASSPNP_ERR_NAME_TOO_LONG;

    classpnp_memset((uint8_t *)dev->driver_name, 0, CLASSPNP_NAME_LEN);
    classpnp_strncpy(dev->driver_name, driver_name, CLASSPNP_DRV_NAME_MAX);

    dev->state = (uint8_t)CLASSPNP_STATE_DRIVER_BOUND;
    ctx->stats.total_driver_binds++;

    return CLASSPNP_OK;
}

classpnp_status_t classpnp_unbind_driver(
    classpnp_context_t *ctx,
    uint32_t            device_idx)
{
    if (!ctx)
        return CLASSPNP_ERR_INVALID_CTX;

    if (device_idx >= ctx->device_count)
        return CLASSPNP_ERR_INVALID_INDEX;

    classpnp_device_t *dev = &ctx->devices[device_idx];

    if (dev->state != (uint8_t)CLASSPNP_STATE_DRIVER_BOUND)
        return CLASSPNP_ERR_DRIVER_NOT_BOUND;

    classpnp_memset((uint8_t *)dev->driver_name, 0, CLASSPNP_NAME_LEN);
    dev->state = (uint8_t)CLASSPNP_STATE_RESOURCES_ASSIGNED;
    ctx->stats.total_driver_unbinds++;

    return CLASSPNP_OK;
}

/* ------------------------------------------------------------------ */
/*  Device Enable/Disable                                             */
/* ------------------------------------------------------------------ */

classpnp_status_t classpnp_disable_device(
    classpnp_context_t *ctx,
    uint32_t            device_idx)
{
    if (!ctx)
        return CLASSPNP_ERR_INVALID_CTX;

    if (device_idx >= ctx->device_count)
        return CLASSPNP_ERR_INVALID_INDEX;

    classpnp_device_t *dev = &ctx->devices[device_idx];

    if (dev->state == (uint8_t)CLASSPNP_STATE_DISABLED)
        return CLASSPNP_ERR_INVALID_STATE;

    if (dev->state == (uint8_t)CLASSPNP_STATE_UNKNOWN)
        return CLASSPNP_ERR_INVALID_STATE;

    /* Encode previous state in flags for re-enable restoration */
    dev->flags &= ~CLASSPNP_FLAG_PREV_STATE_MASK;
    dev->flags |= (dev->state & CLASSPNP_FLAG_PREV_STATE_MASK);
    dev->flags |= CLASSPNP_FLAG_DISABLED;

    dev->state = (uint8_t)CLASSPNP_STATE_DISABLED;
    ctx->stats.total_devices_disabled++;

    return CLASSPNP_OK;
}

classpnp_status_t classpnp_reenable_device(
    classpnp_context_t *ctx,
    uint32_t            device_idx)
{
    if (!ctx)
        return CLASSPNP_ERR_INVALID_CTX;

    if (device_idx >= ctx->device_count)
        return CLASSPNP_ERR_INVALID_INDEX;

    classpnp_device_t *dev = &ctx->devices[device_idx];

    if (dev->state != (uint8_t)CLASSPNP_STATE_DISABLED)
        return CLASSPNP_ERR_INVALID_STATE;

    if (!(dev->flags & CLASSPNP_FLAG_DISABLED))
        return CLASSPNP_ERR_INVALID_STATE;

    /* Restore previous state from encoded flags */
    uint8_t prev = (uint8_t)(dev->flags & CLASSPNP_FLAG_PREV_STATE_MASK);

    dev->flags &= ~CLASSPNP_FLAG_DISABLED;
    dev->flags &= ~CLASSPNP_FLAG_PREV_STATE_MASK;

    dev->state = prev;
    ctx->stats.total_devices_reenabled++;

    return CLASSPNP_OK;
}

/* ------------------------------------------------------------------ */
/*  Query Operations                                                  */
/* ------------------------------------------------------------------ */

const classpnp_device_t *classpnp_get_device(
    const classpnp_context_t *ctx,
    uint32_t                  device_idx)
{
    if (!ctx)
        return (const classpnp_device_t *)0;

    if (device_idx >= ctx->device_count)
        return (const classpnp_device_t *)0;

    return &ctx->devices[device_idx];
}

const classpnp_bus_t *classpnp_get_bus(
    const classpnp_context_t *ctx,
    uint32_t                  bus_idx)
{
    if (!ctx)
        return (const classpnp_bus_t *)0;

    if (bus_idx >= ctx->bus_count)
        return (const classpnp_bus_t *)0;

    return &ctx->buses[bus_idx];
}

int32_t classpnp_find_device(
    const classpnp_context_t *ctx,
    uint16_t                  vendor_id,
    uint16_t                  device_id)
{
    if (!ctx)
        return -1;

    for (uint32_t i = 0; i < ctx->device_count; i++) {
        const classpnp_device_t *dev = &ctx->devices[i];

        if (dev->vendor_id == vendor_id &&
            dev->device_id == device_id) {
            return (int32_t)i;
        }
    }

    return -1;
}
