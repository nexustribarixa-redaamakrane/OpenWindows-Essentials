/*
 * bdmp_format.h - OpenWindows BANcode Crash Dump (.bdmp) Header Format
 *
 * Captures non-volatile crash state when Banhammer executes kernel halt:
 * CPU architectural register frame, paging root, last 64 BANcode traps,
 * and sentinel execution trace.
 *
 * Header size: exactly 256 bytes. C99 freestanding.
 */

#ifndef BDMP_FORMAT_H
#define BDMP_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BDMP_MAGIC              0x42444D50u /* "BDMP" in little-endian */
#define BDMP_HEADER_SIZE        256u
#define BDMP_FORMAT_VERSION     0x0001u
#define BDMP_MAX_TRAPS          64u

typedef struct {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8,  r9,  r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, rflags;
    uint64_t cr0, cr2, cr3, cr4;
} bdmp_cpu_frame_t;

typedef struct {
    uint32_t bancode;
    uint32_t trap_slot;
    uint64_t timestamp;
    uint64_t context_ip;
} bdmp_trap_record_t;

typedef struct {
    /* 0x00 */ uint32_t magic;                /* BDMP_MAGIC (0x42444D50)   */
    /* 0x04 */ uint16_t format_version;       /* 0x0001                    */
    /* 0x06 */ uint16_t header_size;          /* 256 bytes                 */
    /* 0x08 */ uint32_t dump_size;            /* Total dump size (bytes)   */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0xFF*/
    /* 0x10 */ uint32_t primary_bancode;      /* Fatal panic code          */
    /* 0x14 */ uint32_t faulting_cpu_id;      /* Core ID triggering panic  */
    /* 0x18 */ uint64_t fault_tsc;            /* Timestamp counter at panic*/
    /* 0x20 */ uint64_t fault_cr2;            /* Faulting memory address   */
    /* 0x28 */ uint32_t trap_count;           /* Recorded trap count       */
    /* 0x2C */ uint32_t sentinel_invoked;     /* 1 if sentinel attempted   */
    /* 0x30 */ uint32_t sentinel_result;      /* Sentinel outcome code     */
    /* 0x34 */ uint32_t memory_dump_offset;   /* Offset to stack/ram pages */
    /* 0x38 */ uint64_t memory_dump_size;     /* Size of memory pages dump */
    /* 0x40 */ uint32_t dump_checksum;        /* Full CRC32c               */
    /* 0x44 */ uint32_t padding[47];          /* Pad to 256 bytes          */
} bdmp_header_t;

/* ---------------------------------------------------------------------------
 * Body layout (the contract the header alone does not state).
 *
 * A .bdmp file is the 256-byte header followed by a body.  Nothing is stored
 * before the header, and every body structure is little-endian and 8-byte
 * aligned at the offsets below.  The body is, in order:
 *
 *   +0x100  bdmp_cpu_frame_t                    (BDMP_CPU_FRAME_SIZE bytes)
 *   +...    bdmp_trap_record_t[trap_count]      (trap_count * 24 bytes)
 *   <pad to 8>                                    (zero fill)
 *           memory image (memory_dump_size bytes)
 *
 * The trap table starts immediately after the register frame; trap_count is
 * bounded by BDMP_MAX_TRAPS.  memory_dump_offset is the byte offset of the
 * memory region from the START OF THE FILE (not the body), rounded up to an
 * 8-byte boundary, and is at least BDMP_MIN_BODY_SIZE(trap_count).  A dump with
 * no memory region sets memory_dump_size to 0 and memory_dump_offset to the
 * aligned end of the trap table.  No other section may appear: a reader that
 * finds trailing bytes past memory_dump_offset + memory_dump_size must treat
 * the file as malformed rather than guess.
 *
 * dump_size is the total file length: it must equal
 * memory_dump_offset + memory_dump_size for a body that ends in the memory
 * region, and no field may claim a length that runs past dump_size.
 *
 * Checksums (CRC32c, Castagnoli, reflected 0x82F63B78 -- the same polynomial
 * the .owx packer and OwFsChecksumCalc() use):
 *
 *   header_checksum  covers bytes [0x10, 0x100) exactly as stored.
 *   dump_checksum    covers the whole file [0, dump_size) with the four bytes of
 *                    the dump_checksum field itself (0x40..0x43) AND the four
 *                    bytes of the header_checksum field (0x0C..0x0F) treated as
 *                    zero.  The header_checksum field lies inside its own
 *                    dependency: header_checksum covers [0x10,0x100), which
 *                    includes the dump_checksum field, while dump_checksum covers
 *                    [0,dump_size), which includes the header_checksum field.
 *                    Zeroing the header_checksum field in the dump window breaks
 *                    that cycle, so the dump checksum can be computed first and
 *                    the header checksum written over it afterwards.  Both are
 *                    produced by Extensions/bdmp_core.h, which is the normative
 *                    producer and validator.
 * ------------------------------------------------------------------------- */
#define BDMP_CPU_FRAME_OFFSET  BDMP_HEADER_SIZE
#define BDMP_TRAP_TABLE_OFFSET (BDMP_CPU_FRAME_OFFSET + (uint32_t)sizeof(bdmp_cpu_frame_t))
#define BDMP_TRAP_RECORD_SIZE  ((uint32_t)sizeof(bdmp_trap_record_t))

static inline bool bdmp_header_valid(const bdmp_header_t *h)
{
    if (!h) return false;
    if (h->magic != BDMP_MAGIC) return false;
    if (h->header_size != BDMP_HEADER_SIZE) return false;
    return true;
}

#endif /* BDMP_FORMAT_H */
