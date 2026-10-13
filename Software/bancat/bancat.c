/*
 * bancat.c - OpenWindows BANcode Crash Dump Decoder (.owx)
 *
 * Reads a .bdmp crash dump off the primary OWFS volume through the Ring 3
 * gateway, validates it against the format contract in Extensions/bdmp_core.h,
 * and prints the fault register frame, the failing BANcode, and the recorded
 * trap history.
 *
 * The decoder is the exact validation the producer is held to: bdmp_validate()
 * is the same code the kernel's fixture seeder builds with, so a dump this
 * program accepts is one the format definition calls well-formed.  Nothing is
 * decoded until the whole file has passed (magic, version, every length against
 * the buffer, header checksum, and the whole-dump checksum), so a truncated or
 * corrupted dump is reported as such rather than half-printed.
 *
 * It runs as a privileged (BOOT-subsystem) process, so the OWFS permission
 * model admits it to the hidden, root-owned dump the crash path would write.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../Extensions/bdmp_format.h"
#include "../../Extensions/bdmp_core.h"
#include "../owx_gate.h"

/* The name the crash path (and the boot-time fixture) writes.  Kept in step
 * with OW_CRASH_DUMP_NAME in the kernel's storage/owdisk.h by the qemu-bancat
 * regression, which seeds the file under this name and boots this program. */
#define BANCAT_DEFAULT_DUMP "crash.bdmp"

/* One dump.  A fixed buffer, like every other staging pool in the ecosystem:
 * the size is a hard ceiling on a single read, and a larger file is reported as
 * too large rather than truncated into something that looks valid. */
#define BANCAT_MAX_DUMP 4096U
static uint8_t s_dump[BANCAT_MAX_DUMP];

/* One output line is assembled here and flushed with a single owx_puts, so a
 * concurrently scheduled writer can only appear between lines, never inside one.
 * The longest line the decoder generates is the four-register frame,
 *   "bancat: rip=" + "0x"+16 hex + " rsp=" + "0x"+16 + " cr2=" + "0x"+16 +
 *   " tsc=" + "0x"+16 + CRLF  =  101 bytes,
 * which fits inside 128 with margin.  The only unbounded input is an argv path;
 * bancat_line_putc truncates that rather than overflowing, and bancat_line_flush
 * still NUL-terminates, so a truncated line is short, never corrupt. */
#define BANCAT_LINE_MAX 128U
static char s_line[BANCAT_LINE_MAX];
static uint32_t s_linelen;

static void bancat_line_putc(char c) {
    if (s_linelen + 1U < BANCAT_LINE_MAX) {
        s_line[s_linelen++] = c;
    }
}

static void bancat_line_puts(const char* s) {
    while (*s) {
        bancat_line_putc(*s++);
    }
}

static void bancat_line_hex(uint64_t v) {
    static const char digits[] = "0123456789abcdef";
    int i;
    bancat_line_putc('0');
    bancat_line_putc('x');
    for (i = 60; i >= 0; i -= 4) {
        bancat_line_putc(digits[(v >> i) & 0xFU]);
    }
}

static void bancat_line_u64(uint64_t v) {
    char tmp[21];
    int i = 0;
    if (v == 0U) {
        bancat_line_putc('0');
        return;
    }
    while (v > 0U && i < (int)sizeof(tmp)) {
        tmp[i++] = '0' + (char)(v % 10U);
        v /= 10U;
    }
    while (i > 0) {
        bancat_line_putc(tmp[--i]);
    }
}

static void bancat_line_i64(int64_t v) {
    if (v < 0) {
        bancat_line_putc('-');
        bancat_line_u64((uint64_t)0U - (uint64_t)v);
    } else {
        bancat_line_u64((uint64_t)v);
    }
}

static void bancat_line_flush(void) {
    s_line[s_linelen] = '\0';
    owx_puts(s_line);
    s_linelen = 0U;
}

int bancat_main(int argc, const char *const *argv)
{
    const char *path = BANCAT_DEFAULT_DUMP;
    int64_t n;
    const bdmp_header_t *h;
    const bdmp_cpu_frame_t *f;
    const bdmp_trap_record_t *tr;
    uint32_t i;

    if (argc > 1 && argv && argv[1] && argv[1][0]) {
        path = argv[1];
    }

    bancat_line_puts("bancat: reading ");
    bancat_line_puts(path);
    bancat_line_puts("\r\n");
    bancat_line_flush();

    n = owx_file_read(path, s_dump, (uint32_t)sizeof(s_dump));
    if (n < 0) {
        bancat_line_puts("bancat: read failed code=");
        bancat_line_i64(n);
        bancat_line_puts("\r\n");
        bancat_line_flush();
        owx_puts("bancat: done\r\n");
        return 1;
    }

    if (!bdmp_validate(s_dump, (uint32_t)n)) {
        bancat_line_puts("bancat: not a valid .bdmp (");
        bancat_line_u64((uint64_t)n);
        bancat_line_puts(" bytes)\r\n");
        bancat_line_flush();
        owx_puts("bancat: done\r\n");
        return 2;
    }

    h = (const bdmp_header_t *)(const void *)s_dump;
    f = bdmp_cpu_frame(h);
    tr = bdmp_traps(h);

    bancat_line_puts("bancat: dump OK size=");
    bancat_line_u64((uint64_t)n);
    bancat_line_puts(" version=");
    bancat_line_u64((uint64_t)h->format_version);
    bancat_line_puts("\r\n");
    bancat_line_flush();

    bancat_line_puts("bancat: primary_bancode=");
    bancat_line_hex(h->primary_bancode);
    bancat_line_puts(" cpu=");
    bancat_line_u64((uint64_t)h->faulting_cpu_id);
    bancat_line_puts(" traps=");
    bancat_line_u64((uint64_t)h->trap_count);
    bancat_line_puts(" sentinel=");
    bancat_line_u64((uint64_t)h->sentinel_invoked);
    bancat_line_puts("\r\n");
    bancat_line_flush();

    bancat_line_puts("bancat: rip=");
    bancat_line_hex(f->rip);
    bancat_line_puts(" rsp=");
    bancat_line_hex(f->rsp);
    bancat_line_puts(" cr2=");
    bancat_line_hex(h->fault_cr2);
    bancat_line_puts(" tsc=");
    bancat_line_hex(h->fault_tsc);
    bancat_line_puts("\r\n");
    bancat_line_flush();

    /* trap_count is proven <= BDMP_MAX_TRAPS by bdmp_validate(). */
    for (i = 0; i < h->trap_count; i++) {
        bancat_line_puts("bancat: trap[");
        bancat_line_u64((uint64_t)i);
        bancat_line_puts("] bancode=");
        bancat_line_hex(tr[i].bancode);
        bancat_line_puts(" ip=");
        bancat_line_hex(tr[i].context_ip);
        bancat_line_puts("\r\n");
        bancat_line_flush();
    }

    bancat_line_puts("bancat: memory offset=");
    bancat_line_u64((uint64_t)h->memory_dump_offset);
    bancat_line_puts(" size=");
    bancat_line_u64(h->memory_dump_size);
    bancat_line_puts("\r\n");
    bancat_line_flush();

    owx_puts("bancat: done\r\n");
    return 0;
}
