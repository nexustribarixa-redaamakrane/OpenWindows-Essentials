/*
 * vmm64.c - OpenWindows Virtual Memory Manager Implementation (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "vmm64.h"

void vmm_init(vmm_address_space_t *as, uint64_t pml4_phys)
{
    if (!as) return;
    as->pml4_physical_base = pml4_phys;
    as->mapping_count = 0u;
    for (size_t i = 0u; i < VMM_MAX_REGIONS; ++i) {
        as->mappings[i].is_active = false;
    }
}

bool vmm_map_pages(vmm_address_space_t *as, uint64_t vaddr, uint64_t paddr, uint64_t pages, uint64_t flags)
{
    if (!as || pages == 0u) return false;
    if (as->mapping_count >= VMM_MAX_REGIONS) return false;

    for (size_t i = 0u; i < VMM_MAX_REGIONS; ++i) {
        if (!as->mappings[i].is_active) {
            as->mappings[i].virtual_addr = vaddr & ~(uint64_t)0xFFF;
            as->mappings[i].physical_addr = paddr & ~(uint64_t)0xFFF;
            as->mappings[i].page_count = pages;
            as->mappings[i].flags = flags | VMM_FLAG_PRESENT;
            as->mappings[i].is_active = true;
            as->mapping_count++;
            return true;
        }
    }
    return false;
}

uint64_t vmm_virt_to_phys(const vmm_address_space_t *as, uint64_t vaddr)
{
    if (!as) return 0u;
    uint64_t page_offset = vaddr & 0xFFF;

    for (size_t i = 0u; i < VMM_MAX_REGIONS; ++i) {
        if (as->mappings[i].is_active) {
            uint64_t start = as->mappings[i].virtual_addr;
            uint64_t end = start + (as->mappings[i].page_count * VMM_PAGE_SIZE);
            if (vaddr >= start && vaddr < end) {
                uint64_t delta = vaddr - start;
                return as->mappings[i].physical_addr + delta;
            }
        }
    }
    return page_offset; /* Identity or unmapped fallback */
}
