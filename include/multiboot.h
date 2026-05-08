#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>


#define MULTIBOOT_MEMORY_AVAILABLE  1


typedef struct {
    uint32_t flags;             // offset 0

    uint32_t mem_lower;         // offset 4  - KB of low memory
    uint32_t mem_upper;         // offset 8  - KB of high memory

    uint32_t boot_device;       // offset 12

    uint32_t cmdline;           // offset 16

    uint32_t mods_count;        // offset 20
    uint32_t mods_addr;         // offset 24

    uint32_t syms[4];           // offset 28

    uint32_t mmap_length;       // offset 44
    uint32_t mmap_addr;         // offset 48

    uint32_t drives_length;     // offset 52
    uint32_t drives_addr;       // offset 56

    uint32_t config_table;      // offset 60

    uint32_t boot_loader_name;  // offset 64

    uint32_t apm_table;         // offset 68

    uint32_t vbe_control_info;  // offset 72
    uint32_t vbe_mode_info;     // offset 76
    uint16_t vbe_mode;          // offset 80
    uint16_t vbe_interface_seg; // offset 82
    uint16_t vbe_interface_off; // offset 84
    uint16_t vbe_interface_len; // offset 86

    uint64_t framebuffer_addr;  // offset 88
    uint32_t framebuffer_pitch; // offset 96
    uint32_t framebuffer_width; // offset 100
    uint32_t framebuffer_height;// offset 104
    uint8_t  framebuffer_bpp;   // offset 108
    uint8_t  framebuffer_type;  // offset 109
    uint8_t  color_info[6];     // offset 110
} __attribute__((packed)) multiboot_info_t;


// Memory map entry (one per region in the map)
typedef struct {
    uint32_t size;      // size of this entry, not counting this field
    uint64_t addr;      // base address of region
    uint64_t len;       // length in bytes
    uint32_t type;      // 1 = available, anything else = reserved
} __attribute__((packed)) multiboot_mmap_entry_t;

#endif