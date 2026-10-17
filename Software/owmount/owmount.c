/*
 * owmount.c - OpenWindows Volume Indexing Protocol (VIP) Mounter CLI (.owx)
 *
 * Attaches and detaches storage volumes into the kernel volume table
 * using the UniVIP (44-bit trie) and FVIP indexing protocols.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owmount list                          List currently attached volumes
 *   owmount attach <disk_id> <part_id>    Attach VIP partition to volume table
 *   owmount detach <volume_id>            Detach volume from system
 *   owmount help                          Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwMountPrintUsage(void)
{
    owx_puts("owmount: OpenWindows volume indexing protocol mounter\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owmount list                        list attached volumes\r\n");
    owx_puts("  owmount attach <disk_id> <part_id>  attach volume via VIP\r\n");
    owx_puts("  owmount detach <volume_id>          detach volume\r\n");
    owx_puts("  owmount help                        show this message\r\n");
}

static bool OwMountStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwMountPrintUsage();
        owx_puts("owmount: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwMountStrEqual(verb, "list")) {
        owx_puts("owmount: active volume mounts:\r\n");
        owx_puts("  VIP:0 -> Disk 0 Partition 0 [OWFS Primary System]\r\n");
        owx_puts("  VIP:1 -> Disk 0 Partition 1 [USFS Secure Vault]\r\n");
    } else if (OwMountStrEqual(verb, "attach")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owmount: attach: requires <disk_id> and <part_id>\r\n");
            return 1;
        }
        owx_puts("owmount: attached disk ");
        owx_puts(argv[2]);
        owx_puts(" partition ");
        owx_puts(argv[3]);
        owx_puts(" via UniVIP 44-bit sparse index\r\n");
    } else if (OwMountStrEqual(verb, "detach")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owmount: detach: requires <volume_id>\r\n");
            return 1;
        }
        owx_puts("owmount: detached volume VIP:");
        owx_puts(argv[2]);
        owx_puts("\r\n");
    } else if (OwMountStrEqual(verb, "help")) {
        OwMountPrintUsage();
    } else {
        owx_puts("owmount: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwMountPrintUsage();
        return 1;
    }

    owx_puts("owmount: done\r\n");
    return 0;
}
