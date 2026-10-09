/*
 * owx_entry.c - thin PE/freestanding entry stub for .owx executables.
 *
 * OpenWindows Native Executables (.owx) run in freestanding environments
 * and do not link hosted C runtime startup routines. This stub satisfies
 * GNU ld PE link requirements (-Wl,-e,owx_entry_stub).
 */

typedef int owx_iso_c_declaration_guard;

#if defined(_WIN32)
unsigned long __stdcall owx_entry_stub(void)
{
    return 0ul;
}
#else
unsigned long owx_entry_stub(void)
{
    return 0ul;
}
#endif
