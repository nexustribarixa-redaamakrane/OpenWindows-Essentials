/*
 * hdaudio.c - OpenWindows Intel HD Audio Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * The controller is memory mapped.  The driver performs the reset handshake
 * through GCTL.CRST, discovers codecs from STATESTS, talks to them through the
 * immediate command/response interface, and drives playback with a stream
 * descriptor plus a bus-master descriptor list (BDL).
 */

#include "hdaudio.h"

/* ── Constants ────────────────────────────────────────────────── */

#define HDA_TIMEOUT_LOOPS   100000u  /* bounded MMIO poll             */
#define HDA_MAX_STREAMS     8u       /* 4 input + 4 output descriptors */
#define HDA_MAX_BDL_ENTRIES 256u     /* 256 entries x 16 bytes        */

/* ── Internal state ───────────────────────────────────────────── */

static volatile uint8_t *hda_mmio;
static bool              hda_ready;
static hda_info_t        hda_info;

/* ── MMIO access helpers ──────────────────────────────────────── */

static void hda_write8(volatile uint8_t *base, uint32_t offset, uint8_t value)
{
    *(volatile uint8_t *)(base + offset) = value;
}

static uint16_t hda_read16(volatile uint8_t *base, uint32_t offset)
{
    return *(volatile uint16_t *)(base + offset);
}

static void hda_write16(volatile uint8_t *base, uint32_t offset, uint16_t value)
{
    *(volatile uint16_t *)(base + offset) = value;
}

static uint32_t hda_read32(volatile uint8_t *base, uint32_t offset)
{
    return *(volatile uint32_t *)(base + offset);
}

static void hda_write32(volatile uint8_t *base, uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(base + offset) = value;
}

/* ── Verb encoding ────────────────────────────────────────────── */

/*
 * Pack a codec verb plus payload into the 32-bit command frame.  Verbs whose
 * bits 10:8 are all set use the 12-bit/8-bit "short" encoding; every other
 * verb uses the 4-bit/16-bit "long" encoding.
 */
static uint32_t hda_build_verb(uint8_t codec_addr, uint8_t node_id,
                               uint16_t verb, uint32_t payload)
{
    uint32_t command = ((uint32_t)codec_addr << 28) | ((uint32_t)node_id << 20);

    if ((verb & 0x0700u) == 0x0700u) {
        command |= ((uint32_t)verb << 8) | (payload & 0x00FFu);
    } else {
        command |= ((((uint32_t)verb >> 8) & 0x000Fu) << 16)
                   | (payload & 0xFFFFu);
    }
    return command;
}

/* ── Stream helpers ───────────────────────────────────────────── */

/* Byte offset of a stream descriptor from the MMIO base. */
static uint32_t hda_stream_base(uint32_t stream_index)
{
    return HDA_STREAM_BASE + (stream_index * HDA_STREAM_STRIDE);
}

/* Assert and release SRST, waiting for each transition. */
static bool hda_stream_reset(uint32_t stream_index)
{
    uint32_t base = hda_stream_base(stream_index);
    uint32_t ctl;
    uint32_t guard;

    ctl = hda_read32(hda_mmio, base + HDA_SD_CTL);
    hda_write32(hda_mmio, base + HDA_SD_CTL, ctl | HDA_SD_CTL_SRST);

    guard = HDA_TIMEOUT_LOOPS;
    while ((hda_read32(hda_mmio, base + HDA_SD_CTL) & HDA_SD_CTL_SRST) == 0u) {
        if (--guard == 0u) {
            return false;
        }
    }

    hda_write32(hda_mmio, base + HDA_SD_CTL, ctl & ~HDA_SD_CTL_SRST);
    guard = HDA_TIMEOUT_LOOPS;
    while ((hda_read32(hda_mmio, base + HDA_SD_CTL) & HDA_SD_CTL_SRST) != 0u) {
        if (--guard == 0u) {
            return false;
        }
    }
    return true;
}

/* ── Implementation ───────────────────────────────────────────── */

bool hda_init(volatile uint8_t *mmio_base)
{
    uint16_t gcap;
    uint16_t version;
    uint16_t statests;
    uint32_t guard;
    uint8_t addr;

    if (!mmio_base) {
        return false;
    }

    hda_mmio = mmio_base;
    hda_ready = true;

    hda_info.initialized = false;
    hda_info.version_major = 0u;
    hda_info.version_minor = 0u;
    hda_info.codec_mask = 0u;
    hda_info.codec_addr = 0xFFu;
    hda_info.out_streams = 0u;
    hda_info.in_streams = 0u;

    gcap = hda_read16(mmio_base, HDA_REG_GCAP);
    hda_info.out_streams = (uint32_t)((gcap >> 12) & 0x000Fu);
    hda_info.in_streams = (uint32_t)((gcap >> 8) & 0x000Fu);

    version = hda_read16(mmio_base, HDA_REG_VMIN);
    hda_info.version_minor = (uint8_t)(version & 0x00FFu);
    hda_info.version_major = (uint8_t)((version >> 8) & 0x00FFu);

    if (!hda_reset()) {
        return false;
    }

    /* Wait briefly for the codecs to report themselves through STATESTS. */
    statests = 0u;
    for (guard = 0u; guard < HDA_TIMEOUT_LOOPS && statests == 0u; ++guard) {
        statests = hda_read16(mmio_base, HDA_REG_STATESTS);
    }
    hda_info.codec_mask = statests;

    hda_info.codec_addr = 0xFFu;
    for (addr = 0u; addr < 16u; ++addr) {
        if ((statests & (uint16_t)(1u << addr)) != 0u) {
            hda_info.codec_addr = addr;
            break;
        }
    }

    hda_info.initialized = (hda_info.codec_addr != 0xFFu);
    return hda_info.initialized;
}

bool hda_reset(void)
{
    uint32_t guard;

    if (!hda_ready) {
        return false;
    }

    /* Assert controller reset and wait for CRST to clear. */
    hda_write32(hda_mmio, HDA_REG_GCTL,
                hda_read32(hda_mmio, HDA_REG_GCTL) & ~HDA_GCTL_CRST);
    guard = HDA_TIMEOUT_LOOPS;
    while ((hda_read32(hda_mmio, HDA_REG_GCTL) & HDA_GCTL_CRST) != 0u) {
        if (--guard == 0u) {
            return false;
        }
    }

    /* Release reset and wait for CRST to be reported set again. */
    hda_write32(hda_mmio, HDA_REG_GCTL,
                hda_read32(hda_mmio, HDA_REG_GCTL) | HDA_GCTL_CRST);
    guard = HDA_TIMEOUT_LOOPS;
    while ((hda_read32(hda_mmio, HDA_REG_GCTL) & HDA_GCTL_CRST) == 0u) {
        if (--guard == 0u) {
            return false;
        }
    }

    /* Discard any stale codec state-change bits. */
    hda_write16(hda_mmio, HDA_REG_STATESTS, 0xFFFFu);
    return true;
}

bool hda_send_verb(uint8_t codec_addr, uint8_t node_id, uint16_t verb,
                   uint32_t payload, uint32_t *response)
{
    uint32_t command;
    uint32_t guard;

    if (!hda_ready) {
        return false;
    }
    if (codec_addr > 0x0Fu || node_id > 0x7Fu) {
        return false;
    }

    command = hda_build_verb(codec_addr, node_id, verb, payload);

    /* Wait for the command interface to become free. */
    guard = HDA_TIMEOUT_LOOPS;
    while ((hda_read16(hda_mmio, HDA_REG_IRS) & HDA_IRS_BUSY) != 0u) {
        if (--guard == 0u) {
            return false;
        }
    }
    hda_write32(hda_mmio, HDA_REG_IC, command);

    /* Wait for the response to become valid, then collect it. */
    guard = HDA_TIMEOUT_LOOPS;
    while ((hda_read16(hda_mmio, HDA_REG_IRS) & HDA_IRS_VALID) == 0u) {
        if (--guard == 0u) {
            return false;
        }
    }
    if (response) {
        *response = hda_read32(hda_mmio, HDA_REG_IR);
    }
    hda_write16(hda_mmio, HDA_REG_IRS, HDA_IRS_VALID);
    return true;
}

bool hda_play_stream(uint32_t stream_index, uint64_t bdl_phys,
                     uint32_t bdl_entries, uint32_t buffer_bytes,
                     uint16_t format)
{
    uint32_t base;
    uint32_t ctl;

    if (!hda_info.initialized) {
        return false;
    }
    if (stream_index >= HDA_MAX_STREAMS) {
        return false;
    }
    if (bdl_phys == 0u || bdl_entries == 0u || bdl_entries > HDA_MAX_BDL_ENTRIES) {
        return false;
    }

    base = hda_stream_base(stream_index);
    if (!hda_stream_reset(stream_index)) {
        return false;
    }

    /* Program the cyclic buffer geometry, format and descriptor list. */
    hda_write32(hda_mmio, base + HDA_SD_CBL, buffer_bytes);
    hda_write16(hda_mmio, base + HDA_SD_LVI, (uint16_t)(bdl_entries - 1u));
    hda_write16(hda_mmio, base + HDA_SD_FORMAT, format);
    hda_write32(hda_mmio, base + HDA_SD_BDLPL,
                (uint32_t)(bdl_phys & 0xFFFFFFFFu));
    hda_write32(hda_mmio, base + HDA_SD_BDLPU,
                (uint32_t)((bdl_phys >> 32) & 0xFFFFFFFFu));

    /* Clear pending status bits. */
    hda_write8(hda_mmio, base + HDA_SD_STS, (uint8_t)HDA_SD_STS_CLR);

    /* Tag the stream as zero and start DMA. */
    ctl = hda_read32(hda_mmio, base + HDA_SD_CTL);
    ctl &= ~HDA_SD_CTL_TAG_MASK;
    ctl |= HDA_SD_CTL_RUN;
    hda_write32(hda_mmio, base + HDA_SD_CTL, ctl);
    return true;
}

bool hda_stop_stream(uint32_t stream_index)
{
    uint32_t base;
    uint32_t ctl;

    if (!hda_info.initialized) {
        return false;
    }
    if (stream_index >= HDA_MAX_STREAMS) {
        return false;
    }

    base = hda_stream_base(stream_index);

    /* Stop DMA, then return the stream to reset. */
    ctl = hda_read32(hda_mmio, base + HDA_SD_CTL);
    hda_write32(hda_mmio, base + HDA_SD_CTL, ctl & ~HDA_SD_CTL_RUN);
    return hda_stream_reset(stream_index);
}

bool hda_set_volume(uint8_t codec_addr, uint8_t node_id, uint32_t percent)
{
    uint32_t gain;
    uint32_t payload;

    if (!hda_info.initialized) {
        return false;
    }
    if (percent > 100u) {
        percent = 100u;
    }

    /* Map 0..100% onto the 7-bit amplifier gain field. */
    gain = (percent * HDA_AMP_GAIN) / 100u;
    payload = HDA_AMP_SET_OUTPUT | HDA_AMP_SET_LEFT | HDA_AMP_SET_RIGHT | gain;
    if (percent == 0u) {
        payload |= HDA_AMP_MUTE;
    }

    return hda_send_verb(codec_addr, node_id, HDA_VERB_SET_AMP_GAIN_MUTE,
                         payload, NULL);
}

void hda_get_info(hda_info_t *out)
{
    if (!out) {
        return;
    }
    *out = hda_info;
}
