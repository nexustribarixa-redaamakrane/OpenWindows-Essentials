/*
 * e1000.h - OpenWindows Intel e1000 NIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef E1000_H
#define E1000_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t e1000_status_t;

#define E1000_OK            0x00000000u  /* Operation completed        */
#define E1000_ERR_PARAM     0x00000001u  /* Invalid argument           */
#define E1000_ERR_NOT_READY 0x00000002u  /* Device not initialised     */
#define E1000_ERR_TIMEOUT   0x00000003u  /* Hardware did not respond   */
#define E1000_ERR_NO_SPACE  0x00000004u  /* Ring full / buffer too small */
#define E1000_ERR_IO        0x00000005u  /* MMIO / descriptor fault    */

/* ── Register offsets (MMIO BAR0) ─────────────────────────────── */

#define E1000_REG_CTRL    0x0000u   /* Device control               */
#define E1000_REG_STATUS  0x0008u   /* Device status                */
#define E1000_REG_ICR     0x00C0u   /* Interrupt cause read         */
#define E1000_REG_IMS     0x00D0u   /* Interrupt mask set           */
#define E1000_REG_IMC     0x00D8u   /* Interrupt mask clear         */
#define E1000_REG_RCTL    0x0100u   /* Receive control              */
#define E1000_REG_TCTL    0x0400u   /* Transmit control             */
#define E1000_REG_TIPG    0x0410u   /* Transmit inter-packet gap    */
#define E1000_REG_RDBAL   0x2800u   /* RX descriptor base low       */
#define E1000_REG_RDBAH   0x2804u   /* RX descriptor base high      */
#define E1000_REG_RDLEN   0x2808u   /* RX ring length (bytes)       */
#define E1000_REG_RDH     0x2810u   /* RX descriptor head           */
#define E1000_REG_RDT     0x2818u   /* RX descriptor tail           */
#define E1000_REG_TDBAL   0x3800u   /* TX descriptor base low       */
#define E1000_REG_TDBAH   0x3804u   /* TX descriptor base high      */
#define E1000_REG_TDLEN   0x3808u   /* TX ring length (bytes)       */
#define E1000_REG_TDH     0x3810u   /* TX descriptor head           */
#define E1000_REG_TDT     0x3818u   /* TX descriptor tail           */
#define E1000_REG_MTA     0x5200u   /* Multicast table array        */
#define E1000_REG_RAL     0x5400u   /* Receive address low          */
#define E1000_REG_RAH     0x5404u   /* Receive address high         */

/* ── CTRL bits ────────────────────────────────────────────────── */

#define E1000_CTRL_SLU    0x00000040u  /* Set link up                */
#define E1000_CTRL_RST    0x04000000u  /* Device reset (self-clearing) */

/* ── STATUS bits ──────────────────────────────────────────────── */

#define E1000_STATUS_LU   0x00000002u  /* Link up                    */

/* ── TCTL bits ────────────────────────────────────────────────── */

#define E1000_TCTL_EN     0x00000002u  /* Transmit enable            */
#define E1000_TCTL_PSP    0x00000008u  /* Pad short packets          */
#define E1000_TCTL_CT     0x000000F0u  /* Collision threshold (0xF)  */
#define E1000_TCTL_COLD   0x00040000u  /* Collision distance (0x40)  */

/* ── RCTL bits ────────────────────────────────────────────────── */

#define E1000_RCTL_EN     0x00000002u  /* Receiver enable            */
#define E1000_RCTL_UPE    0x00000008u  /* Unicast promiscuous        */
#define E1000_RCTL_MPE    0x00000010u  /* Multicast promiscuous      */
#define E1000_RCTL_BAM    0x00008000u  /* Broadcast accept           */
#define E1000_RCTL_SECRC  0x04000000u  /* Strip Ethernet CRC         */

/* ── TX descriptor command / status bits ──────────────────────── */

#define E1000_TXD_CMD_EOP  0x01u       /* End of packet              */
#define E1000_TXD_CMD_IFCS 0x02u       /* Insert FCS (CRC)           */
#define E1000_TXD_CMD_RS   0x08u       /* Report status              */
#define E1000_TXD_STAT_DD  0x01u       /* Descriptor done            */

/* ── RX descriptor status bits ────────────────────────────────── */

#define E1000_RXD_STAT_DD  0x01u       /* Descriptor done            */
#define E1000_RXD_STAT_EOP 0x02u       /* End of packet              */

/* ── Capacity limits (static storage) ─────────────────────────── */

#define E1000_TX_RING_SIZE 8u          /* TX descriptors             */
#define E1000_RX_RING_SIZE 8u          /* RX descriptors             */
#define E1000_FRAME_MAX    2048u       /* Per-descriptor data buffer */

/* ── Legacy (non-advanced) descriptor layouts ─────────────────── */

typedef struct {
    uint64_t addr;      /* Buffer physical address                  */
    uint16_t length;    /* Data length                              */
    uint8_t  cso;       /* Checksum offset                          */
    uint8_t  cmd;       /* Command bits                             */
    uint8_t  status;    /* Status bits (DD/EC)                      */
    uint8_t  css;       /* Checksum start                           */
    uint16_t special;   /* VLAN / special                           */
} e1000_tx_desc_t;

typedef struct {
    uint64_t addr;      /* Buffer physical address                  */
    uint16_t length;    /* Received length                          */
    uint16_t checksum;  /* Packet checksum                          */
    uint8_t  status;    /* Status bits (DD/EOP)                     */
    uint8_t  errors;    /* Error bits                               */
    uint16_t special;   /* VLAN / special                           */
} e1000_rx_desc_t;

/* ── Driver instance (caller-provided, zero heap) ─────────────── */

typedef struct {
    volatile uint8_t *mmio;                       /* BAR0 mapping     */
    uint8_t  mac[6];                              /* Station address  */
    uint8_t  reserved[2];
    uint32_t tx_tail;                             /* Next TDT index   */
    uint32_t rx_tail;                             /* Next RDT index   */
    bool     initialized;
    e1000_tx_desc_t tx_ring[E1000_TX_RING_SIZE];
    e1000_rx_desc_t rx_ring[E1000_RX_RING_SIZE];
    uint8_t  tx_buf[E1000_TX_RING_SIZE][E1000_FRAME_MAX];
    uint8_t  rx_buf[E1000_RX_RING_SIZE][E1000_FRAME_MAX];
} e1000_dev_t;

/* ── API ──────────────────────────────────────────────────────── */

e1000_status_t e1000_init(e1000_dev_t *dev, volatile uint8_t *mmio);
e1000_status_t e1000_send(e1000_dev_t *dev, const void *frame, uint32_t len);
e1000_status_t e1000_recv(e1000_dev_t *dev, void *buf, uint32_t cap,
                          uint32_t *out_len);
e1000_status_t e1000_get_mac(const e1000_dev_t *dev, uint8_t out_mac[6]);

#endif /* E1000_H */
