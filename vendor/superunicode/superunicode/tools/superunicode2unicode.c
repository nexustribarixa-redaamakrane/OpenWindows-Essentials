/**
 * @file superunicode2unicode.c
 * @brief SuperUnicode (SUCS) to Unicode Conversion CLI Tool
 *
 * Converts SuperUnicode (SUTF-8 byte stream or SUCS codepoints) into
 * standard Unicode (UTF-8).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "superunicode/superunicode.h"

int main(int argc, char **argv) {
    bool strict = false;
    int arg_offset = 1;

    if (argc > 1 && strcmp(argv[1], "--strict") == 0) {
        strict = true;
        arg_offset = 2;
    }

    if (argc - arg_offset < 2) {
        fprintf(stderr, "SuperUnicode (SUCS) to Unicode Conversion Tool\n");
        fprintf(stderr, "Usage: %s [--strict] <input_sucs_file> <output_unicode_file>\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  --strict    Fail on native SUCS codepoints (>U+10FFFF) instead of emitting U+FFFD\n");
        return 1;
    }

    const char *in_path = argv[arg_offset];
    const char *out_path = argv[arg_offset + 1];

    FILE *f_in = fopen(in_path, "rb");
    if (!f_in) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", in_path);
        return 1;
    }

    fseek(f_in, 0, SEEK_END);
    long in_sz = ftell(f_in);
    fseek(f_in, 0, SEEK_SET);

    if (in_sz <= 0) {
        FILE *f_out = fopen(out_path, "wb");
        if (f_out) fclose(f_out);
        fclose(f_in);
        printf("[SUCCESS] Converted 0 bytes SuperUnicode -> Unicode\n");
        return 0;
    }

    uint8_t *in_buf = (uint8_t *)malloc((size_t)in_sz);
    if (!in_buf || fread(in_buf, 1, (size_t)in_sz, f_in) != (size_t)in_sz) {
        fprintf(stderr, "Error: Failed to read input file\n");
        if (in_buf) free(in_buf);
        fclose(f_in);
        return 1;
    }
    fclose(f_in);

    size_t out_cap = (size_t)in_sz * 2 + 128;
    uint8_t *out_buf = (uint8_t *)malloc(out_cap);
    if (!out_buf) {
        fprintf(stderr, "Error: Allocation failed\n");
        free(in_buf);
        return 1;
    }

    size_t written = 0;
    sucs_conv_status_t st = sucs_conv_sutf8_to_utf8(in_buf, (size_t)in_sz, out_buf, out_cap, &written, strict);
    free(in_buf);

    if (st != SUCS_CONV_OK) {
        if (st == SUCS_CONV_ERR_OUT_OF_RANGE) {
            fprintf(stderr, "Error: SuperUnicode file contains native SUCS codepoints outside Unicode space (>U+10FFFF)\n");
        } else {
            fprintf(stderr, "Error: SuperUnicode to Unicode conversion failed with code %d\n", (int)st);
        }
        free(out_buf);
        return 1;
    }

    FILE *f_out = fopen(out_path, "wb");
    if (!f_out) {
        fprintf(stderr, "Error: Cannot open output file '%s'\n", out_path);
        free(out_buf);
        return 1;
    }

    size_t wrote = fwrite(out_buf, 1, written, f_out);
    fclose(f_out);
    free(out_buf);

    if (wrote != written) {
        fprintf(stderr, "Error: Incomplete write to '%s'\n", out_path);
        return 1;
    }

    printf("[SUCCESS] Converted SuperUnicode '%s' (%ld bytes) -> Unicode '%s' (%zu bytes UTF-8)\n",
           in_path, in_sz, out_path, written);
    return 0;
}
