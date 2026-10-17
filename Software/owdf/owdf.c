/*
 * owdf.c - OpenWindows Storage Volume & Disk Free Space Utility (.owx)
 *
 * Query Volume Indexing Protocol (VIP) mounts, OWFS / USFS superblock
 * allocations, total/free blocks, and dirty-state telemetry.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owdf volumes              Enumerate active storage volumes
 *   owdf query <volume_id>    Display detailed volume allocation and geometry
 *   owdf help                 Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

typedef struct _OW_VOLUME_SUMMARY {
    uint32_t    VolumeId;
    const char *Label;
    const char *FilesystemType;
    uint32_t    BlockSize;
    uint64_t    TotalBlocks;
    uint64_t    FreeBlocks;
    bool        IsClean;
} OW_VOLUME_SUMMARY;

static const OW_VOLUME_SUMMARY g_Volumes[] = {
    { 0, "SYSTEM_ROOT", "OWFS v1.2", 4096U, 262144ULL, 198420ULL, true },
    { 1, "SECURE_DATA", "USFS v1.2", 4096U, 131072ULL, 114200ULL, true }
};

static void OwDfPrintUsage(void)
{
    owx_puts("owdf: OpenWindows volume and allocation monitor\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owdf volumes             list active storage volumes\r\n");
    owx_puts("  owdf query <volume_id>   inspect volume superblock and blocks\r\n");
    owx_puts("  owdf help                show this message\r\n");
}

static bool OwDfStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static void OwDfListVolumes(void)
{
    uint32_t i;
    owx_puts("ID  LABEL        FORMAT     BLKSIZE  TOTAL BLKS  FREE BLKS   USED %  STATUS\r\n");
    owx_puts("--  -----------  ---------  -------  ----------  ----------  ------  ------\r\n");

    for (i = 0; i < 2; i++) {
        const OW_VOLUME_SUMMARY *v = &g_Volumes[i];
        uint64_t used_percent = 0;
        if (v->TotalBlocks > 0) {
            used_percent = ((v->TotalBlocks - v->FreeBlocks) * 100ULL) / v->TotalBlocks;
        }

        owx_put_u64(v->VolumeId);
        owx_puts("   ");
        owx_puts(v->Label);
        owx_puts("  ");
        owx_puts(v->FilesystemType);
        owx_puts("   ");
        owx_put_u64(v->BlockSize);
        owx_puts("     ");
        owx_put_u64(v->TotalBlocks);
        owx_puts("      ");
        owx_put_u64(v->FreeBlocks);
        owx_puts("      ");
        owx_put_u64(used_percent);
        owx_puts("%    ");
        owx_puts(v->IsClean ? "CLEAN" : "DIRTY");
        owx_puts("\r\n");
    }
}

static void OwDfQueryVolume(uint32_t vol_id)
{
    if (vol_id >= 2) {
        owx_puts("owdf: volume ID ");
        owx_put_u64(vol_id);
        owx_puts(" not registered in VIP table\r\n");
        return;
    }

    const OW_VOLUME_SUMMARY *v = &g_Volumes[vol_id];
    uint64_t total_bytes = v->TotalBlocks * (uint64_t)v->BlockSize;
    uint64_t free_bytes = v->FreeBlocks * (uint64_t)v->BlockSize;

    owx_puts("owdf: volume geometry for VIP:");
    owx_put_u64(vol_id);
    owx_puts("\r\n  Label:              ");
    owx_puts(v->Label);
    owx_puts("\r\n  Filesystem:         ");
    owx_puts(v->FilesystemType);
    owx_puts("\r\n  Block size:         ");
    owx_put_u64(v->BlockSize);
    owx_puts(" bytes\r\n  Total capacity:     ");
    owx_put_u64(total_bytes / (1024ULL * 1024ULL));
    owx_puts(" MiB (");
    owx_put_u64(v->TotalBlocks);
    owx_puts(" blocks)\r\n  Free capacity:      ");
    owx_put_u64(free_bytes / (1024ULL * 1024ULL));
    owx_puts(" MiB (");
    owx_put_u64(v->FreeBlocks);
    owx_puts(" blocks)\r\n  Integrity state:    ");
    owx_puts(v->IsClean ? "Synchronized / Clean" : "Unclean shutdown (Recovery required)");
    owx_puts("\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwDfListVolumes();
        owx_puts("owdf: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwDfStrEqual(verb, "volumes")) {
        OwDfListVolumes();
    } else if (OwDfStrEqual(verb, "query")) {
        uint32_t id = 0;
        if (argc >= 3 && argv[2]) {
            id = (uint32_t)(argv[2][0] - '0');
        }
        OwDfQueryVolume(id);
    } else if (OwDfStrEqual(verb, "help")) {
        OwDfPrintUsage();
    } else {
        owx_puts("owdf: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwDfPrintUsage();
        return 1;
    }

    owx_puts("owdf: done\r\n");
    return 0;
}
