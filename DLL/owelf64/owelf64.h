/*
 * owelf64.h - OpenWindows OWX Executable Loader (.owd)
 *
 * Helpers for validating and loading native `.owx` images into a
 * caller-provided buffer, applying base relocations, and resolving
 * imported symbols through a caller-supplied resolver.
 *
 * Binary layout (little-endian):
 *   - 256-byte OWX header (see Specs/OWX_SPECIFICATION.md):
 *       0x00 u32 Magic (0x4F575831 "OWX1")   0x04 u32 Version
 *       0x08 u32 Flags                        0x0C u32 Entry Point
 *       0x10 u32 Section Count                0x14 u32 Import Count
 *       0x18 u32 Export Count                 0x1C u64 Image Size
 *       0x24 u32 Stack Size                   0x28 u32 Heap Size
 *       0x2C u32 Checksum                     0x30 u32 Subsystem
 *       0x34        204 bytes reserved
 *   - Section table at offset 256: `section_count` entries of 40 bytes.
 *   - Import table follows the section table: `import_count` entries of
 *     24 bytes.
 *   - String table follows the import table (referenced by byte offset).
 *   - Relocations live in a section of type OWELF_SECTION_RELOC.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWELF64_H
#define OWELF64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Format constants ─────────────────────────────────────────── */

#define OWELF_MAGIC            0x4F575831u  /* "OWX1"                  */
#define OWELF_VERSION          1u           /* Current format version  */
#define OWELF_HEADER_SIZE      256u         /* Fixed header size       */
#define OWELF_MAX_SECTIONS     64u          /* Section table capacity  */
#define OWELF_MAX_IMPORTS      128u         /* Import table capacity   */
#define OWELF_SECTION_SIZE     40u          /* Bytes per section entry */
#define OWELF_IMPORT_SIZE      24u          /* Bytes per import entry  */
#define OWELF_POINTER_SIZE     8u           /* 64-bit import slots     */

/* Header field offsets. */
#define OWELF_OFF_MAGIC        0x00u
#define OWELF_OFF_VERSION      0x04u
#define OWELF_OFF_FLAGS        0x08u
#define OWELF_OFF_ENTRY        0x0Cu
#define OWELF_OFF_SECTIONS     0x10u
#define OWELF_OFF_IMPORTS      0x14u
#define OWELF_OFF_EXPORTS      0x18u
#define OWELF_OFF_IMAGE_SIZE   0x1Cu
#define OWELF_OFF_STACK        0x24u
#define OWELF_OFF_HEAP         0x28u
#define OWELF_OFF_CHECKSUM     0x2Cu
#define OWELF_OFF_SUBSYSTEM    0x30u
#define OWELF_OFF_RESERVED     0x34u

/* Section types. */
#define OWELF_SECTION_CODE     0x01u
#define OWELF_SECTION_RDATA    0x02u
#define OWELF_SECTION_DATA     0x03u
#define OWELF_SECTION_BSS      0x04u
#define OWELF_SECTION_RELOC    0x05u
#define OWELF_SECTION_IMPORT   0x06u
#define OWELF_SECTION_EXPORT   0x07u

/* Base relocation types (PE-compatible numbering). */
#define OWELF_RELOC_ABSOLUTE   0u
#define OWELF_RELOC_HIGH       1u
#define OWELF_RELOC_LOW        2u
#define OWELF_RELOC_HIGHLOW    3u
#define OWELF_RELOC_DIR64      4u

/* Loader status codes. */
typedef uint32_t owelf_status_t;
#define OWELF_OK               0u
#define OWELF_ERR_ARG          1u
#define OWELF_ERR_MAGIC        2u
#define OWELF_ERR_VERSION      3u
#define OWELF_ERR_HEADER       4u
#define OWELF_ERR_CHECKSUM     5u
#define OWELF_ERR_TRUNCATED    6u
#define OWELF_ERR_RANGE        7u
#define OWELF_ERR_SECTION      8u
#define OWELF_ERR_NOT_LOADED   9u
#define OWELF_ERR_RELOC        10u
#define OWELF_ERR_IMPORT       11u
#define OWELF_ERR_RESOLVE      12u

/* Parsed header fields (naturally aligned host copy). */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t entry_point;
    uint32_t section_count;
    uint32_t import_count;
    uint32_t export_count;
    uint64_t image_size;
    uint32_t stack_size;
    uint32_t heap_size;
    uint32_t checksum;
    uint32_t subsystem;
} owelf_header_info_t;

/* One decoded section-table entry. */
typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t virtual_addr;
    uint64_t file_offset;
    uint64_t size;
    uint32_t checksum;
    uint32_t reserved;
} owelf_section_t;

/* One decoded import descriptor. */
typedef struct {
    uint32_t module_name_off;   /* String-table offset of module name */
    uint32_t symbol_table_off;  /* File offset of u32 name offsets    */
    uint32_t iat_off;           /* Image offset of the IAT slots      */
    uint32_t symbol_count;      /* Number of imported symbols         */
    uint32_t checksum;          /* Expected export-set checksum       */
    uint32_t reserved;
} owelf_import_t;

/* Loader state bound to one image. */
typedef struct {
    const uint8_t *file;        /* Source image bytes                 */
    size_t         file_size;
    uint8_t       *image;       /* Caller load buffer                 */
    size_t         capacity;    /* Load buffer capacity               */
    owelf_header_info_t header;
    uint32_t       section_table_off;
    uint32_t       import_table_off;
    uint32_t       string_table_off;
    int64_t        delta;       /* Relocation bias already applied    */
    bool           loaded;
} owelf_image_t;

/*
 * Symbol resolver supplied by the caller. Returns true and stores the
 * resolved address in `*out_address` on success; returns false (leaving
 * `*out_address` untouched) when the symbol cannot be resolved.
 */
typedef bool (*owelf_resolver_fn)(const char *module, const char *symbol,
                                  uintptr_t *out_address, void *user);

/* ── API ──────────────────────────────────────────────────────── */

/*
 * Validate the header, section table placement, and image checksum.
 * When `info` is non-NULL the parsed header is written to it.
 */
owelf_status_t owelf_validate(const uint8_t *file, size_t size,
                              owelf_header_info_t *info);

/*
 * Validate and load `file` into `load_buffer` (capacity `load_capacity`).
 * Section payloads are copied to their virtual addresses and BSS sections
 * are zeroed. `image` must be provided by the caller and is fully
 * initialised on success.
 */
owelf_status_t owelf_load(owelf_image_t *image,
                          const uint8_t *file, size_t size,
                          uint8_t *load_buffer, size_t load_capacity);

/*
 * Apply base relocations for the given image bias `delta`. Must be called
 * after a successful owelf_load().
 */
owelf_status_t owelf_relocate(owelf_image_t *image, int64_t delta);

/*
 * Resolve every import through `resolver` and patch the Import Address
 * Table. Must be called after a successful owelf_load().
 */
owelf_status_t owelf_resolve_imports(owelf_image_t *image,
                                     owelf_resolver_fn resolver, void *user);

/* Entry point pointer inside a loaded image, or NULL when not loaded. */
void *owelf_get_entry(const owelf_image_t *image);

/* Copy section entry `index` into `*out_section`. */
bool owelf_section_at(const owelf_image_t *image, uint32_t index,
                      owelf_section_t *out_section);

/* Find the first section of `type`; returns true and fills `*out_section`. */
bool owelf_find_section(const owelf_image_t *image, uint32_t type,
                        owelf_section_t *out_section);

/* CRC32c checksum of an image with the header checksum field zeroed. */
uint32_t owelf_compute_checksum(const uint8_t *file, size_t size);

#endif /* OWELF64_H */
