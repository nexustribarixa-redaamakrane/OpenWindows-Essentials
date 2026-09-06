/*
 * owkill.c - OpenWindows Frozen Process Killer & Task Terminator (.owx)
 *
 * Scans task tables, identifies unresponsive or sentinel-frozen processes,
 * and forces clean termination and resource release.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../DLL/taskmgr64/taskmgr64.h"
#include "../../DLL/sentinel64/sentinel64.h"

int owkill_terminate(taskmgr_table_t *tasks, sentinel_watchdog_state_t *sentinel, uint32_t target_pid)
{
    if (!tasks || target_pid == 0u) return -1;

    /* 1. Remove from sentinel watchdog if frozen */
    if (sentinel) {
        sentinel64_terminate_frozen(sentinel, target_pid);
    }

    /* 2. Terminate task entry in master process table */
    if (taskmgr64_kill(tasks, target_pid)) {
        return 0; /* Successfully terminated */
    }

    return -2; /* Process ID not found */
}
