#include "gdt.h"
#include "string.h"
#include "vga.h"

uint8_t gdt[GDT_MAX_ENTRIES * 8]; // each entry is 8 bytes

struct gdtr	gdtr;

static uint8_t i = 0;
extern void load_gdt(void);

void install_gdt() {

	// set gdtr
	gdtr.limit 	= sizeof(gdt) - 1;
	gdtr.base	= (uint32_t)gdt;

	// initialize gdt in memory with zeroes
	memset(gdt, 0, sizeof(gdt));

	// Null descriptor (entry 0)
	gdt_set_descriptor((struct gdt_descriptor){0, 0, 0, 0});

	// Kernel code segment (entry 1)
	gdt_set_descriptor((struct gdt_descriptor){
	    .limit = 0xFFFFF,
	    .base = 0x00000000,
	    .access_byte = 0x9A,   // present, ring 0, code, executable, readable
	    .flags = 0xC           // 4KB granularity, 32-bit
	});

	// Kernel data segment (entry 2)
	gdt_set_descriptor((struct gdt_descriptor){
	    .limit = 0xFFFFF,
	    .base = 0x00000000,
	    .access_byte = 0x92,   // present, ring 0, data, writable
	    .flags = 0xC
	});

	// User code segment (entry 3)
	gdt_set_descriptor((struct gdt_descriptor){
	    .limit = 0xFFFFF,
	    .base = 0x00000000,
	    .access_byte = 0xFA,   // present, ring 3, code, executable, readable
	    .flags = 0xC           // 4KB granularity, 32-bit
	});

	// User data segment (entry 4)
	gdt_set_descriptor((struct gdt_descriptor){
	    .limit = 0xFFFFF,
	    .base = 0x00000000,
	    .access_byte = 0xF2,   // present, ring 3, data, writable
	    .flags = 0xC           // 4KB granularity, 32-bit
	});

	// TSS descriptor will be created after tss is installed in it's own file
	load_gdt();
}



void gdt_set_descriptor(struct gdt_descriptor source)
{
	if (i >= GDT_MAX_ENTRIES)
		return;
	
	uint8_t *target = &gdt[i++ * 8];
	// Check the limit to make sure that it can be encoded
    
    // Encode the limit
    target[0] = source.limit & 0xFF;
    target[1] = (source.limit >> 8) & 0xFF;
    target[6] = (source.limit >> 16) & 0x0F;
    
    // Encode the base
    target[2] = source.base & 0xFF;
    target[3] = (source.base >> 8) & 0xFF;
    target[4] = (source.base >> 16) & 0xFF;
    target[7] = (source.base >> 24) & 0xFF;
    
    // Encode the access byte
    target[5] = source.access_byte;
    
    // Encode the flags
    target[6] |= (source.flags << 4);

}
