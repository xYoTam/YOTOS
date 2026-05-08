#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>

#define KHEAP_FIXED_SIZE  (1024 * 1024)  // 1MB to start

typedef struct block_header {
    uint32_t size;          // size of the block (not including header)
    uint8_t  is_free;
    struct block_header *next;
    struct block_header *prev;
} block_header_t;

void    kheap_init();
void*   kmalloc(uint32_t size);
void    kfree(void *ptr);
void*   kcalloc(uint32_t size);

#endif