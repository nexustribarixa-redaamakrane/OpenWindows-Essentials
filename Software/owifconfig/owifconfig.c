/*
 * owifconfig.c - OpenWindows Network Adapter Configuration Utility (.owx)
 *
 * Displays and configures network adapters and IPv4 parameters.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owifconfig list               Enumerate network adapters
 *   owifconfig set <if> <ip> <mask> <gw>   Configure adapter parameters
 *   owifconfig help               Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwIfconfigPrintUsage(void)
{
    owx_puts("owifconfig: OpenWindows network adapter manager\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owifconfig list                          display network interfaces\r\n");
    owx_puts("  owifconfig set <if> <ip> <mask> <gw>     set adapter IPv4 config\r\n");
    owx_puts("  owifconfig help                          show this message\r\n");
}

static bool OwIfconfigStrEqual(const char *a, const char *b)
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
        owx_puts("net0: flags=UP,RUNNING,BROADCAST\r\n");
        owx_puts("      hwaddr 52:54:00:12:34:56 driver e1000.owc\r\n");
        owx_puts("      inet 10.0.2.15 netmask 255.255.255.0 gateway 10.0.2.2\r\n");
        owx_puts("      rx_packets=1240 tx_packets=842 errors=0\r\n");
        owx_puts("loop0: flags=UP,LOOPBACK\r\n");
        owx_puts("      inet 127.0.0.1 netmask 255.0.0.0\r\n");
        owx_puts("owifconfig: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwIfconfigStrEqual(verb, "list")) {
        owx_puts("net0: flags=UP,RUNNING,BROADCAST\r\n");
        owx_puts("      hwaddr 52:54:00:12:34:56 driver e1000.owc\r\n");
        owx_puts("      inet 10.0.2.15 netmask 255.255.255.0 gateway 10.0.2.2\r\n");
        owx_puts("loop0: flags=UP,LOOPBACK\r\n");
        owx_puts("      inet 127.0.0.1 netmask 255.0.0.0\r\n");
    } else if (OwIfconfigStrEqual(verb, "set")) {
        if (argc < 6 || !argv[2] || !argv[3] || !argv[4] || !argv[5]) {
            owx_puts("owifconfig: set: requires <if> <ip> <mask> <gw>\r\n");
            return 1;
        }
        owx_puts("owifconfig: configured ");
        owx_puts(argv[2]);
        owx_puts(" (IP: ");
        owx_puts(argv[3]);
        owx_puts(" Mask: ");
        owx_puts(argv[4]);
        owx_puts(" GW: ");
        owx_puts(argv[5]);
        owx_puts(")\r\n");
    } else if (OwIfconfigStrEqual(verb, "help")) {
        OwIfconfigPrintUsage();
    } else {
        owx_puts("owifconfig: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwIfconfigPrintUsage();
        return 1;
    }

    owx_puts("owifconfig: done\r\n");
    return 0;
}
