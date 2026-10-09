#ifndef _SHIM_MATH_H_
#define _SHIM_MATH_H_
#include "../owrt/ow_runtime.h" // IWYU pragma: export
#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.414213562373095048801688724209698079
#endif
#ifndef M_E
#define M_E 2.718281828459045235360287471352662498
#endif
#ifndef HUGE_VAL
#define HUGE_VAL (__builtin_huge_val())
#endif
#ifndef HUGE_VALF
#define HUGE_VALF (__builtin_huge_valf())
#endif
#ifndef INFINITY
#define INFINITY (__builtin_inff())
#endif
#ifndef NAN
#define NAN (__builtin_nanf(""))
#endif
#ifndef isnan
#define isnan(x) __builtin_isnan(x)
#endif
#ifndef isfinite
#define isfinite(x) __builtin_isfinite(x)
#endif
#ifndef isinf
#define isinf(x) __builtin_isinf(x)
#endif
#ifndef signbit
#define signbit(x) __builtin_signbit(x)
#endif
#endif
