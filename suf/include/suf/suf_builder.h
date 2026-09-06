/**
 * @file suf_builder.h
 * @brief Dynamic Serializer and Builder for SuperUnicode Font (.suf) files
 */

#ifndef SUF_BUILDER_H
#define SUF_BUILDER_H

#include "suf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct suf_builder suf_builder_t;

/**
 * @brief Creates an empty .suf builder instance.
 */
suf_builder_t *suf_builder_create(uint16_t units_per_em, int16_t ascender, int16_t descender, uint16_t flags);

/**
 * @brief Sets early boot console bitmap dimensions and format.
 */
void suf_builder_set_boot_params(suf_builder_t *b, uint8_t width, uint8_t height, uint8_t bpp);

/**
 * @brief Sets typographic line gap.
 */
void suf_builder_set_line_gap(suf_builder_t *b, int16_t line_gap);

/**
 * @brief Sets global font bounding box.
 */
void suf_builder_set_bbox(suf_builder_t *b, int16_t min_x, int16_t min_y, int16_t max_x, int16_t max_y);

/**
 * @brief Appends a glyph to the builder.
 */
uint32_t suf_builder_add_glyph(suf_builder_t *b, uint64_t codepoint, const suf_metric_t *metric,
                              const uint8_t *boot_bitmap, size_t bitmap_len,
                              const uint8_t *outline_cmds, size_t outline_len);

/**
 * @brief Adds a BANcode kernel panic / diagnostic glyph.
 */
uint32_t suf_builder_add_bancode_glyph(suf_builder_t *b, uint32_t bancode_cp, const suf_metric_t *metric,
                                      const uint8_t *boot_bitmap, size_t bitmap_len,
                                      const uint8_t *outline_cmds, size_t outline_len);

/**
 * @brief Adds a horizontal kerning pair.
 */
bool suf_builder_add_kerning(suf_builder_t *b, uint32_t left_glyph, uint32_t right_glyph, int16_t kerning);

/**
 * @brief Adds a direct ligature replacement.
 */
bool suf_builder_add_ligature(suf_builder_t *b, uint32_t first_glyph, uint32_t second_glyph, uint32_t replacement_glyph);

/**
 * @brief Registers a variable design axis (e.g. SUF_AXIS_WGHT, SUF_AXIS_WDTH, or custom tag).
 */
bool suf_builder_add_axis(suf_builder_t *b, uint32_t tag, const char *name, float min_val, float def_val, float max_val);

/**
 * @brief Attaches a raw (already remapped) GlyphVariationData block to a glyph.
 *
 * The block must be self-contained: embedded peak tuple, private packed point
 * numbers, no shared-tuple references. Sets SUF_FLAG_GLYPH_VARIATIONS.
 */
bool suf_builder_set_glyph_variation(suf_builder_t *b, uint32_t glyph_id,
                                     const uint8_t *data, size_t len);

/**
 * @brief Preserves a font name record (OpenType nameID semantics).
 *
 * Stored in an 'SNM1' blob so exports keep the source font's identity
 * (family, subfamily, PostScript name, version...) instead of generic
 * defaults. UTF-8 input; replaces any existing record with the same ID.
 */
bool suf_builder_set_name(suf_builder_t *b, uint16_t name_id, const char *utf8);

/**
 * @brief Sets modded SuperUnicode plugin metadata (for Extended Mode plugin fonts).
 */
bool suf_builder_set_plugin_meta(suf_builder_t *b, const char *plugin_id,
                                uint8_t ver_major, uint8_t ver_minor, uint8_t ver_patch,
                                uint32_t range_count);

/**
 * @brief Serializes the font into a self-contained .suf binary buffer.
 */
suf_status_t suf_builder_serialize(const suf_builder_t *b, uint8_t **out_buffer, size_t *out_size);

/**
 * @brief Serializes and writes directly to a .suf binary file.
 */
suf_status_t suf_builder_write_file(const suf_builder_t *b, const char *filepath);

/**
 * @brief Destroys and frees all resources associated with the builder.
 */
void suf_builder_free(suf_builder_t *b);

#ifdef __cplusplus
}
#endif

#endif /* SUF_BUILDER_H */
