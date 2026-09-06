/**
 * @file utf2sutf.c
 * @brief UTF-8 to vSUTF (SuperUnicode Extended) Conversion CLI Tool
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extsucs_conv.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "SuperUnicode Extended Converter: UTF-8 -> vSUTF\n");
        fprintf(stderr, "Usage: %s <input.utf8> <output.vsutf>\n", argv[0]);
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

    FILE *f_in = fopen(in_path, "rb");
    if (!f_in) {
        fprintf(stderr, "Error: Cannot open '%s'\n", in_path);
        return 1;
    }

    fseek(f_in, 0, SEEK_END);
    long in_sz = ftell(f_in);
    fseek(f_in, 0, SEEK_SET);

    if (in_sz < 0) {
        fprintf(stderr, "Error: Failed to get file size\n");
        fclose(f_in);
        return 1;
    }

    if (in_sz == 0) {
        FILE *f_out = fopen(out_path, "wb");
        if (f_out) fclose(f_out);
        fclose(f_in);
        printf("[SUCCESS] Converted 0 bytes UTF-8 -> vSUTF\n");
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
        fprintf(stderr, "Error: Memory error\n");
        return 1;
    }

    size_t written = 0;
    extsucs_conv_status_t st = extsucs_conv_utf8_to_vsutf(in_buf, (size_t)in_sz, out_buf, out_cap, &written);
    free(in_buf);

    if (st != EXTSUCS_CONV_OK) {
        fprintf(stderr, "Error: UTF-8 to vSUTF conversion failed with code %d\n", (int)st);
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

    printf("[SUCCESS] Converted UTF-8 '%s' (%ld bytes) -> vSUTF '%s' (%zu bytes)\n",
           in_path, in_sz, out_path, written);
    return 0;
}
