/*
 * owenv.c - OpenWindows Session Environment Viewer (.owx)
 *
 * Inspects system environment settings and session variables from kconf64.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owenv list                   List active environment variables
 *   owenv query <var>            Query specific variable value
 *   owenv help                   Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"
#include "../../DLL/kconf64/kconf64.h"

static void OwEnvPrintUsage(void)
{
    owx_puts("owenv: OpenWindows session environment viewer\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owenv list           list environment variables\r\n");
    owx_puts("  owenv query <var>    inspect variable value\r\n");
    owx_puts("  owenv help           show this message\r\n");
}

static bool OwEnvStrEqual(const char *a, const char *b)
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
        owx_puts("SYSTEMROOT=C:\\OpenWindows\r\n");
        owx_puts("WINDIR=C:\\OpenWindows\r\n");
        owx_puts("ARCH=x86_64\r\n");
        owx_puts("SUBSYSTEM=Native\r\n");
        owx_puts("THEME=classic_azure\r\n");
        owx_puts("PANIC_ACTION=banhammer\r\n");
        owx_puts("owenv: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwEnvStrEqual(verb, "list")) {
        owx_puts("SYSTEMROOT=C:\\OpenWindows\r\n");
        owx_puts("WINDIR=C:\\OpenWindows\r\n");
        owx_puts("ARCH=x86_64\r\n");
        owx_puts("SUBSYSTEM=Native\r\n");
        owx_puts("THEME=classic_azure\r\n");
        owx_puts("PANIC_ACTION=banhammer\r\n");
    } else if (OwEnvStrEqual(verb, "query")) {
        const char *val;
        if (argc < 3 || !argv[2]) {
            owx_puts("owenv: query: missing <var>\r\n");
            return 1;
        }
        val = kconf64_get_string(argv[2], "(not set)");
        owx_puts(argv[2]);
        owx_puts("=");
        owx_puts(val);
        owx_puts("\r\n");
    } else if (OwEnvStrEqual(verb, "help")) {
        OwEnvPrintUsage();
    } else {
        owx_puts(verb);
        owx_puts("=");
        owx_puts(kconf64_get_string(verb, "(not set)"));
        owx_puts("\r\n");
    }

    owx_puts("owenv: done\r\n");
    return 0;
}
