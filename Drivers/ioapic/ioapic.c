/*
 * ioapic.c - OpenWindows I/O APIC Driver (.owc)
 *
 * Register access goes through IOREGSEL/IOWIN. Redirection entries are
 * read-modify-written so the read-only delivery-status (bit 12) and
 * remote-IRR (bit 14) fields are never clobbered.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ioapic.h"

/* ── Internal state ───────────────────────────────────────────── */

static volatile uint8_t *ioapic_base;
static uint32_t ioapic_gsi;
static uint32_t ioapic_entries;
static bool ioapic_initialized;

/* ── Register access ──────────────────────────────────────────── */

static uint32_t ioapic_read(uint8_t reg)
{
    volatile uint32_t *selector =
        (volatile uint32_t *)(ioapic_base + IOAPIC_MMIO_REGSEL);
    volatile uint32_t *window =
        (volatile uint32_t *)(ioapic_base + IOAPIC_MMIO_WIN);
    *selector = (uint32_t)reg;
    return *window;
}

static void ioapic_write(uint8_t reg, uint32_t value)
{
    volatile uint32_t *selector =
        (volatile uint32_t *)(ioapic_base + IOAPIC_MMIO_REGSEL);
    volatile uint32_t *window =
        (volatile uint32_t *)(ioapic_base + IOAPIC_MMIO_WIN);
    *selector = (uint32_t)reg;
    *window = value;
}

/* Translate an absolute GSI to a redirection table index. */
static bool ioapic_irq_index(uint32_t irq, uint32_t *index)
{
    if (!ioapic_initialized) return false;
    if (irq < ioapic_gsi) return false;

    uint32_t local = irq - ioapic_gsi;
    if (local >= ioapic_entries) return false;

    if (index) *index = local;
    return true;
}

static void ioapic_set_mask(uint32_t irq, bool masked)
{
    uint32_t index;
    if (!ioapic_irq_index(irq, &index)) return;

    uint8_t reg = (uint8_t)(IOAPIC_REG_REDTBL + (index * 2u));
    uint32_t low = ioapic_read(reg);
    if (masked) low |= (1u << 16);
    else        low &= ~(1u << 16);
    ioapic_write(reg, low);
}

/* ── Public API ───────────────────────────────────────────────── */

bool ioapic_init(volatile void *mmio_base, uint32_t gsi_base)
{
    ioapic_base = (volatile uint8_t *)(mmio_base
                    ? mmio_base
                    : (volatile void *)(uintptr_t)IOAPIC_DEFAULT_BASE);
    ioapic_gsi = gsi_base;
    ioapic_entries = 0u;
    ioapic_initialized = false;

    /* Version register bits 16-23 hold (max redirection entry index). */
    uint32_t version = ioapic_read(IOAPIC_REG_VERSION);
    ioapic_entries = ((version >> 16) & 0xFFu) + 1u;
    if (ioapic_entries > IOAPIC_MAX_ENTRIES) ioapic_entries = IOAPIC_MAX_ENTRIES;

    /* Mask every entry during bring-up so no stale vector can fire. */
    for (uint32_t i = 0u; i < ioapic_entries; ++i) {
        uint8_t reg = (uint8_t)(IOAPIC_REG_REDTBL + (i * 2u));
        uint32_t low = ioapic_read(reg);
        ioapic_write(reg, low | (1u << 16));
    }

    ioapic_initialized = true;
    return true;
}

bool ioapic_set_irq(uint32_t irq, const ioapic_irq_config_t *config)
{
    uint32_t index;
    if (!config) return false;
    if (!ioapic_irq_index(irq, &index)) return false;

    uint8_t reg_low  = (uint8_t)(IOAPIC_REG_REDTBL + (index * 2u));
    uint8_t reg_high = (uint8_t)(reg_low + 1u);

    uint32_t low  = ioapic_read(reg_low);
    uint32_t high = ioapic_read(reg_high);

    /* Preserve the read-only delivery status and remote IRR bits. */
    uint32_t preserved = low & ((1u << 12) | (1u << 14));

    uint32_t value = (uint32_t)(config->vector & 0xFFu);
    value |= ((uint32_t)(config->delivery & 0x07u)) << 8;
    value |= ((uint32_t)(config->dest_mode & 0x01u)) << 11;
    value |= ((uint32_t)(config->polarity & 0x01u)) << 13;
    value |= ((uint32_t)(config->trigger & 0x01u)) << 15;
    if (config->masked) value |= (1u << 16);
    value |= preserved;

    high &= 0x00FFFFFFu;
    high |= ((uint32_t)config->dest_apic_id) << 24;

    ioapic_write(reg_low, value);
    ioapic_write(reg_high, high);
    return true;
}

void ioapic_mask(uint32_t irq)
{
    ioapic_set_mask(irq, true);
}

void ioapic_unmask(uint32_t irq)
{
    ioapic_set_mask(irq, false);
}

void ioapic_eoi(void)
{
    /* The I/O APIC has no EOI register; completion is signalled to the
       local APIC through its EOI register at a fixed MMIO address. */
    volatile uint32_t *lapic_eoi = (volatile uint32_t *)(uintptr_t)0xFEE000B0u;
    *lapic_eoi = 0u;
}

uint32_t ioapic_gsi_base(void)
{
    return ioapic_gsi;
}

uint32_t ioapic_irq_count(void)
{
    return ioapic_entries;
}

uint8_t ioapic_id(void)
{
    if (!ioapic_initialized) return 0u;
    return (uint8_t)((ioapic_read(IOAPIC_REG_ID) >> 24) & 0x0Fu);
}
