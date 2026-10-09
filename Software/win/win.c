/*
 * win.c - OpenWindows 'win' GUI Launcher Command (.owx)
 *
 * Spiritual friend to the classic 'win' command, launching the OpenWindows
 * graphical desktop environment directly from the interactive shell or console.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

extern int startwm_main(int argc, const char *const *argv);

int win_main(int argc, const char *const *argv)
{
    return startwm_main(argc, argv);
}
