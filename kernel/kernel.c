#include "vga.h"
#include "tss.h"
#include "gdt.h"
#include "idt.h"
#include "irq.h"
#include "multiboot.h"
#include "pmm.h"
#include "vmm.h"
#include "stdio.h"
#include "timer.h"
#include "utils.h"
#include "kheap.h"
#include "keyboard.h"
#include "shell.h"
#include "ata.h"
#include "vfs.h"
#include "fat16.h"
#include "syscall.h"
#include "uart.h"

extern uint32_t stack_top;
extern uint32_t kernel_end;


void deinit_special_regions() {
	 pmmngr_deinit_region(0x0, 0x400000);
	// // first 1MB — BIOS, stack, etc.
	// // 1MB to 2MB is a safety gap + 2MB to kernel_end
	// pmmngr_deinit_region(0x0, align_up((uint32_t)&kernel_end, PMMNGR_BLOCK_SIZE));

    // // PMM bitmap
    // pmmngr_deinit_region((uint32_t)&kernel_end, align_up(pmmngr_get_bitmap_size(), PMMNGR_BLOCK_SIZE));
}


// // For debugging pmm
// void print_pmm(multiboot_info_t *mbi) {
// 	pmm_print_mmap(mbi->mmap_addr, mbi->mmap_length);
// 	pmm_print_state();
// 	printf("kernel_end: %x\n", (uint32_t)&kernel_end);
// 	printf("mem_upper: %d KB\n", mbi->mem_upper);
// }


void kmain(multiboot_info_t *mbi) 
{	

	install_gdt();
	install_tss(stack_top);

	PIC_remap(0x20, 0x28);		// For IRQ
	install_idt();				// ISR + IRQ
	syscall_install();			// int 0x80


	// Install pmm and set regions
	pmmngr_init(mbi->mem_upper + 1024, (uint32_t)&kernel_end);
	pmm_init_available_regions(mbi->mmap_addr, mbi->mmap_length);
	deinit_special_regions();


	vmm_init();
	// Allow user-mode programs to write to the VGA text buffer directly
	vmm_map_page(0xB8000, 0xB8000, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
	kheap_init();

	timer_install(100);		// Set timer with frequency 100
	keyboard_install();
	uart_init();

  	terminal_initialize();
	ata_init();            // detect which ATA drive holds the disk image

	if (fat16_init() == 0) {
		vfs_mount("/", fat16_vfs_root());
	}
	
	sleep(2000);

	shell_run();
	

}
