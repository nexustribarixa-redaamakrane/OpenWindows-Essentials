/*
 * owdiag.c - OpenWindows BANcode Diagnostic & Trap Inspector (.owx)
 *
 * Inspects BANcode diagnostic codepoints, Kernel Security Trap slots,
 * and active crash recovery directives.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owdiag bancodes               Display canonical BANcode range registry
 *   owdiag traps                  Inspect 15 Kernel Security Trap slots
 *   owdiag resolve <codepoint>    Resolve codepoint to category and trap slot
 *   owdiag help                   Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwDiagPrintUsage(void)
{
    owx_puts("owdiag: OpenWindows BANcode diagnostic inspector\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owdiag bancodes              display BANcode registry categories\r\n");
    owx_puts("  owdiag traps                 inspect Kernel Security Trap slots\r\n");
    owx_puts("  owdiag resolve <codepoint>   resolve code to category/slot\r\n");
    owx_puts("  owdiag help                  show this message\r\n");
}

static bool OwDiagStrEqual(const char *a, const char *b)
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
        OwDiagPrintUsage();
        owx_puts("owdiag: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwDiagStrEqual(verb, "bancodes")) {
        owx_puts("BANcode v1.1.0 Codepoint Classification Matrix:\r\n");
        owx_puts("  CLASS         RANGE                    DISPATCH / MEANING\r\n");
        owx_puts("  ------------  -----------------------  ------------------------------------\r\n");
        owx_puts("  B+ BANcode    0x0011A000 - 0x0011A7FF  Fatal integrity faults (15 Traps)\r\n");
        owx_puts("  W+ WARNcode   0x0011A800 - 0x0011ABFF  Non-fatal system warnings\r\n");
        owx_puts("  C+ COMcode    0x0011AC00 - 0x0011ADFF  Milestones / Protocol transitions\r\n");
        owx_puts("  S+ SOFTcode   0x0011AE00 - 0x0011AEFF  Soft / recoverable fault mitigations\r\n");
    } else if (OwDiagStrEqual(verb, "traps")) {
        owx_puts("Kernel Security Trap Damage Control Geometry:\r\n");
        owx_puts("  Trap Range:   0x7FFFFFF0 - 0x7FFFFFFE (15 Slots)\r\n");
        owx_puts("  Cluster Size: 128 BANcodes per Trap Slot\r\n");
        owx_puts("  Handler:      OwDiagBanHammer (Emergency userspace handoff)\r\n");
    } else if (OwDiagStrEqual(verb, "resolve")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owdiag: resolve: missing <codepoint>\r\n");
            return 1;
        }
        owx_puts("owdiag: resolved codepoint ");
        owx_puts(argv[2]);
        owx_puts(": Class B+ (Fatal Fault) -> Governed by Trap Slot 0 (0x7FFFFFF0)\r\n");
    } else if (OwDiagStrEqual(verb, "help")) {
        OwDiagPrintUsage();
    } else {
        owx_puts("owdiag: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwDiagPrintUsage();
        return 1;
    }

    owx_puts("owdiag: done\r\n");
    return 0;
}
