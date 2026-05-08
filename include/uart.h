#ifndef UART_H
#define UART_H

extern int uart_output_mode;

void uart_init();
void uart_putc(char c);
void uart_puts(const char *s);
char uart_getchar();

#endif
