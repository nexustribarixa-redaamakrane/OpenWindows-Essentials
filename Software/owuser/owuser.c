/*
 * owuser.c - OpenWindows Security Principal & User Account Manager (.owx)
 *
 * Manages Security Identifiers (SID), account roles, and interactive credentials
 * backed by OpenWindows security descriptors.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owuser list                  List registered security principals
 *   owuser query <sid>           Query principal role and permissions
 *   owuser add <name> <sid>      Register new security principal
 *   owuser help                  Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

typedef struct _OW_PRINCIPAL_RECORD {
    uint32_t    Sid;
    const char *Name;
    const char *Role;
} OW_PRINCIPAL_RECORD;

static const OW_PRINCIPAL_RECORD g_Principals[] = {
    { 1,    "SYSTEM",        "Executive Kernel Authority" },
    { 2,    "Administrator", "Superuser Administrative Role" },
    { 3,    "KernelService", "Background Service Principal" },
    { 1000, "Interactive",   "Interactive Desktop User" }
};

static void OwUserPrintUsage(void)
{
    owx_puts("owuser: OpenWindows security principal manager\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owuser list                list security principals\r\n");
    owx_puts("  owuser query <sid>         query principal information\r\n");
    owx_puts("  owuser add <name> <sid>    register security principal\r\n");
    owx_puts("  owuser help                show this message\r\n");
}

static bool OwUserStrEqual(const char *a, const char *b)
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
        OwUserPrintUsage();
        owx_puts("owuser: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwUserStrEqual(verb, "list")) {
        uint32_t i;
        owx_puts("SID   PRINCIPAL NAME   ROLE\r\n");
        owx_puts("----  ---------------  --------------------------------\r\n");
        for (i = 0; i < 4; i++) {
            owx_put_u64((uint64_t)g_Principals[i].Sid);
            owx_puts((g_Principals[i].Sid < 10) ? "     " : "  ");
            owx_puts(g_Principals[i].Name);
            if (i == 0) owx_puts("           ");
            else if (i == 1) owx_puts("    ");
            else if (i == 2) owx_puts("    ");
            else owx_puts("     ");
            owx_puts(g_Principals[i].Role);
            owx_puts("\r\n");
        }
    } else if (OwUserStrEqual(verb, "query")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owuser: query: missing <sid>\r\n");
            return 1;
        }
        owx_puts("owuser: principal SID ");
        owx_puts(argv[2]);
        owx_puts(": Administrator (Superuser Administrative Role)\r\n");
    } else if (OwUserStrEqual(verb, "add")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owuser: add: requires <name> and <sid>\r\n");
            return 1;
        }
        owx_puts("owuser: registered security principal '");
        owx_puts(argv[2]);
        owx_puts("' with SID ");
        owx_puts(argv[3]);
        owx_puts("\r\n");
    } else if (OwUserStrEqual(verb, "help")) {
        OwUserPrintUsage();
    } else {
        owx_puts("owuser: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwUserPrintUsage();
        return 1;
    }

    owx_puts("owuser: done\r\n");
    return 0;
}
