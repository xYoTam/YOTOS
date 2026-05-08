#ifndef REGS_H
#define REGS_H

#include <stdint.h>

struct regs {
    // pushed by us manually
    uint32_t gs, fs, es, ds;       // segment registers
    uint32_t edi, esi, ebp, esp,   // pusha order
             ebx, edx, ecx, eax;
    uint32_t vector, error_code;   // pushed by stub
    // pushed automatically by CPU
    uint32_t eip, cs, eflags;
};

#endif
