/*
 * hdaudio.h - OpenWindows Intel HD Audio Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 * MMIO controller, STATESTS codec discovery, immediate verbs and BDL streams.
 */

#ifndef HDAUDIO_H
#define HDAUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Controller register offsets ──────────────────────────────── */

#define HDA_REG_GCAP        0x00u   /* global capabilities (16-bit)      */
#define HDA_REG_VMIN        0x02u   /* version minor (8-bit)             */
#define HDA_REG_VMAJ        0x03u   /* version major (8-bit)             */
#define HDA_REG_GCTL        0x08u   /* global control (32-bit)           */
#define HDA_REG_STATESTS    0x0Eu   /* codec state-change status (16-bit)*/
#define HDA_REG_IC          0x60u   /* immediate command (32-bit)        */
#define HDA_REG_IR          0x64u   /* immediate response (32-bit)       */
#define HDA_REG_IRS         0x68u   /* immediate command status (16-bit) */

#define HDA_GCTL_CRST       0x00000001u  /* controller reset              */
#define HDA_IRS_BUSY        0x0001u      /* command interface busy        */
#define HDA_IRS_VALID       0x0002u      /* response available            */

/* ── Stream descriptor layout ─────────────────────────────────── */

#define HDA_STREAM_BASE     0x80u   /* input stream 0 descriptor base    */
#define HDA_STREAM_STRIDE   0x20u   /* bytes between stream descriptors  */

#define HDA_SD_CTL          0x00u   /* stream control (32-bit)           */
#define HDA_SD_STS          0x03u   /* stream status (8-bit)             */
#define HDA_SD_LPIB         0x04u   /* link position in buffer (32-bit)  */
#define HDA_SD_CBL          0x08u   /* cyclic buffer length (32-bit)     */
#define HDA_SD_LVI          0x0Cu   /* last valid index (16-bit)         */
#define HDA_SD_FORMAT       0x12u   /* stream format (16-bit)            */
#define HDA_SD_BDLPL        0x18u   /* BDL lower address (32-bit)        */
#define HDA_SD_BDLPU        0x1Cu   /* BDL upper address (32-bit)        */

#define HDA_SD_CTL_SRST     0x00000001u  /* stream reset                  */
#define HDA_SD_CTL_RUN      0x00000002u  /* stream DMA run                */
#define HDA_SD_CTL_TAG_MASK 0x00F00000u  /* stream tag field              */
#define HDA_SD_STS_CLR      0x1Cu        /* write-1-to-clear status bits  */
#define HDA_SD_STS_FIFORDY  0x20u        /* FIFO ready                    */

/* ── Codec verbs (Intel HD Audio specification values) ────────── */

#define HDA_VERB_GET_PARAM             0x0F00u
#define HDA_VERB_SET_STREAM_FORMAT     0x0200u
#define HDA_VERB_SET_AMP_GAIN_MUTE     0x0300u
#define HDA_VERB_SET_POWER_STATE       0x0705u
#define HDA_VERB_SET_CONV_STREAM_CHAN  0x0706u
#define HDA_VERB_SET_PIN_WIDGET_CTRL   0x0707u
#define HDA_VERB_GET_PIN_WIDGET_CTRL   0x0F07u
#define HDA_VERB_SET_CODEC_RESET       0x07FFu

/* Parameter IDs used with HDA_VERB_GET_PARAM. */
#define HDA_PARAM_VENDOR_ID            0x00u
#define HDA_PARAM_REV_ID               0x02u
#define HDA_PARAM_NODE_COUNT           0x04u
#define HDA_PARAM_FUNCTION_TYPE        0x05u
#define HDA_PARAM_AUDIO_WIDGET_CAP     0x09u

/* Amp gain/mute payload bits used with HDA_VERB_SET_AMP_GAIN_MUTE. */
#define HDA_AMP_SET_OUTPUT  0x8000u
#define HDA_AMP_SET_INPUT   0x4000u
#define HDA_AMP_SET_LEFT    0x2000u
#define HDA_AMP_SET_RIGHT   0x1000u
#define HDA_AMP_SET_INDEX   0x0F00u
#define HDA_AMP_MUTE        0x0080u
#define HDA_AMP_GAIN        0x007Fu

/* ── Types ────────────────────────────────────────────────────── */

/* Bus-master descriptor list entry (16 bytes on the wire). */
typedef struct {
    uint64_t address;   /* physical address of the sample buffer         */
    uint32_t length;    /* buffer length in bytes                        */
    uint32_t flags;     /* bit0: interrupt on completion (IOC)           */
} hda_bdl_entry_t;

/* Result of controller and codec discovery. */
typedef struct {
    bool     initialized;    /* hda_init() completed successfully        */
    uint8_t  version_major;
    uint8_t  version_minor;
    uint16_t codec_mask;     /* STATESTS bitmask of responding codecs    */
    uint8_t  codec_addr;     /* first responding codec, 0xFF when none   */
    uint32_t out_streams;    /* number of output stream descriptors      */
    uint32_t in_streams;     /* number of input stream descriptors       */
} hda_info_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Bring up the controller at the given MMIO base and discover codecs. */
bool hda_init(volatile uint8_t *mmio_base);

/* Assert then release the controller reset and clear STATESTS. */
bool hda_reset(void);

/* Issue one immediate verb and collect its response. */
bool hda_send_verb(uint8_t codec_addr, uint8_t node_id, uint16_t verb,
                   uint32_t payload, uint32_t *response);

/* Reset a stream, load its BDL and start playback. */
bool hda_play_stream(uint32_t stream_index, uint64_t bdl_phys,
                     uint32_t bdl_entries, uint32_t buffer_bytes,
                     uint16_t format);

/* Stop a stream and put it back into reset. */
bool hda_stop_stream(uint32_t stream_index);

/* Set a node's output amplifier gain, percent in the range 0..100. */
bool hda_set_volume(uint8_t codec_addr, uint8_t node_id, uint32_t percent);

/* Copy the discovery result into *out. */
void hda_get_info(hda_info_t *out);

#endif /* HDAUDIO_H */
