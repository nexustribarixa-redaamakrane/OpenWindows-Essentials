#ifndef _SHIM_CTYPE_H_
#define _SHIM_CTYPE_H_
#include "../owrt/ow_runtime.h" // IWYU pragma: export
static inline int tolower(int c) { return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c; }
static inline int toupper(int c) { return (c >= 'a' && c <= 'z') ? (c - ('a' - 'A')) : c; }
static inline int isxdigit(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static inline int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static inline int isalnum(int c) { return isalpha(c) || isdigit(c); }
#endif
