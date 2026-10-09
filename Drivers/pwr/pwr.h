/*
 * pwr.h - OpenWindows Power Management Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef PWR_H
#define PWR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void pwr_init(void);
void pwr_shutdown(void);
void pwr_reboot(void);
void pwr_sleep(void);
void pwr_get_state(void);

#endif /* PWR_H */

