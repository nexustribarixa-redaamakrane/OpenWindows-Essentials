/*
 * spi.c - OpenWindows SPI Bus Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Drives a generic memory-mapped SPI master. A transfer shifts one byte per
 * write to (and read from) the data register, giving a full duplex exchange.
 * Only volatile 32-bit register accesses are used.
 */

#include "spi.h"

/* ── Register access helpers ──────────────────────────────────── */

/* Read a 32-bit controller register at a byte offset. */
static uint32_t spi_reg_read(const spi_ctrl_t *ctrl, uint32_t off)
{
    return ctrl->regs[off / 4u];
}

/* Write a 32-bit controller register at a byte offset. */
static void spi_reg_write(const spi_ctrl_t *ctrl, uint32_t off, uint32_t value)
{
    ctrl->regs[off / 4u] = value;
}

/* Translate a target SCLK into the core's divider (SCLK = clock / divider).
 * The divider is clamped into the representable range. */
static uint32_t spi_divider_for(uint32_t base_clock_hz, uint32_t hz)
{
    uint32_t div;

    if (hz == 0u) {
        return SPI_MAX_DIVIDER;
    }

    div = base_clock_hz / hz;
    if (div == 0u) {
        div = 1u;
    }
    if (div > SPI_MAX_DIVIDER) {
        div = SPI_MAX_DIVIDER;
    }
    return div;
}

/* True when ctrl is usable for a transfer. */
static bool spi_ready(const spi_ctrl_t *ctrl)
{
    return (ctrl != NULL) && ctrl->initialized && (ctrl->regs != NULL);
}

/* Wait until the transmit FIFO can accept a byte. */
static spi_status_t spi_wait_tx(const spi_ctrl_t *ctrl)
{
    uint32_t budget = SPI_TIMEOUT_LOOPS;

    while (budget != 0u) {
        uint32_t st = spi_reg_read(ctrl, SPI_REG_STATUS);

        if ((st & SPI_STS_TIMEOUT) != 0u) {
            return SPI_ERR_TIMEOUT;
        }
        if ((st & SPI_STS_TX_FULL) == 0u) {
            return SPI_OK;
        }
        budget--;
    }

    return SPI_ERR_TIMEOUT;
}

/* Wait until a received byte is available. */
static spi_status_t spi_wait_rx(const spi_ctrl_t *ctrl)
{
    uint32_t budget = SPI_TIMEOUT_LOOPS;

    while (budget != 0u) {
        uint32_t st = spi_reg_read(ctrl, SPI_REG_STATUS);

        if ((st & SPI_STS_TIMEOUT) != 0u) {
            return SPI_ERR_TIMEOUT;
        }
        if ((st & SPI_STS_RX_EMPTY) == 0u) {
            return SPI_OK;
        }
        budget--;
    }

    return SPI_ERR_TIMEOUT;
}

/* ── Public API ───────────────────────────────────────────────── */

bool spi_init(spi_ctrl_t *ctrl, volatile uint32_t *regs,
              uint32_t base_clock_hz)
{
    if ((ctrl == NULL) || (regs == NULL)) {
        return false;
    }
    if (base_clock_hz < SPI_MIN_CLOCK_HZ) {
        return false;
    }

    ctrl->regs = regs;
    ctrl->base_clock_hz = base_clock_hz;
    ctrl->mode = SPI_MODE_0;
    ctrl->speed_hz = SPI_DEFAULT_HZ;

    spi_reg_write(ctrl, SPI_REG_DIVIDER,
                  spi_divider_for(base_clock_hz, SPI_DEFAULT_HZ));
    /* Enable the core, mode 0 (CPOL=0, CPHA=0), MSB first. */
    spi_reg_write(ctrl, SPI_REG_CTRL, SPI_CTRL_EN);

    ctrl->initialized = true;
    return true;
}

spi_status_t spi_transfer(spi_ctrl_t *ctrl, const uint8_t *tx,
                          uint8_t *rx, size_t len)
{
    size_t i;

    if (!spi_ready(ctrl) || (tx == NULL) || (len == 0u)) {
        return SPI_ERR_PARAM;
    }

    for (i = 0u; i < len; i++) {
        spi_status_t st;

        st = spi_wait_tx(ctrl);
        if (st != SPI_OK) {
            return st;
        }
        spi_reg_write(ctrl, SPI_REG_DATA, (uint32_t)tx[i]);

        st = spi_wait_rx(ctrl);
        if (st != SPI_OK) {
            return st;
        }

        if (rx != NULL) {
            rx[i] = (uint8_t)(spi_reg_read(ctrl, SPI_REG_DATA) & 0xFFu);
        } else {
            /* Drain the receive FIFO so the next byte has room. */
            (void)spi_reg_read(ctrl, SPI_REG_DATA);
        }
    }

    return SPI_OK;
}

bool spi_set_mode(spi_ctrl_t *ctrl, spi_mode_t mode)
{
    uint32_t ctrl_val;

    if (!spi_ready(ctrl)) {
        return false;
    }
    if ((uint32_t)mode > (uint32_t)SPI_MODE_3) {
        return false;
    }

    ctrl_val = spi_reg_read(ctrl, SPI_REG_CTRL);
    ctrl_val &= ~(SPI_CTRL_CPOL | SPI_CTRL_CPHA);

    if (((uint32_t)mode & 0x2u) != 0u) {
        ctrl_val |= SPI_CTRL_CPOL;
    }
    if (((uint32_t)mode & 0x1u) != 0u) {
        ctrl_val |= SPI_CTRL_CPHA;
    }

    spi_reg_write(ctrl, SPI_REG_CTRL, ctrl_val);
    ctrl->mode = mode;
    return true;
}

spi_status_t spi_set_speed(spi_ctrl_t *ctrl, uint32_t hz)
{
    if (!spi_ready(ctrl)) {
        return SPI_ERR_PARAM;
    }
    if ((hz < SPI_MIN_CLOCK_HZ) || (hz > ctrl->base_clock_hz)) {
        return SPI_ERR_RANGE;
    }

    spi_reg_write(ctrl, SPI_REG_DIVIDER,
                  spi_divider_for(ctrl->base_clock_hz, hz));
    ctrl->speed_hz = hz;
    return SPI_OK;
}
