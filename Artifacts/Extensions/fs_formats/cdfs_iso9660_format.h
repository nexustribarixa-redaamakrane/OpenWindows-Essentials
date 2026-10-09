/*
 * cdfs_iso9660_format.h - Compact Disc File System (CDFS / ISO 9660 / Joliet)
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef CDFS_ISO9660_FORMAT_H
#define CDFS_ISO9660_FORMAT_H

#include <stdint.h>

#define ISO9660_PRIMARY_VD_SECTOR 16u
#define ISO9660_STANDARD_ID_MAGIC 0x3130304443ULL /* "CD001" */
#define ISO9660_SECTOR_SIZE       2048u

#pragma pack(push, 1)
typedef struct {
    uint8_t  vd_type;               /* 1 = Primary, 2 = Supplementary (Joliet), 255 = Terminator */
    char     standard_id[5];        /* "CD001" */
    uint8_t  vd_version;            /* 1 */
    uint8_t  unused1;
    char     system_id[32];
    char     volume_id[32];         /* Volume label */
    uint8_t  unused2[8];
    uint32_t volume_space_size_le;
    uint32_t volume_space_size_be;
    uint8_t  unused3[32];
    uint16_t volume_set_size_le;
    uint16_t volume_set_size_be;
    uint16_t volume_sequence_le;
    uint16_t volume_sequence_be;
    uint16_t logical_block_size_le; /* Typically 2048 */
    uint16_t logical_block_size_be;
    uint32_t path_table_size_le;
    uint32_t path_table_size_be;
    uint32_t type_l_path_table_le;
    uint32_t opt_type_l_path_table_le;
    uint32_t type_m_path_table_be;
    uint32_t opt_type_m_path_table_be;
    uint8_t  root_directory_record[34];
    char     volume_set_id[128];
    char     publisher_id[128];
    char     data_preparer_id[128];
    char     application_id[128];
} iso9660_pvd_t;
#pragma pack(pop)

#endif /* CDFS_ISO9660_FORMAT_H */
