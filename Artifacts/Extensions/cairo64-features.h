/*
 * cairo-features.h — OpenWindows freestanding build configuration
 * for the genuine cairo-1.18.2 core image renderer.
 *
 * This file is generated (in spirit) by cairo's meson make-features step,
 * restricted to the feature set required by the OpenWindows platform:
 *   image surfaces, recording surfaces, user fonts, observer/mime support.
 * All display backends (xlib/xcb/quartz/win32), font backends (ft/dwrite/fc),
 * raster backends (pdf/ps/svg/script/png) and simd acceleration are disabled.
 */
#ifndef CAIRO_FEATURES_H
#define CAIRO_FEATURES_H

#define CAIRO_VERSION_MAJOR 1
#define CAIRO_VERSION_MINOR 18
#define CAIRO_VERSION_MICRO 2

#define CAIRO_HAS_DEFLATE_STREAM 0
#define CAIRO_HAS_DWRITE_FONT 0
#define CAIRO_HAS_FC_FONT 0
#define CAIRO_HAS_FONT_SUBSET 0
#define CAIRO_HAS_FT_FONT 0
#define CAIRO_HAS_HIDDEN_SYMBOLS 0
#define CAIRO_HAS_IMAGE_SURFACE 1
#define CAIRO_HAS_MIME_SURFACE 1
#define CAIRO_HAS_OBSERVER_SURFACE 1
#define CAIRO_HAS_PDF_OPERATORS 0
#define CAIRO_HAS_PDF_SURFACE 0
#define CAIRO_HAS_PNG_FUNCTIONS 0
#define CAIRO_HAS_PS_SURFACE 0
#define CAIRO_HAS_PTHREAD 0
#define CAIRO_HAS_QUARTZ_FONT 0
#define CAIRO_HAS_QUARTZ_IMAGE_SURFACE 0
#define CAIRO_HAS_QUARTZ_SURFACE 0
#define CAIRO_HAS_RECORDING_SURFACE 1
#define CAIRO_HAS_RASTER_SOURCE_PATTERN 1
#define CAIRO_HAS_SCRIPT_SURFACE 0
#define CAIRO_HAS_SVG_SURFACE 0
#define CAIRO_HAS_TEE_SURFACE 0
#define CAIRO_HAS_TEST_PAGINATED_SURFACE 0
#define CAIRO_HAS_USER_FONT 1
#define CAIRO_HAS_UTF8_TO_UTF16 1
#define CAIRO_HAS_WIN32_FONT 0
#define CAIRO_HAS_WIN32_SURFACE 0
#define CAIRO_HAS_XCB_SHM_FUNCTIONS 0
#define CAIRO_HAS_XCB_SURFACE 0
#define CAIRO_HAS_XLIB_SURFACE 0
#define CAIRO_HAS_XLIB_XCB_FUNCTIONS 0
#define CAIRO_HAS_XLIB_XRENDER_SURFACE 0

#endif /* CAIRO_FEATURES_H */