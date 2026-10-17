/*
 * owrestore.c - OpenWindows Backup Archive Verifier/Reader (.owx)
 *
 * Reads an archive written by owbackup off the primary OWFS volume, validates
 * its structure end to end, and lists its contents.  The container is the one
 * documented in owbackup.c:
 *
 *   header   4x "OWBK", u16 version, u16 flags, u32 entry_count, u32 header_crc
 *   record   u32 name_len, u32 data_len, u32 rec_crc, name bytes, data bytes
 *   trailer  4x "OWEN", u32 entry_count, u32 archive_crc
 *
 * Validation is strict: the header CRC, every record's length against the
 * remaining buffer, every record CRC, the trailer magic, and the archive CRC
 * must all agree before a single entry is reported as readable.  A truncated
 * or bit-rotted archive is rejected whole rather than half-listed.
 *
 * The gateway exposes no write syscall, so entries are listed and verified
 * rather than written out; `-v` additionally shows a hex preview of each.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWRESTORE_VERSION "1.0.0"

#define OWRESTORE_MAX_ARCHIVE 65536u
#define OWRESTORE_PREVIEW     16u

#define OWRESTORE_HEADER_SIZE  16u
#define OWRESTORE_RECORD_SIZE  12u
#define OWRESTORE_TRAILER_SIZE 12u

/* ── Buffer ───────────────────────────────────────────────────── */

static union {
    uint8_t  bytes[OWRESTORE_MAX_ARCHIVE];
    uint64_t align;
} s_archive;

/* ── Forward declarations ─────────────────────────────────────── */

static int  owrestore_run(int argc, const char *const *argv);
static void owrestore_usage(void);

/* ── CRC32c ───────────────────────────────────────────────────── */

static uint32_t owrestore_crc32c_byte(uint32_t crc, uint8_t byte)
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

static uint32_t owrestore_crc32c(const uint8_t *p, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFul;
    uint32_t i;
    for (i = 0u; i < len; i++) {
        crc = owrestore_crc32c_byte(crc, p[i]);
    }
    return ~crc;
}

/* ── Little-endian ────────────────────────────────────────────── */

static uint32_t owrestore_rd32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool owrestore_magic(const uint8_t *p, const char *m)
{
    return p[0] == (uint8_t)m[0] && p[1] == (uint8_t)m[1] &&
           p[2] == (uint8_t)m[2] && p[3] == (uint8_t)m[3];
}

static void owrestore_put_hex32(uint32_t v)
{
    static const char d[] = "0123456789abcdef";
    int i;
    for (i = 28; i >= 0; i -= 4) {
        owx_putc(d[(v >> i) & 0xFu]);
    }
}

static void owrestore_put_hex8(uint8_t v)
{
    static const char d[] = "0123456789abcdef";
    owx_putc(d[(v >> 4) & 0xFu]);
    owx_putc(d[v & 0xFu]);
}

/* ── Core walk ────────────────────────────────────────────────── */

/*
 * Validate the whole archive and list it.  Returns 0 when the archive is
 * well-formed, nonzero otherwise.  `verbose` adds a hex preview per entry.
 */
static int owrestore_list(const uint8_t *buf, uint32_t len, bool verbose)
{
    if (len < OWRESTORE_HEADER_SIZE + OWRESTORE_TRAILER_SIZE) {
        owx_puts("owrestore: archive too short (");
        owx_put_u64((uint64_t)len);
        owx_puts(" bytes)\r\n");
        return 1;
    }
    if (!owrestore_magic(buf, "OWBK")) {
        owx_puts("owrestore: bad header magic\r\n");
        return 1;
    }
    uint16_t version = (uint16_t)(buf[4] | ((uint16_t)buf[5] << 8));
    if (version != 1u) {
        owx_puts("owrestore: unsupported version ");
        owx_put_u64((uint64_t)version);
        owx_puts("\r\n");
        return 1;
    }
    if (owrestore_rd32(&buf[12]) != owrestore_crc32c(buf, 12u)) {
        owx_puts("owrestore: header CRC mismatch\r\n");
        return 1;
    }

    uint32_t count = owrestore_rd32(&buf[8]);
    owx_puts("owrestore: archive version ");
    owx_put_u64((uint64_t)version);
    owx_puts(", ");
    owx_put_u64((uint64_t)count);
    owx_puts(" recorded entry(ies)\r\n");

    uint32_t pos = OWRESTORE_HEADER_SIZE;
    uint32_t seen = 0u;
    while (seen < count) {
        if (pos + OWRESTORE_RECORD_SIZE > len) {
            owx_puts("owrestore: truncated at record ");
            owx_put_u64((uint64_t)seen);
            owx_puts("\r\n");
            return 1;
        }
        uint32_t name_len = owrestore_rd32(&buf[pos]);
        uint32_t data_len = owrestore_rd32(&buf[pos + 4u]);
        uint32_t stored = owrestore_rd32(&buf[pos + 8u]);
        if (name_len > OWRESTORE_MAX_ARCHIVE || data_len > OWRESTORE_MAX_ARCHIVE) {
            owx_puts("owrestore: record ");
            owx_put_u64((uint64_t)seen);
            owx_puts(": length out of range\r\n");
            return 1;
        }
        uint32_t total = OWRESTORE_RECORD_SIZE + name_len + data_len;
        if ((uint64_t)pos + total > (uint64_t)len) {
            owx_puts("owrestore: record ");
            owx_put_u64((uint64_t)seen);
            owx_puts(": length runs past end of archive\r\n");
            return 1;
        }

        const uint8_t *payload = &buf[pos + OWRESTORE_RECORD_SIZE];
        uint32_t crc = 0xFFFFFFFFul;
        uint32_t i;
        for (i = 0u; i < name_len; i++) {
            crc = owrestore_crc32c_byte(crc, payload[i]);
        }
        for (i = 0u; i < data_len; i++) {
            crc = owrestore_crc32c_byte(crc, payload[name_len + i]);
        }
        bool crc_ok = (~crc == stored);

        owx_puts("owrestore:   [");
        owx_put_u64((uint64_t)seen);
        owx_puts("] ");
        for (i = 0u; i < name_len; i++) {
            owx_putc((char)payload[i]);
        }
        owx_puts("  size=");
        owx_put_u64((uint64_t)data_len);
        owx_puts(" crc=0x");
        owrestore_put_hex32(stored);
        owx_puts(crc_ok ? " OK" : " BAD");
        owx_puts("\r\n");

        if (verbose && data_len > 0u) {
            owx_puts("owrestore:       ");
            uint32_t pv = data_len < OWRESTORE_PREVIEW ? data_len : OWRESTORE_PREVIEW;
            for (i = 0u; i < pv; i++) {
                owrestore_put_hex8(payload[name_len + i]);
                owx_putc(' ');
            }
            if (data_len > pv) {
                owx_puts("...");
            }
            owx_puts("\r\n");
        }

        if (!crc_ok) {
            owx_puts("owrestore: entry CRC failed; archive corrupt\r\n");
            return 1;
        }

        pos += total;
        seen++;
    }

    if (pos + OWRESTORE_TRAILER_SIZE > len) {
        owx_puts("owrestore: missing trailer\r\n");
        return 1;
    }
    if (!owrestore_magic(&buf[pos], "OWEN")) {
        owx_puts("owrestore: bad trailer magic\r\n");
        return 1;
    }
    if (owrestore_rd32(&buf[pos + 4u]) != seen) {
        owx_puts("owrestore: trailer entry count mismatch\r\n");
        return 1;
    }
    if (owrestore_rd32(&buf[pos + 8u]) != owrestore_crc32c(buf, pos)) {
        owx_puts("owrestore: archive CRC mismatch\r\n");
        return 1;
    }

    owx_puts("owrestore: verified ");
    owx_put_u64((uint64_t)seen);
    owx_puts(" entries, archive CRC OK\r\n");
    return 0;
}

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owrestore_usage();
        return 1;
    }
    return owrestore_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static bool owrestore_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static int owrestore_run(int argc, const char *const *argv)
{
    const char *path = NULL;
    bool verbose = false;
    int i;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (a == NULL) {
            continue;
        }
        if (owrestore_streq(a, "-h") || owrestore_streq(a, "--help")) {
            owrestore_usage();
            return 0;
        } else if (owrestore_streq(a, "-v")) {
            verbose = true;
        } else if (owrestore_streq(a, "-l")) {
            /* listing is the default action; accepted for symmetry */
        } else if (a[0] == '-') {
            owx_puts("owrestore: unknown option: ");
            owx_puts(a);
            owx_puts("\r\n");
            owrestore_usage();
            return 1;
        } else if (path == NULL) {
            path = a;
        } else {
            owx_puts("owrestore: too many arguments\r\n");
            owrestore_usage();
            return 1;
        }
    }

    if (path == NULL) {
        owrestore_usage();
        return 1;
    }

    owx_puts("owrestore: reading ");
    owx_puts(path);
    owx_puts("\r\n");
    int64_t n = owx_file_read(path, s_archive.bytes, OWRESTORE_MAX_ARCHIVE);
    if (n < 0) {
        owx_puts("owrestore: read failed, code=");
        owx_put_i64(n);
        owx_puts("\r\n");
        return 1;
    }
    owx_puts("owrestore: no write syscall; verifying and listing only\r\n");
    return owrestore_list(s_archive.bytes, (uint32_t)n, verbose);
}

static void owrestore_usage(void)
{
    owx_puts("owrestore " OWRESTORE_VERSION " - verify/list an OWBK archive\r\n");
    owx_puts("usage: owrestore [-l] [-v] <archive>\r\n");
    owx_puts("  -l   list entries (default)\r\n");
    owx_puts("  -v   verbose: hex preview of each entry\r\n");
}
