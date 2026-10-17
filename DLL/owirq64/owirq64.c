/*
 * owirq64.c - OpenWindows IRQ Management Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owirq64.h"

/* ── Internal state ───────────────────────────────────────────── */

#define OWIRQ_PIC_MASTER_CMD   0x20u
#define OWIRQ_PIC_MASTER_DATA  0x21u
#define OWIRQ_PIC_SLAVE_CMD    0xA0u
#define OWIRQ_PIC_SLAVE_DATA   0xA1u
#define OWIRQ_PIC_EOI          0x20u
#define OWIRQ_CASCADE_IRQ      2u     /* IRQ2 links master to slave      */

static owirq_state_t owirq64_state;

/* ── Port I/O helpers ─────────────────────────────────────────── */

static inline uint8_t ow_inb(uint16_t port)
{
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void ow_outb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* ── Helpers ──────────────────────────────────────────────────── */

/* Write the cached masks back to both PIC data registers. */
static void owirq_apply_masks(void)
{
    ow_outb((uint16_t)OWIRQ_PIC_MASTER_DATA, owirq64_state.master_mask);
    ow_outb((uint16_t)OWIRQ_PIC_SLAVE_DATA, owirq64_state.slave_mask);
}

/* ── Implementation ───────────────────────────────────────────── */

bool owirq_init(void)
{
    uint32_t i;

    /* Snapshot the live masks so we can restore them on teardown paths. */
    owirq64_state.master_mask = ow_inb((uint16_t)OWIRQ_PIC_MASTER_DATA);
    owirq64_state.slave_mask = ow_inb((uint16_t)OWIRQ_PIC_SLAVE_DATA);

    for (i = 0u; i < OWIRQ_MAX_LINES; ++i) {
        owirq64_state.slots[i].handler = NULL;
        owirq64_state.slots[i].context = NULL;
        owirq64_state.slots[i].invocations = 0u;
        owirq64_state.slots[i].enabled = false;
    }

    /* Start from a known quiescent state: every line masked. */
    owirq64_state.master_mask = 0xFFu;
    owirq64_state.slave_mask = 0xFFu;
    owirq_apply_masks();

    owirq64_state.initialized = true;
    return true;
}

uint32_t owirq_register(uint32_t irq, owirq_handler_fn handler, void *context)
{
    if (!owirq64_state.initialized) {
        return OWIRQ_ERR_STATE;
    }
    if (irq >= OWIRQ_MAX_LINES) {
        return OWIRQ_ERR_RANGE;
    }
    if (handler == NULL) {
        return OWIRQ_ERR_PARAM;
    }
    if (owirq64_state.slots[irq].handler != NULL) {
        return OWIRQ_ERR_IN_USE;
    }

    owirq64_state.slots[irq].handler = handler;
    owirq64_state.slots[irq].context = context;
    owirq64_state.slots[irq].invocations = 0u;
    owirq64_state.slots[irq].enabled = false;
    return OWIRQ_OK;
}

uint32_t owirq_unregister(uint32_t irq)
{
    if (!owirq64_state.initialized) {
        return OWIRQ_ERR_STATE;
    }
    if (irq >= OWIRQ_MAX_LINES) {
        return OWIRQ_ERR_RANGE;
    }

    (void)owirq_disable(irq);
    owirq64_state.slots[irq].handler = NULL;
    owirq64_state.slots[irq].context = NULL;
    owirq64_state.slots[irq].invocations = 0u;
    return OWIRQ_OK;
}

uint32_t owirq_enable(uint32_t irq)
{
    if (!owirq64_state.initialized) {
        return OWIRQ_ERR_STATE;
    }
    if (irq >= OWIRQ_MAX_LINES) {
        return OWIRQ_ERR_RANGE;
    }

    if (irq < 8u) {
        owirq64_state.master_mask &= (uint8_t)~(1u << irq);
    } else {
        owirq64_state.slave_mask &= (uint8_t)~(1u << (irq - 8u));
        /* Slave lines are only reachable through the cascade. */
        owirq64_state.master_mask &= (uint8_t)~(1u << OWIRQ_CASCADE_IRQ);
    }

    owirq_apply_masks();
    owirq64_state.slots[irq].enabled = true;
    return OWIRQ_OK;
}

uint32_t owirq_disable(uint32_t irq)
{
    if (!owirq64_state.initialized) {
        return OWIRQ_ERR_STATE;
    }
    if (irq >= OWIRQ_MAX_LINES) {
        return OWIRQ_ERR_RANGE;
    }

    if (irq < 8u) {
        owirq64_state.master_mask |= (uint8_t)(1u << irq);
    } else {
        owirq64_state.slave_mask |= (uint8_t)(1u << (irq - 8u));
        /* Drop the cascade once no slave line remains unmasked. */
        if (owirq64_state.slave_mask == 0xFFu) {
            owirq64_state.master_mask |= (uint8_t)(1u << OWIRQ_CASCADE_IRQ);
        }
    }

    owirq_apply_masks();
    owirq64_state.slots[irq].enabled = false;
    return OWIRQ_OK;
}

uint32_t owirq_acknowledge(uint32_t irq)
{
    if (!owirq64_state.initialized) {
        return OWIRQ_ERR_STATE;
    }
    if (irq >= OWIRQ_MAX_LINES) {
        return OWIRQ_ERR_RANGE;
    }

    /* Slave-sourced interrupts must be acked at the slave first. */
    if (irq >= 8u) {
        ow_outb((uint16_t)OWIRQ_PIC_SLAVE_CMD, (uint8_t)OWIRQ_PIC_EOI);
    }
    ow_outb((uint16_t)OWIRQ_PIC_MASTER_CMD, (uint8_t)OWIRQ_PIC_EOI);
    return OWIRQ_OK;
}

uint32_t owirq_dispatch(uint32_t irq)
{
    owirq_slot_t *slot;

    if (!owirq64_state.initialized) {
        return OWIRQ_ERR_STATE;
    }
    if (irq >= OWIRQ_MAX_LINES) {
        return OWIRQ_ERR_RANGE;
    }

    slot = &owirq64_state.slots[irq];
    if (slot->handler == NULL) {
        /* Spurious interrupt: still acknowledge so the PIC unblocks. */
        (void)owirq_acknowledge(irq);
        return OWIRQ_ERR_PARAM;
    }

    slot->invocations++;
    slot->handler(irq, slot->context);
    (void)owirq_acknowledge(irq);
    return OWIRQ_OK;
}

uint16_t owirq_get_mask(void)
{
    /* Report the unmasked (enabled) lines, i.e. the inverse of OCW1. */
    uint16_t master = (uint16_t)(~owirq64_state.master_mask & 0xFFu);
    uint16_t slave = (uint16_t)(~owirq64_state.slave_mask & 0xFFu);
    return (uint16_t)(master | (slave << 8));
}

bool owirq_is_enabled(uint32_t irq)
{
    if (irq >= OWIRQ_MAX_LINES) {
        return false;
    }
    return owirq64_state.slots[irq].enabled;
}

owirq_state_t *owirq_state(void)
{
    return &owirq64_state;
}
