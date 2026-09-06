/*
 * owdlg64.h - OpenWindows Dialog/Message Box Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWDLG64_H
#define OWDLG64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owdlg_init(void);
void owdlg_message(void);
void owdlg_confirm(void);
void owdlg_input(void);
void owdlg_file_open(void);
void owdlg_file_save(void);

#endif /* OWDLG64_H */

