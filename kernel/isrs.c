#include <stdint.h>
#include "isrs.h"
#include "stdio.h"

const char* exception_messages[] = {
    "Divide by Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating Point",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating Point",
    "Virtualization",
    "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved",
    "Hypervisor Injection",
    "VMM Communication",
    "Security Exception",
    "Reserved"
};



static void normal_exception_handler(uint32_t interrupt_num) {
    printf("EXCEPTION: %s\n", exception_messages[interrupt_num]);
    // TODO: print error code
    __asm__ volatile ("cli; hlt");
}


static void page_fault_handler(uint32_t error_code, struct regs *r) {
    uint32_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));
    uint32_t present  =     error_code & 0x1;           // 0 = not-present page
    uint32_t write    =     error_code & 0x2;           // 1 = write, 0 = read
    uint32_t user     =     error_code & 0x4;           // 1 = user mode, 0 = kernel

    // if (user && !present && is_valid_user_region(faulting_address)) {
    //     // demand paging — map it and resume
    //     vmm_alloc_page(faulting_address & ~0xFFF, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    //     return;
    // }

    printf("PAGE FAULT at %x\n", faulting_address);
    printf("  cause: %s %s %s\n",
        present ? "protection"  : "not-present",
        write   ? "write"       : "read",
        user    ? "user-mode"   : "kernel-mode"
    );
    printf("  eip: %x  cs: %x\n", r->eip, r->cs);

    __asm__ volatile ("cli; hlt");
}


__attribute__((noreturn))
void exception_handler(struct regs* r) {
    // get the vector number and error code from the stack frame
    uint32_t interrupt_num = r->vector;
    uint32_t error_code = r->error_code;

    if (interrupt_num == 0x0E)  // Page fault
        page_fault_handler(error_code, r);
    else
        normal_exception_handler(interrupt_num);

    __builtin_unreachable();

}
