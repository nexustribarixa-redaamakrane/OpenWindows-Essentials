/*
 * sutf.h - SuperUnicode Transformation Format (SUTF) Master Header
 *
 * Self-contained freestanding compatibility layer. Mirrors the canonical
 * superunicode/sutf/include/sutf.h umbrella: SUTF and extSUTF are strictly
 * TRANSFORMATION FORMATS. They define the endian-neutral mapping between
 * SUCS codepoints and symbol sequences (byte words, hex nibbles, symbol
 * frames). SUTF does NOT define physical byte ordering, bit-packing, or
 * stream framing — those live in the SUST (SuperUnicode Serialization
 * Transports) library, see <sust.h>.
 *
 * Base SUTF Transformation Formats (Included):
 * - SUTF-8:  1..6 Byte Stream Transformation
 * - SUTF-16: 1..2 16-Bit Word Transformation (endian-neutral; byte
 *   serialization via SUST-16, see <sust16.h>)
 * - SUTF-4:  4-Bit Hex Nibble Transformation
 * - SUTF-2:  2-Bit Symbol Frame Transformation
 * - Kernel Mode-Switching: <sucs_mode.h> (Base <-> ExtSUCS restart controller)
 * - Kernel Security Trap Dispatch: <sucs_trap.h> (B+ BANcode damage control)
 *
 * Extended Transformation Formats (Forwarded if superunicode_extended is on
 * the include path — canonical vendor tree under vendor/superunicode):
 * - vSUTF: Variable Multi-Byte Streaming Transformation (<vsutf.h>)
 *
 * SUST Serialization Transports (compat <sust16.h> included for byte-level
 * SUST-16 I/O with explicit big/little-endian variants; fixed-width vectors
 * and e-SUST are forwarded from the canonical vendor tree via __has_include):
 * - SUST-16/32/64/128/256/512/N and e-SUST (<sustfixed.h>, <esust.h>).
 *
 * C99 freestanding - stdint.h / stdbool.h / stddef.h only, zero allocation.
 */

#ifndef OWE_SUCS_MASTER_H
#define OWE_SUCS_MASTER_H

#include "sucs_types.h"
#include "sucs_mode.h"
#include "sucs_trap.h"
#include "sutf8.h"
#include "sutf16.h"
#include "sutf4.h"
#include "sutf2.h"
#include "sust16.h"

/* Forwarding references for the extended transformation format and SUST
 * serialization transports if superunicode_extended/sust are on the include
 * path. */
#if defined(__has_include)
  #if __has_include("vsutf.h")
    #include "vsutf.h"
  #endif
  #if __has_include("sustfixed.h")
    #include "sustfixed.h"
  #endif
  #if __has_include("esust.h")
    #include "esust.h"
  #endif
#endif

#endif /* OWE_SUCS_MASTER_H */