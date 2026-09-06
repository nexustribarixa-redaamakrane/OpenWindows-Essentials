/*
 * engines.c - engines.owd (Managed Language Engines bundle)
 *
 * A hosted language/component registry for modules that want to run
 * small, budgeted, deterministic programs. Every known language name is
 * a valid census key by construction (FNV-1a 32); execution happens in
 * whatever engine handler owns the key. Bundled engines in this build:
 *
 *   poco        - shared integer/string stack-machine kernel (the IR)
 *   managednet  - textual bytecode front-end over the kernel
 *   manageddl   - meta engine: key-prefixed nested dispatch
 *   managedcom  - component registry (factories, IIDs, thunks)
 *   managedsh   - shell subset with set/echo/label/goto/inc/halt
 *   managedpy   - Python-like micro subset (assignment, print, while/
 *                 if/else via indentation, int arithmetic, comparisons)
 *   managedjs   - JavaScript-like micro subset (let/var, print(),
 *                 braces, and block/line comments, while/if-else,
 *                 comparisons)
 *   managedlua  - Lua-like micro subset (NAME = expr, print(...),
 *                 while ... do ... end, if ... then ... else ... end,
 *                 -- comments)
 *
 * All bundled engines are delivered: every reserved census slot has a
 * real run handler.  dispatch on any engine key yields its result.
 *
 * Conforms to OWD1 binary format (Extensions/owd_format.h).
 * C99 freestanding - no heap, no hosted libc, no external imports.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#include "managed_engines.h"    /* Extensions ABI mirror */
#include "owd_format.h"
#include "owc_format.h"

/* OWD1 binary header metadata (documentary; see Extensions/owd_format.h). */
#define MENG_VERSION_STRING     MENG_LIB_VERSION
#define MENG_INIT_FLAGS         OWC_INIT_REQUIRES_BANC

static const uint8_t meng_ident_str[] = MENG_VERSION_STRING;

static bool     meng_initialized = false;
static uint32_t meng_generation = 0u;

/* ------------------------------------------------------------------ */
/*  Forward declarations                                               */
/* ------------------------------------------------------------------ */

static int  strcmp_wr(const char *a, const char *b);
static uint32_t strlen32(const char *s);
static uint32_t pack4cc(const uint8_t *s);
static int64_t  parse_i64(const char *s, meng_status_t *st);
static void copy_wordn(char *dst, uint32_t cap, const char *src, size_t n);
static uint32_t meng_split(const char *line, char words[][33], uint32_t maxw);

static meng_status_t engine_bytecode_run(meng_machine_t *m,
                                         const uint8_t *manifest,
                                         size_t manifest_len);
static meng_status_t engine_dl_run(meng_machine_t *m,
                                   const uint8_t *manifest,
                                   size_t manifest_len);
static meng_status_t engine_com_run(meng_machine_t *m,
                                    const uint8_t *manifest,
                                    size_t manifest_len);
static meng_status_t engine_sh_run(meng_machine_t *m,
                                   const uint8_t *manifest,
                                   size_t manifest_len);
static meng_status_t engine_py_run(meng_machine_t *m,
                                   const uint8_t *manifest,
                                   size_t manifest_len);
static meng_status_t engine_js_run(meng_machine_t *m,
                                   const uint8_t *manifest,
                                   size_t manifest_len);
static meng_status_t engine_lua_run(meng_machine_t *m,
                                    const uint8_t *manifest,
                                    size_t manifest_len);
static meng_status_t meng_engine_dispatch_m(meng_machine_t *m, uint32_t key,
                                            const uint8_t *manifest,
                                            size_t manifest_len);

/* ------------------------------------------------------------------ */
/*  Small helpers                                                      */
/* ------------------------------------------------------------------ */

static int strcmp_wr(const char *a, const char *b)
{
    size_t i = 0u;

    if (!a || !b) return (a == b) ? 0 : ((a ? 1 : -1));
    while (a[i] == b[i] && a[i] != '\0') i++;
    return (int)((unsigned char)a[i] - (unsigned char)b[i]);
}

static uint32_t strlen32(const char *s)
{
    uint32_t n = 0u;

    if (!s) return 0u;
    while (s[n] != '\0') n++;
    return n;
}

static uint32_t pack4cc(const uint8_t *s)
{
    uint32_t k = 0u;
    uint32_t i;

    for (i = 0u; i < 4u; i++) {
        uint8_t c = (s[i] != '\0') ? s[i] : (uint8_t)'?';
        k |= (uint32_t)c << (i * 8u);
    }
    return k;
}

static int64_t parse_i64(const char *s, meng_status_t *st)
{
    uint64_t u = 0u;
    bool     neg = false;
    size_t   i = 0u;

    if (!s || !st) { if (st) *st = MENG_ERR_BAD_ARG; return 0; }
    if (s[i] == '-') { neg = true; i++; }
    if (s[i] == '\0') { *st = MENG_ERR_SYNTAX; return 0; }
    for (; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9') { *st = MENG_ERR_SYNTAX; return 0; }
        u = u * 10u + (uint64_t)(s[i] - '0');
    }
    *st = MENG_OK;
    return neg ? -(int64_t)u : (int64_t)u;
}

static void copy_wordn(char *dst, uint32_t cap, const char *src, size_t n)
{
    uint32_t i = 0u;

    if (!dst || cap == 0u) return;
    for (; i < n && i + 1u < cap; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static uint32_t meng_split(const char *line, char words[][33], uint32_t maxw)
{
    uint32_t n = 0u;
    size_t   i = 0u;

    if (!line || !words || maxw == 0u) return 0u;
    while (line[i] == ' ') i++;
    while (line[i] != '\0' && n < maxw) {
        size_t s = i;
        while (line[i] != '\0' && line[i] != ' ') i++;
        copy_wordn(words[n], 33u, line + s, i - s);
        n++;
        while (line[i] == ' ') i++;
    }
    return n;
}

static void meng_itoa(int64_t v, char *buf, size_t cap)
{
    char     tmp[24];
    size_t   n = 0u;
    uint64_t u;
    size_t   d;
    bool     neg = false;

    if (cap < 2u) { if (cap > 0u) buf[0] = '\0'; return; }
    if (v < 0) {
        neg = true;
        u = (uint64_t)(-(v + 1)) + 1u;
    } else {
        u = (uint64_t)v;
    }
    if (u == 0u) tmp[n++] = '0';
    while (u != 0u && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (int)(u % 10u));
        u /= 10u;
    }
    d = 0u;
    if (neg) buf[d++] = '-';
    while (n > 0u && d + 1u < cap) buf[d++] = tmp[--n];
    buf[d] = '\0';
}

static uint32_t meng_fnv1a32(const uint8_t *s, size_t n)
{
    uint32_t h = 2166136261u;
    size_t   i;

    for (i = 0u; i < n; i++) {
        h ^= (uint32_t)s[i];
        h *= 16777619u;
    }
    return h;
}

/* ------------------------------------------------------------------ */
/*  Census                                                             */
/* ------------------------------------------------------------------ */

typedef struct meng_census_entry {
    uint32_t    key;
    const char *name;
} meng_census_entry_t;

static const meng_census_entry_t meng_census[] = {
    { MENG_KEY_MANAGEDCOM, "managedcom" },
    { MENG_KEY_MANAGEDDL,  "manageddl"  },
    { MENG_KEY_MANAGEDPY,  "managedpy"  },
    { MENG_KEY_MANAGEDJS,  "managedjs"  },
    { MENG_KEY_MANAGEDNET, "managednet" },
    { MENG_KEY_MANAGEDLUA, "managedlua" },
    { MENG_KEY_MANAGEDSH,  "managedsh"  },
    { MENG_KEY_POCO,       "poco"       }
};

uint32_t meng_census_count(void)
{
    return (uint32_t)(sizeof(meng_census) / sizeof(meng_census[0]));
}

/* Alias: well-known language/dialect names fold onto the bundled engine
 * census entries; everything else still hashes to a stable key, so every
 * known language gets a census slot by construction. */
static const struct { const char *name; uint32_t key; } meng_census_aliases[] = {
    { "managedcom", MENG_KEY_MANAGEDCOM },
    { "manageddl",  MENG_KEY_MANAGEDDL  },
    { "managedpy",  MENG_KEY_MANAGEDPY  },
    { "python",     MENG_KEY_MANAGEDPY  },
    { "py",         MENG_KEY_MANAGEDPY  },
    { "managedjs",  MENG_KEY_MANAGEDJS  },
    { "javascript", MENG_KEY_MANAGEDJS  },
    { "js",         MENG_KEY_MANAGEDJS  },
    { "managednet", MENG_KEY_MANAGEDNET },
    { "managedlua", MENG_KEY_MANAGEDLUA },
    { "lua",        MENG_KEY_MANAGEDLUA },
    { "managedsh",  MENG_KEY_MANAGEDSH  },
    { "shell",      MENG_KEY_MANAGEDSH  },
    { "sh",         MENG_KEY_MANAGEDSH  },
    { "poco",       MENG_KEY_POCO       },
    { NULL,         0u                 }
};

meng_status_t meng_census_key(const char *name, uint32_t *out)
{
    char   folded[MENG_MAX_NAME + 1u];
    size_t i;

    if (!name || !out) return MENG_ERR_BAD_ARG;
    for (i = 0u; name[i] != '\0'; i++) {
        unsigned char c = (unsigned char)name[i];
        if (i >= MENG_MAX_NAME) return MENG_ERR_BAD_ARG;
        if (c < 0x20u || c > 0x7Eu) return MENG_ERR_BAD_ARG;
        folded[i] = (char)((c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : (char)c);
    }
    if (i == 0u) return MENG_ERR_BAD_ARG;
    folded[i] = '\0';

    for (i = 0u; meng_census_aliases[i].name != NULL; i++) {
        if (strcmp_wr(folded, meng_census_aliases[i].name) == 0) {
            *out = meng_census_aliases[i].key;
            return MENG_OK;
        }
    }
    *out = meng_fnv1a32((const uint8_t *)folded, strlen32(folded));
    return MENG_OK;
}

meng_status_t meng_census_name(uint32_t key, char *buf, size_t cap)
{
    size_t i;

    if (!buf || cap == 0u) return MENG_ERR_BAD_ARG;
    for (i = 0u; i < meng_census_count(); i++) {
        size_t c;

        if (meng_census[i].key != key) continue;
        for (c = 0u; meng_census[i].name[c] != '\0'; c++) {
            if (c + 1u >= cap) return MENG_ERR_SHORT_OUT;
            buf[c] = meng_census[i].name[c];
        }
        buf[c] = '\0';
        return MENG_OK;
    }
    return MENG_ERR_NOT_FOUND;
}

const char *meng_status_text(meng_status_t st)
{
    switch (st) {
    case MENG_OK:                  return "OK";
    case MENG_BAN_SCRATCH:         return "internal arena exhausted";
    case MENG_BAN_CORE:            return "kernel invariant violated";
    case MENG_WARN_BUDGET:         return "step budget consumed";
    case MENG_WARN_TRUNC:          return "output capped";
    case MENG_ERR_UNINITIALIZED:   return "module not initialized";
    case MENG_ERR_BAD_ARG:         return "bad argument";
    case MENG_ERR_SHORT_OUT:       return "output too small";
    case MENG_ERR_DUPLICATE:       return "entry already present";
    case MENG_ERR_NOT_FOUND:       return "no such key";
    case MENG_ERR_SYNTAX:          return "program parse failed";
    case MENG_ERR_RUNTIME:         return "program runtime fault";
    case MENG_ERR_TRAP:            return "watchdog fired";
    case MENG_ERR_STACK_OVERFLOW:  return "value stack overflow";
    case MENG_ERR_DEPENDENCY:      return "no engine handler";
    case MENG_ERR_TYPE:            return "type mismatch";
    case MENG_ERR_STATE:           return "wrong machine state";
    case MENG_SOFT_HALT:           return "script halted";
    case MENG_SOFT_YIELD:          return "script yielded";
    case MENG_SOFT_EOF:            return "script ended";
    default:                       return "unknown status";
    }
}

/* ------------------------------------------------------------------ */
/*  Machine helpers                                                    */
/* ------------------------------------------------------------------ */

void meng_machine_reset(meng_machine_t *m)
{
    if (!m) return;
    m->out_len = 0u;
    m->out_total = 0u;
    m->tick = 0u;
    m->steps = 0u;
    m->flags = 0u;
    m->generation = meng_generation;
    m->_scratch_len = 0u;
    m->_num[0] = '\0';
}

meng_status_t meng_machine_out(meng_machine_t *m, const uint8_t *p,
                               size_t n)
{
    uint32_t room;
    size_t   take;

    if (!m) return MENG_ERR_BAD_ARG;
    m->out_total += (uint64_t)n;
    if (!m->out || m->out_cap == 0u || m->out_len >= m->out_cap) {
        m->flags |= MENG_MFLAG_TRUNC;
        return MENG_WARN_TRUNC;
    }
    room = m->out_cap - m->out_len;
    take = (n < (size_t)room) ? n : (size_t)room;
    if (p && take > 0u) {
        size_t k;
        for (k = 0u; k < take; k++) m->out[m->out_len + (uint32_t)k] = p[k];
    }
    m->out_len += (uint32_t)take;
    if (take < n) {
        m->flags |= MENG_MFLAG_TRUNC;
        return MENG_WARN_TRUNC;
    }
    return MENG_OK;
}

meng_status_t meng_machine_out_cstr(meng_machine_t *m, const char *s)
{
    size_t n;

    if (!m || !s) return MENG_ERR_BAD_ARG;
    for (n = 0u; s[n] != '\0'; n++) ;
    return meng_machine_out(m, (const uint8_t *)s, n);
}

meng_status_t meng_machine_out_char(meng_machine_t *m, char c)
{
    return meng_machine_out(m, (const uint8_t *)&c, 1u);
}

meng_status_t meng_machine_out_int(meng_machine_t *m, int64_t v)
{
    size_t n = 0u;

    if (!m) return MENG_ERR_BAD_ARG;
    meng_itoa(v, m->_num, (size_t)MENG_NUM_BUF_SIZE);
    while (m->_num[n] != '\0') n++;
    return meng_machine_out(m, (const uint8_t *)m->_num, n);
}

uint32_t meng_machine_step(meng_machine_t *m)
{
    if (!m) return 0u;
    m->steps++;
    if (m->step_budget != 0u && m->steps > m->step_budget) return 0u;
    return 1u;
}

void meng_machine_trim(meng_machine_t *m)
{
    if (m && m->out_cap > 0u && m->out_len > m->out_cap) {
        m->out_len = m->out_cap;
        m->flags |= MENG_MFLAG_TRUNC;
    }
}

/* ------------------------------------------------------------------ */
/*  Poco kernel - assembler                                            */
/* ------------------------------------------------------------------ */

typedef enum {
    TK_OP = 0u,
    TK_NUM,
    TK_STR,
    TK_LABDEF,
    TK_LABREF
} meng_tok_kind_t;

typedef struct meng_tok {
    meng_tok_kind_t  kind;
    uint32_t         op;
    int64_t          num;
    uint32_t         str_off;
    uint32_t         str_len;
    int64_t          label_pc;      /* -1 unless defined               */
    char             label[17];
} meng_tok_t;

#define MENG_MAX_TOKENS  256u
#define MENG_POOL_SIZE   512u

static uint8_t  meng_pool[MENG_POOL_SIZE];
static uint32_t meng_pool_len = 0u;

static const struct { const char *mn; uint32_t op; } meng_ops[] = {
    { "nop",    MENG_OP_NOP    },
    { "pushi",  MENG_OP_PUSHI  },
    { "pushs",  MENG_OP_PUSHS  },
    { "dup2",   MENG_OP_DUP2   },
    { "swap2",  MENG_OP_SWAP2  },
    { "drop2",  MENG_OP_DROP2  },
    { "iadd",   MENG_OP_IADD   },
    { "isub",   MENG_OP_ISUB   },
    { "imul",   MENG_OP_IMUL   },
    { "idiv",   MENG_OP_IDIV   },
    { "imod",   MENG_OP_IMOD   },
    { "ineg",   MENG_OP_INEG   },
    { "cmpeq",  MENG_OP_CMPEQ  },
    { "cmpne",  MENG_OP_CMPNE  },
    { "cmplt",  MENG_OP_CMPLT  },
    { "cmple",  MENG_OP_CMPLE  },
    { "jnz",    MENG_OP_JNZ    },
    { "jmp",    MENG_OP_JMP    },
    { "gload",  MENG_OP_GLOAD  },
    { "gstore", MENG_OP_GSTORE },
    { "call",   MENG_OP_CALL   },
    { "ret",    MENG_OP_RET    },
    { "concat", MENG_OP_CONCAT },
    { "print",  MENG_OP_PRINT  },
    { "halt",   MENG_OP_HALT   },
    { NULL,     MENG_OP_NOP    }
};

static uint32_t meng_op_lookup(const char *s, size_t n)
{
    size_t i;

    for (i = 0u; meng_ops[i].mn != NULL; i++) {
        size_t k = 0u;
        while (k < n && meng_ops[i].mn[k] != '\0') {
            char a = meng_ops[i].mn[k];
            char b = s[k];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
            k++;
        }
        if (k == n && meng_ops[i].mn[k] == '\0') return meng_ops[i].op;
    }
    return UINT32_MAX;
}

static bool meng_parse_dec(const uint8_t *s, size_t n, bool *neg,
                           uint64_t *v)
{
    size_t i;

    *neg = false;
    *v = 0u;
    if (n == 0u) return false;
    i = 0u;
    if (s[i] == '-') {
        *neg = true;
        i++;
        if (i >= n) return false;
    }
    for (; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        *v = *v * 10u + (uint64_t)(s[i] - '0');
    }
    return true;
}

static meng_status_t meng_kernel_tokenize(const uint8_t *src, size_t len,
                                          meng_tok_t *tk, uint32_t cap,
                                          uint32_t *count,
                                          uint32_t *word_count)
{
    uint32_t nt = 0u;
    uint32_t pc = 0u;
    size_t   i = 0u;
    bool     neg;
    uint64_t num;

    meng_pool_len = 0u;

    while (i < len) {
        meng_tok_t *t;
        bool        is_label;
        size_t      start;
        size_t      n;

        if (src[i] == '\0') break;                /* defensive NUL end  */
        if (src[i] == ' ' || src[i] == '\t' || src[i] == '\r' ||
            src[i] == '\n') {
            i++;
            continue;
        }
        if (src[i] == '#' || src[i] == ';') {
            while (i < len && src[i] != '\n') i++;
            continue;
        }

        if (nt + 1u > cap) return MENG_ERR_SHORT_OUT;

        start = i;
        while (i < len && src[i] != ' ' && src[i] != '\t' &&
               src[i] != '\r' && src[i] != '\n' && src[i] != '#' &&
               src[i] != ';') {
            i++;
        }
        n = i - start;
        if (n == 0u) continue;

        if (src[start] == '"' && n >= 2u && src[start + n - 1u] == '"') {
            uint32_t off = meng_pool_len;
            uint32_t pl = 0u;
            size_t   b = start + 1u;
            size_t   e = start + n - 1u;

            while (b < e) {
                uint8_t c = src[b];
                if (c == '\\' && b + 1u < e) {
                    uint8_t d = src[b + 1u];
                    if (d == 'n') c = '\n';
                    else if (d == 't') c = '\t';
                    else if (d == '\\') c = '\\';
                    else if (d == '"') c = '"';
                    else c = d;
                    b += 2u;
                } else {
                    b++;
                }
                if (meng_pool_len + 1u > MENG_POOL_SIZE) {
                    return MENG_BAN_SCRATCH;
                }
                meng_pool[meng_pool_len++] = c;
                pl++;
            }
            t = &tk[nt++];
            t->kind = TK_STR;
            t->op = MENG_OP_PUSHS;
            t->str_off = off;
            t->str_len = pl;
            t->label_pc = -1;
            pc += 1u;
            continue;
        }

        is_label = (src[start + n - 1u] == ':');
        if (is_label) n--;

        if (n > 16u) return MENG_ERR_SYNTAX;

        if (!is_label && meng_parse_dec(src + start, n, &neg, &num)) {
            t = &tk[nt++];
            t->kind = TK_NUM;
            t->num = neg ? -(int64_t)num : (int64_t)num;
            t->label_pc = -1;
            pc += 1u;
            continue;
        }

        {
            uint32_t k2 = meng_op_lookup((const char *)(src + start), n);
            if (!is_label && k2 != UINT32_MAX) {
                t = &tk[nt++];
                t->kind = TK_OP;
                t->op = k2;
                t->label_pc = -1;
                pc += 1u;
                continue;
            }
        }

        /* label definition or reference                            */
        if (n == 0u) return MENG_ERR_SYNTAX;
        t = &tk[nt++];
        t->kind = is_label ? TK_LABDEF : TK_LABREF;
        {
            uint32_t c;
            uint32_t L = (n < 16u) ? (uint32_t)n : 16u;
            for (c = 0u; c < L; c++) {
                char ch = (char)src[start + c];
                if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
                t->label[c] = ch;
            }
            t->label[L] = '\0';
        }
        t->label_pc = is_label ? (int64_t)pc : -1;
        pc += 1u;
    }

    *count = nt;
    *word_count = pc;
    return MENG_OK;
}

/* Find the label-definition token's pc. */
static bool meng_label_pc(const meng_tok_t *tk, uint32_t nt,
                          const char *name, int64_t *pc)
{
    uint32_t i;

    for (i = 0u; i < nt; i++) {
        if (tk[i].kind == TK_LABDEF &&
            tk[i].label[0] == name[0]) {
            size_t c;
            bool   eq = true;

            for (c = 0u; name[c] != '\0' && c < 16u; c++) {
                if (tk[i].label[c] != name[c]) { eq = false; break; }
            }
            if (eq && tk[i].label[c] == '\0') {
                *pc = tk[i].label_pc;
                return true;
            }
        }
    }
    return false;
}

meng_status_t meng_kernel_assemble(const uint8_t *src, size_t len,
                                   meng_word_t *words, uint32_t cap,
                                   uint32_t *count)
{
    meng_tok_t   tk[MENG_MAX_TOKENS];
    uint32_t     nt = 0u;
    uint32_t     twc = 0u;
    uint32_t     wc = 0u;
    meng_status_t st;
    uint32_t     i;

    if (!src || !words || !count) return MENG_ERR_BAD_ARG;
    if (cap == 0u || cap > MENG_K_WORDS) return MENG_ERR_BAD_ARG;

    st = meng_kernel_tokenize(src, len, tk, MENG_MAX_TOKENS, &nt, &twc);
    if (st != MENG_OK) return st;
    if (twc > cap) return MENG_ERR_SHORT_OUT;

    /* Prepass: re-anchor each label definition at its WORD position so
     * jumps end up on real instructions, not token offsets.             */
    {
        uint32_t w = 0u;
        for (i = 0u; i < nt; i++) {
            if (tk[i].kind == TK_OP) {
                switch (tk[i].op) {
                case MENG_OP_PUSHS:
                case MENG_OP_PUSHI:
                case MENG_OP_GLOAD:
                case MENG_OP_GSTORE:
                case MENG_OP_JNZ:
                case MENG_OP_JMP:
                case MENG_OP_CALL:
                    i++;
                    break;
                default:
                    break;
                }
                w++;
            } else if (tk[i].kind == TK_LABDEF) {
                tk[i].label_pc = (int64_t)w;
            } else if (tk[i].kind == TK_NUM || tk[i].kind == TK_STR ||
                       tk[i].kind == TK_LABREF) {
                return MENG_ERR_SYNTAX;   /* bare operand - missing op   */
            }
        }
        i = 0u;
    }

    for (i = 0u; i < nt; i++) {
        if (tk[i].kind == TK_OP) {
            switch (tk[i].op) {
            case MENG_OP_PUSHS: {
                uint32_t a = i + 1u;
                if (a >= nt) return MENG_ERR_SYNTAX;
                if (tk[a].kind == TK_STR) {
                    words[wc].op = tk[i].op;
                    words[wc].pad = 0;
                    words[wc].a0 = tk[a].str_off;
                    words[wc].a1 = tk[a].str_len;
                } else {
                    return MENG_ERR_SYNTAX;
                }
                wc++;
                i = a;
                break;
            }
            case MENG_OP_PUSHI:
            case MENG_OP_GLOAD:
            case MENG_OP_GSTORE:
            case MENG_OP_JNZ:
            case MENG_OP_JMP:
            case MENG_OP_CALL: {
                uint32_t a = i + 1u;
                if (a >= nt) return MENG_ERR_SYNTAX;
                if (tk[a].kind == TK_NUM) {
                    words[wc].op = tk[i].op;
                    words[wc].pad = 0;
                    words[wc].a0 = tk[a].num;
                    words[wc].a1 = 0;
                } else if (tk[a].kind == TK_LABREF) {
                    bool    found = false;
                    int64_t lp = 0;

                    found = meng_label_pc(tk, nt, tk[a].label, &lp);
                    if (!found) return MENG_ERR_SYNTAX;
                    words[wc].op = tk[i].op;
                    words[wc].pad = 0;
                    words[wc].a0 = lp;
                    words[wc].a1 = 0;
                } else {
                    return MENG_ERR_SYNTAX;
                }
                wc++;
                i = a;
                break;
            }
            default:
                words[wc].op = tk[i].op;
                words[wc].pad = 0;
                words[wc].a0 = 0;
                words[wc].a1 = 0;
                wc++;
                break;
            }
        } else if (tk[i].kind == TK_NUM || tk[i].kind == TK_STR ||
                   tk[i].kind == TK_LABREF) {
            return MENG_ERR_SYNTAX;   /* bare operand - missing op     */
        }
        /* TK_LABDEF emits nothing */
    }

    *count = wc;
    return MENG_OK;
}

/* ------------------------------------------------------------------ */
/*  Poco kernel - executor                                             */
/* ------------------------------------------------------------------ */

/* Value cells: every pushed value is exactly two cells.               */
/*   int  : [ payload ][ type = MENG_K_T_INT ]                         */
/*   str  : [ ptr     ][ (len << 2) | MENG_K_T_STR ]                   */

#define MENG_K_T_STR   1u
#define MENG_K_T_INT   2u
#define MENG_K_T_RA    3u        /* return-address marker             */

static uint32_t meng_cell_type(uint64_t meta)
{
    return (uint32_t)(meta & 3u);
}

static uint32_t meng_cell_strlen(uint64_t meta)
{
    return (uint32_t)(meta >> 2);
}

meng_status_t meng_kernel_run(meng_machine_t *m, const meng_word_t *words,
                              uint32_t nw, int64_t *g, uint32_t ng)
{
    uint64_t cells[MENG_K_STACK_CELLS];
    uint32_t depth = 0u;
    uint32_t rstk[MENG_K_STACK_CELLS / 2u];
    uint32_t rdepth = 0u;
    uint32_t pc = 0u;

    if (!m || !words || nw == 0u) return MENG_ERR_BAD_ARG;
    if (ng > MENG_K_GLOBALS) return MENG_ERR_BAD_ARG;

    for (;;) {
        const meng_word_t *w;
        uint64_t meta;
        uint32_t tag;
        int64_t  a, b;

        if (pc >= nw) break;
        w = &words[pc];

        if (!meng_machine_step(m)) return MENG_ERR_TRAP;
        pc++;

        switch (w->op) {
        case MENG_OP_NOP:
            break;

        case MENG_OP_PUSHI:
            if (depth + 2u > MENG_K_STACK_CELLS) {
                return MENG_ERR_STACK_OVERFLOW;
            }
            cells[depth++] = (uint64_t)w->a0;
            cells[depth++] = MENG_K_T_INT;
            break;

        case MENG_OP_PUSHS: {
            size_t off, l;
            if (w->a0 < 0 || w->a1 < 0) return MENG_ERR_RUNTIME;
            off = (size_t)w->a0;
            l = (size_t)w->a1;
            if (off > MENG_POOL_SIZE || l > MENG_POOL_SIZE - off) {
                return MENG_ERR_RUNTIME;
            }
            if (depth + 2u > MENG_K_STACK_CELLS) {
                return MENG_ERR_STACK_OVERFLOW;
            }
            cells[depth++] = (uint64_t)(uintptr_t)(meng_pool + off);
            cells[depth++] = ((uint64_t)l << 2) | MENG_K_T_STR;
            break;
        }

        case MENG_OP_DUP2:
            if (depth < 2u) return MENG_ERR_RUNTIME;
            if (depth + 2u > MENG_K_STACK_CELLS) {
                return MENG_ERR_STACK_OVERFLOW;
            }
            cells[depth] = cells[depth - 2u];
            cells[depth + 1u] = cells[depth - 1u];
            depth += 2u;
            break;

        case MENG_OP_SWAP2:
            if (depth < 4u) return MENG_ERR_RUNTIME;
            {
                uint64_t p0 = cells[depth - 4u], m0 = cells[depth - 3u];
                cells[depth - 4u] = cells[depth - 2u];
                cells[depth - 3u] = cells[depth - 1u];
                cells[depth - 2u] = p0;
                cells[depth - 1u] = m0;
            }
            break;

        case MENG_OP_DROP2:
            if (depth < 2u) return MENG_ERR_RUNTIME;
            depth -= 2u;
            break;

        case MENG_OP_IADD:
        case MENG_OP_ISUB:
        case MENG_OP_IMUL:
        case MENG_OP_IDIV:
        case MENG_OP_IMOD:
            if (depth < 4u) return MENG_ERR_RUNTIME;
            meta = cells[depth - 1u];
            b = (int64_t)cells[depth - 2u];
            if (meng_cell_type(meta) != MENG_K_T_INT) return MENG_ERR_TYPE;
            depth -= 2u;
            meta = cells[depth - 1u];
            a = (int64_t)cells[depth - 2u];
            if (meng_cell_type(meta) != MENG_K_T_INT) return MENG_ERR_TYPE;
            depth -= 2u;
            if (w->op == MENG_OP_IADD) {
                a = a + b;
            } else if (w->op == MENG_OP_ISUB) {
                a = a - b;
            } else if (w->op == MENG_OP_IMUL) {
                a = a * b;
            } else {
                if (b == 0) return MENG_ERR_RUNTIME;
                if (w->op == MENG_OP_IDIV) a = a / b;
                else                       a = a % b;
            }
            if (depth + 2u > MENG_K_STACK_CELLS) {
                return MENG_ERR_STACK_OVERFLOW;
            }
            cells[depth++] = (uint64_t)a;
            cells[depth++] = MENG_K_T_INT;
            break;

        case MENG_OP_INEG:
            if (depth < 2u) return MENG_ERR_RUNTIME;
            meta = cells[depth - 1u];
            if (meng_cell_type(meta) != MENG_K_T_INT) return MENG_ERR_TYPE;
            a = (int64_t)cells[depth - 2u];
            cells[depth - 2u] = (uint64_t)(-a);
            break;

        case MENG_OP_CMPEQ:
        case MENG_OP_CMPNE:
        case MENG_OP_CMPLT:
        case MENG_OP_CMPLE: {
            int r;
            if (depth < 4u) return MENG_ERR_RUNTIME;
            meta = cells[depth - 1u];
            b = (int64_t)cells[depth - 2u];
            if (meng_cell_type(meta) != MENG_K_T_INT) return MENG_ERR_TYPE;
            depth -= 2u;
            meta = cells[depth - 1u];
            a = (int64_t)cells[depth - 2u];
            if (meng_cell_type(meta) != MENG_K_T_INT) return MENG_ERR_TYPE;
            depth -= 2u;
            if (w->op == MENG_OP_CMPEQ) r = (a == b);
            else if (w->op == MENG_OP_CMPNE) r = (a != b);
            else if (w->op == MENG_OP_CMPLT) r = (a < b);
            else r = (a <= b);
            cells[depth++] = (uint64_t)(int64_t)r;
            cells[depth++] = MENG_K_T_INT;
            break;
        }

        case MENG_OP_JNZ:
            if (depth < 2u) return MENG_ERR_RUNTIME;
            meta = cells[depth - 1u];
            if (meng_cell_type(meta) != MENG_K_T_INT) return MENG_ERR_TYPE;
            a = (int64_t)cells[depth - 2u];
            depth -= 2u;
            if (a != 0) {
                if (w->a0 < 0 || (uint64_t)w->a0 > nw) {
                    return MENG_ERR_RUNTIME;
                }
                pc = (uint32_t)w->a0;
            }
            break;

        case MENG_OP_JMP:
            if (w->a0 < 0 || (uint64_t)w->a0 > nw) return MENG_ERR_RUNTIME;
            pc = (uint32_t)w->a0;
            break;

        case MENG_OP_GLOAD:
            if (w->a0 < 0 || (uint64_t)w->a0 >= ng) return MENG_ERR_RUNTIME;
            if (depth + 2u > MENG_K_STACK_CELLS) {
                return MENG_ERR_STACK_OVERFLOW;
            }
            cells[depth++] = (uint64_t)g[w->a0];
            cells[depth++] = MENG_K_T_INT;
            break;

        case MENG_OP_GSTORE:
            if (w->a0 < 0 || (uint64_t)w->a0 >= ng) return MENG_ERR_RUNTIME;
            if (depth < 2u) return MENG_ERR_RUNTIME;
            meta = cells[depth - 1u];
            if (meng_cell_type(meta) != MENG_K_T_INT) return MENG_ERR_TYPE;
            g[w->a0] = (int64_t)cells[depth - 2u];
            depth -= 2u;
            break;

        case MENG_OP_CALL:
            if (w->a0 < 0 || (uint64_t)w->a0 >= nw) return MENG_ERR_RUNTIME;
            if (rdepth >= MENG_K_STACK_CELLS / 2u) {
                return MENG_ERR_STACK_OVERFLOW;
            }
            rstk[rdepth++] = pc;
            pc = (uint32_t)w->a0;
            break;

        case MENG_OP_RET:
            if (rdepth == 0u) return MENG_ERR_RUNTIME;
            pc = rstk[--rdepth];
            break;

        case MENG_OP_CONCAT: {
            uint64_t ma, mb;
            const uint8_t *pa, *pb;
            uint32_t la, lb, k;
            if (depth < 4u) return MENG_ERR_RUNTIME;
            mb = cells[depth - 1u];
            pb = (const uint8_t *)(uintptr_t)cells[depth - 2u];
            if (meng_cell_type(mb) != MENG_K_T_STR) return MENG_ERR_TYPE;
            lb = meng_cell_strlen(mb);
            depth -= 2u;
            ma = cells[depth - 1u];
            pa = (const uint8_t *)(uintptr_t)cells[depth - 2u];
            if (meng_cell_type(ma) != MENG_K_T_STR) return MENG_ERR_TYPE;
            la = meng_cell_strlen(ma);
            depth -= 2u;
            {
                uint32_t total = la + lb;
                if (total > MENG_SCRATCH_SIZE) return MENG_BAN_SCRATCH;
                for (k = 0u; k < la; k++) m->_scratch[k] = pa[k];
                for (k = 0u; k < lb; k++) m->_scratch[la + k] = pb[k];
                m->_scratch_len = total;
            }
            if (depth + 2u > MENG_K_STACK_CELLS) {
                return MENG_ERR_STACK_OVERFLOW;
            }
            cells[depth++] = (uint64_t)(uintptr_t)m->_scratch;
            cells[depth++] = ((uint64_t)m->_scratch_len << 2) | MENG_K_T_STR;
            break;
        }

        case MENG_OP_PRINT:
            if (depth < 2u) return MENG_ERR_RUNTIME;
            meta = cells[depth - 1u];
            tag = meng_cell_type(meta);
            if (tag == MENG_K_T_INT) {
                depth -= 2u;
                a = (int64_t)cells[depth];
                {
                    meng_status_t ps = meng_machine_out_int(m, a);
                    if (ps != MENG_OK && ps != MENG_WARN_TRUNC) return ps;
                }
            } else if (tag == MENG_K_T_STR) {
                uint32_t l = meng_cell_strlen(meta);
                const uint8_t *p =
                    (const uint8_t *)(uintptr_t)cells[depth - 2u];
                depth -= 2u;
                {
                    meng_status_t ps = meng_machine_out(m, p, (size_t)l);
                    if (ps != MENG_OK && ps != MENG_WARN_TRUNC) return ps;
                }
            } else {
                return MENG_ERR_RUNTIME;
            }
            break;

        case MENG_OP_HALT:
            return MENG_OK;

        default:
            return MENG_BAN_CORE;
        }
    }

    return MENG_OK;
}

meng_status_t meng_kernel_exec(meng_machine_t *m, const uint8_t *src,
                               size_t len, int64_t *g, uint32_t ng,
                               uint32_t *pc)
{
    meng_word_t   words[MENG_K_WORDS];
    uint32_t      count = 0u;
    meng_status_t st;

    if (!m || !src) return MENG_ERR_BAD_ARG;
    st = meng_kernel_assemble(src, len, words, MENG_K_WORDS, &count);
    if (st != MENG_OK) return st;
    st = meng_kernel_run(m, words, count, g, ng);
    if (pc) *pc = count;
    return st;
}

/* ------------------------------------------------------------------ */
/*  Bytecode engine (poco / managednet front-end)                      */
/* ------------------------------------------------------------------ */

static meng_status_t engine_bytecode_run(meng_machine_t *m,
                                         const uint8_t *manifest,
                                         size_t manifest_len)
{
    static int64_t g[MENG_K_GLOBALS];
    uint32_t i;

    if (!m || !manifest) return MENG_ERR_BAD_ARG;
    for (i = 0u; i < MENG_K_GLOBALS; i++) g[i] = 0;
    return meng_kernel_exec(m, manifest, manifest_len, g, MENG_K_GLOBALS,
                            NULL);
}

/* ------------------------------------------------------------------ */
/*  manageddl - meta engine (key-prefixed nested dispatch)             */
/* ------------------------------------------------------------------ */

static meng_status_t engine_dl_run(meng_machine_t *m,
                                   const uint8_t *manifest,
                                   size_t manifest_len)
{
    uint32_t key;

    if (!m || !manifest) return MENG_ERR_BAD_ARG;
    if (manifest_len < 4u) return MENG_ERR_BAD_ARG;

    key = (uint32_t)manifest[0] |
          ((uint32_t)manifest[1] << 8) |
          ((uint32_t)manifest[2] << 16) |
          ((uint32_t)manifest[3] << 24);

    return meng_engine_dispatch_m(m, key, manifest + 4, manifest_len - 4u);
}

/* ------------------------------------------------------------------ */
/*  managedcom - component registry                                    */
/* ------------------------------------------------------------------ */

static meng_com_factory_t  meng_com_factories[MENG_COM_MAX_FACTORIES];
static uint32_t            meng_com_factory_n = 0u;
static meng_com_instance_t meng_com_instances[MENG_COM_MAX_INSTANCES];
static uint32_t            meng_com_next_id = 1u;
static meng_com_instance_t *meng_com_current = NULL;
static uint32_t            meng_com_current_iid = 0u;

uint32_t meng_com_factory_count(void)
{
    return meng_com_factory_n;
}

uint32_t meng_com_instance_count(void)
{
    uint32_t n = 0u, i;

    for (i = 0u; i < MENG_COM_MAX_INSTANCES; i++) {
        if (meng_com_instances[i].id != 0u) n++;
    }
    return n;
}

meng_status_t meng_com_factory_register(uint32_t key,
                                        meng_com_factory_fn create,
                                        void *owner)
{
    uint32_t i;

    if (key == 0u || !create) return MENG_ERR_BAD_ARG;
    for (i = 0u; i < meng_com_factory_n; i++) {
        if (meng_com_factories[i].key == key) return MENG_ERR_DUPLICATE;
    }
    if (meng_com_factory_n >= MENG_COM_MAX_FACTORIES) {
        return MENG_ERR_SHORT_OUT;
    }
    meng_com_factories[meng_com_factory_n].key = key;
    meng_com_factories[meng_com_factory_n].create = create;
    meng_com_factories[meng_com_factory_n].owner = owner;
    meng_com_factory_n++;
    return MENG_OK;
}

meng_status_t meng_com_factory_lookup(uint32_t key,
                                      meng_com_factory_t **out)
{
    uint32_t i;

    if (!out) return MENG_ERR_BAD_ARG;
    for (i = 0u; i < meng_com_factory_n; i++) {
        if (meng_com_factories[i].key == key) {
            *out = &meng_com_factories[i];
            return MENG_OK;
        }
    }
    return MENG_ERR_NOT_FOUND;
}

meng_status_t meng_com_create(uint32_t key, const uint8_t ccc[4],
                              uint32_t *instance_id)
{
    meng_com_factory_t *f = NULL;
    meng_com_instance_t *inst = NULL;
    uint32_t i;
    meng_status_t st;

    st = meng_com_factory_lookup(key, &f);
    if (st != MENG_OK) return st;

    for (i = 0u; i < MENG_COM_MAX_INSTANCES; i++) {
        if (meng_com_instances[i].id == 0u) {
            inst = &meng_com_instances[i];
            break;
        }
    }
    if (!inst) return MENG_ERR_SHORT_OUT;

    if (meng_com_next_id == 0u) meng_com_next_id = 1u;
    inst->id = meng_com_next_id++;
    inst->factory = key;
    inst->data = NULL;
    inst->iface_count = 0u;
    {
        uint32_t k;
        for (k = 0u; k < MENG_COM_MAX_IFACE; k++) {
            inst->ifaces[k].iid = 0u;
        }
    }

    st = f->create(ccc, inst, inst->id);
    if (st != MENG_OK) {
        inst->id = 0u;
        return st;
    }
    if (instance_id) *instance_id = inst->id;
    return MENG_OK;
}

meng_status_t meng_com_destroy(uint32_t instance_id)
{
    uint32_t i;

    for (i = 0u; i < MENG_COM_MAX_INSTANCES; i++) {
        if (meng_com_instances[i].id == instance_id) {
            meng_com_instances[i].id = 0u;
            return MENG_OK;
        }
    }
    return MENG_ERR_NOT_FOUND;
}

meng_status_t meng_com_resolve(uint32_t instance_id, uint32_t iid,
                               meng_com_instance_t **out)
{
    uint32_t i;

    (void)iid;
    if (!out) return MENG_ERR_BAD_ARG;
    for (i = 0u; i < MENG_COM_MAX_INSTANCES; i++) {
        if (meng_com_instances[i].id == instance_id) {
            *out = &meng_com_instances[i];
            return MENG_OK;
        }
    }
    return MENG_ERR_NOT_FOUND;
}

meng_status_t meng_com_invoke(uint32_t instance_id, uint32_t iid,
                              uint32_t slot, const int64_t *args,
                              uint32_t argc, int64_t *result)
{
    meng_com_instance_t *inst = NULL;
    uint32_t i;

    if (argc > 0u && !args) return MENG_ERR_BAD_ARG;
    for (i = 0u; i < MENG_COM_MAX_INSTANCES; i++) {
        if (meng_com_instances[i].id == instance_id) {
            inst = &meng_com_instances[i];
            break;
        }
    }
    if (!inst) return MENG_ERR_NOT_FOUND;

    for (i = 0u; i < inst->iface_count; i++) {
        if (inst->ifaces[i].iid == iid) {
            meng_com_method_fn fn;
            if (slot >= MENG_COM_MAX_IFACE) return MENG_ERR_BAD_ARG;
            fn = inst->ifaces[i].mslots[slot].fn;
            if (!fn) return MENG_ERR_BAD_ARG;
            return fn(inst, args, argc, result);
        }
    }
    return MENG_ERR_NOT_FOUND;
}

static meng_status_t engine_com_run(meng_machine_t *m,
                                    const uint8_t *manifest,
                                    size_t manifest_len)
{
    size_t i = 0u;

    if (!m || !manifest) return MENG_ERR_BAD_ARG;

    while (i < manifest_len) {
        char       linebuf[128];
        char       words[8][33];
        uint32_t   nw;
        size_t     ls = i;
        size_t     buflen = 0u;
        size_t     k;

        while (i < manifest_len && manifest[i] != '\n') i++;
        buflen = i - ls;
        if (buflen > 127u) buflen = 127u;
        for (k = 0u; k < buflen; k++) linebuf[k] = (char)manifest[ls + k];
        linebuf[buflen] = '\0';
        for (k = 0u; k < buflen; k++) {
            if (linebuf[k] == '#') { linebuf[k] = '\0'; break; }
        }
        nw = meng_split(linebuf, words, 8u);

        if (i < manifest_len) i++;

        if (nw == 0u) continue;

        {
            uint32_t w;
            for (w = 0u; w < nw; w++) {
                if (!meng_machine_step(m)) return MENG_ERR_TRAP;
            }
        }

        if (strcmp_wr(words[0], "create") == 0 && nw >= 2u) {
            uint32_t key = pack4cc((const uint8_t *)words[1]);
            uint32_t id = 0u;
            meng_status_t st;

            st = meng_com_create(key, (const uint8_t *)words[1], &id);
            if (st != MENG_OK) return st;
            if (meng_com_resolve(id, 0u, &meng_com_current) != MENG_OK) {
                return MENG_ERR_STATE;
            }
            meng_com_current_iid = 0u;
        } else if (strcmp_wr(words[0], "resolv") == 0 && nw >= 2u) {
            uint32_t iid = meng_fnv1a32((const uint8_t *)words[1],
                                        strlen32(words[1]));
            bool      present = false;
            uint32_t  k;

            if (meng_com_current == NULL) return MENG_ERR_STATE;
            for (k = 0u; k < meng_com_current->iface_count; k++) {
                if (meng_com_current->ifaces[k].iid == iid) present = true;
            }
            if (!present) return MENG_ERR_NOT_FOUND;
            meng_com_current_iid = iid;
        } else if (strcmp_wr(words[0], "call") == 0 && nw >= 2u) {
            int64_t      args[4];
            uint32_t     argc = 0u;
            int64_t      slot;
            meng_status_t es;

            if (meng_com_current == NULL) return MENG_ERR_STATE;
            slot = parse_i64(words[1], &es);
            if (es != MENG_OK) return MENG_ERR_SYNTAX;
            {
                uint32_t k;
                for (k = 2u; k < nw && k < 6u; k++) {
                    args[argc++] = parse_i64(words[k], &es);
                    if (es != MENG_OK) return MENG_ERR_SYNTAX;
                }
            }
            es = meng_com_invoke(meng_com_current->id,
                                 meng_com_current_iid, (uint32_t)slot,
                                 args, argc, &(int64_t){0});
            if (es != MENG_OK) return es;
        } else {
            return MENG_ERR_SYNTAX;
        }
    }

    return MENG_OK;
}

/* ------------------------------------------------------------------ */
/*  managedsh - shell engine                                           */
/* ------------------------------------------------------------------ */

typedef struct meng_sh_var {
    char     name[16];
    bool     is_str;
    int64_t  v;
    char     sval[32];
    uint32_t slen;
} meng_sh_var_t;

#define MENG_SH_MAX_VARS    16u
#define MENG_SH_MAX_LABELS  32u

static meng_sh_var_t meng_sh_vars[MENG_SH_MAX_VARS];
static uint32_t      meng_sh_var_count = 0u;

typedef struct meng_sh_label {
    char     name[16];
    uint32_t line;
} meng_sh_label_t;

static meng_sh_var_t *meng_sh_find_var(const char *name)
{
    uint32_t i;

    for (i = 0u; i < meng_sh_var_count; i++) {
        if (strcmp_wr(meng_sh_vars[i].name, name) == 0) {
            return &meng_sh_vars[i];
        }
    }
    return NULL;
}

static meng_status_t meng_sh_set_var(const char *name, int64_t v,
                                     bool is_str, const char *sv,
                                     uint32_t slen)
{
    meng_sh_var_t *vx = meng_sh_find_var(name);

    if (!name) return MENG_ERR_BAD_ARG;
    if (!vx) {
        if (meng_sh_var_count >= MENG_SH_MAX_VARS) return MENG_ERR_SHORT_OUT;
        /* copy name (max 15 chars + NUL) */
        {
            char nb[16];
            uint32_t i;
            for (i = 0u; i < 15u && name[i] != '\0'; i++) nb[i] = name[i];
            nb[i] = '\0';
            vx = &meng_sh_vars[meng_sh_var_count++];
            for (i = 0u; i < 16u; i++) vx->name[i] = nb[i];
        }
    }
    vx->is_str = is_str;
    if (is_str) {
        uint32_t lim = slen < 31u ? slen : 31u;
        uint32_t i;
        for (i = 0u; i < lim; i++) vx->sval[i] = sv[i];
        vx->sval[lim] = '\0';
        vx->slen = lim;
        vx->v = 0;
    } else {
        vx->v = v;
        vx->slen = 0u;
    }
    return MENG_OK;
}

/* Expand a single word (which may contain $NAME references) to output. */
static meng_status_t meng_sh_expand_word(meng_machine_t *m,
                                         const char *word)
{
    size_t i = 0u;
    meng_status_t st;

    while (word[i] != '\0') {
        if (word[i] == '$') {
            char  nb[16];
            uint32_t ni = 0u;
            size_t j = i + 1u;
            meng_sh_var_t *vx;

            while (word[j] != '\0' && ni < 15u) {
                char c = word[j];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '_')) break;
                nb[ni++] = c;
                j++;
            }
            nb[ni] = '\0';
            i = j;
            if (ni == 0u) {
                st = meng_machine_out_char(m, '$');
                if (st != MENG_OK && st != MENG_WARN_TRUNC) return st;
                continue;
            }
            vx = meng_sh_find_var(nb);
            if (vx) {
                if (vx->is_str) {
                    st = meng_machine_out(m, (const uint8_t *)vx->sval,
                                          (size_t)vx->slen);
                } else {
                    st = meng_machine_out_int(m, vx->v);
                }
                if (st != MENG_OK && st != MENG_WARN_TRUNC) return st;
            }
        } else {
            if (!meng_machine_step(m)) return MENG_ERR_TRAP;
            st = meng_machine_out_char(m, word[i]);
            if (st != MENG_OK && st != MENG_WARN_TRUNC) return st;
            i++;
        }
    }
    return MENG_OK;
}

static int64_t meng_sh_expand_int(const char *txt, meng_status_t *st)
{
    meng_status_t e;

    if (txt[0] == '$') {
        meng_sh_var_t *vx = meng_sh_find_var(txt + 1);
        if (!vx || vx->is_str) { *st = MENG_ERR_RUNTIME; return 0; }
        *st = MENG_OK;
        return vx->v;
    }
    {
        int64_t v = parse_i64(txt, &e);
        if (e != MENG_OK) { *st = e; return 0; }
        *st = MENG_OK;
        return v;
    }
}

/* Locate the byte span of `lineno` (0-based). Returns false at EOF. */
static bool meng_sh_find_line(const uint8_t *manifest, size_t len,
                              uint32_t lineno, size_t *start, size_t *end)
{
    uint32_t c = 0u;
    size_t   i = 0u;

    while (i < len) {
        *start = i;
        if (c == lineno) {
            while (i < len && manifest[i] != '\n') i++;
            *end = i;
            return true;
        }
        while (i < len && manifest[i] != '\n') i++;
        if (i < len) i++;
        c++;
    }
    if (c == lineno) {   /* final line without newline */
        *start = 0u;
        *end = 0u;
        return false;
    }
    return false;
}

static meng_status_t engine_sh_run(meng_machine_t *m,
                                   const uint8_t *manifest,
                                   size_t manifest_len)
{
    meng_sh_label_t labels[MENG_SH_MAX_LABELS];
    uint32_t nlabels = 0u;
    uint32_t lineno = 0u;
    meng_status_t st;

    if (!m || !manifest) return MENG_ERR_BAD_ARG;
    meng_sh_var_count = 0u;

    /* pass 1 - collect label -> line mappings (forward refs allowed) */
    {
        uint32_t l = 0u;

        for (;;) {
            size_t s, e;
            char   linebuf[128];
            char   words[8][33];
            uint32_t nw;
            size_t   buflen, p, k;

            if (!meng_sh_find_line(manifest, manifest_len, l, &s, &e)) break;

            p = s;
            while (p < e && manifest[p] == ' ') p++;
            if (p >= e || manifest[p] == '#') { l++; continue; }
            buflen = e - p;
            if (buflen > 127u) buflen = 127u;
            for (k = 0u; k < buflen; k++) linebuf[k] = (char)manifest[p + k];
            linebuf[buflen] = '\0';
            for (k = 0u; k < buflen; k++) {
                if (linebuf[k] == '#') { linebuf[k] = '\0'; break; }
            }
            nw = meng_split(linebuf, words, 8u);
            if (nw >= 1u) {
                size_t wl = strlen32(words[0]);
                if (wl >= 1u && words[0][wl - 1u] == ':') {
                    if (nlabels < MENG_SH_MAX_LABELS) {
                        char nm[16];
                        uint32_t i2;
                        uint32_t L = (wl - 1u < 15u) ? (uint32_t)(wl - 1u) : 15u;
                        for (i2 = 0u; i2 < L; i2++) nm[i2] = words[0][i2];
                        for (i2 = 0u; i2 < L; i2++) labels[nlabels].name[i2] = nm[i2];
                        labels[nlabels].name[L] = '\0';
                        labels[nlabels].line = l;
                        nlabels++;
                    }
                }
            }
            l++;
        }
    }

    /* pass 2 - execute */
    for (;;) {
        size_t s, e, p, buflen, k;
        char   linebuf[128];
        char   words[8][33];
        uint32_t nw;

        if (!meng_sh_find_line(manifest, manifest_len, lineno, &s, &e)) break;
        if (!meng_machine_step(m)) return MENG_ERR_TRAP;

        p = s;
        while (p < e && manifest[p] == ' ') p++;
        if (p >= e || manifest[p] == '#') { lineno++; continue; }

        buflen = e - p;
        if (buflen > 127u) buflen = 127u;
        for (k = 0u; k < buflen; k++) linebuf[k] = (char)manifest[p + k];
        linebuf[buflen] = '\0';
        for (k = 0u; k < buflen; k++) {
            if (linebuf[k] == '#') { linebuf[k] = '\0'; break; }
        }
        nw = meng_split(linebuf, words, 8u);
        if (nw == 0u) { lineno++; continue; }

        if (strcmp_wr(words[0], "halt") == 0 ||
            strcmp_wr(words[0], "quit") == 0) {
            break;
        } else if (strcmp_wr(words[0], "set") == 0 && nw >= 3u) {
            meng_status_t e2;
            int64_t v = parse_i64(words[2], &e2);
            if (e2 == MENG_OK) {
                st = meng_sh_set_var(words[1], v, false, NULL, 0u);
            } else {
                st = meng_sh_set_var(words[1], 0, true, words[2],
                                     strlen32(words[2]));
            }
            if (st != MENG_OK) return st;
            lineno++;
        } else if (strcmp_wr(words[0], "inc") == 0 && nw >= 2u) {
            meng_sh_var_t *vx = meng_sh_find_var(words[1]);
            if (!vx || vx->is_str) return MENG_ERR_RUNTIME;
            vx->v += 1;
            lineno++;
        } else if (strcmp_wr(words[0], "echo") == 0 && nw >= 2u) {
            uint32_t w;
            for (w = 1u; w < nw; w++) {
                if (w > 1u) {
                    st = meng_machine_out_char(m, ' ');
                    if (st != MENG_OK && st != MENG_WARN_TRUNC) return st;
                }
                st = meng_sh_expand_word(m, words[w]);
                if (st != MENG_OK) return st;
            }
            lineno++;
        } else if (strcmp_wr(words[0], "label") == 0 && nw >= 2u) {
            lineno++;                    /* already collected in pass 1 */
        } else if (words[0][strlen32(words[0]) - 1u] == ':') {
            lineno++;                    /* `name:` label definition   */
        } else if (strcmp_wr(words[0], "goto") == 0 && nw >= 2u) {
            bool     cond = true;
            uint32_t tgt = 0u;
            bool     found = false;
            uint32_t j;

            if (nw >= 6u && strcmp_wr(words[2], "IF") == 0) {
                meng_status_t e2;
                int64_t av = meng_sh_expand_int(words[3], &e2);
                int64_t bv = meng_sh_expand_int(words[5], &e2);
                if (e2 != MENG_OK) return e2;
                if (strcmp_wr(words[4], "lt") == 0) cond = av < bv;
                else if (strcmp_wr(words[4], "le") == 0) cond = av <= bv;
                else if (strcmp_wr(words[4], "gt") == 0) cond = av > bv;
                else if (strcmp_wr(words[4], "ge") == 0) cond = av >= bv;
                else if (strcmp_wr(words[4], "eq") == 0) cond = av == bv;
                else if (strcmp_wr(words[4], "ne") == 0) cond = av != bv;
                else return MENG_ERR_SYNTAX;
            }

            for (j = 0u; j < nlabels; j++) {
                if (strcmp_wr(labels[j].name, words[1]) == 0) {
                    tgt = labels[j].line;
                    found = true;
                    break;
                }
            }
            if (!found) return MENG_ERR_SYNTAX;
            if (cond) lineno = tgt;
            else lineno++;
        } else {
            return MENG_ERR_SYNTAX;
        }
    }

    return MENG_OK;
}

/* ------------------------------------------------------------------ */
/*  managedpy - Python-flavored subset lowered to the poco IR          */
/* ------------------------------------------------------------------ */
/*  Manuscript grammar (verified subset):                              */
/*    stmt   := 'print' expr | name '=' cond | 'while' cond ':'        */
/*            | 'if' cond ':' ['else' ':']  (block via indentation)    */
/*    cond   := expr [cmp expr]                                        */
/*    expr   := term (('+'|'-') term)*                                 */
/*    term   := factor (('*'|'/'|'%') factor)*                         */
/*    factor := number | name | '(' expr ')' | '-' factor              */
/*  Everything lowers straight into meng_word_t IR and is executed by  */
/*  meng_kernel_run (the same determinism, budget, and TRAP rules as   */
/*  the textual bytecode front-ends).                                  */

#define MENG_PY_MAX_VARS     16u
#define MENG_PY_MAX_STMTS    64u
#define MENG_PY_MAX_TOK      16u
#define MENG_PY_MAX_DEPTH    24u

typedef enum {
    PY_TK_NUM = 0,
    PY_TK_IDENT,
    PY_TK_OP,
    PY_TK_STR
} py_tk_kind_t;

typedef struct py_tk {
    py_tk_kind_t kind;
    int64_t      num;
    char         name[16];
    uint8_t      op;          /* operator code (PY_OP_*)               */
    uint32_t     so;          /* string pool offset (PY_TK_STR)        */
    uint32_t     sl;          /* string length                         */
} py_tk_t;

enum {
    PY_OP_ADD, PY_OP_SUB, PY_OP_MUL, PY_OP_DIV, PY_OP_MOD,
    PY_OP_LP, PY_OP_RP, PY_OP_EQ, PY_OP_NE, PY_OP_LT, PY_OP_LE,
    PY_OP_GT, PY_OP_GE, PY_OP_ASSIGN, PY_OP_COLON
};

/* statement classes (manifest lines, pre-block structure) */
enum {
    PY_S_ASSIGN = 0,
    PY_S_PRINT,
    PY_S_WHILE,
    PY_S_IF,
    PY_S_ELSE
};

typedef struct py_stmt {
    uint16_t indent;
    uint8_t  kind;
    uint8_t  nt;
    py_tk_t  tk[MENG_PY_MAX_TOK];
} py_stmt_t;

static char py_vars[MENG_PY_MAX_VARS][16];
static uint32_t py_var_count = 0u;

static uint32_t py_var_find(const char *name)
{
    uint32_t i;

    for (i = 0u; i < py_var_count; i++) {
        if (strcmp_wr(py_vars[i], name) == 0) return i;
    }
    return UINT32_MAX;
}

static meng_status_t py_var_declare(const char *name, uint32_t *slot)
{
    uint32_t i;
    uint32_t k;

    if (!name || !slot) return MENG_ERR_BAD_ARG;
    i = py_var_find(name);
    if (i != UINT32_MAX) {
        *slot = i;
        return MENG_OK;
    }
    if (py_var_count >= MENG_PY_MAX_VARS) return MENG_ERR_SHORT_OUT;
    for (k = 0u; k < 15u && name[k] != '\0'; k++) {
        py_vars[py_var_count][k] = name[k];
    }
    py_vars[py_var_count][k] = '\0';
    *slot = py_var_count;
    py_var_count++;
    return MENG_OK;
}

static meng_status_t py_emit(meng_word_t *words, uint32_t cap, uint32_t *wc,
                             uint16_t op, int64_t a0, int64_t a1)
{
    if (*wc >= cap) return MENG_ERR_SHORT_OUT;
    words[*wc].op = op;
    words[*wc].pad = 0;
    words[*wc].a0 = a0;
    words[*wc].a1 = a1;
    (*wc)++;
    return MENG_OK;
}

static meng_status_t py_tokenize(const char *s, py_tk_t *tk, uint32_t cap,
                                 uint32_t *n)
{
    uint32_t i = 0u;
    uint32_t nt = 0u;

    *n = 0u;
    while (s[i] != '\0') {
        char c = s[i];

        if (c == ' ' || c == '\t' || c == '\r') { i++; continue; }
        if (c == '#') break;
        if (nt >= cap) return MENG_ERR_SHORT_OUT;

        if (c >= '0' && c <= '9') {
            int64_t v = 0;

            while (s[i] >= '0' && s[i] <= '9') {
                v = v * (int64_t)10 + (int64_t)(s[i] - '0');
                i++;
            }
            tk[nt].kind = PY_TK_NUM;
            tk[nt].num = v;
            nt++;
            continue;
        }

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            uint32_t k = 0u;

            while (((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    c == '_' || (c >= '0' && c <= '9'))) {
                if (k + 1u < 16u) tk[nt].name[k++] = c;
                i++;
                c = s[i];
            }
            tk[nt].name[k] = '\0';
            tk[nt].kind = PY_TK_IDENT;
            nt++;
            continue;
        }

        if (c == '"') {
            uint32_t off = meng_pool_len;
            uint32_t pl = 0u;

            i++;
            while (s[i] != '\0' && s[i] != '"') {
                uint8_t d = (uint8_t)s[i];
                if (d == '\\' && s[i + 1u] != '\0') {
                    if (s[i + 1u] == 'n')      d = (uint8_t)'\n';
                    else if (s[i + 1u] == 't') d = (uint8_t)'\t';
                    else if (s[i + 1u] == '\\') d = (uint8_t)'\\';
                    else if (s[i + 1u] == '"') d = (uint8_t)'"';
                    else                       d = (uint8_t)s[i + 1u];
                    i += 2u;
                } else {
                    i++;
                }
                if (meng_pool_len + 1u > MENG_POOL_SIZE) {
                    return MENG_BAN_SCRATCH;
                }
                meng_pool[meng_pool_len++] = d;
                pl++;
            }
            if (s[i] != '"') return MENG_ERR_SYNTAX;
            i++;
            tk[nt].kind = PY_TK_STR;
            tk[nt].so = off;
            tk[nt].sl = pl;
            nt++;
            continue;
        }

        {
            uint8_t op = 0u;

            if (c == '=' && s[i + 1u] == '=')      { op = PY_OP_EQ;   i += 2u; }
            else if (c == '!' && s[i + 1u] == '=') { op = PY_OP_NE;   i += 2u; }
            else if (c == '<' && s[i + 1u] == '=') { op = PY_OP_LE;   i += 2u; }
            else if (c == '>' && s[i + 1u] == '=') { op = PY_OP_GE;   i += 2u; }
            else if (c == '<')                     { op = PY_OP_LT;   i += 1u; }
            else if (c == '>')                     { op = PY_OP_GT;   i += 1u; }
            else if (c == '+')                     { op = PY_OP_ADD;  i += 1u; }
            else if (c == '-')                     { op = PY_OP_SUB;  i += 1u; }
            else if (c == '*')                     { op = PY_OP_MUL;  i += 1u; }
            else if (c == '/')                     { op = PY_OP_DIV;  i += 1u; }
            else if (c == '%')                     { op = PY_OP_MOD;  i += 1u; }
            else if (c == '(')                     { op = PY_OP_LP;   i += 1u; }
            else if (c == ')')                     { op = PY_OP_RP;   i += 1u; }
            else if (c == '=')                     { op = PY_OP_ASSIGN; i += 1u; }
            else if (c == ':')                     { op = PY_OP_COLON; i += 1u; }
            else return MENG_ERR_SYNTAX;
            tk[nt].kind = PY_TK_OP;
            tk[nt].op = op;
            nt++;
        }
    }
    *n = nt;
    return MENG_OK;
}

static meng_status_t py_expr(meng_word_t *words, uint32_t cap, uint32_t *wc,
                             const py_tk_t *tk, uint32_t lo, uint32_t hi,
                             uint32_t *pos);

static meng_status_t py_factor(meng_word_t *words, uint32_t cap, uint32_t *wc,
                               const py_tk_t *tk, uint32_t lo, uint32_t hi,
                               uint32_t *pos)
{
    uint32_t p = *pos;
    meng_status_t st;

    if (p > hi) return MENG_ERR_SYNTAX;
    if (tk[p].kind == PY_TK_NUM) {
        st = py_emit(words, cap, wc, MENG_OP_PUSHI, tk[p].num, 0);
        if (st != MENG_OK) return st;
        *pos = p + 1u;
        return MENG_OK;
    }
    if (tk[p].kind == PY_TK_IDENT) {
        uint32_t slot = py_var_find(tk[p].name);
        if (slot == UINT32_MAX) return MENG_ERR_RUNTIME;   /* NameError */
        st = py_emit(words, cap, wc, MENG_OP_GLOAD, (int64_t)slot, 0);
        if (st != MENG_OK) return st;
        *pos = p + 1u;
        return MENG_OK;
    }
    if (tk[p].kind == PY_TK_OP) {
        if (tk[p].op == PY_OP_LP) {
            *pos = p + 1u;
            st = py_expr(words, cap, wc, tk, lo, hi, pos);
            if (st != MENG_OK) return st;
            if (*pos > hi || tk[*pos].kind != PY_TK_OP ||
                tk[*pos].op != PY_OP_RP) {
                return MENG_ERR_SYNTAX;
            }
            (*pos)++;
            return MENG_OK;
        }
        if (tk[p].op == PY_OP_SUB) {
            *pos = p + 1u;
            st = py_factor(words, cap, wc, tk, lo, hi, pos);
            if (st != MENG_OK) return st;
            return py_emit(words, cap, wc, MENG_OP_INEG, 0, 0);
        }
    }
    return MENG_ERR_SYNTAX;
}

static meng_status_t py_term(meng_word_t *words, uint32_t cap, uint32_t *wc,
                             const py_tk_t *tk, uint32_t lo, uint32_t hi,
                             uint32_t *pos)
{
    meng_status_t st = py_factor(words, cap, wc, tk, lo, hi, pos);

    if (st != MENG_OK) return st;
    while (*pos <= hi && tk[*pos].kind == PY_TK_OP &&
           (tk[*pos].op == PY_OP_MUL || tk[*pos].op == PY_OP_DIV ||
            tk[*pos].op == PY_OP_MOD)) {
        uint8_t op = tk[*pos].op;
        (*pos)++;
        st = py_factor(words, cap, wc, tk, lo, hi, pos);
        if (st != MENG_OK) return st;
        st = py_emit(words, cap, wc,
                     (op == PY_OP_MUL) ? MENG_OP_IMUL :
                     (op == PY_OP_DIV) ? MENG_OP_IDIV : MENG_OP_IMOD, 0, 0);
        if (st != MENG_OK) return st;
    }
    return MENG_OK;
}

static meng_status_t py_expr(meng_word_t *words, uint32_t cap, uint32_t *wc,
                             const py_tk_t *tk, uint32_t lo, uint32_t hi,
                             uint32_t *pos)
{
    meng_status_t st = py_term(words, cap, wc, tk, lo, hi, pos);

    if (st != MENG_OK) return st;
    while (*pos <= hi && tk[*pos].kind == PY_TK_OP &&
           (tk[*pos].op == PY_OP_ADD || tk[*pos].op == PY_OP_SUB)) {
        uint8_t op = tk[*pos].op;
        (*pos)++;
        st = py_term(words, cap, wc, tk, lo, hi, pos);
        if (st != MENG_OK) return st;
        st = py_emit(words, cap, wc,
                     (op == PY_OP_ADD) ? MENG_OP_IADD : MENG_OP_ISUB, 0, 0);
        if (st != MENG_OK) return st;
    }
    return MENG_OK;
}

/* Conditional expression: optional single trailing comparison. */
static meng_status_t py_cond(meng_word_t *words, uint32_t cap, uint32_t *wc,
                             const py_tk_t *tk, uint32_t lo, uint32_t hi)
{
    uint32_t pos = lo;
    meng_status_t st = py_expr(words, cap, wc, tk, lo, hi, &pos);

    if (st != MENG_OK) return st;
    if (pos > hi) return MENG_OK;               /* truthy plain value   */
    if (tk[pos].kind != PY_TK_OP) return MENG_ERR_SYNTAX;

    {
        uint8_t op = tk[pos].op;

        if (op == PY_OP_EQ || op == PY_OP_NE || op == PY_OP_LT ||
            op == PY_OP_LE || op == PY_OP_GT || op == PY_OP_GE) {
            pos++;
            st = py_expr(words, cap, wc, tk, lo, hi, &pos);
            if (st != MENG_OK) return st;
            if (pos <= hi) return MENG_ERR_SYNTAX;
            switch (op) {
            case PY_OP_EQ: st = py_emit(words, cap, wc, MENG_OP_CMPEQ, 0, 0); break;
            case PY_OP_NE: st = py_emit(words, cap, wc, MENG_OP_CMPNE, 0, 0); break;
            case PY_OP_LT: st = py_emit(words, cap, wc, MENG_OP_CMPLT, 0, 0); break;
            case PY_OP_LE: st = py_emit(words, cap, wc, MENG_OP_CMPLE, 0, 0); break;
            case PY_OP_GT: st = py_emit(words, cap, wc, MENG_OP_SWAP2, 0, 0);
                           if (st == MENG_OK)
                               st = py_emit(words, cap, wc, MENG_OP_CMPLT, 0, 0);
                           break;
            default:       st = py_emit(words, cap, wc, MENG_OP_SWAP2, 0, 0);
                           if (st == MENG_OK)
                               st = py_emit(words, cap, wc, MENG_OP_CMPLE, 0, 0);
                           break;
            }
            return st;
        }
    }
    return MENG_ERR_SYNTAX;
}

static meng_status_t py_collect(const uint8_t *src, size_t len,
                                py_stmt_t *stmts, uint32_t cap, uint32_t *n)
{
    size_t i = 0u;

    *n = 0u;
    while (i < len) {
        char     linebuf[128];
        uint32_t blen = 0u;
        uint32_t indent = 0u;
        py_tk_t  tk[MENG_PY_MAX_TOK];
        uint32_t nt = 0u;
        size_t   ls = i;
        size_t   k;
        bool     q = false;
        uint32_t p;
        meng_status_t st;
        py_stmt_t *s;

        while (i < len && src[i] != '\n') i++;
        blen = (uint32_t)(i - ls);
        if (blen > 127u) blen = 127u;
        for (k = 0u; k < blen; k++) linebuf[k] = (char)src[ls + k];
        linebuf[blen] = '\0';
        if (i < len) i++;

        k = 0u;
        while (linebuf[k] == ' ' || linebuf[k] == '\t') { indent++; k++; }

        /* strip '#' comments while respecting string literals */
        for (p = (uint32_t)k; linebuf[p] != '\0'; p++) {
            if (linebuf[p] == '"') q = !q;
            else if (linebuf[p] == '#' && !q) { linebuf[p] = '\0'; break; }
        }

        /* skip blank lines */
        {
            uint32_t z = 0u;
            while (linebuf[k + (size_t)z] == ' ') z++;
            if (linebuf[k + (size_t)z] == '\0') continue;
        }

        st = py_tokenize(linebuf + (size_t)k, tk, MENG_PY_MAX_TOK, &nt);
        if (st != MENG_OK) return st;
        if (*n >= cap) return MENG_ERR_SHORT_OUT;

        s = &stmts[*n];
        s->indent = (uint16_t)indent;
        s->nt = (uint8_t)nt;
        {
            uint32_t z;
            for (z = 0u; z < nt; z++) s->tk[z] = tk[z];
        }

        if (s->nt < 1u || s->tk[0].kind != PY_TK_IDENT) {
            return MENG_ERR_SYNTAX;
        }

        if (strcmp_wr(s->tk[0].name, "print") == 0) {
            if (s->nt < 2u) return MENG_ERR_SYNTAX;
            s->kind = PY_S_PRINT;
        } else if (strcmp_wr(s->tk[0].name, "while") == 0) {
            if (s->nt < 3u || s->tk[s->nt - 1u].kind != PY_TK_OP ||
                s->tk[s->nt - 1u].op != PY_OP_COLON) return MENG_ERR_SYNTAX;
            s->kind = PY_S_WHILE;
        } else if (strcmp_wr(s->tk[0].name, "if") == 0) {
            if (s->nt < 3u || s->tk[s->nt - 1u].kind != PY_TK_OP ||
                s->tk[s->nt - 1u].op != PY_OP_COLON) return MENG_ERR_SYNTAX;
            s->kind = PY_S_IF;
        } else if (strcmp_wr(s->tk[0].name, "else") == 0) {
            if (s->nt > 2u) return MENG_ERR_SYNTAX;
            if (s->nt == 2u && (s->tk[1].kind != PY_TK_OP ||
                                s->tk[1].op != PY_OP_COLON)) {
                return MENG_ERR_SYNTAX;
            }
            s->kind = PY_S_ELSE;
        } else if (s->nt >= 3u && s->tk[1].kind == PY_TK_OP &&
                   s->tk[1].op == PY_OP_ASSIGN) {
            s->kind = PY_S_ASSIGN;
        } else {
            return MENG_ERR_SYNTAX;
        }

        (*n)++;
    }
    return MENG_OK;
}

static meng_status_t py_compile_block(meng_word_t *words, uint32_t cap,
                                      uint32_t *wc, const py_stmt_t *stmts,
                                      uint32_t n, uint32_t *idx,
                                      int32_t parent_indent, uint32_t depth)
{
    meng_status_t st;

    if (depth > MENG_PY_MAX_DEPTH) return MENG_ERR_SYNTAX;

    while (*idx < n && (int32_t)stmts[*idx].indent > parent_indent) {
        const py_stmt_t *s = &stmts[*idx];
        uint32_t d = (uint32_t)s->indent;

        switch (s->kind) {
        case PY_S_ASSIGN: {
            uint32_t hi = (uint32_t)s->nt - 1u;
            uint32_t slot;

            if (s->nt < 3u || s->tk[0].kind != PY_TK_IDENT ||
                s->tk[1].kind != PY_TK_OP ||
                s->tk[1].op != PY_OP_ASSIGN) {
                return MENG_ERR_SYNTAX;
            }
            st = py_var_declare(s->tk[0].name, &slot);
            if (st != MENG_OK) return st;
            st = py_cond(words, cap, wc, s->tk, 2u, hi);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_GSTORE, (int64_t)slot, 0);
            if (st != MENG_OK) return st;
            (*idx)++;
            break;
        }

        case PY_S_PRINT: {
            if (s->nt >= 2u && s->tk[1].kind == PY_TK_STR) {
                st = py_emit(words, cap, wc, MENG_OP_PUSHS,
                             (int64_t)s->tk[1].so, (int64_t)s->tk[1].sl);
                if (st != MENG_OK) return st;
            } else {
                st = py_cond(words, cap, wc, s->tk, 1u,
                             (uint32_t)s->nt - 1u);
                if (st != MENG_OK) return st;
            }
            st = py_emit(words, cap, wc, MENG_OP_PRINT, 0, 0);
            if (st != MENG_OK) return st;
            (*idx)++;
            break;
        }

        case PY_S_WHILE: {
            uint32_t top, jnz;

            top = *wc;
            st = py_cond(words, cap, wc, s->tk, 1u, (uint32_t)s->nt - 2u);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_PUSHI, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_CMPEQ, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_JNZ, 0, 0);   /* fwd */
            if (st != MENG_OK) return st;
            jnz = *wc - 1u;
            (*idx)++;
            st = py_compile_block(words, cap, wc, stmts, n, idx,
                                  (int32_t)d, depth + 1u);
            if (st != MENG_OK) return st;
            if (*wc == jnz + 1u) return MENG_ERR_SYNTAX;       /* empty body */
            st = py_emit(words, cap, wc, MENG_OP_JMP, (int64_t)top, 0);
            if (st != MENG_OK) return st;
            words[jnz].a0 = (int64_t)*wc;                      /* patch */
            break;
        }

        case PY_S_IF: {
            uint32_t jnz;

            st = py_cond(words, cap, wc, s->tk, 1u, (uint32_t)s->nt - 2u);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_PUSHI, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_CMPEQ, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_JNZ, 0, 0);   /* -> else */
            if (st != MENG_OK) return st;
            jnz = *wc - 1u;
            (*idx)++;
            st = py_compile_block(words, cap, wc, stmts, n, idx,
                                  (int32_t)d, depth + 1u);
            if (st != MENG_OK) return st;
            if (*idx >= n || stmts[*idx].kind != PY_S_ELSE) {
                words[jnz].a0 = (int64_t)*wc;
                break;
            }
            {
                uint32_t jel = *wc;
                st = py_emit(words, cap, wc, MENG_OP_JMP, 0, 0); /* -> end */
                if (st != MENG_OK) return st;
                words[jnz].a0 = (int64_t)(jel + 1u);
                (*idx)++;
                st = py_compile_block(words, cap, wc, stmts, n, idx,
                                      (int32_t)d, depth + 1u);
                if (st != MENG_OK) return st;
                words[jel].a0 = (int64_t)*wc;
            }
            break;
        }

        case PY_S_ELSE:
            return MENG_ERR_SYNTAX;               /* stray else */

        default:
            return MENG_ERR_SYNTAX;
        }
    }
    return MENG_OK;
}

static meng_status_t engine_py_run(meng_machine_t *m,
                                   const uint8_t *manifest,
                                   size_t manifest_len)
{
    meng_word_t  words[MENG_K_WORDS];
    py_stmt_t    stmts[MENG_PY_MAX_STMTS];
    static int64_t g[MENG_K_GLOBALS];
    uint32_t     wc = 0u;
    uint32_t     ns = 0u;
    uint32_t     idx = 0u;
    uint32_t     i;
    meng_status_t st;

    if (!m || !manifest) return MENG_ERR_BAD_ARG;
    if (manifest_len == 0u) return MENG_OK;        /* empty program */

    py_var_count = 0u;
    meng_pool_len = 0u;
    for (i = 0u; i < MENG_K_GLOBALS; i++) g[i] = 0;

        st = py_collect(manifest, manifest_len, stmts, MENG_PY_MAX_STMTS, &ns);
    if (st != MENG_OK) return st;
    if (ns == 0u) return MENG_OK;

    idx = 0u;
    st = py_compile_block(words, MENG_K_WORDS, &wc, stmts, ns, &idx, -1, 0u);
    if (st != MENG_OK) return st;
    if (idx != ns) return MENG_ERR_SYNTAX;

    return meng_kernel_run(m, words, wc, g, MENG_K_GLOBALS);
}

/* ------------------------------------------------------------------ */
/*  managedjs - JavaScript-like micro subset                            */
/*  Statements: let/var assignment, print(...), while(...){}/           */
/*  if(...){}/}else{ control flow.  Subset shares the poco expression   */
/*  grammar with managedpy (py_tokenize + py_cond for + - * / % and     */
/*  == != < <= > >= with parens and unary minus).  // line comments.    */
/* ------------------------------------------------------------------ */

enum {
    JS_S_ASSIGN = 0,
    JS_S_PRINT,
    JS_S_WHILE,
    JS_S_IF,
    JS_S_ELSE
};

static meng_status_t js_collect(const uint8_t *src, size_t len,
                                py_stmt_t *stmts, uint32_t cap, uint32_t *n)
{
    size_t i = 0u;

    *n = 0u;
    while (i < len) {
        char     linebuf[128];
        uint32_t blen = 0u;
        uint32_t indent = 0u;
        size_t   ls = i;
        size_t   k;
        size_t   p;
        bool     q = false;
        uint32_t kind = 0u;
        py_tk_t  tk[MENG_PY_MAX_TOK];
        uint32_t nt = 0u;
        meng_status_t st;
        char     payload[96];
        uint32_t pl = 0u;
        py_stmt_t *s;

        while (i < len && src[i] != '\n') i++;
        blen = (uint32_t)(i - ls);
        if (blen > 127u) blen = 127u;
        for (k = 0u; k < blen; k++) linebuf[k] = (char)src[ls + k];
        linebuf[blen] = '\0';
        if (i < len) i++;

        /* leading indentation */
        k = 0u;
        while (linebuf[k] == ' ' || linebuf[k] == '\t') { indent++; k++; }

        /* strip // comments (string-aware) */
        for (p = k; linebuf[p] != '\0'; p++) {
            if (linebuf[p] == '"') q = !q;
            else if (linebuf[p] == '/' && linebuf[p + 1u] == '/' && !q) {
                linebuf[p] = '\0';
                break;
            }
        }

        /* find first non-space after indent (skip leading blanks again) */
        while (linebuf[k] == ' ' || linebuf[k] == '\t') k++;
        if (linebuf[k] == '\0') continue;          /* blank / comment-only */

        /* block close "}" alone -> structural no-op */
        if (linebuf[k] == '}' && linebuf[k + 1u] == '\0') continue;
        /* "} else {"/"else {" -> JS_S_ELSE */
        {
            size_t m = k;
            while (linebuf[m] == ' ' || linebuf[m] == '}' ||
                   linebuf[m] == '{') m++;
            if (linebuf[m] == 'e' && linebuf[m + 1u] == 'l' &&
                linebuf[m + 2u] == 's' && linebuf[m + 3u] == 'e') {
                if (*n >= cap) return MENG_ERR_SHORT_OUT;
                s = &stmts[*n];
                s->indent = (uint16_t)indent;
                s->kind = JS_S_ELSE;
                s->nt = 0u;
                (*n)++;
                continue;
            }
        }

        /* classify + extract payload */
        {
            size_t m = k;
            size_t end = strlen32(linebuf);
            while (end > m && (linebuf[end - 1u] == ' ' ||
                               linebuf[end - 1u] == '\t' ||
                               linebuf[end - 1u] == ';' ||
                               linebuf[end - 1u] == '{' ||
                               linebuf[end - 1u] == '}')) {
                end--;
            }
            if ((linebuf[m] == 'l' && linebuf[m + 1u] == 'e' &&
                 linebuf[m + 2u] == 't' &&
                 (linebuf[m + 3u] == ' ' || linebuf[m + 3u] == '\t')) ||
                (linebuf[m] == 'v' && linebuf[m + 1u] == 'a' &&
                 linebuf[m + 2u] == 'r' &&
                 (linebuf[m + 3u] == ' ' || linebuf[m + 3u] == '\t'))) {
                kind = JS_S_ASSIGN;
                m += 3u;
                while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
                while (((linebuf[m] >= 'a' && linebuf[m] <= 'z') ||
                        (linebuf[m] >= 'A' && linebuf[m] <= 'Z') ||
                        linebuf[m] == '_') ||
                       (linebuf[m] >= '0' && linebuf[m] <= '9')) {
                    if (pl + 1u < 32u) payload[pl] = linebuf[m];
                    pl++; m++;
                }
                payload[pl] = '\0';
                while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
                if (linebuf[m] != '=') return MENG_ERR_SYNTAX;
                m++;
                while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
            } else if (linebuf[m] == 'p' && linebuf[m + 1u] == 'r' &&
                       linebuf[m + 2u] == 'i' && linebuf[m + 3u] == 'n' &&
                       linebuf[m + 4u] == 't') {
                kind = JS_S_PRINT;
                m += 5u;
                while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
                if (linebuf[m] != '(') return MENG_ERR_SYNTAX;
                m++;
                if (end > m && linebuf[end - 1u] == ')') end--;
                /* payload = expr between parens, m..end */
            } else if (linebuf[m] == 'w' && linebuf[m + 1u] == 'h' &&
                       linebuf[m + 2u] == 'i' && linebuf[m + 3u] == 'l' &&
                       linebuf[m + 4u] == 'e') {
                kind = JS_S_WHILE;
                m += 5u;
                while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
                if (linebuf[m] != '(') return MENG_ERR_SYNTAX;
                m++;
                if (end > m && linebuf[end - 1u] == ')') end--;
            } else if (linebuf[m] == 'i' && linebuf[m + 1u] == 'f') {
                kind = JS_S_IF;
                m += 2u;
                while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
                if (linebuf[m] != '(') return MENG_ERR_SYNTAX;
                m++;
                if (end > m && linebuf[end - 1u] == ')') end--;
            } else {
                /* plain assignment:  NAME = expr ; */
                size_t p2 = m;
                if (((linebuf[m] >= 'a' && linebuf[m] <= 'z') ||
                     (linebuf[m] >= 'A' && linebuf[m] <= 'Z') ||
                     linebuf[m] == '_')) {
                    p2++;
                    while (((linebuf[p2] >= 'a' && linebuf[p2] <= 'z') ||
                            (linebuf[p2] >= 'A' && linebuf[p2] <= 'Z') ||
                            linebuf[p2] == '_') ||
                           (linebuf[p2] >= '0' && linebuf[p2] <= '9')) p2++;
                    while (linebuf[p2] == ' ' || linebuf[p2] == '\t') p2++;
                    if (linebuf[p2] == '=') {
                        kind = JS_S_ASSIGN;
                        while (m < p2 && linebuf[m] != ' ' && linebuf[m] != '\t') {
                            if (pl + 1u < 32u) payload[pl] = linebuf[m];
                            pl++; m++;
                        }
                        payload[pl] = '\0';
                        while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
                        m++;                       /* skip '=' */
                        while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
                    } else {
                        continue;                  /* unrecognised -> skip */
                    }
                } else {
                    continue;                      /* unrecognised -> skip */
                }
            }

            /* copy payload m..end into a fresh token buffer via py_tokenize */
            {
                char  buf[96];
                uint32_t b = 0u;
                size_t z;
                for (z = m; z < end && b + 1u < 96u; z++) buf[b++] = linebuf[z];
                buf[b] = '\0';
                st = py_tokenize(buf, tk, MENG_PY_MAX_TOK, &nt);
                if (st != MENG_OK) return st;
            }
        }

        if (*n >= cap) return MENG_ERR_SHORT_OUT;
        s = &stmts[*n];
        s->indent = (uint16_t)indent;
        s->kind = (uint8_t)kind;
        s->nt = (uint8_t)nt;

        if (kind == JS_S_ASSIGN) {
            /* tk layout: [name ident, expr...]; rebuild with name first */
            py_tk_t tmp[MENG_PY_MAX_TOK];
            uint32_t z;
            if (nt + 1u > MENG_PY_MAX_TOK) return MENG_ERR_SYNTAX;
            tmp[0].kind = PY_TK_IDENT;
            for (z = 0u; payload[z] != '\0' && z < 15u; z++) tmp[0].name[z] = payload[z];
            tmp[0].name[z] = '\0';
            for (z = 0u; z < nt; z++) tmp[z + 1u] = tk[z];
            for (z = 0u; z <= nt; z++) s->tk[z] = tmp[z];
            s->nt = (uint8_t)(nt + 1u);
        } else {
            uint32_t z;
            for (z = 0u; z < nt; z++) s->tk[z] = tk[z];
        }
        (*n)++;
    }
    return MENG_OK;
}

static meng_status_t js_compile_block(meng_word_t *words, uint32_t cap,
                                      uint32_t *wc, const py_stmt_t *stmts,
                                      uint32_t n, uint32_t *idx,
                                      int32_t parent_indent, uint32_t depth)
{
    meng_status_t st;

    if (depth > MENG_PY_MAX_DEPTH) return MENG_ERR_SYNTAX;

    while (*idx < n && (int32_t)stmts[*idx].indent > parent_indent) {
        const py_stmt_t *s = &stmts[*idx];
        uint32_t d = (uint32_t)s->indent;

        switch (s->kind) {
        case JS_S_ASSIGN: {
            uint32_t hi = (uint32_t)s->nt - 1u;
            uint32_t slot;

            if (s->nt < 2u || s->tk[0].kind != PY_TK_IDENT) {
                return MENG_ERR_SYNTAX;
            }
            st = py_var_declare(s->tk[0].name, &slot);
            if (st != MENG_OK) return st;
            if (hi >= 1u) {
                st = py_cond(words, cap, wc, s->tk, 1u, hi);
                if (st != MENG_OK) return st;
            } else {
                st = py_emit(words, cap, wc, MENG_OP_PUSHI, 0, 0);
                if (st != MENG_OK) return st;
            }
            st = py_emit(words, cap, wc, MENG_OP_GSTORE, (int64_t)slot, 0);
            if (st != MENG_OK) return st;
            (*idx)++;
            break;
        }

        case JS_S_PRINT: {
            if (s->nt >= 1u && s->tk[0].kind == PY_TK_STR) {
                st = py_emit(words, cap, wc, MENG_OP_PUSHS,
                             (int64_t)s->tk[0].so, (int64_t)s->tk[0].sl);
                if (st != MENG_OK) return st;
            } else {
                st = py_cond(words, cap, wc, s->tk, 0u,
                             (uint32_t)s->nt - 1u);
                if (st != MENG_OK) return st;
            }
            st = py_emit(words, cap, wc, MENG_OP_PRINT, 0, 0);
            if (st != MENG_OK) return st;
            (*idx)++;
            break;
        }

        case JS_S_WHILE: {
            uint32_t top, jnz;

            top = *wc;
            st = py_cond(words, cap, wc, s->tk, 0u, (uint32_t)s->nt - 1u);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_PUSHI, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_CMPEQ, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_JNZ, 0, 0);   /* fwd */
            if (st != MENG_OK) return st;
            jnz = *wc - 1u;
            (*idx)++;
            st = js_compile_block(words, cap, wc, stmts, n, idx,
                                  (int32_t)d, depth + 1u);
            if (st != MENG_OK) return st;
            if (*wc == jnz + 1u) return MENG_ERR_SYNTAX;       /* empty body */
            st = py_emit(words, cap, wc, MENG_OP_JMP, (int64_t)top, 0);
            if (st != MENG_OK) return st;
            words[jnz].a0 = (int64_t)*wc;
            break;
        }

        case JS_S_IF: {
            uint32_t jnz;

            st = py_cond(words, cap, wc, s->tk, 0u, (uint32_t)s->nt - 1u);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_PUSHI, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_CMPEQ, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_JNZ, 0, 0);
            if (st != MENG_OK) return st;
            jnz = *wc - 1u;
            (*idx)++;
            st = js_compile_block(words, cap, wc, stmts, n, idx,
                                  (int32_t)d, depth + 1u);
            if (st != MENG_OK) return st;
            if (*idx >= n || stmts[*idx].kind != JS_S_ELSE ||
                stmts[*idx].indent != s->indent) {
                words[jnz].a0 = (int64_t)*wc;
                break;
            }
            {
                uint32_t jel = *wc;
                st = py_emit(words, cap, wc, MENG_OP_JMP, 0, 0);
                if (st != MENG_OK) return st;
                words[jnz].a0 = (int64_t)(jel + 1u);
                (*idx)++;
                st = js_compile_block(words, cap, wc, stmts, n, idx,
                                      (int32_t)d, depth + 1u);
                if (st != MENG_OK) return st;
                words[jel].a0 = (int64_t)*wc;
            }
            break;
        }

        case JS_S_ELSE:
            return MENG_ERR_SYNTAX;               /* stray else */

        default:
            return MENG_ERR_SYNTAX;
        }
    }
    return MENG_OK;
}

static meng_status_t engine_js_run(meng_machine_t *m,
                                   const uint8_t *manifest,
                                   size_t manifest_len)
{
    meng_word_t  words[MENG_K_WORDS];
    py_stmt_t    stmts[MENG_PY_MAX_STMTS];
    static int64_t g[MENG_K_GLOBALS];
    uint32_t     wc = 0u;
    uint32_t     ns = 0u;
    uint32_t     idx = 0u;
    uint32_t     i;
    meng_status_t st;

    if (!m || !manifest) return MENG_ERR_BAD_ARG;
    if (manifest_len == 0u) return MENG_OK;

    py_var_count = 0u;
    meng_pool_len = 0u;
    for (i = 0u; i < MENG_K_GLOBALS; i++) g[i] = 0;

    st = js_collect(manifest, manifest_len, stmts, MENG_PY_MAX_STMTS, &ns);
    if (st != MENG_OK) return st;
    if (ns == 0u) return MENG_OK;

    idx = 0u;
    st = js_compile_block(words, MENG_K_WORDS, &wc, stmts, ns, &idx, -1, 0u);
    if (st != MENG_OK) return st;
    if (idx != ns) return MENG_ERR_SYNTAX;

    return meng_kernel_run(m, words, wc, g, MENG_K_GLOBALS);
}

/* ------------------------------------------------------------------ */
/*  managedlua - Lua-like micro subset                                  */
/*                                                                      */
/*  print(...), NAME = expr, while cond do ... end, if cond then ...   */
/*  else ... end.  -- line comments.  Shared poco expression grammar.   */
/* ------------------------------------------------------------------ */

enum {
    LUA_S_ASSIGN = 0,
    LUA_S_PRINT,
    LUA_S_WHILE,
    LUA_S_IF,
    LUA_S_ELSE
};

static bool lua_word(const char *s, size_t p, const char *w)
{
    size_t z = 0u;

    while (w[z] != '\0') {
        if (s[p + z] != w[z]) return false;
        z++;
    }
    /* require a non-identifier terminator */
    {
        char c = s[p + z];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
            (c >= '0' && c <= '9')) {
            return false;
        }
    }
    return true;
}

static meng_status_t lua_collect(const uint8_t *src, size_t len,
                                 py_stmt_t *stmts, uint32_t cap, uint32_t *n)
{
    size_t i = 0u;

    *n = 0u;
    while (i < len) {
        char     linebuf[128];
        uint32_t blen = 0u;
        uint32_t indent = 0u;
        size_t   ls = i;
        size_t   k;
        size_t   p;
        size_t   q;
        bool     in = false;                 /* inside string literal   */
        uint32_t kind = 0u;
        py_tk_t  tk[MENG_PY_MAX_TOK];
        uint32_t nt = 0u;
        meng_status_t st;
        char     name[16];
        uint32_t nl = 0u;
        size_t   m, e;
        py_stmt_t *s;

        while (i < len && src[i] != '\n') i++;
        blen = (uint32_t)(i - ls);
        if (blen > 127u) blen = 127u;
        for (k = 0u; k < blen; k++) linebuf[k] = (char)src[ls + k];
        linebuf[blen] = '\0';
        if (i < len) i++;

        k = 0u;
        while (linebuf[k] == ' ' || linebuf[k] == '\t') { indent++; k++; }

        /* strip -- comments (string-aware) */
        for (p = k; linebuf[p] != '\0'; p++) {
            if (linebuf[p] == '"') in = !in;
            else if (linebuf[p] == '-' && linebuf[p + 1u] == '-' && !in) {
                linebuf[p] = '\0';
                break;
            }
        }
        while (k > 0u && (linebuf[k] == ' ' || linebuf[k] == '\t')) k++;
        while (linebuf[k] == ' ' || linebuf[k] == '\t') k++;
        if (linebuf[k] == '\0') continue;      /* blank / comment-only */

        /* structural terminator "end" -> close block (dedent handles it) */
        if (lua_word(linebuf, k, "end") &&
            (linebuf[k + 3u] == '\0' || linebuf[k + 3u] == ' ' ||
             linebuf[k + 3u] == '\t')) {
            continue;
        }

        /* trailing trim of spaces */
        e = strlen32(linebuf);
        while (e > k && (linebuf[e - 1u] == ' ' || linebuf[e - 1u] == '\t' ||
                         linebuf[e - 1u] == ';')) e--;

        /* keyword dispatch */
        if (lua_word(linebuf, k, "print")) {
            kind = LUA_S_PRINT;
            m = k + 5u;
            while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
            if (linebuf[m] != '(') return MENG_ERR_SYNTAX;
            m++;
            if (e > m && linebuf[e - 1u] == ')') e--;
        } else if (lua_word(linebuf, k, "while")) {
            kind = LUA_S_WHILE;
            m = k + 5u;
            while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
            /* find trailing "do" */
            q = m;
            while (q + 2u <= e) {
                if (lua_word(linebuf, q, "do")) { e = q; break; }
                q++;
            }
        } else if (lua_word(linebuf, k, "if")) {
            kind = LUA_S_IF;
            m = k + 2u;
            while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
            q = m;
            while (q + 4u <= e) {
                if (lua_word(linebuf, q, "then")) { e = q; break; }
                q++;
            }
        } else if (lua_word(linebuf, k, "else")) {
            if (*n >= cap) return MENG_ERR_SHORT_OUT;
            s = &stmts[*n];
            s->indent = (uint16_t)indent;
            s->kind = LUA_S_ELSE;
            s->nt = 0u;
            (*n)++;
            continue;
        } else {
            /* plain assignment: NAME = expr */
            kind = LUA_S_ASSIGN;
            m = k;
            while (((linebuf[m] >= 'a' && linebuf[m] <= 'z') ||
                    (linebuf[m] >= 'A' && linebuf[m] <= 'Z') ||
                    linebuf[m] == '_') ||
                   (linebuf[m] >= '0' && linebuf[m] <= '9')) {
                if (nl + 1u < 16u) name[nl] = linebuf[m];
                nl++; m++;
            }
            while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
            if (linebuf[m] != '=') return MENG_ERR_SYNTAX;
            m++;
            while (linebuf[m] == ' ' || linebuf[m] == '\t') m++;
        }

        /* copy payload m..e into token buffer */
        {
            char  buf[96];
            uint32_t b = 0u;
            size_t z;
            for (z = m; z < e && b + 1u < 96u; z++) buf[b++] = linebuf[z];
            buf[b] = '\0';
            st = py_tokenize(buf, tk, MENG_PY_MAX_TOK, &nt);
            if (st != MENG_OK) return st;
        }

        if (*n >= cap) return MENG_ERR_SHORT_OUT;
        s = &stmts[*n];
        s->indent = (uint16_t)indent;
        s->kind = (uint8_t)kind;

        if (kind == LUA_S_ASSIGN) {
            py_tk_t tmp[MENG_PY_MAX_TOK];
            uint32_t z;
            if (nt + 1u > MENG_PY_MAX_TOK) return MENG_ERR_SYNTAX;
            tmp[0].kind = PY_TK_IDENT;
            for (z = 0u; z < nl; z++) tmp[0].name[z] = (z < 15u) ? name[z] : '\0';
            tmp[0].name[15u] = '\0';
            for (z = 0u; z < nt; z++) tmp[z + 1u] = tk[z];
            for (z = 0u; z <= nt; z++) s->tk[z] = tmp[z];
            s->nt = (uint8_t)(nt + 1u);
        } else {
            uint32_t z;
            for (z = 0u; z < nt; z++) s->tk[z] = tk[z];
            s->nt = (uint8_t)nt;
        }
        (*n)++;
    }
    return MENG_OK;
}

static meng_status_t lua_compile_block(meng_word_t *words, uint32_t cap,
                                       uint32_t *wc, const py_stmt_t *stmts,
                                       uint32_t n, uint32_t *idx,
                                       int32_t parent_indent, uint32_t depth)
{
    meng_status_t st;

    if (depth > MENG_PY_MAX_DEPTH) return MENG_ERR_SYNTAX;

    while (*idx < n && (int32_t)stmts[*idx].indent > parent_indent) {
        const py_stmt_t *s = &stmts[*idx];
        uint32_t d = (uint32_t)s->indent;

        switch (s->kind) {
        case LUA_S_ASSIGN: {
            uint32_t hi = (uint32_t)s->nt - 1u;
            uint32_t slot;

            if (s->nt < 2u || s->tk[0].kind != PY_TK_IDENT) {
                return MENG_ERR_SYNTAX;
            }
            st = py_var_declare(s->tk[0].name, &slot);
            if (st != MENG_OK) return st;
            if (hi >= 1u) {
                st = py_cond(words, cap, wc, s->tk, 1u, hi);
                if (st != MENG_OK) return st;
            } else {
                st = py_emit(words, cap, wc, MENG_OP_PUSHI, 0, 0);
                if (st != MENG_OK) return st;
            }
            st = py_emit(words, cap, wc, MENG_OP_GSTORE, (int64_t)slot, 0);
            if (st != MENG_OK) return st;
            (*idx)++;
            break;
        }

        case LUA_S_PRINT: {
            if (s->nt >= 1u && s->tk[0].kind == PY_TK_STR) {
                st = py_emit(words, cap, wc, MENG_OP_PUSHS,
                             (int64_t)s->tk[0].so, (int64_t)s->tk[0].sl);
                if (st != MENG_OK) return st;
            } else {
                st = py_cond(words, cap, wc, s->tk, 0u,
                             (uint32_t)s->nt - 1u);
                if (st != MENG_OK) return st;
            }
            st = py_emit(words, cap, wc, MENG_OP_PRINT, 0, 0);
            if (st != MENG_OK) return st;
            (*idx)++;
            break;
        }

        case LUA_S_WHILE: {
            uint32_t top, jnz;

            top = *wc;
            st = py_cond(words, cap, wc, s->tk, 0u, (uint32_t)s->nt - 1u);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_PUSHI, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_CMPEQ, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_JNZ, 0, 0);
            if (st != MENG_OK) return st;
            jnz = *wc - 1u;
            (*idx)++;
            st = lua_compile_block(words, cap, wc, stmts, n, idx,
                                   (int32_t)d, depth + 1u);
            if (st != MENG_OK) return st;
            if (*wc == jnz + 1u) return MENG_ERR_SYNTAX;
            st = py_emit(words, cap, wc, MENG_OP_JMP, (int64_t)top, 0);
            if (st != MENG_OK) return st;
            words[jnz].a0 = (int64_t)*wc;
            break;
        }

        case LUA_S_IF: {
            uint32_t jnz;

            st = py_cond(words, cap, wc, s->tk, 0u, (uint32_t)s->nt - 1u);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_PUSHI, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_CMPEQ, 0, 0);
            if (st != MENG_OK) return st;
            st = py_emit(words, cap, wc, MENG_OP_JNZ, 0, 0);
            if (st != MENG_OK) return st;
            jnz = *wc - 1u;
            (*idx)++;
            st = lua_compile_block(words, cap, wc, stmts, n, idx,
                                   (int32_t)d, depth + 1u);
            if (st != MENG_OK) return st;
            if (*idx >= n || stmts[*idx].kind != LUA_S_ELSE ||
                stmts[*idx].indent != s->indent) {
                words[jnz].a0 = (int64_t)*wc;
                break;
            }
            {
                uint32_t jel = *wc;
                st = py_emit(words, cap, wc, MENG_OP_JMP, 0, 0);
                if (st != MENG_OK) return st;
                words[jnz].a0 = (int64_t)(jel + 1u);
                (*idx)++;
                st = lua_compile_block(words, cap, wc, stmts, n, idx,
                                       (int32_t)d, depth + 1u);
                if (st != MENG_OK) return st;
                words[jel].a0 = (int64_t)*wc;
            }
            break;
        }

        case LUA_S_ELSE:
            return MENG_ERR_SYNTAX;

        default:
            return MENG_ERR_SYNTAX;
        }
    }
    return MENG_OK;
}

static meng_status_t engine_lua_run(meng_machine_t *m,
                                    const uint8_t *manifest,
                                    size_t manifest_len)
{
    meng_word_t  words[MENG_K_WORDS];
    py_stmt_t    stmts[MENG_PY_MAX_STMTS];
    static int64_t g[MENG_K_GLOBALS];
    uint32_t     wc = 0u;
    uint32_t     ns = 0u;
    uint32_t     idx = 0u;
    uint32_t     i;
    meng_status_t st;

    if (!m || !manifest) return MENG_ERR_BAD_ARG;
    if (manifest_len == 0u) return MENG_OK;

    py_var_count = 0u;
    meng_pool_len = 0u;
    for (i = 0u; i < MENG_K_GLOBALS; i++) g[i] = 0;

    st = lua_collect(manifest, manifest_len, stmts, MENG_PY_MAX_STMTS, &ns);
    if (st != MENG_OK) return st;
    if (ns == 0u) return MENG_OK;

    idx = 0u;
    st = lua_compile_block(words, MENG_K_WORDS, &wc, stmts, ns, &idx, -1, 0u);
    if (st != MENG_OK) return st;
    if (idx != ns) return MENG_ERR_SYNTAX;

    return meng_kernel_run(m, words, wc, g, MENG_K_GLOBALS);
}

static meng_engine_t meng_registry[MENG_MAX_ENGINES];
static uint32_t      meng_registry_count = 0u;

static meng_engine_t meng_engine_poco = {
    MENG_KEY_POCO, "poco", 1, 0, engine_bytecode_run
};
static meng_engine_t meng_engine_net = {
    MENG_KEY_MANAGEDNET, "managednet", 1, 0, engine_bytecode_run
};
static meng_engine_t meng_engine_dl = {
    MENG_KEY_MANAGEDDL, "manageddl", 1, 0, engine_dl_run
};
static meng_engine_t meng_engine_com = {
    MENG_KEY_MANAGEDCOM, "managedcom", 1, 0, engine_com_run
};
static meng_engine_t meng_engine_sh = {
    MENG_KEY_MANAGEDSH, "managedsh", 1, 0, engine_sh_run
};
static meng_engine_t meng_engine_py = {
    MENG_KEY_MANAGEDPY, "managedpy", 1, 0, engine_py_run
};
static meng_engine_t meng_engine_js = {
    MENG_KEY_MANAGEDJS, "managedjs", 1, 0, engine_js_run
};
static meng_engine_t meng_engine_lua = {
    MENG_KEY_MANAGEDLUA, "managedlua", 1, 0, engine_lua_run
};

meng_status_t meng_engine_register(meng_engine_t *engine)
{
    uint32_t i;

    if (!engine || engine->key == 0u || engine->name == NULL ||
        engine->run == NULL) {
        return MENG_ERR_BAD_ARG;
    }
    for (i = 0u; i < meng_registry_count; i++) {
        if (meng_registry[i].key == engine->key) {
            return MENG_ERR_DUPLICATE;
        }
    }
    if (meng_registry_count >= MENG_MAX_ENGINES) return MENG_ERR_SHORT_OUT;
    meng_registry[meng_registry_count] = *engine;
    meng_registry[meng_registry_count].generation = meng_generation;
    meng_registry_count++;
    return MENG_OK;
}

meng_status_t meng_engine_resolve(uint32_t key, meng_engine_t **out)
{
    uint32_t i;

    if (!out) return MENG_ERR_BAD_ARG;
    for (i = 0u; i < meng_registry_count; i++) {
        if (meng_registry[i].key == key) {
            *out = &meng_registry[i];
            return MENG_OK;
        }
    }
    return MENG_ERR_DEPENDENCY;
}

static meng_status_t meng_engine_dispatch_m(meng_machine_t *m, uint32_t key,
                                            const uint8_t *manifest,
                                            size_t manifest_len)
{
    meng_engine_t *e;

    if (!m || !manifest) return MENG_ERR_BAD_ARG;
    if (!meng_initialized) return MENG_ERR_UNINITIALIZED;
    if (meng_engine_resolve(key, &e) != MENG_OK) {
        return MENG_ERR_DEPENDENCY;
    }
    if (!e->run) return MENG_ERR_STATE;
    m->generation = e->generation;
    return e->run(m, manifest, manifest_len);
}

meng_status_t meng_engine_dispatch(meng_machine_t *m, uint32_t key,
                                   const uint8_t *manifest,
                                   size_t manifest_len)
{
    return meng_engine_dispatch_m(m, key, manifest, manifest_len);
}

uint32_t meng_engine_count(void)
{
    return meng_registry_count;
}

meng_status_t meng_dl_register_engine(meng_engine_t *engine)
{
    return meng_engine_register(engine);
}

meng_status_t meng_dl_load(meng_machine_t *m, uint32_t key,
                           const uint8_t *manifest, size_t manifest_len)
{
    return meng_engine_dispatch_m(m, key, manifest, manifest_len);
}

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

meng_status_t meng_module_init(void)
{
    meng_status_t st;

    if (meng_initialized) return MENG_OK;

    meng_generation++;
    meng_registry_count = 0u;

    st = meng_engine_register(&meng_engine_poco);
    if (st != MENG_OK) return st;
    st = meng_engine_register(&meng_engine_net);
    if (st != MENG_OK) return st;
    st = meng_engine_register(&meng_engine_dl);
    if (st != MENG_OK) return st;
    st = meng_engine_register(&meng_engine_com);
    if (st != MENG_OK) return st;
    st = meng_engine_register(&meng_engine_sh);
    if (st != MENG_OK) return st;
    st = meng_engine_register(&meng_engine_py);
    if (st != MENG_OK) return st;
    st = meng_engine_register(&meng_engine_js);
    if (st != MENG_OK) return st;
    st = meng_engine_register(&meng_engine_lua);
    if (st != MENG_OK) return st;

    meng_initialized = true;
    return MENG_OK;
}

meng_status_t meng_module_shutdown(void)
{
    if (!meng_initialized) return MENG_ERR_UNINITIALIZED;
    meng_initialized = false;
    meng_registry_count = 0u;
    return MENG_OK;
}

uint32_t meng_abi_version(void)
{
    return 0x00010000u;
}

uint16_t meng_abi_major(void)
{
    return 1u;
}

uint16_t meng_abi_minor(void)
{
    return 0u;
}

const uint8_t *meng_ident(void)
{
    return meng_ident_str;
}