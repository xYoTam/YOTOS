#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "vfs.h"

#define PROCESS_MAX_PAGES   64
#define PROCESS_KSTACK_SIZE 8192

// Fixed virtual address layout for the user stack
#define USER_STACK_VIRT  0xBFFFF000   // lowest page of the user stack
#define USER_STACK_PAGES 4
#define USER_STACK_TOP   0xC0000000   // one byte past the top page

typedef struct {
    uint32_t  entry;
    uint32_t  stack_top;
    int       exit_code;
    void     *kstack;              // kernel stack used during syscalls
    uint32_t  kstack_top;
    uint32_t  mapped_pages[PROCESS_MAX_PAGES];
    uint32_t  mapped_count;
} process_t;

// Load an ELF binary from file and set up a user stack. Returns NULL on error.
process_t *process_create(vfs_node_t *file);

// Jump to ring 3 entry point. Returns when the process calls sys_exit.
void process_run(process_t *proc);

// Unmap all user pages and free the process struct.
void process_free(process_t *proc);

// Called from the syscall handler: save exit code and return to process_run.
__attribute__((noreturn)) void process_exit(int code);

// Returns 1 if a process is currently running, 0 otherwise.
int process_is_running(void);

// Set the kill flag — process_exit(1) will be called on the next timer tick.
void process_signal_kill(void);

// Called from the timer IRQ handler every tick: if the kill flag is set and a
// process is running, send EOI manually and call process_exit(1).
void process_check_kill(void);

#endif
