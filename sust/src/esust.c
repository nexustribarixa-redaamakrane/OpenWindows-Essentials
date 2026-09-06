/**
 * e-SUST (Emulated/Virtual SUST) Serialization Transport
 *
 * Implementation of page-mapped hypervisor host/guest coordinate translation
 * routines for transmitting ExtSUCS codepoints between virtual machine
 * contexts across IPC boundaries.
 *
 * The ExtSUCS address space is divided into fixed-size pages of
 * ESUST_PAGE_SIZE (4096) codepoints. Guest systems reference codepoints
 * via (page_index, offset) coordinate pairs. The host translates these
 * to flat ExtSUCS codepoint addresses THROUGH a real page table: a codepoint
 * only translates if the page containing it has been mapped by the hypervisor.
 *
 * Zero standard library dependencies; zero dynamic allocation.
 */

#include "esust.h"

/* ============================================================================
 * Page Table (static, fixed-size)
 * ============================================================================ */
static esust_page_entry_t g_page_table[ESUST_MAX_PAGES];
static uint32_t g_page_count = 0;

/* Inherited Kernel Security Trap range — excluded from mappable host pages */
#define ESUST_TRAP_MIN   0x7FFFFFF0ULL
#define ESUST_TRAP_MAX   0x7FFFFFFEULL

static int find_page(uint32_t page_index) {
    for (uint32_t i = 0; i < g_page_count; i++) {
        if (g_page_table[i].page_index == page_index) {
            return (int)i;
        }
    }
    return -1;
}

static bool host_base_valid(sucs_ex_char_t host_base) {
    /* Must be page-aligned */
    if ((host_base & ESUST_OFFSET_MASK) != 0) {
        return false;
    }
    /* The page must not wrap the 64-bit address space */
    if (host_base > ~(sucs_ex_char_t)(ESUST_PAGE_SIZE - 1)) {
        return false;
    }
    /* The page must not intersect the inherited Kernel Security Trap range */
    sucs_ex_char_t page_end = host_base + (ESUST_PAGE_SIZE - 1);
    if (host_base <= ESUST_TRAP_MAX && page_end >= ESUST_TRAP_MIN) {
        return false;
    }
    return true;
}

/* ============================================================================
 * Page Table API
 * ============================================================================ */
bool esust_map_page(uint32_t page_index, sucs_ex_char_t host_base, uint32_t flags) {
    if (!host_base_valid(host_base)) {
        return false;
    }

    int idx = find_page(page_index);
    if (idx >= 0) {
        /* Re-map: update base and flags, keep PRESENT sticky */
        g_page_table[idx].host_base = host_base;
        g_page_table[idx].flags     = flags | ESUST_PAGE_PRESENT;
        return true;
    }

    if (g_page_count >= ESUST_MAX_PAGES) {
        return false; /* table full */
    }

    g_page_table[g_page_count].page_index = page_index;
    g_page_table[g_page_count].host_base  = host_base;
    g_page_table[g_page_count].flags      = flags | ESUST_PAGE_PRESENT;
    g_page_count++;
    return true;
}

bool esust_unmap_page(uint32_t page_index) {
    int idx = find_page(page_index);
    if (idx < 0) {
        return false;
    }
    g_page_count--;
    g_page_table[idx] = g_page_table[g_page_count]; /* swap-remove */
    return true;
}

bool esust_is_page_mapped(uint32_t page_index, uint32_t* out_flags) {
    int idx = find_page(page_index);
    if (idx < 0) {
        return false;
    }
    if (out_flags) {
        *out_flags = g_page_table[idx].flags;
    }
    return true;
}

void esust_unmap_all(void) {
    g_page_count = 0;
}

/* ============================================================================
 * Host -> Guest Coordinate Translation
 *
 * Converts a flat ExtSUCS codepoint address into a guest-relative
 * (page_index, offset) coordinate pair by consulting the page table.
 * ============================================================================ */
bool esust_translate_to_guest(sucs_ex_char_t host_cp,
                              uint32_t* out_page_index,
                              uint16_t* out_offset) {
    if (!out_page_index || !out_offset) {
        return false;
    }
    if (!extsucs_is_valid(host_cp)) {
        return false;
    }

    /* Look up the mapped page containing this codepoint */
    for (uint32_t i = 0; i < g_page_count; i++) {
        sucs_ex_char_t base     = g_page_table[i].host_base;
        sucs_ex_char_t page_end = base + (ESUST_PAGE_SIZE - 1);
        if (host_cp >= base && host_cp <= page_end) {
            *out_page_index = g_page_table[i].page_index;
            *out_offset     = (uint16_t)(host_cp - base);
            return true;
        }
    }
    return false; /* unmapped address */
}

/* ============================================================================
 * Guest -> Host Coordinate Translation
 *
 * Converts a guest-relative (page_index, offset) coordinate pair into
 * a flat ExtSUCS codepoint address through the page table.
 * ============================================================================ */
bool esust_translate_to_host(uint32_t page_index,
                             uint16_t offset,
                             sucs_ex_char_t* out_host_cp) {
    if (!out_host_cp) {
        return false;
    }

    /* Offset must be within page bounds */
    if (offset >= ESUST_PAGE_SIZE) {
        return false;
    }

    int idx = find_page(page_index);
    if (idx < 0) {
        return false; /* unmapped page */
    }

    sucs_ex_char_t host_cp = g_page_table[idx].host_base + (sucs_ex_char_t)offset;

    if (!extsucs_is_valid(host_cp)) {
        return false;
    }

    *out_host_cp = host_cp;
    return true;
}

/* ============================================================================
 * e-SUST IPC Frame Encoding
 *
 * Encodes an ExtSUCS codepoint into a compact 6-byte IPC frame:
 *   [4 bytes: page_index big-endian] [2 bytes: offset big-endian]
 * ============================================================================ */
size_t esust_encode_ipc(sucs_ex_char_t host_cp, uint8_t* out_buf, size_t buf_size) {
    if (!out_buf || buf_size < ESUST_IPC_FRAME_BYTES) {
        return 0;
    }

    uint32_t page_index;
    uint16_t offset;

    if (!esust_translate_to_guest(host_cp, &page_index, &offset)) {
        return 0;
    }

    /* Write page_index as 4-byte big-endian */
    out_buf[0] = (uint8_t)((page_index >> 24) & 0xFFU);
    out_buf[1] = (uint8_t)((page_index >> 16) & 0xFFU);
    out_buf[2] = (uint8_t)((page_index >> 8)  & 0xFFU);
    out_buf[3] = (uint8_t)(page_index          & 0xFFU);

    /* Write offset as 2-byte big-endian */
    out_buf[4] = (uint8_t)((offset >> 8) & 0xFFU);
    out_buf[5] = (uint8_t)(offset        & 0xFFU);

    return ESUST_IPC_FRAME_BYTES;
}

/* ============================================================================
 * e-SUST IPC Frame Decoding
 *
 * Decodes a 6-byte IPC frame into an ExtSUCS codepoint:
 *   [4 bytes: page_index big-endian] [2 bytes: offset big-endian]
 * ============================================================================ */
size_t esust_decode_ipc(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp) {
    if (!in_buf || !out_cp || buf_size < ESUST_IPC_FRAME_BYTES) {
        return 0;
    }

    uint32_t page_index = ((uint32_t)in_buf[0] << 24) |
                          ((uint32_t)in_buf[1] << 16) |
                          ((uint32_t)in_buf[2] << 8)  |
                          ((uint32_t)in_buf[3]);

    uint16_t offset = ((uint16_t)in_buf[4] << 8) |
                      ((uint16_t)in_buf[5]);

    sucs_ex_char_t host_cp;
    if (!esust_translate_to_host(page_index, offset, &host_cp)) {
        return 0;
    }

    *out_cp = host_cp;
    return ESUST_IPC_FRAME_BYTES;
}