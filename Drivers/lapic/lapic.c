/*
 * lapic.c - OpenWindows Local APIC Driver (.owc)
 *
 * All registers are accessed as 32-bit volatile MMIO words. The ICR is
 * only written after the previous delivery has drained.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "lapic.h"

/* ── Internal state ───────────────────────────────────────────── */

static volatile uint8_t *lapic_base;
static bool lapic_initialized;

#define LAPIC_ICR_WAIT_LIMIT 1000000u

/* ── Register access ──────────────────────────────────────────── */

static uint32_t lapic_read(uint16_t reg)
{
    return *(volatile uint32_t *)(lapic_base + reg);
}

static void lapic_write(uint16_t reg, uint32_t value)
{
    *(volatile uint32_t *)(lapic_base + reg) = value;
}

/* Poll until the ICR no longer reports a pending delivery. */
static bool lapic_icr_ready(void)
{
    for (uint32_t i = 0u; i < LAPIC_ICR_WAIT_LIMIT; ++i) {
        if ((lapic_read(LAPIC_REG_ICR_LOW) & LAPIC_ICR_DELIVERY_PENDING) == 0u) {
            return true;
        }
    }
    return false;
}

/* ── Public API ───────────────────────────────────────────────── */

bool lapic_init(volatile void *mmio_base)
{
    lapic_base = (volatile uint8_t *)(mmio_base
                    ? mmio_base
                    : (volatile void *)(uintptr_t)LAPIC_DEFAULT_BASE);
    lapic_initialized = false;

    if (!lapic_base) return false;

    /* Run at task priority 0; interrupts stay masked until soft_enable. */
    lapic_write(LAPIC_REG_TPR, 0u);

    lapic_initialized = true;
    return true;
}

void lapic_send_ipi(uint8_t dest_apic_id, uint32_t icr_low)
{
    if (!lapic_initialized) return;
    if (!lapic_icr_ready()) return;

    lapic_write(LAPIC_REG_ICR_HIGH, (uint32_t)dest_apic_id << 24);
    lapic_write(LAPIC_REG_ICR_LOW,
                icr_low & ~LAPIC_ICR_DELIVERY_PENDING);
}

void lapic_eoi(void)
{
    if (!lapic_initialized) return;
    lapic_write(LAPIC_REG_EOI, 0u);
}

void lapic_timer_set(uint8_t vector, uint32_t initial_count,
                     uint8_t divider, lapic_timer_mode_t mode)
{
    if (!lapic_initialized) return;

    lapic_write(LAPIC_REG_TIMER_DIV, (uint32_t)(divider & 0x0Fu));

    uint32_t lvt = (uint32_t)vector & 0xFFu;
    lvt |= ((uint32_t)mode & 0x03u) << 17;   /* mode field */
    lapic_write(LAPIC_REG_LVT_TIMER, lvt);

    lapic_write(LAPIC_REG_TIMER_INIT, initial_count);
}

uint8_t lapic_get_id(void)
{
    if (!lapic_initialized) return 0u;
    return (uint8_t)((lapic_read(LAPIC_REG_ID) >> 24) & 0xFFu);
}

bool lapic_soft_enable(uint8_t spurious_vector)
{
    if (!lapic_initialized) return false;
    lapic_write(LAPIC_REG_SVR,
                LAPIC_SVR_ENABLE | (uint32_t)spurious_vector);
    return true;
}

bool lapic_is_enabled(void)
{
    if (!lapic_initialized) return false;
    return (lapic_read(LAPIC_REG_SVR) & LAPIC_SVR_ENABLE) != 0u;
}
