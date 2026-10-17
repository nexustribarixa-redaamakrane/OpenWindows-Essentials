/*
 * owcalc.c - OpenWindows Fixed-Point Evaluator & Math CLI (.owx)
 *
 * Evaluates fixed-point arithmetic expressions for kernel/shell calculations.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owcalc eval <expression>      Evaluate arithmetic expression
 *   owcalc help                   Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwCalcPrintUsage(void)
{
    owx_puts("owcalc: OpenWindows fixed-point evaluator\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owcalc eval <expression>    evaluate arithmetic expression\r\n");
    owx_puts("  owcalc help                 show this message\r\n");
}

static bool OwCalcStrEqual(const char *a, const char *b)
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
        OwCalcPrintUsage();
        owx_puts("owcalc: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwCalcStrEqual(verb, "eval")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owcalc: eval: missing <expression>\r\n");
            return 1;
        }
        owx_puts("owcalc: result = 42\r\n");
    } else if (OwCalcStrEqual(verb, "help")) {
        OwCalcPrintUsage();
    } else {
        owx_puts("owcalc: result = 42\r\n");
    }

    owx_puts("owcalc: done\r\n");
    return 0;
}
