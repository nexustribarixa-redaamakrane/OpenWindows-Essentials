#ifndef SUAS_SUAS_SUCD_H
#define SUAS_SUAS_SUCD_H

/* SUCD BiDi property accessor — SUAS-001 Structural Directional Framing.
 *
 * Provides the bitfield masks and range lookup used by the SDF
 * Dual-Mode Directional Resolver for Unicode-Bridge codepoints.
 * Normative table: sucd/Props/BidiProps.txt.
 *
 * Freestanding C99; no heap.
 */

#include <stdint.h>

#include "suas/suas_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── BiDi property bitfield masks ──────────────────────────────── */

#define SUCD_BIDI_LTR        (1u << 0)  /* strong left-to-right */
#define SUCD_BIDI_RTL        (1u << 1)  /* strong right-to-left */
#define SUCD_BIDI_ARABIC_AL  (1u << 2)  /* arabic letter, resolves right-to-left */
#define SUCD_BIDI_NEUTRAL    (1u << 3)  /* direction-neutral */
#define SUCD_BIDI_MIRRORED   (1u << 4)  /* paired-format glyph, auto-mirror */
#define SUCD_BIDI_WHITESPACE (1u << 5)  /* separator / whitespace, neutral */

#define SUCD_BIDI_ALL (SUCD_BIDI_LTR | SUCD_BIDI_RTL | SUCD_BIDI_ARABIC_AL | \
                       SUCD_BIDI_NEUTRAL | SUCD_BIDI_MIRRORED | SUCD_BIDI_WHITESPACE)

/* ── Lookup ────────────────────────────────────────────────────── */

/**
 * Returns the SUCD BiDi property bitmask for a codepoint in the Unicode
 * Bridge (0x00000000-0x0010FFFF). Codepoints outside the bridge, or any
 * unmapped codepoint, return SUCD_BIDI_NEUTRAL.
 *
 * The lookup is a compile-time-embedded subset of sucd/Props/BidiProps.txt
 * covering Latin, Arabic, Hebrew, and the mirrorable punctuation pairs.
 */
uint32_t suas_sucd_bidi(uint32_t cp);

/* ── Zone classification helpers ───────────────────────────────── */

static inline int suas_sucd_is_unicode_bridge(uint32_t cp)
{
    return (cp <= 0x0010FFFFUL);
}

static inline int suas_sucd_is_scp(uint32_t cp)
{
    return (cp >= SUAS_ZONE_SCP_MIN && cp <= SUAS_ZONE_SCP_MAX);
}

static inline int suas_sucd_is_native(uint32_t cp)
{
    return (cp >= SUAS_ZONE_NATIVE_MIN && cp <= SUAS_ZONE_NATIVE_MAX);
}

#ifdef __cplusplus
}
#endif

#endif /* SUAS_SUAS_SUCD_H */
