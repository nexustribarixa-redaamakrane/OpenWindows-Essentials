/**
 * @file superunicode2unicode.c
 * @brief SuperUnicode Extended (ExtSUCS) to Unicode Conversion CLI Tool
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "extsucs_conv.h"

int main(int argc, char **argv) {
    bool strict = false;
    int arg_offset = 1;

    if (argc > 1 && strcmp(argv[1], "--strict") == 0) {
        strict = true;
        arg_offset = 2;
    }

    if (argc - arg_offset < 2) {
        fprintf(stderr, "SuperUnicode Extended (ExtSUCS) to Unicode Conversion Tool\n");
        fprintf(stderr, "Usage: %s [--strict] <input_extsucs_file> <output_unicode_file>\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  --strict    Fail on ExtSUCS codepoints (>U+10FFFF) instead of emitting U+FFFD\n");
        return 1;
    }

    const char *in_path = argv[arg_offset];
    const char *out_path = argv[arg_offset + 1];

    FILE *f_in = fopen(in_path, "rb");
    if (!f_in) {
        fprintf(stderr, "Error: Cannot open '%s'\n", in_path);
        return 1;
    }

    fseek(f_in, 0, SEEK_END);
    long in_sz = ftell(f_in);
    fseek(f_in, 0, SEEK_SET);

    if (in_sz <= 0) {
        FILE *f_out = fopen(out_path, "wb");
        if (f_out) fclose(f_out);
        fclose(f_in);
        printf("[SUCCESS] Converted 0 bytes ExtSUCS -> Unicode\n");
        return 0;
    }

    uint8_t *in_buf = (uint8_t *)malloc((size_t)in_sz);
    if (!in_buf || fread(in_buf, 1, (size_t)in_sz, f_in) != (size_t)in_sz) {
        fprintf(stderr, "Error: Read error\n");
        if (in_buf) free(in_buf);
        fclose(f_in);
        return 1;
    }
    fclose(f_in);

    size_t out_cap = (size_t)in_sz * 3 + 128;
    uint8_t *out_buf = (uint8_t *)malloc(out_cap);
    if (!out_buf) {
        free(in_buf);
        return 1;
    }

    size_t written = 0;
    extsucs_conv_status_t st = extsucs_conv_vsutf_to_utf8(in_buf, (size_t)in_sz, out_buf, out_cap, &written, strict);
    free(in_buf);

    if (st != EXTSUCS_CONV_OK) {
        if (st == EXTSUCS_CONV_ERR_OUT_OF_RANGE) {
            fprintf(stderr, "Error: ExtSUCS file contains codepoints outside Unicode space (>U+10FFFF)\n");
        } else {
            fprintf(stderr, "Error: ExtSUCS to Unicode conversion failed with code %d\n", (int)st);
        }
        free(out_buf);
        return 1;
    }

    FILE *f_out = fopen(out_path, "wb");
    if (!f_out) {
        fprintf(stderr, "Error: Cannot open output '%s'\n", out_path);
        free(out_buf);
        return 1;
    }

    size_t wrote = fwrite(out_buf, 1, written, f_out);
    fclose(f_out);
    free(out_buf);

    if (wrote != written) {
        fprintf(stderr, "Error: Incomplete write\n");
        return 1;
    }

    printf("[SUCCESS] Converted ExtSUCS '%s' (%ld bytes) -> Unicode '%s' (%zu bytes UTF-8)\n",
           in_path, in_sz, out_path, written);
    return 0;
}
