/*
 * owedit.c - OpenWindows Zero-Allocation Text Editor (.owx)
 *
 * Full-screen lightweight text editor supporting SUTF-8 & ASCII text buffers.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWEDIT_MAX_ROWS     256u
#define OWEDIT_MAX_COLS     128u

typedef struct {
    char     lines[OWEDIT_MAX_ROWS][OWEDIT_MAX_COLS];
    uint32_t row_count;
    uint32_t cursor_row;
    uint32_t cursor_col;
    bool     is_modified;
} owedit_buffer_t;

void owedit_init(owedit_buffer_t *buf)
{
    if (!buf) return;
    buf->row_count = 1u;
    buf->cursor_row = 0u;
    buf->cursor_col = 0u;
    buf->is_modified = false;
    buf->lines[0][0] = '\0';
}

bool owedit_insert_char(owedit_buffer_t *buf, char ch)
{
    if (!buf || buf->cursor_row >= OWEDIT_MAX_ROWS) return false;

    uint32_t r = buf->cursor_row;
    uint32_t c = buf->cursor_col;
    if (c >= OWEDIT_MAX_COLS - 1) return false;

    buf->lines[r][c] = ch;
    buf->lines[r][c + 1] = '\0';
    buf->cursor_col++;
    buf->is_modified = true;
    return true;
}

bool owedit_newline(owedit_buffer_t *buf)
{
    if (!buf || buf->row_count >= OWEDIT_MAX_ROWS) return false;

    buf->row_count++;
    buf->cursor_row++;
    buf->cursor_col = 0u;
    buf->lines[buf->cursor_row][0] = '\0';
    buf->is_modified = true;
    return true;
}
