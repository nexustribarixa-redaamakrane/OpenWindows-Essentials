/*
 * smbios.h - OpenWindows SMBIOS Table Parser (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef SMBIOS_H
#define SMBIOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void smbios_init(void);
void smbios_find_table(void);
void smbios_get_string(void);
void smbios_version(void);

#endif /* SMBIOS_H */

