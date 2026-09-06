#ifndef ESUST_H
#define ESUST_H

/**
 * e-SUST (Emulated/Virtual SUST) Serialization Transport
 *
 * e-SUST is strictly a SERIALIZATION TRANSPORT defining page-mapped
 * virtual coordinate translation routines for transmitting ExtSUCS
 * codepoints between hypervisor host and guest contexts across IPC
 * boundaries. It frames serialized codepoint coordinates for the wire —
 * distinct from the SUTF transformation formats, which only define the
 * codepoint <-> symbol-sequence mapping.
 *
 * Page Architecture:
 * - The ExtSUCS address space is divided into fixed-size "pages" of
 *   ESUST_PAGE_SIZE codepoints each.
 * - Guest systems reference codepoints via (page_index, offset) pairs.
 * - The hypervisor translates between guest virtual addresses and host
 *   physical ExtSUCS codepoint addresses.
 *
 * IPC Frame Structure:
 * - 4 bytes: page_index (uint32_t, big-endian)
 * - 2 bytes: offset     (uint16_t, big-endian)
 * - Total: 6 bytes per e-SUST IPC frame
 */

#include "extsucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* e-SUST Page Architecture Constants */
#define ESUST_PAGE_SIZE         4096ULL   /* Codepoints per page */
#define ESUST_PAGE_SHIFT        12        /* log2(ESUST_PAGE_SIZE) */
#define ESUST_OFFSET_MASK       0x0FFFULL /* Lower 12 bits = offset within page */
#define ESUST_IPC_FRAME_BYTES   6         /* 4 (page_index) + 2 (offset) */

/* ============================================================================
 * e-SUST Page Entry
 *
 * Maps a guest page_index to a host base address in the ExtSUCS encoding.
 * The host_base must be page-aligned (multiple of ESUST_PAGE_SIZE).
 * ============================================================================ */
typedef struct {
    sucs_ex_char_t  host_base;    /* Host-side ExtSUCS base address (page-aligned) */
    uint32_t        page_index;   /* Guest-side page identifier */
    uint32_t        flags;        /* Page flags (read/write/execute permissions) */
} esust_page_entry_t;

/* Page Flags */
#define ESUST_PAGE_READ     0x01U
#define ESUST_PAGE_WRITE    0x02U
#define ESUST_PAGE_EXEC     0x04U
#define ESUST_PAGE_PRESENT  0x08U

/* Fixed-size page table capacity (no dynamic allocation) */
#define ESUST_MAX_PAGES     256

/* ============================================================================
 * e-SUST Page Table
 *
 * A real (caller-populated) mapping of guest page_index to host ExtSUCS base
 * addresses. Translation is data-driven: a codepoint only translates to a
 * guest page if that page is currently mapped. This is what makes e-SUST a
 * page-mapped transport rather than a flat arithmetic re-encoding.
 * ============================================================================ */

/**
 * Maps (or re-maps) a guest page_index to a page-aligned host base address.
 * The base must be a multiple of ESUST_PAGE_SIZE and its page must not
 * intersect the inherited Kernel Security Trap range.
 * Returns false on invalid arguments; true on success.
 */
bool esust_map_page(uint32_t page_index, sucs_ex_char_t host_base, uint32_t flags);

/**
 * Unmaps a guest page. Returns true if a page was removed.
 */
bool esust_unmap_page(uint32_t page_index);

/**
 * Returns true if page_index is currently mapped; optionally writes its
 * flags via out_flags (may be NULL).
 */
bool esust_is_page_mapped(uint32_t page_index, uint32_t* out_flags);

/**
 * Unmaps every page (used at hypervisor shutdown / teardown).
 */
void esust_unmap_all(void);

/* ============================================================================
 * Host <-> Guest Coordinate Translation
 *
 * These routines translate between flat ExtSUCS codepoint addresses and
 * guest-relative (page_index, offset) coordinate pairs.
 * ============================================================================ */

/**
 * Translates a host ExtSUCS codepoint to a guest (page_index, offset) pair.
 * Returns true on success, false on error (e.g., trap range violation).
 */
bool esust_translate_to_guest(sucs_ex_char_t host_cp,
                              uint32_t* out_page_index,
                              uint16_t* out_offset);

/**
 * Translates a guest (page_index, offset) pair to a host ExtSUCS codepoint.
 * Returns true on success, false on error (e.g., reconstructed address invalid).
 */
bool esust_translate_to_host(uint32_t page_index,
                             uint16_t offset,
                             sucs_ex_char_t* out_host_cp);

/* ============================================================================
 * e-SUST IPC Frame Encoding/Decoding
 *
 * Serializes/deserializes guest-relative coordinates into compact 6-byte
 * IPC frames for transmission between hypervisor host and guest contexts.
 * ============================================================================ */

/**
 * Encodes an ExtSUCS codepoint into a 6-byte e-SUST IPC frame.
 * Returns bytes written (6), or 0 on error.
 */
size_t esust_encode_ipc(sucs_ex_char_t host_cp, uint8_t* out_buf, size_t buf_size);

/**
 * Decodes a 6-byte e-SUST IPC frame into an ExtSUCS codepoint.
 * Returns bytes consumed (6), or 0 on error.
 */
size_t esust_decode_ipc(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp);

#ifdef __cplusplus
}
#endif

#endif /* ESUST_H */