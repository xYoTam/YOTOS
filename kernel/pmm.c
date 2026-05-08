#include "pmm.h"
#include "multiboot.h"
#include "string.h"
#include "stdio.h"
#include <stdint.h>
#include <stdbool.h>


// size of physical memory
static	uint32_t	_mmngr_memory_size 	= 0;
 
// number of blocks currently in use
static	uint32_t	_mmngr_used_blocks 	= 0;
 
// maximum number of available memory blocks
static	uint32_t	_mmngr_max_blocks 	= 0;
 
// memory map bit array. Each bit represents a memory block
static	uint32_t*	_mmngr_memory_map 	= 0;


uint32_t pmmngr_get_max_blocks()
{
	return _mmngr_max_blocks;
}

// get memory size in kilobytes (1024 bytes)
uint32_t pmmngr_get_memory_size()
{
	return _mmngr_memory_size;
}

uint32_t pmmngr_get_used_blocks()
{
	return _mmngr_used_blocks;
}

uint32_t pmmngr_get_free_block_count()
{
	return _mmngr_max_blocks - _mmngr_used_blocks;
}

uint32_t pmmngr_get_bitmap_size()
{
	return (_mmngr_max_blocks + PMMNGR_BLOCKS_PER_BYTE - 1) / PMMNGR_BLOCKS_PER_BYTE;
}


// set the n bit to 1 in the bitmap of pm (allocate)
static inline void mmap_set(int bit_n) {
 
	_mmngr_memory_map[bit_n / 32] |= (1 << (bit_n % 32));
}

// set the n bit to 0 in the bitmap of pm (deallocate)
static inline void mmap_unset(int bit_n) {
 
  _mmngr_memory_map[bit_n / 32] &= ~ (1 << (bit_n % 32));
}

// check if the n bit is unallocated (0, return false) or allocated (1, return true)
static inline bool mmap_test(int bit_n) {
 
 return _mmngr_memory_map[bit_n / 32] &  (1 << (bit_n % 32));
}


// Returns index of first free bit in bit map
int mmap_first_free() {
 
	//! find the first free bit
	for (uint32_t i = 0; i < pmmngr_get_max_blocks() / 32; i++)
		if (_mmngr_memory_map[i] != 0xffffffff)
			for (int j = 0; j < 32; j++) {		//! test each bit in the dword
 
				int bit = 1 << j;
				if (! (_mmngr_memory_map[i] & bit) )
					return i*4*8+j;
			}
 
	return -1;
}


void pmmngr_init (uint32_t memSize, uint32_t bitmap) {
 
	_mmngr_memory_size	=		memSize;
	_mmngr_memory_map	=		(uint32_t*) bitmap;
	_mmngr_max_blocks	=		pmmngr_get_memory_size() / 4;	// a block is 4KB
	_mmngr_used_blocks	=		pmmngr_get_max_blocks();
 
	// By default, all of memory is in use
	memset (_mmngr_memory_map, 0xff, pmmngr_get_max_blocks() / PMMNGR_BLOCKS_PER_BYTE );
}


// set memory in the size of "size" as free starting at "base"
void pmmngr_init_region (uint32_t base, uint32_t size) {
 
	int align = base / PMMNGR_BLOCK_SIZE;
	int blocks = size / PMMNGR_BLOCK_SIZE;
 
	for (; blocks > 0; blocks--) {
        if (mmap_test(align)) {  // only count if it was actually used
            _mmngr_used_blocks--;
        }
        mmap_unset(align++);
    }
 
}


// set memory in the size of "size" as used starting at "base"
void pmmngr_deinit_region (uint32_t base, uint32_t size) {
 
	int align = base / PMMNGR_BLOCK_SIZE;
	int blocks = size / PMMNGR_BLOCK_SIZE;
 
	for (; blocks>0; blocks--) {
		if (!mmap_test(align))		// only count if it was actually free
            _mmngr_used_blocks++;
        mmap_set(align++);
    }
}


void* pmmngr_alloc_block () {
 
	if (pmmngr_get_free_block_count() <= 0)
		return 0;	//out of memory
	int frame = mmap_first_free(); // find the first free frame

	if (frame == -1)
		return 0;	//out of memory
 
	mmap_set(frame); // set as used
 	_mmngr_used_blocks++;
 	
	uint32_t addr = frame * PMMNGR_BLOCK_SIZE; // get the address of the frame
	return (void*)addr;
}

void pmmngr_free_block (void* p) {
 
	uint32_t addr = (uint32_t)p;
	int frame = addr / PMMNGR_BLOCK_SIZE; // get which frame the address is
 
	mmap_unset (frame);
 
	_mmngr_used_blocks--;
}


void pmm_init_available_regions(uint32_t mmap_addr, uint32_t mmap_length) {
    multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)mmap_addr;
    
    while ((uint32_t)entry < mmap_addr + mmap_length) {
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            pmmngr_init_region(entry->addr, entry->len);
        }
        entry = (multiboot_mmap_entry_t *)((uintptr_t)entry + entry->size + sizeof(entry->size));
    }
}


void pmm_print_mmap(uint32_t mmap_addr, uint32_t mmap_length) {
    multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)mmap_addr;

    while ((uint32_t)entry < mmap_addr + mmap_length) {
        printf("base: %x  length: %x  type: %d\n",
            (uint32_t)entry->addr,
            (uint32_t)entry->len,
            entry->type);

        entry = (multiboot_mmap_entry_t *)((uintptr_t)entry + entry->size + sizeof(entry->size));
    }
}


void pmm_print_state(void) {

    printf("total blocks: %d\n", pmmngr_get_max_blocks());
    printf("used blocks:  %d\n", pmmngr_get_used_blocks());
    printf("free blocks:  %d\n", pmmngr_get_free_block_count());
}

