#include "tss.h"
#include "gdt.h"
#include "string.h"

#include <stdint.h>

static struct tss_structure tss;
extern void load_tss(void);

static void gdt_set_tss_descriptor(){
	uint32_t limit = sizeof(struct tss_structure) - 1;
	uint32_t base = (uint32_t)&tss;
	gdt_set_descriptor((struct gdt_descriptor) {
        .limit = limit,
        .base  = base,
        .access_byte = 0x89,  // present, ring 0, TSS 32-bit available
        .flags = 0x0
    	});
}


void install_tss(uint32_t kernel_stack) {

	// initialize structure with zeroes
	memset(&tss, 0, sizeof(struct tss_structure));

	tss.esp0 = kernel_stack;
	tss.ss0 = 0x10; // kernel data descriptor entry

	// Set selectors to user segments descriptors with RPL = 3 (for user mode)
	tss.cs = 0x1b;
	tss.ss = 0x23;
	tss.ds = 0x23;
	tss.es = 0x23;
	tss.fs = 0x23;
	tss.gs = 0x23;

	gdt_set_tss_descriptor();
	load_tss();
}


void tss_set_kernel_stack(uint32_t esp0) {

	tss.esp0 = esp0;
	
}



