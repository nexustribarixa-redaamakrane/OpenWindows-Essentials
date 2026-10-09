#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "superunicode/superunicode.h"

static const char* get_bancode_type_name(sucs_bancode_type_t type) {
    switch (type) {
        case SUCS_BANCODE_FATAL:
            return "B+ (Fatal Kernel Error)";
        case SUCS_BANCODE_WARN:
            return "W+ (Kernel Warning)";
        case SUCS_BANCODE_COM:
            return "C+ (Success / Communications)";
        case SUCS_BANCODE_SOFT:
            return "S+ (Soft Error)";
        default:
            return "None (Not in BANcode Registry)";
    }
}

static const char* get_type_name(sucs_codepoint_type_t type) {
    switch (type) {
        case SUCS_TYPE_UNICODE_COMPAT:
            return "Unicode Compatibility Zone (1:1 Standard Unicode Parity)";
        case SUCS_TYPE_SYS_FUNCTION:
            return "System Control Plane / SCP (OS System Function & Formatting)";
        case SUCS_TYPE_NATIVE_ALLOC:
            return "Native Extended SUCS Allocation Space";
        default:
            return "Unknown Type";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("SUES SuperUnicode Codepoint Inspector\n");
        printf("Usage: %s <hex_codepoint>\n", argv[0]);
        printf("Example: %s 0x123456\n", argv[0]);
        return 1;
    }

    const char* hex_str = argv[1];
    sucs_char_t cp = (sucs_char_t)strtoul(hex_str, NULL, 0);

    if (cp > SUCS_MAX_CODEPOINT) {
        printf("Error: Codepoint 0x%X exceeds max valid 31-bit limit (0x7FFFFFFF)\n", cp);
        return 1;
    }

    printf("=================================================================\n");
    printf(" SUCS CODEPOINT INSPECTOR: 0x%08X (%u)\n", cp, cp);
    printf("=================================================================\n");

    /* Coordinates (128 Zones x 256 Districts x 256 Planes x 256 Block Offsets) */
    printf(" Zone ID     : %u (0x%02X) [Bits 24..30]\n", SUCS_GET_ZONE(cp), SUCS_GET_ZONE(cp));
    printf(" District ID : %u (0x%02X) [Bits 16..23]\n", SUCS_GET_DISTRICT(cp), SUCS_GET_DISTRICT(cp));
    printf(" Plane ID    : %u (0x%02X) [Bits 8..15]\n", SUCS_GET_PLANE(cp), SUCS_GET_PLANE(cp));
    printf(" Block Offset: %u (0x%02X)   [Bits 0..7]\n", SUCS_GET_OFFSET(cp), SUCS_GET_OFFSET(cp));
    printf(" Plane Type  : %s\n", sucs_is_fixed_plane(cp) ? "Fixed-Width (Plane 0/1)" : "Variable-Width (Plane 2+)");

    /* Classification */
    sucs_codepoint_type_t type = sucs_classify_codepoint(cp);
    printf("\n Classification: %s\n", get_type_name(type));
    printf(" Unicode Compat: %s\n", sucs_is_unicode_compatible(cp) ? "YES" : "NO (Native Extended / Incompatible)");
    printf(" System Control: %s\n", sucs_is_scp_plane(cp) ? "YES (System Control Plane / Formatting)" : "NO");

    /* BANcode Registry & Kernel Trap Damage Control */
    if (sucs_is_bancode_registry(cp) || sucs_is_kernel_trap(cp)) {
        printf("\n BANcode Registry: %s\n", get_bancode_type_name(sucs_classify_bancode(cp)));

        if (sucs_is_bancode(cp)) {
            sucs_char_t trap = sucs_bancode_to_trap(cp);
            if (trap != SUCS_INVALID_CODEPOINT) {
                printf(" Damage Control : Kernel Security Trap 0x%08X (Slot %u of %u)\n",
                       trap, (unsigned)(trap - SUCS_KERNEL_TRAP_MIN), SUCS_TRAP_SLOT_COUNT);
                sucs_char_t lo = 0, hi = 0;
                if (sucs_trap_to_bancode_range(trap, &lo, &hi)) {
                    printf(" BANcode Cluster: 0x%08X - 0x%08X (%u BANcodes)\n",
                           lo, hi, SUCS_BANCODES_PER_TRAP);
                }
            } else {
                printf(" Damage Control : UNMAPPED (no Kernel Security Trap slot governs this BANcode)\n");
            }
        }

        if (sucs_is_kernel_trap(cp)) {
            sucs_char_t lo = 0, hi = 0;
            if (sucs_trap_to_bancode_range(cp, &lo, &hi)) {
                printf(" Trap Slot     : %u of %u\n",
                       (unsigned)(cp - SUCS_KERNEL_TRAP_MIN), SUCS_TRAP_SLOT_COUNT);
                printf(" Managed Cluster: 0x%08X - 0x%08X (%u BANcodes)\n",
                       lo, hi, SUCS_BANCODES_PER_TRAP);
            }
        }
    }

    /* SUTF Encoding */
    char buf[8];
    size_t written = 0;
    int status = sutf_encode_char(cp, buf, sizeof(buf), &written);

    if (status == SUES_SUCCESS) {
        printf("\n SUTF Length   : %zu Byte(s)\n", written);
        printf(" SUTF Sequence : ");
        for (size_t i = 0; i < written; i++) {
            printf("0x%02X ", (unsigned char)buf[i]);
        }
        printf("\n");
    } else {
        printf("\n SUTF Encoding Error: %d\n", status);
    }
    printf("=================================================================\n");

    return 0;
}
