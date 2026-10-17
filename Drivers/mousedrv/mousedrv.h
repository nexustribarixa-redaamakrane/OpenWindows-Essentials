/*
 * mousedrv.h - OpenWindows Mouse Input Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * PS/2 auxiliary device driver layered on the i8042 controller. Packets
 * are three bytes: a flag byte followed by signed X and Y deltas. The
 * driver validates the sync bit, decodes the deltas and accumulates an
 * absolute cursor position.
 */

#ifndef MOUSEDRV_H
#define MOUSEDRV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── i8042 ports ──────────────────────────────────────────────── */

#define MOUSE_DATA_PORT         0x60u
#define MOUSE_STATUS_PORT       0x64u
#define MOUSE_CMD_PORT          0x64u
#define MOUSE_PULSE_PORT        0x80u

/* Status bits */
#define MOUSE_STATUS_OBF        (1u << 0)
#define MOUSE_STATUS_IBF        (1u << 1)
#define MOUSE_STATUS_AUXB       (1u << 5)

/* Controller commands */
#define MOUSE_CMD_ENABLE_AUX    0xA8u
#define MOUSE_CMD_READ_CONFIG   0x20u
#define MOUSE_CMD_WRITE_CONFIG  0x60u
#define MOUSE_CMD_WRITE_AUX     0xD4u

/* Configuration byte clock bits (active low) */
#define MOUSE_CFG_AUX_INT       0x02u
#define MOUSE_CFG_AUX_CLK       0x20u

/* ── PS/2 mouse device commands ───────────────────────────────── */

#define MOUSE_DEV_RESET         0xFFu   /* reset and self test          */
#define MOUSE_DEV_SET_DEFAULTS  0xF6u   /* sample rate 100, res 4/mm    */
#define MOUSE_DEV_DISABLE       0xF5u   /* disable data reporting       */
#define MOUSE_DEV_ENABLE        0xF4u   /* enable data reporting        */
#define MOUSE_DEV_SET_RATE      0xF3u   /* next byte is sample rate     */
#define MOUSE_DEV_GET_ID        0xF2u   /* read device id               */
#define MOUSE_DEV_SET_RES       0xE8u   /* next byte is resolution      */
#define MOUSE_DEV_STATUS_REQ    0xE9u   /* request 3-byte status        */
#define MOUSE_DEV_SET_SCALING   0xE6u   /* 1:1 scaling                  */

/* Device replies */
#define MOUSE_ACK               0xFAu
#define MOUSE_RESEND            0xFEu
#define MOUSE_SELF_TEST_OK      0xAAu
#define MOUSE_ERROR             0xFCu

#define MOUSE_DEFAULT_RATE      100u
#define MOUSE_DEFAULT_RES       2u      /* 4 counts/mm                  */
#define MOUSE_PACKET_SIZE       3u
#define MOUSE_PACKET_SYNC       0x08u   /* bit 3 is always one          */
#define MOUSE_TIMEOUT           100000u

/* Decoded button state. */
typedef struct {
    int32_t x;              /* absolute cursor x                        */
    int32_t y;              /* absolute cursor y                        */
    int32_t dx;             /* last packet x delta                      */
    int32_t dy;             /* last packet y delta                      */
    uint8_t buttons;        /* raw button bits 0..2                     */
    bool    left;           /* left button                              */
    bool    right;          /* right button                             */
    bool    middle;         /* middle button                            */
} mouse_report_t;

/* Raw three byte packet plus assembly cursor. */
typedef struct {
    uint8_t bytes[MOUSE_PACKET_SIZE];
    uint8_t index;          /* bytes collected so far                   */
} mouse_packet_t;

/* Driver state, owned by the caller. */
typedef struct {
    int32_t x;              /* absolute cursor x                        */
    int32_t y;              /* absolute cursor y                        */
    int32_t last_dx;        /* x delta of the last packet               */
    int32_t last_dy;        /* y delta of the last packet               */
    uint8_t buttons;        /* latest button bits                       */
    uint8_t device_id;      /* device id from 0xF2                      */
    uint8_t sample_rate;    /* current sample rate                      */
    uint8_t resolution;     /* current resolution                       */
    mouse_packet_t packet;  /* packet under construction                */
    bool    initialized;    /* device ready                             */
} mouse_state_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Bring up the auxiliary port and configure default mouse behaviour. */
bool mouse_init(mouse_state_t *state);

/* Drain any pending packets and update the accumulated cursor. */
bool mouse_poll(mouse_state_t *state);

/* Copy the current decoded cursor/button state into report. */
bool mouse_get_state(const mouse_state_t *state, mouse_report_t *report);

/* Program the device sample rate (10, 20, 40, 60, 80, 100 or 200 Hz). */
bool mouse_set_rate(mouse_state_t *state, uint8_t rate);

/* Read and validate exactly one three byte packet. */
bool mouse_read_packet(mouse_state_t *state, mouse_packet_t *packet);

#endif /* MOUSEDRV_H */
