/*
 * owirq64.h - OpenWindows IRQ Management Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Legacy 8259 PIC based IRQ routing for the 16 ISA interrupt lines.
 * Handlers are stored in a fixed table; enable/disable manipulate the
 * PIC interrupt mask registers on ports 0x21 (master) and 0xA1 (slave).
 */

#ifndef OWIRQ64_H
#define OWIRQ64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────────── */

#define OWIRQ_OK            0u  /* operation succeeded                  */
#define OWIRQ_ERR_RANGE     1u  /* IRQ number out of range              */
#define OWIRQ_ERR_PARAM     2u  /* null handler or bad argument         */
#define OWIRQ_ERR_STATE     3u  /* library not initialized              */
#define OWIRQ_ERR_IN_USE    4u  /* line already has a handler           */

/* ── Configuration ────────────────────────────────────────────────── */

#define OWIRQ_MAX_LINES     16u /* ISA IRQ0..IRQ15                      */

/* ── Public types ─────────────────────────────────────────────────── */

/* Interrupt service routine signature. */
typedef void (*owirq_handler_fn)(uint32_t irq, void *context);

/* One installable IRQ line. */
typedef struct {
    owirq_handler_fn handler;   /* NULL when the line is free           */
    void            *context;   /* opaque handler context               */
    uint64_t         invocations;
    bool             enabled;
} owirq_slot_t;

/* Global IRQ controller state. */
typedef struct {
    owirq_slot_t slots[OWIRQ_MAX_LINES];
    uint8_t      master_mask;   /* cached OCW1 for port 0x21            */
    uint8_t      slave_mask;    /* cached OCW1 for port 0xA1            */
    bool         initialized;
} owirq_state_t;

/* ── API ──────────────────────────────────────────────────────────── */

/* Quiesce both PICs, clear the handler table and latch the current masks. */
bool owirq_init(void);

/* Install `handler` on `irq`. Returns OWIRQ_ERR_IN_USE if occupied. */
uint32_t owirq_register(uint32_t irq, owirq_handler_fn handler, void *context);

/* Remove the handler installed on `irq` and mask the line. */
uint32_t owirq_unregister(uint32_t irq);

/* Unmask `irq` on the PIC (also unmasks the cascade for slave lines). */
uint32_t owirq_enable(uint32_t irq);

/* Mask `irq` on the PIC (masks the cascade once the slave is empty). */
uint32_t owirq_disable(uint32_t irq);

/* Send end-of-interrupt to the PIC(s) that raised `irq`. */
uint32_t owirq_acknowledge(uint32_t irq);

/* Invoke the installed handler for `irq` (called from the ISR trampoline). */
uint32_t owirq_dispatch(uint32_t irq);

/* Current unmasked lines as a 16-bit bitmap. */
uint16_t owirq_get_mask(void);

/* True if the line is unmasked. */
bool owirq_is_enabled(uint32_t irq);

/* Pointer to the global controller state. */
owirq_state_t *owirq_state(void);

#endif /* OWIRQ64_H */
