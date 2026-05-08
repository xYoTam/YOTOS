#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

static inline uint32_t align_up(uint32_t value, uint32_t alignment) {
    // Round value up so we dont loose a part of it.
    // paging is in 4KiB pages, so e.g. 5000 bytes will be 2 pages and not 1.
    return (value + alignment - 1) & ~(alignment - 1);
}

#endif