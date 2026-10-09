/*
 * cdrom.h - OpenWindows ATAPI CD-ROM Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef CDROM_H
#define CDROM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void cdrom_init(void);
void cdrom_read_sector(void);
void cdrom_eject(void);
void cdrom_capacity(void);

#endif /* CDROM_H */

