#include <stdint.h>
#include "irq.h"
#include "io.h"

static void (*irq_handlers[16])(void) = {0};

void PIC_sendEOI(uint8_t irq)
{
	if(irq >= 8)
		outb(PIC2_COMMAND,PIC_EOI);
	
	outb(PIC1_COMMAND,PIC_EOI);
}

void PIC_remap(int offset1, int offset2)
{
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

	outb(PIC1_DATA, offset1);                 	// ICW2: Master PIC vector offset
	outb(PIC2_DATA, offset2);                 	// ICW2: Slave PIC vector offset

	outb(PIC1_DATA, 1 << CASCADE_IRQ);        	// ICW3: tell Master PIC that there is a slave PIC at IRQ2
	outb(PIC2_DATA, 2);                       	// ICW3: tell Slave PIC its cascade identity (0000 0010)
	
	outb(PIC1_DATA, ICW4_8086);              	 // ICW4: have the PICs use 8086 mode (and not 8080 mode)
	outb(PIC2_DATA, ICW4_8086);

	// mask both PICs.
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);
}


void IRQ_set_mask(uint8_t IRQline) {
    uint16_t port;
    uint8_t value;

    if (IRQline < 8) {
        port = PIC1_DATA;
    } 
    else if (IRQline < 16) {
        port = PIC2_DATA;
        IRQline -= 8;
    }
    else {
    	return;
    }
    value = inb(port) | (1 << IRQline);
    outb(port, value);        
}

void IRQ_clear_mask(uint8_t IRQline) {
    uint16_t port;
    uint8_t value;

    if (IRQline < 8) {
        port = PIC1_DATA;
    }
    else if (IRQline < 16) {
        port = PIC2_DATA;
        IRQline -= 8;
    }
    else {
    	return;
    }
    value = inb(port) & ~(1 << IRQline);
    outb(port, value);        
}



void irq_install_handler(uint8_t irq, void (*handler)(void)) {
    if (irq >= 16) return;
    irq_handlers[irq] = handler;
    IRQ_clear_mask(irq);        // unmask the line when a handler is registered
}

void irq_uninstall_handler(uint8_t irq)
{
    if (irq >= 16) return;
    irq_handlers[irq] = 0;
    IRQ_set_mask(irq);          // re-mask when handler is removed
}

void irq_dispatch(struct regs *r)
{
	uint8_t irq = r->vector;
    if (irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq]();
    }
    PIC_sendEOI(irq);           // always send EOI even if no handler
}
