/*
 * acpitbl.h - OpenWindows ACPI Table Parser (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef ACPITBL_H
#define ACPITBL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void acpitbl_init(void);
void acpitbl_find_table(void);
void acpitbl_validate_rsdp(void);

#endif /* ACPITBL_H */

