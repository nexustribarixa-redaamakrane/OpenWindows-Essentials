/*
 * owdev64.h - OpenWindows Device Manager Library (.owd)
 *
 * Central device registry: drivers register a device descriptor and
 * receive a stable handle, and clients look devices up by name or by
 * handle and enumerate the whole table. All storage is a fixed static
 * table; nothing is heap allocated.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWDEV64_H
#define OWDEV64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Limits ───────────────────────────────────────────────────── */

#define OWDEV64_MAX_DEVICES     32u
#define OWDEV64_NAME_MAX        32u
#define OWDEV64_INVALID_ID      0u

/* ── Device classification ────────────────────────────────────── */

typedef enum {
    OWDEV_KIND_UNKNOWN = 0,
    OWDEV_KIND_BUS,
    OWDEV_KIND_STORAGE,
    OWDEV_KIND_NETWORK,
    OWDEV_KIND_DISPLAY,
    OWDEV_KIND_INPUT,
    OWDEV_KIND_AUDIO,
    OWDEV_KIND_MISC
} owdev_kind_t;

/* ── Public types ─────────────────────────────────────────────── */

typedef uint32_t owdev_id_t;

/* Descriptor supplied by the registering driver. */
typedef struct {
    char          name[OWDEV64_NAME_MAX];   /* unique device name      */
    owdev_kind_t  kind;                     /* classification          */
    uint16_t      vendor_id;                /* optional vendor id      */
    uint16_t      device_id;                /* optional device id      */
    uint32_t      bus;                      /* parent bus index        */
    uint32_t      slot;                     /* bus slot / address      */
    uint32_t      irq;                      /* assigned interrupt line */
    uint32_t      flags;                    /* OWDEV_FLAG_* bits       */
} owdev_desc_t;

/* Full record returned by lookups and enumeration. */
typedef struct {
    owdev_id_t    id;                       /* registration handle     */
    char          name[OWDEV64_NAME_MAX];   /* unique device name      */
    owdev_kind_t  kind;                     /* classification          */
    uint16_t      vendor_id;                /* vendor identifier       */
    uint16_t      device_id;                /* device identifier       */
    uint32_t      bus;                      /* parent bus index        */
    uint32_t      slot;                     /* bus slot / address      */
    uint32_t      irq;                      /* assigned interrupt line */
    uint32_t      flags;                    /* OWDEV_FLAG_* bits       */
} owdev_record_t;

/* Device flags. */
#define OWDEV_FLAG_PRESENT      0x00000001u
#define OWDEV_FLAG_REMOVABLE    0x00000002u
#define OWDEV_FLAG_BOOT         0x00000004u

/* ── API ──────────────────────────────────────────────────────── */

/* Reset the device table to empty. */
void owdev_init(void);

/* Register a device described by *desc; returns its handle, or
 * OWDEV_INVALID_ID if the descriptor is rejected or the table is full. */
owdev_id_t owdev_register(const owdev_desc_t *desc);

/* Remove the device with the given handle; true if it existed. */
bool owdev_unregister(owdev_id_t id);

/* Find the first device whose name equals *name. */
bool owdev_find(const char *name, owdev_record_t *out);

/* Fetch the record for a specific handle. */
bool owdev_find_by_id(owdev_id_t id, owdev_record_t *out);

/* Copy up to capacity records into out; returns the number copied. */
uint32_t owdev_enumerate(owdev_record_t *out, uint32_t capacity);

/* Fetch the record for a handle (alias of owdev_find_by_id). */
bool owdev_get_info(owdev_id_t id, owdev_record_t *out);

#endif /* OWDEV64_H */
