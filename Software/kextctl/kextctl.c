/*
 * kextctl.c - OpenWindows Kernel Extension Control CLI (.owx)
 *
 * Reads .kext images out of the primary OWFS volume and checks them against
 * the binary header contract in Extensions/kext_format.h, the same header the
 * kernel-side loader validates.  Conforms to the OpenWindows non-POSIX
 * verb-driven CLI convention:
 *
 *   kextctl verify <file>   read an image and verdict its 192-byte header
 *   kextctl info <file>     print the decoded header fields
 *   kextctl selftest        run the verifier over a synthetic good/bad pair
 *   kextctl help            show usage information
 *
 * The selftest exists because the Ring 3 gateway can read files but cannot
 * create them: it builds the header in its own BSS, walks the verifier through
 * one accepted and two rejected images, and reports the observed return codes,
 * so the verdict path is exercised end to end without a fixture on the volume.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../Extensions/kext_format.h"
#include "../owx_gate.h"

/* One page is enough for the header plus the hook and string tables of any
 * small extension this CLI is meant to inspect, and it keeps the read to a
 * single gateway call. */
#define KEXTCTL_IMAGE_CAP 4096U

/* The image is read through a uint64_t-typed member so the overlay of
 * kext_header_t (8-byte naturally aligned) is aligned without a heap or a
 * compiler-specific aligned attribute.  The union lives at file scope, so the
 * loader's zeroed BSS is its initial state. */
static union {
    uint64_t align;
    uint8_t  bytes[KEXTCTL_IMAGE_CAP];
} g_KextImage;

static kext_header_t g_KextTestHeader;

/* ---------------------------------------------------------------- */
/* Helpers                                                          */
/* ---------------------------------------------------------------- */

static bool KextctlStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static const char *KextctlDomainToString(uint8_t domain)
{
    switch (domain) {
    case KEXT_DOMAIN_GENERIC:   return "GENERIC";
    case KEXT_DOMAIN_STORAGE:   return "STORAGE";
    case KEXT_DOMAIN_NETWORK:   return "NETWORK";
    case KEXT_DOMAIN_MEMORY:    return "MEMORY";
    case KEXT_DOMAIN_SECURITY:  return "SECURITY";
    case KEXT_DOMAIN_GRAPHICS:  return "GRAPHICS";
    default:                    return "UNASSIGNED";
    }
}

static void KextctlPrintUsage(void)
{
    owx_puts("kextctl: OpenWindows kernel extension (.kext) control\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  kextctl verify <file>    verify the image header\r\n");
    owx_puts("  kextctl info <file>      decode and print the header fields\r\n");
    owx_puts("  kextctl selftest         run the verifier over a synthetic pair\r\n");
    owx_puts("  kextctl help             show this message\r\n");
}

/* Print one gateway error with the code the kernel returned, so a refusal is
 * never reported as a generic failure. */
static void KextctlPrintReadError(const char *path, int64_t code)
{
    owx_puts("kextctl: cannot read '");
    owx_puts(path);
    owx_puts("' (error code: ");
    owx_put_i64(code);
    owx_puts(")\r\n");
}

/*
 * Read a file into the staging buffer and return the header overlay, or NULL
 * after reporting why.  A short read is refused here rather than in the caller
 * so no path can validate a partially filled header.
 */
static const kext_header_t *KextctlLoadHeader(const char *path)
{
    int64_t bytes_read;

    if (!path || !*path) {
        owx_puts("kextctl: missing <file>\r\n");
        return NULL;
    }

    bytes_read = owx_file_read(path, g_KextImage.bytes,
                               (uint32_t)KEXTCTL_IMAGE_CAP);
    if (bytes_read < 0) {
        KextctlPrintReadError(path, bytes_read);
        return NULL;
    }
    if (bytes_read < (int64_t)KEXT_HEADER_SIZE) {
        owx_puts("kextctl: '");
        owx_puts(path);
        owx_puts("' is ");
        owx_put_i64(bytes_read);
        owx_puts(" bytes; a .kext header is ");
        owx_put_u64((uint64_t)KEXT_HEADER_SIZE);
        owx_puts("\r\n");
        return NULL;
    }

    return (const kext_header_t *)(const void *)g_KextImage.bytes;
}

/* ---------------------------------------------------------------- */
/* Verbs                                                            */
/* ---------------------------------------------------------------- */

static int KextctlVerify(const char *path)
{
    const kext_header_t *hdr = KextctlLoadHeader(path);

    if (!hdr) return 1;

    owx_puts("kextctl: '");
    owx_puts(path);
    owx_puts("': ");

    if (!kext_header_valid(hdr)) {
        owx_puts("REJECTED\r\n");
        owx_puts("  magic        0x");
        owx_put_u64((uint64_t)hdr->magic);
        owx_puts(" (expected 0x");
        owx_put_u64((uint64_t)KEXT_MAGIC);
        owx_puts(")\r\n");
        owx_puts("  header_size  ");
        owx_put_u64((uint64_t)hdr->header_size);
        owx_puts(" (expected ");
        owx_put_u64((uint64_t)KEXT_HEADER_SIZE);
        owx_puts(")\r\n");
        owx_puts("  hook_count   ");
        owx_put_u64((uint64_t)hdr->hook_count);
        owx_puts(" (max ");
        owx_put_u64((uint64_t)KEXT_MAX_HOOKS);
        owx_puts(")\r\n");
        return 1;
    }

    owx_puts("ACCEPTED\r\n");
    owx_puts("  domain       ");
    owx_puts(KextctlDomainToString(hdr->domain));
    owx_puts("\r\n  image_size   ");
    owx_put_u64((uint64_t)hdr->image_size);
    owx_puts(" bytes\r\n  hooks        ");
    owx_put_u64((uint64_t)hdr->hook_count);
    owx_puts("\r\n");
    return 0;
}

static int KextctlInfo(const char *path)
{
    const kext_header_t *hdr = KextctlLoadHeader(path);

    if (!hdr) return 1;

    owx_puts("kextctl: header of '");
    owx_puts(path);
    owx_puts("'\r\n");
    owx_puts("  magic              0x");
    owx_put_u64((uint64_t)hdr->magic);
    owx_puts("\r\n  format_version     ");
    owx_put_u64((uint64_t)hdr->format_version);
    owx_puts("\r\n  header_size        ");
    owx_put_u64((uint64_t)hdr->header_size);
    owx_puts("\r\n  image_size         ");
    owx_put_u64((uint64_t)hdr->image_size);
    owx_puts("\r\n  header_checksum    0x");
    owx_put_u64((uint64_t)hdr->header_checksum);
    owx_puts("\r\n  image_checksum     0x");
    owx_put_u64((uint64_t)hdr->image_checksum);
    owx_puts("\r\n  domain             ");
    owx_puts(KextctlDomainToString(hdr->domain));
    owx_puts("\r\n  target_arch        ");
    owx_put_u64((uint64_t)hdr->target_arch);
    owx_puts("\r\n  hook_count         ");
    owx_put_u64((uint64_t)hdr->hook_count);
    owx_puts("\r\n  hook_table_offset  0x");
    owx_put_u64((uint64_t)hdr->hook_table_offset);
    owx_puts("\r\n  required_kernel    ");
    owx_put_u64((uint64_t)hdr->required_kernel_ver);
    owx_puts("\r\n  sentinel_bancode   0x");
    owx_put_u64((uint64_t)hdr->sentinel_bancode);
    owx_puts("\r\n  valid              ");
    owx_puts(kext_header_valid(hdr) ? "yes" : "no");
    owx_puts("\r\n");
    return 0;
}

/*
 * Walk the verifier over one image it must accept and two it must refuse.
 * Returns 0 only when every observed code matched the expectation, so this
 * verb doubles as the program's regression check.
 */
static int KextctlSelfTest(void)
{
    int failures = 0;

    owx_puts("=== kextctl selftest ===\r\n");

    g_KextTestHeader.magic = KEXT_MAGIC;
    g_KextTestHeader.format_version = KEXT_FORMAT_VERSION;
    g_KextTestHeader.header_size = KEXT_HEADER_SIZE;
    g_KextTestHeader.image_size = KEXT_HEADER_SIZE;
    g_KextTestHeader.domain = KEXT_DOMAIN_GENERIC;
    g_KextTestHeader.hook_count = 1u;

    if (kext_header_valid(&g_KextTestHeader) == true) {
        owx_puts("[ok]   well-formed header accepted\r\n");
    } else {
        owx_puts("[FAIL] well-formed header rejected\r\n");
        failures++;
    }

    if (kext_header_valid(NULL) == false) {
        owx_puts("[ok]   NULL header rejected\r\n");
    } else {
        owx_puts("[FAIL] NULL header accepted\r\n");
        failures++;
    }

    g_KextTestHeader.magic = 0u;
    if (kext_header_valid(&g_KextTestHeader) == false) {
        owx_puts("[ok]   bad magic rejected\r\n");
    } else {
        owx_puts("[FAIL] bad magic accepted\r\n");
        failures++;
    }
    g_KextTestHeader.magic = KEXT_MAGIC;

    g_KextTestHeader.hook_count = KEXT_MAX_HOOKS + 1u;
    if (kext_header_valid(&g_KextTestHeader) == false) {
        owx_puts("[ok]   oversized hook table rejected\r\n");
    } else {
        owx_puts("[FAIL] oversized hook table accepted\r\n");
        failures++;
    }
    g_KextTestHeader.hook_count = 1u;

    owx_puts("kextctl selftest: ");
    owx_put_i64(failures);
    owx_puts(" failure(s)\r\n");
    return (failures == 0) ? 0 : 1;
}

/* ---------------------------------------------------------------- */
/* Entry point                                                      */
/* ---------------------------------------------------------------- */

int kextctl_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        KextctlPrintUsage();
        return 1;
    }

    verb = argv[1];

    if (KextctlStrEqual(verb, "help") ||
        KextctlStrEqual(verb, "-h") ||
        KextctlStrEqual(verb, "--help")) {
        KextctlPrintUsage();
        return 0;
    }
    if (KextctlStrEqual(verb, "selftest")) {
        return KextctlSelfTest();
    }
    if (KextctlStrEqual(verb, "verify")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("kextctl: verify: missing <file>\r\n");
            return 1;
        }
        return KextctlVerify(argv[2]);
    }
    if (KextctlStrEqual(verb, "info")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("kextctl: info: missing <file>\r\n");
            return 1;
        }
        return KextctlInfo(argv[2]);
    }

    owx_puts("kextctl: unknown verb '");
    owx_puts(verb);
    owx_puts("'\r\n");
    KextctlPrintUsage();
    return 2;
}
