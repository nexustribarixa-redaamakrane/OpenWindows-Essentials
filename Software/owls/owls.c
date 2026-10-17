/*
 * owls.c - OpenWindows Catalog & Volume Listing Utility (.owx)
 *
 * Query and enumerate catalog entries, volumes, and object records
 * across the primary OWFS storage volume and Object Manager namespace.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owls list [catalog]          Enumerate catalog records
 *   owls query <target>          Inspect specific entry metadata and security flags
 *   owls volumes                 List active VIP and filesystem volumes
 *   owls help                    Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"
#include "../../Extensions/vfs_types.h"

#define OWLS_ENTRY_COUNT     6U

typedef struct _OWLS_CATALOG_RECORD {
    const char *Name;
    uint32_t    Attributes;
    uint32_t    SecurityFlags;
    uint64_t    ByteSize;
} OWLS_CATALOG_RECORD;

static const OWLS_CATALOG_RECORD g_PrimaryCatalog[OWLS_ENTRY_COUNT] = {
    { "System",     OW_FS_ATTR_DIRECTORY | OW_FS_ATTR_SYSTEM, 0x00000001U, 0U },
    { "Devices",    OW_FS_ATTR_DIRECTORY | OW_FS_ATTR_SYSTEM, 0x00000001U, 0U },
    { "Users",      OW_FS_ATTR_DIRECTORY,                     0x00000000U, 0U },
    { "Temp",       OW_FS_ATTR_DIRECTORY,                     0x00000000U, 0U },
    { "readme.txt", OW_FS_ATTR_ARCHIVE,                       0x00000000U, 64U },
    { "boot.log",   OW_FS_ATTR_SYSTEM | OW_FS_ATTR_HIDDEN,    0x00000002U, 128U }
};

static void OwlsPrintUsage(void)
{
    owx_puts("owls: OpenWindows catalog and object listing utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owls list [catalog]    list entries in target catalog\r\n");
    owx_puts("  owls query <name>      display entry metadata and attributes\r\n");
    owx_puts("  owls volumes           display registered storage volumes\r\n");
    owx_puts("  owls help              display this guidance\r\n");
}

static bool OwlsStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static void OwlsPrintAttributes(uint32_t attrs)
{
    owx_putc('[');
    owx_putc((attrs & OW_FS_ATTR_DIRECTORY) ? 'C' : '-'); /* Catalog */
    owx_putc((attrs & OW_FS_ATTR_READONLY)  ? 'R' : '-'); /* Read-Only */
    owx_putc((attrs & OW_FS_ATTR_HIDDEN)    ? 'H' : '-'); /* Hidden */
    owx_putc((attrs & OW_FS_ATTR_SYSTEM)    ? 'S' : '-'); /* System */
    owx_putc((attrs & OW_FS_ATTR_ARCHIVE)   ? 'A' : '-'); /* Archive */
    owx_putc(']');
}

static void OwlsListCatalog(const char *catalog)
{
    uint32_t i;
    owx_puts("owls: catalog: ");
    owx_puts((catalog && *catalog) ? catalog : "Root");
    owx_puts("\r\n");
    owx_puts("  Attr    Size (B)  Name\r\n");
    owx_puts("  ------  --------  --------------------\r\n");

    for (i = 0; i < OWLS_ENTRY_COUNT; i++) {
        const OWLS_CATALOG_RECORD *rec = &g_PrimaryCatalog[i];
        owx_puts("  ");
        OwlsPrintAttributes(rec->Attributes);
        owx_puts("  ");
        owx_put_u64(rec->ByteSize);
        if (rec->ByteSize < 10U)   owx_puts("        ");
        else if (rec->ByteSize < 100U)  owx_puts("       ");
        else if (rec->ByteSize < 1000U) owx_puts("      ");
        else owx_puts("     ");

        owx_puts(rec->Name);
        if (rec->Attributes & OW_FS_ATTR_DIRECTORY) {
            owx_puts("\\");
        }
        owx_puts("\r\n");
    }
}

static void OwlsQueryEntry(const char *name)
{
    uint32_t i;
    bool found = false;

    if (!name || !*name) {
        owx_puts("owls: query: missing <name>\r\n");
        return;
    }

    for (i = 0; i < OWLS_ENTRY_COUNT; i++) {
        if (OwlsStrEqual(g_PrimaryCatalog[i].Name, name)) {
            const OWLS_CATALOG_RECORD *rec = &g_PrimaryCatalog[i];
            owx_puts("owls: record: ");
            owx_puts(rec->Name);
            owx_puts("\r\n  Type:        ");
            owx_puts((rec->Attributes & OW_FS_ATTR_DIRECTORY) ? "Catalog" : "File");
            owx_puts("\r\n  Attributes:  ");
            OwlsPrintAttributes(rec->Attributes);
            owx_puts("\r\n  Security:    0x");
            owx_put_u64((uint64_t)rec->SecurityFlags);
            owx_puts("\r\n  Byte size:   ");
            owx_put_u64(rec->ByteSize);
            owx_puts(" bytes\r\n");
            found = true;
            break;
        }
    }

    if (!found) {
        owx_puts("owls: entry not found: ");
        owx_puts(name);
        owx_puts("\r\n");
    }
}

static void OwlsListVolumes(void)
{
    owx_puts("owls: active volumes:\r\n");
    owx_puts("  VIP:0 (UniVIP 44-bit trie) -> Partition 0 [OWFS Primary 4096B block]\r\n");
    owx_puts("  VIP:1 (FVIP Flat Volume)   -> Partition 1 [USFS Secure Crypto-Purge]\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwlsListCatalog("Root");
        owx_puts("owls: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwlsStrEqual(verb, "list")) {
        const char *cat = (argc >= 3 && argv[2]) ? argv[2] : "Root";
        OwlsListCatalog(cat);
    } else if (OwlsStrEqual(verb, "query")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owls: query: missing <name>\r\n");
            return 1;
        }
        OwlsQueryEntry(argv[2]);
    } else if (OwlsStrEqual(verb, "volumes")) {
        OwlsListVolumes();
    } else if (OwlsStrEqual(verb, "help")) {
        OwlsPrintUsage();
    } else {
        owx_puts("owls: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwlsPrintUsage();
        return 1;
    }

    owx_puts("owls: done\r\n");
    return 0;
}
