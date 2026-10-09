/*
 * spi.c - OpenWindows SPI Bus Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "spi.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool spi_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void spi_init(void)
{
    if (!spi_initialized) { return; }
    /* TODO: implement spi_init */
    (void)0;
}
void spi_transfer(void)
{
    if (!spi_initialized) { return; }
    /* TODO: implement spi_transfer */
    (void)0;
}
void spi_set_mode(void)
{
    if (!spi_initialized) { return; }
    /* TODO: implement spi_set_mode */
    (void)0;
}
void spi_set_speed(void)
{
    if (!spi_initialized) { return; }
    /* TODO: implement spi_set_speed */
    (void)0;
}

