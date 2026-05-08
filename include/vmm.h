#ifndef VMM_H
#define VMM_H

#define PAGE_SIZE           4096
#define PAGE_ENTRIES        1024
#define TEMP_MAP_ADDR       0xFFBFF000

// Page flags
#define PAGE_PRESENT        (1 << 0)
#define PAGE_WRITABLE       (1 << 1)
#define PAGE_USER           (1 << 2)

typedef uint32_t pd_entry;   // page directory entry
typedef uint32_t pt_entry;   // page table entry


typedef struct {
    pt_entry entries[PAGE_ENTRIES];
} page_table_t;

typedef struct {
    pd_entry entries[PAGE_ENTRIES];
} page_directory_t;


// Extract indices from virtual address
#define PD_INDEX(addr)    ((addr) >> 22)
#define PT_INDEX(addr)    (((addr) >> 12) & 0x3FF)
#define PAGE_OFFSET(addr) ((addr) & 0xFFF)

// Get/set physical address in an entry (top 20 bits)
#define ENTRY_GET_FRAME(entry)       ((entry) & ~0xFFF)
#define ENTRY_SET_FRAME(entry, addr) ((entry) = ((entry) & 0xFFF) | ((addr) & ~0xFFF))

// Flag helpers
#define ENTRY_SET_FLAG(entry, flag)   ((entry) |= (flag))
#define ENTRY_CLEAR_FLAG(entry, flag) ((entry) &= ~(flag))
#define ENTRY_HAS_FLAG(entry, flag)   ((entry) & (flag))

page_directory_t*   vmm_get_kernel_directory();
page_directory_t*   vmm_create_directory();
void                vmm_map_kernel(page_directory_t *dir);
void                vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void*               vmm_alloc_page(uint32_t virt, uint32_t flags);
void                vmm_unmap_page(uint32_t virt);
void                vmm_init();

#endif