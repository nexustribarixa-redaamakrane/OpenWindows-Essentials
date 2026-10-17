/*
 * lapic.h - OpenWindows Local APIC Driver (.owc)
 *
 * Drives the per-CPU local APIC through its memory-mapped register
 * bank: interrupt commands (ICR), end-of-interrupt, the spurious
 * interrupt vector register and the LVT timer.
 *
 * C99 freestanding. Zero dynamic heap allocation; the caller provides
 * the MMIO base address (0xFEE00000 by default).
 */

#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define LAPIC_DEFAULT_BASE      0xFEE00000u

/* Register offsets within the local APIC MMIO page. */
#define LAPIC_REG_ID            0x020u  /* local APIC id              */
#define LAPIC_REG_VERSION       0x030u  /* version register           */
#define LAPIC_REG_TPR           0x080u  /* task priority              */
#define LAPIC_REG_EOI           0x0B0u  /* end of interrupt           */
#define LAPIC_REG_SVR           0x0F0u  /* spurious vector            */
#define LAPIC_REG_ICR_LOW       0x300u  /* interrupt command, low     */
#define LAPIC_REG_ICR_HIGH      0x310u  /* interrupt command, high    */
#define LAPIC_REG_LVT_TIMER     0x320u  /* LVT timer entry            */
#define LAPIC_REG_TIMER_INIT    0x380u  /* timer initial count        */
#define LAPIC_REG_TIMER_CURRENT 0x390u  /* timer current count        */
#define LAPIC_REG_TIMER_DIV     0x3E0u  /* timer divide configuration */

/* Spurious vector register. */
#define LAPIC_SVR_ENABLE        0x00000100u

/* ICR low-dword fields. */
#define LAPIC_ICR_DELIVERY_FIXED   0x00000000u
#define LAPIC_ICR_DELIVERY_LOWEST  0x00000100u
#define LAPIC_ICR_DELIVERY_SMI     0x00000200u
#define LAPIC_ICR_DELIVERY_NMI     0x00000400u
#define LAPIC_ICR_DELIVERY_INIT    0x00000500u
#define LAPIC_ICR_DELIVERY_STARTUP 0x00000600u
#define LAPIC_ICR_DEST_PHYSICAL    0x00000000u
#define LAPIC_ICR_DEST_LOGICAL     0x00000800u
#define LAPIC_ICR_DELIVERY_PENDING 0x00001000u
#define LAPIC_ICR_LEVEL_ASSERT     0x00004000u
#define LAPIC_ICR_TRIGGER_LEVEL    0x00008000u

/* Timer divide configuration encodings. */
#define LAPIC_TIMER_DIV_2       0x00u
#define LAPIC_TIMER_DIV_4       0x01u
#define LAPIC_TIMER_DIV_8       0x02u
#define LAPIC_TIMER_DIV_16      0x03u
#define LAPIC_TIMER_DIV_32      0x08u
#define LAPIC_TIMER_DIV_64      0x09u
#define LAPIC_TIMER_DIV_128     0x0Au
#define LAPIC_TIMER_DIV_1       0x0Bu

/* LVT timer mode field (bits 17-18). */
typedef enum {
    LAPIC_TIMER_ONESHOT  = 0u,
    LAPIC_TIMER_PERIODIC = 1u,
    LAPIC_TIMER_DEADLINE = 2u
} lapic_timer_mode_t;

/* Bind the driver to an MMIO base (NULL selects LAPIC_DEFAULT_BASE). */
bool lapic_init(volatile void *mmio_base);

/* Issue an inter-processor interrupt through the ICR. */
void lapic_send_ipi(uint8_t dest_apic_id, uint32_t icr_low);

/* Acknowledge the in-service interrupt (write 0 to the EOI register). */
void lapic_eoi(void);

/* Program the LVT timer: vector, initial count, divider and mode. */
void lapic_timer_set(uint8_t vector, uint32_t initial_count,
                     uint8_t divider, lapic_timer_mode_t mode);

/* Read the local APIC id (top 8 bits of the ID register). */
uint8_t lapic_get_id(void);

/* Enable the local APIC with the given spurious vector. */
bool lapic_soft_enable(uint8_t spurious_vector);

/* True when the APIC software-enable bit is set. */
bool lapic_is_enabled(void);

#endif /* LAPIC_H */
