#ifndef BOOTVID_H
#define BOOTVID_H

#include <stdint.h>
#include <stdbool.h>

#include "sucs_types.h"
#include "sutf8.h"

#define FONT_W 8
#define FONT_H 16

extern const uint8_t bootvid_font[95][16];

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserved;
} bootvid_pixel_t;

#define BOOTVID_COLOR_BLACK      0xFF000000u
#define BOOTVID_COLOR_WHITE      0xFFFFFFFFu
#define BOOTVID_COLOR_RED        0xFF0000FFu
#define BOOTVID_COLOR_GREEN      0xFF00FF00u
#define BOOTVID_COLOR_BLUE       0xFFFF0000u
#define BOOTVID_COLOR_YELLOW     0xFF00FFFFu
#define BOOTVID_COLOR_CYAN       0xFFFFFF00u
#define BOOTVID_COLOR_MAGENTA    0xFFFF00FFu
#define BOOTVID_COLOR_DARK_GRAY  0xFF404040u
#define BOOTVID_COLOR_LIGHT_RED   0xFF8080FFu
#define BOOTVID_COLOR_LIGHT_GREEN 0xFF80FF80u
#define BOOTVID_COLOR_LIGHT_BLUE  0xFFFF8080u
#define BOOTVID_COLOR_ORANGE      0xFF0080FFu

typedef enum {
    PIXELFORMAT_RGBX32 = 0,
    PIXELFORMAT_BGRX32 = 1,
} bootvid_pixel_format_t;

/* B+ (0x0011A000-0x0011A77F): Fatal faults                          */
/* S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                    */
typedef uint32_t bootvid_status_t;

#define BOOTVID_OK                      0x00000000u  /* Success              */
/* B+ Fatal */
#define BOOTVID_ERR_OUT_OF_BOUNDS       0x0011A000u  /* Framebuffer OOB      */
/* S+ Soft */
#define BOOTVID_ERR_NOT_INIT            0x0011AE00u  /* Not initialized      */
#define BOOTVID_ERR_INVALID_POS         0x0011AE01u  /* Bad position         */
#define BOOTVID_ERR_NULL_POINTER        0x0011AE02u  /* Null pointer         */

typedef struct {
    void    *framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t pixel_format;
    uint32_t cursor_row;
    uint32_t cursor_col;
    uint32_t text_fg;
    uint32_t text_bg;
    bool     initialized;
    uint32_t max_rows;
    uint32_t max_cols;
} bootvid_context_t;

bootvid_status_t bootvid_init(bootvid_context_t *ctx, void *framebuffer, uint32_t width, uint32_t height, uint32_t pitch);
bootvid_status_t bootvid_clear(bootvid_context_t *ctx, uint32_t color);
bootvid_status_t bootvid_putchar(bootvid_context_t *ctx, uint32_t row, uint32_t col, char ch, uint32_t fg, uint32_t bg);
bootvid_status_t bootvid_puts(bootvid_context_t *ctx, uint32_t row, uint32_t col, const char *str, uint32_t fg, uint32_t bg);
bootvid_status_t bootvid_putc(bootvid_context_t *ctx, char ch);
bootvid_status_t bootvid_puts_at(bootvid_context_t *ctx, const char *str, uint32_t fg, uint32_t bg);
bootvid_status_t bootvid_fill_rect(bootvid_context_t *ctx, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
bootvid_status_t bootvid_draw_hline(bootvid_context_t *ctx, uint32_t x, uint32_t y, uint32_t length, uint32_t color);
bootvid_status_t bootvid_draw_vline(bootvid_context_t *ctx, uint32_t x, uint32_t y, uint32_t length, uint32_t color);
bootvid_status_t bootvid_set_cursor(bootvid_context_t *ctx, uint32_t row, uint32_t col);
bootvid_status_t bootvid_set_colors(bootvid_context_t *ctx, uint32_t fg, uint32_t bg);
bootvid_status_t bootvid_scroll_up(bootvid_context_t *ctx, uint32_t lines);
bootvid_status_t bootvid_progress_bar(bootvid_context_t *ctx, uint32_t row, uint32_t col, uint32_t width, uint8_t progress, uint32_t fg, uint32_t bg);
bootvid_status_t bootvid_panic_screen(bootvid_context_t *ctx, const char *message);

/* ================================================================== */
/*  SuperUnicode (SUCS) text API                                      */
/*  Text is passed as SUCS codepoints (sucs_char_t) or SUTF-8 streams. */
/*  Only the built-in glyph subset (0x20-0x7E) is renderable; all     */
/*  other valid SUCS codepoints render as space.                      */
/* ================================================================== */

bootvid_status_t bootvid_putchar_sucs(bootvid_context_t *ctx, uint32_t row, uint32_t col, sucs_char_t cp, uint32_t fg, uint32_t bg);
bootvid_status_t bootvid_puts_sucs(bootvid_context_t *ctx, uint32_t row, uint32_t col, const uint8_t *sutf8, uint32_t fg, uint32_t bg);
bootvid_status_t bootvid_putc_sucs(bootvid_context_t *ctx, sucs_char_t cp);
bootvid_status_t bootvid_panic_screen_sucs(bootvid_context_t *ctx, const uint8_t *sutf8_message);

#endif /* BOOTVID_H */
