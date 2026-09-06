/*
 * owc_entry.c - thin PE entry stub for .owc component executables.
 *
 * A .owc is an OpenWindows component (the OW equivalent of a .sys/.inf
 * driver image). It is loaded by name and driven through its init hooks,
 * so it needs no CRT startup: this stub just satisfies PE linkage so the
 * GNU ld default (WinMain/crtexewin) is never pulled in.
 */

#if defined(_WIN32)
unsigned long __stdcall owc_entry_stub(void)
{
    return 1ul;
}
#endif
