/*
 * entropy.h - OpenWindows Hardware Entropy Source (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 * A mixing pool backed by the CPU RDRAND instruction when available.
 */

#ifndef ENTROPY_H
#define ENTROPY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t entropy_status_t;

#define ENTROPY_OK              0x00000000u  /* Request satisfied        */
#define ENTROPY_ERR_NOT_INIT    0x00000001u  /* entropy_init() not run   */
#define ENTROPY_ERR_INVALID     0x00000002u  /* NULL buffer / bad args   */

/* ── Configuration ────────────────────────────────────────────── */

/* Number of 32-bit words in the mixing pool (must be a power of two). */
#define ENTROPY_POOL_WORDS      32u

/* Largest byte count folded into the pool by one entropy_seed() call. */
#define ENTROPY_SEED_MAX        256u

/* Compile-time snapshot of the entropy subsystem. */
typedef struct {
    bool     rdrand;         /* RDRAND produced data during init         */
    bool     initialized;    /* entropy_init() has completed             */
    uint32_t pool_words;     /* size of the internal mixing pool         */
    uint32_t seed_bytes;     /* bytes folded in through entropy_seed()   */
} entropy_info_t;

/* ── API ──────────────────────────────────────────────────────── */

/* One-time initialisation; seeds and mixes the internal pool. */
bool entropy_init(void);

/* Fill a caller-owned buffer with entropy; returns a status code. */
entropy_status_t entropy_read(void *buffer, size_t length);

/* Report whether the source is ready to supply entropy. */
bool entropy_available(void);

/* Fold caller-supplied entropy into the pool. */
entropy_status_t entropy_seed(const void *data, size_t length);

/* Copy a description of the active entropy source into *out. */
void entropy_get_info(entropy_info_t *out);

#endif /* ENTROPY_H */
