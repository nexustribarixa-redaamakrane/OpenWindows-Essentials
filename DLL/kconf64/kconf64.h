/*
 * kconf64.h - OpenWindows Hierarchical Configuration Dynamic Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KCONF64_H
#define KCONF64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void kconf64_init(void);
const char *kconf64_get_string(const char *key, const char *default_val);
int64_t kconf64_get_int(const char *key, int64_t default_val);
bool kconf64_set_string(const char *key, const char *val);
bool kconf64_set_int(const char *key, int64_t val);
uint32_t kconf64_count(void);

#endif /* KCONF64_H */
