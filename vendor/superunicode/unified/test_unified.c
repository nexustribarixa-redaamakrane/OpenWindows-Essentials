/**
 * Unified Header Coexistence Test
 *
 * This single translation unit includes EVERY public header from all three
 * modules:
 *
 *   - Base SUCS            : superunicode/superunicode.h (+ sucs_trap.h)
 *   - SUTF transform       : sutf/sutf.h (sucs_types, sucs_mode, sutf8/16/4/2)
 *   - SUST serialization   : sust.h (sust16, sustfixed, esust)
 *   - Extended ExtSUCS     : extsucs_types.h, vsutf.h
 *   - Plugin subsystem     : plugin.h, plugin_checksum.h, plugin_stage.h,
 *                            plugin_boot.h, plugin_partition.h
 *   - SUAS architecture    : suas/suas_core.h, suas/suas_sucd.h, suas/suas_sdf.h
 *
 * Prior to the header-dedupe work, the identical constants (SUCS_INVALID_
 * CODEPOINT, SUCS_TRAP_RANGE_MIN/MAX, SUCS_MAX_CODEPOINT, ...), the
 * sucs_char_t typedef, and sucs_is_valid() were each defined in multiple
 * headers; this file would NOT compile. It now must compile cleanly and run.
 */

#include <stdio.h>
#include <assert.h>
#include "superunicode/superunicode.h" // IWYU pragma: keep
#include "superunicode/sucs_trap.h"     // IWYU pragma: keep
#include "sutf.h"                       // IWYU pragma: keep
#include "sust.h"                       // IWYU pragma: keep
#include "extsucs_types.h"              // IWYU pragma: keep
#include "vsutf.h"                      // IWYU pragma: keep
#include "superunicode_extended/plugin.h"           // IWYU pragma: keep
#include "superunicode_extended/plugin_checksum.h"  // IWYU pragma: keep
#include "superunicode_extended/plugin_stage.h"     // IWYU pragma: keep
#include "superunicode_extended/plugin_boot.h"      // IWYU pragma: keep
#include "superunicode_extended/plugin_partition.h" // IWYU pragma: keep
#include "extsucs_conv.h"               // IWYU pragma: keep
#include "suf/suf_types.h"              // IWYU pragma: keep
#include "suf/suf_parser.h"             // IWYU pragma: keep
#include "suf/suf_conv.h"               // IWYU pragma: keep
#include "suas/suas_core.h"             // IWYU pragma: keep
#include "suas/suas_sucd.h"             // IWYU pragma: keep
#include "suas/suas_sdf.h"              // IWYU pragma: keep
#include "suas/suas_sgw.h"              // IWYU pragma: keep
#include "suas/suas_sbr.h"              // IWYU pragma: keep
#include "suas/suas_sucf.h"             // IWYU pragma: keep
#include "suts/suts_suca.h"             // IWYU pragma: keep

int main(void) {

    /* --- Duplicated constants agree across modules --- */
    #ifndef SUTF_MASTER_H
    #error "SUTF_MASTER_H must be defined by sutf.h"
    #endif


    assert(SUCS_MAX_CODEPOINT == 0x7FFFFFFFUL);
    assert(SUCS_INVALID_CODEPOINT == 0x7FFFFFFFUL);
    assert(SUCS_TRAP_RANGE_MIN == SUCS_KERNEL_TRAP_MIN);
    assert(SUCS_TRAP_RANGE_MAX == SUCS_KERNEL_TRAP_MAX);
    assert(SUCS_KERNEL_TRAP_MIN == 0x7FFFFFF0UL);
    assert(SUCS_KERNEL_TRAP_MAX == 0x7FFFFFFEUL);

    /* --- The shared sucs_char_t / sucs_is_valid() coexist --- */
    assert(sucs_is_valid(0x41) == true);
    assert(sucs_is_valid(0x7FFFFFFFUL) == false);   /* sentinel */
    assert(sucs_is_valid(0x7FFFFFF5UL) == false);   /* trap range */
    assert(sucs_is_valid(0x7FFFFFEFUL) == true);

    /* --- ExtSUCS validator distinct and correct --- */
    assert(extsucs_is_valid(0x7FFFFFFFULL) == true);
    assert(extsucs_is_valid(0xFFFFFFFFFFFFFFFFULL) == true);
    assert(extsucs_is_valid(0x7FFFFFF5ULL) == false);   /* inherited trap range */

    /* --- Mode-switch enums from sutf/sucs_mode.h --- */
    assert(SUCS_MODE_BASE == 0);
    assert(SUCS_MODE_EXTENDED == 1);

    /* --- Classifier + trap math work with the shared constants --- */
    assert(sucs_classify_codepoint(0x41) == SUCS_TYPE_UNICODE_COMPAT);
    assert(sucs_classify_codepoint(0x7FFFFFF5UL) == SUCS_TYPE_INVALID);
    assert(sucs_bancode_to_trap(0x0011A080) == 0x7FFFFFF1);

    /* --- Real trap dispatch works from the unified header --- */
    assert(sucs_trap_dispatch(0x0011A005) == false); /* nothing registered */
    assert(sucs_trap_register_handler(14, NULL, NULL) == false); /* NULL handler */
    assert(sucs_trap_unregister_handler(99) == false);

    /* --- An actual SUTF-8 & SUTF-16 round trip using sutf_static --- */
    uint8_t buf[8];
    assert(sutf8_encode_char(0x110000, buf, sizeof(buf)) == 5);
    sucs_char_t cp = 0;
    assert(sutf8_decode_char(buf, 5, &cp) == 5);
    assert(cp == 0x110000);
    uint16_t wbuf[2];
    assert(sutf16_encode_char(0x41, wbuf, 2) == 1);

    /* --- SUST serialization transports coexist (headers + a quick call) --- */
    sucs_ex_char_t ex_decoded = 0;
    assert(sust64_encode(0xDEADBEEFCAFEULL, buf, sizeof(buf)) == 8);
    assert(sust64_decode(buf, 8, &ex_decoded) == 8);
    assert(ex_decoded == 0xDEADBEEFCAFEULL);
    assert(vsutf_encode(0x41ULL, buf, sizeof(buf)) == 1);
    assert(vsutf_decode(buf, 1, &ex_decoded) == 1);
    assert(ex_decoded == 0x41ULL);

    /* e-SUST page constants and structures */
    assert(ESUST_PAGE_SIZE == 4096ULL);
    assert(ESUST_IPC_FRAME_BYTES == 6);

    /* --- Plugin subsystem headers coexist with the encodings --- */
    assert(SUCS_PLUGIN_BLOB_MAGIC == 0x53435343UL);
    assert(SUCS_PLUGIN_BASE_LIMIT == 0x7FFFFFFFULL);
    assert(SUCS_PLUGIN_REBOOT_REQUIRED == 13);
    assert(SUCS_PARTITION_FS_OWFS == 1);
    assert(sucs_plugin_partition_fs_is_valid(SUCS_PARTITION_FS_USFS) == true);
    assert(sucs_plugin_get_pending_count() <= SUCS_PLUGIN_MAX_PENDING);

    sucs_fletcher64_state_t fstate;
    sucs_fletcher64_init(&fstate);

    sucs_plugin_boot_config_t boot_cfg = {0};
    assert(boot_cfg.mounted_count == 0);

    /* --- SuperUnicode Font (.suf) headers & SIMD types coexist --- */
    assert(SUF_MAGIC == 0x53554631UL);
    assert(sizeof(suf_metric_t) == 16);
    assert(sizeof(suf_header_t) == 128);
    assert(sizeof(suf_var_axis_t) == 48);
    assert(sizeof(suf_plugin_font_meta_t) == 128);
    assert(SUF_CMD_MOVE_TO == 0x01);
    assert(SUF_AXIS_WGHT == 0x77676874UL);
    assert(SUF_BANCODE_FATAL_MIN == 0x0011A000UL);
    assert(suf_validate_header(NULL, 0, NULL) == SUF_ERR_NULL_POINTER);

    /* --- SUAS architecture standard (SUAS-001 SDF) headers coexist --- */
    assert(SCP_DIR_LTR == 0x00110101UL);
    assert(SCP_DIR_RTL == 0x00110102UL);
    assert(SCP_DIR_ISOLATE_PUSH == 0x00110104UL);
    assert(SCP_DIR_ISOLATE_POP == 0x00110108UL);
    assert((suas_sucd_bidi(0x0041) & SUCD_BIDI_LTR) != 0);
    {
        suas_sdf_state_t st;
        suts32_framed_t fw;
        size_t cnt = 0;
        suas_sdf_init(&st, SUAS_DIR_LTR);
        assert(suas_sdf_process_codepoint(&st, 0x05D1, &fw, &cnt) == SUAS_SDF_OK);
        assert(cnt == 1);
        assert(SUAS_SDF_FRAMED_DIR(fw) == SUAS_DIR_RTL);
    }

    /* --- SUTS-001 SUCA header coexists; 64-bit extSUCS codepoints -- */
    {
        suts_suca_options_t so;
        suts_suca_options_default(&so);
        sucs_ex_char_t na[] = {0x00120000ULL};
        sucs_ex_char_t nb[] = {0x80000000ULL};
        int cr = 0;
        assert(suts_suca_compare(na,1,nb,1,&so,&cr) == SUTS_SUCA_OK);
        assert(cr < 0); /* native Base sorts before extSUCS plugin */
        assert(suts_suca_implicit_ce(0x0041ULL).l1 != 0);
    }

    /* --- SUAS-002 SGW header coexists; zoned 64-bit grid metric -- */
    {
        suas_sgw_options_t go;
        suas_sgw_options_default(&go);
        assert(suas_sgw_resolve(0x4E00ULL, &go) == SUAS_SGW_W_WIDE);
        assert(suas_sgw_cells(0x4E00ULL, false, &go) == SUAS_SGW_GRID_TWO);
        assert(suas_sgw_cells(0x80000000ULL, false, &go) == SUAS_SGW_GRID_ONE);
        assert(suas_sgw_cells(0x00110001ULL, false, &go) == SUAS_SGW_GRID_NONE);
        assert(suas_sgw_resolve(0x00C5ULL, &go) == SUAS_SGW_W_NEUTRAL);
    }

    /* --- SUAS-003 SBR header coexists; single-pass break engine --- */
    {
        suas_sbr_options_t bo;
        suas_sbr_options_default(&bo);
        assert(SCP_BRK_MANDATORY == 0x00110020UL);
        assert(SCP_BRK_PROHIBITED == 0x00110021UL);
        assert(SCP_BRK_OPPORTUNISTIC == 0x00110022UL);
        assert(suas_sbr_classify(0x0041ULL, &bo) == SUAS_SBR_CLS_AL);
        assert(suas_sbr_pair(0x0041ULL, 0x0062ULL, &bo) == SUAS_BRK_NO_BREAK);
        assert(suas_sbr_pair(0x0041ULL, 0x000AULL, &bo) == SUAS_BRK_MUST_BREAK);
        assert(suas_sbr_is_native_word(0x00120000ULL) == true);
    }

    /* --- SUAS-004 SUCF header coexists; canonical (de)composition --- */
    {
        suas_sucf_options_t co;
        suas_sucf_options_default(&co);
        sucs_ex_char_t cbuf[8];
        size_t ccount = 0;
        const sucs_ex_char_t dec[] = { 0x0065ULL, 0x0301ULL };
        assert(suas_sucf_ccc(0x0301ULL, &co) == 230);
        assert(suas_sucf_is_scp(0x0011A080ULL) == true);
        assert(suas_sucf_is_invariant(0x00110020ULL) == true);
        assert(suas_sucf_transform(dec, 2, SUAS_SUCF_FORM_C, &co,
                                   cbuf, 8, &ccount) == SUAS_SUCF_OK);
        assert(ccount == 1 && cbuf[0] == 0x00E9ULL); /* e + acute → é */
    }

    printf("[PASS] test_unified (all module headers coexist in one TU)\n");
    return 0;
}



