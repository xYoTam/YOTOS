#include <stdint.h>
#include "kheap.h"
#include "pmm.h"
#include "vmm.h"
#include "utils.h"
#include "string.h"
#include "stdio.h"


extern uint32_t kernel_end;

static block_header_t* heap_start_ptr = 0;

void kheap_init() {
	uint32_t heap_start = align_up((uint32_t)&kernel_end + pmmngr_get_bitmap_size(), PAGE_SIZE);
    for (uint32_t addr = heap_start; addr < heap_start + KHEAP_FIXED_SIZE; addr += PAGE_SIZE) {
        vmm_alloc_page(addr, PAGE_PRESENT | PAGE_WRITABLE);
    }

    heap_start_ptr = (block_header_t *)heap_start;
    heap_start_ptr->size    = KHEAP_FIXED_SIZE - sizeof(block_header_t);
    heap_start_ptr->is_free = 1;
    heap_start_ptr->next    = 0;
    heap_start_ptr->prev    = 0;
}


void* kmalloc(uint32_t size) {
    block_header_t* current = heap_start_ptr;
    while (current) {
        if (current->is_free && current->size >= size) {
            if (current->size >= size + sizeof(block_header_t) + 1) {
                block_header_t* new_block = (block_header_t*)((uint32_t)current + sizeof(block_header_t) + size);
                new_block->size    = current->size - size - sizeof(block_header_t);
                new_block->is_free = 1;
                new_block->next    = current->next;
                new_block->prev    = current;

                if (current->next)
                    current->next->prev = new_block;

                current->next = new_block;
                current->size = size;
            }
            current->is_free = 0;
            return (void *)((uint32_t)current + sizeof(block_header_t));
        }
        current = current->next;
    }
    return 0;  // out of memory
}


void kfree(void *ptr) {
    if (!ptr)
        return;

    // 1. get the header from the pointer
    block_header_t *header = (block_header_t *)((uint32_t)ptr - sizeof(block_header_t));
    header->is_free = 1;

    // 2. coalesce with next block if it's free
    if (header->next && header->next->is_free) {
        header->size += sizeof(block_header_t) + header->next->size;
        header->next  = header->next->next;
        if (header->next)
            header->next->prev = header;
    }

    // 3. coalesce with prev block if it's free
    if (header->prev && header->prev->is_free) {
        header->prev->size += sizeof(block_header_t) + header->size;
        header->prev->next  = header->next;
        if (header->next)
            header->next->prev = header->prev;
    }
}


void *kcalloc(uint32_t size) {
    void *ptr = kmalloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}
