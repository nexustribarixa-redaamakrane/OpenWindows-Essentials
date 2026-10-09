/*
 * owelf64.h - OpenWindows OWX Executable Loader (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWELF64_H
#define OWELF64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owelf_load(void);
void owelf_validate(void);
void owelf_relocate(void);
void owelf_resolve_imports(void);
void owelf_get_entry(void);

#endif /* OWELF64_H */

