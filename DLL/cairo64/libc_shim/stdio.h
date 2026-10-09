#ifndef _SHIM_STDIO_H_
#define _SHIM_STDIO_H_
#include <stddef.h>
#include <stdarg.h>
#include "../owrt/ow_runtime.h" // IWYU pragma: export
#ifndef NULL
#define NULL ((void*)0)
#endif
#ifndef EOF
#define EOF (-1)
#endif
typedef void FILE;
#define stdin  ((FILE*)0)
#define stdout ((FILE*)1)
#define stderr ((FILE*)2)
#endif
