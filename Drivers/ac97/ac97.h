/*
 * ac97.h - OpenWindows AC97 Audio Codec Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef AC97_H
#define AC97_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void ac97_init(void);
void ac97_set_volume(void);
void ac97_play_buffer(void);
void ac97_stop(void);

#endif /* AC97_H */

