/*
 * owfwupd.c - OpenWindows Firmware Update Utility (.owx)
 *
 * Reads a firmware image from the primary OWFS volume through the Ring 3
 * gateway (owx_file_read), parses its 16-byte header, validates the payload
 * against the header's Fletcher-32 checksum, and prints the update plan.
 *
 * Image layout (little-endian):
 *
 *     offset 0   magic[4]    'O','W','F','W'
 *     offset 4   version     u32, (major<<16)|(minor<<8)|patch
 *     offset 8   size        u32 payload length in bytes
 *     offset 12  checksum    u32 Fletcher-32 over the payload
 *     offset 16  payload     `size` bytes
 *
 * Nothing is written back to the device: the gateway exposes no flash call, so
 * the tool is a validator and a planner, and it says so in the report.  With no
 * path argument it validates a built-in fixture image so the parser and the
 * checksum path can be exercised without a file.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWFWUPD_VERSION "1.0.0"
#define OWFWUPD_HEADER 16u
#define OWFWUPD_MAX_IMAGE 4096u
#define OWFWUPD_DEFAULT_IMAGE "firmware.owfw"

static uint8_t s_image[OWFWUPD_MAX_IMAGE];

/* ── Little-endian and checksum helpers ───────────────────────── */

static uint32_t owfwupd_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void owfwupd_put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

/* Fletcher-32: two running 16-bit sums modulo 65535, packed big-half first. */
static uint32_t owfwupd_fletcher32(const uint8_t *data, uint32_t len)
{
    uint32_t sum1 = 0xFFFFu;
    uint32_t sum2 = 0xFFFFu;
    for (uint32_t i = 0u; i < len; i++) {
        sum1 = (sum1 + (uint32_t)data[i]) % 65535u;
        sum2 = (sum2 + sum1) % 65535u;
    }
    return (sum2 << 16) | sum1;
}

static void owfwupd_hex32(uint32_t v)
{
    static const char digits[] = "0123456789abcdef";
    owx_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        owx_putc(digits[(v >> shift) & 0x0Fu]);
    }
}

/* Build a self-consistent fixture image so the no-file path still validates a
 * real payload with a real checksum. */
static uint32_t owfwupd_build_fixture(void)
{
    static const char payload[] = "OPENWINDOWS-FIRMWARE-FIXTURE";
    uint32_t size = (uint32_t)(sizeof(payload) - 1u);
    if (OWFWUPD_HEADER + size > OWFWUPD_MAX_IMAGE) {
        return 0u;
    }
    s_image[0] = 'O';
    s_image[1] = 'W';
    s_image[2] = 'F';
    s_image[3] = 'W';
    owfwupd_put_le32(&s_image[4], 0x00010000u); /* version 1.0.0 */
    owfwupd_put_le32(&s_image[8], size);
    for (uint32_t i = 0u; i < size; i++) {
        s_image[OWFWUPD_HEADER + i] = (uint8_t)payload[i];
    }
    owfwupd_put_le32(&s_image[12], owfwupd_fletcher32(&s_image[OWFWUPD_HEADER], size));
    return OWFWUPD_HEADER + size;
}

/* ── Entry point ──────────────────────────────────────────────── */

static int  owfwupd_run(int argc, const char *const *argv);
static void owfwupd_usage(void);

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owfwupd_usage();
        return 1;
    }
    return owfwupd_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static int owfwupd_run(int argc, const char *const *argv)
{
    const char *path = NULL;
    bool check_only = false;
    uint32_t total = 0u;
    uint32_t version;
    uint32_t size;
    uint32_t stored;
    uint32_t computed;
    int i;

    for (i = 1; i < argc && argv != NULL && argv[i] != NULL; i++) {
        const char *a = argv[i];
        if (a[0] == '-' && a[1] == 'c' && a[2] == '\0') {
            check_only = true;
        } else if (a[0] == '-' && a[1] == 'h' && a[2] == '\0') {
            owfwupd_usage();
            return 0;
        } else if (a[0] == '-') {
            owx_puts("owfwupd: unknown option: ");
            owx_puts(a);
            owx_puts("\r\n");
            owfwupd_usage();
            return 1;
        } else if (path == NULL) {
            path = a;
        } else {
            owx_puts("owfwupd: unexpected argument: ");
            owx_puts(a);
            owx_puts("\r\n");
            owfwupd_usage();
            return 1;
        }
    }

    if (path == NULL) {
        total = owfwupd_build_fixture();
        if (total == 0u) {
            owx_puts("owfwupd: fixture does not fit the image buffer\r\n");
            return 1;
        }
        owx_puts("owfwupd: using built-in fixture image (no path given)\r\n");
    } else {
        int64_t n = owx_file_read(path, s_image, (uint32_t)sizeof(s_image));
        if (n < 0) {
            owx_puts("owfwupd: cannot read ");
            owx_puts(path);
            owx_puts(": vfs error ");
            owx_put_i64(n);
            owx_puts("\r\n");
            return 1;
        }
        total = (uint32_t)n;
        owx_puts("owfwupd: read ");
        owx_put_u64((uint64_t)total);
        owx_puts(" byte(s) from ");
        owx_puts(path);
        owx_puts("\r\n");
    }

    if (total < OWFWUPD_HEADER) {
        owx_puts("owfwupd: image too small for a header\r\n");
        return 2;
    }
    if (s_image[0] != 'O' || s_image[1] != 'W' || s_image[2] != 'F' ||
        s_image[3] != 'W') {
        owx_puts("owfwupd: bad magic; not an .owfw image\r\n");
        return 2;
    }

    version = owfwupd_le32(&s_image[4]);
    size = owfwupd_le32(&s_image[8]);
    stored = owfwupd_le32(&s_image[12]);

    if (size != total - OWFWUPD_HEADER) {
        owx_puts("owfwupd: size mismatch: header says ");
        owx_put_u64((uint64_t)size);
        owx_puts(", file carries ");
        owx_put_u64((uint64_t)(total - OWFWUPD_HEADER));
        owx_puts("\r\n");
        return 2;
    }

    computed = owfwupd_fletcher32(&s_image[OWFWUPD_HEADER], size);

    owx_puts("owfwupd: image OWFW version ");
    owx_put_u64((uint64_t)((version >> 16) & 0xFFu));
    owx_putc('.');
    owx_put_u64((uint64_t)((version >> 8) & 0xFFu));
    owx_putc('.');
    owx_put_u64((uint64_t)(version & 0xFFu));
    owx_puts(" size=");
    owx_put_u64((uint64_t)size);
    owx_puts("\r\n");

    owx_puts("owfwupd: checksum stored=");
    owfwupd_hex32(stored);
    owx_puts(" computed=");
    owfwupd_hex32(computed);
    owx_puts("\r\n");
    if (stored != computed) {
        owx_puts("owfwupd: checksum mismatch; image rejected\r\n");
        return 2;
    }
    owx_puts("owfwupd: checksum ok\r\n");

    if (check_only) {
        owx_puts("owfwupd: check complete; no update performed\r\n");
        return 0;
    }

    owx_puts("owfwupd: update plan:\r\n");
    owx_puts("owfwupd:   1. stage image to the boot volume\r\n");
    owx_puts("owfwupd:   2. verify signature and Fletcher-32 checksum\r\n");
    owx_puts("owfwupd:   3. commit payload to flash region 0\r\n");
    owx_puts("owfwupd:   4. schedule a reboot to activate\r\n");
    owx_puts("owfwupd: [dry-run] no device was modified\r\n");
    return 0;
}

static void owfwupd_usage(void)
{
    owx_puts("owfwupd " OWFWUPD_VERSION " - OpenWindows firmware update\r\n");
    owx_puts("usage: owfwupd [-c] [image]\r\n");
    owx_puts("  image defaults to " OWFWUPD_DEFAULT_IMAGE
             "; with no argument a built-in fixture is used\r\n");
    owx_puts("  -c  validate only, do not print the update plan\r\n");
}
