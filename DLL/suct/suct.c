/*
 * suct.c - SuperUnicode Text Services implementation (.owd)
 *
 * C99 freestanding. No heap; planes and consoles are caller-owned.
 */

#include "suct.h"
#include "owrp.h"

/* ================================================================== */
/*  Scratch Row Buffer                                                */
/* ================================================================== */

#define SUCT_ROW_BUF_CAP (SUCT_ROW_BYTES_MAX + 1u)  /* + trailing LF */

/* ================================================================== */
/*  Cell Width Ranges                                                 */
/* ================================================================== */

typedef struct {
    sucs_char_t lo;
    sucs_char_t hi;
} suct_range_t;

/* Zero-width: combining marks, joins, variation selectors. */
static const suct_range_t zero_width_ranges[] = {
    { 0x0300u, 0x036Fu },
    { 0x200Du, 0x200Du },
    { 0xFE00u, 0xFE0Fu },
    { 0xE0100u, 0xE01EFu },
};

/* Double-width: East Asian Wide / Fullwidth classes. */
static const suct_range_t wide_ranges[] = {
    { 0x1100u, 0x115Fu },
    { 0x2329u, 0x232Au },
    { 0x2E80u, 0x303Eu },
    { 0x3041u, 0x33FFu },
    { 0x3400u, 0x4DBFu },
    { 0x4E00u, 0x9FFFu },
    { 0xA000u, 0xA4CFu },
    { 0xAC00u, 0xD7A3u },
    { 0xF900u, 0xFAFFu },
    { 0xFE30u, 0xFE4Fu },
    { 0xFF01u, 0xFF60u },
    { 0xFFE0u, 0xFFE6u },
    { 0x1F300u, 0x1F64Fu },
    { 0x20000u, 0x2FFFDu },
    { 0x30000u, 0x3FFFDu },
};

static bool in_ranges(const suct_range_t *ranges, size_t count,
                      sucs_char_t cp)
{
    for (size_t i = 0u; i < count; i++) {
        if (cp >= ranges[i].lo && cp <= ranges[i].hi) {
            return true;
        }
    }
    return false;
}

/* ================================================================== */
/*  PUA Geometry                                                      */
/* ================================================================== */

bool suct_is_pua(sucs_char_t cp)
{
    if (cp >= SUCT_PUA_BMP_START && cp <= SUCT_PUA_BMP_END) {
        return true;
    }
    if (cp >= SUCT_PUA_SUPP_A_START && cp <= SUCT_PUA_SUPP_A_END) {
        return true;
    }
    if (cp >= SUCT_PUA_SUPP_B_START && cp <= SUCT_PUA_SUPP_B_END) {
        return true;
    }
    return false;
}

bool suct_is_ow_glyph(sucs_char_t cp)
{
    if (cp >= SUCT_OW_GLYPHZONE_START && cp <= SUCT_OW_GLYPHZONE_END) {
        return true;
    }
    if (cp >= SUCT_OW_BOOTZONE_START && cp <= SUCT_OW_BOOTZONE_END) {
        return true;
    }
    if (cp >= SUCT_OW_TELZONE_START && cp <= SUCT_OW_TELZONE_END) {
        return true;
    }
    return false;
}

uint32_t suct_ow_glyph_id(sucs_char_t cp)
{
    if (cp >= SUCT_OW_GLYPHZONE_START && cp <= SUCT_OW_GLYPHZONE_END) {
        return (uint32_t)(cp - SUCT_OW_GLYPHZONE_START);
    }
    if (cp >= SUCT_OW_BOOTZONE_START && cp <= SUCT_OW_BOOTZONE_END) {
        return (uint32_t)(cp - SUCT_OW_BOOTZONE_START);
    }
    if (cp >= SUCT_OW_TELZONE_START && cp <= SUCT_OW_TELZONE_END) {
        return (uint32_t)(cp - SUCT_OW_TELZONE_START);
    }
    return 0xFFFFFFFFu;
}

sucs_char_t suct_ow_glyph_cp(uint8_t zone, uint32_t glyph_id)
{
    if (zone == 0u && glyph_id < SUCT_OW_GLYPHZONE_COUNT) {
        return (sucs_char_t)(SUCT_OW_GLYPHZONE_START + glyph_id);
    }
    if (zone == 1u && glyph_id < 64u) {
        return (sucs_char_t)(SUCT_OW_BOOTZONE_START + glyph_id);
    }
    if (zone == 2u && glyph_id < 32u) {
        return (sucs_char_t)(SUCT_OW_TELZONE_START + glyph_id);
    }
    return SUCS_INVALID_CODEPOINT;
}

/* ================================================================== */
/*  Cell Width Model                                                  */
/* ================================================================== */

uint8_t suct_cell_width(sucs_char_t cp)
{
    if (cp == 0u || !sucs_is_valid(cp)) {
        return 0u;
    }
    if (in_ranges(zero_width_ranges,
                  sizeof(zero_width_ranges) / sizeof(suct_range_t), cp)) {
        return 0u;
    }
    if (in_ranges(wide_ranges,
                  sizeof(wide_ranges) / sizeof(suct_range_t), cp)) {
        return 2u;
    }
    return 1u;
}

/* ================================================================== */
/*  Legacy Encoding Bridges                                           */
/* ================================================================== */

suct_status_t suct_ascii_to_sucs(const uint8_t *ascii, size_t len,
                                 sucs_char_t *out, size_t cap,
                                 size_t *out_count)
{
    if (ascii == NULL || out == NULL || out_count == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    size_t n = 0u;
    for (size_t i = 0u; i < len; i++) {
        uint8_t b = ascii[i];
        if (b > 0x7Fu) {
            return SUCT_ERR_BAD_STREAM;
        }
        if (n >= cap) {
            return SUCT_ERR_ROW_RANGE;
        }
        out[n++] = (sucs_char_t)b;
    }
    *out_count = n;
    return SUCT_OK;
}

suct_status_t suct_utf16le_to_sucs(const uint8_t *u16, size_t bytes,
                                   sucs_char_t *out, size_t cap,
                                   size_t *out_count)
{
    if (u16 == NULL || out == NULL || out_count == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    if ((bytes & 1u) != 0u) {
        return SUCT_ERR_TRUNCATED;
    }
    size_t n = 0u;
    size_t i = 0u;
    while (i < bytes) {
        uint16_t w = (uint16_t)((uint16_t)u16[i] |
                                ((uint16_t)u16[i + 1u] << 8));
        i += 2u;
        sucs_char_t cp;
        if (w >= 0xD800u && w <= 0xDBFFu) {
            if (i + 1u >= bytes) {
                return SUCT_ERR_TRUNCATED;
            }
            uint16_t lo = (uint16_t)((uint16_t)u16[i] |
                                     ((uint16_t)u16[i + 1u] << 8));
            i += 2u;
            if (lo < 0xDC00u || lo > 0xDFFFu) {
                return SUCT_ERR_BAD_STREAM;
            }
            cp = (sucs_char_t)(0x10000u +
                    (((uint32_t)(w - 0xD800u)) << 10) +
                    (uint32_t)(lo - 0xDC00u));
        } else if (w >= 0xDC00u && w <= 0xDFFFu) {
            return SUCT_ERR_BAD_STREAM;
        } else {
            cp = (sucs_char_t)w;
        }
        if (n >= cap) {
            return SUCT_ERR_ROW_RANGE;
        }
        out[n++] = cp;
    }
    *out_count = n;
    return SUCT_OK;
}

/* ================================================================== */
/*  Stream Validation                                                 */
/* ================================================================== */

suct_status_t suct_stream_validate(const uint8_t *sutf8,
                                   size_t *out_codepoints)
{
    if (sutf8 == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    size_t count = 0u;
    const uint8_t *p = sutf8;
    while (p[0] != 0u) {
        sucs_char_t cp;
        size_t adv = sutf8_next_codepoint(p, 6u, &cp);
        if (adv == 0u) {
            break;
        }
        if (cp == SUCS_INVALID_CODEPOINT) {
            return SUCT_ERR_BAD_STREAM;
        }
        count++;
        p += adv;
    }
    if (out_codepoints != NULL) {
        *out_codepoints = count;
    }
    return SUCT_OK;
}

/* ================================================================== */
/*  Text Plane                                                        */
/* ================================================================== */

suct_status_t suct_plane_init(suct_plane_t *plane)
{
    if (plane == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    suct_status_t st = suct_plane_clear(plane);
    if (st != SUCT_OK) {
        return st;
    }
    plane->fg = 7u;
    plane->bg = 0u;
    plane->wrap = true;
    plane->initialized = true;
    return SUCT_OK;
}

suct_status_t suct_plane_clear(suct_plane_t *plane)
{
    if (plane == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    uint16_t attrs = (uint16_t)(plane->fg | (plane->bg << 4));
    for (uint32_t r = 0u; r < SUCT_PLANE_ROWS; r++) {
        for (uint32_t c = 0u; c < SUCT_PLANE_COLS; c++) {
            plane->cells[r][c].cp = 0x20u;
            plane->cells[r][c].attrs = attrs;
        }
    }
    plane->cursor_row = 0u;
    plane->cursor_col = 0u;
    plane->writes = 0u;
    plane->scrolls = 0u;
    return SUCT_OK;
}

suct_status_t suct_plane_set_style(suct_plane_t *plane,
                                   uint16_t fg, uint16_t bg)
{
    if (plane == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    plane->fg = fg & SUCT_ATTR_FG_MASK;
    plane->bg = bg & SUCT_ATTR_BG_MASK;
    return SUCT_OK;
}

suct_status_t suct_plane_put_cp(suct_plane_t *plane, uint16_t row,
                                uint16_t col, sucs_char_t cp,
                                uint16_t attrs)
{
    if (plane == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    if (row >= SUCT_PLANE_ROWS) {
        return SUCT_ERR_ROW_RANGE;
    }
    if (col >= SUCT_PLANE_COLS) {
        return SUCT_ERR_COL_RANGE;
    }

    uint8_t w = suct_cell_width(cp);
    if (w > 1u) {
        attrs |= SUCT_ATTR_WIDE_LEAD;
    }
    plane->cells[row][col].cp = cp;
    plane->cells[row][col].attrs = attrs;

    if (w > 1u && col + 1u < SUCT_PLANE_COLS) {
        suct_cell_t *tail = &plane->cells[row][col + 1u];
        tail->cp = SUCT_GLYPH_CONT;
        tail->attrs = (uint16_t)(attrs | SUCT_ATTR_WIDE_TAIL);
    }
    return SUCT_OK;
}

static void plane_advance_line(suct_plane_t *plane)
{
    if (plane->cursor_row + 1u < SUCT_PLANE_ROWS) {
        plane->cursor_row++;
    } else {
        (void)suct_plane_scroll(plane, 1u);
    }
    plane->cursor_col = 0u;
}

suct_status_t suct_plane_newline(suct_plane_t *plane)
{
    if (plane == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    plane_advance_line(plane);
    return SUCT_OK;
}

static suct_status_t plane_emit_cp(suct_plane_t *plane, sucs_char_t cp,
                                   uint16_t attrs)
{
    uint16_t w = suct_cell_width(cp);
    if (w == 2u) {
        if (plane->cursor_col + 1u >= SUCT_PLANE_COLS) {
            plane_advance_line(plane);
        }
        if (plane->cursor_col + 2u > SUCT_PLANE_COLS) {
            return SUCT_ERR_COL_RANGE;  /* unplaceable even after wrap */
        }
    } else if (w == 0u) {
        return SUCT_OK;                 /* combining: no cell consumed */
    }

    if (plane->cursor_col >= SUCT_PLANE_COLS) {
        plane_advance_line(plane);
    }

    suct_status_t st = suct_plane_put_cp(plane, plane->cursor_row,
                                         plane->cursor_col, cp, attrs);
    if (st != SUCT_OK) {
        return st;
    }
    plane->cursor_col = (uint16_t)(plane->cursor_col + w);
    return SUCT_OK;
}

suct_status_t suct_plane_write_sutf8(suct_plane_t *plane, uint16_t row,
                                     uint16_t col, const uint8_t *sutf8,
                                     uint16_t attrs, uint16_t *out_col)
{
    if (plane == NULL || sutf8 == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    if (!plane->initialized) {
        return SUCT_ERR_NOT_INIT;
    }
    if (row >= SUCT_PLANE_ROWS) {
        return SUCT_ERR_ROW_RANGE;
    }
    if (col >= SUCT_PLANE_COLS) {
        return SUCT_ERR_COL_RANGE;
    }

    uint16_t r = row;
    uint16_t c = col;
    const uint8_t *p = sutf8;
    while (p[0] != 0u) {
        sucs_char_t cp;
        size_t adv = sutf8_next_codepoint(p, 6u, &cp);
        if (adv == 0u) {
            break;
        }
        if (cp == SUCS_INVALID_CODEPOINT) {
            return SUCT_ERR_BAD_STREAM;
        }
        p += adv;

        if (cp == (sucs_char_t)0x0Au) {
            r = (uint16_t)(r + 1u);
            c = 0u;
            continue;
        }
        if (cp == (sucs_char_t)0x0Du) {
            c = 0u;
            continue;
        }

        uint8_t w = suct_cell_width(cp);
        if (w == 0u) {
            continue;
        }
        if (c + (uint16_t)w > SUCT_PLANE_COLS) {
            if (!plane->wrap) {
                return SUCT_ERR_COL_RANGE;
            }
            r = (uint16_t)(r + 1u);
            c = 0u;
        }
        if (r >= SUCT_PLANE_ROWS) {
            /* Bottom: scroll and recompute target row. */
            suct_status_t st = suct_plane_scroll(plane, 1u);
            if (st != SUCT_OK) {
                return st;
            }
            if (r > 0u) {
                r = (uint16_t)(r - 1u);
            }
        }
        suct_status_t st = suct_plane_put_cp(plane, r, c, cp, attrs);
        if (st != SUCT_OK) {
            return st;
        }
        c = (uint16_t)(c + w);
    }

    plane->writes++;
    if (out_col != NULL) {
        *out_col = c;
    }
    return SUCT_OK;
}

suct_status_t suct_plane_cursor_put_sutf8(suct_plane_t *plane,
                                          const uint8_t *sutf8)
{
    if (plane == NULL || sutf8 == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    if (!plane->initialized) {
        return SUCT_ERR_NOT_INIT;
    }
    uint16_t attrs = (uint16_t)(plane->fg | (plane->bg << 4));
    const uint8_t *p = sutf8;
    while (p[0] != 0u) {
        sucs_char_t cp;
        size_t adv = sutf8_next_codepoint(p, 6u, &cp);
        if (adv == 0u) {
            break;
        }
        if (cp == SUCS_INVALID_CODEPOINT) {
            return SUCT_ERR_BAD_STREAM;
        }
        p += adv;

        if (cp == (sucs_char_t)0x0Au) {
            plane_advance_line(plane);
            continue;
        }
        if (cp == (sucs_char_t)0x0Du) {
            plane->cursor_col = 0u;
            continue;
        }
        suct_status_t st = plane_emit_cp(plane, cp, attrs);
        if (st != SUCT_OK) {
            return st;
        }
    }
    plane->writes++;
    return SUCT_OK;
}

suct_status_t suct_plane_scroll(suct_plane_t *plane, uint16_t lines)
{
    if (plane == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    if (lines == 0u || lines >= SUCT_PLANE_ROWS) {
        return SUCT_ERR_ROW_RANGE;
    }
    uint16_t attrs = (uint16_t)(plane->fg | (plane->bg << 4));
    for (uint32_t r = lines; r < SUCT_PLANE_ROWS; r++) {
        uint32_t dst = r - (uint32_t)lines;
        for (uint32_t c = 0u; c < SUCT_PLANE_COLS; c++) {
            plane->cells[dst][c] = plane->cells[r][c];
        }
    }
    for (uint32_t r = SUCT_PLANE_ROWS - lines; r < SUCT_PLANE_ROWS; r++) {
        for (uint32_t c = 0u; c < SUCT_PLANE_COLS; c++) {
            plane->cells[r][c].cp = 0x20u;
            plane->cells[r][c].attrs = attrs;
        }
    }
    plane->scrolls += (uint64_t)lines;
    return SUCT_OK;
}

suct_status_t suct_plane_row_to_sutf8(const suct_plane_t *plane,
                                      uint16_t row, uint8_t *out,
                                      size_t cap, size_t *out_len)
{
    if (plane == NULL || out == NULL || out_len == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    if (row >= SUCT_PLANE_ROWS) {
        return SUCT_ERR_ROW_RANGE;
    }

    /* Trim trailing blanks (cells hold 0x20 after clear/scroll). */
    int16_t last = -1;
    for (uint16_t c = 0u; c < SUCT_PLANE_COLS; c++) {
        const suct_cell_t *cell = &plane->cells[row][c];
        if ((cell->attrs & SUCT_ATTR_WIDE_TAIL) != 0u) {
            continue;
        }
        sucs_char_t cp = cell->cp;
        if (cp != 0x20u && cp != 0u) {
            last = (int16_t)c;
        }
    }

    size_t used = 0u;
    if (last >= 0) {
        for (uint16_t c = 0u; (int16_t)c <= last; c++) {
            const suct_cell_t *cell = &plane->cells[row][c];
            if ((cell->attrs & SUCT_ATTR_WIDE_TAIL) != 0u) {
                continue;   /* continuation cell of a wide glyph */
            }
            size_t n = sutf8_encode_char(cell->cp, out + used,
                                         cap - used);
            if (n == 0u) {
                return SUCT_ERR_UNTRANSMITTABLE;
            }
            used += n;
        }
    }
    *out_len = used;
    return SUCT_OK;
}

/* ================================================================== */
/*  Console                                                           */
/* ================================================================== */

static void console_mark_dirty(suct_console_t *console, uint16_t from_row)
{
    uint32_t mask = 0u;
    for (uint32_t r = from_row; r < SUCT_PLANE_ROWS; r++) {
        mask |= (uint32_t)1u << r;
    }
    console->dirty_rows |= mask;
}

suct_status_t suct_console_init(suct_console_t *console)
{
    if (console == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    suct_status_t st = suct_plane_init(&console->plane);
    if (st != SUCT_OK) {
        return st;
    }
    console->sink.emit = NULL;
    console->sink.token = 0u;
    console->sink_bound = false;
    console->dirty_rows = 0u;
    return SUCT_OK;
}

suct_status_t suct_console_bind_sink(suct_console_t *console,
                                     suct_sink_t sink)
{
    if (console == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    console->sink = sink;
    console->sink_bound = (sink.emit != NULL);
    return SUCT_OK;
}

suct_status_t suct_console_write_sutf8(suct_console_t *console,
                                       const uint8_t *sutf8)
{
    if (console == NULL || sutf8 == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    if (!console->plane.initialized) {
        return SUCT_ERR_NOT_INIT;
    }
    uint16_t start_row = console->plane.cursor_row;
    uint64_t scrolls_before = console->plane.scrolls;

    suct_status_t st = suct_plane_cursor_put_sutf8(&console->plane, sutf8);
    if (st != SUCT_OK) {
        return st;
    }

    if (scrolls_before != console->plane.scrolls) {
        console_mark_dirty(console, 0u);       /* everything shifted */
    } else {
        console_mark_dirty(console, start_row);
    }

    if (console->sink_bound) {
        return suct_console_render(console);
    }
    return SUCT_OK;
}

suct_status_t suct_console_render(suct_console_t *console)
{
    if (console == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    if (!console->sink_bound) {
        return SUCT_ERR_NO_SINK;
    }

    uint8_t buf[SUCT_ROW_BUF_CAP];
    for (uint32_t r = 0u; r < SUCT_PLANE_ROWS; r++) {
        if ((console->dirty_rows & ((uint32_t)1u << r)) == 0u) {
            continue;
        }
        size_t len = 0u;
        suct_status_t st = suct_plane_row_to_sutf8(&console->plane,
                                                   (uint16_t)r, buf,
                                                   SUCT_ROW_BYTES_MAX,
                                                   &len);
        if (st != SUCT_OK) {
            return st;
        }
        if (len < SUCT_ROW_BUF_CAP) {
            buf[len++] = 0x0Au;                /* line feed framing */
        }
        console->sink.emit(console->sink.token, buf, (uint32_t)len);
        console->dirty_rows &= ~((uint32_t)1u << r);
    }
    return SUCT_OK;
}

/* ================================================================== */
/*  OWRP-Backed Sink                                                  */
/* ================================================================== */

static void suct_emit_via_owrp(uintptr_t token, const uint8_t *sutf8,
                               uint32_t len)
{
    owrp_context_t *g = (owrp_context_t *)token;
    if (g != NULL) {
        /* Gate args: port=0, user buffer pointer, byte count. The kernel
         * syscall handler copies the frame across the ring boundary and
         * routes it to sio_gate_write(). */
        (void)owrp_invoke_gate(g, OWRP_SYS_SIO_WRITE, 0u,
                               (uint64_t)(uintptr_t)sutf8, (uint64_t)len);
    }
}

suct_status_t suct_console_bind_owrp(suct_console_t *console,
                                     owrp_context_t *g)
{
    if (console == NULL || g == NULL) {
        return SUCT_ERR_NULL_POINTER;
    }
    suct_sink_t sink;
    sink.emit  = suct_emit_via_owrp;
    sink.token = (uintptr_t)g;
    return suct_console_bind_sink(console, sink);
}