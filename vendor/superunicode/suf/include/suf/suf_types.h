/**
 * @file suf_types.h
 * @brief SuperUnicode Font (.suf) Binary Format Definitions
 *
 * The .suf binary format is a high-performance, dual-mode font container
 * tailored for SuperUnicode (SUCS 31-bit and ExtSUCS 64-bit codepoints).
 * It features 16-byte SIMD register alignment for glyph metrics, zero UTF-16
 * legacy constraints, dual-mode support for early kernel boot bitmap
 * consoles (GOP/VBE) and OS GUI vector compositor engines, first-class BANcode
 * crash diagnostics badges, continuous variable axes (weight, width, slant,
 * italic, grade, roundness, optical size, and custom axes), and SuperUnicode
 * Extended plugin fontmaking.
 *
 * Strictly freestanding (-nostdlib -ffreestanding compliant).
 */

#ifndef SUF_TYPES_H
#define SUF_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- SUF Magic & Format Version --- */
#define SUF_MAGIC               0x53554631UL  /* 'SUF1' in Big-Endian */
#define SUF_VERSION_CURRENT     1

/* --- SUF Feature Flags --- */
#define SUF_FLAG_BOOT_BITMAP    0x0001U /* Includes pre-rendered raster bitmap table */
#define SUF_FLAG_OS_VECTOR      0x0002U /* Includes Bézier vector outline table */
#define SUF_FLAG_EXTSUCS        0x0004U /* Uses 64-bit ExtSUCS codepoints in cmap */
#define SUF_FLAG_LIGATURES      0x0008U /* Includes direct ligature substitution table */
#define SUF_FLAG_KERNING        0x0010U /* Includes horizontal kerning pair table */
#define SUF_FLAG_BANCODE        0x0020U /* Includes BANcode kernel panic / diagnostic glyphs */
#define SUF_FLAG_VARIABLE       0x0040U /* Includes continuous variable axis variation tables */
#define SUF_FLAG_PLUGIN_FONT    0x0080U /* Modded SuperUnicode plugin font (Extended Mode only) */
#define SUF_FLAG_GLYPH_VARIATIONS 0x0100U /* Includes per-glyph outline variation delta blob (gvar) */

/* --- Standard Variable Axis Tags --- */
#define SUF_AXIS_TAG(a,b,c,d)   (((uint32_t)(uint8_t)(a) << 24) | \
                                 ((uint32_t)(uint8_t)(b) << 16) | \
                                 ((uint32_t)(uint8_t)(c) << 8)  | \
                                 ((uint32_t)(uint8_t)(d)))

#define SUF_AXIS_WGHT           SUF_AXIS_TAG('w','g','h','t') /* Weight: 100..900 (def: 400) */
#define SUF_AXIS_WDTH           SUF_AXIS_TAG('w','d','t','h') /* Width: 50..200% (def: 100) */
#define SUF_AXIS_SLNT           SUF_AXIS_TAG('s','l','n','t') /* Slant: -90..+90 deg (def: 0) */
#define SUF_AXIS_ITAL           SUF_AXIS_TAG('i','t','a','l') /* Italic: 0..1 (def: 0) */
#define SUF_AXIS_GRAD           SUF_AXIS_TAG('g','r','a','d') /* Grade: -200..200 (def: 0) */
#define SUF_AXIS_ROND           SUF_AXIS_TAG('r','o','n','d') /* Roundness: 0..100 (def: 0) */
#define SUF_AXIS_OPSZ           SUF_AXIS_TAG('o','p','s','z') /* Optical Size: 6..72 pt (def: 12) */

/* --- BANcode Registry Ranges (System Control Plane) --- */
#define SUF_BANCODE_FATAL_MIN   0x0011A000UL  /* B+ BANcode: 2048 fatal kernel errors */
#define SUF_BANCODE_FATAL_MAX   0x0011A7FFUL
#define SUF_BANCODE_WARN_MIN    0x0011A800UL  /* W+ WARNcode: 1024 kernel warnings */
#define SUF_BANCODE_WARN_MAX    0x0011ABFFUL
#define SUF_BANCODE_COM_MIN     0x0011AC00UL  /* C+ COMcode: 512 communications/success */
#define SUF_BANCODE_COM_MAX     0x0011ADFFUL
#define SUF_BANCODE_SOFT_MIN    0x0011AE00UL  /* S+ SOFTcode: 256 soft errors */
#define SUF_BANCODE_SOFT_MAX    0x0011AEFFUL
#define SUF_KERNEL_TRAP_MIN     0x7FFFFFF0UL  /* 15 Kernel Security Traps */
#define SUF_KERNEL_TRAP_MAX     0x7FFFFFFEUL

/* --- Outline Command Codes --- */
#define SUF_CMD_END_GLYPH       0x00U /* End of outline stream */
#define SUF_CMD_MOVE_TO         0x01U /* 1 point: (int16_t x, int16_t y) */
#define SUF_CMD_LINE_TO         0x02U /* 1 point: (int16_t x, int16_t y) */
#define SUF_CMD_QUAD_TO         0x03U /* 2 points: (int16_t cx, cy, int16_t x, y) */
#define SUF_CMD_CUBIC_TO        0x04U /* 3 points: (int16_t c1x, c1y, int16_t c2x, c2y, int16_t x, y) */
#define SUF_CMD_CLOSE_PATH      0x05U /* Close current contour */

/* --- Return / Status Codes --- */
typedef enum {
    SUF_OK                  = 0,
    SUF_ERR_INVALID_MAGIC   = -1,
    SUF_ERR_INVALID_VERSION = -2,
    SUF_ERR_INVALID_HEADER  = -3,
    SUF_ERR_BUFFER_TOO_SMALL= -4,
    SUF_ERR_GLYPH_NOT_FOUND = -5,
    SUF_ERR_NO_BITMAP       = -6,
    SUF_ERR_NO_OUTLINE      = -7,
    SUF_ERR_CORRUPT_DATA    = -8,
    SUF_ERR_OUT_OF_BOUNDS   = -9,
    SUF_ERR_NULL_POINTER    = -10,
    SUF_ERR_ALLOC_FAIL      = -11,
    SUF_ERR_NOT_EXTENDED    = -12,
    SUF_ERR_INVALID_PLUGIN  = -13,
    SUF_ERR_AXIS_NOT_FOUND  = -14
} suf_status_t;

/* --- Header Layout (128 Bytes, 16-Byte SIMD Aligned, Big-Endian / Portable) --- */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;                 /* 0x00: 0x53554631 ('SUF1') */
    uint16_t version;               /* 0x04: Version (1) */
    uint16_t flags;                 /* 0x06: SUF_FLAG_* */
    uint32_t glyph_count;           /* 0x08: Total number of glyphs */
    uint16_t units_per_em;          /* 0x0C: EM square units (e.g. 1000, 2048) */
    int16_t  ascender;              /* 0x0E: Typographic ascender */
    int16_t  descender;             /* 0x10: Typographic descender (negative) */
    int16_t  line_gap;              /* 0x12: Typographic line gap */
    int16_t  bbox_min_x;            /* 0x14: Global bounding box min X */
    int16_t  bbox_min_y;            /* 0x16: Global bounding box min Y */
    int16_t  bbox_max_x;            /* 0x18: Global bounding box max X */
    int16_t  bbox_max_y;            /* 0x1A: Global bounding box max Y */
    uint8_t  boot_bitmap_width;     /* 0x1C: Early boot bitmap width (e.g. 8) */
    uint8_t  boot_bitmap_height;    /* 0x1D: Early boot bitmap height (e.g. 16) */
    uint8_t  boot_bitmap_bpp;       /* 0x1E: Bits per pixel (1 or 8) */
    uint8_t  reserved0;             /* 0x1F: Reserved / alignment padding */
    uint32_t cmap_offset;           /* 0x20: Offset to cmap table */
    uint32_t cmap_size;             /* 0x24: Size of cmap table */
    uint32_t metrics_offset;        /* 0x28: Offset to 16-byte aligned metrics */
    uint32_t metrics_size;          /* 0x2C: Size of metrics table */
    uint32_t boot_bitmap_offset;    /* 0x30: Offset to boot bitmap blob */
    uint32_t boot_bitmap_size;      /* 0x34: Size of boot bitmap blob */
    uint32_t outlines_offset;       /* 0x38: Offset to vector outlines blob */
    uint32_t outlines_size;         /* 0x3C: Size of vector outlines blob */
    uint32_t kerning_offset;        /* 0x40: Offset to kerning table */
    uint32_t kerning_size;          /* 0x44: Size of kerning table */
    uint32_t ligatures_offset;      /* 0x48: Offset to ligatures table */
    uint32_t ligatures_size;        /* 0x4C: Size of ligatures table */
    uint32_t var_axes_offset;       /* 0x50: Offset to variable axes table */
    uint32_t var_axes_size;         /* 0x54: Size of variable axes table */
    uint32_t plugin_meta_offset;    /* 0x58: Offset to modded plugin metadata */
    uint32_t plugin_meta_size;      /* 0x5C: Size of modded plugin metadata */
    uint32_t checksum;              /* 0x60: CRC32 / Adler32 checksum */
    uint32_t gvar_offset;           /* 0x64: Offset to per-glyph outline variation blob */
    uint32_t gvar_size;             /* 0x68: Size of per-glyph outline variation blob */
    uint32_t names_offset;          /* 0x6C: Offset to font name records blob */
    uint32_t names_size;            /* 0x70: Size of font name records blob */
    uint8_t  reserved2[12];         /* 0x74: 128-byte total header padding */
} suf_header_t;
#pragma pack(pop)

/* Compile-time check for exact 128-byte header size */
typedef char suf_header_size_assert[(sizeof(suf_header_t) == 128) ? 1 : -1];

/*
 * --- Per-Glyph Outline Variation Blob (SUF_FLAG_GLYPH_VARIATIONS) ---
 *
 * Stored at [gvar_offset, gvar_offset + gvar_size). Holds one raw OpenType
 * 'gvar' GlyphVariationData block per glyph, already remapped onto the SUF
 * outline point list (implied on-curve midpoints materialized), so the
 * exporter can rebuild a 'gvar' table verbatim.
 *
 * Layout (little-endian / native u32):
 *   u32 magic   = 'SGV1' (0x31564753)
 *   u32 count   = glyph_count
 *   u32 sizes[] = count entries: byte size of each block (may be 0)
 *   blocks      = concatenated raw GlyphVariationData, prefix-sum of sizes[]
 *
 * Blocks are self-contained: every tuple carries an embedded peak tuple and
 * private packed point numbers; shared tuples are never referenced. Phantom
 * point deltas and composite-glyph deltas are dropped by the importer.
 */
#define SUF_GVAR_BLOB_MAGIC     0x31564753UL  /* 'SGV1' little-endian */

/*
 * --- Font Name Records Blob ---
 *
 * Stored at [names_offset, names_offset + names_size). Preserves the source
 * font's identity metadata through conversions so exports no longer fall
 * back to generic "SuperUnicode Font" naming.
 *
 * Layout (little-endian / native u32 framing):
 *   u32 magic   = 'SNM1' (0x314D4E53)
 *   u32 count   = number of records (<= 64)
 *   records[]   = each:
 *     u16 name_id   (OpenType nameID: 0=copyright, 1=family, 2=subfamily,
 *                    3=uniqueID, 4=full name, 5=version, 6=PostScript name,
 *                    7=trademark, 8=manufacturer, 9=designer, 11=vendor URL,
 *                    12=designer URL, 13=license, 14=license URL,
 *                    16=preferred family, 17=preferred subfamily)
 *     u16 len       = byte length of the UTF-8 string (no NUL terminator)
 *     utf8 bytes[]
 */
#define SUF_NAMES_BLOB_MAGIC    0x314D4E53UL  /* 'SNM1' little-endian */
#define SUF_NAMES_MAX_RECORDS   64

/* --- SIMD 16-Byte Aligned Glyph Metric Descriptor --- */
#pragma pack(push, 1)
typedef struct {
    int16_t  advance_width;         /* 0x00: Advance width in font units */
    int16_t  left_side_bearing;     /* 0x02: Left side bearing in font units */
    int16_t  x_min;                 /* 0x04: Glyph bounding box min X */
    int16_t  y_min;                 /* 0x06: Glyph bounding box min Y */
    int16_t  x_max;                 /* 0x08: Glyph bounding box max X */
    int16_t  y_max;                 /* 0x0A: Glyph bounding box max Y */
    uint32_t data_offset;           /* 0x0C: Offset into outline or bitmap blob */
} suf_metric_t;
#pragma pack(pop)

typedef char suf_simd_metric_size_assert[(sizeof(suf_metric_t) == 16) ? 1 : -1];

/* --- Direct SUCS Cmap Entry (Base 31-bit) --- */
#pragma pack(push, 1)
typedef struct {
    uint32_t codepoint;             /* 31-bit SUCS codepoint */
    uint32_t glyph_id;              /* Glyph index [0 .. glyph_count - 1] */
} suf_cmap_entry_t;
#pragma pack(pop)

/* --- ExtSUCS Cmap Entry (64-bit, 16-byte aligned) --- */
#pragma pack(push, 1)
typedef struct {
    uint64_t codepoint;             /* 64-bit ExtSUCS codepoint */
    uint32_t glyph_id;              /* Glyph index */
    uint32_t reserved;              /* Padding to 16 bytes */
} suf_cmap_ext_entry_t;
#pragma pack(pop)

/* --- Kerning Pair Entry --- */
#pragma pack(push, 1)
typedef struct {
    uint32_t left_glyph;
    uint32_t right_glyph;
    int16_t  kerning;               /* Font units adjustment */
    uint16_t reserved;
} suf_kern_pair_t;
#pragma pack(pop)

/* --- Direct Ligature Replacement Entry --- */
#pragma pack(push, 1)
typedef struct {
    uint32_t first_glyph;
    uint32_t second_glyph;
    uint32_t replacement_glyph;
    uint32_t reserved;
} suf_ligature_t;
#pragma pack(pop)

/* --- Variable Axis Descriptor (48 Bytes, 16-byte aligned) --- */
#pragma pack(push, 1)
typedef struct {
    uint32_t tag;                   /* 4-char axis tag (e.g. SUF_AXIS_WGHT) */
    float    min_val;               /* Minimum axis value (e.g. 100.0f) */
    float    def_val;               /* Default axis value (e.g. 400.0f) */
    float    max_val;               /* Maximum axis value (e.g. 900.0f) */
    uint32_t flags;                 /* 0 = continuous, 1 = hidden */
    char     name[28];              /* Human-readable axis name (e.g. "Weight") */
} suf_var_axis_t;
#pragma pack(pop)

typedef char suf_var_axis_size_assert[(sizeof(suf_var_axis_t) == 48) ? 1 : -1];

/* --- Modded SuperUnicode Plugin Font Metadata (128 Bytes, 16-byte aligned) --- */
#pragma pack(push, 1)
typedef struct {
    char     plugin_id[64];         /* Unique plugin ID (e.g. "com.openwindows.font.neon") */
    uint8_t  ver_major;             /* Semantic major */
    uint8_t  ver_minor;             /* Semantic minor */
    uint8_t  ver_patch;             /* Semantic patch */
    uint8_t  fs_type;               /* Partition FS type (1 = OWFS) */
    uint32_t range_count;           /* Number of registered ExtSUCS ranges */
    uint64_t base_limit;            /* Runtime base ceiling (0x7FFFFFFF) */
    uint32_t crc32c;                /* Castagnoli CRC32 */
    uint32_t reserved0;
    uint64_t fletcher64;            /* Fletcher-64 checksum */
    uint8_t  reserved1[32];         /* Padding to 128 bytes */
} suf_plugin_font_meta_t;
#pragma pack(pop)

typedef char suf_plugin_meta_size_assert[(sizeof(suf_plugin_font_meta_t) == 128) ? 1 : -1];

#ifdef __cplusplus
}
#endif

#endif /* SUF_TYPES_H */
