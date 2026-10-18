/*
 * sfontview.c - OpenWindows SuperUnicode Font (.suf) Previewer (.owx)
 *
 * Exercises DLL/sufrender (linked statically into this image): a font face is
 * filled from a built-in 8x16 1bpp sample glyph, the glyph is fetched back
 * through the cache, and -- on request -- rasterised through
 * sufrender_blit_char() into a small framebuffer that is read back and
 * printed.  Conforms to the OpenWindows non-POSIX verb-driven CLI convention:
 *
 *   sfontview              preview the sample glyph and report face stats
 *   sfontview show <cp>    print glyph <cp> as ASCII art
 *   sfontview blit <cp>    rasterise <cp> into a framebuffer and print it
 *   sfontview info         print face statistics
 *   sfontview help         show usage information
 *
 * <cp> is a decimal codepoint or 0x-prefixed hex.  This build carries one
 * sample glyph (U+0041, 'A'), so any other codepoint is reported as uncached
 * rather than silently rendered as a blank box.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"
#include "../../DLL/sufrender/sufrender.h"

/* 8x16 'A': rows are bytes, MSB leftmost, exactly the layout
 * sufrender_add_glyph() caches and sufrender_blit_char() rasterises. */
static const uint8_t g_SampleGlyphA[SUF_GLYPH_HEIGHT] = {
    0x00u, 0x18u, 0x3Cu, 0x3Cu, 0x66u, 0x66u, 0x7Eu, 0x66u,
    0xC3u, 0xC3u, 0xC3u, 0xC3u, 0x00u, 0x00u, 0x00u, 0x00u
};

static suf_font_face_t g_Face;

/* Wide enough for one glyph (8 pixels * 4 bytes each), read back after the
 * blit so the printed art is what the rasteriser actually wrote. */
#define SUFVIEW_FB_PIXELS (SUF_GLYPH_WIDTH * SUF_GLYPH_HEIGHT)
static uint32_t g_Framebuffer[SUFVIEW_FB_PIXELS];

#define SUFVIEW_FG 0x00FFFFFFu
#define SUFVIEW_BG 0x00000000u

/* ---------------------------------------------------------------- */
/* Helpers                                                          */
/* ---------------------------------------------------------------- */

static bool SufviewStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static int SufviewHexVal(char c)
{
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (c >= 'a' && c <= 'f') return (int)(c - 'a') + 10;
    if (c >= 'A' && c <= 'F') return (int)(c - 'A') + 10;
    return -1;
}

/* Decimal by default, 0x/0X prefixed hex accepted; false on an empty or
 * partially numeric token so a typo is an error, not codepoint 0. */
static bool SufviewParseCodepoint(const char *s, uint32_t *out)
{
    uint32_t value = 0u;

    if (!s || !*s || !out) return false;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        if (*s == '\0') return false;
        while (*s != '\0') {
            int d = SufviewHexVal(*s);
            if (d < 0) return false;
            value = (value << 4) | (uint32_t)d;
            s++;
        }
    } else {
        while (*s != '\0') {
            int d = SufviewHexVal(*s);
            if (d < 0 || d > 9) return false;
            value = (value * 10u) + (uint32_t)d;
            s++;
        }
    }

    *out = value;
    return true;
}

static void SufviewPrintUsage(void)
{
    owx_puts("sfontview: OpenWindows SuperUnicode font previewer\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  sfontview              preview the sample glyph\r\n");
    owx_puts("  sfontview show <cp>    print glyph <cp> (decimal or 0x hex)\r\n");
    owx_puts("  sfontview blit <cp>    rasterise <cp> into a framebuffer\r\n");
    owx_puts("  sfontview info         print face statistics\r\n");
    owx_puts("  sfontview help         show this message\r\n");
}

/* The face is BSS, so the loader hands it over zeroed; init + one add makes
 * the cached state exactly the sample glyph and nothing else. */
static void SufviewLoadFace(void)
{
    sufrender_init(&g_Face);
    (void)sufrender_add_glyph(&g_Face, 0x0041u, g_SampleGlyphA);
}

static void SufviewPrintGlyph(const uint8_t bitmap[SUF_GLYPH_HEIGHT])
{
    uint32_t row;
    uint32_t col;

    for (row = 0u; row < SUF_GLYPH_HEIGHT; row++) {
        owx_puts("    ");
        for (col = 0u; col < SUF_GLYPH_WIDTH; col++) {
            owx_putc((bitmap[row] & (uint8_t)(0x80u >> col)) ? '#' : '.');
        }
        owx_puts("\r\n");
    }
}

/* ---------------------------------------------------------------- */
/* Verbs                                                            */
/* ---------------------------------------------------------------- */

static int SufviewShow(uint32_t cp)
{
    const uint8_t *glyph = sufrender_get_glyph(&g_Face, cp);

    if (!glyph) {
        owx_puts("sfontview: codepoint ");
        owx_put_u64((uint64_t)cp);
        owx_puts(" is not cached in this face\r\n");
        return 1;
    }

    owx_puts("sfontview: codepoint ");
    owx_put_u64((uint64_t)cp);
    owx_puts(" (");
    owx_putc((cp >= 0x20u && cp < 0x7Fu) ? (char)cp : '?');
    owx_puts(")\r\n");
    SufviewPrintGlyph(glyph);
    return 0;
}

/*
 * Rasterise through the library's blit path, then read the framebuffer back:
 * what is printed afterwards is the pixel memory sufrender_blit_char wrote,
 * not the source bitmap re-printed.
 */
static int SufviewBlit(uint32_t cp)
{
    const uint8_t *glyph = sufrender_get_glyph(&g_Face, cp);
    uint32_t i;
    uint32_t painted = 0u;

    if (!glyph) {
        owx_puts("sfontview: codepoint ");
        owx_put_u64((uint64_t)cp);
        owx_puts(" is not cached in this face\r\n");
        return 1;
    }

    for (i = 0u; i < SUFVIEW_FB_PIXELS; i++) {
        g_Framebuffer[i] = SUFVIEW_BG;
    }

    sufrender_blit_char(g_Framebuffer, SUF_GLYPH_WIDTH * 4u,
                        0, 0, glyph, SUFVIEW_FG, SUFVIEW_BG);

    owx_puts("sfontview: framebuffer after blit of codepoint ");
    owx_put_u64((uint64_t)cp);
    owx_puts("\r\n");
    for (i = 0u; i < SUFVIEW_FB_PIXELS; i++) {
        if ((i % SUF_GLYPH_WIDTH) == 0u) {
            owx_puts("    ");
        }
        if (g_Framebuffer[i] == SUFVIEW_FG) {
            owx_putc('#');
            painted++;
        } else {
            owx_putc('.');
        }
        if ((i % SUF_GLYPH_WIDTH) == (SUF_GLYPH_WIDTH - 1u)) {
            owx_puts("\r\n");
        }
    }

    owx_puts("sfontview: ");
    owx_put_u64((uint64_t)painted);
    owx_puts(" foreground pixel(s) of ");
    owx_put_u64((uint64_t)SUFVIEW_FB_PIXELS);
    owx_puts("\r\n");
    return (painted > 0u) ? 0 : 1;
}

static void SufviewInfo(void)
{
    owx_puts("sfontview: face holds ");
    owx_put_u64((uint64_t)g_Face.glyph_count);
    owx_puts(" of ");
    owx_put_u64((uint64_t)SUF_CACHED_GLYPHS);
    owx_puts(" cached glyph(s); glyph cell is ");
    owx_put_u64((uint64_t)SUF_GLYPH_WIDTH);
    owx_puts("x");
    owx_put_u64((uint64_t)SUF_GLYPH_HEIGHT);
    owx_puts(" 1bpp\r\n");
}

/* ---------------------------------------------------------------- */
/* Entry point                                                      */
/* ---------------------------------------------------------------- */

int sfontview_main(int argc, const char *const *argv)
{
    uint32_t cp = 0x0041u;

    SufviewLoadFace();

    if (argc < 2 || !argv || !argv[1]) {
        SufviewInfo();
        return SufviewShow(cp);
    }

    if (SufviewStrEqual(argv[1], "help") ||
        SufviewStrEqual(argv[1], "-h") ||
        SufviewStrEqual(argv[1], "--help")) {
        SufviewPrintUsage();
        return 0;
    }
    if (SufviewStrEqual(argv[1], "info")) {
        SufviewInfo();
        return 0;
    }
    if (SufviewStrEqual(argv[1], "show") || SufviewStrEqual(argv[1], "blit")) {
        bool blit = SufviewStrEqual(argv[1], "blit");

        if (argc < 3 || !argv[2]) {
            owx_puts("sfontview: show/blit: missing <cp>\r\n");
            return 1;
        }
        if (!SufviewParseCodepoint(argv[2], &cp)) {
            owx_puts("sfontview: '");
            owx_puts(argv[2]);
            owx_puts("' is not a codepoint\r\n");
            return 1;
        }
        return blit ? SufviewBlit(cp) : SufviewShow(cp);
    }

    owx_puts("sfontview: unknown verb '");
    owx_puts(argv[1]);
    owx_puts("'\r\n");
    SufviewPrintUsage();
    return 2;
}
