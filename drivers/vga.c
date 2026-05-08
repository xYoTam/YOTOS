#include "vga.h"
#include "string.h"
#include "io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "Use a cross compiler!"
#endif

/* 32bit x86 targets. */
#if !defined(__i386__)
#error "Kernel works on a 32bit x86 target"
#endif


static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) 
{
	return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) 
{
	return (uint16_t) uc | (uint16_t) color << 8;
}


#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  0xB8000 

#define VGA_CRTC_INDEX_PORT        0x3D4
#define VGA_CRTC_DATA_PORT         0x3D5

#define VGA_CURSOR_HIGH_REGISTER   0x0E
#define VGA_CURSOR_LOW_REGISTER    0x0F

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer = (uint16_t*)VGA_MEMORY;


static void terminal_update_cursor(void)
{
	const uint16_t position = (uint16_t)(terminal_row * VGA_WIDTH + terminal_column);

	outb(VGA_CRTC_INDEX_PORT, VGA_CURSOR_LOW_REGISTER);
	outb(VGA_CRTC_DATA_PORT, (uint8_t)(position & 0xFF));
	outb(VGA_CRTC_INDEX_PORT, VGA_CURSOR_HIGH_REGISTER);
	outb(VGA_CRTC_DATA_PORT, (uint8_t)((position >> 8) & 0xFF));
}


void terminal_initialize(void) 
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
}

void change_color(enum vga_color fg, enum vga_color bg)
{
	terminal_color = vga_entry_color(fg, bg);
}


void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) 
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}


static void terminal_clear_row(size_t row)
{
	for (size_t x = 0; x < VGA_WIDTH; x++) {
		terminal_putentryat(' ', terminal_color, x, row);
	}
}


static void terminal_scroll(void)
{
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[(y - 1) * VGA_WIDTH + x] =
                terminal_buffer[y * VGA_WIDTH + x];
        }
    }

    terminal_clear_row(VGA_HEIGHT - 1);
    terminal_row = VGA_HEIGHT - 1;
}



void terminal_putchar(char c)
{
	if (c == '\n') {
		terminal_column = 0;
		terminal_row++;
		if (terminal_row == VGA_HEIGHT)
            terminal_scroll();
        terminal_update_cursor();
        return;
	}
	if (c == '\b') {
		if (terminal_column > 0) {
	        terminal_column--;
	        terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
	        terminal_update_cursor();
	    }
	    else {
	    	terminal_row--;
	    	terminal_column = VGA_WIDTH - 1;
	        terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
	        terminal_update_cursor();
	    }
	    return;
	}

	terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
			terminal_scroll();
	}
	terminal_update_cursor();
}

void terminal_write(const char* data, size_t size) 
{
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) 
{
	terminal_write(data, strlen(data));
}



