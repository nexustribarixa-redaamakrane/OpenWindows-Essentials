#ifndef SUST_MASTER_H
#define SUST_MASTER_H

// IWYU pragma: export

/**
 * SuperUnicode Serialization Transports (SUST) Master Header
 *
 * SUST is strictly the SERIALIZATION TRANSPORT layer of SuperUnicode. While
 * SUTF (SuperUnicode Transformation Formats) defines the codepoint <->
 * symbol-sequence MAPPING, SUST defines the physical byte-packing,
 * bit-alignment, memory layout, and stream framing rules for storing and
 * transmitting those symbol sequences across memory, SIMD vector registers,
 * IPC channels, and low-level hardware buses.
 *
 * SUST Transports (Included):
 * - SUST-16:  Canonical BIG-ENDIAN byte serialization of the SUTF-16 word stream
 * - SUST-32/64/128/256/512/N: Fixed-width big-endian vector slot serialization
 * - e-SUST:   Hypervisor page-mapped virtual IPC transport (6-byte IPC frames)
 *
 * SUTF Transformation Formats (NOT included — see <sutf.h> in the sutf module):
 * - SUTF-8/16/4/2, vSUTF transformation encoders/decoders.
 */

// IWYU pragma: begin_exports
#include "sucs_types.h"       // IWYU pragma: export
#include "extsucs_types.h"    // IWYU pragma: export
#include "sust16.h"           // IWYU pragma: export
#include "sustfixed.h"        // IWYU pragma: export
#include "esust.h"            // IWYU pragma: export
// IWYU pragma: end_exports

#endif /* SUST_MASTER_H */