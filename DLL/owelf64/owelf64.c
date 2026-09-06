/*
 * owelf64.c - OpenWindows OWX Executable Loader (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owelf64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owelf64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owelf_load(void)
{
    (void)owelf64_ready;
    /* TODO: implement owelf_load */
}
void owelf_validate(void)
{
    (void)owelf64_ready;
    /* TODO: implement owelf_validate */
}
void owelf_relocate(void)
{
    (void)owelf64_ready;
    /* TODO: implement owelf_relocate */
}
void owelf_resolve_imports(void)
{
    (void)owelf64_ready;
    /* TODO: implement owelf_resolve_imports */
}
void owelf_get_entry(void)
{
    (void)owelf64_ready;
    /* TODO: implement owelf_get_entry */
}

