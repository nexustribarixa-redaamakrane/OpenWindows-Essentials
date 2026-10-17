/*
 * pwr.c - OpenWindows Power Management Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * ACPI system-state transitions are performed by writing SLP_TYP | SLP_EN to
 * the PM1a and (optionally) PM1b control blocks.  Rebooting falls back to the
 * PCI reset control port 0xCF9 and then to the 8042 keyboard controller.
 */

#include "pwr.h"

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
static inline void ow_outw(uint16_t port, uint16_t value)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(port));
}

/* ── Constants ────────────────────────────────────────────────── */

#define PWR_PCI_RESET_PORT      0xCF9u   /* PCI reset control register   */
#define PWR_KBC_COMMAND_PORT    0x64u    /* 8042 status/command port     */
#define PWR_KBC_RESET_CMD       0xFEu    /* pulse the CPU reset line     */

/* Legacy fixed ACPI shutdown ports used by common virtual machines. */
#define PWR_QEMU_SHUTDOWN_PORT  0x604u
#define PWR_BOCHS_SHUTDOWN_PORT 0xB004u

/* ── Internal state ───────────────────────────────────────────── */

static pwr_config_t pwr_cfg;
static pwr_state_t  pwr_state = PWR_STATE_UNKNOWN;
static bool         pwr_ready;

/* ── Helpers ──────────────────────────────────────────────────── */

/* Compose the PM1 control value for one ACPI SLP_TYP. */
static uint16_t pwr_slp_value(uint8_t slp_typ)
{
    return (uint16_t)((((uint16_t)slp_typ << PWR_SLP_TYP_SHIFT) & PWR_SLP_TYP_MASK)
                      | PWR_SLP_EN);
}

/* Program PM1a and PM1b with the same SLP_TYP and enable bit. */
static bool pwr_write_slp(uint8_t slp_typ)
{
    uint16_t value = pwr_slp_value(slp_typ);
    bool wrote = false;

    if (pwr_cfg.pm1a_control != 0u) {
        ow_outw(pwr_cfg.pm1a_control, value);
        wrote = true;
    }
    if (pwr_cfg.pm1b_control != 0u) {
        ow_outw(pwr_cfg.pm1b_control, value);
        wrote = true;
    }
    return wrote;
}

/* SLP_TYP selected for a requested sleep depth. */
static uint8_t pwr_slp_typ_for(pwr_sleep_mode_t mode)
{
    if (mode == PWR_SLEEP_S1) {
        return pwr_cfg.slp_typ_s1;
    }
    return pwr_cfg.slp_typ_s3;
}

/* ── Implementation ───────────────────────────────────────────── */

bool pwr_init(const pwr_config_t *config)
{
    if (!config) {
        return false;
    }

    pwr_cfg = *config;
    pwr_state = PWR_STATE_RUNNING;
    pwr_ready = true;
    return true;
}

bool pwr_shutdown(void)
{
    if (!pwr_ready) {
        return false;
    }

    pwr_state = PWR_STATE_SHUTDOWN;

    if (pwr_write_slp(pwr_cfg.slp_typ_s5)) {
        return true;
    }

    /* No ACPI block was supplied: try the well-known VM shutdown ports. */
    ow_outw(PWR_QEMU_SHUTDOWN_PORT, PWR_SLP_EN);
    ow_outw(PWR_BOCHS_SHUTDOWN_PORT, PWR_SLP_EN);
    return false;
}

bool pwr_reboot(void)
{
    if (!pwr_ready) {
        return false;
    }

    pwr_state = PWR_STATE_REBOOT;

    if (!pwr_cfg.prefer_kbc_reset) {
        /* PCI reset control register: SYS_RST then RST_CPU | SYS_RST. */
        ow_outb(PWR_PCI_RESET_PORT, 0x02u);
        ow_outb(PWR_PCI_RESET_PORT, 0x06u);
    }

    /* Fall back to the 8042 CPU-reset command. */
    ow_outb(PWR_KBC_COMMAND_PORT, PWR_KBC_RESET_CMD);
    return true;
}

bool pwr_sleep(pwr_sleep_mode_t mode)
{
    if (!pwr_ready) {
        return false;
    }
    if (mode != PWR_SLEEP_S1 && mode != PWR_SLEEP_S3) {
        return false;
    }

    pwr_state = (mode == PWR_SLEEP_S1) ? PWR_STATE_SLEEP_S1 : PWR_STATE_SLEEP_S3;
    return pwr_write_slp(pwr_slp_typ_for(mode));
}

pwr_state_t pwr_get_state(void)
{
    return pwr_state;
}
