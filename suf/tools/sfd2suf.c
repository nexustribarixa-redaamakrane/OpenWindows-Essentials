/**
 * @file sfd2suf.c
 * @brief FontForge Spline Font Database (.sfd) to SuperUnicode Font (.suf) CLI Converter
 */

#include <stdio.h>
#include <stdlib.h>
#include "suf/suf_conv.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.sfd> <output.suf>\n", argv[0]);
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

    char *in_buf = (char *)malloc((size_t)sz + 1);
    if (!in_buf || fread(in_buf, 1, (size_t)sz, f) != (size_t)sz) {
        fprintf(stderr, "Error: Failed to read input file\n");
        if (in_buf) free(in_buf);
        fclose(f);
        return 1;
    }
    in_buf[sz] = '\0';
    fclose(f);

    suf_builder_t *builder = NULL;
    suf_status_t st = suf_conv_sfd_to_suf(in_buf, (size_t)sz, &builder);
    free(in_buf);

    if (st != SUF_OK || !builder) {
        fprintf(stderr, "Error: SFD to SUF conversion failed with code %d\n", st);
        return 1;
    }

    st = suf_builder_write_file(builder, out_path);
    suf_builder_free(builder);

    if (st != SUF_OK) {
        fprintf(stderr, "Error: Failed to write output file '%s'\n", out_path);
        return 1;
    }

    printf("[SUCCESS] Converted FontForge SFD '%s' -> SUF '%s'\n", in_path, out_path);
    return 0;
}
