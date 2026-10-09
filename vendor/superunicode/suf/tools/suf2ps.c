/**
 * @file suf2ps.c
 * @brief SuperUnicode Font (.suf) to PostScript Type 1 / PFA (.ps/.pfa) CLI Exporter
 */

#include <stdio.h>
#include <stdlib.h>
#include "suf/suf_conv.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.suf> <output.pfa|output.ps>\n", argv[0]);
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

    char *out_ps = NULL;
    size_t out_ps_len = 0;
    suf_status_t st = suf_conv_suf_to_ps(in_buf, (size_t)sz, &out_ps, &out_ps_len);
    free(in_buf);

    if (st != SUF_OK || !out_ps) {
        fprintf(stderr, "Error: SUF to PostScript export failed with code %d\n", st);
        return 1;
    }

    FILE *out_f = fopen(out_path, "wb");
    if (!out_f) {
        fprintf(stderr, "Error: Failed to create output file '%s'\n", out_path);
        free(out_ps);
        return 1;
    }

    size_t written = fwrite(out_ps, 1, out_ps_len, out_f);
    fclose(out_f);
    free(out_ps);

    if (written != out_ps_len) {
        fprintf(stderr, "Error: Incomplete write to output file\n");
        return 1;
    }

    printf("[SUCCESS] Exported SUF '%s' -> PostScript Type 1 '%s' (%zu bytes)\n", in_path, out_path, out_ps_len);
    return 0;
}
