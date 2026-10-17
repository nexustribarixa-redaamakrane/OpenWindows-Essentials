/*
 * ac97.c - OpenWindows AC97 Audio Codec Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * The controller is described by an ac97_backend_t vtable supplied by the
 * caller, so the same code drives I/O-port and memory-mapped AC'97 BARs.
 * The mixer registers live in the NAM BAR and the DMA engine in the NABM BAR.
 */

#include "ac97.h"

/* ── Constants ────────────────────────────────────────────────── */

#define AC97_TIMEOUT_LOOPS      100000u  /* bounded register poll        */
#define AC97_DEFAULT_VOLUME     80u      /* percent applied on init      */
#define AC97_MAX_BDL_ENTRIES    32u      /* 32 entries x 8 bytes         */

/* ── Internal state ───────────────────────────────────────────── */

static const ac97_backend_t *ac97_backend;
static void                 *ac97_context;
static bool                  ac97_initialized;
static bool                  ac97_running;

/* ── Helpers ──────────────────────────────────────────────────── */

/* A usable backend must implement every hook this driver calls. */
static bool ac97_backend_valid(const ac97_backend_t *backend)
{
    if (!backend) {
        return false;
    }
    if (!backend->read8 || !backend->write8) {
        return false;
    }
    if (!backend->read16 || !backend->write16) {
        return false;
    }
    if (!backend->write32) {
        return false;
    }
    return true;
}

/* Byte offset of a codec register inside the NAM BAR. */
static uint32_t ac97_codec_offset(uint8_t codec_reg)
{
    return (uint32_t)codec_reg * 2u;
}

/* Translate a 0..100 level into an AC'97 5-bit-per-channel gain value. */
static uint16_t ac97_volume_value(uint32_t percent)
{
    uint32_t attenuation;
    uint16_t value;

    if (percent > 100u) {
        percent = 100u;
    }
    /* 0x00 is 0 dB (loudest) and 0x1F is -46.5 dB (quietest). */
    attenuation = ((100u - percent) * 31u) / 100u;
    value = (uint16_t)(attenuation | (attenuation << 8));
    if (percent == 0u) {
        value |= 0x8000u;   /* assert the channel mute bit */
    }
    return value;
}

/* ── Codec register access ────────────────────────────────────── */

uint16_t ac97_codec_read(uint8_t codec_reg)
{
    if (!ac97_initialized) {
        return 0u;
    }
    return ac97_backend->read16(ac97_context, AC97_REGION_NAM,
                                ac97_codec_offset(codec_reg));
}

bool ac97_codec_write(uint8_t codec_reg, uint16_t value)
{
    if (!ac97_initialized) {
        return false;
    }
    ac97_backend->write16(ac97_context, AC97_REGION_NAM,
                          ac97_codec_offset(codec_reg), value);
    return true;
}

/* ── Implementation ───────────────────────────────────────────── */

bool ac97_init(const ac97_config_t *config)
{
    uint32_t guard;

    if (!config || !ac97_backend_valid(config->backend)) {
        return false;
    }

    ac97_backend = config->backend;
    ac97_context = config->context;
    ac97_initialized = true;
    ac97_running = false;

    /* Cold reset the codec, then wait for the reset bit to self-clear. */
    (void)ac97_codec_write((uint8_t)AC97_MIXER_RESET, 0x0001u);
    for (guard = 0u; guard < AC97_TIMEOUT_LOOPS; ++guard) {
        if ((ac97_codec_read((uint8_t)AC97_MIXER_RESET) & 0x0001u) == 0u) {
            break;
        }
    }

    /* Apply a safe default gain on master and PCM outputs. */
    (void)ac97_codec_write((uint8_t)AC97_MIXER_MASTER,
                           ac97_volume_value(AC97_DEFAULT_VOLUME));
    (void)ac97_codec_write((uint8_t)AC97_MIXER_PCM_OUT,
                           ac97_volume_value(AC97_DEFAULT_VOLUME));
    return true;
}

bool ac97_set_volume(uint32_t master_percent, uint32_t pcm_percent)
{
    if (!ac97_initialized) {
        return false;
    }
    if (!ac97_codec_write((uint8_t)AC97_MIXER_MASTER,
                          ac97_volume_value(master_percent))) {
        return false;
    }
    return ac97_codec_write((uint8_t)AC97_MIXER_PCM_OUT,
                            ac97_volume_value(pcm_percent));
}

bool ac97_play_buffer(uint32_t bdl_phys_addr, uint16_t entry_count)
{
    if (!ac97_initialized || bdl_phys_addr == 0u) {
        return false;
    }
    if (entry_count == 0u || entry_count > AC97_MAX_BDL_ENTRIES) {
        return false;
    }

    /* Point the engine at the descriptor list and size the run. */
    ac97_backend->write32(ac97_context, AC97_REGION_NABM,
                          AC97_NABM_PO_BDBAR, bdl_phys_addr);
    ac97_backend->write8(ac97_context, AC97_REGION_NABM,
                         AC97_NABM_PO_CIV, 0u);
    ac97_backend->write8(ac97_context, AC97_REGION_NABM,
                         AC97_NABM_PO_LVI, (uint8_t)(entry_count - 1u));

    /* Clear sticky status, then start the bus master. */
    ac97_backend->write16(ac97_context, AC97_REGION_NABM,
                          AC97_NABM_PO_SR, AC97_NABM_SR_CLR);
    ac97_backend->write8(ac97_context, AC97_REGION_NABM,
                         AC97_NABM_PO_CR, AC97_NABM_CR_RUN);
    ac97_running = true;
    return true;
}

bool ac97_stop(void)
{
    uint32_t guard;

    if (!ac97_initialized) {
        return false;
    }

    /* Clear RUN, then assert the register-reset bit and wait for it. */
    ac97_backend->write8(ac97_context, AC97_REGION_NABM, AC97_NABM_PO_CR, 0u);
    ac97_backend->write8(ac97_context, AC97_REGION_NABM,
                         AC97_NABM_PO_CR, AC97_NABM_CR_RESET);

    for (guard = 0u; guard < AC97_TIMEOUT_LOOPS; ++guard) {
        uint8_t cr = ac97_backend->read8(ac97_context, AC97_REGION_NABM,
                                         AC97_NABM_PO_CR);
        if ((cr & AC97_NABM_CR_RESET) == 0u) {
            break;
        }
    }

    ac97_running = false;
    return true;
}

bool ac97_is_running(void)
{
    return ac97_initialized && ac97_running;
}
