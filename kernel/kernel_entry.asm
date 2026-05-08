; Declare a stack
section .bss
align 16
stack_bottom:
	resb 65536 ; 16KiB, ~16kb
stack_top:


section .text
global _start
global stack_top

extern kmain		; from kernel.c

_start:
	mov esp, stack_top
	push ebx
	call kmain


cli
_hang:
	hlt
	jmp _hang
