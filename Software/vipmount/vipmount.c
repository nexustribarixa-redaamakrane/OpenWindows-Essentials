/*
 * vipmount.c - OpenWindows Volume Indexing Protocol (VIP) Mounter (.owx)
 *
 * Maintains the VIP mount table a user-mode session can act on: attach a
 * (disk, partition) pair, detach it, and list what is currently attached.
 * Conforms to the OpenWindows non-POSIX verb-driven CLI convention:
 *
 *   vipmount attach <disk> <partition>   attach a volume index
 *   vipmount detach <disk> <partition>   detach a volume index
 *   vipmount list                        print the mount table
 *   vipmount help                        show usage information
 *
 * What user mode can and cannot do here is deliberate.  The Ring 3 gateway
 * (owx_gate.h) has no raw-sector call, so the superblock read and the
 * magic/CRC32c check stay where they belong -- in the vipm.owc driver, which
 * owns the disk at attach time -- and each attach prints that split of
 * responsibility instead of pretending to have verified bytes it cannot see.
 * Everything this program does own (bounds, duplicates, detach of an absent
 * entry, table state) is enforced for real.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

#define VIPM_MAX_DISKS       16u
#define VIPM_MAX_PARTITIONS  16u
#define VIPM_MAX_MOUNTS      8u

typedef enum {
    VIP_INDEX_PENDING = 0, /* registered; type/geometry confirmed by vipm.owc */
    VIP_INDEX_UNITARY = 1  /* whole-disk index (partition 0)                 */
} vip_index_state_t;

typedef struct {
    uint32_t         disk_id;
    uint32_t         partition_id;
    vip_index_state_t state;
    bool             in_use;
} vipmount_entry_t;

static vipmount_entry_t g_MountTable[VIPM_MAX_MOUNTS];

/* ---------------------------------------------------------------- */
/* Helpers                                                          */
/* ---------------------------------------------------------------- */

static bool VipmountStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static bool VipmountParseIndex(const char *s, uint32_t max, uint32_t *out)
{
    uint32_t value = 0u;

    if (!s || !*s || !out) return false;
    while (*s != '\0') {
        if (*s < '0' || *s > '9') return false;
        value = (value * 10u) + (uint32_t)(*s - '0');
        if (value > max) return false;
        s++;
    }
    *out = value;
    return true;
}

static void VipmountPrintUsage(void)
{
    owx_puts("vipmount: OpenWindows VIP volume index mounter\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  vipmount attach <disk> <partition>   attach a volume index\r\n");
    owx_puts("  vipmount detach <disk> <partition>   detach a volume index\r\n");
    owx_puts("  vipmount list                        print the mount table\r\n");
    owx_puts("  vipmount help                        show this message\r\n");
    owx_puts("disks 0-");
    owx_put_u64((uint64_t)(VIPM_MAX_DISKS - 1u));
    owx_puts(", partitions 0-");
    owx_put_u64((uint64_t)(VIPM_MAX_PARTITIONS - 1u));
    owx_puts(" (0 = whole disk)\r\n");
}

static vipmount_entry_t *VipmountFind(uint32_t disk_id, uint32_t partition_id)
{
    uint32_t i;

    for (i = 0u; i < VIPM_MAX_MOUNTS; i++) {
        if (g_MountTable[i].in_use &&
            g_MountTable[i].disk_id == disk_id &&
            g_MountTable[i].partition_id == partition_id) {
            return &g_MountTable[i];
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------- */
/* The attach/detach operations                                     */
/* ---------------------------------------------------------------- */

/*
 * Attach a volume index.  Returns 0 when the pair is now held in the mount
 * table, or a negative code: -1 bad ids, -2 table full, -3 already attached.
 * The superblock steps are named in the report because vipm.owc performs them;
 * user mode records the outcome, it does not invent one.
 */
int vipmount_attach(uint32_t disk_id, uint32_t partition_id)
{
    uint32_t i;

    if (disk_id >= VIPM_MAX_DISKS || partition_id >= VIPM_MAX_PARTITIONS) {
        return -1;
    }
    if (VipmountFind(disk_id, partition_id) != NULL) {
        return -3;
    }

    for (i = 0u; i < VIPM_MAX_MOUNTS; i++) {
        if (!g_MountTable[i].in_use) {
            g_MountTable[i].disk_id = disk_id;
            g_MountTable[i].partition_id = partition_id;
            g_MountTable[i].state = (partition_id == 0u)
                                        ? VIP_INDEX_UNITARY
                                        : VIP_INDEX_PENDING;
            g_MountTable[i].in_use = true;
            return 0;
        }
    }

    return -2;
}

/*
 * Detach a volume index.  Returns 0 on success, -1 for bad ids, -4 when the
 * pair is not attached (a detach that changes nothing is an error, not a
 * silent success, so a stale script fails visibly).
 */
int vipmount_detach(uint32_t disk_id, uint32_t partition_id)
{
    vipmount_entry_t *entry;

    if (disk_id >= VIPM_MAX_DISKS || partition_id >= VIPM_MAX_PARTITIONS) {
        return -1;
    }

    entry = VipmountFind(disk_id, partition_id);
    if (!entry) {
        return -4;
    }

    entry->in_use = false;
    entry->state = VIP_INDEX_PENDING;
    entry->disk_id = 0u;
    entry->partition_id = 0u;
    return 0;
}

static void VipmountPrintAttachReport(uint32_t disk_id, uint32_t partition_id,
                                      int rc)
{
    owx_puts("vipmount: attach disk ");
    owx_put_u64((uint64_t)disk_id);
    owx_puts(" partition ");
    owx_put_u64((uint64_t)partition_id);
    owx_puts("\r\n");
    owx_puts("  1. read VIP sector 0        deferred to vipm.owc "
             "(no raw-sector call in Ring 3)\r\n");
    owx_puts("  2. verify magic & CRC32c    deferred to vipm.owc\r\n");

    owx_puts("  3. register in mount table  ");
    if (rc == 0) {
        owx_puts("held (state ");
        owx_puts((partition_id == 0u) ? "UNITARY" : "PENDING");
        owx_puts(")\r\n");
    } else if (rc == -1) {
        owx_puts("rejected: id out of range\r\n");
    } else if (rc == -2) {
        owx_puts("rejected: mount table full (");
        owx_put_u64((uint64_t)VIPM_MAX_MOUNTS);
        owx_puts(" entries)\r\n");
    } else if (rc == -3) {
        owx_puts("rejected: already attached\r\n");
    } else {
        owx_puts("rejected (code ");
        owx_put_i64((int64_t)rc);
        owx_puts(")\r\n");
    }
}

static void VipmountPrintList(void)
{
    uint32_t i;
    uint32_t used = 0u;

    owx_puts("DISK  PARTITION  STATE\r\n");
    owx_puts("----  ---------  ----------------\r\n");

    for (i = 0u; i < VIPM_MAX_MOUNTS; i++) {
        if (!g_MountTable[i].in_use) continue;
        owx_put_u64((uint64_t)g_MountTable[i].disk_id);
        owx_puts("     ");
        owx_put_u64((uint64_t)g_MountTable[i].partition_id);
        if (g_MountTable[i].partition_id < 10u) owx_putc(' ');
        owx_puts("        ");
        owx_puts((g_MountTable[i].state == VIP_INDEX_UNITARY)
                     ? "UNITARY" : "PENDING");
        owx_puts("\r\n");
        used++;
    }

    owx_puts("vipmount: ");
    owx_put_u64((uint64_t)used);
    owx_puts(" of ");
    owx_put_u64((uint64_t)VIPM_MAX_MOUNTS);
    owx_puts(" mount slot(s) in use\r\n");
}

/* ---------------------------------------------------------------- */
/* Entry point                                                      */
/* ---------------------------------------------------------------- */

int vipmount_main(int argc, const char *const *argv)
{
    uint32_t disk_id = 0u;
    uint32_t partition_id = 0u;
    int rc;

    if (argc < 2 || !argv || !argv[1]) {
        VipmountPrintUsage();
        return 1;
    }

    if (VipmountStrEqual(argv[1], "help") ||
        VipmountStrEqual(argv[1], "-h") ||
        VipmountStrEqual(argv[1], "--help")) {
        VipmountPrintUsage();
        return 0;
    }
    if (VipmountStrEqual(argv[1], "list")) {
        VipmountPrintList();
        return 0;
    }
    if (VipmountStrEqual(argv[1], "attach") ||
        VipmountStrEqual(argv[1], "detach")) {
        bool attach = VipmountStrEqual(argv[1], "attach");

        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("vipmount: ");
            owx_puts(argv[1]);
            owx_puts(": missing <disk> <partition>\r\n");
            return 1;
        }
        if (!VipmountParseIndex(argv[2], VIPM_MAX_DISKS - 1u, &disk_id)) {
            owx_puts("vipmount: '");
            owx_puts(argv[2]);
            owx_puts("' is not a disk id (0-");
            owx_put_u64((uint64_t)(VIPM_MAX_DISKS - 1u));
            owx_puts(")\r\n");
            return 1;
        }
        if (!VipmountParseIndex(argv[3], VIPM_MAX_PARTITIONS - 1u,
                                &partition_id)) {
            owx_puts("vipmount: '");
            owx_puts(argv[3]);
            owx_puts("' is not a partition id (0-");
            owx_put_u64((uint64_t)(VIPM_MAX_PARTITIONS - 1u));
            owx_puts(")\r\n");
            return 1;
        }

        if (attach) {
            rc = vipmount_attach(disk_id, partition_id);
            VipmountPrintAttachReport(disk_id, partition_id, rc);
            return (rc == 0) ? 0 : 1;
        }

        rc = vipmount_detach(disk_id, partition_id);
        if (rc == 0) {
            owx_puts("vipmount: detached disk ");
            owx_put_u64((uint64_t)disk_id);
            owx_puts(" partition ");
            owx_put_u64((uint64_t)partition_id);
            owx_puts("\r\n");
            return 0;
        }
        owx_puts("vipmount: detach failed (code ");
        owx_put_i64((int64_t)rc);
        owx_puts("); nothing was changed\r\n");
        return 1;
    }

    owx_puts("vipmount: unknown verb '");
    owx_puts(argv[1]);
    owx_puts("'\r\n");
    VipmountPrintUsage();
    return 2;
}
