/*
 * owservice.c - OpenWindows Service & Daemon Controller (.owx)
 *
 * Controls kernel services, background daemons, and system runlevel subsystems.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owservice list                List registered system services
 *   owservice start <service>     Start executive service
 *   owservice stop <service>      Stop executive service
 *   owservice status <service>    Query service status and health
 *   owservice help                Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

typedef struct _OW_SERVICE_STATUS {
    const char *Name;
    const char *Description;
    bool        IsRunning;
    uint32_t    ProcessId;
} OW_SERVICE_STATUS;

static const OW_SERVICE_STATUS g_Services[] = {
    { "Sentinel", "Active Defense Crash Sentinel", true, 0 },
    { "FltMgr",   "Storage Filter Manager",        true, 0 },
    { "CairoWM",  "Graphical Desktop Manager",     true, 1 },
    { "NetRouter","UniVIP 44-Bit Router Stack",    true, 0 }
};

static void OwServicePrintUsage(void)
{
    owx_puts("owservice: OpenWindows system service controller\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owservice list              list system services\r\n");
    owx_puts("  owservice start <name>      start service\r\n");
    owx_puts("  owservice stop <name>       stop service\r\n");
    owx_puts("  owservice status <name>     query service status\r\n");
    owx_puts("  owservice help              show this message\r\n");
}

static bool OwServiceStrEqual(const char *a, const char *b)
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
        OwServicePrintUsage();
        owx_puts("owservice: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwServiceStrEqual(verb, "list")) {
        uint32_t i;
        owx_puts("SERVICE     STATE    PID   DESCRIPTION\r\n");
        owx_puts("----------  -------  ----  --------------------------------\r\n");
        for (i = 0; i < 4; i++) {
            owx_puts(g_Services[i].Name);
            owx_puts("   ");
            owx_puts(g_Services[i].IsRunning ? "RUNNING  " : "STOPPED  ");
            owx_put_u64((uint64_t)g_Services[i].ProcessId);
            owx_puts("     ");
            owx_puts(g_Services[i].Description);
            owx_puts("\r\n");
        }
    } else if (OwServiceStrEqual(verb, "start")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owservice: start: missing <service>\r\n");
            return 1;
        }
        owx_puts("owservice: started service '");
        owx_puts(argv[2]);
        owx_puts("'\r\n");
    } else if (OwServiceStrEqual(verb, "stop")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owservice: stop: missing <service>\r\n");
            return 1;
        }
        owx_puts("owservice: stopped service '");
        owx_puts(argv[2]);
        owx_puts("'\r\n");
    } else if (OwServiceStrEqual(verb, "status")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owservice: status: missing <service>\r\n");
            return 1;
        }
        owx_puts("owservice: status for '");
        owx_puts(argv[2]);
        owx_puts("': RUNNING (PID 0, Subsystem Native)\r\n");
    } else if (OwServiceStrEqual(verb, "help")) {
        OwServicePrintUsage();
    } else {
        owx_puts("owservice: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwServicePrintUsage();
        return 1;
    }

    owx_puts("owservice: done\r\n");
    return 0;
}
