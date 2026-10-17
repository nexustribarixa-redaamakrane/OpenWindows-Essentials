/*
 * ac97.h - OpenWindows AC97 Audio Codec Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 * AC'97 mixer + bus-master transport behind a caller-supplied register backend.
 */

#ifndef AC97_H
#define AC97_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Bus master descriptor flags ──────────────────────────────── */

#define AC97_BDL_IOC    0x8000u  /* interrupt on completion            */
#define AC97_BDL_BUP    0x4000u  /* buffer underrun policy             */

/* ── Register map ─────────────────────────────────────────────── */

/* AC'97 codec (mixer) register indices, accessed through the NAM BAR. */
#define AC97_MIXER_RESET        0x00u
#define AC97_MIXER_MASTER       0x02u
#define AC97_MIXER_PCM_OUT      0x16u

/* Native Audio Bus Master PCM-out register offsets. */
#define AC97_NABM_PO_BDBAR      0x10u  /* descriptor list base address   */
#define AC97_NABM_PO_CIV        0x14u  /* current index value            */
#define AC97_NABM_PO_LVI        0x15u  /* last valid index               */
#define AC97_NABM_PO_SR         0x16u  /* status register                */
#define AC97_NABM_PO_PICB       0x18u  /* position in current buffer     */
#define AC97_NABM_PO_CR         0x1Bu  /* control register               */

#define AC97_NABM_CR_RUN        0x01u  /* run bus master                 */
#define AC97_NABM_CR_RESET      0x02u  /* reset bus master registers     */
#define AC97_NABM_SR_CLR        0x001Eu/* write-1-to-clear status bits   */

/* ── Types ────────────────────────────────────────────────────── */

/* Which device BAR a backend access targets. */
typedef enum {
    AC97_REGION_NAM  = 0,   /* Native Audio Mixer (codec registers)      */
    AC97_REGION_NABM = 1    /* Native Audio Bus Master                   */
} ac97_region_t;

/* Address space of the audio BAR. */
typedef enum {
    AC97_BAR_IO = 0,
    AC97_BAR_MMIO
} ac97_bar_kind_t;

/* Physical bus-master descriptor entry, as read by the controller. */
typedef struct {
    uint32_t base_addr;     /* 32-bit physical address of the PCM buffer */
    uint16_t length;        /* number of samples in the buffer           */
    uint16_t flags;         /* AC97_BDL_IOC / AC97_BDL_BUP               */
} ac97_bdl_entry_t;

/* Caller-provided register access backend. All six hooks are required. */
typedef struct {
    uint8_t  (*read8)(void *context, ac97_region_t region, uint32_t offset);
    void     (*write8)(void *context, ac97_region_t region, uint32_t offset, uint8_t value);
    uint16_t (*read16)(void *context, ac97_region_t region, uint32_t offset);
    void     (*write16)(void *context, ac97_region_t region, uint32_t offset, uint16_t value);
    uint32_t (*read32)(void *context, ac97_region_t region, uint32_t offset);
    void     (*write32)(void *context, ac97_region_t region, uint32_t offset, uint32_t value);
} ac97_backend_t;

/* Controller description handed to ac97_init(). */
typedef struct {
    const ac97_backend_t *backend;    /* register access backend          */
    void                 *context;    /* opaque backend context           */
    ac97_bar_kind_t       kind;       /* BAR address space (informational) */
    uint32_t              nam_base;   /* mixer BAR base (informational)    */
    uint32_t              nabm_base;  /* bus-master BAR base (informational) */
} ac97_config_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Reset the codec and install a default volume. */
bool ac97_init(const ac97_config_t *config);

/* Set master and PCM output volume, each in the range 0..100. */
bool ac97_set_volume(uint32_t master_percent, uint32_t pcm_percent);

/* Program the descriptor list and start bus-master playback. */
bool ac97_play_buffer(uint32_t bdl_phys_addr, uint16_t entry_count);

/* Halt playback and reset the bus-master engine. */
bool ac97_stop(void);

/* Report whether the bus-master engine is currently running. */
bool ac97_is_running(void);

/* Read / write one AC'97 codec register through the NAM BAR. */
uint16_t ac97_codec_read(uint8_t codec_reg);
bool ac97_codec_write(uint8_t codec_reg, uint16_t value);

#endif /* AC97_H */
