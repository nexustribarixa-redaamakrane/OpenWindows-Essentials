/**
 * @file utf2sutf.c
 * @brief UTF-8 to SUTF-8 Stream Conversion CLI Tool
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "superunicode/superunicode.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "SuperUnicode Encoding Converter: UTF-8 -> SUTF-8\n");
        fprintf(stderr, "Usage: %s <input.utf8> <output.sutf8>\n", argv[0]);
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

    FILE *f_in = fopen(in_path, "rb");
    if (!f_in) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", in_path);
        return 1;
    }

    fseek(f_in, 0, SEEK_END);
    long in_sz = ftell(f_in);
    fseek(f_in, 0, SEEK_SET);

    if (in_sz < 0) {
        fprintf(stderr, "Error: Failed to determine input file size\n");
        fclose(f_in);
        return 1;
    }

    if (in_sz == 0) {
        FILE *f_out = fopen(out_path, "wb");
        if (f_out) fclose(f_out);
        fclose(f_in);
        printf("[SUCCESS] Converted 0 bytes UTF-8 -> SUTF-8\n");
        return 0;
    }

    uint8_t *in_buf = (uint8_t *)malloc((size_t)in_sz);
    if (!in_buf || fread(in_buf, 1, (size_t)in_sz, f_in) != (size_t)in_sz) {
        fprintf(stderr, "Error: Failed to read input file '%s'\n", in_path);
        if (in_buf) free(in_buf);
        fclose(f_in);
        return 1;
    }
    fclose(f_in);

    /* SUTF-8 max byte expansion is at most 1.5x of UTF-8 */
    size_t out_cap = (size_t)in_sz * 2 + 64;
    uint8_t *out_buf = (uint8_t *)malloc(out_cap);
    if (!out_buf) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(in_buf);
        return 1;
    }

    size_t written = 0;
    sucs_conv_status_t st = sucs_conv_utf8_to_sutf8(in_buf, (size_t)in_sz, out_buf, out_cap, &written);
    free(in_buf);

    if (st != SUCS_CONV_OK) {
        fprintf(stderr, "Error: UTF-8 to SUTF-8 conversion failed with code %d\n", (int)st);
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

    printf("[SUCCESS] Converted UTF-8 '%s' (%ld bytes) -> SUTF-8 '%s' (%zu bytes)\n",
           in_path, in_sz, out_path, written);
    return 0;
}
