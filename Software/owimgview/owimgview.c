/*
 * owimgview.c - OpenWindows Image Viewer (.owx)
 *
 * Parses a small raster header (OWIMG: magic, version, width, height, bpp)
 * from a file through the OWFS gateway, validates every field and the payload
 * length, then renders an ASCII-art thumbnail by sampling the pixel buffer down
 * to a character grid.  With no file - or when the read fails - it renders a
 * modelled grayscale test pattern so the renderer can always be exercised.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWIMGVIEW_VERSION "1.0.0"

#define OWIMGVIEW_MAX_FILE   65536u
#define OWIMGVIEW_MODEL_W    64u
#define OWIMGVIEW_MODEL_H    32u
#define OWIMGVIEW_COLS_MAX   96u
#define OWIMGVIEW_ROWS_MAX   48u
#define OWIMGVIEW_HEADER_LEN 12u

/* Raster formats the reader understands. */
#define OWIMGVIEW_BPP_1   1u
#define OWIMGVIEW_BPP_8   8u
#define OWIMGVIEW_BPP_24  24u

typedef struct {
    uint16_t       width;
    uint16_t       height;
    uint8_t        bpp;
    const uint8_t *pixels;
    uint32_t       payload_len;
} owimgview_image_t;

static uint8_t s_file[OWIMGVIEW_MAX_FILE];
static uint8_t s_model[OWIMGVIEW_MODEL_W * OWIMGVIEW_MODEL_H];
static char    s_line[OWIMGVIEW_COLS_MAX + 1u];

/* ── Forward declarations ─────────────────────────────────────── */

static int  owimgview_run(int argc, const char *const *argv);
static void owimgview_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owimgview_usage();
        return 1;
    }
    return owimgview_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owimgview_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static bool owimgview_parse_u32(const char *s, uint32_t *out)
{
    uint64_t v = 0u;
    if (s == NULL || *s == '\0') {
        return false;
    }
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return false;
        }
        v = v * 10u + (uint64_t)(uint32_t)(*s - '0');
        if (v > 100000u) {
            return false;
        }
        s++;
    }
    *out = (uint32_t)v;
    return true;
}

static uint16_t owimgview_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void owimgview_put_hex(uint32_t v)
{
    static const char digits[] = "0123456789abcdef";
    int shift;
    owx_puts("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        owx_putc(digits[(v >> shift) & 0xFu]);
    }
}

/* Packed-format pixel luminance, 0..255. */
static uint32_t owimgview_luma(const owimgview_image_t *img, uint32_t x,
                               uint32_t y)
{
    if (img->bpp == OWIMGVIEW_BPP_8) {
        return (uint32_t)img->pixels[y * (uint32_t)img->width + x];
    }
    if (img->bpp == OWIMGVIEW_BPP_1) {
        uint32_t stride = ((uint32_t)img->width + 7u) / 8u;
        uint8_t byte = img->pixels[y * stride + (x / 8u)];
        uint32_t bit = ((uint32_t)byte >> (7u - (x & 7u))) & 1u;
        return bit ? 255u : 0u;
    }
    {
        uint32_t off = (y * (uint32_t)img->width + x) * 3u;
        uint32_t b = img->pixels[off + 0u];
        uint32_t g = img->pixels[off + 1u];
        uint32_t r = img->pixels[off + 2u];
        return (r * 30u + g * 59u + b * 11u) / 100u;
    }
}

static bool owimgview_parse(const uint8_t *buf, uint32_t len,
                            owimgview_image_t *img, const char **err)
{
    uint16_t w;
    uint16_t h;
    uint8_t bpp;
    uint32_t need;

    if (len < OWIMGVIEW_HEADER_LEN) {
        *err = "file shorter than the 12-byte header";
        return false;
    }
    if (buf[0] != 'O' || buf[1] != 'W' || buf[2] != 'I' || buf[3] != 'M') {
        *err = "bad magic (expected OWIM)";
        return false;
    }
    if (owimgview_u16(buf + 4u) != 1u) {
        *err = "unsupported format version";
        return false;
    }
    w = owimgview_u16(buf + 6u);
    h = owimgview_u16(buf + 8u);
    bpp = buf[10];
    if (w == 0u || h == 0u) {
        *err = "zero width or height";
        return false;
    }
    if (w > 256u || h > 256u) {
        *err = "dimension exceeds 256";
        return false;
    }
    if (bpp != OWIMGVIEW_BPP_1 && bpp != OWIMGVIEW_BPP_8 &&
        bpp != OWIMGVIEW_BPP_24) {
        *err = "unsupported bits-per-pixel";
        return false;
    }

    if (bpp == OWIMGVIEW_BPP_1) {
        need = (((uint32_t)w + 7u) / 8u) * (uint32_t)h;
    } else if (bpp == OWIMGVIEW_BPP_8) {
        need = (uint32_t)w * (uint32_t)h;
    } else {
        need = (uint32_t)w * (uint32_t)h * 3u;
    }
    if (need > len - OWIMGVIEW_HEADER_LEN) {
        *err = "payload shorter than the header claims";
        return false;
    }

    img->width = w;
    img->height = h;
    img->bpp = bpp;
    img->pixels = buf + OWIMGVIEW_HEADER_LEN;
    img->payload_len = need;
    return true;
}

/* Build a deterministic grayscale test pattern (radial falloff + gradient). */
static void owimgview_build_model(owimgview_image_t *img)
{
    uint32_t x;
    uint32_t y;
    for (y = 0u; y < OWIMGVIEW_MODEL_H; y++) {
        for (x = 0u; x < OWIMGVIEW_MODEL_W; x++) {
            int32_t dx = (int32_t)x - (int32_t)(OWIMGVIEW_MODEL_W / 2u);
            int32_t dy = (int32_t)y - (int32_t)(OWIMGVIEW_MODEL_H / 2u);
            uint32_t d2 = (uint32_t)(dx * dx + dy * dy);
            int32_t v = 255 - (int32_t)(d2 / 6u);
            v += (int32_t)(x * 2u) - (int32_t)(y * 3u);
            if (v < 0) {
                v = 0;
            }
            if (v > 255) {
                v = 255;
            }
            s_model[y * OWIMGVIEW_MODEL_W + x] = (uint8_t)v;
        }
    }
    img->width = (uint16_t)OWIMGVIEW_MODEL_W;
    img->height = (uint16_t)OWIMGVIEW_MODEL_H;
    img->bpp = OWIMGVIEW_BPP_8;
    img->pixels = s_model;
    img->payload_len = OWIMGVIEW_MODEL_W * OWIMGVIEW_MODEL_H;
}

static void owimgview_render(const owimgview_image_t *img, uint32_t cols,
                             uint32_t rows)
{
    static const char ramp[] = " .:-=+*#%@";
    uint32_t j;
    uint32_t i;

    for (j = 0u; j < rows; j++) {
        uint32_t sy = (j * (uint32_t)img->height) / rows;
        for (i = 0u; i < cols; i++) {
            uint32_t sx = (i * (uint32_t)img->width) / cols;
            uint32_t lum = owimgview_luma(img, sx, sy);
            uint32_t idx = (lum * 9u) / 255u;
            if (idx > 9u) {
                idx = 9u;
            }
            s_line[i] = ramp[idx];
        }
        s_line[cols] = '\0';
        owx_puts(s_line);
        owx_puts("\r\n");
    }
}

static uint32_t owimgview_checksum(const owimgview_image_t *img)
{
    uint32_t sum = 0u;
    uint32_t i;
    for (i = 0u; i < img->payload_len; i++) {
        sum = (sum + (uint32_t)img->pixels[i]) & 0xFFFFFFFFu;
    }
    return sum;
}

/* ── Implementation ───────────────────────────────────────────── */

static int owimgview_run(int argc, const char *const *argv)
{
    const char *path = NULL;
    uint32_t cols = 32u;
    uint32_t rows = 16u;
    owimgview_image_t img;
    const char *err = "";
    int64_t n = 0;
    bool from_file = false;
    int i;

    for (i = 1; i < argc; i++) {
        const char *opt = argv[i];
        if (opt == NULL) {
            continue;
        }
        if (owimgview_streq(opt, "-f") || owimgview_streq(opt, "--file")) {
            if (i + 1 >= argc || argv[i + 1] == NULL) {
                owx_puts("owimgview: -f requires a path\r\n");
                owimgview_usage();
                return 2;
            }
            path = argv[++i];
        } else if (owimgview_streq(opt, "-c") || owimgview_streq(opt, "--cols")) {
            if (!owimgview_parse_u32((i + 1 < argc) ? argv[i + 1] : NULL, &cols) ||
                cols < 4u || cols > OWIMGVIEW_COLS_MAX) {
                owx_puts("owimgview: -c must be 4..96\r\n");
                return 2;
            }
            i++;
        } else if (owimgview_streq(opt, "-r") || owimgview_streq(opt, "--rows")) {
            if (!owimgview_parse_u32((i + 1 < argc) ? argv[i + 1] : NULL, &rows) ||
                rows < 2u || rows > OWIMGVIEW_ROWS_MAX) {
                owx_puts("owimgview: -r must be 2..48\r\n");
                return 2;
            }
            i++;
        } else if (owimgview_streq(opt, "-h") || owimgview_streq(opt, "--help")) {
            owimgview_usage();
            return 0;
        } else {
            owx_puts("owimgview: unknown option '");
            owx_puts(opt);
            owx_puts("'\r\n");
            owimgview_usage();
            return 2;
        }
    }

    if (path != NULL) {
        n = owx_file_read(path, s_file, (uint32_t)sizeof(s_file));
        if (n >= 0 && owimgview_parse(s_file, (uint32_t)n, &img, &err)) {
            from_file = true;
        } else {
            if (n < 0) {
                owx_puts("owimgview: cannot read '");
                owx_puts(path);
                owx_puts("' (gateway code ");
                owx_put_i64(n);
                owx_puts(")\r\n");
            } else {
                owx_puts("owimgview: '");
                owx_puts(path);
                owx_puts("' is not a valid OWIMG: ");
                owx_puts(err);
                owx_puts("\r\n");
            }
            owimgview_build_model(&img);
        }
    } else {
        owimgview_build_model(&img);
    }

    owx_puts("owimgview: source=");
    owx_puts(from_file ? path : "model");
    owx_puts(" format=OWIMG");
    owx_puts(" size=");
    owx_put_u64((uint64_t)img.width);
    owx_putc('x');
    owx_put_u64((uint64_t)img.height);
    owx_puts(" bpp=");
    owx_put_u64((uint64_t)img.bpp);
    owx_puts("\r\n");

    owx_puts("owimgview: payload=");
    owx_put_u64((uint64_t)img.payload_len);
    owx_puts(" bytes checksum=");
    owimgview_put_hex(owimgview_checksum(&img));
    owx_puts(" thumbnail=");
    owx_put_u64((uint64_t)cols);
    owx_putc('x');
    owx_put_u64((uint64_t)rows);
    owx_puts("\r\n");
    owx_puts("+");
    for (i = 0; i < (int)cols; i++) {
        owx_putc('-');
    }
    owx_puts("+\r\n");

    owimgview_render(&img, cols, rows);

    owx_puts("+");
    for (i = 0; i < (int)cols; i++) {
        owx_putc('-');
    }
    owx_puts("+\r\n");

    if (path != NULL && !from_file) {
        return 3;
    }
    return 0;
}

static void owimgview_usage(void)
{
    owx_puts(
        "usage: owimgview [-f file] [-c cols] [-r rows] [-h]\r\n"
        "  -f, --file P    read an OWIMG file; without it a model pattern is shown\r\n"
        "  -c, --cols N    thumbnail width in characters, 4..96 (default 32)\r\n"
        "  -r, --rows N    thumbnail height in characters, 2..48 (default 16)\r\n"
        "  -h, --help      show this help\r\n"
        "  OWIMG header: 'OWIM', u16 version=1, u16 width, u16 height, u8 bpp (1/8/24)\r\n");
}
