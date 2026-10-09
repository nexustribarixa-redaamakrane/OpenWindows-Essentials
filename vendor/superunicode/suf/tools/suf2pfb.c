/**
 * @file suf2pfb.c
 * @brief SuperUnicode Font (.suf) to PostScript Binary Type 1 (.pfb) CLI Exporter
 */

#include <stdio.h>
#include <stdlib.h>
#include "suf/suf_conv.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.suf> <output.pfb>\n", argv[0]);
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

    FILE *f = fopen(in_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open '%s'\n", in_path);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fprintf(stderr, "Error: Input file '%s' is empty\n", in_path);
        fclose(f);
        return 1;
    }

    uint8_t *in_buf = (uint8_t *)malloc((size_t)sz);
    if (!in_buf || fread(in_buf, 1, (size_t)sz, f) != (size_t)sz) {
        fprintf(stderr, "Error: Failed to read input file\n");
        if (in_buf) free(in_buf);
        fclose(f);
        return 1;
    }
    fclose(f);

    uint8_t *out_pfb = NULL;
    size_t out_pfb_size = 0;
    suf_status_t st = suf_conv_suf_to_pfb(in_buf, (size_t)sz, &out_pfb, &out_pfb_size);
    free(in_buf);

    if (st != SUF_OK || !out_pfb) {
        fprintf(stderr, "Error: SUF to PFB export failed with code %d\n", st);
        return 1;
    }

    FILE *out_f = fopen(out_path, "wb");
    if (!out_f) {
        fprintf(stderr, "Error: Failed to create output file '%s'\n", out_path);
        free(out_pfb);
        return 1;
    }

    size_t written = fwrite(out_pfb, 1, out_pfb_size, out_f);
    fclose(out_f);
    free(out_pfb);

    if (written != out_pfb_size) {
        fprintf(stderr, "Error: Incomplete write to output file\n");
        return 1;
    }

    printf("[SUCCESS] Exported SUF '%s' -> PostScript Binary '%s' (%zu bytes)\n", in_path, out_path, out_pfb_size);
    return 0;
}
