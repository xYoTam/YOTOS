#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stdbool.h>

// 8 blocks per byte in bitmap
#define PMMNGR_BLOCKS_PER_BYTE 8
 
// block size (4k)
#define PMMNGR_BLOCK_SIZE	4096
 
// block alignment
#define PMMNGR_BLOCK_ALIGN	PMMNGR_BLOCK_SIZE

int mmap_first_free();

void  pmmngr_init(uint32_t memSize, uint32_t bitmap);
void  pmmngr_init_region(uint32_t base, uint32_t size);
void  pmmngr_deinit_region(uint32_t base, uint32_t size);
void* pmmngr_alloc_block();
void  pmmngr_free_block(void *p);
uint32_t   	pmmngr_get_max_blocks();
uint32_t   	pmmngr_get_memory_size();
uint32_t   	pmmngr_get_used_blocks();
uint32_t   	pmmngr_get_free_block_count();
uint32_t	pmmngr_get_bitmap_size();
void pmm_init_available_regions(uint32_t mmap_addr, uint32_t mmap_length);

void pmm_print_mmap(uint32_t mmap_addr, uint32_t mmap_length);
void pmm_print_state(void);

#endif
