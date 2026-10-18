/*
 * win.c - OpenWindows 'win' GUI Launcher Command (.owx)
 *
 * Spiritual friend to the classic 'win' command, launching the OpenWindows
 * graphical desktop environment directly from the interactive shell or console.
 *
 * It is a thin front door over the startwm orchestrator (linked into the same
 * image), so all it adds is the banner and the pass-through of the argument
 * vector.  Output goes through the Ring 3 console in Software/owx_gate.h.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

extern int startwm_main(int argc, const char *const *argv);

int win_main(int argc, const char *const *argv)
{
    owx_puts("win: starting the OpenWindows graphical desktop\r\n");
    owx_puts("win: handing the session to startwm\r\n");
    return startwm_main(argc, argv);
}
