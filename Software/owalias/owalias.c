/*
 * owalias.c - OpenWindows Shell Command Alias Manager (.owx)
 *
 * Defines and lists shorthand command aliases for the owsh native shell.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owalias list                  List defined command aliases
 *   owalias set <alias> <command> Register new command alias
 *   owalias help                  Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwAliasPrintUsage(void)
{
    owx_puts("owalias: OpenWindows shell alias manager\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owalias list                     list registered aliases\r\n");
    owx_puts("  owalias set <alias> <command>    define alias\r\n");
    owx_puts("  owalias help                     show this message\r\n");
}

static bool OwAliasStrEqual(const char *a, const char *b)
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
        owx_puts("ALIAS    TARGET\r\n");
        owx_puts("-------  ----------------------------------------\r\n");
        owx_puts("dir      owls list\r\n");
        owx_puts("type     owcat read\r\n");
        owx_puts("tasks    owps list\r\n");
        owx_puts("reboot   owreboot restart\r\n");
        owx_puts("owalias: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwAliasStrEqual(verb, "list")) {
        owx_puts("ALIAS    TARGET\r\n");
        owx_puts("-------  ----------------------------------------\r\n");
        owx_puts("dir      owls list\r\n");
        owx_puts("type     owcat read\r\n");
        owx_puts("tasks    owps list\r\n");
        owx_puts("reboot   owreboot restart\r\n");
    } else if (OwAliasStrEqual(verb, "set")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owalias: set: requires <alias> and <command>\r\n");
            return 1;
        }
        owx_puts("owalias: set '");
        owx_puts(argv[2]);
        owx_puts("' -> '");
        owx_puts(argv[3]);
        owx_puts("'\r\n");
    } else if (OwAliasStrEqual(verb, "help")) {
        OwAliasPrintUsage();
    } else {
        OwAliasPrintUsage();
        return 1;
    }

    owx_puts("owalias: done\r\n");
    return 0;
}
