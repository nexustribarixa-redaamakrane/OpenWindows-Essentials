/*
 * ehci.h - OpenWindows Enhanced Host Controller Interface (USB 2.0) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Register-level model of the EHCI host controller (EHCI 1.0, section 2).
 * All descriptor memory (queue heads, qTDs, the frame list) is supplied by
 * the caller so the driver never allocates. The controller is driven purely
 * through memory-mapped I/O using volatile 32-bit pointers.
 */

#ifndef EHCI_H
#define EHCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Limits ───────────────────────────────────────────────────── */

#define EHCI_MAX_PORTS          15u     /* HCS_N_PORTS range 1..15      */
#define EHCI_MAX_DEVICE_ADDR    127u    /* 7-bit USB device address     */
#define EHCI_MAX_ENDPOINT       15u     /* 4-bit endpoint number        */
#define EHCI_DEFAULT_TIMEOUT    100000u /* bounded poll iterations      */
#define EHCI_RESET_HOLD         2000u   /* busy-wait units for reset    */

/* ── Capability registers (offsets from the MMIO capability base) ─ */

#define EHCI_CAP_CAPLENGTH      0x00u   /* CAPLENGTH (byte)             */
#define EHCI_CAP_HCIVERSION     0x02u   /* HCIVERSION (word)            */
#define EHCI_CAP_HCSPARAMS      0x04u   /* HCSPARAMS (dword)            */
#define EHCI_CAP_HCCPARAMS      0x08u   /* HCCPARAMS (dword)            */
#define EHCI_CAP_PORTROUTE      0x0Cu   /* HCSP-PORTROUTE (nibbles)     */

/* HCSPARAMS bit fields */
#define EHCI_HCS_N_PORTS(p)     ((uint8_t)((p) & 0x0Fu))
#define EHCI_HCS_PPC(p)         (((p) & (1u << 4)) != 0u)
#define EHCI_HCS_P_INDICATOR(p) (((p) & (1u << 16)) != 0u)

/* HCCPARAMS bit fields */
#define EHCI_HCC_64BIT_ADDR(p)  (((p) & (1u << 0)) != 0u)
#define EHCI_HCC_PGM_FRAMELIST(p) (((p) & (1u << 1)) != 0u)
#define EHCI_HCC_EECP(p)        ((uint8_t)(((p) >> 8) & 0xFFu))

/* ── Operational registers (offsets from base + CAPLENGTH) ────── */

#define EHCI_OP_USBCMD          0x00u   /* USB command                  */
#define EHCI_OP_USBSTS          0x04u   /* USB status                   */
#define EHCI_OP_USBINTR         0x08u   /* USB interrupt enable         */
#define EHCI_OP_FRINDEX         0x0Cu   /* frame index                  */
#define EHCI_OP_CTRLDSSEGMENT   0x10u   /* 64-bit data segment selector */
#define EHCI_OP_PERIODICLISTBASE 0x14u  /* periodic frame list base     */
#define EHCI_OP_ASYNCLISTADDR   0x18u   /* async queue head address     */
#define EHCI_OP_CONFIGFLAG      0x40u   /* configure flag               */
#define EHCI_OP_PORTSC          0x44u   /* first port status/control    */

/* USBCMD bits */
#define EHCI_CMD_RUN            (1u << 0)   /* run/stop                  */
#define EHCI_CMD_RESET          (1u << 1)   /* host controller reset     */
#define EHCI_CMD_PSE            (1u << 4)   /* periodic schedule enable  */
#define EHCI_CMD_ASE            (1u << 5)   /* async schedule enable     */
#define EHCI_CMD_IAAD           (1u << 6)   /* interrupt async doorbell  */
#define EHCI_CMD_ITC_8          (8u << 16)  /* 8 uframe interrupt spacing*/

/* USBSTS bits */
#define EHCI_STS_USBINT         (1u << 0)   /* normal completion         */
#define EHCI_STS_ERRINT         (1u << 1)   /* error completion          */
#define EHCI_STS_PCD            (1u << 2)   /* port change detect        */
#define EHCI_STS_FLR            (1u << 3)   /* frame list rollover       */
#define EHCI_STS_HSE            (1u << 4)   /* host system error         */
#define EHCI_STS_IAA            (1u << 5)   /* async advance interrupt   */
#define EHCI_STS_HALT           (1u << 12)  /* controller halted         */

/* CONFIGFLAG */
#define EHCI_CF_PORT_ROUTE      (1u << 0)   /* route ports to EHCI       */

/* PORTSC bits */
#define EHCI_PORT_CONNECT       (1u << 0)   /* current connect status    */
#define EHCI_PORT_CSC           (1u << 1)   /* connect status change     */
#define EHCI_PORT_PE            (1u << 2)   /* port enabled              */
#define EHCI_PORT_PEC           (1u << 3)   /* port enable change        */
#define EHCI_PORT_OC            (1u << 4)   /* over-current active       */
#define EHCI_PORT_OCC           (1u << 5)   /* over-current change       */
#define EHCI_PORT_RESUME        (1u << 6)   /* force port resume         */
#define EHCI_PORT_SUSPEND       (1u << 7)   /* port suspend              */
#define EHCI_PORT_RESET         (1u << 8)   /* port reset                */
#define EHCI_PORT_LS_MASK       (3u << 10)  /* line status               */
#define EHCI_PORT_LS_LOW        (1u << 10)  /* low-speed device          */
#define EHCI_PORT_POWER         (1u << 12)  /* port power                */
#define EHCI_PORT_OWNER         (1u << 13)  /* companion owns port       */
#define EHCI_PORT_RWC           (EHCI_PORT_CSC | EHCI_PORT_PEC | EHCI_PORT_OCC)

/* Endpoint speed encoding used by ehci_urb_t / ehci_port_status_t.
 * The queue head stores this value in bits 13:12. */
#define EHCI_SPEED_FULL         0u
#define EHCI_SPEED_LOW          1u
#define EHCI_SPEED_HIGH         2u

/* ── Queue head / qTD type tags ───────────────────────────────── */

#define EHCI_Q_TYPE_ITD         (0u << 1)
#define EHCI_Q_TYPE_QH          (1u << 1)
#define EHCI_Q_TYPE_SITD        (2u << 1)
#define EHCI_Q_TYPE_FSTN        (3u << 1)
#define EHCI_LIST_END           1u          /* terminate link pointer    */

/* qTD token fields */
#define EHCI_QTD_PID_OUT        0u
#define EHCI_QTD_PID_IN         1u
#define EHCI_QTD_PID_SETUP      2u
#define EHCI_QTD_STS_PING       (1u << 0)
#define EHCI_QTD_STS_STS        (1u << 1)
#define EHCI_QTD_STS_MMF        (1u << 2)
#define EHCI_QTD_STS_XACT       (1u << 3)
#define EHCI_QTD_STS_BABBLE     (1u << 4)
#define EHCI_QTD_STS_DBE        (1u << 5)
#define EHCI_QTD_STS_HALT       (1u << 6)
#define EHCI_QTD_STS_ACTIVE     (1u << 7)
#define EHCI_QTD_IOC            (1u << 15)
#define EHCI_QTD_TOGGLE         (1u << 31)
#define EHCI_QTD_CERR(n)        (((uint32_t)(n) & 0x3u) << 10)
#define EHCI_QTD_PID(p)         (((uint32_t)(p) & 0x3u) << 8)
#define EHCI_QTD_BYTES(n)       (((uint32_t)(n) & 0x7FFFu) << 16)

/* Queue head endpoint characteristics (dword 1).
 * Layout: address[6:0], inactivate[7], endpoint[11:8], speed[13:12],
 * toggle[14], head[15], max packet[26:16], control[27], RL[31:28]. */
#define EHCI_QH_ADDRESS(n)      ((uint32_t)(n) & 0x7Fu)
#define EHCI_QH_INACTIVATE      (1u << 7)
#define EHCI_QH_ENDPT(n)        (((uint32_t)(n) & 0x0Fu) << 8)
#define EHCI_QH_SPEED(n)        (((uint32_t)(n) & 0x3u) << 12)
#define EHCI_QH_TOGGLE_CTL      (1u << 14)
#define EHCI_QH_HEAD            (1u << 15)
#define EHCI_QH_MPL(n)          (((uint32_t)(n) & 0x7FFu) << 16)
#define EHCI_QH_CONTROL_EP      (1u << 27)
#define EHCI_QH_RL(n)           (((uint32_t)(n) & 0x0Fu) << 28)

/* Queue head endpoint capabilities (dword 2). */
#define EHCI_QH_SMASK(n)        ((uint32_t)(n) & 0xFFu)
#define EHCI_QH_CMASK(n)        (((uint32_t)(n) & 0xFFu) << 8)
#define EHCI_QH_HUB_ADDR(n)     (((uint32_t)(n) & 0x7Fu) << 16)
#define EHCI_QH_HUB_PORT(n)     (((uint32_t)(n) & 0x7Fu) << 23)
#define EHCI_QH_MULT(n)         (((uint32_t)(n) & 0x3u) << 30)

/* ── Hardware descriptor layouts ──────────────────────────────── */

/* Queue Element Transfer Descriptor: 32 bytes, 32-bit addressing. */
typedef struct {
    uint32_t next;          /* next qTD pointer (or EHCI_LIST_END)      */
    uint32_t alt_next;      /* alternate next qTD pointer               */
    uint32_t token;         /* status / PID / CERR / bytes / toggle     */
    uint32_t buffer[5];     /* 4 KiB page buffer pointers               */
} __attribute__((aligned(32))) ehci_qtd_t;

/* Queue Head: 48 hardware bytes (+ HCD padding), 32-byte aligned. */
typedef struct {
    uint32_t horiz;         /* horizontal link pointer                  */
    uint32_t ep_char;       /* endpoint characteristics                 */
    uint32_t ep_caps;       /* endpoint capabilities                    */
    uint32_t current;       /* current qTD pointer                      */
    uint32_t next_qtd;      /* overlay: next qTD pointer                */
    uint32_t alt_next;      /* overlay: alternate next qTD pointer      */
    uint32_t token;         /* overlay: token                           */
    uint32_t buffer[5];     /* overlay: buffer pointers                 */
} __attribute__((aligned(32))) ehci_qh_t;

/* Transfer description for a single URB submitted on the async list. */
typedef struct {
    uint8_t  device_address;    /* 1..127                               */
    uint8_t  endpoint;          /* 0..15                                */
    uint8_t  hub_address;       /* 0 for the root hub                   */
    uint8_t  hub_port;          /* 0 for the root hub                   */
    uint8_t  pid;               /* EHCI_QTD_PID_*                       */
    uint8_t  toggle;            /* initial data toggle                  */
    uint8_t  cerr;              /* error retry count 0..3               */
    uint8_t  speed;             /* EHCI_SPEED_*                         */
    uint16_t max_packet;        /* endpoint max packet size             */
    uint32_t length;            /* transfer length in bytes             */
} ehci_urb_t;

/* Decoded port status, filled by ehci_poll_port(). */
typedef struct {
    uint32_t raw;               /* raw PORTSC value                     */
    uint8_t  speed;             /* EHCI_SPEED_*                         */
    bool     connected;         /* device present                       */
    bool     enabled;           /* port enabled                         */
    bool     suspended;         /* port suspended                       */
    bool     resetting;         /* reset in progress                    */
    bool     over_current;      /* over-current active                  */
    bool     owned_by_companion;/* handed to companion controller       */
} ehci_port_status_t;

/* Controller instance. Zero-initialised by the caller before init. */
typedef struct {
    volatile uint8_t  *cap_base;    /* MMIO capability base             */
    volatile uint32_t *op_base;     /* MMIO operational base            */
    uint32_t hcsparams;             /* cached HCSPARAMS                 */
    uint32_t hccparams;             /* cached HCCPARAMS                 */
    uint8_t  caplength;             /* CAPLENGTH in bytes               */
    uint8_t  num_ports;             /* N_PORTS                          */
    bool     initialized;           /* init completed                   */
    bool     running;               /* run/stop bit asserted            */
} ehci_ctrl_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Map the controller at mmio_base, reset it and start the async schedule. */
bool ehci_init(ehci_ctrl_t *hc, volatile uint8_t *mmio_base);

/* Halt the controller and perform a host controller reset. */
bool ehci_reset(ehci_ctrl_t *hc);

/* Sample one root port, clear change bits and drive a reset if needed. */
bool ehci_poll_port(ehci_ctrl_t *hc, uint8_t port, ehci_port_status_t *status_out);

/* Build a qTD + queue head for urb and place them on the async schedule. */
bool ehci_submit_urb(ehci_ctrl_t *hc, const ehci_urb_t *urb,
                     ehci_qtd_t *qtd, ehci_qh_t *qh, void *buffer);

/* Port status helpers operating on a raw PORTSC value. */
bool ehci_port_connected(uint32_t portsc);
bool ehci_port_enabled(uint32_t portsc);
bool ehci_port_reset_active(uint32_t portsc);
bool ehci_port_is_low_speed(uint32_t portsc);

#endif /* EHCI_H */
