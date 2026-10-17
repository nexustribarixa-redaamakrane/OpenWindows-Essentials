/*
 * rtl8139.h - OpenWindows Realtek RTL8139 NIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t rtl8139_status_t;

#define RTL8139_OK            0x00000000u  /* Operation completed      */
#define RTL8139_ERR_PARAM     0x00000001u  /* Invalid argument         */
#define RTL8139_ERR_NOT_READY 0x00000002u  /* Device not initialised   */
#define RTL8139_ERR_TIMEOUT   0x00000003u  /* Hardware did not answer  */
#define RTL8139_ERR_NO_SPACE  0x00000004u  /* TX slot busy / short buf */
#define RTL8139_ERR_IO        0x00000005u  /* Malformed RX frame       */

/* ── Register offsets (I/O BAR) ───────────────────────────────── */

#define RTL_IDR0      0x00u   /* MAC bytes 0..5                        */
#define RTL_TSD0      0x10u   /* Transmit status, 4 slots              */
#define RTL_TSAD0     0x20u   /* Transmit start address, 4 slots       */
#define RTL_RBSTART   0x30u   /* Receive buffer start address          */
#define RTL_CR        0x37u   /* Command register                      */
#define RTL_CAPR      0x38u   /* Current address of packet read        */
#define RTL_CBR       0x3Au   /* Current buffer address (written)      */
#define RTL_IMR       0x3Cu   /* Interrupt mask                        */
#define RTL_ISR       0x3Eu   /* Interrupt status                      */
#define RTL_TCR       0x40u   /* Transmit configuration                */
#define RTL_RCR       0x44u   /* Receive configuration                 */
#define RTL_9346CR    0x50u   /* EEPROM / config register              */
#define RTL_CONFIG1   0x52u   /* Configuration 1                       */

/* ── Command register bits ────────────────────────────────────── */

#define RTL_CR_RST    0x10u   /* Software reset (self-clearing)        */
#define RTL_CR_RE     0x08u   /* Receiver enable                       */
#define RTL_CR_TE     0x04u   /* Transmitter enable                    */
#define RTL_CR_BUFE   0x01u   /* Receive buffer empty                  */

/* ── Transmit status descriptor bits ──────────────────────────── */

#define RTL_TSD_SIZE_MASK 0x00001FFFu  /* Transfer size                */
#define RTL_TSD_OWN       0x00002000u  /* Owned by controller          */
#define RTL_TSD_TOK       0x00008000u  /* Transmit OK                  */

/* ── Receive status word bits ─────────────────────────────────── */

#define RTL_RSR_ROK   0x0001u  /* Receive OK                           */
#define RTL_RSR_FAE   0x0002u  /* Frame alignment error                */
#define RTL_RSR_CRC   0x0004u  /* CRC error                            */
#define RTL_RSR_LONG  0x0008u  /* Long frame                           */
#define RTL_RSR_RUNT  0x0010u  /* Runt frame                           */
#define RTL_RSR_ISE   0x0020u  /* Invalid symbol error                 */

/* ── Receive / transmit configuration bits ────────────────────── */

#define RTL_RCR_AAP   0x00000001u  /* Accept all packets               */
#define RTL_RCR_APM   0x00000002u  /* Accept physical match            */
#define RTL_RCR_AM    0x00000004u  /* Accept multicast                 */
#define RTL_RCR_AB    0x00000008u  /* Accept broadcast                 */
#define RTL_RCR_WRAP  0x00000080u  /* Wrap at end of RX buffer         */
#define RTL_RCR_MXDMA 0x00000700u  /* Max DMA burst (unlimited)        */
#define RTL_TCR_MXDMA 0x00000700u  /* Max DMA burst (unlimited)        */

/* ── Capacity limits (static storage) ─────────────────────────── */

#define RTL8139_TX_SLOTS   4u       /* Hardware TX descriptor slots  */
#define RTL8139_FRAME_MAX  2048u    /* Per-slot data buffer          */
#define RTL8139_RX_MIN     8192u    /* Smallest sane RX ring         */

/* ── Driver instance (caller-provided, zero heap) ─────────────── */

typedef struct {
    uint16_t io_base;                          /* I/O BAR base        */
    uint8_t  mac[6];                           /* Station address     */
    uint8_t  tx_slot;                          /* Next TX slot        */
    uint32_t rx_offset;                        /* Software CAPR       */
    bool     initialized;
    uint8_t  reserved[3];
    uint8_t  tx_buf[RTL8139_TX_SLOTS][RTL8139_FRAME_MAX];
    uint8_t *rx_buf;                           /* Caller-provided ring */
    uint32_t rx_buf_len;                       /* Ring size in bytes  */
} rtl8139_dev_t;

/* ── API ──────────────────────────────────────────────────────── */

rtl8139_status_t rtl8139_init(rtl8139_dev_t *dev, uint16_t io_base,
                              uint8_t *rx_buffer, uint32_t rx_len);
rtl8139_status_t rtl8139_send(rtl8139_dev_t *dev, const void *frame,
                              uint32_t len);
rtl8139_status_t rtl8139_recv(rtl8139_dev_t *dev, void *buf, uint32_t cap,
                              uint32_t *out_len);
rtl8139_status_t rtl8139_reset(rtl8139_dev_t *dev);
rtl8139_status_t rtl8139_get_mac(const rtl8139_dev_t *dev,
                                 uint8_t out_mac[6]);

#endif /* RTL8139_H */
