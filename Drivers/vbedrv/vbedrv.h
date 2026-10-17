/*
 * vbedrv.h - OpenWindows VESA BIOS Extensions Driver (.owc)
 *
 * Models a VBE linear-framebuffer mode database plus the currently
 * selected mode. A real-mode BIOS INT 10h is not reachable from long
 * mode, so the mode set is populated from a caller-provided capability
 * descriptor and every candidate mode is fully validated before use.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef VBEDRV_H
#define VBEDRV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t vbe_status_t;

#define VBE_OK                    0x00000000u  /* Success              */
#define VBE_ERR_INVALID_PARAM     0x0011B000u  /* Bad pointer/geometry */
#define VBE_ERR_NOT_INITIALIZED   0x0011B001u  /* vbe_init() not run   */
#define VBE_ERR_TABLE_FULL        0x0011B002u  /* Mode database full   */
#define VBE_ERR_NO_SUCH_MODE      0x0011B003u  /* Mode number unknown  */
#define VBE_ERR_UNSUPPORTED       0x0011B004u  /* Format/bpp mismatch  */
#define VBE_ERR_NO_MODE_SELECTED  0x0011B005u  /* No active mode       */

/* ── Capacity limits ──────────────────────────────────────────── */

#define VBE_MAX_MODES             64u          /* Database capacity    */

/* ── Pixel formats ────────────────────────────────────────────── */

typedef enum {
    VBE_FMT_UNKNOWN  = 0,   /* Untyped / not usable          */
    VBE_FMT_INDEX8   = 1,   /* 8bpp palettised               */
    VBE_FMT_RGB565   = 2,   /* 5-6-5 direct colour           */
    VBE_FMT_RGB888   = 3,   /* 8-8-8 packed direct colour    */
    VBE_FMT_XRGB8888 = 4    /* 8-8-8 direct with pad byte    */
} vbe_format_t;

/* ── Mode descriptor ──────────────────────────────────────────── */

/* Complete geometry for one linear-framebuffer mode. */
typedef struct {
    uint32_t     mode_number;  /* VBE mode identifier                  */
    uint16_t     width;        /* Visible width in pixels              */
    uint16_t     height;       /* Visible height in pixels             */
    uint16_t     bpp;          /* Bits per pixel: 8, 16, 24 or 32      */
    uint16_t     pitch;        /* Bytes per scan line                  */
    uint64_t     framebuffer;  /* Physical linear-framebuffer base     */
    vbe_format_t format;       /* Pixel layout                         */
    uint8_t      red_size;     /* Red channel width in bits            */
    uint8_t      red_pos;      /* Red channel bit position             */
    uint8_t      green_size;   /* Green channel width in bits          */
    uint8_t      green_pos;    /* Green channel bit position           */
    uint8_t      blue_size;    /* Blue channel width in bits           */
    uint8_t      blue_pos;     /* Blue channel bit position            */
} vbe_mode_info_t;

/* Controller capabilities supplied by the caller (boot VBE probe). */
typedef struct {
    uint16_t vbe_version;         /* e.g. 0x0300 for VBE 3.0           */
    uint16_t advertised_modes;    /* Controller-reported mode count    */
    uint32_t memory_kb;           /* Total video memory in KiB         */
    uint64_t framebuffer;         /* Linear-framebuffer base address   */
    bool     linear_framebuffer;  /* LFB bit was advertised            */
} vbe_caps_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Initialise the driver and reset the mode database from a caps block. */
vbe_status_t vbe_init(const vbe_caps_t *caps);

/* Register (or replace) one validated mode in the static database. */
vbe_status_t vbe_add_mode(const vbe_mode_info_t *info);

/* Select a mode: adopts `info` when non-NULL, otherwise looks up the
 * stored entry for `mode_number`. Validates before committing. */
vbe_status_t vbe_set_mode(uint32_t mode_number, const vbe_mode_info_t *info);

/* Return a volatile pointer to the selected linear framebuffer (or NULL). */
volatile uint8_t *vbe_get_framebuffer(void);

/* Copy the selected mode descriptor into `out_info`. */
bool vbe_get_info(vbe_mode_info_t *out_info);

/* Number of modes currently held in the database. */
uint32_t vbe_mode_count(void);

#endif /* VBEDRV_H */
