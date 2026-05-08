; In this file there are all assembly functions that manipulates cpu
BITS 32


global load_gdt
extern gdtr

load_gdt:
   lgdt [gdtr]

    ; reload data segments
    mov ax, 0x10   ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.flush
	.flush:
    	ret


global load_tss

load_tss:
   cli
   mov ax, 0x28 ; the tss segment selector (5 * 8)
   ltr ax ; load the tss segment descriptor
   ret


global load_idt
extern idtr

load_idt:
    lidt [idtr]
    sti
    ret


; <=====----- ISR stubs -----=====>
extern exception_handler

%macro isr_no_err_stub 1
isr_stub_%+%1:
    push 0          ; fake error code
    push %1         ; vector number
    jmp isr_global_action
%endmacro


%macro isr_err_stub 1
isr_stub_%+%1:
    push %1                 ; vector number
    jmp isr_global_action   ; error code already pushed
%endmacro


isr_no_err_stub 0  ; Divide by Zero
isr_no_err_stub 1  ; Debug
isr_no_err_stub 2  ; Non-Maskable Interrupt
isr_no_err_stub 3  ; Breakpoint
isr_no_err_stub 4  ; Overflow
isr_no_err_stub 5  ; Bound Range Exceeded
isr_no_err_stub 6  ; Invalid Opcode
isr_no_err_stub 7  ; Device Not Available (FPU)
isr_err_stub    8  ; Double Fault
isr_no_err_stub 9  ; Coprocessor Segment Overrun (obsolete)
isr_err_stub    10 ; Invalid TSS
isr_err_stub    11 ; Segment Not Present
isr_err_stub    12 ; Stack Segment Fault
isr_err_stub    13 ; General Protection Fault
isr_err_stub    14 ; Page Fault
isr_no_err_stub 15 ; Reserved
isr_no_err_stub 16 ; x87 Floating Point Exception
isr_err_stub    17 ; Alignment Check
isr_no_err_stub 18 ; Machine Check
isr_no_err_stub 19 ; SIMD Floating Point Exception
isr_no_err_stub 20 ; Virtualization Exception
isr_no_err_stub 21 ; Control Protection Exception
isr_no_err_stub 22 ; Reserved
isr_no_err_stub 23 ; Reserved
isr_no_err_stub 24 ; Reserved
isr_no_err_stub 25 ; Reserved
isr_no_err_stub 26 ; Reserved
isr_no_err_stub 27 ; Reserved
isr_no_err_stub 28 ; Hypervisor Injection
isr_no_err_stub 29 ; VMM Communication
isr_err_stub    30 ; Security Exception
isr_no_err_stub 31 ; Reserved


global isr_stub_table
isr_stub_table:
%assign i 0 
%rep    32 
    dd isr_stub_%+i
%assign i i+1 
%endrep


isr_global_action:
    pusha
    push ds
    push es
    push fs
    push gs

    ; reload kernel segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp        ; pass pointer to entire stack frame
    call exception_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa
    
    add esp, 8      ; remove vector + error code
    iret



; <=====----- IRQ stubs -----=====>

extern irq_dispatch

%macro irq_stub 1
irq_stub_%+%1:
    push 0          ; to fit the regs struct
    push %1         ; push IRQ number
    jmp irq_global_action
%endmacro

irq_stub 0   ; timer
irq_stub 1   ; keyboard
irq_stub 2   ; cascade (don't use)
irq_stub 3   ; COM2
irq_stub 4   ; COM1
irq_stub 5   ; LPT2
irq_stub 6   ; floppy
irq_stub 7   ; LPT1 / spurious
irq_stub 8   ; CMOS RTC
irq_stub 9   ; free
irq_stub 10  ; free
irq_stub 11  ; free
irq_stub 12  ; PS/2 mouse
irq_stub 13  ; FPU
irq_stub 14  ; primary ATA
irq_stub 15  ; secondary ATA / spurious

global irq_stub_table
irq_stub_table:
%assign i 0
%rep    16
    dd irq_stub_%+i
%assign i i+1
%endrep


irq_global_action:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp        ; pass pointer to entire stack frame
    call irq_dispatch
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8      ; remove irq number + fake error code
    iret



; <=====----- Syscall stub (int 0x80) -----=====>

global syscall_stub
extern syscall_handler

syscall_stub:
    push 0          ; fake error code
    push 0x80       ; vector
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call syscall_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret


; <=====----- Ring 3 entry -----=====>
; enter_user_mode(uint32_t entry, uint32_t stack)
; Saves the kernel ESP so process_exit can unwind back here, then irets to ring 3.

global enter_user_mode
global kernel_resume_after_exit
extern process_saved_esp
extern process_saved_ebp
extern process_saved_ebx
extern process_saved_esi
extern process_saved_edi

; kernel_resume_after_exit()
; Called by process_exit() to abandon the per-process kstack and jump back to
; the main kernel stack at the point right after enter_user_mode was called
; (i.e. the return site inside process_run).
kernel_resume_after_exit:
    mov esp, [process_saved_esp]   ; switch to main kernel stack
    mov ebp, [process_saved_ebp]   ; restore caller's frame pointer
    mov ebx, [process_saved_ebx]   ; restore callee-saved registers
    mov esi, [process_saved_esi]
    mov edi, [process_saved_edi]
    ret                            ; pop return address → back inside process_run

enter_user_mode:
    mov eax, [esp+4]            ; entry point
    mov ecx, [esp+8]            ; user stack pointer
    mov [process_saved_esp], esp ; save kernel esp (points to our return address)
    mov [process_saved_ebp], ebp ; save callee-saved registers
    mov [process_saved_ebx], ebx
    mov [process_saved_esi], esi
    mov [process_saved_edi], edi

    pushf
    pop edx
    or edx, 0x200               ; enable interrupts in user mode (set IF)

    push 0x23                   ; user SS  (ring 3 data selector)
    push ecx                    ; user ESP
    push edx                    ; EFLAGS
    push 0x1b                   ; user CS  (ring 3 code selector)
    push eax                    ; EIP
    iret


; Paging
global enable_paging
global load_cr3
global invlpg_addr

enable_paging:
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    ret

load_cr3:
    mov eax, [esp + 4]  ; get the argument (physical address of page directory)
    mov cr3, eax
    ret

invlpg_addr:
    mov eax, [esp + 4]  ; get the virtual address argument
    invlpg [eax]
    ret
