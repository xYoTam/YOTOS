#include "idt.h"
#include <stdint.h>
#include <stdbool.h>
#include "vga.h"

static struct idt_entry idt[MAX_ENTRIES] __attribute__((aligned(0x10)));

struct idtr idtr;

extern void load_idt(void);


void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    struct idt_entry* descriptor = &idt[vector];

    descriptor->isr_low        = (uint32_t)isr & 0xFFFF;
    descriptor->kernel_cs      = 0x08;
    descriptor->attributes     = flags;
    descriptor->isr_high       = (uint32_t)isr >> 16;
    descriptor->reserved       = 0;
}


static bool vectors[MAX_ENTRIES];   // which stubs are loaded

// Both defined in cpu.asm
extern void* isr_stub_table[];
extern void* irq_stub_table[];

void install_idt() {
    idtr.base  = (uint32_t )&idt[0];
    idtr.limit = (uint16_t)sizeof(struct idt_entry) * MAX_ENTRIES - 1;

    // set up all 32 CPU exception stubs
    for (uint8_t vector = 0; vector < 32; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E); // 0x8E are the flags
        vectors[vector] = true;
    }

    // set up all 16 hardware exception stubs (32 - 47)
    for (uint8_t i = 0; i < 16; i++) {
    idt_set_descriptor(32 + i, irq_stub_table[i], 0x8E);
    vectors[32 + i] = true;
    }
    

    load_idt();  // lidt command assembly

}