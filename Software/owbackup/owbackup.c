/*
 * owbackup.c - OpenWindows Backup Archive Builder (.owx)
 *
 * Reads a list of files from the primary OWFS volume through the Ring 3
 * gateway and packs them into a single self-describing archive built in a
 * static buffer.  The container:
 *
 *   header   4x "OWBK", u16 version, u16 flags, u32 entry_count, u32 header_crc
 *   record   u32 name_len, u32 data_len, u32 rec_crc, name bytes, data bytes
 *   trailer  4x "OWEN", u32 entry_count, u32 archive_crc
 *
 * Every CRC32c is the same rolling polynomial the storage tree uses, so the
 * archive is verifiable independently of the code that wrote it.  After
 * building, the archive is re-parsed and each record's CRC re-checked before
 * anything is reported as a success -- a backup that cannot be read back is
 * not a backup.
 *
 * The gateway exposes no write syscall, so the archive is produced and
 * verified in memory; `-o` records the intended destination name in the report.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWBACKUP_VERSION "1.0.0"

#define OWBACKUP_MAX_ARCHIVE  65536u   /* whole archive lives here       */
#define OWBACKUP_MAX_FILE     32768u   /* one input file at a time       */
#define OWBACKUP_MAX_ENTRIES  64u
#define OWBACKUP_NAME_MAX     128u

#define OWBACKUP_HEADER_SIZE  16u
#define OWBACKUP_RECORD_SIZE  12u
#define OWBACKUP_TRAILER_SIZE 12u

/* ── Buffers ──────────────────────────────────────────────────── */

static union {
    uint8_t  bytes[OWBACKUP_MAX_ARCHIVE];
    uint64_t align;
} s_archive;

static union {
    uint8_t  bytes[OWBACKUP_MAX_FILE];
    uint64_t align;
} s_file;

static char s_outname[OWBACKUP_NAME_MAX];

/* ── Forward declarations ─────────────────────────────────────── */

static int  owbackup_run(int argc, const char *const *argv);
static void owbackup_usage(void);

/* ── CRC32c ───────────────────────────────────────────────────── */

static uint32_t owbackup_crc32c_byte(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (int i = 0; i < 8; i++) {
        if ((crc & 1u) != 0u) {
            crc = (crc >> 1) ^ 0x82F63B78ul;
        } else {
            crc = crc >> 1;
        }
    }
    return crc;
}

static uint32_t owbackup_crc32c(const uint8_t *p, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFul;
    uint32_t i;
    for (i = 0u; i < len; i++) {
        crc = owbackup_crc32c_byte(crc, p[i]);
    }
    return ~crc;
}

/* ── Little-endian helpers ────────────────────────────────────── */

static uint32_t owbackup_rd32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void owbackup_wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void owbackup_wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static bool owbackup_magic(const uint8_t *p, const char *m)
{
    return p[0] == (uint8_t)m[0] && p[1] == (uint8_t)m[1] &&
           p[2] == (uint8_t)m[2] && p[3] == (uint8_t)m[3];
}

static void owbackup_copy(uint8_t *dst, const uint8_t *src, uint32_t n)
{
    uint32_t i;
    for (i = 0u; i < n; i++) {
        dst[i] = src[i];
    }
}

/* ── Building ─────────────────────────────────────────────────── */

typedef struct {
    uint32_t offset;   /* absolute offset of the record prefix */
    uint32_t name_len;
    uint32_t data_len;
} owbackup_record_t;

static uint32_t s_archive_len;
static uint32_t s_entry_count;
static owbackup_record_t s_index[OWBACKUP_MAX_ENTRIES];

static void owbackup_start(void)
{
    uint8_t *h = s_archive.bytes;
    s_archive_len = 0u;
    s_entry_count = 0u;

    h[0] = 'O'; h[1] = 'W'; h[2] = 'B'; h[3] = 'K';
    owbackup_wr16(&h[4], 1u);
    owbackup_wr16(&h[6], 0u);
    owbackup_wr32(&h[8], 0u);
    owbackup_wr32(&h[12], owbackup_crc32c(h, 12u));
    s_archive_len = OWBACKUP_HEADER_SIZE;
}

/* Append one file's content; returns false when it does not fit. */
static bool owbackup_append(const char *name, uint32_t name_len,
                            const uint8_t *data, uint32_t data_len)
{
    if (s_entry_count >= OWBACKUP_MAX_ENTRIES ||
        name_len > OWBACKUP_NAME_MAX) {
        return false;
    }
    if (s_archive_len + OWBACKUP_RECORD_SIZE + name_len + data_len +
        OWBACKUP_TRAILER_SIZE > OWBACKUP_MAX_ARCHIVE) {
        return false;
    }

    uint8_t *r = &s_archive.bytes[s_archive_len];
    owbackup_record_t rec;
    rec.offset = s_archive_len;
    rec.name_len = name_len;
    rec.data_len = data_len;

    owbackup_wr32(&r[0], name_len);
    owbackup_wr32(&r[4], data_len);

    /* Record checksum covers the name and the payload. */
    uint32_t crc = 0xFFFFFFFFul;
    uint32_t i;
    for (i = 0u; i < name_len; i++) {
        crc = owbackup_crc32c_byte(crc, (uint8_t)name[i]);
    }
    for (i = 0u; i < data_len; i++) {
        crc = owbackup_crc32c_byte(crc, data[i]);
    }
    owbackup_wr32(&r[8], ~crc);

    owbackup_copy(&r[OWBACKUP_RECORD_SIZE], (const uint8_t *)name, name_len);
    owbackup_copy(&r[OWBACKUP_RECORD_SIZE + name_len], data, data_len);

    s_archive_len += OWBACKUP_RECORD_SIZE + name_len + data_len;
    s_index[s_entry_count] = rec;
    s_entry_count++;
    return true;
}

static void owbackup_finish(void)
{
    uint8_t *t = &s_archive.bytes[s_archive_len];
    t[0] = 'O'; t[1] = 'W'; t[2] = 'E'; t[3] = 'N';
    owbackup_wr32(&t[4], s_entry_count);
    owbackup_wr32(&t[8], owbackup_crc32c(s_archive.bytes, s_archive_len));
    s_archive_len += OWBACKUP_TRAILER_SIZE;

    owbackup_wr32(&s_archive.bytes[8], s_entry_count);
    owbackup_wr32(&s_archive.bytes[12], owbackup_crc32c(s_archive.bytes, 12u));
}

/* Print the record index that was accumulated while building. */
static void owbackup_print_index(void)
{
    uint32_t i;
    for (i = 0u; i < s_entry_count; i++) {
        const uint8_t *rec = &s_archive.bytes[s_index[i].offset];
        owx_puts("owbackup:   [");
        owx_put_u64((uint64_t)i);
        owx_puts("] offset=");
        owx_put_u64((uint64_t)s_index[i].offset);
        owx_puts(" data=");
        owx_put_u64((uint64_t)s_index[i].data_len);
        owx_puts(" bytes name=");
        uint32_t k;
        for (k = 0u; k < s_index[i].name_len; k++) {
            owx_putc((char)rec[OWBACKUP_RECORD_SIZE + k]);
        }
        owx_puts("\r\n");
    }
}

/* ── Read-back verification ───────────────────────────────────── */

static bool owbackup_verify(uint32_t *out_entries)
{
    const uint8_t *buf = s_archive.bytes;
    uint32_t pos;
    uint32_t count = 0u;

    if (s_archive_len < OWBACKUP_HEADER_SIZE + OWBACKUP_TRAILER_SIZE) {
        return false;
    }
    if (!owbackup_magic(buf, "OWBK")) {
        return false;
    }
    if (owbackup_rd32(&buf[12]) != owbackup_crc32c(buf, 12u)) {
        return false;
    }
    count = owbackup_rd32(&buf[8]);

    pos = OWBACKUP_HEADER_SIZE;
    while (count > 0u) {
        if (pos + OWBACKUP_RECORD_SIZE > s_archive_len) {
            return false;
        }
        uint32_t name_len = owbackup_rd32(&buf[pos]);
        uint32_t data_len = owbackup_rd32(&buf[pos + 4u]);
        uint32_t stored = owbackup_rd32(&buf[pos + 8u]);
        uint32_t total = OWBACKUP_RECORD_SIZE + name_len + data_len;
        if ((uint64_t)pos + total > (uint64_t)s_archive_len) {
            return false;
        }
        const uint8_t *payload = &buf[pos + OWBACKUP_RECORD_SIZE];
        uint32_t crc = 0xFFFFFFFFul;
        uint32_t i;
        for (i = 0u; i < name_len; i++) {
            crc = owbackup_crc32c_byte(crc, payload[i]);
        }
        for (i = 0u; i < data_len; i++) {
            crc = owbackup_crc32c_byte(crc, payload[name_len + i]);
        }
        if (~crc != stored) {
            return false;
        }
        pos += total;
        count--;
    }

    if (pos + OWBACKUP_TRAILER_SIZE > s_archive_len) {
        return false;
    }
    if (!owbackup_magic(&buf[pos], "OWEN")) {
        return false;
    }
    if (owbackup_crc32c(buf, pos) != owbackup_rd32(&buf[pos + 8u])) {
        return false;
    }
    *out_entries = owbackup_rd32(&buf[8]);
    return true;
}

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owbackup_usage();
        return 1;
    }
    return owbackup_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static bool owbackup_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static uint32_t owbackup_strlen(const char *s)
{
    uint32_t n = 0u;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static bool owbackup_copy_str(char *dst, const char *src, uint32_t cap)
{
    uint32_t i = 0u;
    while (src[i] != '\0') {
        if (i + 1u >= cap) {
            return false;
        }
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return true;
}

static int owbackup_run(int argc, const char *const *argv)
{
    int first_file = -1;
    int i;

    s_outname[0] = '\0';
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (a == NULL) {
            continue;
        }
        if (owbackup_streq(a, "-h") || owbackup_streq(a, "--help")) {
            owbackup_usage();
            return 0;
        } else if (owbackup_streq(a, "-o") && i + 1 < argc) {
            if (!owbackup_copy_str(s_outname, argv[++i], OWBACKUP_NAME_MAX)) {
                owx_puts("owbackup: output name too long\r\n");
                return 1;
            }
        } else if (a[0] == '-') {
            owx_puts("owbackup: unknown option: ");
            owx_puts(a);
            owx_puts("\r\n");
            owbackup_usage();
            return 1;
        } else if (first_file < 0) {
            first_file = i;
        }
    }

    if (first_file < 0) {
        owbackup_usage();
        return 1;
    }

    owbackup_start();

    for (i = first_file; i < argc; i++) {
        const char *path = argv[i];
        if (path == NULL || path[0] == '-') {
            continue;
        }
        int64_t n = owx_file_read(path, s_file.bytes, OWBACKUP_MAX_FILE);
        if (n < 0) {
            owx_puts("owbackup: cannot read ");
            owx_puts(path);
            owx_puts(" (code=");
            owx_put_i64(n);
            owx_puts(")\r\n");
            return 1;
        }
        uint32_t name_len = owbackup_strlen(path);
        if (n > (int64_t)OWBACKUP_MAX_FILE) {
            n = (int64_t)OWBACKUP_MAX_FILE;
        }
        if (!owbackup_append(path, name_len, s_file.bytes, (uint32_t)n)) {
            owx_puts("owbackup: archive full while adding ");
            owx_puts(path);
            owx_puts("\r\n");
            return 1;
        }
        owx_puts("owbackup: added ");
        owx_puts(path);
        owx_puts(" (");
        owx_put_u64((uint64_t)n);
        owx_puts(" bytes)\r\n");
    }

    owbackup_finish();
    owbackup_print_index();

    uint32_t verified_entries = 0u;
    bool ok = owbackup_verify(&verified_entries);

    owx_puts("owbackup: archive ");
    owx_put_u64((uint64_t)s_archive_len);
    owx_puts(" bytes, ");
    owx_put_u64((uint64_t)s_entry_count);
    owx_puts(" entry(ies)\r\n");
    owx_puts("owbackup: verify: ");
    owx_puts(ok ? "OK" : "FAILED");
    owx_puts(" (");
    owx_put_u64((uint64_t)verified_entries);
    owx_puts(" entries re-read)\r\n");
    owx_puts("owbackup: output ");
    owx_puts(s_outname[0] != '\0' ? s_outname : "(unnamed)");
    owx_puts(" - built in memory (no VFS write syscall)\r\n");
    return ok ? 0 : 2;
}

static void owbackup_usage(void)
{
    owx_puts("owbackup " OWBACKUP_VERSION " - build an OWBK archive\r\n");
    owx_puts("usage: owbackup [-o output] <file...>\r\n");
    owx_puts("  -o output   name to report for the built archive\r\n");
}
