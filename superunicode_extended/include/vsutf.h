#ifndef VSUTF_H
#define VSUTF_H

/**
 * vSUTF (Variable SUTF) Transformation Format
 *
 * vSUTF is strictly a TRANSFORMATION FORMAT defining the variable-length
 * multi-byte mapping between ExtSUCS character encoding codepoints and
 * byte sequences, spanning the full unbounded (0 -> infinity) address space.
 *
 * Stream Structure:
 * - Codepoints 0x00000000 to 0x7FFFFFFF (Base SUCS fast-path):
 *   Encoded using standard SUTF-8 transport framing (1 to 6 bytes).
 *
 * - Codepoints 0x80000000 to 0xFFFFFFFFFFFFFFFF (ExtSUCS extended range):
 *   Encoded with extended prefix headers:
 *     0xFE prefix + 8 bytes big-endian = 9 bytes total (full 64-bit transport)
 *
 * - 0xFF prefix: Reserved for future extension beyond 64-bit implementations.
 *
 * Note: 0xFE and 0xFF are unused in SUTF-8 (which uses 0xC0-0xFD for lead
 * bytes), making them safe extension markers for vSUTF extended framing.
 */

#include "extsucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* vSUTF Transport Stream Constants */
#define VSUTF_EXT64_PREFIX   ((uint8_t)0xFE)  /* 64-bit extended prefix */
#define VSUTF_RESERVED_PREFIX ((uint8_t)0xFF)  /* Reserved for future >64-bit */
#define VSUTF_EXT64_TOTAL     9                /* 1 prefix + 8 payload bytes */
#define VSUTF_MAX_BYTES       9                /* Maximum bytes per codepoint */

/**
 * Returns the vSUTF transport stream byte length for a given ExtSUCS codepoint.
 * Returns 0 if the codepoint is in the inherited trap range.
 */
static inline size_t vsutf_codepoint_length(sucs_ex_char_t ex_cp) {
    if (!extsucs_is_valid(ex_cp)) {
        return 0;
    }
    if (ex_cp <= 0x7FULL) {
        return 1;
    } else if (ex_cp <= 0x7FFULL) {
        return 2;
    } else if (ex_cp <= 0xFFFFULL) {
        return 3;
    } else if (ex_cp <= 0x0010FFFFULL) {
        /* 4-byte framing covers only the Unicode-compatible range */
        return 4;
    } else if (ex_cp <= 0x03FFFFFFULL) {
        /* 5-byte framing covers native extended space (0x110000-0x3FFFFFF) */
        return 5;
    } else if (ex_cp <= 0x7FFFFFFFULL) {
        return 6;
    } else {
        return VSUTF_EXT64_TOTAL; /* 0xFE prefix + 8 bytes */
    }
}

/**
 * Encodes an ExtSUCS codepoint into the vSUTF variable-length transport stream.
 * Base SUCS range (0x00-0x7FFFFFFF): SUTF-8 framing (1-6 bytes).
 * Extended range (>0x7FFFFFFF): 0xFE prefix + 8-byte big-endian (9 bytes).
 * Returns bytes written, or 0 on error.
 */
size_t vsutf_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size);

/**
 * Decodes a vSUTF transport stream into an ExtSUCS character encoding codepoint.
 * Returns bytes consumed, or 0 on error (out_cp left unchanged on failure).
 */
size_t vsutf_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp);

#ifdef __cplusplus
}
#endif

#endif /* VSUTF_H */
