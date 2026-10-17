/*
 * owpci64.h - OpenWindows PCI Configuration Library (.owd)
 *
 * PCI configuration space access using configuration mechanism #1
 * (I/O ports 0xCF8 address / 0xCFC data). Provides byte, word and
 * dword config cycles plus device discovery by vendor/device id.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWPCI64_H
#define OWPCI64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Configuration mechanism #1 ports ─────────────────────────── */

#define OWPCI_CONFIG_ADDRESS    0xCF8u
#define OWPCI_CONFIG_DATA       0xCFCu

/* ── Topology limits ──────────────────────────────────────────── */

#define OWPCI_MAX_BUS           256u
#define OWPCI_MAX_DEVICE        32u
#define OWPCI_MAX_FUNCTION      8u

/* Sentinel returned by the host bridge when no device is present. */
#define OWPCI_VENDOR_NONE       0xFFFFu

/* ── Standard configuration space offsets ─────────────────────── */

#define OWPCI_REG_VENDOR_ID     0x00u   /* 16-bit vendor identifier       */
#define OWPCI_REG_DEVICE_ID     0x02u   /* 16-bit device identifier       */
#define OWPCI_REG_COMMAND       0x04u   /* 16-bit command register        */
#define OWPCI_REG_STATUS        0x06u   /* 16-bit status register         */
#define OWPCI_REG_REVISION      0x08u   /* 8-bit  revision id             */
#define OWPCI_REG_PROG_IF       0x09u   /* 8-bit  programming interface   */
#define OWPCI_REG_SUBCLASS      0x0Au   /* 8-bit  sub-class code          */
#define OWPCI_REG_CLASS         0x0Bu   /* 8-bit  base class code         */
#define OWPCI_REG_HEADER_TYPE   0x0Eu   /* 8-bit  header type             */
#define OWPCI_REG_BAR0          0x10u   /* 32-bit base address register 0 */

/* Header type bit 7 signals a multi-function device. */
#define OWPCI_HEADER_MULTIFUNC  0x80u

/* ── Discovered device descriptor ─────────────────────────────── */

typedef struct {
    uint8_t  bus;           /* PCI bus number                        */
    uint8_t  device;        /* device (slot) number                  */
    uint8_t  function;      /* function number                       */
    uint8_t  revision;      /* revision identifier                   */
    uint8_t  prog_if;       /* programming interface                 */
    uint8_t  subclass;      /* sub-class code                        */
    uint8_t  class_code;    /* base class code                       */
    uint8_t  header_type;   /* header type register                  */
    uint16_t vendor_id;     /* vendor identifier                     */
    uint16_t device_id;     /* device identifier                     */
} owpci_device_t;

/* ── Configuration cycles ─────────────────────────────────────── */

/* Read one byte from configuration space at (bus, device, function, offset). */
uint8_t  owpci_read8 (uint8_t bus, uint8_t device, uint8_t function,
                      uint8_t offset);

/* Read one 16-bit word from configuration space. */
uint16_t owpci_read16(uint8_t bus, uint8_t device, uint8_t function,
                      uint8_t offset);

/* Read one 32-bit dword from configuration space. */
uint32_t owpci_read32(uint8_t bus, uint8_t device, uint8_t function,
                      uint8_t offset);

/* Write one byte into configuration space (read-modify-write). */
void owpci_write8 (uint8_t bus, uint8_t device, uint8_t function,
                   uint8_t offset, uint8_t value);

/* Write one 16-bit word into configuration space (read-modify-write). */
void owpci_write16(uint8_t bus, uint8_t device, uint8_t function,
                   uint8_t offset, uint16_t value);

/* Write one 32-bit dword into configuration space. */
void owpci_write32(uint8_t bus, uint8_t device, uint8_t function,
                   uint8_t offset, uint32_t value);

/* ── Discovery ────────────────────────────────────────────────── */

/* Scan the bus for the first device matching vendor_id/device_id.
 * On success fills *out (when non-NULL) and returns true. */
bool owpci_find_device(uint16_t vendor_id, uint16_t device_id,
                       owpci_device_t *out);

/* Walk the PCI topology and copy up to capacity descriptors into out.
 * Returns the number of devices actually written. */
uint32_t owpci_enumerate(owpci_device_t *out, uint32_t capacity);

#endif /* OWPCI64_H */
