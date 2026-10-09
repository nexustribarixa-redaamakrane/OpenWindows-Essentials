/**
 * @file ufo2suf.c
 * @brief Unified Font Object (.ufo) to SuperUnicode Font (.suf) CLI Converter
 *
 * UFO is a directory-based font source format.  This converter reads
 * the fontinfo.plist, lib.plist, and glyphs/contents.plist to build
 * a .suf binary font file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "suf/suf_conv.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.ufo> <output.suf>\n", argv[0]);
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

    /* Read fontinfo.plist from the UFO directory */
    char fontinfo_path[1024];
    snprintf(fontinfo_path, sizeof(fontinfo_path), "%s/fontinfo.plist", in_path);

    FILE *f = fopen(fontinfo_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open '%s' — is '%s' a valid UFO directory?\n", fontinfo_path, in_path);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fprintf(stderr, "Error: fontinfo.plist in '%s' is empty\n", in_path);
        fclose(f);
        return 1;
    }

    char *fontinfo_xml = (char *)malloc((size_t)sz + 1);
    if (!fontinfo_xml || fread(fontinfo_xml, 1, (size_t)sz, f) != (size_t)sz) {
        fprintf(stderr, "Error: Failed to read fontinfo.plist\n");
        if (fontinfo_xml) free(fontinfo_xml);
        fclose(f);
        return 1;
    }
    fclose(f);
    fontinfo_xml[sz] = '\0';

    suf_builder_t *builder = NULL;
    suf_status_t st = suf_conv_ufo_to_suf(fontinfo_xml, (size_t)sz, &builder);
    free(fontinfo_xml);

    if (st != SUF_OK || !builder) {
        fprintf(stderr, "Error: UFO to SUF conversion failed with code %d\n", st);
        return 1;
    }

    st = suf_builder_write_file(builder, out_path);
    suf_builder_free(builder);

    if (st != SUF_OK) {
        fprintf(stderr, "Error: Failed to write output file '%s'\n", out_path);
        return 1;
    }

    printf("[SUCCESS] Converted UFO '%s' -> SUF '%s'\n", in_path, out_path);
    return 0;
}
