/*
 * partmgr.h - OpenWindows Partition Manager (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef PARTMGR_H
#define PARTMGR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Logical sector size used for every on-disk structure. */
#define PARTMGR_SECTOR_SIZE      512u
/* Maximum partitions tracked in one table. */
#define PARTMGR_MAX_ENTRIES      16u
/* GPT partition entry size mandated by the UEFI specification. */
#define PARTMGR_GPT_ENTRY_SIZE   128u
/* Maximum number of extended-MBR (EBR) links followed. */
#define PARTMGR_MAX_EBR_CHAIN    64u
/* GPT name field: 36 UTF-16 code units. */
#define PARTMGR_GPT_NAME_UNITS   36u

/* Status codes returned by every partition manager entry point. */
typedef enum {
    PARTMGR_OK = 0,
    PARTMGR_ERR_INVALID_PARAM,
    PARTMGR_ERR_NOT_INITIALIZED,
    PARTMGR_ERR_IO,
    PARTMGR_ERR_NO_TABLE,
    PARTMGR_ERR_CRC,
    PARTMGR_ERR_NO_SPACE
} partmgr_status_t;

/* Partition table flavour discovered on the medium. */
typedef enum {
    PARTMGR_STYLE_NONE = 0,
    PARTMGR_STYLE_MBR,
    PARTMGR_STYLE_GPT
} partmgr_style_t;

/* Block read callback: read `count` sectors at `lba` into `buf`.
 * Returns 0 on success and a nonzero value on failure. */
typedef int32_t (*partmgr_read_fn)(void *ctx, uint64_t lba, uint32_t count,
                                   void *buf);

/* One discovered partition. */
typedef struct {
    uint32_t index;                    /* 1-based ordinal within the table   */
    bool     bootable;                 /* MBR boot flag 0x80                 */
    bool     gpt;                      /* entry came from a GPT              */
    uint8_t  mbr_type;                 /* MBR type byte (0 for GPT)          */
    uint8_t  type_guid[16];            /* GPT type GUID                      */
    uint8_t  unique_guid[16];          /* GPT unique partition GUID          */
    uint64_t first_lba;                /* absolute first sector              */
    uint64_t last_lba;                 /* absolute last sector (inclusive)   */
    uint64_t sector_count;             /* number of sectors                  */
    uint16_t name[PARTMGR_GPT_NAME_UNITS]; /* GPT UTF-16LE name              */
} partmgr_entry_t;

/* Parsed partition table for a single disk. */
typedef struct {
    partmgr_style_t style;             /* MBR or GPT                          */
    bool            protective_mbr;    /* a type 0xEE entry was seen          */
    uint32_t        entry_count;       /* entries filled in entries[]         */
    uint64_t        disk_sectors;      /* total addressable sectors (GPT)     */
    uint8_t         disk_guid[16];     /* GPT disk GUID                       */
    bool            valid;
    partmgr_entry_t entries[PARTMGR_MAX_ENTRIES];
} partmgr_table_t;

/* Partition manager instance. Caller owns the storage; no heap is used. */
typedef struct {
    partmgr_read_fn read;          /* caller-supplied block reader */
    void           *ctx;           /* opaque argument for `read`   */
    partmgr_table_t table;         /* most recent parse result     */
    bool            initialized;
} partmgr_t;

/* Bind a block reader and reset the table. */
partmgr_status_t partmgr_init(partmgr_t *pm, partmgr_read_fn read_fn, void *ctx);

/* Read LBA 0 and dispatch to the MBR or GPT parser. */
partmgr_status_t partmgr_scan(partmgr_t *pm);

/* Parse a 512-byte MBR sector, following the extended chain. */
partmgr_status_t partmgr_read_mbr(partmgr_t *pm, const uint8_t *sector);

/* Read and validate the GPT header, partition entries and CRC32s. */
partmgr_status_t partmgr_read_gpt(partmgr_t *pm);

#endif /* PARTMGR_H */
