/*
 * atapio.h - OpenWindows ATA PIO Mode Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef ATAPIO_H
#define ATAPIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void ata_init(void);
void ata_identify(void);
void ata_read_sectors(void);
void ata_write_sectors(void);

#endif /* ATAPIO_H */

