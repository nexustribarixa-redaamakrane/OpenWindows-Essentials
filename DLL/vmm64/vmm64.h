/*
 * vmm64.h - OpenWindows Virtual Memory Manager & Paging Library (.owd)
 *
 * x86_64 4-level paging (PML4, PDPT, PD, PT) abstraction.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef VMM64_H
#define VMM64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VMM_PAGE_SIZE       4096u
#define VMM_FLAG_PRESENT    0x001u
#define VMM_FLAG_WRITABLE   0x002u
#define VMM_FLAG_USER       0x004u
#define VMM_FLAG_NO_EXEC    (1ULL << 63)

#define VMM_MAX_REGIONS     32u

typedef struct {
    uint64_t virtual_addr;
    uint64_t physical_addr;
    uint64_t page_count;
    uint64_t flags;
    bool     is_active;
} vmm_mapping_t;

typedef struct {
    uint64_t      pml4_physical_base;
    vmm_mapping_t mappings[VMM_MAX_REGIONS];
    uint32_t      mapping_count;
} vmm_address_space_t;

void vmm_init(vmm_address_space_t *as, uint64_t pml4_phys);
bool vmm_map_pages(vmm_address_space_t *as, uint64_t vaddr, uint64_t paddr, uint64_t pages, uint64_t flags);
uint64_t vmm_virt_to_phys(const vmm_address_space_t *as, uint64_t vaddr);

#endif /* VMM64_H */
