/*
 * banhammer_texts.h - OpenWindows Banhammer Panic Text & Quote Selector
 *
 * Provides curated quotes (sarcastic and informational) selected
 * pseudo-randomly using the CPU Time Stamp Counter (TSC) or fault CRC.
 *
 * C99 freestanding - zero dynamic allocation.
 */

#ifndef BANHAMMER_TEXTS_H
#define BANHAMMER_TEXTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    BH_TEXT_SARCASTIC     = 0,
    BH_TEXT_INFORMATIONAL = 1,
    BH_TEXT_RECOVERY      = 2
} bh_text_category_t;

/* Static table of sarcastic panic messages */
static const char *const BH_SARCASTIC_QUOTES[] = {
    "Well, that was graceful.",
    "Congratulations, you hit the B+ trap jackpot!",
    "Have you tried turning the kernel off and never turning it on again?",
    "Kernel went out for milk. It is not coming back.",
    "A wild BANcode appeared! It was super effective.",
    "Oops, all traps.",
    "The kernel has encountered a fatal moment of philosophical self-doubt.",
    "System crashed successfully. No humans were harmed in this disaster.",
    "Divide by cucumber error. Please reinstall universe and reboot.",
    "You have unlocked the secret BANhammer emergency halt achievement."
};
#define BH_SARCASTIC_COUNT (sizeof(BH_SARCASTIC_QUOTES) / sizeof(BH_SARCASTIC_QUOTES[0]))

/* Static table of informational panic messages */
static const char *const BH_INFORMATIONAL_QUOTES[] = {
    "Hardware Translation Layer reported unrecoverable bus stall.",
    "Sentinel recovery was invoked, but damage exceeded threshold.",
    "Page fault in ring-0 without matching PML4 descriptor.",
    "Kernel integrity trap triggered by invalid state vector.",
    "Zero-allocation static buffer bounds overrun intercepted by guard page.",
    "Volume Indexing Protocol (VIP) root partition checksum mismatch.",
    "Modular Bootloader handoff descriptor corrupted in early stage.",
    "Non-maskable interrupt fired during critical spinlock acquisition."
};
#define BH_INFORMATIONAL_COUNT (sizeof(BH_INFORMATIONAL_QUOTES) / sizeof(BH_INFORMATIONAL_QUOTES[0]))

static inline const char *bh_select_quote(uint64_t seed_tsc, bh_text_category_t cat)
{
    /* Simple linear congruential pseudo-random step */
    uint64_t hash = (seed_tsc ^ 0x5DEECE66DULL) * 0x27BB2EE687BULL + 0xBULL;

    if (cat == BH_TEXT_SARCASTIC) {
        return BH_SARCASTIC_QUOTES[hash % BH_SARCASTIC_COUNT];
    } else {
        return BH_INFORMATIONAL_QUOTES[hash % BH_INFORMATIONAL_COUNT];
    }
}

#endif /* BANHAMMER_TEXTS_H */
