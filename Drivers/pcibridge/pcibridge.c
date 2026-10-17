/*
 * pcibridge.c - OpenWindows PCI Bus Bridge Enumerator (.owc)
 *
 * Implements the PCI configuration mechanism #1 register protocol and a
 * recursive bus walk that follows PCI-to-PCI bridge secondary buses.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "pcibridge.h"

/* ── Port I/O helpers ─────────────────────────────────────────── */

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

/* ── Internal state ───────────────────────────────────────────── */

static bool pcibridge_initialized;
static uint8_t pcib_bus_seen[PCIB_MAX_BUSES / 8u];  /* visited bus bitmap */

/* ── Helpers ──────────────────────────────────────────────────── */

static void pcib_mark_bus(uint8_t bus)
{
    pcib_bus_seen[bus >> 3] |= (uint8_t)(1u << (bus & 7u));
}

static bool pcib_bus_visited(uint8_t bus)
{
    return (pcib_bus_seen[bus >> 3] & (uint8_t)(1u << (bus & 7u))) != 0u;
}

static void pcib_clear_seen(void)
{
    for (size_t i = 0; i < sizeof(pcib_bus_seen); ++i) pcib_bus_seen[i] = 0u;
}

/* Build a mechanism #1 CONFIG_ADDRESS dword. */
static uint32_t pcib_config_address(uint8_t bus, uint8_t slot, uint8_t func,
                                    uint8_t offset)
{
    return ((uint32_t)bus << 16) |
           ((uint32_t)(slot & 0x1Fu) << 11) |
           ((uint32_t)(func & 0x07u) << 8) |
           ((uint32_t)offset & 0xFCu) |
           0x80000000u;                     /* enable bit */
}

/* ── Configuration space access ───────────────────────────────── */

uint32_t pcib_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    ow_outl(PCIB_CONFIG_ADDRESS, pcib_config_address(bus, slot, func, offset));
    return ow_inl(PCIB_CONFIG_DATA);
}

void pcib_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset,
                       uint32_t value)
{
    ow_outl(PCIB_CONFIG_ADDRESS, pcib_config_address(bus, slot, func, offset));
    ow_outl(PCIB_CONFIG_DATA, value);
}

uint8_t pcib_read_config8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t dword = pcib_read_config(bus, slot, func, (uint8_t)(offset & 0xFCu));
    return (uint8_t)(dword >> ((offset & 0x03u) * 8u));
}

uint16_t pcib_read_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t dword = pcib_read_config(bus, slot, func, (uint8_t)(offset & 0xFCu));
    return (uint16_t)(dword >> ((offset & 0x02u) * 8u));
}

bool pcib_is_bridge(uint8_t bus, uint8_t slot, uint8_t func)
{
    uint8_t class_code = pcib_read_config8(bus, slot, func, PCIB_CFG_CLASS);
    uint8_t subclass   = pcib_read_config8(bus, slot, func, PCIB_CFG_SUBCLASS);
    return class_code == PCIB_CLASS_BRIDGE &&
           subclass == PCIB_SUBCLASS_PCI_BRIDGE;
}

/* ── Enumeration ──────────────────────────────────────────────── */

typedef struct {
    pcib_device_t *devices;
    uint32_t       capacity;
    uint32_t       count;
} pcib_scan_ctx_t;

static void pcib_fill_device(pcib_device_t *dev, uint8_t bus, uint8_t slot,
                             uint8_t func, uint16_t vendor, uint16_t device,
                             uint8_t header_type, uint8_t class_code,
                             uint8_t subclass)
{
    dev->bus         = bus;
    dev->slot        = slot;
    dev->func        = func;
    dev->revision    = pcib_read_config8(bus, slot, func, PCIB_CFG_REVISION);
    dev->vendor_id   = vendor;
    dev->device_id   = device;
    dev->class_code  = class_code;
    dev->subclass    = subclass;
    dev->prog_if     = pcib_read_config8(bus, slot, func, PCIB_CFG_PROG_IF);
    dev->header_type = header_type;
    dev->is_bridge   = (class_code == PCIB_CLASS_BRIDGE &&
                        subclass == PCIB_SUBCLASS_PCI_BRIDGE);
    dev->secondary_bus = dev->is_bridge
        ? pcib_read_config8(bus, slot, func, PCIB_CFG_SECONDARY_BUS)
        : 0u;
}

/* Scan one bus and recurse into any secondary buses behind bridges. */
static void pcib_scan_bus(pcib_scan_ctx_t *ctx, uint8_t bus)
{
    if (pcib_bus_visited(bus)) return;
    pcib_mark_bus(bus);

    for (uint8_t slot = 0u; slot < 32u; ++slot) {
        /* Function 0 vendor id tells us whether the slot is populated. */
        uint16_t vendor0 = pcib_read_config16(bus, slot, 0u, PCIB_CFG_VENDOR_ID);
        if (vendor0 == 0xFFFFu) continue;

        uint8_t header0 = pcib_read_config8(bus, slot, 0u, PCIB_CFG_HEADER_TYPE);
        uint8_t functions = (header0 & 0x80u) ? 8u : 1u;

        for (uint8_t func = 0u; func < functions; ++func) {
            uint16_t vendor = pcib_read_config16(bus, slot, func,
                                                 PCIB_CFG_VENDOR_ID);
            if (vendor == 0xFFFFu) continue;

            uint16_t device = pcib_read_config16(bus, slot, func,
                                                 PCIB_CFG_DEVICE_ID);
            uint8_t header = pcib_read_config8(bus, slot, func,
                                               PCIB_CFG_HEADER_TYPE);
            uint8_t class_code = pcib_read_config8(bus, slot, func,
                                                   PCIB_CFG_CLASS);
            uint8_t subclass = pcib_read_config8(bus, slot, func,
                                                 PCIB_CFG_SUBCLASS);

            if (ctx->devices && ctx->count < ctx->capacity) {
                pcib_fill_device(&ctx->devices[ctx->count], bus, slot, func,
                                 vendor, device, header, class_code, subclass);
            }
            ctx->count++;

            /* Recurse through PCI-to-PCI bridges into their secondary bus. */
            if (class_code == PCIB_CLASS_BRIDGE &&
                subclass == PCIB_SUBCLASS_PCI_BRIDGE) {
                uint8_t secondary = pcib_read_config8(bus, slot, func,
                                                      PCIB_CFG_SECONDARY_BUS);
                if (secondary != bus) pcib_scan_bus(ctx, secondary);
            }
        }
    }
}

/* ── Public API ───────────────────────────────────────────────── */

bool pcib_init(void)
{
    pcib_clear_seen();
    pcibridge_initialized = true;
    return true;
}

uint32_t pcib_enumerate(pcib_device_t *devices, uint32_t capacity)
{
    pcib_scan_ctx_t ctx;
    ctx.devices  = devices;
    ctx.capacity = capacity;
    ctx.count    = 0u;

    pcib_clear_seen();
    pcib_scan_bus(&ctx, 0u);
    return ctx.count;
}

bool pcib_is_initialized(void)
{
    return pcibridge_initialized;
}
