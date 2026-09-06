/*
 * owhash64.c - OpenWindows Hash Functions Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owhash64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owhash64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owhash_sha256_init(void)
{
    (void)owhash64_ready;
    /* TODO: implement owhash_sha256_init */
}
void owhash_sha256_update(void)
{
    (void)owhash64_ready;
    /* TODO: implement owhash_sha256_update */
}
void owhash_sha256_final(void)
{
    (void)owhash64_ready;
    /* TODO: implement owhash_sha256_final */
}
void owhash_md5(void)
{
    (void)owhash64_ready;
    /* TODO: implement owhash_md5 */
}
void owhash_crc32c(void)
{
    (void)owhash64_ready;
    /* TODO: implement owhash_crc32c */
}

