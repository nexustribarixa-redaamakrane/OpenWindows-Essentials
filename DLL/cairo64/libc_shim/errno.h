#ifndef _SHIM_ERRNO_H_
#define _SHIM_ERRNO_H_
#include "../owrt/ow_runtime.h" // IWYU pragma: export
#define errno (*_errno())
#define EDOM   33
#define ERANGE 34
#define ENOMEM 12
#define EINVAL 22
#define EACCES 13
#define EEXIST 17
#define ENOENT 2
#define ENOSPC 28
#endif
