#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include <stdbool.h>


struct idt_entry {
	uint16_t    isr_low;      // The lower 16 bits of the ISR's address
	uint16_t    kernel_cs;
	uint8_t     reserved;
	uint8_t     attributes;
	uint16_t    isr_high;     // The higher 16 bits of the ISR's address

} __attribute__((packed));

struct idtr {
	uint16_t 	limit;
	uint32_t 	base;

} __attribute__((packed));


void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);
void install_idt();


#define MAX_ENTRIES 256

#endif