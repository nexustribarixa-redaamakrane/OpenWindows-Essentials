/*
 * vgapci.h - OpenWindows PCI Configuration Space & Linear Framebuffer Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef VGAPCI_H
#define VGAPCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

#define PCI_MAX_DEVICES     32u

typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint32_t bar0;
} pci_device_desc_t;

typedef struct {
    pci_device_desc_t devices[PCI_MAX_DEVICES];
    uint32_t          device_count;

    uint32_t          fb_width;
    uint32_t          fb_height;
    uint32_t          fb_pitch;
    uint8_t           fb_bpp;
    uint64_t          fb_physical_base;
} vgapci_state_t;

void vgapci_init(vgapci_state_t *state);
bool vgapci_add_device(vgapci_state_t *state, const pci_device_desc_t *dev);
void vgapci_set_framebuffer(vgapci_state_t *state, uint64_t base, uint32_t w, uint32_t h, uint32_t pitch, uint8_t bpp);

#endif /* VGAPCI_H */
