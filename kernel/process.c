#include "process.h"
#include "elf.h"
#include "vmm.h"
#include "kheap.h"
#include "tss.h"
#include "irq.h"
#include "string.h"
#include "stdio.h"
#include <stdint.h>

// Saved by enter_user_mode (assembly), restored by process_exit to unwind
// back to process_run's call site.
uint32_t process_saved_esp = 0;
uint32_t process_saved_ebp = 0;
uint32_t process_saved_ebx = 0;
uint32_t process_saved_esi = 0;
uint32_t process_saved_edi = 0;

static process_t *current_process  = NULL;
static volatile int kill_pending   = 0;

int process_is_running(void) {
    return current_process != NULL;
}

void process_signal_kill(void) {
    kill_pending = 1;
}

extern void enter_user_mode(uint32_t entry, uint32_t stack);
extern void kernel_resume_after_exit(void);

process_t *process_create(vfs_node_t *file) {
    process_t *proc = (process_t *)kmalloc(sizeof(process_t));
    if (!proc) return NULL;
    memset(proc, 0, sizeof(process_t));

    proc->kstack = kmalloc(PROCESS_KSTACK_SIZE);
    if (!proc->kstack) { kfree(proc); return NULL; }
    proc->kstack_top = (uint32_t)proc->kstack + PROCESS_KSTACK_SIZE;

    proc->entry = elf_load(file, proc->mapped_pages, &proc->mapped_count, PROCESS_MAX_PAGES);
    if (!proc->entry) {
        kfree(proc->kstack);
        kfree(proc);
        return NULL;
    }

    for (int i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t vaddr = USER_STACK_VIRT - (uint32_t)i * PAGE_SIZE;
        if (!vmm_alloc_page(vaddr, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)) {
            printf("[process] out of memory for user stack\n");
            kfree(proc->kstack);
            kfree(proc);
            return NULL;
        }
        if (proc->mapped_count < PROCESS_MAX_PAGES)
            proc->mapped_pages[proc->mapped_count++] = vaddr;
    }

    proc->stack_top = USER_STACK_TOP - 4;
    proc->exit_code = 0;
    return proc;
}

void process_run(process_t *proc) {
    current_process = proc;
    tss_set_kernel_stack(proc->kstack_top);
    enter_user_mode(proc->entry, proc->stack_top);
    // Execution resumes here when process_exit restores process_saved_esp.
}

void process_free(process_t *proc) {
    if (!proc) return;
    for (uint32_t i = 0; i < proc->mapped_count; i++)
        vmm_unmap_page(proc->mapped_pages[i]);
    kfree(proc->kstack);
    kfree(proc);
}

void process_check_kill(void) {
    if (!kill_pending || !current_process) return;
    kill_pending = 0;
    PIC_sendEOI(0);   // timer is IRQ 0 — must EOI before abandoning the IRQ stack
    process_exit(1);
}

__attribute__((noreturn)) void process_exit(int code) {
    if (current_process)
        current_process->exit_code = code;
    current_process = NULL;
    kernel_resume_after_exit();   // switches to main kernel stack and ret → process_run
    __builtin_unreachable();
}
