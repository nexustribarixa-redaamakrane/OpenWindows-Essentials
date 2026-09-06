/*
 * owwm.c - OpenWindows Window Manager Main Process (.owx)
 *
 * Runs the desktop composition loop over owwm64 dynamic library.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include "../../DLL/owwm64/owwm64.h"

static owwm_desktop_t g_desktop;

int owwm_main(void)
{
    owwm64_init(&g_desktop);

    /* Spawn default desktop windows */
    owwm64_create_window(&g_desktop, 1u, "OpenWindows Terminal (owsh)", 100, 100, 640, 400);
    owwm64_create_window(&g_desktop, 1u, "System Monitor (top)", 200, 150, 480, 320);

    return 0;
}
