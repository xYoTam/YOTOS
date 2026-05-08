#include "elf.h"
#include "vfs.h"
#include "vmm.h"
#include "stdio.h"
#include "string.h"
#include <stdint.h>

uint32_t elf_load(vfs_node_t *file, uint32_t *pages, uint32_t *count, uint32_t max) {
    Elf32_Ehdr ehdr;

    if (vfs_read(file, 0, sizeof(Elf32_Ehdr), (uint8_t *)&ehdr) != (int)sizeof(Elf32_Ehdr))
        return 0;

    if (ehdr.e_ident[0] != ELF_MAGIC0 || ehdr.e_ident[1] != ELF_MAGIC1 ||
        ehdr.e_ident[2] != ELF_MAGIC2 || ehdr.e_ident[3] != ELF_MAGIC3) {
        printf("[ELF] bad magic\n");
        return 0;
    }
    if (ehdr.e_type != ET_EXEC || ehdr.e_machine != EM_386) {
        printf("[ELF] not an x86 executable\n");
        return 0;
    }

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr phdr;
        uint32_t off = ehdr.e_phoff + (uint32_t)i * ehdr.e_phentsize;

        if (vfs_read(file, off, sizeof(Elf32_Phdr), (uint8_t *)&phdr) != (int)sizeof(Elf32_Phdr))
            return 0;

        if (phdr.p_type != PT_LOAD || phdr.p_memsz == 0)
            continue;

        // Allocate pages covering [p_vaddr, p_vaddr + p_memsz)
        uint32_t seg_start = phdr.p_vaddr & ~(uint32_t)(PAGE_SIZE - 1);
        uint32_t seg_end   = (phdr.p_vaddr + phdr.p_memsz + PAGE_SIZE - 1) & ~(uint32_t)(PAGE_SIZE - 1);

        for (uint32_t vaddr = seg_start; vaddr < seg_end; vaddr += PAGE_SIZE) {
            if (!vmm_alloc_page(vaddr, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)) {
                printf("[ELF] out of memory\n");
                return 0;
            }
            if (*count < max)
                pages[(*count)++] = vaddr;
        }

        // Copy file data into place
        if (phdr.p_filesz > 0) {
            uint8_t buf[512];
            uint32_t remaining = phdr.p_filesz;
            uint32_t read_off  = 0;

            while (remaining > 0) {
                uint32_t chunk = remaining < 512 ? remaining : 512;
                int got = vfs_read(file, phdr.p_offset + read_off, chunk, buf);
                if (got <= 0) return 0;

                uint8_t *dst = (uint8_t *)(phdr.p_vaddr + read_off);
                for (int j = 0; j < got; j++)
                    dst[j] = buf[j];

                read_off  += (uint32_t)got;
                remaining -= (uint32_t)got;
            }
        }

        // Zero BSS region (memory beyond file data)
        if (phdr.p_memsz > phdr.p_filesz) {
            uint8_t *bss = (uint8_t *)(phdr.p_vaddr + phdr.p_filesz);
            uint32_t bss_len = phdr.p_memsz - phdr.p_filesz;
            for (uint32_t j = 0; j < bss_len; j++)
                bss[j] = 0;
        }
    }

    return ehdr.e_entry;
}
