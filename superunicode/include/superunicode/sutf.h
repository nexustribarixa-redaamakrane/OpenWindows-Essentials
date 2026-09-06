#ifndef SUPERUNICODE_SUTF_H
#define SUPERUNICODE_SUTF_H

#include "sucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encodes a 31-bit SUCS codepoint into a SUTF byte sequence (1 to 6 bytes).
 *
 * @param cp                The 31-bit SUCS codepoint (0x00000000 to 0x7FFFFFFF).
 * @param out_buf           Pointer to destination buffer.
 * @param buf_size          Size of destination buffer in bytes.
 * @param out_bytes_written Pointer to store the number of bytes written.
 * @return                  SUES_SUCCESS (0) on success, or a negative error status.
 */
int sutf_encode_char(sucs_char_t cp, char* out_buf, size_t buf_size, size_t* out_bytes_written);

/**
 * Decodes a SUTF byte sequence (1 to 6 bytes) into a 31-bit SUCS codepoint.
 *
 * @param in_buf         Pointer to input SUTF byte buffer.
 * @param buf_size       Size of input buffer in bytes.
 * @param out_cp         Pointer to store decoded 31-bit SUCS codepoint.
 * @param out_bytes_read Pointer to store the number of bytes consumed.
 * @return               SUES_SUCCESS (0) on success, or a negative error status.
 */
int sutf_decode_char(const char* in_buf, size_t buf_size, sucs_char_t* out_cp, size_t* out_bytes_read);

#ifdef __cplusplus
}
#endif

#endif /* SUPERUNICODE_SUTF_H */
