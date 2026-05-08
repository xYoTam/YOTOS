# YOTOS
32bit x86 OS built from scratch in C and Assembly.

---

## Features

- **Protected mode** — full 32-bit x86 protected mode with GDT (ring 0/3 segments)
- **Memory management** — physical memory manager (bitmap allocator), paging (VMM), kernel heap
- **Interrupts** — IDT: 32 ISR stubs, IRQ remapping via PIC 8259, syscalls via `int 0x80`
- **Processes** — ELF binary loader, user-mode execution (ring 3), TSS
- **Drivers** — VGA text mode (80×25), PS/2 keyboard, PIT timer, ATA/IDE disk, UART serial
- **Filesystem** — VFS abstraction layer with a full FAT16 implementation
- **Shell** — interactive shell with built-in commands
- **User programs** — hello world and a playable Snake game
- **klibc** — minimal C library (printf, string utilities)
- **UART terminal** — `yotos_terminal.py`, a pygame GUI client that connects to the UART serial port over TCP and lets you interact with the OS remotely


---

## Project structure

```
.
├── boot/        Multiboot header (NASM)
├── kernel/      GDT, IDT, PMM, VMM, heap, processes, ELF loader, syscalls
├── drivers/     VGA, keyboard, timer, ATA, UART
├── fs/          VFS layer and FAT16 implementation
├── shell/       Shell and built-in commands
├── klibc/       Minimal libc (stdio, string)
├── user/        User-mode programs (hello, snake)
├── include/     Header files
├── linker.ld    Kernel linker script (loads at 2 MB)
├── grub.cfg     GRUB bootloader config
└── makefile
```

---

## Building

You need a Linux environment (native or WSL).

### Dependencies

```bash
sudo apt install nasm make qemu-system-x86 grub-pc-bin xorriso mtools dosfstools
for the python remote client: pip install pygame
```

You also need an **i686-elf cross-compiler**. Follow the [OSDev GCC Cross-Compiler guide](https://wiki.osdev.org/GCC_Cross-Compiler) to build it, then add it to your `PATH`.

### Build commands

```bash
make          # Build the kernel ELF
make iso      # Build a bootable ISO
make snake    # Build the Snake user program
make disk-snake  # Copy Snake to the disk image
make hello    # Build the hello world user program
make disk-hello  # Copy hello to the disk image
```

Output goes to `build/`:

| File | Description |
|------|-------------|
| `build/yotos.elf` | Kernel ELF |
| `build/yotos.iso` | Bootable ISO |
| `build/disk.img` | 64 MB FAT16 disk image |
| `build/snake.elf` | Snake user program |
| `build/hello.elf` | Hello world user program |

---

## Running

### Linux
```bash
make run
```

This boots the ISO in QEMU with the FAT16 disk attached. A serial port is available on TCP port 4444 for a telnet like protocol.

---

## Shell commands

```
ls                          List files
cat <file>                  Print file contents
cd <dir>                    Change directory
mkdir <dir>                 Create a directory
touch <file>                Create an empty file
echo <text>                 Print text
echo <text> '>' <files>     Create a files with text in it
exec <file>                 Execute a user ELF binary
```

### Running Snake

```
exec SNAKE.ELF
```

Use wasd keys to move. Press `q` to quit.

---

## Syscalls (`int 0x80`)

| Number | Name | Description |
|--------|------|-------------|
| 0 | `sys_exit` | Terminate the current process |
| 1 | `sys_write` | Write a string to the VGA console |
| 2 | `sys_readkey` | Read a keypress |

---

## Memory layout

| Address | Contents |
|---------|----------|
| `0x00000000` – `0x000FFFFF` | Low memory / BIOS |
| `0x00200000` | Kernel |
| `0x00400000` | User programs |
| `0xBFFFF000` | User stack (4 pages) |

---

## Clean

```bash
make clean
```
