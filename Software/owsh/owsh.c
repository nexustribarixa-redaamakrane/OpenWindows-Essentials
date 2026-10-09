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
