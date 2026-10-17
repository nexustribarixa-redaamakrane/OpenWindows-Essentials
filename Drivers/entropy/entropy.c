/*
 * entropy.c - OpenWindows Hardware Entropy Source (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * The driver keeps a 32-word mixing pool that is stirred with samples from
 * the RDRAND instruction (when the CPU provides it) and with time-stamp
 * jitter.  When RDRAND is unavailable the pool is advanced with a
 * splitmix-style nonlinear mixer, which makes the output a deterministic but
 * well-distributed stream that the caller can seed with real entropy.
 */

#include "entropy.h"

/* ── Port / CPU helpers ───────────────────────────────────────── */

#if defined(__x86_64__)

/* Retire one hardware random word; returns false when the sample is not
 * ready (carry flag clear), matching the RDRAND contract. */
static bool entropy_hw32(uint32_t *out)
{
    uint32_t value;
    uint8_t carry;
    __asm__ __volatile__("rdrand %1\n\tsetc %0"
                         : "=qm"(carry), "=r"(value)
                         :
                         : "cc");
    if (carry == 0u) {
        return false;
    }
    *out = value;
    return true;
}

/* Read the time-stamp counter for low-grade jitter. */
static uint64_t entropy_rdtsc(void)
{
    uint32_t lo;
    uint32_t hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

#else /* !__x86_64__ */

/* Portable fallback: no hardware source is available. */
static bool entropy_hw32(uint32_t *out)
{
    (void)out;
    return false;
}

static uint64_t entropy_rdtsc(void)
{
    return 0u;
}

#endif

/* ── Internal state ───────────────────────────────────────────── */

static uint32_t entropy_pool[ENTROPY_POOL_WORDS];
static uint32_t entropy_index;       /* write cursor around the pool  */
static uint32_t entropy_counter;     /* monotonic stir counter        */
static uint32_t entropy_seed_bytes;
static bool     entropy_have_rdrand;
static bool     entropy_initialized;

/* ── Mixing helpers ───────────────────────────────────────────── */

/* Splitmix64 finaliser reduced to 32 bits: strong avalanche per word. */
static uint32_t entropy_mix32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

/* Fold one 32-bit sample into two pool slots. */
static void entropy_stir(uint32_t sample)
{
    uint32_t i = entropy_index & (ENTROPY_POOL_WORDS - 1u);
    uint32_t j = (i + 13u) & (ENTROPY_POOL_WORDS - 1u);

    entropy_pool[i] ^= sample;
    entropy_pool[j] = entropy_mix32(entropy_pool[j] + sample + entropy_counter);
    entropy_counter++;
    entropy_index = i + 1u;
}

/* Produce the next word from the pool. */
static uint32_t entropy_pool_next(void)
{
    uint32_t i = entropy_index & (ENTROPY_POOL_WORDS - 1u);
    uint32_t j = (i + 7u) & (ENTROPY_POOL_WORDS - 1u);
    uint32_t k = (i + 19u) & (ENTROPY_POOL_WORDS - 1u);
    uint32_t v = entropy_pool[i] ^ entropy_pool[j] ^ entropy_pool[k];

    v = entropy_mix32(v + entropy_counter);
    entropy_counter++;
    entropy_pool[i] = v;
    entropy_index = i + 1u;
    return v;
}

/* ── Implementation ───────────────────────────────────────────── */

bool entropy_init(void)
{
    uint32_t i;

    if (entropy_initialized) {
        return true;
    }

    /* Deterministic starting state: nothing secret, just a fixed layout. */
    for (i = 0u; i < ENTROPY_POOL_WORDS; ++i) {
        entropy_pool[i] = entropy_mix32(0x9E3779B9u * (i + 1u));
    }
    entropy_index = 0u;
    entropy_counter = 0u;
    entropy_seed_bytes = 0u;
    entropy_have_rdrand = false;

    /* Probe RDRAND and harvest a handful of hardware words. */
    for (i = 0u; i < 8u; ++i) {
        uint32_t sample = 0u;
        if (entropy_hw32(&sample)) {
            entropy_have_rdrand = true;
            entropy_stir(sample);
        }
    }

    /* Mix in time-stamp jitter as a last-resort source. */
    entropy_stir((uint32_t)entropy_rdtsc());
    entropy_stir((uint32_t)(entropy_rdtsc() >> 32));

    entropy_initialized = true;
    return true;
}

entropy_status_t entropy_read(void *buffer, size_t length)
{
    uint8_t *out = (uint8_t *)buffer;
    size_t i = 0u;

    if (!entropy_initialized) {
        return ENTROPY_ERR_NOT_INIT;
    }
    if (!buffer) {
        return ENTROPY_ERR_INVALID;
    }
    if (length == 0u) {
        return ENTROPY_OK;
    }

    /* Prefer hardware words while RDRAND keeps succeeding. */
    while (entropy_have_rdrand && (length - i) >= 4u) {
        uint32_t word = 0u;
        if (!entropy_hw32(&word)) {
            break;
        }
        out[i + 0u] = (uint8_t)(word & 0xFFu);
        out[i + 1u] = (uint8_t)((word >> 8) & 0xFFu);
        out[i + 2u] = (uint8_t)((word >> 16) & 0xFFu);
        out[i + 3u] = (uint8_t)((word >> 24) & 0xFFu);
        i += 4u;
    }

    /* Anything left (or an all-software source) comes from the pool. */
    while (i < length) {
        uint32_t word = entropy_pool_next();
        uint32_t n = (uint32_t)(length - i);
        uint32_t b;

        if (n > 4u) {
            n = 4u;
        }
        for (b = 0u; b < n; ++b) {
            out[i + (size_t)b] = (uint8_t)((word >> (8u * b)) & 0xFFu);
        }
        i += (size_t)n;
    }

    return ENTROPY_OK;
}

bool entropy_available(void)
{
    return entropy_initialized;
}

entropy_status_t entropy_seed(const void *data, size_t length)
{
    const uint8_t *in = (const uint8_t *)data;
    size_t i;

    if (!entropy_initialized) {
        return ENTROPY_ERR_NOT_INIT;
    }
    if (!data) {
        return ENTROPY_ERR_INVALID;
    }
    if (length == 0u) {
        return ENTROPY_OK;
    }
    if (length > (size_t)ENTROPY_SEED_MAX) {
        length = (size_t)ENTROPY_SEED_MAX;
    }

    for (i = 0u; i < length; ++i) {
        uint32_t acc = (uint32_t)in[i] ^ (uint32_t)i;
        entropy_stir(entropy_mix32(acc));
    }
    entropy_seed_bytes += (uint32_t)length;
    return ENTROPY_OK;
}

void entropy_get_info(entropy_info_t *out)
{
    if (!out) {
        return;
    }
    out->rdrand = entropy_have_rdrand;
    out->initialized = entropy_initialized;
    out->pool_words = ENTROPY_POOL_WORDS;
    out->seed_bytes = entropy_seed_bytes;
}
