#include "sutf16.h"

size_t sutf16_encode_char(sucs_char_t cp, uint16_t* out_words, size_t buf_words) {
    if (!out_words || !sucs_is_valid(cp)) {
        return 0;
    }

    if (cp <= 0x7FFFUL) {
        if (buf_words < 1) return 0;
        out_words[0] = (uint16_t)(cp & 0x7FFFUL);
        return 1;
    } else {
        if (buf_words < 2) return 0;
        out_words[0] = (uint16_t)(0x8000U | ((cp >> 16) & 0x7FFFUL));
        out_words[1] = (uint16_t)(cp & 0xFFFFUL);
        return 2;
    }

    return 0;
}

size_t sutf16_decode_char(const uint16_t* in_words, size_t buf_words, sucs_char_t* out_cp) {
    if (!out_cp) {
        return 0;
    }
    *out_cp = SUCS_INVALID_CODEPOINT;

    if (!in_words || buf_words == 0) {
        return 0;
    }

    if (in_words[0] & 0x8000U) {
        /* Bit 15 is the 2-word marker and is never a literal, so a lone
         * marker word is a truncated 2-word sequence. */
        if (buf_words < 2) return 0;
        sucs_char_t cp = (((sucs_char_t)(in_words[0] & 0x7FFFU)) << 16) |
                         (sucs_char_t)in_words[1];
        /* Overlong: values that fit in the 1-word form (<= 0x7FFF) must not
         * be carried in the 2-word form. */
        if (cp <= 0x7FFFUL) return 0;
        if (!sucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 2;
    }

    sucs_char_t cp = (sucs_char_t)in_words[0];
    if (!sucs_is_valid(cp)) return 0;
    *out_cp = cp;
    return 1;
}

/* Note: the byte serialization of this word stream is the SUST-16
 * SERIALIZATION TRANSPORT (sust16_encode_bytes[_be|_le]/sust16_decode_bytes[_be|_le]).
 * It lives in the SUST module, not here. The byte order is an explicit
 * transport attribute (BE canonical, or LE), never implicit. */