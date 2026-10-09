/*
 * owd_entry.c - thin PE entry stub for .owd libraries.
 *
 * Libraries are loaded by name and driven through their own init hooks, so
 * the mingw DllMainCRTStartup - and with it the whole msvcrt/KERNEL32
 * import set - must not be linked in. This stub just satisfies PE linkage.
 */

#if defined(_WIN32)
unsigned long __stdcall owd_entry_stub(void)
{
    return 1ul;
}
#endif
