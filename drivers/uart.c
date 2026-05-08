#include "uart.h"
#include "io.h"

#define COM1 0x3F8

int uart_output_mode = 0;

void uart_init() {
    outb(COM1 + 1, 0x00); // disable interrupts
    outb(COM1 + 3, 0x80); // enable DLAB
    outb(COM1 + 0, 0x03); // baud 38400 divisor lo
    outb(COM1 + 1, 0x00); // baud 38400 divisor hi
    outb(COM1 + 3, 0x03); // 8n1
    outb(COM1 + 2, 0xC7); // enable FIFO
    outb(COM1 + 4, 0x0B); // RTS/DSR
}

void uart_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20));    // wait until transmit buffer is empty
    outb(COM1, c);
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

char uart_getchar() {
    // check if a new character is in the buffer by checking the status port
    // of the uart (base port + 5). if true, get it from the base port 
    if (!(inb(COM1 + 5) & 0x01)) return 0;
    return inb(COM1);
}
