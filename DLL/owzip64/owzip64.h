/*
 * owzip64.h - OpenWindows Data Compression Library (.owd)
 *
 * Provides a self-contained LZSS (LZ77 with explicit flag bits) codec plus
 * the two classic integrity checksums used by OWFS/USFS streams:
 *   - CRC32  (IEEE 802.3, reflected polynomial 0xEDB88320)
 *   - Adler-32 (RFC 1950)
 *
 * All operations work in caller-provided buffers. No heap, no libc.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWZIP64_H
#define OWZIP64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* LZSS window geometry (fixed 12-bit back-references). */
#define OWZIP_WINDOW_SIZE   4096u   /* Maximum back-reference distance */
#define OWZIP_MIN_MATCH     3u      /* Shortest encodable match        */
#define OWZIP_MAX_MATCH     18u     /* Longest encodable match         */

/*
 * Compress `src_len` bytes from `src` into `dst` (capacity `dst_cap`).
 *
 * Returns true on success and stores the compressed byte count in `*out_len`.
 * Returns false if `dst` is too small or an argument is invalid.
 */
bool owzip_compress(const uint8_t *src, size_t src_len,
                    uint8_t *dst, size_t dst_cap, size_t *out_len);

/*
 * Decompress `src_len` bytes from `src` into `dst` (capacity `dst_cap`).
 *
 * Returns true on success and stores the decompressed byte count in `*out_len`.
 * Returns false on malformed input, truncation, or output overflow.
 */
bool owzip_decompress(const uint8_t *src, size_t src_len,
                      uint8_t *dst, size_t dst_cap, size_t *out_len);

/*
 * Running CRC32 (IEEE). Pass 0 for the first call to obtain the standard
 * checksum; pass the previous return value to continue across buffers.
 */
uint32_t owzip_crc32(uint32_t crc, const void *data, size_t len);

/*
 * Running Adler-32. The initial value is 1 (pass 1 for the first call);
 * pass the previous return value to continue across buffers.
 */
uint32_t owzip_adler32(uint32_t adler, const void *data, size_t len);

#endif /* OWZIP64_H */
