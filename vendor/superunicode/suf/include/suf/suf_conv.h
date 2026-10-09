/**
 * @file suf_conv.h
 * @brief Bidirectional Font Conversion Engine for SuperUnicode Font (.suf)
 *
 * Supports bidirectional conversions between .suf and:
 *   - TrueType (.ttf)
 *   - OpenType CFF (.otf)
 *   - Web Open Font Format 1.0 (.woff)
 *   - FontForge Spline Font Database (.sfd)
 *   - Embedded OpenType (.eot)
 *   - PostScript Type 1 / PFA / PFB (.ps, .pfa, .pfb)
 *   - Unified Font Object (.ufo)
 *   - SuperUnicode System Plugin Font blobs (.scsp)
 */

#ifndef SUF_CONV_H
#define SUF_CONV_H

#include "suf_types.h"
#include "suf_builder.h"

#ifdef __cplusplus

extern "C" {
#endif

/* ========================================================================= */
/* Inbound Converters (Legacy & External Font Formats -> .suf)               */
/* ========================================================================= */

/**
 * @brief Converts TrueType (.ttf) binary data into a .suf builder.
 */
suf_status_t suf_conv_ttf_to_suf(const uint8_t *ttf_data, size_t ttf_size, suf_builder_t **out_builder);

/**
 * @brief Converts OpenType CFF (.otf) binary data into a .suf builder.
 */
suf_status_t suf_conv_otf_to_suf(const uint8_t *otf_data, size_t otf_size, suf_builder_t **out_builder);

/**
 * @brief Converts Web Open Font Format 1.0 (.woff) data into a .suf builder.
 */
suf_status_t suf_conv_woff_to_suf(const uint8_t *woff_data, size_t woff_size, suf_builder_t **out_builder);

/**
 * @brief Converts FontForge Spline Font Database (.sfd) text into a .suf builder.
 */
suf_status_t suf_conv_sfd_to_suf(const char *sfd_text, size_t sfd_len, suf_builder_t **out_builder);

/**
 * @brief Converts Embedded OpenType (.eot) container data into a .suf builder.
 */
suf_status_t suf_conv_eot_to_suf(const uint8_t *eot_data, size_t eot_size, suf_builder_t **out_builder);

/**
 * @brief Converts PostScript Type 1 / PFA ASCII font into a .suf builder.
 */
suf_status_t suf_conv_ps_to_suf(const char *ps_text, size_t ps_len, suf_builder_t **out_builder);

/**
 * @brief Converts PostScript Binary Type 1 (.pfb) into a .suf builder.
 */
suf_status_t suf_conv_pfb_to_suf(const uint8_t *pfb_data, size_t pfb_size, suf_builder_t **out_builder);

/**
 * @brief Converts Unified Font Object (.ufo) fontinfo.plist XML into a .suf builder.
 */
suf_status_t suf_conv_ufo_to_suf(const char *ufo_xml, size_t ufo_len, suf_builder_t **out_builder);


/* ========================================================================= */
/* Outbound Exporters (.suf -> Legacy Font Formats)                          */
/* ========================================================================= */

/**
 * @brief Exports .suf binary data to TrueType (.ttf) format.
 */
suf_status_t suf_conv_suf_to_ttf(const uint8_t *suf_data, size_t suf_size, uint8_t **out_ttf, size_t *out_ttf_size);

/**
 * @brief Exports .suf binary data to OpenType CFF (.otf) format.
 */
suf_status_t suf_conv_suf_to_otf(const uint8_t *suf_data, size_t suf_size, uint8_t **out_otf, size_t *out_otf_size);

/**
 * @brief Exports .suf binary data to Web Open Font Format 1.0 (.woff) container.
 */
suf_status_t suf_conv_suf_to_woff(const uint8_t *suf_data, size_t suf_size, uint8_t **out_woff, size_t *out_woff_size);

/**
 * @brief Exports .suf binary data to FontForge Spline Font Database (.sfd) text.
 */
suf_status_t suf_conv_suf_to_sfd(const uint8_t *suf_data, size_t suf_size, char **out_sfd, size_t *out_sfd_len);

/**
 * @brief Exports .suf binary data to Embedded OpenType (.eot) container.
 */
suf_status_t suf_conv_suf_to_eot(const uint8_t *suf_data, size_t suf_size, uint8_t **out_eot, size_t *out_eot_size);

/**
 * @brief Exports .suf binary data to PostScript Type 1 / PFA (.pfa / .ps) ASCII format.
 */
suf_status_t suf_conv_suf_to_ps(const uint8_t *suf_data, size_t suf_size, char **out_ps, size_t *out_ps_len);

/**
 * @brief Exports .suf binary data to PostScript Binary Type 1 (.pfb) format.
 */
suf_status_t suf_conv_suf_to_pfb(const uint8_t *suf_data, size_t suf_size, uint8_t **out_pfb, size_t *out_pfb_size);

/**
 * @brief Exports .suf binary data to Unified Font Object (.ufo) fontinfo.plist XML.
 */
suf_status_t suf_conv_suf_to_ufo(const uint8_t *suf_data, size_t suf_size, char **out_ufo, size_t *out_ufo_len);


/* ========================================================================= */
/* Modded SuperUnicode Plugin Font Packaging (Extended Mode Only)            */
/* ========================================================================= */

/**
 * @brief Packages a .suf font into an official SuperUnicode System Plugin blob (.scsp).
 *
 * Enforces SuperUnicode Extended Mode plugin constraints:
 *   - Blob magic: 0x53435343 ('SUCS')
 *   - Partition filesystem: OWFS
 *   - Dual Castagnoli CRC32c + Fletcher-64 integrity verification
 */
suf_status_t suf_conv_pack_plugin_font(const uint8_t *suf_data, size_t suf_size,
                                      const char *plugin_id,
                                      uint8_t ver_major, uint8_t ver_minor, uint8_t ver_patch,
                                      uint8_t **out_blob, size_t *out_blob_size);

/**
 * @brief Unpacks an embedded .suf font from a SuperUnicode System Plugin blob (.scsp).
 */
suf_status_t suf_conv_unpack_plugin_font(const uint8_t *blob_data, size_t blob_size,
                                        uint8_t **out_suf, size_t *out_suf_size);

#ifdef __cplusplus
}
#endif

#endif /* SUF_CONV_H */
