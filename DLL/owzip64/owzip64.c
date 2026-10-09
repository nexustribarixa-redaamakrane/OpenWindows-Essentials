/*
 * owzip64.c - OpenWindows Data Compression Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owzip64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owzip64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owzip_compress(void)
{
    (void)owzip64_ready;
    /* TODO: implement owzip_compress */
}
void owzip_decompress(void)
{
    (void)owzip64_ready;
    /* TODO: implement owzip_decompress */
}
void owzip_crc32(void)
{
    (void)owzip64_ready;
    /* TODO: implement owzip_crc32 */
}
void owzip_adler32(void)
{
    (void)owzip64_ready;
    /* TODO: implement owzip_adler32 */
}

