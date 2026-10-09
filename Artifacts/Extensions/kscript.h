/*
 * kscript.h - OpenWindows Plaintext Kernel Script Parser (.kscript, .trapdef, .kmap)
 *
 * Provides a lightweight, zero-allocation line parser for human-readable
 * configuration, early boot commands, trap routing, and scancode keymaps.
 *
 * C99 freestanding - caller provides memory buffers and token arrays.
 */

#ifndef KSCRIPT_H
#define KSCRIPT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KSCRIPT_MAX_TOKENS_PER_LINE 16u
#define KSCRIPT_MAX_TOKEN_LEN       64u

typedef struct {
    const char *data;
    size_t      length;
} kscript_token_t;

typedef struct {
    uint32_t        line_number;
    uint32_t        token_count;
    kscript_token_t tokens[KSCRIPT_MAX_TOKENS_PER_LINE];
} kscript_line_t;

static inline bool kscript_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/*
 * Parses a single null-terminated or bounded buffer line into tokens.
 * Comments begin with '#' or ';' and discard the rest of the line.
 * Returns true if at least one token was extracted.
 */
static inline bool kscript_parse_line(const char *line, size_t line_len, kscript_line_t *out)
{
    if (!line || !out) return false;
    out->token_count = 0u;

    size_t i = 0u;
    while (i < line_len && out->token_count < KSCRIPT_MAX_TOKENS_PER_LINE) {
        /* Skip leading whitespace */
        while (i < line_len && kscript_is_space(line[i])) i++;
        if (i >= line_len || line[i] == '\n' || line[i] == '\0') break;
        if (line[i] == '#' || line[i] == ';') break; /* Comment */

        const char *tok_start = &line[i];
        size_t tok_len = 0u;
        while (i < line_len && !kscript_is_space(line[i]) && line[i] != '\n' && line[i] != '\0') {
            tok_len++;
            i++;
        }

        if (tok_len > 0u) {
            out->tokens[out->token_count].data = tok_start;
            out->tokens[out->token_count].length = tok_len;
            out->token_count++;
        }
    }

    return (out->token_count > 0u);
}

static inline bool kscript_token_equals(const kscript_token_t *tok, const char *literal)
{
    if (!tok || !literal) return false;
    size_t i = 0u;
    while (i < tok->length && literal[i] != '\0') {
        if (tok->data[i] != literal[i]) return false;
        i++;
    }
    return (i == tok->length && literal[i] == '\0');
}

#endif /* KSCRIPT_H */
