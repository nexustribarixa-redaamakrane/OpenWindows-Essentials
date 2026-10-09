/*
 * pci_cache.h - OpenWindows PCI Device Enumeration Cache (.kcache)
 *
 * Binary cache format with text-parseable header section.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef PCI_CACHE_H
#define PCI_CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PCI_CACHE_MAGIC    0x4B434348u  /* 'KCCH' */
#define PCI_CACHE_VERSION  1u
#define PCI_CACHE_MAX_ENTRIES 4096u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t total_size;
    uint32_t checksum;
    uint8_t  reserved[12];
} pci_cache_header_t;

typedef struct {
    uint32_t key_hash;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t ttl_seconds;
    uint64_t timestamp;
} pci_cache_entry_t;

bool pci_cache_init(void *buffer, size_t size);
bool pci_cache_lookup(const void *buffer, uint32_t key, void *out, size_t *out_size);
bool pci_cache_insert(void *buffer, uint32_t key, const void *data, size_t size, uint32_t ttl);
void pci_cache_flush(void *buffer);
uint32_t pci_cache_count(const void *buffer);

#endif /* PCI_CACHE_H */
