#include "stdio.h"
#include "vga.h"
#include "uart.h"
#include <stdarg.h>
#include <stdint.h>



void putchar(char c) {
    terminal_putchar(c);
    if (uart_output_mode) uart_putc(c);
}

static void print_str(const char *s) {
    while (*s) putchar(*s++);
}

static void print_int(uint32_t value) {
    if (value == 0) {
        putchar('0');
        return;
    }

    char buf[11];
    int i = 10;
    buf[10] = '\0';

    while (value > 0) {
        buf[--i] = '0' + (value % 10);
        value /= 10;
    }

    print_str(&buf[i]);
}

static void print_hex(uint32_t value) {
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    buf[10] = '\0';

    for (int i = 9; i >= 2; i--) {
        uint8_t nibble = value & 0xF;
        buf[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        value >>= 4;
    }

    print_str(buf);
}


void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);   // initialize args, starting after fmt

    while (*fmt) {
    if (*fmt == '%') {
        switch (*++fmt) {
            case 'd':
                int n = va_arg(args, int);     // next arg is int
                print_int(n);
                break;

            case 'x':
                int x = va_arg(args, int);     // next arg is int, print as hex
                print_hex(x);
                break;

            case 's':
                char *s = va_arg(args, char*);
                print_str(s);
                break;

            case 'c':
                char c = va_arg(args, int);
                putchar(c);
                break;
        }
    }
    else {
        putchar(*fmt);
    }

    fmt++;
    }
}
