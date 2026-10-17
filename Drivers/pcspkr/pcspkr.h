/*
 * pcspkr.h - OpenWindows PC Speaker Beep Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 * 8253/8254 channel-2 tone generation through port 0x61.
 */

#ifndef PCSPKR_H
#define PCSPKR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Hardware constants ───────────────────────────────────────── */

#define PCSPKR_PIT_CH2_DATA   0x42u      /* channel 2 data port          */
#define PCSPKR_PIT_COMMAND    0x43u      /* mode/command port            */
#define PCSPKR_CONTROL_PORT   0x61u      /* port B: gate + data enable   */
#define PCSPKR_PIT_BASE_HZ    1193182u   /* 8253/8254 input clock        */

#define PCSPKR_FREQ_MIN_HZ    20u        /* inaudible below this         */
#define PCSPKR_FREQ_MAX_HZ    20000u     /* clamp to the audible range   */
#define PCSPKR_DURATION_MAX   60000u     /* cap a single beep at 60 s    */

/* ── API ──────────────────────────────────────────────────────── */

/* Prepare the PIT channel and silence the speaker. */
bool pcspkr_init(void);

/* Program a tone, hold it for duration_ms, then silence it. */
bool pcspkr_beep(uint32_t frequency_hz, uint32_t duration_ms);

/* Program a tone and leave it running until pcspkr_stop(). */
bool pcspkr_set_freq(uint32_t frequency_hz);

/* Silence the speaker. */
bool pcspkr_stop(void);

/* Report the most recently programmed frequency (0 when silent). */
uint32_t pcspkr_get_freq(void);

#endif /* PCSPKR_H */
