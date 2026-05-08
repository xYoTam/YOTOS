#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct gdt_descriptor {
 
	uint32_t		limit;

	uint32_t		base;

	uint8_t			access_byte;
	uint8_t			flags;

} __attribute__((packed));

struct gdtr {
 
	// size of gdt
	uint16_t		limit;
 
	// base address of gdt
	uint32_t		base;
	
} __attribute__((packed));



#define GDT_MAX_ENTRIES 6
extern uint8_t gdt[];

void install_gdt();
void gdt_set_descriptor(struct gdt_descriptor source);

#endif
