#include <stdint.h>
#include "keyboard.h"
#include "irq.h"
#include "io.h"
#include "process.h"

#define KEYBOARD_DATA_PORT 0x60

static char key_buffer[256];
static uint8_t buf_head = 0;
static uint8_t buf_tail = 0;
static uint8_t caps  = 0;
static uint8_t shift = 0;
static uint8_t ctrl  = 0;

static char scancode_to_ascii[] = {
    0, 0, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' '
};

static char scancode_to_ascii_upper[] = {
    0, 0, '!','@','#','$','%','^','&','*','(',')','-','=', 0,
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' '
};


static void keyboard_handler() {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // key releases (bit 7 set)
    if (scancode & 0x80) {
        if (scancode == 0xAA || scancode == 0xB6) shift = 0;  // shift release
        if (scancode == 0x9D)                     ctrl  = 0;  // ctrl release
        return;
    }

    if (scancode == 0x1D) { ctrl  = 1; return; }  // left ctrl press
    if (scancode == 0x2A || scancode == 0x36) {
    	shift = 1;
    	return;
    }

    // Ctrl+C — kill the running process
    if (ctrl && scancode == 0x2E) {
        if (process_is_running())
            process_signal_kill();
        return;
    }

    if (scancode == 0x3A) {
    	caps = !caps;
    	return;
    }

    if (scancode < sizeof(scancode_to_ascii)) {
        char c = shift ? scancode_to_ascii_upper[scancode]
                       : scancode_to_ascii[scancode];

        if (caps && c >= 'a' && c <= 'z')
            c -= 32;  // lowercase to uppercase
        if (c)
            key_buffer[buf_tail++] = c;  // buf_tail wraps at 256 automatically
    }
}


void keyboard_install() { 
    irq_install_handler(1, keyboard_handler);
}


char keyboard_getchar() {
    if (buf_head == buf_tail)
        return 0;  // buffer empty
    return key_buffer[buf_head++];
}

void keyboard_flush() {
    buf_head = buf_tail;
}

