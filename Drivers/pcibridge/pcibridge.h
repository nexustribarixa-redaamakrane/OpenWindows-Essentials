/*
 * pcibridge.h - OpenWindows PCI Bus Bridge Enumerator (.owc)
 *
 * Configuration space access uses the x86 PCI mechanism #1
 * (0xCF8 address / 0xCFC data). Enumeration starts on bus 0, detects
 * PCI-to-PCI bridges and recurses into every reachable secondary bus.
 *
 * C99 freestanding. Zero dynamic heap allocation; the caller provides
 * the device table filled by pcib_enumerate().
 */

#ifndef PCIBRIDGE_H
#define PCIBRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PCIB_CONFIG_ADDRESS     0x0CF8u  /* configuration address port */
#define PCIB_CONFIG_DATA        0x0CFCu  /* configuration data port    */
#define PCIB_MAX_BUSES          256u     /* bus numbers are 8-bit      */

/* PCI class/subclass codes used to recognise a PCI-to-PCI bridge. */
#define PCIB_CLASS_BRIDGE        0x06u
#define PCIB_SUBCLASS_PCI_BRIDGE 0x04u

/* Common configuration space register offsets. */
#define PCIB_CFG_VENDOR_ID      0x00u
#define PCIB_CFG_DEVICE_ID      0x02u
#define PCIB_CFG_REVISION       0x08u
#define PCIB_CFG_PROG_IF        0x09u
#define PCIB_CFG_SUBCLASS       0x0Au
#define PCIB_CFG_CLASS          0x0Bu
#define PCIB_CFG_HEADER_TYPE    0x0Eu
#define PCIB_CFG_SECONDARY_BUS  0x19u

/* One discovered PCI function. */
typedef struct {
    uint8_t  bus;            /* bus number                        */
    uint8_t  slot;           /* device number (0..31)             */
    uint8_t  func;           /* function number (0..7)            */
    uint8_t  revision;       /* revision id                       */
    uint16_t vendor_id;      /* 0xFFFF when no device present     */
    uint16_t device_id;      /* device id                         */
    uint8_t  class_code;     /* base class                        */
    uint8_t  subclass;       /* sub-class                         */
    uint8_t  prog_if;        /* programming interface             */
    uint8_t  header_type;    /* header type (bit 7 = multifunction) */
    uint8_t  secondary_bus;  /* bridge secondary bus (bridges only) */
    bool     is_bridge;      /* PCI-to-PCI bridge                 */
} pcib_device_t;

/* Reset enumerator bookkeeping. */
bool pcib_init(void);

/* Scan bus 0 and every secondary bus; returns the number of functions.
   Fills up to 'capacity' entries in 'devices' (may be NULL). */
uint32_t pcib_enumerate(pcib_device_t *devices, uint32_t capacity);

/* Mechanism #1 configuration space access (dword aligned). */
uint32_t pcib_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pcib_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset,
                           uint32_t value);

/* Width-specific reads. */
uint8_t  pcib_read_config8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pcib_read_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

/* True when the function is a PCI-to-PCI bridge. */
bool pcib_is_bridge(uint8_t bus, uint8_t slot, uint8_t func);

/* True after pcib_init() and before any explicit reset. */
bool pcib_is_initialized(void);

#endif /* PCIBRIDGE_H */
