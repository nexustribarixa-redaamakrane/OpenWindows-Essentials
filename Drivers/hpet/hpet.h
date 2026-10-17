/*
 * hpet.h - OpenWindows HPET Timer Driver (.owc)
 *
 * Drives a High Precision Event Timer through its memory-mapped
 * register bank: parses the capabilities block (tick period, timer
 * count, 64-bit counter support), reads the main counter and programs
 * per-timer comparators.
 *
 * C99 freestanding. Zero dynamic heap allocation; the caller provides
 * the MMIO base address.
 */

#ifndef HPET_H
#define HPET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Register offsets. */
#define HPET_REG_CAPABILITIES   0x000u  /* general capabilities     */
#define HPET_REG_PERIOD         0x004u  /* tick period (femtoseconds) */
#define HPET_REG_CONFIG         0x010u  /* general configuration    */
#define HPET_REG_MAIN_COUNTER   0x0F0u  /* free-running main counter */
#define HPET_REG_TIMER_BASE     0x100u  /* first timer configuration */
#define HPET_TIMER_STRIDE       0x020u  /* bytes between timers     */

/* General configuration bits. */
#define HPET_CFG_ENABLE         0x0001u
#define HPET_CFG_LEGACY_ROUTE   0x0002u

/* Capabilities bits. */
#define HPET_CAP_COUNT_SIZE     0x2000u  /* 1 = 64-bit main counter  */
#define HPET_CAP_LEGACY         0x8000u  /* legacy replacement capable */

/* Per-timer configuration bits. */
#define HPET_TN_TYPE_MASK       0x0006u  /* bits 1-2: timer type     */
#define HPET_TN_TYPE_PERIODIC   0x0002u  /* 01 = periodic            */
#define HPET_TN_PERIODIC_EN     0x0010u  /* periodic interrupt enable */
#define HPET_TN_32BIT           0x0040u  /* force 32-bit counter     */
#define HPET_TN_64BIT_CAP       0x0100u  /* timer is 64-bit capable  */
#define HPET_TN_INT_ENABLE      0x2000u  /* interrupt enable         */

/* Bind the driver to an MMIO base; returns false when NULL. */
bool hpet_init(volatile void *mmio_base);

/* Read the free-running main counter (32- or 64-bit as configured). */
uint64_t hpet_read_counter(void);

/* Arm timer 'timer_index' with a comparator value. */
bool hpet_set_comparator(uint32_t timer_index, uint64_t value, bool periodic);

/* Start or stop the main counter. */
void hpet_enable(bool enable);

/* Capability read-back. */
uint64_t hpet_period_femtoseconds(void);
uint32_t hpet_timer_count(void);
bool     hpet_is_64bit(void);
bool     hpet_is_initialized(void);

#endif /* HPET_H */
