/*
 * sio.h - Serial I/O (16550 UART) Driver Interface (.owc)
 *
 * Ring-0 bare-metal serial driver for the 16550 UART family and direct
 * compatibles (16450 fallback with FIFO disabled). Provides polled and
 * interrupt-assisted character I/O over static ring buffers, full line
 * programming (baud, word length, parity, stop bits), FIFO depth control,
 * loopback self-diagnostics, and a SuperUnicode SUTF-8 transport path.
 *
 * Register access is abstracted behind an I/O hook pair so the same driver
 * drives either classic x86 port I/O or memory-mapped register windows
 * (reg_shift = 0 for byte ports, 2 for 32-bit MMIO).
 *
 * Ring Portal integration: the driver exposes a gate registrar hook so the
 * kernel syscall layer can publish OWRP_SYS_SIO_WRITE / _READ / _IDLE
 * (system call numbers 0x070-0x072, see owrp.h). sio_gate_write() is the
 * ring-0 dispatch the syscall handler invokes for user-console output.
 *
 * Conforms to OWC1 binary layout (Extensions/owc_format.h).
 * C99 freestanding - stdint/stdbool/stddef only, zero heap, static buffers.
 */

#ifndef OWE_SIO_H
#define OWE_SIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "owc_format.h"
#include "owrp.h"
#include "sucs_types.h"

/* ------------------------------------------------------------------ */
/*  OWC1 Binary Header Metadata                                        */
/* ------------------------------------------------------------------ */

#define SIO_LIB_NAME            "sio.owc"
#define SIO_TARGET_ARCH         0x02u          /* x86_64               */
#define SIO_INIT_REQUIREMENTS   (OWC_INIT_REQUIRES_OWRP | \
                                 OWC_INIT_REQUIRES_BANC)
#define SIO_DRIVER_FLAGS        (OWC_FLAG_BOOT_DRIVER | \
                                 OWC_FLAG_PRIVILEGED | \
                                 OWC_FLAG_SENTINEL_AWARE)

/* ------------------------------------------------------------------ */
/*  Classic x86 COM Port Bases / Clock                                 */
/* ------------------------------------------------------------------ */

#define SIO_PORT_COM1           0x3F8u
#define SIO_PORT_COM2           0x2F8u
#define SIO_PORT_COM3           0x3E8u
#define SIO_PORT_COM4           0x2E8u
#define SIO_DEFAULT_CLOCK_HZ    1843200u        /* Standard COM crystal */

/* ------------------------------------------------------------------ */
/*  16550 Register Offsets                                             */
/*  (offset 0 doubles as THR/RBR with DLAB cleared, DLL with DLAB set) */
/* ------------------------------------------------------------------ */

#define SIO_REG_THR_RBR         0x00u
#define SIO_REG_DLL             0x00u           /* DLAB=1, low divisor  */
#define SIO_REG_IER             0x01u
#define SIO_REG_DLM             0x01u           /* DLAB=1, high divisor */
#define SIO_REG_FCR_IIR         0x02u
#define SIO_REG_LCR             0x03u
#define SIO_REG_MCR             0x04u
#define SIO_REG_LSR             0x05u
#define SIO_REG_MSR             0x06u
#define SIO_REG_SCR             0x07u
#define SIO_FIFO_HARD_DEPTH     16u

/* -- Line control (LCR) bits ---------------------------------------- */
#define SIO_LCR_WL5             0x00u
#define SIO_LCR_WL6             0x01u
#define SIO_LCR_WL7             0x02u
#define SIO_LCR_WL8             0x03u
#define SIO_LCR_STOP1           0x00u
#define SIO_LCR_STOP2           0x04u
#define SIO_LCR_PARITY_NONE     0x00u
#define SIO_LCR_PARITY_ODD      0x08u
#define SIO_LCR_PARITY_EVEN     0x18u
#define SIO_LCR_PARITY_STICKY1  0x28u
#define SIO_LCR_PARITY_STICKY0  0x38u
#define SIO_LCR_BREAK           0x40u
#define SIO_LCR_DLAB            0x80u
#define SIO_LCR_LOOP            0x10u

/* -- Modem control (MCR) bits --------------------------------------- */
#define SIO_MCR_DTR             0x01u
#define SIO_MCR_RTS             0x02u
#define SIO_MCR_OUT1            0x04u
#define SIO_MCR_OUT2            0x08u
#define SIO_MCR_LOOP            0x10u
#define SIO_MCR_SW_OUT          0x20u

/* -- Line status (LSR) bits ----------------------------------------- */
#define SIO_LSR_DR              0x01u           /* Receive data ready   */
#define SIO_LSR_OE              0x02u           /* Overrun error        */
#define SIO_LSR_PE              0x04u           /* Parity error         */
#define SIO_LSR_FE              0x08u           /* Framing error        */
#define SIO_LSR_BI              0x10u           /* Break interrupt      */
#define SIO_LSR_THRE            0x20u           /* TX holding empty     */
#define SIO_LSR_TEMT            0x40u           /* TX empty (shift empty)*/
#define SIO_LSR_ERRORS          (SIO_LSR_OE | SIO_LSR_PE | SIO_LSR_FE | SIO_LSR_BI)

/* -- Modem status (MSR) bits ---------------------------------------- */
#define SIO_MSR_DCD             0x80u
#define SIO_MSR_RI              0x40u
#define SIO_MSR_DSR             0x20u
#define SIO_MSR_CTS             0x10u

/* -- FIFO control (FCR) bits ---------------------------------------- */
#define SIO_FCR_ENABLE          0x01u
#define SIO_FCR_RX_CLEAR        0x02u
#define SIO_FCR_TX_CLEAR        0x04u
#define SIO_FCR_DMA_MODE        0x08u
#define SIO_FCR_TRIG_1          0x00u
#define SIO_FCR_TRIG_4          0x40u
#define SIO_FCR_TRIG_8          0x80u
#define SIO_FCR_TRIG_14         0xC0u

/* ------------------------------------------------------------------ */
/*  Status Codes - BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A7FF): Fatal line faults                      */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal backpressure / overrun       */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                     */
/* ------------------------------------------------------------------ */

typedef uint32_t sio_status_t;

#define SIO_OK                          0x00000000u  /* Success              */
/* B+ Fatal */
#define SIO_ERR_PORT_INVALID            0x0011A2E0u  /* Bad port geometry   */
#define SIO_ERR_FIFO_FAULT              0x0011A2E1u  /* FIFO misbehaviour   */
#define SIO_ERR_LINE_FAULT              0x0011A2E2u  /* Loopback mismatch   */
#define SIO_ERR_TRANSFER_OVERRUN        0x0011A2E3u  /* Hard overrun, no RX */
/* W+ Warning */
#define SIO_ERR_TX_BACKPRESSURE         0x0011A940u  /* TX ring full        */
#define SIO_ERR_RX_OVERRUN              0x0011A941u  /* RX bytes dropped    */
/* S+ Soft */
#define SIO_ERR_NOT_INIT                0x0011AE80u  /* Port not attached   */
#define SIO_ERR_BUSY                    0x0011AE81u  /* Transfer in flight  */
#define SIO_ERR_NULL_POINTER            0x0011AE82u  /* Null argument       */
#define SIO_ERR_LINE_COLLISION          0x0011AE83u  /* Conflicting config  */

/* ------------------------------------------------------------------ */
/*  I/O Abstraction Hooks                                              */
/* ------------------------------------------------------------------ */

typedef uint8_t (*sio_read8_fn)(uintptr_t token, uint32_t reg_addr);
typedef void    (*sio_write8_fn)(uintptr_t token, uint32_t reg_addr, uint8_t value);

typedef struct {
    sio_read8_fn   read8;
    sio_write8_fn  write8;
    uintptr_t      token;
} sio_io_t;

/* ------------------------------------------------------------------ */
/*  Ring Buffer Geometry                                               */
/* ------------------------------------------------------------------ */

#define SIO_TX_RING_SIZE        512u
#define SIO_RX_RING_SIZE        256u
#define SIO_MAX_PORTS           4u
#define SIO_PUMP_BUDGET         32u     /* Max RX bytes drained/pump   */
#define SIO_BLOCK_ITERS         0x8000u /* Bounded poll budget         */

/* ------------------------------------------------------------------ */
/*  Port Context                                                       */
/*  Constant size: caller-allocated, zero dynamic allocation.          */
/* ------------------------------------------------------------------ */

typedef struct {
    /* Port geometry */
    sio_io_t     io;
    uint32_t     base;          /* Port base or MMIO register address  */
    uint8_t      reg_shift;     /* 0 = byte ports, 2 = 32-bit MMIO     */
    uint8_t      irq;           /* Assigned IRQ (0 = none / polled)    */
    uint32_t     clock_hz;
    uint32_t     divisor;       /* Programmed baud divisor             */
    uint8_t      line_cfg;      /* Current LCR value                   */
    uint8_t      fcr;           /* Current FCR flavor                  */
    bool         fifo_enabled;
    bool         loopback;
    bool         initialized;

    /* Static character buffers (never heap) */
    uint8_t      tx_ring[SIO_TX_RING_SIZE];
    uint16_t     tx_head;
    uint16_t     tx_tail;
    uint16_t     tx_count;
    uint8_t      rx_ring[SIO_RX_RING_SIZE];
    uint16_t     rx_head;
    uint16_t     rx_tail;
    uint16_t     rx_count;

    /* Line / modem state */
    uint8_t      lsr_last;
    uint8_t      msr_last;
    bool         carrier;       /* DCD true */

    /* Telemetry */
    uint64_t     tx_total;
    uint64_t     rx_total;
    uint32_t     rx_overruns;
    uint32_t     tx_backpressure_events;
    uint32_t     line_error_events;
} sio_port_t;

/* ------------------------------------------------------------------ */
/*  Gate Registration                                                 */
/* ------------------------------------------------------------------ */

/* Registrar signature matching owrp_register_gate(). The kernel binds
 * its own owrp_register_gate as the registrar so sio.owc remains
 * dependency-free while publishing OWRP_SYS_SIO_WRITE / _READ / _IDLE. */
typedef uint32_t (*sio_gate_registrar_t)(owrp_context_t *ctx,
                                         uint32_t        syscall_nr,
                                         const char     *name,
                                         uint8_t         param_count,
                                         uint16_t        required_ring,
                                         uint8_t         flags);

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

/*
 * Attach a UART to the port context. `io` provides the register read/write
 * hooks, `base` the register window base, `clock_hz` the UART clock and
 * `irq` the assigned interrupt (0 for pure polling). Divisor is computed
 * for the default 115200 baud 8N1; use sio_configure() after attach for a
 * custom line setup.
 */
sio_status_t sio_attach(sio_port_t   *port,
                        const sio_io_t *io,
                        uint32_t      base,
                        uint32_t      clock_hz,
                        uint8_t       irq);

/*
 * Program the line. `baud` selects the divisor; `line_cfg` is composed of
 * SIO_LCR_WLx | SIO_LCR_STOPx | SIO_LCR_PARITY_x. DTR/RTS are asserted.
 */
sio_status_t sio_configure(sio_port_t *port, uint32_t baud, uint8_t line_cfg);

/*
 * Enable the 16550 FIFO with the given trigger (SIO_FCR_TRIG_*). A 16450
 * that does not latch the enable bit reports SIO_ERR_FIFO_FAULT (soft path
 * via polling loopback remains available with FIFO off).
 */
sio_status_t sio_open_fifo(sio_port_t *port, uint8_t trigger);
sio_status_t sio_close_fifo(sio_port_t *port);

/*
 * Service pass: drains the hardware RX FIFO into the receive ring and
 * pushes the transmit ring into the THR. Call from the ISR after a line
 * interrupt, or poll in a tick path when irq == 0.
 */
sio_status_t sio_pump(sio_port_t *port);

/*
 * Enqueue `len` bytes. Non-blocking drops data when the TX ring is full
 * (returns SIO_ERR_TX_BACKPRESSURE, *written reflects actual). Blocking
 * mode runs a bounded poll with sio_pump() and reports the drop the same
 * way if the budget is exhausted (never spins forever).
 */
sio_status_t sio_write(sio_port_t   *port,
                       const uint8_t *data,
                       uint32_t      len,
                       uint32_t     *written,
                       bool          blocking);

/*
 * Receive up to `cap` bytes. Non-blocking returns what is already in the
 * receive ring; blocking polls bounded until at least one byte arrives or
 * the budget expires.
 */
sio_status_t sio_read(sio_port_t *port,
                      uint8_t     *data,
                      uint32_t     cap,
                      uint32_t    *out_read,
                      bool         blocking);

/* Wait until the TX ring drains and TEMT is asserted. Bounded poll. */
sio_status_t sio_flush(sio_port_t *port);
bool         sio_txidle(const sio_port_t *port);

/*
 * Loopback self-test: LCR bit 4 routes TX to RX internally. Writes a probe
 * pattern through the configured line and verifies the echoed bytes.
 */
sio_status_t sio_self_test(sio_port_t *port);

/*
 * SuperUnicode transport: encode one SUCS codepoint to its SUTF-8 frame and
 * transmit (or stream a whole NUL-terminated SUTF-8 string through the
 * line). Malformed streams are skipped one byte at a time.
 */
sio_status_t sio_write_sucs(sio_port_t *port, sucs_char_t cp);
sio_status_t sio_puts_sucs(sio_port_t *port, const uint8_t *sutf8);

/*
 * Publish gates in the OWRP gate table via the registrar hook. Bind
 * owrp_register_gate itself in the kernel to enable ring-3 console writes.
 */
uint32_t sio_register_gates(owrp_context_t *ctx, sio_gate_registrar_t registrar);

/*
 * Ring-0 dispatch backing OWRP_SYS_SIO_WRITE. Called by the syscall handler
 * for port index `idx`; never blocks.
 */
sio_status_t sio_gate_write(sio_port_t *port,
                            const uint8_t *data,
                            uint32_t      len,
                            uint32_t     *written);

#endif /* OWE_SIO_H */