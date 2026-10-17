/*
 * i2c.h - OpenWindows I2C Bus Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Register-level model of a generic memory-mapped I2C master (DesignWare /
 * OpenCores style). The controller exposes 32-bit registers for control,
 * status, data, slave address, clock pre-scale and command issue. The bus
 * base address and clock rates are supplied by the caller so the driver
 * never allocates and never assumes a fixed physical mapping.
 */

#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Limits ───────────────────────────────────────────────────── */

#define I2C_TIMEOUT_LOOPS       100000u /* bounded poll iterations      */
#define I2C_SCAN_FIRST          0x03u   /* first probed 7-bit address   */
#define I2C_SCAN_LAST           0x77u   /* last probed 7-bit address    */
#define I2C_MIN_CLOCK_HZ        1000u   /* lowest accepted bus clock    */
#define I2C_FAST_THRESHOLD_HZ   100000u /* above this use fast mode      */

/* ── Register offsets (32-bit registers) ──────────────────────── */

#define I2C_REG_CTRL            0x00u   /* control                      */
#define I2C_REG_STATUS          0x04u   /* status                       */
#define I2C_REG_DATA            0x08u   /* data / receive buffer        */
#define I2C_REG_ADDR            0x0Cu   /* slave address (7-bit << 1)   */
#define I2C_REG_PRESCALE        0x10u   /* clock pre-scale divisor      */
#define I2C_REG_CMD             0x14u   /* command issue                */

/* CTRL bits */
#define I2C_CTRL_EN             (1u << 0)   /* core enable              */
#define I2C_CTRL_FAST           (1u << 1)   /* fast mode (400 kHz)      */
#define I2C_CTRL_AA             (1u << 2)   /* assert ACK               */

/* STATUS bits */
#define I2C_STS_BUSY            (1u << 0)   /* transfer in progress     */
#define I2C_STS_NACK            (1u << 1)   /* slave did not acknowledge*/
#define I2C_STS_ARB             (1u << 2)   /* arbitration lost         */
#define I2C_STS_TIMEOUT         (1u << 3)   /* bus stalled              */

/* CMD bits */
#define I2C_CMD_START           (1u << 0)   /* issue (repeated) start   */
#define I2C_CMD_STOP            (1u << 1)   /* issue stop               */
#define I2C_CMD_READ            (1u << 2)   /* read one byte            */
#define I2C_CMD_WRITE           (1u << 3)   /* write one byte           */
#define I2C_CMD_ACK             (1u << 4)   /* ACK the byte just read   */
#define I2C_CMD_IACK            (1u << 5)   /* clear latched status     */

/* ── Status codes ─────────────────────────────────────────────── */

typedef enum {
    I2C_OK = 0,
    I2C_ERR_PARAM,      /* bad argument                              */
    I2C_ERR_NACK,       /* slave did not acknowledge                 */
    I2C_ERR_BUS,        /* controller not initialised                */
    I2C_ERR_TIMEOUT,    /* transfer did not finish in time           */
    I2C_ERR_ARB         /* arbitration lost                          */
} i2c_status_t;

/* ── Controller instance ──────────────────────────────────────── */

typedef struct {
    volatile uint32_t *regs;            /* MMIO register base           */
    uint32_t base_clock_hz;             /* input clock to the core       */
    uint32_t bus_hz;                    /* requested SCL frequency       */
    bool     initialized;               /* init completed                */
} i2c_ctrl_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Map the controller at regs and program the clock generator. */
bool i2c_init(i2c_ctrl_t *ctrl, volatile uint32_t *regs,
              uint32_t base_clock_hz, uint32_t bus_hz);

/* Read len bytes from a 7-bit slave address into buf. */
i2c_status_t i2c_read(i2c_ctrl_t *ctrl, uint8_t dev_addr,
                      uint8_t *buf, size_t len);

/* Write len bytes from buf to a 7-bit slave address. */
i2c_status_t i2c_write(i2c_ctrl_t *ctrl, uint8_t dev_addr,
                       const uint8_t *buf, size_t len);

/* Probe every 7-bit address on the bus. Store responders in found (up to
 * cap entries) and always report the total in count_out. found may be
 * NULL to only count. */
i2c_status_t i2c_scan_bus(i2c_ctrl_t *ctrl, uint8_t *found,
                          size_t cap, size_t *count_out);

#endif /* I2C_H */
