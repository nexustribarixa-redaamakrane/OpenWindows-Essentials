/*
 * spi.h - OpenWindows SPI Bus Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Register-level model of a generic memory-mapped SPI master. The controller
 * has a 32-bit data register that performs a full duplex shift, a status
 * register exposing the FIFO levels and a clock divider programmed from the
 * caller supplied SPI clock. The MMIO base is caller provided.
 */

#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Limits ───────────────────────────────────────────────────── */

#define SPI_TIMEOUT_LOOPS       100000u /* bounded poll iterations      */
#define SPI_MAX_DIVIDER         65536u  /* largest clock divider        */
#define SPI_DEFAULT_HZ          1000000u/* safe power-on clock (1 MHz)  */
#define SPI_MIN_CLOCK_HZ        1000u   /* lowest accepted clock        */

/* ── Register offsets (32-bit registers) ──────────────────────── */

#define SPI_REG_CTRL            0x00u   /* control                      */
#define SPI_REG_STATUS          0x04u   /* status                       */
#define SPI_REG_DATA            0x08u   /* full duplex data             */
#define SPI_REG_DIVIDER         0x0Cu   /* SCLK divider                 */

/* CTRL bits */
#define SPI_CTRL_EN             (1u << 0)   /* core enable              */
#define SPI_CTRL_CPOL           (1u << 1)   /* clock polarity (idle hi) */
#define SPI_CTRL_CPHA           (1u << 2)   /* clock phase (2nd edge)   */
#define SPI_CTRL_LSB_FIRST      (1u << 3)   /* shift least significant  */
#define SPI_CTRL_CS_ACTIVE      (1u << 4)   /* chip select asserted     */

/* STATUS bits */
#define SPI_STS_BUSY            (1u << 0)   /* shift in progress        */
#define SPI_STS_TX_FULL         (1u << 1)   /* transmit FIFO full       */
#define SPI_STS_RX_EMPTY        (1u << 2)   /* receive FIFO empty       */
#define SPI_STS_TIMEOUT         (1u << 3)   /* bus stalled              */

/* ── Status codes ─────────────────────────────────────────────── */

typedef enum {
    SPI_OK = 0,
    SPI_ERR_PARAM,      /* bad argument                              */
    SPI_ERR_BUS,        /* controller not initialised                */
    SPI_ERR_TIMEOUT,    /* transfer did not finish in time           */
    SPI_ERR_RANGE       /* clock outside the representable range     */
} spi_status_t;

/* SPI clock modes: CPOL is the idle level, CPHA the sampling edge. */
typedef enum {
    SPI_MODE_0 = 0,     /* CPOL=0, CPHA=0                            */
    SPI_MODE_1 = 1,     /* CPOL=0, CPHA=1                            */
    SPI_MODE_2 = 2,     /* CPOL=1, CPHA=0                            */
    SPI_MODE_3 = 3      /* CPOL=1, CPHA=1                            */
} spi_mode_t;

/* ── Controller instance ──────────────────────────────────────── */

typedef struct {
    volatile uint32_t *regs;            /* MMIO register base           */
    uint32_t base_clock_hz;             /* SPI module input clock        */
    uint32_t speed_hz;                  /* current SCLK frequency        */
    spi_mode_t mode;                    /* current clock mode            */
    bool     initialized;               /* init completed                */
} spi_ctrl_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Map the controller at regs and select a safe default clock. */
bool spi_init(spi_ctrl_t *ctrl, volatile uint32_t *regs,
              uint32_t base_clock_hz);

/* Full duplex transfer of len bytes. rx may be NULL to discard input. */
spi_status_t spi_transfer(spi_ctrl_t *ctrl, const uint8_t *tx,
                          uint8_t *rx, size_t len);

/* Select the clock polarity/phase combination. */
bool spi_set_mode(spi_ctrl_t *ctrl, spi_mode_t mode);

/* Program SCLK to (approximately) hz. */
spi_status_t spi_set_speed(spi_ctrl_t *ctrl, uint32_t hz);

#endif /* SPI_H */
