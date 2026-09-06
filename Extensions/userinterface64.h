/*
 * userinterface64.h - userinterface64.owd ABI mirror (compositor / controls)
 *
 * Freestanding C99 ABI for the userinterface64 module: a light compositor
 * over the genuine cairo engine exported by cairo64.owd. A UI window is an
 * ARGB32 back buffer the client can draw into with any cairo context or a
 * cairoplus64 text layer; window_present() composites it onto a target
 * surface with source-over and clipped to the target bounds.
 *
 * Geometry: window origin is top-left; present(dx,dy) places the window's
 * top-left corner at (dx,dy) in target space.
 */

#ifndef OWE_USERINTERRACE64_H
#define OWE_USERINTERRACE64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cairo64.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Status Codes - BANcode mapped                                      */
/*  B+ (0x0011A280-0x0011A2FF): Fatal module faults                    */
/*  W+ (0x0011AAC0-0x0011AADF): Non-fatal degradations                 */
/*  S+ (0x0011AE80-0x0011AE9F): Soft / recoverable                     */
/* ------------------------------------------------------------------ */

typedef uint32_t userinterface64_status_t;

#define UI64_OK                            0x00000000u  /* Success        */
/* B+ Fatal */
#define UI64_BAN_K64_BOOT                  0x0011A280u  /* k64 API dead   */
#define UI64_BAN_SURFACE_FAULT             0x0011A281u  /* backbuffer NG  */
#define UI64_BAN_CTX_FAULT                 0x0011A282u  /* gstate NG      */
/* W+ Warning */
#define UI64_ERR_CLIPPED                   0x0011AAC0u  /* composite cut  */
/* S+ Soft */
#define UI64_ERR_UNINITIALIZED             0x0011AE80u  /* Not init yet   */
#define UI64_ERR_BAD_WINDOW                0x0011AE81u  /* Window invalid */
#define UI64_ERR_BAD_TARGET                0x0011AE82u  /* Target surface */

/* ------------------------------------------------------------------ */
/*  Window (compositor back buffer)                                    */
/* ------------------------------------------------------------------ */

typedef struct userinterface64_window userinterface64_window_t;

/*
 * Create an ARGB32 back buffer w x h, filled with bg_argb. Returns NULL on
 * error (bad args, allocation failure).
 */
userinterface64_window_t *
userinterface64_window_create(uint32_t w, uint32_t h, uint32_t bg_argb);

/* Tear down a window. NULL is a no-op. */
void userinterface64_window_destroy(userinterface64_window_t *win);

/* Repaint the whole buffer with one solid color. */
userinterface64_status_t
userinterface64_window_clear(userinterface64_window_t *win, uint32_t argb);

/*
 * The window's live ARGB32 image surface. The client may draw onto it with
 * any cairo context, then call present() to composite. Caller must not
 * destroy it (owned by the window).
 */
cairo_surface_t *
userinterface64_window_surface(userinterface64_window_t *win);

/*
 * Composite the window over `target` with source-over; the window's
 * top-left corner lands at (dx, dy). Fully off-target placement is clipped
 * by cairo; the result is reported UI64_ERR_CLIPPED when any part was cut.
 */
userinterface64_status_t
userinterface64_window_present(userinterface64_window_t *win,
                               cairo_surface_t *target,
                               int dx, int dy);

/* Window geometry. */
uint32_t userinterface64_window_width(const userinterface64_window_t *win);
uint32_t userinterface64_window_height(const userinterface64_window_t *win);

/* ------------------------------------------------------------------ */
/*  Module lifecycle & identity                                        */
/* ------------------------------------------------------------------ */

userinterface64_status_t userinterface64_module_init(void);
userinterface64_status_t userinterface64_module_shutdown(void);
uint32_t userinterface64_abi_version(void);
uint16_t userinterface64_abi_major(void);
uint16_t userinterface64_abi_minor(void);
const uint8_t *userinterface64_ident(void);

#ifdef __cplusplus
}
#endif

#endif /* OWE_USERINTERRACE64_H */