/*
 * vgapci.c - OpenWindows PCI & Linear Framebuffer Driver Implementation (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "vgapci.h"

void vgapci_init(vgapci_state_t *state)
{
    if (!state) return;
    state->device_count = 0u;
    state->fb_width = 1024u;
    state->fb_height = 768u;
    state->fb_pitch = 1024u * 4u;
    state->fb_bpp = 32u;
    state->fb_physical_base = 0xE0000000ULL;
}

bool vgapci_add_device(vgapci_state_t *state, const pci_device_desc_t *dev)
{
    if (!state || !dev) return false;
    if (state->device_count >= PCI_MAX_DEVICES) return false;

    state->devices[state->device_count++] = *dev;
    return true;
}

void vgapci_set_framebuffer(vgapci_state_t *state, uint64_t base, uint32_t w, uint32_t h, uint32_t pitch, uint8_t bpp)
{
    if (!state) return;
    state->fb_physical_base = base;
    state->fb_width = w;
    state->fb_height = h;
    state->fb_pitch = pitch;
    state->fb_bpp = bpp;
}
