/*
 * owpipe.h - OpenWindows Named Pipe IPC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWPIPE_H
#define OWPIPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void pipe_init(void);
void pipe_create(void);
void pipe_read(void);
void pipe_write(void);
void pipe_close(void);

#endif /* OWPIPE_H */

