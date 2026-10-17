/*
 * ttydrv.c - OpenWindows TTY Console Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ttydrv.h"

/* ── Internal state ───────────────────────────────────────────── */

static tty_cell_t *ttydrv_cells  = NULL;
static uint16_t    ttydrv_rows   = 0u;
static uint16_t    ttydrv_cols   = 0u;
static uint8_t     ttydrv_fg     = TTY_COLOR_DEFAULT;
static uint8_t     ttydrv_bg     = 0u;
static uint16_t    ttydrv_cur_row = 0u;
static uint16_t    ttydrv_cur_col = 0u;
static bool        ttydrv_initialized = false;

/* ── Helpers ──────────────────────────────────────────────────── */

/* Forward byte copy that tolerates overlapping regions (scroll). */
static void ow_memmove(void *dst, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || len == 0u) {
        return;
    }
    if (d < s) {
        size_t i;
        for (i = 0u; i < len; ++i) {
            d[i] = s[i];
        }
    } else {
        size_t i = len;
        while (i > 0u) {
            i--;
            d[i] = s[i];
        }
    }
}

/* Cell pointer for one row, or NULL when out of range. */
static tty_cell_t *ttydrv_row(uint16_t row)
{
    if (row >= ttydrv_rows) {
        return NULL;
    }
    return &ttydrv_cells[(size_t)row * ttydrv_cols];
}

/* Blank a single row using the current colours. */
static void ttydrv_blank_row(uint16_t row)
{
    tty_cell_t *cells = ttydrv_row(row);
    uint16_t col;

    if (cells == NULL) {
        return;
    }
    for (col = 0u; col < ttydrv_cols; ++col) {
        cells[col].ch = (uint8_t)' ';
        cells[col].fg = ttydrv_fg;
        cells[col].bg = ttydrv_bg;
    }
}

/* Place a glyph at the current cursor and advance it. */
static void ttydrv_put_glyph(uint8_t ch)
{
    tty_cell_t *row;

    if (ttydrv_cur_col >= ttydrv_cols) {
        ttydrv_cur_col = 0u;
        ttydrv_cur_row++;
    }
    if (ttydrv_cur_row >= ttydrv_rows) {
        ttydrv_cur_row = (uint16_t)(ttydrv_rows - 1u);
        tty_scroll();
    }

    row = ttydrv_row(ttydrv_cur_row);
    if (row == NULL) {
        return;
    }
    row[ttydrv_cur_col].ch = ch;
    row[ttydrv_cur_col].fg = ttydrv_fg;
    row[ttydrv_cur_col].bg = ttydrv_bg;
    ttydrv_cur_col++;
}

/* ── Implementation ───────────────────────────────────────────── */

bool tty_init(tty_cell_t *buffer, uint16_t rows, uint16_t cols,
              uint8_t fg, uint8_t bg)
{
    if (buffer == NULL || rows == 0u || cols == 0u) {
        return false;
    }

    ttydrv_cells = buffer;
    ttydrv_rows  = rows;
    ttydrv_cols  = cols;
    ttydrv_fg    = (uint8_t)(fg & 0x0Fu);
    ttydrv_bg    = (uint8_t)(bg & 0x0Fu);
    ttydrv_cur_row = 0u;
    ttydrv_cur_col = 0u;
    ttydrv_initialized = true;
    tty_clear();
    return true;
}

void tty_write_char(char c)
{
    if (!ttydrv_initialized) {
        return;
    }

    switch (c) {
    case '\n':
        ttydrv_cur_col = 0u;
        ttydrv_cur_row++;
        if (ttydrv_cur_row >= ttydrv_rows) {
            ttydrv_cur_row = (uint16_t)(ttydrv_rows - 1u);
            tty_scroll();
        }
        break;
    case '\r':
        ttydrv_cur_col = 0u;
        break;
    case '\b':
        if (ttydrv_cur_col > 0u) {
            ttydrv_cur_col--;
        } else if (ttydrv_cur_row > 0u) {
            ttydrv_cur_row--;
            ttydrv_cur_col = (uint16_t)(ttydrv_cols - 1u);
        }
        break;
    case '\t': {
        uint16_t next = (uint16_t)((ttydrv_cur_col + 8u) & (uint16_t)~7u);
        while (ttydrv_cur_col < next) {
            ttydrv_put_glyph((uint8_t)' ');
        }
        break;
    }
    default:
        if ((uint8_t)c < 0x20u) {
            /* Ignore other non-printable control codes. */
            break;
        }
        ttydrv_put_glyph((uint8_t)c);
        break;
    }
}

void tty_write_string(const char *s)
{
    if (!ttydrv_initialized || s == NULL) {
        return;
    }
    while (*s != '\0') {
        tty_write_char(*s);
        s++;
    }
}

void tty_clear(void)
{
    uint16_t row;

    if (!ttydrv_initialized) {
        return;
    }
    for (row = 0u; row < ttydrv_rows; ++row) {
        ttydrv_blank_row(row);
    }
    ttydrv_cur_row = 0u;
    ttydrv_cur_col = 0u;
}

void tty_scroll(void)
{
    size_t row_bytes;
    uint16_t row;

    if (!ttydrv_initialized || ttydrv_rows < 2u) {
        /* A single row simply gets blanked. */
        if (ttydrv_initialized) {
            ttydrv_blank_row(0u);
        }
        return;
    }

    row_bytes = (size_t)ttydrv_cols * sizeof(tty_cell_t);
    for (row = 0u; row + 1u < ttydrv_rows; ++row) {
        tty_cell_t *dst = ttydrv_row(row);
        tty_cell_t *src = ttydrv_row((uint16_t)(row + 1u));
        if (dst != NULL && src != NULL) {
            ow_memmove(dst, src, row_bytes);
        }
    }
    ttydrv_blank_row((uint16_t)(ttydrv_rows - 1u));
}

uint16_t tty_get_rows(void)
{
    return ttydrv_rows;
}

uint16_t tty_get_cols(void)
{
    return ttydrv_cols;
}

uint16_t tty_get_cursor_row(void)
{
    return ttydrv_cur_row;
}

uint16_t tty_get_cursor_col(void)
{
    return ttydrv_cur_col;
}
