/*
 * owdebug64.h - OpenWindows Kernel Debug Support Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Debug output is routed to a pluggable sink. When no sink is installed
 * the module keeps a bounded in-memory trace buffer instead of dropping
 * the text, so early boot diagnostics remain readable.
 */

#ifndef OWDEBUG64_H
#define OWDEBUG64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Configuration ────────────────────────────────────────────────── */

#define OWDBG_BUFFER_SIZE   1024u   /* fallback trace buffer bytes     */
#define OWDBG_MAX_FRAMES    64u     /* stack_trace() reporting limit    */

/* ── Public types ─────────────────────────────────────────────────── */

/* Consumer of debug text. May be NULL to use the fallback buffer. */
typedef void (*owdbg_sink_fn)(const char *text, size_t length, void *context);

/* Register snapshot captured by an exception/ISR entry trampoline. */
typedef struct {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rsp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t rflags;
} owdbg_regs_t;

/* ── API ──────────────────────────────────────────────────────────── */

/* Reset internal state; leaves the sink unset (fallback buffer active). */
bool owdbg_init(void);

/* Execute an INT3 software breakpoint. */
void owdbg_breakpoint(void);

/* Emit raw text through the sink (or into the fallback buffer). */
bool owdbg_print(const char *text, size_t length);

/* Emit every register in `regs` as one formatted line each. */
void owdbg_dump_regs(const owdbg_regs_t *regs);

/* Emit one line per return address; returns the number reported. */
uint32_t owdbg_stack_trace(const uint64_t *frames, uint32_t count);

/* Install (or clear with NULL) the debug sink. */
void owdbg_set_sink(owdbg_sink_fn sink, void *context);

/* Copy the fallback buffer into `dst` (NUL-terminated); returns bytes. */
uint32_t owdbg_read(char *dst, uint32_t capacity);

/* Discard the fallback buffer contents. */
void owdbg_clear(void);

/* Current fallback buffer length in bytes. */
uint32_t owdbg_length(void);

#endif /* OWDEBUG64_H */
