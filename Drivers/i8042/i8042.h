/*
 * i8042.h - OpenWindows i8042 PS/2 Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Careful status-register handling for the legacy i8042 keyboard/mouse
 * controller (ports 0x60 data and 0x64 status/command).
 */

#ifndef I8042_H
#define I8042_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Ports ────────────────────────────────────────────────────── */

#define I8042_DATA_PORT         0x60u   /* read data / write data       */
#define I8042_STATUS_PORT       0x64u   /* read status / write command  */
#define I8042_CMD_PORT          0x64u
#define I8042_PULSE_PORT        0x80u   /* write-only delay port        */

/* ── Status register bits ─────────────────────────────────────── */

#define I8042_STATUS_OBF        (1u << 0)   /* output buffer full       */
#define I8042_STATUS_IBF        (1u << 1)   /* input buffer full        */
#define I8042_STATUS_SYS        (1u << 2)   /* system (POST) flag       */
#define I8042_STATUS_A2         (1u << 3)   /* data came from command   */
#define I8042_STATUS_INH        (1u << 4)   /* keyboard inhibit         */
#define I8042_STATUS_AUXB       (1u << 5)   /* aux (mouse) output       */
#define I8042_STATUS_TIM        (1u << 6)   /* timeout error            */
#define I8042_STATUS_PAR        (1u << 7)   /* parity error             */

/* ── Controller commands ──────────────────────────────────────── */

#define I8042_CMD_READ_CONFIG   0x20u   /* read configuration byte      */
#define I8042_CMD_WRITE_CONFIG  0x60u   /* write configuration byte     */
#define I8042_CMD_DISABLE_AUX   0xA7u   /* disable second (mouse) port  */
#define I8042_CMD_ENABLE_AUX    0xA8u   /* enable second (mouse) port   */
#define I8042_CMD_TEST_AUX      0xA9u   /* test second port             */
#define I8042_CMD_SELF_TEST     0xAAu   /* controller self test         */
#define I8042_CMD_TEST_KBD      0xABu   /* test first (keyboard) port   */
#define I8042_CMD_DISABLE_KBD   0xADu   /* disable first port           */
#define I8042_CMD_ENABLE_KBD    0xAEu   /* enable first port            */
#define I8042_CMD_WRITE_AUX     0xD4u   /* send next byte to aux device */
#define I8042_CMD_READ_OUTPUT   0xD0u   /* read output port             */
#define I8042_CMD_WRITE_OUTPUT  0xD1u   /* write output port            */

/* ── Configuration byte bits ──────────────────────────────────── */

#define I8042_CFG_KBD_INT       0x01u   /* keyboard interrupt enable    */
#define I8042_CFG_AUX_INT       0x02u   /* mouse interrupt enable       */
#define I8042_CFG_SYS_FLAG      0x04u   /* system flag                  */
#define I8042_CFG_KBD_CLK       0x10u   /* keyboard clock disable       */
#define I8042_CFG_AUX_CLK       0x20u   /* mouse clock disable          */
#define I8042_CFG_KBD_XLAT      0x40u   /* scancode translation enable  */

/* ── Results ──────────────────────────────────────────────────── */

#define I8042_SELF_TEST_OK      0x55u   /* 0xAA self-test passed        */
#define I8042_PORT_TEST_OK      0x00u   /* 0xAB/0xA9 port test passed   */
#define I8042_INTERNAL_BUFFER   0x99u   /* controller detection pattern */
#define I8042_TIMEOUT           100000u /* bounded poll budget          */

/* Controller state shared with the caller. */
typedef struct {
    uint8_t config;         /* cached configuration byte                */
    uint8_t self_test;      /* result byte of the 0xAA self test        */
    uint8_t kbd_test;       /* result byte of the keyboard port test    */
    uint8_t aux_test;       /* result byte of the auxiliary port test   */
    bool    initialized;    /* controller is up                         */
} i8042_state_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Disable both ports, flush, self-test and re-enable the controller. */
bool i8042_init(i8042_state_t *state);

/* Write one command byte to port 0x64, waiting for IBF to clear. */
bool i8042_send_cmd(uint8_t command);

/* Write one data byte to port 0x60, waiting for IBF to clear. */
bool i8042_send_data(uint8_t value);

/* Read one data byte from port 0x60, waiting for OBF to rise. */
bool i8042_read_data(uint8_t *out);

/* Drain any pending controller/device bytes. */
void i8042_flush(void);

/* Read the status register (port 0x64). */
uint8_t i8042_read_status(void);

/* Wait until the input buffer is free (IBF == 0). */
bool i8042_wait_input_clear(void);

/* Wait until a byte is available (OBF == 1). */
bool i8042_wait_output_full(void);

/* Run the 0xAA controller self test; stores the raw result in *result. */
bool i8042_self_test(uint8_t *result);

/* Read and write the controller configuration byte. */
bool i8042_read_config(uint8_t *config);
bool i8042_write_config(uint8_t config);

#endif /* I8042_H */
