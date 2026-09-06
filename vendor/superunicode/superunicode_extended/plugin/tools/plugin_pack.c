/**
 * plugin_pack — SuperUnicode Plugin Blob Packer (host tool)
 *
 * Usage:
 *   plugin_pack <id> <major> <minor> <patch> <ranges.txt> <payload.bin> <out.sucsplugin>
 *
 * ranges.txt:
 *   One "<start> <end>" codepoint range per line, in hexadecimal
 *   (0x-prefixed or bare). Lines starting with '#' are comments.
 *   Every range MUST start past the base limit 0x7FFFFFFF.
 *
 * payload.bin:
 *   Raw plugin data bytes (names/props/mappings tables, etc.).
 *
 * Produces a CRC32c + Fletcher-64 checksummed .sucsplugin blob ready for
 * staging (sucs_plugin_stage_install) and boot-time verification.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "superunicode_extended/plugin.h"
#include "superunicode_extended/plugin_checksum.h"

#define MAX_RANGES SUCS_PLUGIN_MAX_RANGES

static int parse_ranges_file(const char* path, sucs_plugin_range_t* out, uint32_t* out_count) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "error: cannot open ranges file: %s\n", path);
        return -1;
    }
    char line[256];
    uint32_t count = 0;
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        char* start_str = p;
        char* end_str = strchr(p, ' ');
        if (!end_str) end_str = strchr(p, '\t');
        if (!end_str) {
            fprintf(stderr, "error: malformed range line: %s", line);
            fclose(f);
            return -1;
        }
        *end_str = '\0';
        end_str++;
        while (*end_str == ' ' || *end_str == '\t') end_str++;
        char* tail = 0;
        unsigned long long s = strtoull(start_str, &tail, 16);
        unsigned long long e = strtoull(end_str, &tail, 16);
        if (count >= MAX_RANGES) {
            fprintf(stderr, "error: too many ranges (max %u)\n", (unsigned)MAX_RANGES);
            fclose(f);
            return -1;
        }
        out[count].start = (sucs_ex_char_t)s;
        out[count].end = (sucs_ex_char_t)e;
        count++;
    }
    fclose(f);
    if (count == 0) {
        fprintf(stderr, "error: no ranges parsed from %s\n", path);
        return -1;
    }
    *out_count = count;
    return 0;
}

static int read_file(const char* path, uint8_t** out_data, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open file: %s\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) {
        fclose(f);
        return -1;
    }
    uint8_t* data = (uint8_t*)malloc((size_t)len ? (size_t)len : 1);
    if (!data) {
        fclose(f);
        return -1;
    }
    size_t rd = fread(data, 1, (size_t)len, f);
    fclose(f);
    *out_data = data;
    *out_size = rd;
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 8) {
        fprintf(stderr,
                "usage: plugin_pack <id> <major> <minor> <patch> "
                "<ranges.txt> <payload.bin> <out.sucsplugin>\n");
        return 2;
    }

    const char* id = argv[1];
    unsigned major = (unsigned)strtoul(argv[2], 0, 10);
    unsigned minor = (unsigned)strtoul(argv[3], 0, 10);
    unsigned patch = (unsigned)strtoul(argv[4], 0, 10);

    sucs_plugin_range_t ranges[MAX_RANGES];
    uint32_t range_count = 0;
    if (parse_ranges_file(argv[5], ranges, &range_count) != 0) {
        return 1;
    }
    if (sucs_plugin_validate_ranges(ranges, range_count) != SUCS_PLUGIN_OK) {
        fprintf(stderr, "error: a range is malformed or does not extend past 0x7FFFFFFF\n");
        return 1;
    }

    uint8_t* payload = 0;
    size_t payload_size = 0;
    if (read_file(argv[6], &payload, &payload_size) != 0) {
        return 1;
    }

    size_t hdr_size = sizeof(sucs_plugin_blob_header_t);
    size_t ranges_bytes = (size_t)range_count * sizeof(sucs_plugin_range_t);
    size_t total = hdr_size + ranges_bytes + payload_size;

    uint8_t* blob = (uint8_t*)calloc(1, total);
    if (!blob) {
        fprintf(stderr, "error: out of memory\n");
        free(payload);
        return 1;
    }

    sucs_plugin_blob_header_t* hdr = (sucs_plugin_blob_header_t*)blob;
    hdr->magic = SUCS_PLUGIN_BLOB_MAGIC;
    hdr->blob_version = SUCS_PLUGIN_BLOB_VERSION;
    hdr->ver_major = (uint8_t)major;
    hdr->ver_minor = (uint8_t)minor;
    hdr->ver_patch = (uint8_t)patch;
    strncpy(hdr->id, id, SUCS_PLUGIN_ID_MAX - 1);
    hdr->range_count = range_count;
    hdr->blob_size = (uint32_t)total;

    uint8_t* p = blob + hdr_size;
    for (uint32_t r = 0; r < range_count; ++r) {
        for (int b = 0; b < 8; ++b) {
            p[r * 16 + b]     = (uint8_t)(ranges[r].start >> (8 * b));
            p[r * 16 + 8 + b] = (uint8_t)(ranges[r].end >> (8 * b));
        }
    }
    memcpy(blob + hdr_size + ranges_bytes, payload, payload_size);

    uint32_t crc = 0;
    uint64_t fletcher = 0;
    sucs_plugin_status_t st = sucs_plugin_compute_checksums(blob, total, &crc, &fletcher);
    if (st != SUCS_PLUGIN_OK) {
        fprintf(stderr, "error: checksum computation failed (%d)\n", (int)st);
        free(payload);
        free(blob);
        return 1;
    }
    hdr->crc32c = crc;
    hdr->fletcher64 = fletcher;

    FILE* out = fopen(argv[7], "wb");
    if (!out) {
        fprintf(stderr, "error: cannot write output: %s\n", argv[7]);
        free(payload);
        free(blob);
        return 1;
    }
    fwrite(blob, 1, total, out);
    fclose(out);

    printf("packed %s v%u.%u.%u: %u ranges, %zu bytes\n",
           id, major, minor, patch, (unsigned)range_count, total);
    printf("  crc32c   = 0x%08X\n", (unsigned)crc);
    printf("  fletcher = 0x%016llX\n", (unsigned long long)fletcher);

    free(payload);
    free(blob);
    return 0;
}
