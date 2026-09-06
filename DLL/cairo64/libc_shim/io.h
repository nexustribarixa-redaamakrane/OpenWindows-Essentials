#ifndef _SHIM_IO_H_
#define _SHIM_IO_H_
#include <stddef.h>
#include <stdint.h>
#include "../owrt/ow_runtime.h"
int  _close(int fd);
void *_fdopen(int fd, const char *mode);
void *fdopen(int fd, const char *mode);
int  _open_osfhandle(long long h, int flags);
#endif
