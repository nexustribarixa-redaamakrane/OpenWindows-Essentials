/*
 * owsh.c - OpenWindows Native Command Shell (.owx)
 *
 * Provides a zero-allocation interactive command line supporting
 * builtins: help, kconf, kext, banhammer, wm, kill, edit, clear, exit.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "kscript.h"
#include "../owx_gate.h"


int owsh_execute_command(const char *cmd_line, size_t len)
{
    if (!cmd_line || len == 0u) return 0;

    kscript_line_t parsed;
    if (!kscript_parse_line(cmd_line, len, &parsed)) {
        return 0;
    }

    if (kscript_token_equals(&parsed.tokens[0], "help")) {
        /* Display command summary */
        return 0;
    } else if (kscript_token_equals(&parsed.tokens[0], "kconf")) {
        /* Query/set config key */
        return 0;
    } else if (kscript_token_equals(&parsed.tokens[0], "kill")) {
        /* Terminate process */
        return 0;
    } else if (kscript_token_equals(&parsed.tokens[0], "edit")) {
        /* Launch owedit */
        return 0;
    } else if (kscript_token_equals(&parsed.tokens[0], "wm") ||
               kscript_token_equals(&parsed.tokens[0], "startwm") ||
               kscript_token_equals(&parsed.tokens[0], "win")) {
        /* Launch OpenWindows Window Manager and Graphical Desktop */
        return 0;
    } else if (kscript_token_equals(&parsed.tokens[0], "banhammer")) {
        /* Trigger crasher / panic test */
        return 0;
    }

    return -1;
}

/*
 * owsh - native command shell entry point.
 *
 * The kernel launches this image at CPL3 with the System V argument registers
 * (RDI = argc, RSI = argv, RDX = envp), which the C ABI presents as main()'s
 * parameters.  This first, deliberately minimal, body echoes the argument vector
 * to the console so the round trip "kernel builds argv -> user process observes
 * it" can be exercised end to end under emulation.  Command dispatch lives in
 * owsh_execute_command() and is wired in once the launch path is proven.
 */
int owsh_main(int argc, const char *const *argv)
{
    owx_puts("owsh: argc=");
    owx_put_i64((int64_t)argc);
    owx_puts("\r\n");

    for (int i = 0; i < argc; i++) {
        owx_puts("owsh: argv[");
        owx_put_i64((int64_t)i);
        owx_puts("]=");
        owx_puts((argv && argv[i]) ? argv[i] : "(null)");
        owx_puts("\r\n");
    }

    owx_puts("owsh: done\r\n");
    return 0;
}
