/*
 * owkill.c - OpenWindows Frozen Process Killer & Task Terminator (.owx)
 *
 * Runs the terminate path over the task table in DLL/taskmgr64 and the
 * sentinel watchdog in DLL/sentinel64, both of which are linked statically
 * into this image.  Conforms to the OpenWindows non-POSIX verb-driven CLI
 * convention:
 *
 *   owkill <pid>       terminate a process the caller can see
 *   owkill list        show the local task snapshot and the caller's pid
 *   owkill selftest    drive spawn -> freeze -> terminate and report each
 *                      return code the libraries produced
 *   owkill help        show usage information
 *
 * Scope, stated rather than hidden: the Ring 3 gateway in Software/owx_gate.h
 * exposes GET_PID, SLEEP, YIELD, EXIT, SPAWN_OWX, UART and VFS_READ -- there is
 * no cross-process table query and no terminate call in inc/ow_syscall.h.  So
 * owkill terminates its own process for its own pid (a real termination of the
 * real process), reports a foreign pid honestly as absent from the snapshot it
 * holds, and proves the sentinel + task-table terminate path end to end with
 * `owkill selftest`.  When the kernel grows a terminate call, only the lookup
 * arm changes.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"
#include "../../DLL/taskmgr64/taskmgr64.h"
#include "../../DLL/sentinel64/sentinel64.h"

static taskmgr_table_t        g_OwkillTasks;
static sentinel_watchdog_state_t g_OwkillSentinel;

/* ---------------------------------------------------------------- */
/* The terminate path (the module this CLI drives)                  */
/* ---------------------------------------------------------------- */

/*
 * Terminate one process: release it from the sentinel watchdog first so a
 * frozen task is not left with a stale isolation slot, then remove its entry
 * from the master process table.
 *
 *   0   terminated
 *  -1   invalid target (null table, or pid 0)
 *  -2   pid not present in the table
 */
int owkill_terminate(taskmgr_table_t *tasks, sentinel_watchdog_state_t *sentinel,
                     uint32_t target_pid)
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

/* ---------------------------------------------------------------- */
/* Helpers                                                          */
/* ---------------------------------------------------------------- */

/* Read-only predicate: sentinel64 exposes no query-by-pid call, so the frozen
 * arm of the watchdog is inspected through its own (public) task array. */
static bool OwkillIsFrozen(uint32_t pid)
{
    uint32_t i;

    for (i = 0u; i < SENTINEL_MAX_ISOLATED_TASKS; i++) {
        if (g_OwkillSentinel.tasks[i].is_frozen &&
            g_OwkillSentinel.tasks[i].process_id == pid) {
            return true;
        }
    }
    return false;
}

static bool OwkillStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

/* Full-token decimal parse.  Returns false when the token is empty, has a
 * non-digit in it, or would not fit the 32-bit pid the table uses. */
static bool OwkillParsePid(const char *s, uint32_t *out)
{
    uint64_t value = 0u;

    if (!s || !*s || !out) return false;
    while (*s != '\0') {
        if (*s < '0' || *s > '9') return false;
        value = (value * 10u) + (uint64_t)(*s - '0');
        if (value > 0xFFFFFFFFu) return false;
        s++;
    }
    *out = (uint32_t)value;
    return true;
}

static const char *OwkillStateToString(proc_state_t state)
{
    switch (state) {
    case PROC_STATE_INACTIVE: return "INACTIVE";
    case PROC_STATE_ACTIVE:   return "ACTIVE";
    case PROC_STATE_FROZEN:   return "FROZEN";
    case PROC_STATE_CRASHED:  return "CRASHED";
    default:                  return "UNKNOWN";
    }
}

static void OwkillPrintUsage(void)
{
    owx_puts("owkill: OpenWindows process killer and task terminator\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owkill <pid>        terminate a process this caller can see\r\n");
    owx_puts("  owkill list         show the local task snapshot\r\n");
    owx_puts("  owkill selftest     run spawn/freeze/terminate against the libraries\r\n");
    owx_puts("  owkill help         show this message\r\n");
}

/* ---------------------------------------------------------------- */
/* Verbs                                                            */
/* ---------------------------------------------------------------- */

static void OwkillList(void)
{
    uint32_t i;
    int64_t self = owx_getpid();

    owx_puts("owkill: caller pid ");
    owx_put_i64(self);
    owx_puts("\r\n");
    owx_puts("PID     NAME                STATE     THREADS  FROZEN-IN-SENTINEL\r\n");
    owx_puts("------  ------------------  --------  -------  ------------------\r\n");

    for (i = 0u; i < TASKMGR_MAX_PROCESSES; i++) {
        const task_process_entry_t *p = &g_OwkillTasks.processes[i];
        if (p->state == PROC_STATE_INACTIVE) continue;
        owx_put_u64((uint64_t)p->pid);
        owx_puts("     ");
        owx_puts(p->name);
        owx_puts("  ");
        owx_puts(OwkillStateToString(p->state));
        owx_puts("      ");
        owx_put_u64((uint64_t)p->thread_count);
        owx_puts("        ");
        owx_puts(OwkillIsFrozen(p->pid) ? "yes" : "no");
        owx_puts("\r\n");
    }

    owx_puts("owkill: snapshot holds ");
    owx_put_u64((uint64_t)g_OwkillTasks.active_count);
    owx_puts(" entr");
    owx_puts((g_OwkillTasks.active_count == 1u) ? "y" : "ies");
    owx_puts(", sentinel holds ");
    owx_put_u64((uint64_t)sentinel64_get_frozen_count(&g_OwkillSentinel));
    owx_puts("\r\n");
}

/*
 * One row of truth for a stage: print the call, its return code, and whether
 * that matches what the stage was supposed to produce.
 */
static int OwkillExpect(const char *stage, int64_t got, int64_t want)
{
    int ok = (got == want);

    owx_puts(ok ? "[ok]   " : "[FAIL] ");
    owx_puts(stage);
    owx_puts(" -> ");
    owx_put_i64(got);
    if (!ok) {
        owx_puts(" (expected ");
        owx_put_i64(want);
        owx_puts(")");
    }
    owx_puts("\r\n");
    return ok ? 0 : 1;
}

/*
 * Spawn three tasks, freeze one under the sentinel, terminate it through
 * owkill_terminate(), and confirm both libraries observed the removal.  The
 * second terminate must miss (-2) because the entry is already gone, and pid 0
 * must be refused (-1): those two are the error arms of the same path.
 */
static int OwkillSelfTest(void)
{
    int failures = 0;
    int32_t pid_a;
    int32_t pid_b;
    int32_t pid_c;
    int32_t rc;

    owx_puts("=== owkill selftest ===\r\n");

    taskmgr64_init(&g_OwkillTasks);
    sentinel64_init(&g_OwkillSentinel);

    pid_a = taskmgr64_spawn(&g_OwkillTasks, "owterm", 0x1000u);
    pid_b = taskmgr64_spawn(&g_OwkillTasks, "owtop", 0x2000u);
    pid_c = taskmgr64_spawn(&g_OwkillTasks, "owfm", 0x3000u);
    owx_puts("spawned pids: ");
    owx_put_i64((int64_t)pid_a);
    owx_puts(", ");
    owx_put_i64((int64_t)pid_b);
    owx_puts(", ");
    owx_put_i64((int64_t)pid_c);
    owx_puts("\r\n");

    failures += OwkillExpect("spawn slots exhausted check (active_count)",
                             (int64_t)g_OwkillTasks.active_count, 3);
    failures += OwkillExpect("sentinel freeze(pid_b)",
                             sentinel64_freeze_process(&g_OwkillSentinel,
                                                        (uint32_t)pid_b,
                                                        1u, 0x0011A100u, 4242u)
                                 ? 0 : 1, 0);
    failures += OwkillExpect("sentinel frozen count",
                             (int64_t)sentinel64_get_frozen_count(&g_OwkillSentinel), 1);

    rc = owkill_terminate(&g_OwkillTasks, &g_OwkillSentinel, (uint32_t)pid_b);
    failures += OwkillExpect("owkill_terminate(frozen pid_b)", (int64_t)rc, 0);

    failures += OwkillExpect("task table no longer finds pid_b",
                             taskmgr64_find(&g_OwkillTasks, (uint32_t)pid_b) ? 1 : 0, 0);
    failures += OwkillExpect("sentinel released pid_b",
                             (int64_t)sentinel64_get_frozen_count(&g_OwkillSentinel), 0);
    failures += OwkillExpect("active_count decremented",
                             (int64_t)g_OwkillTasks.active_count, 2);
    failures += OwkillExpect("pid_a survives",
                             taskmgr64_find(&g_OwkillTasks, (uint32_t)pid_a) ? 0 : 1, 0);
    failures += OwkillExpect("pid_c survives",
                             taskmgr64_find(&g_OwkillTasks, (uint32_t)pid_c) ? 0 : 1, 0);

    rc = owkill_terminate(&g_OwkillTasks, &g_OwkillSentinel, (uint32_t)pid_b);
    failures += OwkillExpect("second terminate of pid_b (gone)", (int64_t)rc, -2);
    rc = owkill_terminate(&g_OwkillTasks, &g_OwkillSentinel, 0u);
    failures += OwkillExpect("terminate pid 0 (refused)", (int64_t)rc, -1);

    owx_puts("owkill selftest: ");
    owx_put_i64(failures);
    owx_puts(" failure(s)\r\n");
    return (failures == 0) ? 0 : 1;
}

/* ---------------------------------------------------------------- */
/* Entry point                                                      */
/* ---------------------------------------------------------------- */

int owkill_main(int argc, const char *const *argv)
{
    uint32_t pid = 0u;
    int64_t self;

    taskmgr64_init(&g_OwkillTasks);
    sentinel64_init(&g_OwkillSentinel);

    if (argc < 2 || !argv || !argv[1]) {
        OwkillPrintUsage();
        return 1;
    }

    if (OwkillStrEqual(argv[1], "help") ||
        OwkillStrEqual(argv[1], "-h") ||
        OwkillStrEqual(argv[1], "--help")) {
        OwkillPrintUsage();
        return 0;
    }
    if (OwkillStrEqual(argv[1], "list")) {
        OwkillList();
        return 0;
    }
    if (OwkillStrEqual(argv[1], "selftest")) {
        return OwkillSelfTest();
    }
    if (!OwkillParsePid(argv[1], &pid)) {
        owx_puts("owkill: '");
        owx_puts(argv[1]);
        owx_puts("' is not a pid\r\n");
        OwkillPrintUsage();
        return 2;
    }
    if (pid == 0u) {
        owx_puts("owkill: pid 0 is the idle process; refusing\r\n");
        return 1;
    }

    self = owx_getpid();
    if ((int64_t)pid == self) {
        /* The one termination this gateway can perform for real. */
        owx_puts("owkill: terminating own pid ");
        owx_put_i64(self);
        owx_puts("\r\n");
        owx_exit(0);
        return 0; /* unreachable: owx_exit does not return */
    }

    if (taskmgr64_find(&g_OwkillTasks, pid) == NULL) {
        owx_puts("owkill: pid ");
        owx_put_u64((uint64_t)pid);
        owx_puts(" is not in the task snapshot (caller sees pid ");
        owx_put_i64(self);
        owx_puts(" only: the gateway exposes no cross-process table)\r\n");
        return 1;
    }

    if (owkill_terminate(&g_OwkillTasks, &g_OwkillSentinel, pid) == 0) {
        owx_puts("owkill: terminated pid ");
        owx_put_u64((uint64_t)pid);
        owx_puts("\r\n");
        return 0;
    }

    owx_puts("owkill: pid ");
    owx_put_u64((uint64_t)pid);
    owx_puts(" could not be terminated\r\n");
    return 1;
}
