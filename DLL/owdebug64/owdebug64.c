/*
 * owdebug64.c - OpenWindows Kernel Debug Support Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owdebug64.h"

/* ── Internal state ───────────────────────────────────────────── */

static char        owdebug64_buffer[OWDBG_BUFFER_SIZE];
static uint32_t    owdebug64_length = 0u;
static owdbg_sink_fn owdebug64_sink = NULL;
static void       *owdebug64_sink_ctx = NULL;
static bool        owdebug64_ready = false;

/* ── Helpers ──────────────────────────────────────────────────── */

static void ow_memcpy(void *dst, const void *src, size_t len)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;

    for (i = 0u; i < len; ++i) {
        d[i] = s[i];
    }
}

/* Shift `len` bytes down by one; used to evict the oldest trace bytes. */
static void ow_memmove_left(char *dst, const char *src, size_t len)
{
    size_t i;

    for (i = 0u; i < len; ++i) {
        dst[i] = src[i];
    }
}

/* Append text to the fallback buffer, evicting the oldest bytes if full. */
static void owdebug64_store(const char *text, size_t length)
{
    size_t room = (size_t)(OWDBG_BUFFER_SIZE - 1u);

    if (length >= room) {
        /* Keep only the tail of an oversized record. */
        text += (length - room);
        length = room;
        owdebug64_length = 0u;
    }

    if ((size_t)owdebug64_length + length > room) {
        size_t drop = ((size_t)owdebug64_length + length) - room;
        ow_memmove_left(&owdebug64_buffer[0], &owdebug64_buffer[drop],
                        (size_t)owdebug64_length - drop);
        owdebug64_length -= (uint32_t)drop;
    }

    ow_memcpy(&owdebug64_buffer[owdebug64_length], text, length);
    owdebug64_length += (uint32_t)length;
    owdebug64_buffer[owdebug64_length] = '\0';
}

/* Emit raw bytes to the installed sink, or to the fallback buffer. */
static void owdebug64_emit(const char *text, size_t length)
{
    if (owdebug64_sink != NULL) {
        owdebug64_sink(text, length, owdebug64_sink_ctx);
    } else {
        owdebug64_store(text, length);
    }
}

static void owdebug64_put_cstr(const char *text)
{
    size_t len = 0u;

    while (text[len] != '\0') {
        len++;
    }
    owdebug64_emit(text, len);
}

/* Emit `value` as 0x-prefixed, zero-padded 16-digit hex. */
static void owdebug64_put_hex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    char out[18];
    int i;

    out[0] = '0';
    out[1] = 'x';
    for (i = 0; i < 16; ++i) {
        out[2 + i] = digits[(value >> (60 - (i * 4))) & 0xFu];
    }
    owdebug64_emit(out, sizeof(out));
}

/* Emit `value` as unsigned decimal. */
static void owdebug64_put_dec(uint32_t value)
{
    char tmp[10];
    uint32_t n = 0u;
    uint32_t v = value;

    if (v == 0u) {
        owdebug64_emit("0", 1u);
        return;
    }
    while (v != 0u) {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0u) {
        n--;
        owdebug64_emit(&tmp[n], 1u);
    }
}

/* Emit "name=" + hex value + newline. */
static void owdebug64_emit_reg(const char *name, uint64_t value)
{
    owdebug64_put_cstr(name);
    owdebug64_emit("=", 1u);
    owdebug64_put_hex(value);
    owdebug64_emit("\n", 1u);
}

/* ── Implementation ───────────────────────────────────────────── */

bool owdbg_init(void)
{
    owdebug64_length = 0u;
    owdebug64_buffer[0] = '\0';
    owdebug64_sink = NULL;
    owdebug64_sink_ctx = NULL;
    owdebug64_ready = true;
    return true;
}

void owdbg_breakpoint(void)
{
    /* Trap into the debugger; a no-op when none is attached. */
    __asm__ __volatile__("int3");
}

bool owdbg_print(const char *text, size_t length)
{
    if (!owdebug64_ready || text == NULL) {
        return false;
    }
    owdebug64_emit(text, length);
    return true;
}

void owdbg_dump_regs(const owdbg_regs_t *regs)
{
    if (!owdebug64_ready || regs == NULL) {
        return;
    }

    owdebug64_emit_reg("rax   ", regs->rax);
    owdebug64_emit_reg("rbx   ", regs->rbx);
    owdebug64_emit_reg("rcx   ", regs->rcx);
    owdebug64_emit_reg("rdx   ", regs->rdx);
    owdebug64_emit_reg("rsi   ", regs->rsi);
    owdebug64_emit_reg("rdi   ", regs->rdi);
    owdebug64_emit_reg("rbp   ", regs->rbp);
    owdebug64_emit_reg("rsp   ", regs->rsp);
    owdebug64_emit_reg("r8    ", regs->r8);
    owdebug64_emit_reg("r9    ", regs->r9);
    owdebug64_emit_reg("r10   ", regs->r10);
    owdebug64_emit_reg("r11   ", regs->r11);
    owdebug64_emit_reg("r12   ", regs->r12);
    owdebug64_emit_reg("r13   ", regs->r13);
    owdebug64_emit_reg("r14   ", regs->r14);
    owdebug64_emit_reg("r15   ", regs->r15);
    owdebug64_emit_reg("rip   ", regs->rip);
    owdebug64_emit_reg("rflags", regs->rflags);
}

uint32_t owdbg_stack_trace(const uint64_t *frames, uint32_t count)
{
    uint32_t i;
    uint32_t limit = count;

    if (!owdebug64_ready || frames == NULL) {
        return 0u;
    }
    if (limit > OWDBG_MAX_FRAMES) {
        limit = OWDBG_MAX_FRAMES;
    }

    for (i = 0u; i < limit; ++i) {
        owdebug64_put_cstr("  #");
        owdebug64_put_dec(i);
        owdebug64_put_cstr("  ");
        owdebug64_put_hex(frames[i]);
        owdebug64_emit("\n", 1u);
    }
    return limit;
}

void owdbg_set_sink(owdbg_sink_fn sink, void *context)
{
    owdebug64_sink = sink;
    owdebug64_sink_ctx = context;
}

uint32_t owdbg_read(char *dst, uint32_t capacity)
{
    uint32_t i;

    if (dst == NULL || capacity == 0u) {
        return 0u;
    }

    for (i = 0u; i < (capacity - 1u) && i < owdebug64_length; ++i) {
        dst[i] = owdebug64_buffer[i];
    }
    dst[i] = '\0';
    return i;
}

void owdbg_clear(void)
{
    owdebug64_length = 0u;
    owdebug64_buffer[0] = '\0';
}

uint32_t owdbg_length(void)
{
    return owdebug64_length;
}
