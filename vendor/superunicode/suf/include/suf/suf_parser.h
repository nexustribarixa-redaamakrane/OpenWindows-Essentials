/**
 * @file suf_parser.h
 * @brief Zero-allocation Freestanding Parser for SuperUnicode Font (.suf) files
 *
 * Designed for early kernel boot (GOP/VBE console, BANcode crash screen) and
 * OS GUI variable font compositor engines.
 * Requires zero dynamic memory allocation and no standard library dependencies.
 */

#ifndef SUF_PARSER_H
#define SUF_PARSER_H

#include "suf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validates a .suf binary buffer and reads the header.
 */
suf_status_t suf_validate_header(const void *buffer, size_t size, suf_header_t *out_header);

/**
 * @brief Looks up a Glyph ID for a Base 31-bit SUCS codepoint.
 */
suf_status_t suf_lookup_glyph_id(const void *buffer, size_t size, uint32_t codepoint, uint32_t *out_glyph_id);

/**
 * @brief Looks up a Glyph ID for an ExtSUCS 64-bit codepoint.
 */
suf_status_t suf_lookup_glyph_id_ext(const void *buffer, size_t size, uint64_t codepoint, uint32_t *out_glyph_id);

/**
 * @brief Retrieves the 16-byte aligned metric descriptor for a glyph.
 */
suf_status_t suf_get_glyph_metric(const void *buffer, size_t size, uint32_t glyph_id, suf_metric_t *out_metric);

/**
 * @brief Gets a direct pointer to the pre-rendered boot bitmap data for a glyph.
 */
suf_status_t suf_get_boot_bitmap(const void *buffer, size_t size, uint32_t glyph_id,
                                 const uint8_t **out_bitmap_bytes, size_t *out_byte_count);

/**
 * @brief Gets a direct pointer to the vector outline instruction stream for a glyph.
 */
suf_status_t suf_get_glyph_outline(const void *buffer, size_t size, uint32_t glyph_id,
                                   const uint8_t **out_commands, size_t *out_cmd_size);

/**
 * @brief Gets a direct pointer to the raw (remapped) GlyphVariationData block for a glyph.
 *
 * Requires SUF_FLAG_GLYPH_VARIATIONS. A zero-length result means the glyph
 * carries no variation data (e.g. composites or unchanged glyphs).
 */
suf_status_t suf_get_glyph_variation(const void *buffer, size_t size, uint32_t glyph_id,
                                     const uint8_t **out_data, size_t *out_byte_count);

/**
 * @brief Returns the number of preserved font name records (0 if none).
 */
suf_status_t suf_get_name_count(const void *buffer, size_t size, uint32_t *out_count);

/**
 * @brief Gets a font name record by OpenType nameID.
 *
 * Returns a pointer into the input buffer (zero-allocation). The UTF-8
 * string is NOT NUL-terminated; use out_len. Common nameIDs:
 * 1=family, 2=subfamily, 3=uniqueID, 4=full name, 5=version, 6=PostScript.
 */
suf_status_t suf_get_name(const void *buffer, size_t size, uint16_t name_id,
                          const char **out_utf8, size_t *out_len);

/**
 * @brief Looks up horizontal kerning adjustment between two glyphs.
 */
int16_t suf_get_kerning(const void *buffer, size_t size, uint32_t left_glyph, uint32_t right_glyph);

/**
 * @brief Looks up a direct ligature replacement for two adjacent glyphs.
 */
uint32_t suf_lookup_ligature(const void *buffer, size_t size, uint32_t first_glyph, uint32_t second_glyph);

/**
 * @brief Renders an early boot bitmap glyph directly to a 32bpp linear framebuffer.
 */
suf_status_t suf_render_boot_glyph_to_fb(const void *buffer, size_t size, uint32_t glyph_id,
                                        uint32_t *fb, uint32_t fb_width, uint32_t fb_height,
                                        uint32_t fb_pitch_pixels, uint32_t x, uint32_t y,
                                        uint32_t fg_color, uint32_t bg_color);

/* ========================================================================= */
/* BANcode & Kernel Crash Diagnostics Extensions                             */
/* ========================================================================= */

/**
 * @brief Looks up a dedicated glyph for a Kernel BANcode / Trap codepoint.
 */
suf_status_t suf_lookup_bancode_glyph(const void *buffer, size_t size, uint32_t bancode_cp, uint32_t *out_glyph_id);

/**
 * @brief Renders a color-coded diagnostic BANcode crash badge directly to framebuffer.
 *
 * Automatically styles the badge border, background, and icon based on BANcode category:
 *   - Fatal B+ BANcodes: Glowing Crimson Red (0xFFFF2020)
 *   - Warning W+ WARNcodes: Warning Amber (0xFFFFBF00)
 *   - Communications C+ COMcode: Emerald Green (0xFF20DF80)
 *   - Soft Error S+ SOFTcode: Deep Cyan (0xFF20C0FF)
 *   - Kernel Security Traps: Neon Magenta (0xFFFF0080)
 */
suf_status_t suf_render_bancode_badge(const void *buffer, size_t size, uint32_t bancode_cp,
                                     uint32_t *fb, uint32_t fb_width, uint32_t fb_height,
                                     uint32_t fb_pitch_pixels, uint32_t x, uint32_t y);

/* ========================================================================= */
/* Variable Axes & Interpolation Extensions                                  */
/* ========================================================================= */

/**
 * @brief Returns total number of variable design axes in the font.
 */
suf_status_t suf_get_axis_count(const void *buffer, size_t size, uint32_t *out_count);

/**
 * @brief Retrieves information about a variable axis by index [0 .. count - 1].
 */
suf_status_t suf_get_axis_info(const void *buffer, size_t size, uint32_t axis_index, suf_var_axis_t *out_axis);

/**
 * @brief Retrieves information about a variable axis by 4-char tag (e.g. SUF_AXIS_WGHT).
 */
suf_status_t suf_find_axis_by_tag(const void *buffer, size_t size, uint32_t axis_tag, suf_var_axis_t *out_axis);

/**
 * @brief Interpolates glyph metrics continuously across active variable axes coordinates.
 *
 * @param buffer Pointer to .suf binary.
 * @param size Buffer size.
 * @param glyph_id Target glyph index.
 * @param axis_values Array of design coordinate floats corresponding to axes in order.
 * @param num_axes Number of elements in axis_values.
 * @param out_metric Receives interpolated 16-byte metric descriptor.
 */
suf_status_t suf_interpolate_glyph_metric(const void *buffer, size_t size, uint32_t glyph_id,
                                         const float *axis_values, uint32_t num_axes,
                                         suf_metric_t *out_metric);

/* ========================================================================= */
/* Modded SuperUnicode Plugin Font Extensions                                */
/* ========================================================================= */

/**
 * @brief Retrieves modded SuperUnicode plugin metadata from font container.
 */
suf_status_t suf_get_plugin_meta(const void *buffer, size_t size, suf_plugin_font_meta_t *out_meta);

#ifdef __cplusplus
}
#endif

#endif /* SUF_PARSER_H */
