/*
 * sio.c - Serial I/O (16550 UART) Driver implementation (.owc)
 *
 * C99 freestanding. No heap allocation; all state is caller-provided.
 */

#include "sio.h"
#include "sutf8.h"

/* ================================================================== */
/*  Register Access                                                   */
/* ================================================================== */

static uint8_t reg_read(const sio_port_t *port, uint8_t reg_off)
{
    if (port->io.read8 == NULL) {
        return 0u;
    }
    uint32_t addr = port->base + ((uint32_t)reg_off << port->reg_shift);
    return port->io.read8(port->io.token, addr);
}

static void reg_write(sio_port_t *port, uint8_t reg_off, uint8_t value)
{
    if (port->io.write8 == NULL) {
        return;
    }
    uint32_t addr = port->base + ((uint32_t)reg_off << port->reg_shift);
    port->io.write8(port->io.token, addr, value);
}

/* ================================================================== */
/*  Ring Buffer Primitives                                            */
/* ================================================================== */

static bool txr_full(const sio_port_t *port)
{
    return port->tx_count >= SIO_TX_RING_SIZE;
}

static bool txr_empty(const sio_port_t *port)
{
    return port->tx_count == 0u;
}

static bool txr_push(sio_port_t *port, uint8_t byte)
{
    if (txr_full(port)) {
        return false;
    }
    port->tx_ring[port->tx_head] = byte;
    port->tx_head = (uint16_t)((port->tx_head + 1u) % SIO_TX_RING_SIZE);
    port->tx_count++;
    return true;
}

static uint8_t txr_pop(sio_port_t *port)
{
    uint8_t byte = port->tx_ring[port->tx_tail];
    port->tx_tail = (uint16_t)((port->tx_tail + 1u) % SIO_TX_RING_SIZE);
    port->tx_count--;
    return byte;
}

static bool rxr_full(const sio_port_t *port)
{
    return port->rx_count >= SIO_RX_RING_SIZE;
}

static uint16_t rxr_count(const sio_port_t *port)
{
    return port->rx_count;
}

static bool rxr_push(sio_port_t *port, uint8_t byte)
{
    if (rxr_full(port)) {
        return false;
    }
    port->rx_ring[port->rx_head] = byte;
    port->rx_head = (uint16_t)((port->rx_head + 1u) % SIO_RX_RING_SIZE);
    port->rx_count++;
    return true;
}

static uint8_t rxr_pop(sio_port_t *port)
{
    uint8_t byte = port->rx_ring[port->rx_tail];
    port->rx_tail = (uint16_t)((port->rx_tail + 1u) % SIO_RX_RING_SIZE);
    port->rx_count--;
    return byte;
}

/* ================================================================== */
/*  Line Programming                                                  */
/* ================================================================== */

static uint32_t divisor_for_baud(uint32_t clock_hz, uint32_t baud)
{
    if (baud == 0u) {
        return 0u;
    }
    return (clock_hz + baud * 8u) / (baud * 16u);
}

static sio_status_t program_divisor(sio_port_t *port, uint32_t baud)
{
    uint32_t div = divisor_for_baud(port->clock_hz, baud);
    if (div == 0u || div > 0xFFFFu) {
        return SIO_ERR_LINE_COLLISION;
    }

    uint8_t saved_lcr = reg_read(port, SIO_REG_LCR);
    reg_write(port, SIO_REG_LCR, (uint8_t)(saved_lcr | SIO_LCR_DLAB));
    reg_write(port, SIO_REG_DLL, (uint8_t)(div & 0xFFu));
    reg_write(port, SIO_REG_DLM, (uint8_t)((div >> 8) & 0xFFu));
    reg_write(port, SIO_REG_LCR, saved_lcr);

    port->divisor = div;
    return SIO_OK;
}

static void assert_modem_lines(sio_port_t *port, bool loop)
{
    uint8_t mcr = SIO_MCR_DTR | SIO_MCR_RTS | SIO_MCR_OUT2;
    if (loop) {
        mcr |= SIO_MCR_LOOP;
    }
    reg_write(port, SIO_REG_MCR, mcr);
    port->loopback = loop;
}

/* ================================================================== */
/*  Public API                                                        */
/* ================================================================== */

sio_status_t sio_attach(sio_port_t   *port,
                        const sio_io_t *io,
                        uint32_t      base,
                        uint32_t      clock_hz,
                        uint8_t       irq)
{
    if (port == NULL || io == NULL) {
        return SIO_ERR_NULL_POINTER;
    }
    if (clock_hz == 0u || clock_hz > 0x01000000u) {
        return SIO_ERR_PORT_INVALID;
    }

    port->io             = *io;
    port->base           = base;
    port->reg_shift      = 0u;
    port->irq            = irq;
    port->clock_hz       = clock_hz;
    port->divisor        = 0u;
    port->line_cfg       = SIO_LCR_WL8 | SIO_LCR_STOP1 | SIO_LCR_PARITY_NONE;
    port->fcr            = 0u;
    port->fifo_enabled   = false;
    port->loopback       = false;
    port->initialized    = true;
    port->tx_head        = 0u;
    port->tx_tail        = 0u;
    port->tx_count       = 0u;
    port->rx_head        = 0u;
    port->rx_tail        = 0u;
    port->rx_count       = 0u;
    port->lsr_last       = 0u;
    port->msr_last       = 0u;
    port->carrier        = false;
    port->tx_total       = 0u;
    port->rx_total       = 0u;
    port->rx_overruns    = 0u;
    port->tx_backpressure_events = 0u;
    port->line_error_events      = 0u;

    /* 115200 8N1 by default. FIFO stays closed until sio_open_fifo(). */
    sio_status_t st = program_divisor(port, 115200u);
    if (st != SIO_OK) {
        return st;
    }
    reg_write(port, SIO_REG_LCR, port->line_cfg);
    assert_modem_lines(port, false);
    return SIO_OK;
}

sio_status_t sio_configure(sio_port_t *port, uint32_t baud, uint8_t line_cfg)
{
    if (port == NULL) {
        return SIO_ERR_NULL_POINTER;
    }
    if (!port->initialized) {
        return SIO_ERR_NOT_INIT;
    }
    if ((line_cfg & 0xC0u) != 0u) {
        return SIO_ERR_LINE_COLLISION; /* no DLAB/BREAK in user line cfg */
    }

    sio_status_t st = program_divisor(port, baud);
    if (st != SIO_OK) {
        return st;
    }
    port->line_cfg = line_cfg;
    reg_write(port, SIO_REG_LCR, line_cfg);
    assert_modem_lines(port, port->loopback);
    return SIO_OK;
}

sio_status_t sio_open_fifo(sio_port_t *port, uint8_t trigger)
{
    if (port == NULL) {
        return SIO_ERR_NULL_POINTER;
    }
    if (!port->initialized) {
        return SIO_ERR_NOT_INIT;
    }

    uint8_t fcr = (uint8_t)(SIO_FCR_ENABLE | SIO_FCR_RX_CLEAR | SIO_FCR_TX_CLEAR | trigger);
    reg_write(port, SIO_REG_FCR_IIR, fcr);
    reg_write(port, SIO_REG_FCR_IIR, fcr);

    /* A 16450 ignores FCR entirely; bits 6-7 of IIR stay clear. */
    uint8_t iir = reg_read(port, SIO_REG_FCR_IIR);
    port->fcr          = fcr;
    port->fifo_enabled = (iir & 0xC0u) != 0u;
    if (!port->fifo_enabled) {
        return SIO_ERR_FIFO_FAULT;
    }
    return SIO_OK;
}

sio_status_t sio_close_fifo(sio_port_t *port)
{
    if (port == NULL) {
        return SIO_ERR_NULL_POINTER;
    }
    if (!port->initialized) {
        return SIO_ERR_NOT_INIT;
    }
    reg_write(port, SIO_REG_FCR_IIR, 0u);
    port->fcr          = 0u;
    port->fifo_enabled = false;
    return SIO_OK;
}

sio_status_t sio_pump(sio_port_t *port)
{
    if (port == NULL) {
        return SIO_ERR_NULL_POINTER;
    }
    if (!port->initialized) {
        return SIO_ERR_NOT_INIT;
    }

    uint8_t lsr = reg_read(port, SIO_REG_LSR);
    uint32_t budget = 0u;

    /* Drain hardware RX into the ring (bounded per pump). */
    while ((lsr & SIO_LSR_DR) != 0u) {
        uint8_t byte = reg_read(port, SIO_REG_THR_RBR);
        if (rxr_push(port, byte)) {
            port->rx_total++;
        } else {
            port->rx_overruns++;   /* ring full -> hardware loss */
        }
        if (++budget >= SIO_PUMP_BUDGET) {
            break;
        }
        lsr = reg_read(port, SIO_REG_LSR);
    }

    /* Line error telemetry. */
    if ((lsr & SIO_LSR_OE) != 0u) {
        port->rx_overruns++;
    }
    if ((lsr & SIO_LSR_ERRORS) != 0u) {
        port->line_error_events++;
    }

    /* Push the TX ring into the holding register while THRE is set. */
    if ((lsr & SIO_LSR_THRE) != 0u) {
        while (!txr_empty(port)) {
            reg_write(port, SIO_REG_THR_RBR, txr_pop(port));
            port->tx_total++;
            lsr = reg_read(port, SIO_REG_LSR);
            if ((lsr & SIO_LSR_THRE) == 0u) {
                break;
            }
        }
    }

    /* Carrier / modem tracking. */
    uint8_t msr = reg_read(port, SIO_REG_MSR);
    port->msr_last = msr;
    port->carrier  = (msr & SIO_MSR_DCD) != 0u;
    port->lsr_last = lsr;
    return SIO_OK;
}

sio_status_t sio_write(sio_port_t   *port,
                       const uint8_t *data,
                       uint32_t      len,
                       uint32_t     *written,
                       bool          blocking)
{
    if (port == NULL || data == NULL) {
        return SIO_ERR_NULL_POINTER;
    }
    if (!port->initialized) {
        return SIO_ERR_NOT_INIT;
    }

    uint32_t done = 0u;
    uint32_t iters = 0u;
    while (done < len) {
        if (txr_full(port)) {
            sio_pump(port);
        }
        if (txr_push(port, data[done])) {
            done++;
        } else {
            /* Ring still full after pump. */
            if (!blocking) {
                break;
            }
            if (++iters >= SIO_BLOCK_ITERS) {
                break;
            }
        }
    }
    sio_pump(port);

    if (written != NULL) {
        *written = done;
    }
    if (done != len) {
        port->tx_backpressure_events++;
        return SIO_ERR_TX_BACKPRESSURE;
    }
    return SIO_OK;
}

sio_status_t sio_read(sio_port_t *port,
                      uint8_t     *data,
                      uint32_t     cap,
                      uint32_t    *out_read,
                      bool         blocking)
{
    if (port == NULL || data == NULL) {
        return SIO_ERR_NULL_POINTER;
    }
    if (!port->initialized) {
        return SIO_ERR_NOT_INIT;
    }

    uint32_t got = 0u;
    uint32_t iters = 0u;
    while (got < cap) {
        sio_pump(port);
        while (got < cap && rxr_count(port) > 0u) {
            data[got++] = rxr_pop(port);
        }
        if (got > 0u || !blocking) {
            break;
        }
        if (++iters >= SIO_BLOCK_ITERS) {
            break;
        }
    }

    if (out_read != NULL) {
        *out_read = got;
    }
    return SIO_OK;
}

sio_status_t sio_flush(sio_port_t *port)
{
    if (port == NULL) {
        return SIO_ERR_NULL_POINTER;
    }
    if (!port->initialized) {
        return SIO_ERR_NOT_INIT;
    }
    uint32_t iters = 0u;
    while (!txr_empty(port) || (sio_txidle(port) == false)) {
        sio_pump(port);
        if (++iters >= SIO_BLOCK_ITERS) {
            return SIO_ERR_BUSY;
        }
    }
    return SIO_OK;
}

bool sio_txidle(const sio_port_t *port)
{
    if (port == NULL) {
        return true;
    }
    if (!port->initialized) {
        return true;
    }
    uint8_t lsr = reg_read(port, SIO_REG_LSR);
    return (lsr & SIO_LSR_TEMT) != 0u;
}

sio_status_t sio_self_test(sio_port_t *port)
{
    if (port == NULL) {
        return SIO_ERR_NULL_POINTER;
    }
    if (!port->initialized) {
        return SIO_ERR_NOT_INIT;
    }

    /* Route TX->RX internally (LCR bit 4) and drop FIFO fills first. */
    reg_write(port, SIO_REG_FCR_IIR, 0u);
    reg_write(port, SIO_REG_LCR, (uint8_t)(port->line_cfg | SIO_LCR_LOOP));

    static const uint8_t probe[5] = { 0x4B, 0x31, 0x5F, 0xA5, 0x00 };
    uint8_t echo[5];
    bool ok = true;

    for (uint32_t i = 0u; i < sizeof(probe); i++) {
        uint32_t iters = 0u;
        while ((reg_read(port, SIO_REG_LSR) & SIO_LSR_THRE) == 0u) {
            if (++iters >= SIO_BLOCK_ITERS) {
                ok = false;
                break;
            }
        }
        if (!ok) {
            break;
        }
        reg_write(port, SIO_REG_THR_RBR, probe[i]);
        iters = 0u;
        while ((reg_read(port, SIO_REG_LSR) & SIO_LSR_DR) == 0u) {
            if (++iters >= SIO_BLOCK_ITERS) {
                ok = false;
                break;
            }
        }
        if (!ok) {
            break;
        }
        echo[i] = reg_read(port, SIO_REG_THR_RBR);
        if (echo[i] != probe[i]) {
            ok = false;
            break;
        }
    }

    /* Restore normal line routing. */
    reg_write(port, SIO_REG_LCR, port->line_cfg);
    assert_modem_lines(port, port->loopback);

    if (!ok) {
        return SIO_ERR_LINE_FAULT;
    }
    return SIO_OK;
}

sio_status_t sio_write_sucs(sio_port_t *port, sucs_char_t cp)
{
    if (port == NULL) {
        return SIO_ERR_NULL_POINTER;
    }
    uint8_t frame[6];
    size_t flen = sutf8_encode_char(cp, frame, sizeof(frame));
    if (flen == 0u) {
        return SIO_ERR_LINE_COLLISION; /* untransmissible codepoint */
    }
    uint32_t written = 0u;
    return sio_write(port, frame, (uint32_t)flen, &written, true);
}

sio_status_t sio_puts_sucs(sio_port_t *port, const uint8_t *sutf8)
{
    if (port == NULL || sutf8 == NULL) {
        return SIO_ERR_NULL_POINTER;
    }
    while (sutf8[0] != 0u) {
        sucs_char_t cp;
        size_t adv = sutf8_next_codepoint(sutf8, 6u, &cp);
        if (adv == 0u) {
            break;
        }
        if (cp != SUCS_INVALID_CODEPOINT) {
            uint32_t written = 0u;
            sio_status_t st = sio_write(port, sutf8, (uint32_t)adv, &written, true);
            if (st != SIO_OK) {
                return st;
            }
        }
        sutf8 += adv;
    }
    return SIO_OK;
}

/* ================================================================== */
/*  Ring Portal Gate Publication                                      */
/* ================================================================== */

uint32_t sio_register_gates(owrp_context_t *ctx, sio_gate_registrar_t registrar)
{
    if (ctx == NULL || registrar == NULL) {
        return 0;
    }
    uint32_t rc;
    rc = registrar(ctx, 0x070u, "sio.write", 3u, 3u, 0x05u); /* ACTIVE|BOOT */
    if (rc != 0u) return rc;
    rc = registrar(ctx, 0x071u, "sio.read", 2u, 3u, 0x01u);
    if (rc != 0u) return rc;
    rc = registrar(ctx, 0x072u, "sio.idle", 1u, 3u, 0x01u);
    return rc;
}

sio_status_t sio_gate_write(sio_port_t *port,
                            const uint8_t *data,
                            uint32_t      len,
                            uint32_t     *written)
{
    return sio_write(port, data, len, written, false);
}