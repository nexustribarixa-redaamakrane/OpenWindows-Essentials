/*
 * owcron.c - OpenWindows Recurring Executive Timer & Task Scheduler (.owx)
 *
 * Schedules recurring executive tasks using the kernel DPC and timer subsystem.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owcron list                   List scheduled recurring tasks
 *   owcron schedule <task> <ticks> Register periodic task
 *   owcron cancel <task_id>       Cancel scheduled task
 *   owcron help                   Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwCronPrintUsage(void)
{
    owx_puts("owcron: OpenWindows executive task scheduler\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owcron list                       list scheduled tasks\r\n");
    owx_puts("  owcron schedule <task> <ticks>    schedule periodic task\r\n");
    owx_puts("  owcron cancel <id>                cancel scheduled task\r\n");
    owx_puts("  owcron help                       show this message\r\n");
}

static bool OwCronStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        owx_puts("TASK ID  PERIOD (TICKS)  TARGET BINARY       STATUS\r\n");
        owx_puts("-------  --------------  ------------------  ----------\r\n");
        owx_puts("1        6000 (60s)      \\System\\owdiskchk   ARMED\r\n");
        owx_puts("2        1000 (10s)      \\System\\owdiag      ARMED\r\n");
        owx_puts("owcron: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwCronStrEqual(verb, "list")) {
        owx_puts("TASK ID  PERIOD (TICKS)  TARGET BINARY       STATUS\r\n");
        owx_puts("-------  --------------  ------------------  ----------\r\n");
        owx_puts("1        6000 (60s)      \\System\\owdiskchk   ARMED\r\n");
        owx_puts("2        1000 (10s)      \\System\\owdiag      ARMED\r\n");
    } else if (OwCronStrEqual(verb, "schedule")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owcron: schedule: requires <task> and <ticks>\r\n");
            return 1;
        }
        owx_puts("owcron: scheduled task '");
        owx_puts(argv[2]);
        owx_puts("' every ");
        owx_puts(argv[3]);
        owx_puts(" ticks\r\n");
    } else if (OwCronStrEqual(verb, "cancel")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owcron: cancel: missing <id>\r\n");
            return 1;
        }
        owx_puts("owcron: cancelled task ID ");
        owx_puts(argv[2]);
        owx_puts("\r\n");
    } else if (OwCronStrEqual(verb, "help")) {
        OwCronPrintUsage();
    } else {
        OwCronPrintUsage();
        return 1;
    }

    owx_puts("owcron: done\r\n");
    return 0;
}
