#include <stdint.h>

#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "stdio.h"

static page_directory_t* kernel_directory;

extern uint32_t kernel_end;
extern void enable_paging(void);
extern void load_cr3(uint32_t page_directory);
extern void invlpg_addr(uint32_t virt);

page_directory_t* vmm_get_kernel_directory() {
    return kernel_directory;
}


page_directory_t* vmm_create_directory() {
    page_directory_t* dir = (page_directory_t *)pmmngr_alloc_block();
    if (!dir)
        return 0;  // out of memory

    // temporarily map it at a known virtual address
    vmm_map_page(TEMP_MAP_ADDR, (uint32_t)dir, PAGE_PRESENT | PAGE_WRITABLE);
    memset((void *)TEMP_MAP_ADDR, 0, PAGE_SIZE);
    vmm_unmap_page(TEMP_MAP_ADDR);

    return dir;
}



void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = PD_INDEX(virt);
    uint32_t pt_idx = PT_INDEX(virt);

    page_directory_t *pd = (page_directory_t *)0xFFFFF000;

    // if page table doesn't exist, allocate it
    if (!ENTRY_HAS_FLAG(pd->entries[pd_idx], PAGE_PRESENT)) {
        uint32_t new_table = (uint32_t)pmmngr_alloc_block();
        if (!new_table)
            return;

        pd->entries[pd_idx] = new_table | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);

        // zero it through the recursive window
        memset((void *)(0xFFC00000 + pd_idx * PAGE_SIZE), 0, PAGE_SIZE);
    } else if (flags & PAGE_USER) {
        // x86 requires BOTH the PDE and PTE to have PAGE_USER for ring 3 access.
        // If the page table already exists but lacks PAGE_USER on the PDE, set it now.
        // Other PTEs in this table still need their own PAGE_USER to be accessible.
        pd->entries[pd_idx] |= PAGE_USER;
    }

    // access page table through recursive window and set entry
    page_table_t *pt = (page_table_t *)(0xFFC00000 + pd_idx * PAGE_SIZE);
    pt->entries[pt_idx] = (phys & ~0xFFF) | flags | PAGE_PRESENT;

    invlpg_addr(virt);
}



void* vmm_alloc_page(uint32_t virt, uint32_t flags) {
    void *phys = pmmngr_alloc_block();
    if (!phys) return 0;
    vmm_map_page(virt, (uint32_t)phys, flags);
    return (void *)virt;
}


void vmm_unmap_page(uint32_t virt) {
    uint32_t pd_idx = PD_INDEX(virt);
    uint32_t pt_idx = PT_INDEX(virt);

    page_directory_t *pd = (page_directory_t *)0xFFFFF000;
    if (!ENTRY_HAS_FLAG(pd->entries[pd_idx], PAGE_PRESENT))
        return;

    page_table_t *pt = (page_table_t *)(0xFFC00000 + pd_idx * PAGE_SIZE);
    if (!ENTRY_HAS_FLAG(pt->entries[pt_idx], PAGE_PRESENT))
        return;

    uint32_t phys = ENTRY_GET_FRAME(pt->entries[pt_idx]);
    pt->entries[pt_idx] = 0;
    invlpg_addr(virt);
    pmmngr_free_block((void *)phys);
}


static void vmm_early_identity_map(page_directory_t *dir, uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = PD_INDEX(virt);
    uint32_t pt_idx = PT_INDEX(virt);

    // if page table doesn't exist, allocate it
    if (!ENTRY_HAS_FLAG(dir->entries[pd_idx], PAGE_PRESENT)) {
        page_table_t *new_table = (page_table_t *)pmmngr_alloc_block();
        if (!new_table)
            return;
        dir->entries[pd_idx] = (uint32_t)new_table | PAGE_PRESENT | PAGE_WRITABLE;
    }

    // dereference physical address directly — safe because paging is off
    page_table_t *pt = (page_table_t *)ENTRY_GET_FRAME(dir->entries[pd_idx]);

    pt->entries[pt_idx] = (phys & ~0xFFF) | flags | PAGE_PRESENT;
}



void vmm_map_kernel(page_directory_t *dir) {
    // uint32_t kernel_size = (uint32_t)&kernel_end + pmmngr_get_bitmap_size();
    
    for (uint32_t addr = 0x1000; addr < 0x400000; addr += PAGE_SIZE) {
        vmm_early_identity_map(dir, addr, addr, PAGE_PRESENT | PAGE_WRITABLE);
    }
}


void vmm_init() {
    kernel_directory = (page_directory_t *)pmmngr_alloc_block();
    memset(kernel_directory, 0, PAGE_SIZE);

    // set up recursive mapping at entry 1023
    kernel_directory->entries[1023] = (uint32_t)kernel_directory | PAGE_PRESENT | PAGE_WRITABLE;

    vmm_map_kernel(kernel_directory);
    load_cr3((uint32_t)kernel_directory);

    enable_paging();
}


