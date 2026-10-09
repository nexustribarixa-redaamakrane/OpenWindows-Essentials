/*
 * partmgr.h - OpenWindows Partition Manager (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef PARTMGR_H
#define PARTMGR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void partmgr_init(void);
void partmgr_scan(void);
void partmgr_read_mbr(void);
void partmgr_read_gpt(void);

#endif /* PARTMGR_H */

