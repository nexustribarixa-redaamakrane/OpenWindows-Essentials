/*
 * owdebug64.h - OpenWindows Kernel Debug Support Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWDEBUG64_H
#define OWDEBUG64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owdbg_init(void);
void owdbg_breakpoint(void);
void owdbg_print(void);
void owdbg_dump_regs(void);
void owdbg_stack_trace(void);

#endif /* OWDEBUG64_H */

