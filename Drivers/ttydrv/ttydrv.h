/*
 * ttydrv.h - OpenWindows TTY Console Driver (.owc)
 *
 * Text-cell console driven by a caller-provided cell buffer
 * (rows*cols) with foreground/background colours per cell. Handles
 * CR/LF, wrapping and scrolling.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef TTYDRV_H
#define TTYDRV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Cell descriptor ──────────────────────────────────────────── */

/* One screen cell: ASCII glyph plus a 4-bit fg/bg colour pair. */
typedef struct {
    uint8_t ch;   /* ASCII character code      */
    uint8_t fg;   /* Foreground colour 0..15   */
    uint8_t bg;   /* Background colour 0..15   */
} tty_cell_t;

/* Default light-grey-on-black attribute. */
#define TTY_COLOR_DEFAULT 0x07u

/* ── API ──────────────────────────────────────────────────────── */

/* Bind a cell buffer and reset the cursor to the origin. */
bool tty_init(tty_cell_t *buffer, uint16_t rows, uint16_t cols,
              uint8_t fg, uint8_t bg);

/* Print one character, honouring '\n', '\r' and wrapping. */
void tty_write_char(char c);

/* Print a NUL-terminated string via tty_write_char(). */
void tty_write_string(const char *s);

/* Blank the whole buffer and home the cursor. */
void tty_clear(void);

/* Scroll all rows up by one and blank the last row. */
void tty_scroll(void);

/* ── Introspection helpers ────────────────────────────────────── */

uint16_t tty_get_rows(void);
uint16_t tty_get_cols(void);
uint16_t tty_get_cursor_row(void);
uint16_t tty_get_cursor_col(void);

#endif /* TTYDRV_H */
