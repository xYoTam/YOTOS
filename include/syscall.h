#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_EXIT     1
#define SYS_WRITE    2
#define SYS_READKEY  3   // returns next char from keyboard buffer, or 0 if empty
#define SYS_SLEEP    4   // ebx = milliseconds

// Register the int 0x80 handler in the IDT.
void syscall_install(void);

#endif
