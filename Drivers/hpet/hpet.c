/*
 * hpet.c - OpenWindows HPET Timer Driver (.owc)
 *
 * The 64-bit main counter is latched by reading the high word twice and
 * retrying when it changed, matching the HPET specification's race-free
 * read sequence. Comparators are loaded high-word-first.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "hpet.h"

/* ── Internal state ───────────────────────────────────────────── */

static volatile uint8_t *hpet_base;
static uint64_t hpet_period_fs;
static uint32_t hpet_timers;
static bool hpet_counter64;
static bool hpet_initialized;

/* ── Register access ──────────────────────────────────────────── */

static uint32_t hpet_read32(uint32_t offset)
{
    return *(volatile uint32_t *)(hpet_base + offset);
}

static void hpet_write32(uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(hpet_base + offset) = value;
}

static uint64_t hpet_read64(uint32_t offset)
{
    uint32_t low  = hpet_read32(offset);
    uint32_t high = hpet_read32(offset + 4u);
    return ((uint64_t)high << 32) | (uint64_t)low;
}

static void hpet_write64(uint32_t offset, uint64_t value)
{
    /* High word first keeps the comparator from matching a transient value. */
    hpet_write32(offset + 4u, (uint32_t)(value >> 32));
    hpet_write32(offset, (uint32_t)value);
}

/* ── Public API ───────────────────────────────────────────────── */

bool hpet_init(volatile void *mmio_base)
{
    hpet_base = (volatile uint8_t *)mmio_base;
    hpet_period_fs = 0u;
    hpet_timers = 0u;
    hpet_counter64 = false;
    hpet_initialized = false;

    if (!hpet_base) return false;

    uint32_t capabilities = hpet_read32(HPET_REG_CAPABILITIES);
    hpet_period_fs = (uint64_t)hpet_read32(HPET_REG_PERIOD);
    hpet_timers    = ((capabilities >> 8) & 0x1Fu) + 1u;
    hpet_counter64 = (capabilities & HPET_CAP_COUNT_SIZE) != 0u;

    /* Halt the counter until the caller enables it explicitly. */
    uint32_t config = hpet_read32(HPET_REG_CONFIG);
    hpet_write32(HPET_REG_CONFIG, config & ~HPET_CFG_ENABLE);

    hpet_initialized = true;
    return true;
}

uint64_t hpet_read_counter(void)
{
    if (!hpet_initialized) return 0u;

    if (!hpet_counter64) {
        return (uint64_t)hpet_read32(HPET_REG_MAIN_COUNTER);
    }

    /* Race-free 64-bit read: retry while the high word is unstable. */
    for (uint32_t i = 0u; i < 8u; ++i) {
        uint32_t high  = hpet_read32(HPET_REG_MAIN_COUNTER + 4u);
        uint32_t low   = hpet_read32(HPET_REG_MAIN_COUNTER);
        uint32_t again = hpet_read32(HPET_REG_MAIN_COUNTER + 4u);
        if (high == again) {
            return ((uint64_t)high << 32) | (uint64_t)low;
        }
    }
    return hpet_read64(HPET_REG_MAIN_COUNTER);
}

bool hpet_set_comparator(uint32_t timer_index, uint64_t value, bool periodic)
{
    if (!hpet_initialized || timer_index >= hpet_timers) return false;

    uint32_t config_offset  = HPET_REG_TIMER_BASE +
                              (timer_index * HPET_TIMER_STRIDE);
    uint32_t compare_offset = config_offset + 8u;

    uint32_t config = hpet_read32(config_offset);
    bool timer64 = (config & HPET_TN_64BIT_CAP) != 0u;

    /* Disarm the timer while its comparator is reprogrammed. */
    config &= ~(HPET_TN_INT_ENABLE | HPET_TN_TYPE_MASK | HPET_TN_PERIODIC_EN);
    if (periodic) {
        config |= HPET_TN_TYPE_PERIODIC | HPET_TN_PERIODIC_EN;
    }
    if (!timer64) {
        config |= HPET_TN_32BIT;
    }
    hpet_write32(config_offset, config);

    hpet_write64(compare_offset, value);

    hpet_write32(config_offset, config | HPET_TN_INT_ENABLE);
    return true;
}

void hpet_enable(bool enable)
{
    if (!hpet_initialized) return;

    uint32_t config = hpet_read32(HPET_REG_CONFIG);
    if (enable) config |= HPET_CFG_ENABLE;
    else        config &= ~HPET_CFG_ENABLE;
    hpet_write32(HPET_REG_CONFIG, config);
}

uint64_t hpet_period_femtoseconds(void)
{
    return hpet_period_fs;
}

uint32_t hpet_timer_count(void)
{
    return hpet_timers;
}

bool hpet_is_64bit(void)
{
    return hpet_counter64;
}

bool hpet_is_initialized(void)
{
    return hpet_initialized;
}
