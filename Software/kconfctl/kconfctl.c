/*
 * kconfctl.c - OpenWindows Configuration Tree Control CLI (.owx)
 *
 * Query and set hierarchical keys in the .kconf configuration tree.  The
 * engine is the kconf64 library, statically linked: the OWX1 loader binds
 * nothing, so an .owx must be self-contained.  This is the same kconf64 the
 * boot init (owinit.owx) links, so the CLI and the boot path read one tree.
 *
 * Persistence is deliberately NOT claimed.  kconf64 keeps its registry in a
 * static BSS object and re-seeds the built-in defaults on first use in every
 * new process, so a `set` here changes only this process's tree and is gone
 * when it exits.  Every successful set is therefore reported as "(in-memory,
 * not persisted)" and is read back and checked before it is reported at all.
 *
 * C99 freestanding.  Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "kconfctl_core.h"
#include "../owx_gate.h"

static void kconfctl_usage(void) {
    owx_puts("kconfctl: OpenWindows configuration tree control\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  kconfctl get <key>             print a string value\r\n");
    owx_puts("  kconfctl getint <key>          print an integer value\r\n");
    owx_puts("  kconfctl set <key> <value>     set a string (in-memory)\r\n");
    owx_puts("  kconfctl setint <key> <value>  set an integer (in-memory)\r\n");
    owx_puts("  kconfctl count                 number of configuration keys\r\n");
    owx_puts("  kconfctl help                  show this message\r\n");
}

/*
 * kconfctl_main - native CLI entry point.
 *
 * Entered by the kernel under the Microsoft x64 process-entry contract that the
 * MinGW-built .owx images use: RCX = argc, RDX = argv, R8 = envp.  The C ABI
 * presents those as this function's parameters.
 */
int kconfctl_main(int argc, const char *const *argv) {
    const char *verb;

    if (argv == NULL || argc < 2 || argv[1] == NULL) {
        kconfctl_usage();
        owx_puts("kconfctl: done\r\n");
        return 0;
    }

    verb = argv[1];

    switch (kconfctl_classify(verb)) {
    case KCONFCTL_CMD_HELP:
        kconfctl_usage();
        break;

    case KCONFCTL_CMD_COUNT:
        owx_puts("kconfctl: count=");
        owx_put_i64((int64_t)kconf64_count());
        owx_puts("\r\n");
        break;

    case KCONFCTL_CMD_GET: {
        const char *value;
        if (argc < 3 || argv[2] == NULL) {
            owx_puts("kconfctl: get: missing <key>\r\n");
            owx_puts("kconfctl: done\r\n");
            return 1;
        }
        value = kconfctl_query_string(argv[2]);
        if (value == NULL) {
            owx_puts("kconfctl: ");
            owx_puts(argv[2]);
            owx_puts(": unknown string key\r\n");
            owx_puts("kconfctl: done\r\n");
            return 1;
        }
        owx_puts("kconfctl: ");
        owx_puts(argv[2]);
        owx_putc('=');
        owx_puts(value);
        owx_puts("\r\n");
        break;
    }

    case KCONFCTL_CMD_GETINT: {
        int64_t value = 0;
        if (argc < 3 || argv[2] == NULL) {
            owx_puts("kconfctl: getint: missing <key>\r\n");
            owx_puts("kconfctl: done\r\n");
            return 1;
        }
        if (!kconfctl_query_int(argv[2], &value)) {
            owx_puts("kconfctl: ");
            owx_puts(argv[2]);
            owx_puts(": no integer value\r\n");
            owx_puts("kconfctl: done\r\n");
            return 1;
        }
        owx_puts("kconfctl: ");
        owx_puts(argv[2]);
        owx_putc('=');
        owx_put_i64(value);
        owx_puts("\r\n");
        break;
    }

    case KCONFCTL_CMD_SET: {
        const char *key;
        const char *val;
        const char *back;
        if (argc < 4 || argv[2] == NULL || argv[3] == NULL) {
            owx_puts("kconfctl: set: missing <key> <value>\r\n");
            owx_puts("kconfctl: done\r\n");
            return 1;
        }
        key = argv[2];
        val = argv[3];
        if (!kconfctl_apply_string(key, val)) {
            owx_puts("kconfctl: set: key or value too long\r\n");
            owx_puts("kconfctl: done\r\n");
            return 1;
        }
        /* Verify the write actually landed: reporting success off the setter's
         * own return value alone is how a silently truncated or shadowed key
         * gets reported as applied. */
        back = kconfctl_query_string(key);
        if (back == NULL || !kconfctl_str_eq(back, val)) {
            owx_puts("kconfctl: set: verification failed\r\n");
            owx_puts("kconfctl: done\r\n");
            return 1;
        }
        owx_puts("kconfctl: set ");
        owx_puts(key);
        owx_putc('=');
        owx_puts(back);
        owx_puts(" (in-memory, not persisted)\r\n");
        break;
    }

    case KCONFCTL_CMD_SETINT: {
        const char *key;
        int64_t parsed = 0;
        int64_t back = 0;
        if (argc < 4 || argv[2] == NULL || argv[3] == NULL) {
            owx_puts("kconfctl: setint: missing <key> <value>\r\n");
            owx_puts("kconfctl: done\r\n");
            return 1;
        }
        key = argv[2];
        if (!kconfctl_parse_i64(argv[3], &parsed)) {
            owx_puts("kconfctl: setint: invalid integer: ");
            owx_puts(argv[3]);
            owx_puts("\r\n");
            owx_puts("kconfctl: done\r\n");
            return 1;
        }
        if (!kconfctl_apply_int(key, parsed)) {
            owx_puts("kconfctl: setint: key too long\r\n");
            owx_puts("kconfctl: done\r\n");
            return 1;
        }
        if (!kconfctl_query_int(key, &back) || back != parsed) {
            owx_puts("kconfctl: setint: verification failed\r\n");
            owx_puts("kconfctl: done\r\n");
            return 1;
        }
        owx_puts("kconfctl: set ");
        owx_puts(key);
        owx_putc('=');
        owx_put_i64(back);
        owx_puts(" (in-memory, not persisted)\r\n");
        break;
    }

    default:
        owx_puts("kconfctl: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        owx_puts("kconfctl: done\r\n");
        return 1;
    }

    owx_puts("kconfctl: done\r\n");
    return 0;
}