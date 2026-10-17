/*
 * owpci64.c - OpenWindows PCI Configuration Library (.owd)
 *
 * Configuration mechanism #1: a 32-bit address is written to port 0xCF8
 * and the selected dword of configuration space is then accessed through
 * port 0xCFC. Sub-dword accesses are performed with narrow shifts or a
 * read-modify-write of the containing dword.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owpci64.h"

/* ── Port I/O primitives ──────────────────────────────────────── */

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

/* ── Address encoding ─────────────────────────────────────────── */

/* Build the mechanism #1 CONFIG_ADDRESS value. Bit 31 enables the
 * cycle; bits 23:16 select the bus, 15:11 the device, 10:8 the
 * function and 7:2 the dword-aligned register offset. */
static uint32_t owpci_config_address(uint8_t bus, uint8_t device,
                                     uint8_t function, uint8_t offset)
{
    uint32_t addr = 0x80000000u;
    addr |= ((uint32_t)bus << 16);
    addr |= ((uint32_t)(device & 0x1Fu) << 11);
    addr |= ((uint32_t)(function & 0x07u) << 8);
    addr |= ((uint32_t)(offset & 0xFCu));
    return addr;
}

static bool owpci_slot_valid(uint8_t device, uint8_t function)
{
    return (device < (uint8_t)OWPCI_MAX_DEVICE) &&
           (function < (uint8_t)OWPCI_MAX_FUNCTION);
}

/* ── Configuration cycles ─────────────────────────────────────── */

uint32_t owpci_read32(uint8_t bus, uint8_t device, uint8_t function,
                      uint8_t offset)
{
    if (!owpci_slot_valid(device, function)) {
        return 0xFFFFFFFFu;
    }
    ow_outl(OWPCI_CONFIG_ADDRESS,
            owpci_config_address(bus, device, function, offset));
    return ow_inl(OWPCI_CONFIG_DATA);
}

uint16_t owpci_read16(uint8_t bus, uint8_t device, uint8_t function,
                      uint8_t offset)
{
    uint32_t dword = owpci_read32(bus, device, function,
                                  (uint8_t)(offset & 0xFCu));
    uint32_t shift = (uint32_t)(offset & 0x02u) * 8u;
    return (uint16_t)(dword >> shift);
}

uint8_t owpci_read8(uint8_t bus, uint8_t device, uint8_t function,
                    uint8_t offset)
{
    uint32_t dword = owpci_read32(bus, device, function,
                                  (uint8_t)(offset & 0xFCu));
    uint32_t shift = (uint32_t)(offset & 0x03u) * 8u;
    return (uint8_t)(dword >> shift);
}

void owpci_write32(uint8_t bus, uint8_t device, uint8_t function,
                   uint8_t offset, uint32_t value)
{
    if (!owpci_slot_valid(device, function)) {
        return;
    }
    ow_outl(OWPCI_CONFIG_ADDRESS,
            owpci_config_address(bus, device, function, offset));
    ow_outl(OWPCI_CONFIG_DATA, value);
}

void owpci_write16(uint8_t bus, uint8_t device, uint8_t function,
                   uint8_t offset, uint16_t value)
{
    uint8_t  reg   = (uint8_t)(offset & 0xFCu);
    uint32_t shift = (uint32_t)(offset & 0x02u) * 8u;
    uint32_t mask  = 0x0000FFFFu << shift;
    uint32_t dword = owpci_read32(bus, device, function, reg);

    dword = (dword & ~mask) | (((uint32_t)value << shift) & mask);
    owpci_write32(bus, device, function, reg, dword);
}

void owpci_write8(uint8_t bus, uint8_t device, uint8_t function,
                  uint8_t offset, uint8_t value)
{
    uint8_t  reg   = (uint8_t)(offset & 0xFCu);
    uint32_t shift = (uint32_t)(offset & 0x03u) * 8u;
    uint32_t mask  = 0x000000FFu << shift;
    uint32_t dword = owpci_read32(bus, device, function, reg);

    dword = (dword & ~mask) | (((uint32_t)value << shift) & mask);
    owpci_write32(bus, device, function, reg, dword);
}

/* ── Discovery ────────────────────────────────────────────────── */

/* Populate a descriptor for the device at (bus, device, function). */
static void owpci_fill(owpci_device_t *out, uint8_t bus, uint8_t device,
                       uint8_t function)
{
    out->bus         = bus;
    out->device      = device;
    out->function    = function;
    out->vendor_id   = owpci_read16(bus, device, function, OWPCI_REG_VENDOR_ID);
    out->device_id   = owpci_read16(bus, device, function, OWPCI_REG_DEVICE_ID);
    out->revision    = owpci_read8 (bus, device, function, OWPCI_REG_REVISION);
    out->prog_if     = owpci_read8 (bus, device, function, OWPCI_REG_PROG_IF);
    out->subclass    = owpci_read8 (bus, device, function, OWPCI_REG_SUBCLASS);
    out->class_code  = owpci_read8 (bus, device, function, OWPCI_REG_CLASS);
    out->header_type = owpci_read8 (bus, device, function,
                                    OWPCI_REG_HEADER_TYPE);
}

bool owpci_find_device(uint16_t vendor_id, uint16_t device_id,
                       owpci_device_t *out)
{
    for (uint32_t bus = 0; bus < OWPCI_MAX_BUS; ++bus) {
        for (uint32_t dev = 0; dev < OWPCI_MAX_DEVICE; ++dev) {
            uint16_t vendor = owpci_read16((uint8_t)bus, (uint8_t)dev, 0u,
                                           OWPCI_REG_VENDOR_ID);
            if (vendor == OWPCI_VENDOR_NONE) {
                continue;
            }

            /* A single-function device only occupies function 0. */
            uint8_t  header = owpci_read8((uint8_t)bus, (uint8_t)dev, 0u,
                                          OWPCI_REG_HEADER_TYPE);
            uint32_t funcs  = (header & OWPCI_HEADER_MULTIFUNC) ? 8u : 1u;

            for (uint32_t func = 0; func < funcs; ++func) {
                uint16_t v = owpci_read16((uint8_t)bus, (uint8_t)dev,
                                          (uint8_t)func, OWPCI_REG_VENDOR_ID);
                if (v == OWPCI_VENDOR_NONE) {
                    continue;
                }
                uint16_t d = owpci_read16((uint8_t)bus, (uint8_t)dev,
                                          (uint8_t)func, OWPCI_REG_DEVICE_ID);
                if (v == vendor_id && d == device_id) {
                    if (out != NULL) {
                        owpci_fill(out, (uint8_t)bus, (uint8_t)dev,
                                   (uint8_t)func);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

uint32_t owpci_enumerate(owpci_device_t *out, uint32_t capacity)
{
    uint32_t found = 0;

    for (uint32_t bus = 0; bus < OWPCI_MAX_BUS; ++bus) {
        for (uint32_t dev = 0; dev < OWPCI_MAX_DEVICE; ++dev) {
            uint16_t vendor = owpci_read16((uint8_t)bus, (uint8_t)dev, 0u,
                                           OWPCI_REG_VENDOR_ID);
            if (vendor == OWPCI_VENDOR_NONE) {
                continue;
            }

            uint8_t  header = owpci_read8((uint8_t)bus, (uint8_t)dev, 0u,
                                          OWPCI_REG_HEADER_TYPE);
            uint32_t funcs  = (header & OWPCI_HEADER_MULTIFUNC) ? 8u : 1u;

            for (uint32_t func = 0; func < funcs; ++func) {
                uint16_t v = owpci_read16((uint8_t)bus, (uint8_t)dev,
                                          (uint8_t)func, OWPCI_REG_VENDOR_ID);
                if (v == OWPCI_VENDOR_NONE) {
                    continue;
                }
                if (out != NULL && found < capacity) {
                    owpci_fill(&out[found], (uint8_t)bus, (uint8_t)dev,
                               (uint8_t)func);
                }
                ++found;
            }
        }
    }
    return found;
}
