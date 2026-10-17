/*
 * owps.c - OpenWindows Process & Executive Task Viewer (.owx)
 *
 * Query active executive processes, thread counts, quantum slices,
 * and security principals across the OpenWindows kernel scheduler.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owps list                List active executive processes
 *   owps query <pid>         Inspect process context, VAD, and memory
 *   owps threads <pid>       Display thread table for process
 *   owps help                Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"
#include "../../DLL/owprocfs64/owprocfs64.h"

static void OwPsPrintUsage(void)
{
    owx_puts("owps: OpenWindows executive process and task viewer\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owps list             list active processes\r\n");
    owx_puts("  owps query <pid>      query detailed process telemetry\r\n");
    owx_puts("  owps threads <pid>    list executive threads for process\r\n");
    owx_puts("  owps help             show this message\r\n");
}

static bool OwPsStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static uint32_t OwPsParseDec(const char *s)
{
    uint32_t val = 0;
    if (!s) return 0;
    while (*s >= '0' && *s <= '9') {
        val = (val * 10) + (uint32_t)(*s - '0');
        s++;
    }
    return val;
}

static const char *OwPsStateToString(OW_PROCESS_STATE state)
{
    switch (state) {
    case OW_PS_PROC_CREATED:    return "CREATED";
    case OW_PS_PROC_RUNNING:    return "RUNNING";
    case OW_PS_PROC_TERMINATED: return "TERMINATED";
    default:                    return "UNKNOWN";
    }
}

static void OwPsListAll(void)
{
    OW_SYSTEM_PROCESS_INFORMATION sysinfo;
    OW_STATUS st = OwPsQuerySystemInformation(&sysinfo);
    uint32_t i;

    if (st != OW_SUCCESS) {
        owx_puts("owps: failed to query system processes (st=0x");
        owx_put_u64((uint64_t)st);
        owx_puts(")\r\n");
        return;
    }

    owx_puts("PID   PPID  SID   THRD  STATE       PAGES   IMAGE\r\n");
    owx_puts("----  ----  ----  ----  ----------  ------  --------------------\r\n");

    for (i = 0; i < sysinfo.TotalProcessCount; i++) {
        const OW_PROCESS_ENTRY *p = &sysinfo.Processes[i];
        owx_put_u64(p->ProcessId);
        owx_puts((p->ProcessId < 10) ? "     " : "    ");
        owx_put_u64(p->ParentProcessId);
        owx_puts((p->ParentProcessId < 10) ? "     " : "    ");
        owx_put_u64(p->UserPrincipalSid);
        owx_puts((p->UserPrincipalSid < 10) ? "     " : "    ");
        owx_put_u64(p->ThreadCount);
        owx_puts((p->ThreadCount < 10) ? "     " : "    ");
        owx_puts(OwPsStateToString(p->State));
        owx_puts("     ");
        owx_put_u64(p->WorkingSetPages);
        owx_puts((p->WorkingSetPages < 100) ? "     " : "    ");
        owx_puts(p->ImageName);
        owx_puts("\r\n");
    }
}

static void OwPsQueryPid(uint32_t pid)
{
    OW_PROCESS_ENTRY entry;
    OW_STATUS st = OwPsQueryProcessRecord(pid, &entry);

    if (st != OW_SUCCESS) {
        owx_puts("owps: process PID ");
        owx_put_u64(pid);
        owx_puts(" not found\r\n");
        return;
    }

    owx_puts("owps: process details for PID ");
    owx_put_u64(pid);
    owx_puts(":\r\n");
    owx_puts("  Image name:       ");
    owx_puts(entry.ImageName);
    owx_puts("\r\n  Parent PID:       ");
    owx_put_u64(entry.ParentProcessId);
    owx_puts("\r\n  Principal SID:    ");
    owx_put_u64(entry.UserPrincipalSid);
    owx_puts("\r\n  State:            ");
    owx_puts(OwPsStateToString(entry.State));
    owx_puts("\r\n  Thread count:     ");
    owx_put_u64(entry.ThreadCount);
    owx_puts("\r\n  Working set:      ");
    owx_put_u64(entry.WorkingSetPages);
    owx_puts(" pages (4 KiB)\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwPsListAll();
        owx_puts("owps: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwPsStrEqual(verb, "list")) {
        OwPsListAll();
    } else if (OwPsStrEqual(verb, "query")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owps: query: missing <pid>\r\n");
            return 1;
        }
        OwPsQueryPid(OwPsParseDec(argv[2]));
    } else if (OwPsStrEqual(verb, "help")) {
        OwPsPrintUsage();
    } else {
        owx_puts("owps: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwPsPrintUsage();
        return 1;
    }

    owx_puts("owps: done\r\n");
    return 0;
}
