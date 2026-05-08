#ifndef ISRS_H
#define ISRS_H

#include <stdint.h>
#include "regs.h"


extern const char* exception_messages[];

void exception_handler(struct regs *r) __attribute__((noreturn));

#endif
