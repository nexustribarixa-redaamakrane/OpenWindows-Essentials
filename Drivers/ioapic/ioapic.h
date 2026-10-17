/*
 * ioapic.h - OpenWindows I/O APIC Driver (.owc)
 *
 * Programs the I/O APIC redirection table through the indirect
 * register window: IOREGSEL at MMIO offset 0x00 selects a 32-bit
 * register exposed through IOWIN at MMIO offset 0x10. Each redirection
 * entry is a 64-bit quantity spanning the registers at 0x10 + 2*irq.
 *
 * C99 freestanding. Zero dynamic heap allocation; the caller provides
 * the MMIO base address.
 */

#ifndef IOAPIC_H
#define IOAPIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define IOAPIC_MMIO_REGSEL      0x00u   /* indirect register selector */
#define IOAPIC_MMIO_WIN         0x10u   /* indirect register window   */

#define IOAPIC_REG_ID           0x00u   /* APIC ID                    */
#define IOAPIC_REG_VERSION      0x01u   /* version / entry count      */
#define IOAPIC_REG_ARBITRATION  0x02u   /* arbitration priority       */
#define IOAPIC_REG_REDTBL       0x10u   /* redirection table base     */

#define IOAPIC_DEFAULT_BASE     0xFEC00000u
#define IOAPIC_MAX_ENTRIES      240u    /* architectural upper bound  */

/* Low-dword delivery mode (bits 8-10). */
typedef enum {
    IOAPIC_DELIVERY_FIXED  = 0u,
    IOAPIC_DELIVERY_LOWEST = 1u,
    IOAPIC_DELIVERY_SMI    = 2u,
    IOAPIC_DELIVERY_NMI    = 4u,
    IOAPIC_DELIVERY_INIT   = 5u,
    IOAPIC_DELIVERY_EXTINT = 7u
} ioapic_delivery_t;

/* Low-dword destination mode (bit 11). */
typedef enum {
    IOAPIC_DEST_PHYSICAL = 0u,
    IOAPIC_DEST_LOGICAL  = 1u
} ioapic_dest_mode_t;

/* Low-dword input polarity (bit 13). */
typedef enum {
    IOAPIC_POLARITY_HIGH = 0u,  /* active high */
    IOAPIC_POLARITY_LOW  = 1u   /* active low  */
} ioapic_polarity_t;

/* Low-dword trigger mode (bit 15). */
typedef enum {
    IOAPIC_TRIGGER_EDGE  = 0u,
    IOAPIC_TRIGGER_LEVEL = 1u
} ioapic_trigger_t;

/* Complete description of one redirection table entry. */
typedef struct {
    uint8_t  vector;        /* interrupt vector (0..255)              */
    uint8_t  delivery;      /* ioapic_delivery_t                      */
    uint8_t  dest_mode;     /* ioapic_dest_mode_t                     */
    uint8_t  polarity;      /* ioapic_polarity_t                      */
    uint8_t  trigger;       /* ioapic_trigger_t                       */
    uint8_t  dest_apic_id;  /* destination local APIC id              */
    bool     masked;        /* true: interrupt disabled               */
} ioapic_irq_config_t;

/* Initialise the I/O APIC at 'mmio_base' with a GSI base offset. */
bool ioapic_init(volatile void *mmio_base, uint32_t gsi_base);

/* Program the redirection entry for absolute GSI 'irq'. */
bool ioapic_set_irq(uint32_t irq, const ioapic_irq_config_t *config);

/* Mask or unmask one GSI. */
void ioapic_mask(uint32_t irq);
void ioapic_unmask(uint32_t irq);

/* Signal end-of-interrupt (via the local APIC EOI register). */
void ioapic_eoi(void);

/* Read-back helpers. */
uint32_t ioapic_gsi_base(void);
uint32_t ioapic_irq_count(void);
uint8_t  ioapic_id(void);

#endif /* IOAPIC_H */
