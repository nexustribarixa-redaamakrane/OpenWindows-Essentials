/*
 * bdmp_core.h - OpenWindows BANcode Crash Dump (.bdmp) producer and validator.
 *
 * bdmp_format.h owns the byte layout.  This header owns the two operations that
 * need to agree on it: building a conforming dump, and verifying one.  Keeping
 * them together is the point -- a producer and a validator that disagree about
 * a checksum window or an alignment rule cannot both be right, and the test
 * suite diffs them over the same bytes rather than trusting a comment.
 *
 * Header-only and freestanding: the same code is compiled into the kernel's
 * boot-time fixture seeder, into the Ring 3 bancat decoder, and into the host
 * harness, so there is exactly one definition of "well-formed .bdmp".
 *
 * C99 freestanding.  No libc, no heap, no floating point.
 */
#ifndef BDMP_CORE_H
#define BDMP_CORE_H

#include "bdmp_format.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* CRC32c (Castagnoli), reflected, polynomial 0x82F63B78.  `bdmp_crc32c_pass`
 * feeds a running value and returns it un-inverted, which is what lets the dump
 * checksum skip its own field; `bdmp_crc32c` is the one-shot form.  This MUST
 * stay byte-for-byte identical to the packer's crc32c() and the kernel's
 * OwFsChecksumCalc(); the host harness asserts all three agree. */
static inline uint32_t bdmp_crc32c_pass(uint32_t crc, const uint8_t *data,
                                        uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        uint32_t bit;
        crc ^= (uint32_t)data[i];
        for (bit = 0; bit < 8u; bit++) {
            crc = (crc >> 1) ^ (0x82F63B78u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return crc;
}

static inline uint32_t bdmp_crc32c(const uint8_t *data, uint32_t n)
{
    return ~bdmp_crc32c_pass(0xFFFFFFFFu, data, n);
}

/* Round up to the 8-byte alignment every body structure is placed at. */
static inline uint32_t bdmp_align8(uint32_t x)
{
    return (x + 7u) & ~7u;
}

/* The minimum memory_dump_offset for a dump holding `trap_count` traps: the
 * end of the trap table, 8-byte aligned. */
static inline uint32_t bdmp_body_min_offset(uint32_t trap_count)
{
    return bdmp_align8(BDMP_TRAP_TABLE_OFFSET +
                       trap_count * BDMP_TRAP_RECORD_SIZE);
}

/* ---- Producer -------------------------------------------------------------
 *
 * Lay out a conforming dump into `out` (capacity `cap`), returning the total
 * size in bytes or 0 on any refusal.  The caller owns the buffer; nothing is
 * allocated here.
 *
 * `mem` may be NULL only when `mem_size` is 0, and `traps` only when
 * `trap_count` is 0.  The frame must be non-NULL: a dump with no register frame
 * is not a crash dump.
 *
 * The output is deterministic: the whole [0, total) range is zeroed first, so
 * the padding between the trap table and the memory region is a stable zero and
 * two builds of the same inputs hash identically.  dump_size is exactly
 * memory_dump_offset + mem_size; the memory region's byte count is not padded,
 * so no byte past the caller's `mem` buffer is ever read. */
static inline uint32_t bdmp_build(
    void *out, uint32_t cap,
    const bdmp_cpu_frame_t *frame,
    const bdmp_trap_record_t *traps, uint32_t trap_count,
    uint32_t primary_bancode, uint32_t cpu_id,
    uint64_t tsc, uint64_t cr2,
    const uint8_t *mem, uint32_t mem_size)
{
    uint8_t *p = (uint8_t *)out;
    bdmp_header_t *h;
    uint32_t mem_off;
    uint32_t total;
    uint32_t i;

    if (!p || !frame) return 0u;
    if (trap_count > (uint32_t)BDMP_MAX_TRAPS) return 0u;
    if (trap_count != 0u && !traps) return 0u;
    if (mem_size != 0u && !mem) return 0u;

    mem_off = bdmp_body_min_offset(trap_count);
    total = mem_off + mem_size;
    if (total < mem_off) return 0u;          /* overflow */
    if (total > cap) return 0u;

    for (i = 0; i < total; i++) p[i] = 0u;

    h = (bdmp_header_t *)out;
    h->magic = (uint32_t)BDMP_MAGIC;
    h->format_version = (uint16_t)BDMP_FORMAT_VERSION;
    h->header_size = (uint16_t)BDMP_HEADER_SIZE;
    h->dump_size = total;
    h->header_checksum = 0u;
    h->primary_bancode = primary_bancode;
    h->faulting_cpu_id = cpu_id;
    h->fault_tsc = tsc;
    h->fault_cr2 = cr2;
    h->trap_count = trap_count;
    h->sentinel_invoked = 0u;
    h->sentinel_result = 0u;
    h->memory_dump_offset = mem_off;
    h->memory_dump_size = mem_size;
    h->dump_checksum = 0u;

    {
        const uint8_t *s = (const uint8_t *)frame;
        for (i = 0; i < (uint32_t)sizeof(bdmp_cpu_frame_t); i++) {
            p[BDMP_CPU_FRAME_OFFSET + i] = s[i];
        }
    }
    {
        const uint8_t *s = (const uint8_t *)traps;
        uint32_t tb = trap_count * BDMP_TRAP_RECORD_SIZE;
        for (i = 0; i < tb; i++) {
            p[BDMP_TRAP_TABLE_OFFSET + i] = s[i];
        }
    }
    if (mem != NULL && mem_size != 0u) {
        for (i = 0; i < mem_size; i++) {
            p[mem_off + i] = mem[i];
        }
    }

    /* Both checksum fields are still zero here, so bdmp_crc32c over the whole
     * range is exactly the "dump_checksum and header_checksum treated as zero"
     * window the validator recomputes.  The header checksum is then taken over
     * [0x10,0x100) with dump_checksum already in place. */
    h->dump_checksum = bdmp_crc32c(p, total);
    h->header_checksum = bdmp_crc32c(
        p + 0x10u, (uint32_t)(BDMP_HEADER_SIZE - 0x10u));
    return total;
}

/* ---- Validator ------------------------------------------------------------
 *
 * True when `buf` is a complete, self-consistent dump of `len` bytes.  Every
 * length is checked against the buffer before it is used, so a truncated or
 * hostile file is rejected without an out-of-bounds read.  The checksums are
 * verified last, after the layout they cover has been proven in bounds. */
static inline bool bdmp_validate(const void *buf, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    const bdmp_header_t *h;
    uint32_t total;
    uint32_t min_off;

    if (!p || len < (uint32_t)BDMP_HEADER_SIZE) return false;
    h = (const bdmp_header_t *)buf;

    if (h->magic != (uint32_t)BDMP_MAGIC) return false;
    if (h->format_version != (uint16_t)BDMP_FORMAT_VERSION) return false;
    if (h->header_size != (uint16_t)BDMP_HEADER_SIZE) return false;

    total = h->dump_size;
    if (total < (uint32_t)BDMP_HEADER_SIZE || total > len) return false;
    if (h->trap_count > (uint32_t)BDMP_MAX_TRAPS) return false;

    min_off = bdmp_body_min_offset(h->trap_count);
    if (h->memory_dump_offset < min_off) return false;
    if (h->memory_dump_offset > total) return false;
    if ((uint64_t)h->memory_dump_offset + (uint64_t)h->memory_dump_size >
        (uint64_t)total) {
        return false;
    }

    if (bdmp_crc32c(p + 0x10u, (uint32_t)(BDMP_HEADER_SIZE - 0x10u)) !=
        h->header_checksum) {
        return false;
    }
    /* The dump checksum covers the whole file with the two checksum fields held
     * at zero: the header_checksum field (0x0C..0x0F), which the header checksum
     * covers, and its own dump_checksum field (0x40..0x43).  Zeroing the header
     * checksum is what breaks the otherwise circular pair -- header_checksum
     * covers [0x10,0x100) (which contains the dump checksum), so if the dump
     * checksum also covered the stored header checksum neither could be computed
     * first.  The producer hashes the dump while both fields are still zero,
     * which is exactly this. */
    {
        static const uint8_t zero4[4] = { 0u, 0u, 0u, 0u };
        uint32_t crc;
        crc = bdmp_crc32c_pass(0xFFFFFFFFu, p, 0x0Cu);
        crc = bdmp_crc32c_pass(crc, zero4, 4u);              /* header_checksum at 0x0C */
        crc = bdmp_crc32c_pass(crc, p + 0x10u, 0x30u);       /* 0x10..0x40 */
        crc = bdmp_crc32c_pass(crc, zero4, 4u);              /* dump_checksum at 0x40 */
        crc = bdmp_crc32c_pass(crc, p + 0x44u, total - 0x44u);
        if (~crc != h->dump_checksum) return false;
    }
    return true;
}

/* ---- Accessors ------------------------------------------------------------ */

static inline const bdmp_cpu_frame_t *bdmp_cpu_frame(const bdmp_header_t *h)
{
    return (const bdmp_cpu_frame_t *)(const void *)
        ((const uint8_t *)h + BDMP_CPU_FRAME_OFFSET);
}

static inline const bdmp_trap_record_t *bdmp_traps(const bdmp_header_t *h)
{
    return (const bdmp_trap_record_t *)(const void *)
        ((const uint8_t *)h + BDMP_TRAP_TABLE_OFFSET);
}

static inline const uint8_t *bdmp_memory(const bdmp_header_t *h)
{
    return (const uint8_t *)h + h->memory_dump_offset;
}

#endif /* BDMP_CORE_H */
