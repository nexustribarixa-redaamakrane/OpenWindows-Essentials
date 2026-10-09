#ifndef _SHIM_TIME_H_
#define _SHIM_TIME_H_
#include <stddef.h>
typedef long long time_t;
typedef long long clock_t;
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};
#endif
