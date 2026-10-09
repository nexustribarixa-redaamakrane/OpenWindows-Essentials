/*
 * owdebug64.c - OpenWindows Kernel Debug Support Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owdebug64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owdebug64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owdbg_init(void)
{
    (void)owdebug64_ready;
    /* TODO: implement owdbg_init */
}
void owdbg_breakpoint(void)
{
    (void)owdebug64_ready;
    /* TODO: implement owdbg_breakpoint */
}
void owdbg_print(void)
{
    (void)owdebug64_ready;
    /* TODO: implement owdbg_print */
}
void owdbg_dump_regs(void)
{
    (void)owdebug64_ready;
    /* TODO: implement owdbg_dump_regs */
}
void owdbg_stack_trace(void)
{
    (void)owdebug64_ready;
    /* TODO: implement owdbg_stack_trace */
}

