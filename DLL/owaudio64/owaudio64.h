/*
 * owaudio64.h - OpenWindows Audio Mixer Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWAUDIO64_H
#define OWAUDIO64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owaudio_init(void);
void owaudio_play(void);
void owaudio_stop(void);
void owaudio_set_volume(void);
void owaudio_get_devices(void);

#endif /* OWAUDIO64_H */

