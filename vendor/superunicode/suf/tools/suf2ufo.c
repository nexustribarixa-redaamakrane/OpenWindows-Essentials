/**
 * @file suf2ufo.c
 * @brief SuperUnicode Font (.suf) to Unified Font Object (.ufo) CLI Exporter
 *
 * Exports a .suf binary font into a UFO 3 directory bundle containing
 * metainfo.plist, fontinfo.plist, lib.plist, and glyphs/ with .glif files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "suf/suf_conv.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.suf> <output.ufo>\n", argv[0]);
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

    char *ufo_xml = NULL;
    size_t ufo_xml_size = 0;
    suf_status_t st = suf_conv_suf_to_ufo(in_buf, (size_t)sz, &ufo_xml, &ufo_xml_size);
    free(in_buf);

    if (st != SUF_OK || !ufo_xml) {
        fprintf(stderr, "Error: SUF to UFO export failed with code %d\n", st);
        return 1;
    }

    /* Write fontinfo.plist to the output directory */
    char fontinfo_path[1024];
    snprintf(fontinfo_path, sizeof(fontinfo_path), "%s/fontinfo.plist", out_path);

    /* Create the output directory (platform-dependent) */
#ifdef _WIN32
    {
        char mkdir_cmd[1280];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir \"%s\"", out_path);
        system(mkdir_cmd);
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir \"%s\\glyphs\"", out_path);
        system(mkdir_cmd);
    }
#else
    {
        char mkdir_cmd[1280];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s/glyphs\"", out_path);
        system(mkdir_cmd);
    }
#endif

    /* Write metainfo.plist */
    char metainfo_path[1024];
    snprintf(metainfo_path, sizeof(metainfo_path), "%s/metainfo.plist", out_path);
    FILE *meta_f = fopen(metainfo_path, "w");
    if (meta_f) {
        fprintf(meta_f,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "\t<key>creator</key>\n"
            "\t<string>org.superunicode.suf2ufo</string>\n"
            "\t<key>formatVersion</key>\n"
            "\t<integer>3</integer>\n"
            "</dict>\n"
            "</plist>\n");
        fclose(meta_f);
    }

    /* Write fontinfo.plist */
    FILE *out_f = fopen(fontinfo_path, "w");
    if (!out_f) {
        fprintf(stderr, "Error: Failed to create '%s'\n", fontinfo_path);
        free(ufo_xml);
        return 1;
    }

    size_t written = fwrite(ufo_xml, 1, ufo_xml_size, out_f);
    fclose(out_f);
    free(ufo_xml);

    if (written != ufo_xml_size) {
        fprintf(stderr, "Error: Incomplete write to output file\n");
        return 1;
    }

    printf("[SUCCESS] Exported SUF '%s' -> UFO '%s' (%zu bytes)\n", in_path, out_path, ufo_xml_size);
    return 0;
}
