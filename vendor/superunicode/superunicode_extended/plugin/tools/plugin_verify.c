/**
 * plugin_verify — SuperUnicode Plugin Blob Verifier (host tool)
 *
 * Usage:
 *   plugin_verify <file.sucsplugin>
 *
 * Recomputes the CRC32c + Fletcher-64 checksums (same primitives the kernel
 * uses at boot) and reports whether the blob passes the integrity gate.
 * Exit code 0 = valid, 1 = invalid.
 */

#include <stdio.h>
#include <stdlib.h>
#include "superunicode_extended/plugin.h"
#include "superunicode_extended/plugin_checksum.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: plugin_verify <file.sucsplugin>\n");
        return 2;
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open: %s\n", argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < (long)sizeof(sucs_plugin_blob_header_t)) {
        fprintf(stderr, "error: file too small to be a plugin blob\n");
        fclose(f);
        return 1;
    }

    uint8_t* blob = (uint8_t*)malloc((size_t)len);
    if (!blob) {
        fclose(f);
        return 1;
    }
    size_t size = fread(blob, 1, (size_t)len, f);
    fclose(f);

    const sucs_plugin_blob_header_t* hdr = (const sucs_plugin_blob_header_t*)blob;
    uint32_t crc = 0;
    uint64_t fletcher = 0;
    sucs_plugin_status_t st = sucs_plugin_compute_checksums(blob, size, &crc, &fletcher);

    if (st == SUCS_PLUGIN_OK) {
        printf("plugin id     : %s\n", hdr->id);
        printf("plugin version: %u.%u.%u\n",
               (unsigned)hdr->ver_major, (unsigned)hdr->ver_minor, (unsigned)hdr->ver_patch);
        printf("ranges        : %u\n", (unsigned)hdr->range_count);
        printf("blob size     : %zu bytes\n", size);
        printf("crc32c        : 0x%08X (stored 0x%08X)\n", (unsigned)crc, (unsigned)hdr->crc32c);
        printf("fletcher64    : 0x%016llX (stored 0x%016llX)\n",
               (unsigned long long)fletcher, (unsigned long long)hdr->fletcher64);
    }

    bool ok = sucs_plugin_blob_verify(blob, size);
    printf(ok ? "VERIFY: PASS (checksum gate satisfied)\n"
              : "VERIFY: FAIL (checksum gate rejected)\n");

    free(blob);
    return ok ? 0 : 1;
}
