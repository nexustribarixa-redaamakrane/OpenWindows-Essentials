/*
 * owreg64.h - OpenWindows Registry Access Library (.owd)
 *
 * String key/value registry over a flat, dot-separated namespace
 * (for example "System.Boot.Timeout"). The backing store is an array
 * of entries supplied by the caller; this library never allocates and
 * only operates on the caller-provided buffer.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWREG64_H
#define OWREG64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Limits ───────────────────────────────────────────────────── */

#define OWREG64_KEY_MAX     96u     /* including NUL terminator     */
#define OWREG64_VALUE_MAX   128u    /* including NUL terminator     */

/* ── Status codes ─────────────────────────────────────────────── */

typedef enum {
    OWREG_OK           = 0,     /* success                            */
    OWREG_ERR_PARAM    = -1,    /* bad parameter                      */
    OWREG_ERR_NOENT    = -2,    /* key does not exist                 */
    OWREG_ERR_EXISTS   = -3,    /* key already exists                 */
    OWREG_ERR_SPACE    = -4,    /* backing store is full              */
    OWREG_ERR_ACCESS   = -5,    /* handle is not writable             */
    OWREG_ERR_TOOBIG   = -6,    /* value or key exceeds its limit     */
    OWREG_ERR_HANDLE   = -7     /* handle is not valid                */
} owreg_status_t;

/* ── Open flags ───────────────────────────────────────────────── */

#define OWREG_OPEN_READ     0x01u   /* request read access            */
#define OWREG_OPEN_WRITE    0x02u   /* request write access           */
#define OWREG_OPEN_CREATE   0x04u   /* create the key when missing    */

/* ── Backing store ────────────────────────────────────────────── */

/* One key/value binding inside the caller-provided store. */
typedef struct {
    bool   in_use;                      /* slot holds a live key      */
    char   key[OWREG64_KEY_MAX];        /* dot-separated key name     */
    char   value[OWREG64_VALUE_MAX];    /* NUL-terminated value       */
} owreg_entry_t;

/* A view over caller-provided entry storage. */
typedef struct {
    owreg_entry_t *entries;             /* caller-owned entry array   */
    uint32_t       capacity;            /* number of entries          */
    uint32_t       count;               /* live keys                  */
} owreg_store_t;

/* An open registry key. */
typedef struct {
    owreg_store_t *store;               /* owning store               */
    uint32_t       index;               /* entry slot                 */
    bool           writable;            /* write access granted       */
    bool           valid;               /* handle is open             */
} owreg_handle_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Bind a store to a caller-provided entry array and clear it. */
void owreg_store_init(owreg_store_t *store, owreg_entry_t *entries,
                      uint32_t capacity);

/* Open (optionally creating) a key. Returns a handle whose `valid`
 * field is false on failure. */
owreg_handle_t owreg_open(owreg_store_t *store, const char *key,
                          uint32_t flags);

/* Close an open handle; invalidates it. */
bool owreg_close(owreg_handle_t *handle);

/* Copy the value of an open key into out (NUL-terminated). Returns the
 * value length, or a negative owreg_status_t. */
int32_t owreg_read(const owreg_handle_t *handle, char *out, size_t out_cap);

/* Replace the value of a writable open key. */
int32_t owreg_write(owreg_handle_t *handle, const char *value);

/* Delete a key by name. */
bool owreg_delete(owreg_store_t *store, const char *key);

/* Test whether a key exists. */
bool owreg_exists(const owreg_store_t *store, const char *key);

/* Enumerate key names beginning with prefix. Skips the first `start`
 * matches and stores up to max_names pointers in names_out. The return
 * value is the total number of matching keys, which may exceed
 * max_names. Pass prefix == NULL to match every key. */
uint32_t owreg_enum_keys(const owreg_store_t *store, const char *prefix,
                         uint32_t start, const char **names_out,
                         uint32_t max_names);

#endif /* OWREG64_H */
