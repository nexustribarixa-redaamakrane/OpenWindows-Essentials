/*
 * owscreenshot.c - OpenWindows Screenshot Capture Tool (.owx)
 *
 * Captures a modelled framebuffer (a chosen geometry plus a generated pattern,
 * or the pixels of a supplied OWIMG file) and exports it as a 24-bpp BMP into a
 * static staging buffer, with the bottom-up, 4-byte-padded rows the format
 * requires.  It then reports the geometry, the encoded size, and a checksum.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWSCREENSHOT_VERSION "1.0.0"

#define OWSCREENSHOT_MAX_W   128u
#define OWSCREENSHOT_MAX_H   96u
#define OWSCREENSHOT_RGB_MAX (OWSCREENSHOT_MAX_W * OWSCREENSHOT_MAX_H * 3u)
#define OWSCREENSHOT_BMP_MAX (54u + ((OWSCREENSHOT_MAX_W * 3u + 3u) / 4u) * 4u * OWSCREENSHOT_MAX_H)
#define OWSCREENSHOT_HEADER  54u

#define OWSCREENSHOT_PAT_GRADIENT 0u
#define OWSCREENSHOT_PAT_CHECKER  1u
#define OWSCREENSHOT_PAT_RINGS    2u

static uint8_t s_rgb[OWSCREENSHOT_RGB_MAX];
static uint8_t s_bmp[OWSCREENSHOT_BMP_MAX];

/* ── Forward declarations ─────────────────────────────────────── */

static int  owscreenshot_run(int argc, const char *const *argv);
static void owscreenshot_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owscreenshot_usage();
        return 1;
    }
    return owscreenshot_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owscreenshot_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static bool owscreenshot_parse_u32(const char *s, uint32_t *out)
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
        if (v > 1000000u) {
            return false;
        }
        s++;
    }
    *out = (uint32_t)v;
    return true;
}

static uint16_t owscreenshot_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void owscreenshot_set(uint8_t *p, uint32_t idx, uint8_t r, uint8_t g,
                             uint8_t b)
{
    p[idx * 3u + 0u] = r;
    p[idx * 3u + 1u] = g;
    p[idx * 3u + 2u] = b;
}

static void owscreenshot_pattern(uint32_t w, uint32_t h, uint32_t pattern)
{
    uint32_t x;
    uint32_t y;
    uint32_t denom_x = (w > 1u) ? (w - 1u) : 1u;
    uint32_t denom_y = (h > 1u) ? (h - 1u) : 1u;

    for (y = 0u; y < h; y++) {
        for (x = 0u; x < w; x++) {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            if (pattern == OWSCREENSHOT_PAT_CHECKER) {
                uint32_t block = ((x / 8u) + (y / 8u)) & 1u;
                r = (uint8_t)(block ? 220u : 30u);
                g = (uint8_t)(block ? 220u : 60u);
                b = (uint8_t)(block ? 240u : 140u);
            } else if (pattern == OWSCREENSHOT_PAT_RINGS) {
                int32_t dx = (int32_t)x - (int32_t)(w / 2u);
                int32_t dy = (int32_t)y - (int32_t)(h / 2u);
                uint32_t d = (uint32_t)(dx * dx + dy * dy);
                uint8_t band = (uint8_t)((d / 32u) & 1u);
                r = (uint8_t)(band ? 240u : 20u);
                g = (uint8_t)(band ? 200u : 20u);
                b = (uint8_t)(band ? 80u : 160u);
            } else {
                r = (uint8_t)((x * 255u) / denom_x);
                g = (uint8_t)((y * 255u) / denom_y);
                b = (uint8_t)(((x + y) * 255u) / (denom_x + denom_y));
            }
            owscreenshot_set(s_rgb, y * w + x, r, g, b);
        }
    }
}

static void owscreenshot_put16(uint32_t *pos, uint16_t v)
{
    s_bmp[(*pos)++] = (uint8_t)(v & 0xFFu);
    s_bmp[(*pos)++] = (uint8_t)((v >> 8) & 0xFFu);
}

static void owscreenshot_put32(uint32_t *pos, uint32_t v)
{
    s_bmp[(*pos)++] = (uint8_t)(v & 0xFFu);
    s_bmp[(*pos)++] = (uint8_t)((v >> 8) & 0xFFu);
    s_bmp[(*pos)++] = (uint8_t)((v >> 16) & 0xFFu);
    s_bmp[(*pos)++] = (uint8_t)((v >> 24) & 0xFFu);
}

/* Encode the current s_rgb frame as a 24-bpp BMP; returns the byte size. */
static uint32_t owscreenshot_build_bmp(uint32_t w, uint32_t h)
{
    uint32_t stride = ((w * 3u + 3u) / 4u) * 4u;
    uint32_t imgsize = stride * h;
    uint32_t filesize = OWSCREENSHOT_HEADER + imgsize;
    uint32_t pos = 0u;
    uint32_t row;

    s_bmp[pos++] = 'B';
    s_bmp[pos++] = 'M';
    owscreenshot_put32(&pos, filesize);
    owscreenshot_put16(&pos, 0u);
    owscreenshot_put16(&pos, 0u);
    owscreenshot_put32(&pos, OWSCREENSHOT_HEADER);

    owscreenshot_put32(&pos, 40u);       /* BITMAPINFOHEADER size */
    owscreenshot_put32(&pos, w);
    owscreenshot_put32(&pos, h);         /* positive -> bottom-up */
    owscreenshot_put16(&pos, 1u);        /* planes */
    owscreenshot_put16(&pos, 24u);       /* bits per pixel */
    owscreenshot_put32(&pos, 0u);        /* BI_RGB */
    owscreenshot_put32(&pos, imgsize);
    owscreenshot_put32(&pos, 2835u);     /* ~72 dpi */
    owscreenshot_put32(&pos, 2835u);
    owscreenshot_put32(&pos, 0u);
    owscreenshot_put32(&pos, 0u);

    for (row = h; row > 0u; row--) {
        uint32_t ry = row - 1u;
        uint32_t x;
        uint32_t pad;
        for (x = 0u; x < w; x++) {
            uint32_t idx = ry * w + x;
            s_bmp[pos++] = s_rgb[idx * 3u + 2u];   /* BMP stores BGR */
            s_bmp[pos++] = s_rgb[idx * 3u + 1u];
            s_bmp[pos++] = s_rgb[idx * 3u + 0u];
        }
        for (pad = w * 3u; pad < stride; pad++) {
            s_bmp[pos++] = 0u;
        }
    }
    return filesize;
}

static uint32_t owscreenshot_checksum(uint32_t size)
{
    uint32_t sum = 0u;
    uint32_t i;
    for (i = 0u; i < size; i++) {
        sum = (sum * 131u + (uint32_t)s_bmp[i]) & 0xFFFFFFFFu;
    }
    return sum;
}

static void owscreenshot_put_hex2(uint8_t v)
{
    static const char digits[] = "0123456789abcdef";
    owx_putc(digits[(v >> 4) & 0xFu]);
    owx_putc(digits[v & 0xFu]);
}

/* Load an OWIMG file into s_rgb; returns 0 on success or a negative code. */
static int32_t owscreenshot_load_file(const char *path, uint32_t *out_w,
                                      uint32_t *out_h)
{
    static uint8_t s_file[OWSCREENSHOT_RGB_MAX + 12u];
    int64_t n = owx_file_read(path, s_file, (uint32_t)sizeof(s_file));
    uint16_t w;
    uint16_t h;
    uint8_t bpp;
    uint32_t need;
    uint32_t x;
    uint32_t y;

    if (n < 0) {
        return (int32_t)n;
    }
    if ((uint32_t)n < 12u) {
        return -1;
    }
    if (s_file[0] != 'O' || s_file[1] != 'W' || s_file[2] != 'I' ||
        s_file[3] != 'M') {
        return -1;
    }
    if (owscreenshot_u16(s_file + 4u) != 1u) {
        return -1;
    }
    w = owscreenshot_u16(s_file + 6u);
    h = owscreenshot_u16(s_file + 8u);
    bpp = s_file[10];
    if (w == 0u || h == 0u || w > OWSCREENSHOT_MAX_W || h > OWSCREENSHOT_MAX_H) {
        return -1;
    }
    if (bpp != 1u && bpp != 8u && bpp != 24u) {
        return -1;
    }
    if (bpp == 1u) {
        need = (((uint32_t)w + 7u) / 8u) * (uint32_t)h;
    } else if (bpp == 8u) {
        need = (uint32_t)w * (uint32_t)h;
    } else {
        need = (uint32_t)w * (uint32_t)h * 3u;
    }
    if ((uint32_t)n < 12u + need) {
        return -1;
    }

    for (y = 0u; y < (uint32_t)h; y++) {
        for (x = 0u; x < (uint32_t)w; x++) {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            const uint8_t *pix = s_file + 12u;
            if (bpp == 24u) {
                uint32_t off = (y * (uint32_t)w + x) * 3u;
                b = pix[off + 0u];
                g = pix[off + 1u];
                r = pix[off + 2u];
            } else if (bpp == 8u) {
                r = pix[y * (uint32_t)w + x];
                g = r;
                b = r;
            } else {
                uint32_t stride = ((uint32_t)w + 7u) / 8u;
                uint8_t byte = pix[y * stride + (x / 8u)];
                uint8_t bit = (uint8_t)((byte >> (7u - (x & 7u))) & 1u);
                r = (uint8_t)(bit ? 255u : 0u);
                g = r;
                b = r;
            }
            owscreenshot_set(s_rgb, y * (uint32_t)w + x, r, g, b);
        }
    }
    *out_w = w;
    *out_h = h;
    return 0;
}

/* ── Implementation ───────────────────────────────────────────── */

static int owscreenshot_run(int argc, const char *const *argv)
{
    uint32_t w = 64u;
    uint32_t h = 48u;
    uint32_t pattern = OWSCREENSHOT_PAT_GRADIENT;
    const char *path = NULL;
    bool dump = false;
    uint32_t size;
    uint32_t i;
    int a;

    for (a = 1; a < argc; a++) {
        const char *opt = argv[a];
        if (opt == NULL) {
            continue;
        }
        if (owscreenshot_streq(opt, "-w") || owscreenshot_streq(opt, "--width")) {
            if (!owscreenshot_parse_u32((a + 1 < argc) ? argv[a + 1] : NULL, &w) ||
                w == 0u || w > OWSCREENSHOT_MAX_W) {
                owx_puts("owscreenshot: -w must be 1..128\r\n");
                return 2;
            }
            a++;
        } else if (owscreenshot_streq(opt, "-H") || owscreenshot_streq(opt, "--height")) {
            if (!owscreenshot_parse_u32((a + 1 < argc) ? argv[a + 1] : NULL, &h) ||
                h == 0u || h > OWSCREENSHOT_MAX_H) {
                owx_puts("owscreenshot: -H must be 1..96\r\n");
                return 2;
            }
            a++;
        } else if (owscreenshot_streq(opt, "-p") || owscreenshot_streq(opt, "--pattern")) {
            if (!owscreenshot_parse_u32((a + 1 < argc) ? argv[a + 1] : NULL, &pattern) ||
                pattern > OWSCREENSHOT_PAT_RINGS) {
                owx_puts("owscreenshot: -p must be 0 (gradient), 1 (checker), 2 (rings)\r\n");
                return 2;
            }
            a++;
        } else if (owscreenshot_streq(opt, "-f") || owscreenshot_streq(opt, "--file")) {
            if (a + 1 >= argc || argv[a + 1] == NULL) {
                owx_puts("owscreenshot: -f requires a path\r\n");
                return 2;
            }
            path = argv[++a];
        } else if (owscreenshot_streq(opt, "--dump")) {
            dump = true;
        } else if (owscreenshot_streq(opt, "-h") || owscreenshot_streq(opt, "--help")) {
            owscreenshot_usage();
            return 0;
        } else {
            owx_puts("owscreenshot: unknown option '");
            owx_puts(opt);
            owx_puts("'\r\n");
            owscreenshot_usage();
            return 2;
        }
    }

    if (path != NULL) {
        uint32_t fw = 0u;
        uint32_t fh = 0u;
        int32_t rc = owscreenshot_load_file(path, &fw, &fh);
        if (rc < 0) {
            owx_puts("owscreenshot: cannot use '");
            owx_puts(path);
            owx_puts("' (code ");
            owx_put_i64((int64_t)rc);
            owx_puts(")\r\n");
            return 3;
        }
        w = fw;
        h = fh;
    } else {
        owscreenshot_pattern(w, h, pattern);
    }

    size = owscreenshot_build_bmp(w, h);

    owx_puts("owscreenshot: captured ");
    owx_put_u64((uint64_t)w);
    owx_putc('x');
    owx_put_u64((uint64_t)h);
    owx_puts(" source=");
    owx_puts(path != NULL ? path : "pattern");
    if (path == NULL) {
        owx_puts(pattern == OWSCREENSHOT_PAT_CHECKER ? " (checker)" :
                 pattern == OWSCREENSHOT_PAT_RINGS ? " (rings)" : " (gradient)");
    }
    owx_puts("\r\n");

    owx_puts("owscreenshot: encoded BMP size=");
    owx_put_u64((uint64_t)size);
    owx_puts(" bytes (24-bpp, bottom-up) checksum=0x");
    {
        static const char digits[] = "0123456789abcdef";
        int shift;
        uint32_t sum = owscreenshot_checksum(size);
        for (shift = 28; shift >= 0; shift -= 4) {
            owx_putc(digits[(sum >> shift) & 0xFu]);
        }
    }
    owx_puts("\r\n");

    if (dump) {
        owx_puts("owscreenshot: header dump (54 bytes):\r\n");
        for (i = 0u; i < OWSCREENSHOT_HEADER; i++) {
            if (i % 16u == 0u) {
                owx_puts("  ");
                owx_put_u64((uint64_t)i);
                owx_puts(": ");
            }
            owscreenshot_put_hex2(s_bmp[i]);
            owx_putc(' ');
            if (i % 16u == 15u) {
                owx_puts("\r\n");
            }
        }
        if (OWSCREENSHOT_HEADER % 16u != 0u) {
            owx_puts("\r\n");
        }
    }

    return 0;
}

static void owscreenshot_usage(void)
{
    owx_puts(
        "usage: owscreenshot [-w width] [-H height] [-p pattern] [-f file] [--dump] [-h]\r\n"
        "  -w, --width N     capture width, 1..128 (default 64)\r\n"
        "  -H, --height N    capture height, 1..96 (default 48)\r\n"
        "  -p, --pattern N   0 gradient, 1 checker, 2 rings (default 0)\r\n"
        "  -f, --file P      capture the pixels of an OWIMG file instead\r\n"
        "  --dump            hex-dump the encoded 54-byte BMP header\r\n"
        "  -h, --help        show this help\r\n");
}
