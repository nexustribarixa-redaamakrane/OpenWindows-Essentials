/*
 * zfs_format.h - OpenZFS / Solaris ZFS Uberblock and VDEV Label Format
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef ZFS_FORMAT_H
#define ZFS_FORMAT_H

#include <stdint.h>

#define ZFS_UBERBLOCK_MAGIC_LE 0x00BAB10CULL
#define ZFS_UBERBLOCK_MAGIC_BE 0x0CB1BA00ULL
#define ZFS_VDEV_LABEL_OFFSET  0x40000ULL

#pragma pack(push, 1)
typedef struct {
    uint64_t ub_magic;              /* ZFS_UBERBLOCK_MAGIC */
    uint64_t ub_version;            /* SPA version */
    uint64_t ub_txg;                /* Transaction group */
    uint64_t ub_guid_sum;
    uint64_t ub_timestamp;
    uint64_t ub_rootbp_dva1[2];     /* Root block pointer DVA */
    uint64_t ub_rootbp_dva2[2];
    uint64_t ub_rootbp_dva3[2];
    uint64_t ub_rootbp_prop;
    uint64_t ub_rootbp_pad[2];
    uint64_t ub_rootbp_birth;
    uint64_t ub_rootbp_fill;
    uint64_t ub_rootbp_checksum[4];
    uint64_t ub_software_version;
} zfs_uberblock_t;
#pragma pack(pop)

#endif /* ZFS_FORMAT_H */
