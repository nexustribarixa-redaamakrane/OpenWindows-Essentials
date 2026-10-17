/*
 * xhci.h - OpenWindows Extensible Host Controller Interface (USB 3.x) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Register-level model of the xHCI host controller (xHCI 1.1). Rings,
 * device context arrays and scratch memory are caller-provided; the driver
 * only programs descriptors and doorbells through memory-mapped I/O.
 */

#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Limits ───────────────────────────────────────────────────── */

#define XHCI_MAX_SLOTS          255u    /* MaxSlots field               */
#define XHCI_MAX_PORTS          255u    /* MaxPorts field               */
#define XHCI_RING_MIN_TRBS      2u      /* one TRB plus a link TRB      */
#define XHCI_TIMEOUT            100000u /* bounded poll iterations      */
#define XHCI_RESET_HOLD         4000u   /* busy-wait units for reset    */

/* ── Capability registers (offsets from the MMIO base) ────────── */

#define XHCI_CAP_CAPLENGTH      0x00u   /* CAPLENGTH (byte)             */
#define XHCI_CAP_HCIVERSION     0x02u   /* HCIVERSION (word)            */
#define XHCI_CAP_HCSPARAMS1     0x04u   /* HCSPARAMS1 (dword)           */
#define XHCI_CAP_HCSPARAMS2     0x08u   /* HCSPARAMS2 (dword)           */
#define XHCI_CAP_HCSPARAMS3     0x0Cu   /* HCSPARAMS3 (dword)           */
#define XHCI_CAP_HCCPARAMS1     0x10u   /* HCCPARAMS1 (dword)           */
#define XHCI_CAP_DBOFF          0x14u   /* doorbell array offset        */
#define XHCI_CAP_RTSOFF         0x18u   /* runtime register offset      */
#define XHCI_CAP_HCCPARAMS2     0x1Cu   /* HCCPARAMS2 (dword)           */

/* HCSPARAMS1 fields */
#define XHCI_HCS1_MAX_SLOTS(p)  ((uint8_t)((p) & 0xFFu))
#define XHCI_HCS1_MAX_INTRS(p)  ((uint16_t)(((p) >> 8) & 0x7FFu))
#define XHCI_HCS1_MAX_PORTS(p)  ((uint8_t)(((p) >> 24) & 0xFFu))

/* HCSPARAMS2 fields */
#define XHCI_HCS2_IST(p)        ((uint8_t)((p) & 0xFu))
#define XHCI_HCS2_ERST_MAX(p)   ((uint8_t)(((p) >> 4) & 0xFu))

/* HCCPARAMS1 fields */
#define XHCI_HCC1_AC64(p)       (((p) & (1u << 0)) != 0u)
#define XHCI_HCC1_CSZ(p)        (((p) & (1u << 2)) != 0u)
#define XHCI_HCC1_PPC(p)        (((p) & (1u << 3)) != 0u)
#define XHCI_HCC1_XECP(p)       ((uint16_t)(((p) >> 16) & 0xFFFFu))

/* ── Operational registers (offsets from base + CAPLENGTH) ────── */

#define XHCI_OP_USBCMD          0x00u   /* USB command                  */
#define XHCI_OP_USBSTS          0x04u   /* USB status                   */
#define XHCI_OP_PAGESIZE        0x08u   /* page size                    */
#define XHCI_OP_DNCTRL          0x14u   /* device notification control  */
#define XHCI_OP_CRCR            0x18u   /* command ring control (low)   */
#define XHCI_OP_CRCR_HI         0x1Cu   /* command ring control (high)  */
#define XHCI_OP_DCBAAP          0x30u   /* device context base array    */
#define XHCI_OP_DCBAAP_HI       0x34u   /* device context base array hi */
#define XHCI_OP_CONFIG          0x38u   /* config (max slots enabled)   */

/* USBCMD bits */
#define XHCI_CMD_RS             (1u << 0)   /* run/stop                  */
#define XHCI_CMD_HCRST          (1u << 1)   /* host controller reset     */
#define XHCI_CMD_INTE           (1u << 2)   /* interrupt enable          */
#define XHCI_CMD_HSEE           (1u << 3)   /* host system error enable  */
#define XHCI_CMD_LHCRST         (1u << 7)   /* light reset               */
#define XHCI_CMD_CSS            (1u << 8)   /* controller save state     */
#define XHCI_CMD_CRS            (1u << 9)   /* controller restore state  */
#define XHCI_CMD_EWE            (1u << 10)  /* wrap event enable         */
#define XHCI_CMD_EU3S           (1u << 11)  /* U3 entry enable           */
#define XHCI_CMD_CMC            (1u << 13)  /* csz change detection      */

/* USBSTS bits */
#define XHCI_STS_HCH            (1u << 0)   /* host controller halted    */
#define XHCI_STS_HSE            (1u << 2)   /* host system error         */
#define XHCI_STS_EINT           (1u << 3)   /* event interrupt           */
#define XHCI_STS_PCD            (1u << 4)   /* port change detect        */
#define XHCI_STS_CNR            (1u << 11)  /* controller not ready      */
#define XHCI_STS_HCE            (1u << 12)  /* host controller error     */

/* CRCR bits */
#define XHCI_CRCR_RCS           (1u << 0)   /* ring cycle state          */
#define XHCI_CRCR_CS            (1u << 1)   /* command stop              */
#define XHCI_CRCR_CA            (1u << 2)   /* command abort             */
#define XHCI_CRCR_CRR           (1u << 3)   /* command ring running      */

/* CONFIG bits */
#define XHCI_CONFIG_MAX_SLOTS(n) ((uint32_t)(n) & 0xFFu)

/* ── Port registers (base + 0x400 + (port - 1) * 0x10) ────────── */

#define XHCI_PORT_STRIDE        0x10u
#define XHCI_PORT_BASE          0x400u
#define XHCI_PORT_PORTSC        0x00u
#define XHCI_PORT_PORTPMSC      0x04u
#define XHCI_PORT_PORTLI        0x08u
#define XHCI_PORT_PORTHLPMC     0x0Cu

/* PORTSC bits */
#define XHCI_PORTSC_CCS         (1u << 0)   /* current connect status    */
#define XHCI_PORTSC_PED         (1u << 1)   /* port enabled/disabled     */
#define XHCI_PORTSC_OCA         (1u << 3)   /* over-current active       */
#define XHCI_PORTSC_PR          (1u << 4)   /* port reset                */
#define XHCI_PORTSC_PLS_MASK    (0xFu << 5) /* port link state           */
#define XHCI_PORTSC_PLS(n)      (((uint32_t)(n) & 0xFu) << 5)
#define XHCI_PORTSC_PP          (1u << 9)   /* port power                */
#define XHCI_PORTSC_SPEED_MASK  (0xFu << 10)/* port speed                */
#define XHCI_PORTSC_GET_SPEED(c) (((c) >> 10) & 0xFu)
#define XHCI_PORTSC_GET_PLS(c)   (((c) >> 5) & 0xFu)
#define XHCI_PORTSC_LWS         (1u << 16)  /* link state write strobe   */
#define XHCI_PORTSC_CSC         (1u << 17)  /* connect status change     */
#define XHCI_PORTSC_PEC         (1u << 18)  /* port enable change        */
#define XHCI_PORTSC_WRC         (1u << 19)  /* warm reset change         */
#define XHCI_PORTSC_OCC         (1u << 20)  /* over-current change       */
#define XHCI_PORTSC_PRC         (1u << 21)  /* port reset change         */
#define XHCI_PORTSC_PLC         (1u << 22)  /* port link change          */
#define XHCI_PORTSC_CEC         (1u << 23)  /* config error change       */
#define XHCI_PORTSC_WPR         (1u << 31)  /* warm port reset           */
#define XHCI_PORTSC_RWC         (XHCI_PORTSC_CSC | XHCI_PORTSC_PEC | \
                                 XHCI_PORTSC_WRC | XHCI_PORTSC_OCC | \
                                 XHCI_PORTSC_PRC | XHCI_PORTSC_PLC | \
                                 XHCI_PORTSC_CEC)

/* Port link states (PLS field) */
#define XHCI_PLS_U0             0u          /* enabled                   */
#define XHCI_PLS_U3             3u          /* suspended                 */
#define XHCI_PLS_DISABLED       4u
#define XHCI_PLS_RX_DETECT      5u
#define XHCI_PLS_INACTIVE       6u
#define XHCI_PLS_POLLING        7u
#define XHCI_PLS_RECOVERY       8u
#define XHCI_PLS_COMPLIANCE     10u

/* Port speed field values */
#define XHCI_SPEED_FULL         1u
#define XHCI_SPEED_LOW          2u
#define XHCI_SPEED_HIGH         3u
#define XHCI_SPEED_SUPER        4u
#define XHCI_SPEED_SUPER_PLUS   5u

/* ── TRBs ─────────────────────────────────────────────────────── */

#define XHCI_TRB_CYCLE          (1u << 0)   /* cycle bit                 */
#define XHCI_TRB_ENT            (1u << 1)   /* evaluate next TRB         */
#define XHCI_TRB_ISP            (1u << 2)   /* interrupt on short packet */
#define XHCI_TRB_NS             (1u << 3)   /* no snoop                  */
#define XHCI_TRB_CH             (1u << 4)   /* chain bit                 */
#define XHCI_TRB_IOC            (1u << 5)   /* interrupt on completion   */
#define XHCI_TRB_IDT            (1u << 6)   /* immediate data            */
#define XHCI_TRB_TYPE_SHIFT     10u
#define XHCI_TRB_TYPE_MASK      (0x3Fu << XHCI_TRB_TYPE_SHIFT)
#define XHCI_TRB_TYPE(n)        (((uint32_t)(n) & 0x3Fu) << XHCI_TRB_TYPE_SHIFT)
#define XHCI_TRB_GET_TYPE(c)    (((c) & XHCI_TRB_TYPE_MASK) >> XHCI_TRB_TYPE_SHIFT)
#define XHCI_TRB_EP_ID(n)       (((uint32_t)(n) & 0xFFu) << 16)
#define XHCI_TRB_GET_EP_ID(c)   (((c) >> 16) & 0xFFu)
#define XHCI_TRB_SLOT_ID(n)     (((uint32_t)(n) & 0xFFu) << 24)
#define XHCI_TRB_GET_SLOT_ID(c) (((c) >> 24) & 0xFFu)
#define XHCI_TRB_INTR_TARGET(n) (((uint32_t)(n) & 0xFFu) << 16)
#define XHCI_TRB_LINK_TC        (1u << 1)   /* link TRB toggle cycle     */

/* TRB type codes (xHCI 1.1 table 6-91). */
typedef enum {
    XHCI_TRB_NORMAL              = 1,
    XHCI_TRB_SETUP_STAGE         = 2,
    XHCI_TRB_DATA_STAGE          = 3,
    XHCI_TRB_STATUS_STAGE        = 4,
    XHCI_TRB_ISOCH               = 5,
    XHCI_TRB_LINK                = 6,
    XHCI_TRB_EVENT_DATA          = 7,
    XHCI_TRB_NO_OP               = 8,
    XHCI_TRB_ENABLE_SLOT         = 9,
    XHCI_TRB_DISABLE_SLOT        = 10,
    XHCI_TRB_ADDRESS_DEVICE      = 11,
    XHCI_TRB_CONFIGURE_ENDPOINT  = 12,
    XHCI_TRB_EVALUATE_CONTEXT    = 13,
    XHCI_TRB_RESET_ENDPOINT      = 14,
    XHCI_TRB_STOP_ENDPOINT       = 15,
    XHCI_TRB_SET_TR_DEQUEUE      = 16,
    XHCI_TRB_RESET_DEVICE        = 17,
    XHCI_TRB_FORCE_EVENT         = 18,
    XHCI_TRB_NEGOTIATE_BW        = 19,
    XHCI_TRB_SET_LTV             = 20,
    XHCI_TRB_GET_PORT_BW         = 21,
    XHCI_TRB_FORCE_HEADER        = 22,
    XHCI_TRB_NO_OP_CMD           = 23,
    XHCI_TRB_TRANSFER_EVENT      = 32,
    XHCI_TRB_CMD_COMPLETION      = 33,
    XHCI_TRB_PORT_STATUS_CHANGE  = 34,
    XHCI_TRB_BANDWIDTH_REQUEST   = 35,
    XHCI_TRB_DOORBELL_EVENT      = 36,
    XHCI_TRB_HOST_CONTROLLER_EVENT = 37,
    XHCI_TRB_DEVICE_NOTIFICATION = 38,
    XHCI_TRB_MFINDEX_WRAP        = 39
} xhci_trb_type_t;

/* Command completion codes (xHCI 1.1 table 6-90). */
typedef enum {
    XHCI_CC_SUCCESS                  = 1,
    XHCI_CC_DATA_BUFFER_ERROR        = 2,
    XHCI_CC_BABBLE_DETECTED          = 3,
    XHCI_CC_USB_TRANSACTION_ERROR    = 4,
    XHCI_CC_TRB_ERROR                = 5,
    XHCI_CC_STALL_ERROR              = 6,
    XHCI_CC_RESOURCE_ERROR           = 7,
    XHCI_CC_BANDWIDTH_ERROR          = 8,
    XHCI_CC_NO_SLOTS_AVAILABLE       = 9,
    XHCI_CC_INVALID_STREAM_TYPE      = 10,
    XHCI_CC_SLOT_NOT_ENABLED         = 11,
    XHCI_CC_ENDPOINT_NOT_ENABLED     = 12,
    XHCI_CC_SHORT_PACKET             = 13,
    XHCI_CC_RING_UNDERRUN            = 14,
    XHCI_CC_RING_OVERRUN             = 15,
    XHCI_CC_VF_EVENT_RING_FULL       = 16,
    XHCI_CC_PARAMETER_ERROR          = 17,
    XHCI_CC_BANDWIDTH_OVERRUN        = 18,
    XHCI_CC_CONTEXT_STATE_ERROR      = 19,
    XHCI_CC_NO_PING_RESPONSE         = 20,
    XHCI_CC_EVENT_RING_FULL          = 21,
    XHCI_CC_INCOMPATIBLE_DEVICE      = 22,
    XHCI_CC_MISSED_SERVICE           = 23,
    XHCI_CC_COMMAND_RING_STOPPED     = 24,
    XHCI_CC_COMMAND_ABORTED          = 25,
    XHCI_CC_STOPPED                  = 26,
    XHCI_CC_STOPPED_LENGTH_INVALID   = 27,
    XHCI_CC_STOPPED_SHORT_PACKET     = 28,
    XHCI_CC_MAX_EXIT_LATENCY_ERROR   = 29,
    XHCI_CC_ISOCH_BUFFER_OVERRUN     = 31,
    XHCI_CC_EVENT_LOST               = 32,
    XHCI_CC_UNDEFINED_ERROR          = 33,
    XHCI_CC_INVALID_STREAM_ID        = 34,
    XHCI_CC_SECONDARY_BANDWIDTH      = 35,
    XHCI_CC_SPLIT_TRANSACTION        = 36
} xhci_completion_code_t;

/* A single 16-byte Transfer Request Block. */
typedef struct {
    uint32_t param_lo;      /* parameter / buffer address low           */
    uint32_t param_hi;      /* parameter / buffer address high          */
    uint32_t status;        /* status / length / completion code        */
    uint32_t control;       /* cycle, flags, type, slot/endpoint ids    */
} xhci_trb_t;

/* Software producer ring over a caller-provided TRB array. The last TRB
 * of the array is reserved for the wrap-around Link TRB. */
typedef struct {
    xhci_trb_t *base;       /* TRB array supplied by the caller         */
    uint32_t    count;      /* total TRBs, including the link slot      */
    uint32_t    enqueue;    /* next producer index                      */
    uint8_t     cycle;      /* producer cycle state                     */
    uint8_t     slot;       /* doorbell slot (0 = command ring)         */
    uint8_t     target;     /* doorbell target (endpoint id)            */
    uint8_t     reserved;
} xhci_ring_t;

/* Slot context (32 bytes / 8 dwords, context size 0). */
typedef struct {
    uint32_t dword0;        /* route string, speed, mtt, hub, entries   */
    uint32_t dword1;        /* exit latency, root port, port count      */
    uint32_t dword2;        /* parent hub/port, tt think, interrupter   */
    uint32_t dword3;        /* device address, slot state               */
    uint32_t reserved[4];
} xhci_slot_ctx_t;

/* Slot context helpers. */
#define XHCI_SLOT_ROUTE(n)      ((uint32_t)(n) & 0xFFFFFu)
#define XHCI_SLOT_SPEED(n)      (((uint32_t)(n) & 0xFu) << 20)
#define XHCI_SLOT_MTT           (1u << 25)
#define XHCI_SLOT_HUB           (1u << 26)
#define XHCI_SLOT_ENTRIES(n)    (((uint32_t)(n) & 0x1Fu) << 27)
#define XHCI_SLOT_EXIT_LATENCY(n) ((uint32_t)(n) & 0xFFFFu)
#define XHCI_SLOT_ROOT_PORT(n)  (((uint32_t)(n) & 0xFFu) << 16)
#define XHCI_SLOT_NUM_PORTS(n)  (((uint32_t)(n) & 0xFFu) << 24)
#define XHCI_SLOT_PARENT_SLOT(n) ((uint32_t)(n) & 0xFFu)
#define XHCI_SLOT_PARENT_PORT(n) (((uint32_t)(n) & 0xFFu) << 8)
#define XHCI_SLOT_TT_THINK(n)   (((uint32_t)(n) & 0x3u) << 16)
#define XHCI_SLOT_INTR_TARGET(n) (((uint32_t)(n) & 0xFu) << 22)
#define XHCI_SLOT_DEV_ADDR(n)   ((uint32_t)(n) & 0xFFu)
#define XHCI_SLOT_STATE(n)      (((uint32_t)(n) & 0x1Fu) << 27)

/* Slot state encoding (slot context dword 3). */
typedef enum {
    XHCI_SLOT_DISABLED   = 0,
    XHCI_SLOT_DEFAULT    = 1,
    XHCI_SLOT_ADDRESSED  = 2,
    XHCI_SLOT_CONFIGURED = 3
} xhci_slot_state_t;

/* Decoded port status, filled by xhci_poll_port(). */
typedef struct {
    uint32_t raw;           /* raw PORTSC value                         */
    uint8_t  speed;         /* XHCI_SPEED_*                             */
    uint8_t  link_state;    /* XHCI_PLS_*                               */
    bool     connected;     /* CCS                                      */
    bool     enabled;       /* PED                                      */
    bool     resetting;     /* PR                                       */
    bool     over_current;  /* OCA                                      */
    bool     powered;       /* PP                                       */
} xhci_port_status_t;

/* Controller instance. Zero-initialised by the caller before init. */
typedef struct {
    volatile uint8_t  *cap_base;    /* MMIO capability base             */
    volatile uint32_t *op_base;     /* MMIO operational base            */
    volatile uint32_t *db_base;     /* doorbell array                   */
    volatile uint32_t *rt_base;     /* runtime register set             */
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hccparams1;
    uint16_t hciversion;
    uint8_t  caplength;
    uint8_t  max_slots;
    uint8_t  max_ports;
    uint16_t max_intrs;
    uint8_t  context_size;          /* 32 or 64 bytes                   */
    uint32_t db_offset;
    uint32_t rt_offset;
    bool     initialized;
    bool     running;
} xhci_ctrl_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Map the controller, reset it and program DCBAAP / CRCR / CONFIG. */
bool xhci_init(xhci_ctrl_t *hc, volatile uint8_t *mmio_base,
               uint64_t dcbaa_address, uint64_t command_ring_address);

/* Assert USBCMD.HCRST and wait for the controller to become ready. */
bool xhci_reset(xhci_ctrl_t *hc);

/* Sample one root port, acknowledge change bits and drive a reset. */
bool xhci_poll_port(xhci_ctrl_t *hc, uint8_t port, xhci_port_status_t *status_out);

/* Queue one command or transfer TRB and ring its doorbell. */
bool xhci_submit_trb(xhci_ctrl_t *hc, xhci_ring_t *ring, uint32_t trb_type,
                     uint64_t parameter, uint32_t status, uint32_t flags,
                     uint8_t slot_id, uint8_t endpoint_id, xhci_trb_t *trb_out);

/* Prepare a producer ring over a caller-owned TRB array. */
void xhci_ring_init(xhci_ring_t *ring, xhci_trb_t *base, uint32_t count,
                    uint8_t slot, uint8_t target);

/* Ring one doorbell register entry. */
void xhci_ring_doorbell(const xhci_ctrl_t *hc, uint8_t slot, uint8_t target);

/* Slot context helpers. */
void xhci_slot_ctx_init(xhci_slot_ctx_t *ctx, uint8_t speed, uint8_t entries);
void xhci_slot_ctx_set_address(xhci_slot_ctx_t *ctx, uint8_t address);
void xhci_slot_ctx_set_state(xhci_slot_ctx_t *ctx, uint8_t state);
void xhci_slot_ctx_set_root_port(xhci_slot_ctx_t *ctx, uint8_t port);
void xhci_slot_ctx_set_route(xhci_slot_ctx_t *ctx, uint32_t route);
bool xhci_trb_is_command(uint32_t trb_type);

#endif /* XHCI_H */
