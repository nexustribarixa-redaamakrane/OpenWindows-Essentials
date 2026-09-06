/*
 * hdaudio.h - OpenWindows Intel HD Audio Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef HDAUDIO_H
#define HDAUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void hda_init(void);
void hda_reset(void);
void hda_play_stream(void);
void hda_stop_stream(void);
void hda_set_volume(void);

#endif /* HDAUDIO_H */

