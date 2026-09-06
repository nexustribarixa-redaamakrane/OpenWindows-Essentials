#ifndef SUTF_MASTER_H
#define SUTF_MASTER_H

// IWYU pragma: export

/**
 * SuperUnicode Transformation Format (SUTF) Master Header
 *
 * SUTF and extSUTF are strictly TRANSFORMATION FORMATS. They define the
 * endian-neutral mapping between SUCS codepoints and symbol sequences
 * (byte words, hex nibbles, symbol frames). SUTF does NOT define physical
 * byte ordering, bit-packing, or stream framing — those live in the SUST
 * (SuperUnicode Serialization Transports) library, see <sust.h>.
 *
 * Included:
 * - SUTF-8:  1..6 Byte Stream Transformation
 * - SUTF-16: 1..2 16-Bit Word Transformation
 * - SUTF-4:  4-Bit Hex Nibble Transformation
 * - SUTF-2:  2-Bit Symbol Frame Transformation
 * - Kernel Mode-Switching: <sucs_mode.h> (Base <-> ExtSUCS system restart controller)
 *
 * Extended Transformation Formats (Forwarded if superunicode_extended is on include path):
 * - vSUTF: Variable Multi-Byte Streaming Transformation (<vsutf.h>)
 *
 * Serialization Transports (NOT included — see SUST library <sust.h>):
 * - SUST-16/32/64/128/256/512/N and e-SUST.
 *
 * SUST-16 requires this library's SUTF-16 word transformation; link both
 * libraries and add sust/ and superunicode_extended/ to the include path to
 * use the full serialization stack.
 */

// IWYU pragma: begin_exports
#include "sucs_types.h"    // IWYU pragma: export
#include "sucs_mode.h"     // IWYU pragma: export
#include "sutf8.h"         // IWYU pragma: export
#include "sutf16.h"        // IWYU pragma: export
#include "sutf4.h"         // IWYU pragma: export
#include "sutf2.h"         // IWYU pragma: export

/* Forwarding reference for the extended transformation format if superunicode_extended is on include path */
#if defined(__has_include)
  #if __has_include("vsutf.h")
    #include "vsutf.h" // IWYU pragma: export
  #endif
#endif
// IWYU pragma: end_exports

#endif /* SUTF_MASTER_H */

