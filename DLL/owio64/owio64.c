/*
 * owio64.c - OpenWindows Port I/O Abstraction Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owio64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owio64_ready = false;

/* ── Low-level helpers ────────────────────────────────────────── */

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

static inline uint16_t ow_inw(uint16_t port)
{
    uint16_t v;
    __asm__ __volatile__("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void ow_outw(uint16_t port, uint16_t value)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(port));
}

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

/* ── Implementation ───────────────────────────────────────────── */

bool owio_init(void)
{
    owio64_ready = true;
    return true;
}

bool owio_is_ready(void)
{
    return owio64_ready;
}

uint8_t owio_inb(uint16_t port)
{
    return ow_inb(port);
}

void owio_outb(uint16_t port, uint8_t value)
{
    ow_outb(port, value);
}

uint16_t owio_inw(uint16_t port)
{
    return ow_inw(port);
}

void owio_outw(uint16_t port, uint16_t value)
{
    ow_outw(port, value);
}

uint32_t owio_ind(uint16_t port)
{
    return ow_inl(port);
}

void owio_outd(uint16_t port, uint32_t value)
{
    ow_outl(port, value);
}

uint32_t owio_inl(uint16_t port)
{
    return ow_inl(port);
}

void owio_outl(uint16_t port, uint32_t value)
{
    ow_outl(port, value);
}

void owio_wait(void)
{
    /* A write to the notoriously unmapped port 0x80 costs ~1us on most
     * chipsets and is the canonical ISA I/O delay. */
    ow_outb((uint16_t)OWIO_DELAY_PORT, 0u);
}
