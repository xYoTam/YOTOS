#include "syscall.h"
#include "idt.h"
#include "process.h"
#include "regs.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include <stdint.h>

extern void syscall_stub(void);

static void sys_write(uint32_t buf, uint32_t len) {
    const char *s = (const char *)buf;
    for (uint32_t i = 0; i < len; i++)
        terminal_putchar(s[i]);
}

void syscall_handler(struct regs *r) {
    switch (r->eax) {
        case SYS_EXIT:
            process_exit((int)r->ebx);
            break;
        case SYS_WRITE:
            sys_write(r->ebx, r->ecx);
            break;
        case SYS_READKEY:
            r->eax = (uint32_t)(uint8_t)keyboard_getchar();
            break;
        case SYS_SLEEP:
            sleep(r->ebx);
            break;
        default:
            break;
    }
}

void syscall_install(void) {
    // 0xEF = present, DPL=3, 32-bit trap gate — allows ring 3 to call int 0x80.
    // Trap gate (unlike interrupt gate) does not clear IF, so timer IRQs keep
    // firing during syscalls like sleep() that busy-wait on timer_ticks.
    idt_set_descriptor(0x80, syscall_stub, 0xEF);
}
