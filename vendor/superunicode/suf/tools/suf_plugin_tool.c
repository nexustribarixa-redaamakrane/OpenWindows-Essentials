/**
 * @file suf_plugin_tool.c
 * @brief SuperUnicode Plugin Fontmaking CLI Tool (.scsp pack/unpack)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "suf/suf_conv.h"
#include "suf/suf_parser.h"


int main(int argc, char **argv) {
    if (argc < 3) {
        printf("=================================================================\n");
        printf(" SuperUnicode Plugin Fontmaking Tool (Extended Mode Only)\n");
        printf("=================================================================\n");
        printf(" Usage:\n");
        printf("   %s pack   <input.suf> <output.scsp> <plugin_id> [vMajor] [vMinor] [vPatch]\n", argv[0]);
        printf("   %s unpack <input.scsp> <output.suf>\n", argv[0]);
        printf("   %s verify <input.scsp>\n", argv[0]);
        printf("=================================================================\n");
        return 1;
    }

    const char *action = argv[1];

    if (strcmp(action, "pack") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Error: pack requires <input.suf> <output.scsp> <plugin_id>\n");
            return 1;
        }
        const char *suf_path = argv[2];
        const char *scsp_path = argv[3];
        const char *plugin_id = argv[4];
        uint8_t vmaj = (argc > 5) ? (uint8_t)atoi(argv[5]) : 1;
        uint8_t vmin = (argc > 6) ? (uint8_t)atoi(argv[6]) : 0;
        uint8_t vpat = (argc > 7) ? (uint8_t)atoi(argv[7]) : 0;

        FILE *f = fopen(suf_path, "rb");
        if (!f) {
            fprintf(stderr, "Error: Cannot open '%s'\n", suf_path);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);

        uint8_t *suf_data = (uint8_t *)malloc((size_t)sz);
        if (!suf_data || fread(suf_data, 1, (size_t)sz, f) != (size_t)sz) {
            fprintf(stderr, "Error: Failed to read '%s'\n", suf_path);
            if (suf_data) free(suf_data);
            fclose(f);
            return 1;
        }
        fclose(f);

        uint8_t *blob = NULL;
        size_t blob_sz = 0;
        suf_status_t st = suf_conv_pack_plugin_font(suf_data, (size_t)sz, plugin_id, vmaj, vmin, vpat, &blob, &blob_sz);
        free(suf_data);

        if (st != SUF_OK || !blob) {
            fprintf(stderr, "Error: Plugin font packaging failed with code %d\n", st);
            return 1;
        }

        FILE *out_f = fopen(scsp_path, "wb");
        if (!out_f) {
            fprintf(stderr, "Error: Failed to create output file '%s'\n", scsp_path);
            free(blob);
            return 1;
        }
        fwrite(blob, 1, blob_sz, out_f);
        fclose(out_f);
        free(blob);

        printf("[SUCCESS] Packaged modded font plugin '%s' -> '%s' (%zu bytes, version %u.%u.%u)\n",
               plugin_id, scsp_path, blob_sz, vmaj, vmin, vpat);
        return 0;
    } else if (strcmp(action, "unpack") == 0) {
        const char *scsp_path = argv[2];
        const char *suf_path = (argc > 3) ? argv[3] : "unpacked_font.suf";

        FILE *f = fopen(scsp_path, "rb");
        if (!f) {
            fprintf(stderr, "Error: Cannot open '%s'\n", scsp_path);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);

        uint8_t *blob_data = (uint8_t *)malloc((size_t)sz);
        if (!blob_data || fread(blob_data, 1, (size_t)sz, f) != (size_t)sz) {
            fprintf(stderr, "Error: Failed to read '%s'\n", scsp_path);
            if (blob_data) free(blob_data);
            fclose(f);
            return 1;
        }
        fclose(f);

        uint8_t *suf_out = NULL;
        size_t suf_sz = 0;
        suf_status_t st = suf_conv_unpack_plugin_font(blob_data, (size_t)sz, &suf_out, &suf_sz);
        free(blob_data);

        if (st != SUF_OK || !suf_out) {
            fprintf(stderr, "Error: Failed to unpack plugin font from '%s'\n", scsp_path);
            return 1;
        }

        FILE *out_f = fopen(suf_path, "wb");
        if (!out_f) {
            fprintf(stderr, "Error: Failed to create output file '%s'\n", suf_path);
            free(suf_out);
            return 1;
        }
        fwrite(suf_out, 1, suf_sz, out_f);
        fclose(out_f);
        free(suf_out);

        printf("[SUCCESS] Unpacked .suf font from plugin blob -> '%s' (%zu bytes)\n", suf_path, suf_sz);
        return 0;
    } else if (strcmp(action, "verify") == 0) {
        const char *scsp_path = argv[2];
        FILE *f = fopen(scsp_path, "rb");
        if (!f) {
            fprintf(stderr, "Error: Cannot open '%s'\n", scsp_path);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);

        uint8_t *blob_data = (uint8_t *)malloc((size_t)sz);
        if (!blob_data || fread(blob_data, 1, (size_t)sz, f) != (size_t)sz) {
            fprintf(stderr, "Error: Failed to read '%s'\n", scsp_path);
            if (blob_data) free(blob_data);
            fclose(f);
            return 1;
        }
        fclose(f);

        uint8_t *suf_out = NULL;
        size_t suf_sz = 0;
        suf_status_t st = suf_conv_unpack_plugin_font(blob_data, (size_t)sz, &suf_out, &suf_sz);
        free(blob_data);

        if (st == SUF_OK && suf_out) {
            suf_header_t hdr;
            suf_validate_header(suf_out, suf_sz, &hdr);
            printf("[VALID] SuperUnicode Plugin Font verified successfully: %u glyphs, em=%u, CRC valid.\n",
                   hdr.glyph_count, hdr.units_per_em);
            free(suf_out);
            return 0;
        } else {
            fprintf(stderr, "[INVALID] Plugin font blob verification failed with code %d\n", st);
            return 1;
        }
    }

    fprintf(stderr, "Error: Unknown action '%s'\n", action);
    return 1;
}
