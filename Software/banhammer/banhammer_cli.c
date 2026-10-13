/*
 * banhammer_cli.c - OpenWindows Banhammer panic-text selector (.owx)
 *
 * Prints the quote the panic path would print for a given category and seed,
 * lists the quote table, or reports a category's size.  The table and the
 * pseudo-random selector come from Extensions/banhammer_texts.h, the same header
 * the kernel's Banhammer path uses, so this can only ever surface a quote that
 * the kernel itself would surface for that seed.
 *
 * It is self-contained: the OWX1 loader binds nothing, so an .owx must not
 * import, and this program reaches the kernel only through the Ring 3 trap in
 * owx_gate.h (UART_WRITE via owx_puts).  No configuration, no filesystem, no
 * clock: the seed is an explicit argument, never a hidden wall-clock read, so
 * the output is reproducible and testable.
 *
 * C99 freestanding.  Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "banhammer_cli_core.h"
#include "../owx_gate.h"

static void banhammer_cli_usage(void) {
    owx_puts("banhammer_cli: OpenWindows Banhammer panic-text selector\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  banhammer_cli quote <category> [seed]   print one selected quote\r\n");
    owx_puts("  banhammer_cli list <category>           print every quote in a category\r\n");
    owx_puts("  banhammer_cli count <category>          number of quotes in a category\r\n");
    owx_puts("  banhammer_cli categories                list the categories\r\n");
    owx_puts("  banhammer_cli help                      show this message\r\n");
    owx_puts("categories: sarcastic, informational\r\n");
    owx_puts("seed: decimal or 0x-prefixed hex, default 0\r\n");
}

/* Resolve argv[2] to a backed category.  Returns true and stores it, or prints a
 * specific reason and returns false.  `verb` names the running command so the
 * message reads `quote: ...` or `list: ...`. */
static bool banhammer_cli_take_category(const char *verb, int argc,
                                        const char *const *argv,
                                        bh_cli_cat_t *out) {
    bh_cli_cat_t cat;

    if (argc < 3 || argv[2] == NULL) {
        owx_puts("banhammer_cli: ");
        owx_puts(verb);
        owx_puts(": missing <category>\r\n");
        return false;
    }

    cat = bh_cli_category(argv[2]);
    if (cat == BH_CLI_CAT_UNKNOWN) {
        owx_puts("banhammer_cli: ");
        owx_puts(verb);
        owx_puts(": unknown category: ");
        owx_puts(argv[2]);
        owx_puts("\r\n");
        return false;
    }
    if (cat == BH_CLI_CAT_UNSUPPORTED) {
        owx_puts("banhammer_cli: ");
        owx_puts(verb);
        owx_puts(": category 'recovery' has no distinct table\r\n");
        return false;
    }

    *out = cat;
    return true;
}

/*
 * banhammer_cli_main - native CLI entry point.
 *
 * Entered by the kernel under the Microsoft x64 process-entry contract that the
 * MinGW-built .owx images use: RCX = argc, RDX = argv, R8 = envp.  The C ABI
 * presents those as this function's parameters.
 */
int banhammer_cli_main(int argc, const char *const *argv) {
    const char *verb;
    bh_cli_cmd_t cmd;

    if (argv == NULL || argc < 2 || argv[1] == NULL) {
        banhammer_cli_usage();
        owx_puts("banhammer_cli: done\r\n");
        return 0;
    }

    verb = argv[1];
    cmd = bh_cli_classify(verb);

    switch (cmd) {
    case BH_CLI_CMD_HELP:
        banhammer_cli_usage();
        break;

    case BH_CLI_CMD_CATEGORIES:
        owx_puts("banhammer_cli: categories: sarcastic, informational\r\n");
        owx_puts("banhammer_cli: note: recovery has no distinct table\r\n");
        break;

    case BH_CLI_CMD_COUNT: {
        bh_cli_cat_t cat;
        if (!banhammer_cli_take_category(verb, argc, argv, &cat)) {
            owx_puts("banhammer_cli: done\r\n");
            return 1;
        }
        owx_puts("banhammer_cli: ");
        owx_puts(bh_cli_category_name(cat));
        owx_puts(" count=");
        owx_put_u64((uint64_t)bh_cli_count(cat));
        owx_puts("\r\n");
        break;
    }

    case BH_CLI_CMD_LIST: {
        bh_cli_cat_t cat;
        uint32_t n;
        uint32_t i;
        if (!banhammer_cli_take_category(verb, argc, argv, &cat)) {
            owx_puts("banhammer_cli: done\r\n");
            return 1;
        }
        n = bh_cli_count(cat);
        for (i = 0u; i < n; ++i) {
            owx_puts("banhammer_cli: ");
            owx_puts(bh_cli_category_name(cat));
            owx_putc('[');
            owx_put_u64((uint64_t)i);
            owx_puts("]=");
            owx_puts(bh_cli_quote_at(cat, i));
            owx_puts("\r\n");
        }
        owx_puts("banhammer_cli: ");
        owx_puts(bh_cli_category_name(cat));
        owx_puts(" count=");
        owx_put_u64((uint64_t)n);
        owx_puts("\r\n");
        break;
    }

    case BH_CLI_CMD_QUOTE: {
        bh_cli_cat_t cat;
        uint64_t seed = 0u;
        const char *quote;

        if (!banhammer_cli_take_category(verb, argc, argv, &cat)) {
            owx_puts("banhammer_cli: done\r\n");
            return 1;
        }
        if (argc >= 4 && argv[3] != NULL) {
            if (!bh_cli_parse_u64(argv[3], &seed)) {
                owx_puts("banhammer_cli: quote: invalid seed: ");
                owx_puts(argv[3]);
                owx_puts("\r\n");
                owx_puts("banhammer_cli: done\r\n");
                return 1;
            }
        }
        quote = bh_cli_select(cat, seed);
        if (quote == NULL) {
            /* Unreachable while `cat` is backed, kept so a future empty table
             * cannot print a NUL through owx_puts. */
            owx_puts("banhammer_cli: quote: no quote available\r\n");
            owx_puts("banhammer_cli: done\r\n");
            return 1;
        }
        owx_puts("banhammer_cli: quote ");
        owx_puts(bh_cli_category_name(cat));
        owx_puts(" seed ");
        owx_put_u64(seed);
        owx_puts(": ");
        owx_puts(quote);
        owx_puts("\r\n");
        break;
    }

    default:
        owx_puts("banhammer_cli: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        owx_puts("banhammer_cli: done\r\n");
        return 1;
    }

    owx_puts("banhammer_cli: done\r\n");
    return 0;
}
