
CC   = i686-elf-gcc
AS   = nasm
LD   = i686-elf-gcc
QEMU     = qemu-system-i386

OS     = yotos
LINKER = linker.ld

# Directories
BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/objects
ISO_DIR   = $(BUILD_DIR)/isofiles
DISK_MNT  = $(BUILD_DIR)/mnt

# Source directories
SRC_DIRS = boot kernel klibc drivers shell fs


# All source files
C_SOURCES   := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/**/*.c $(dir)/*.c))
ASM_SOURCES := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/**/*.asm $(dir)/*.asm))


# All object files
C_OBJECTS   := $(patsubst %.c,   $(OBJ_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS := $(patsubst %.asm, $(OBJ_DIR)/%.o, $(ASM_SOURCES))


ALL_OBJECTS  = $(ASM_OBJECTS) $(C_OBJECTS)


# Output
KERNEL = $(BUILD_DIR)/$(OS).elf
ISO    = $(BUILD_DIR)/$(OS).iso
DISK   = $(BUILD_DIR)/disk.img

# Flags
CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS = -T $(LINKER) -ffreestanding -O2 -nostdlib
LDLIBS  = -lgcc


# ============================================================
#  Targets
# ============================================================

USER_LD   = user/user.ld

SNAKE_SRC = user/snake.c
SNAKE_ELF = $(BUILD_DIR)/snake.elf

HELLO_SRC = user/hello.c
HELLO_ELF = $(BUILD_DIR)/hello.elf

.PHONY: all iso run run-win clean info snake disk-snake hello disk-hello

all: $(KERNEL)

# Compile C files
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ASM files
$(OBJ_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

# Link kernel
$(KERNEL): $(ALL_OBJECTS) $(LINKER)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(ALL_OBJECTS) -o $@ $(LDLIBS)
	@echo "  --> Built $(KERNEL)"

# Build ISO
iso: $(ISO)

$(ISO): $(KERNEL) grub.cfg
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL)  $(ISO_DIR)/boot/$(OS).elf
	cp grub.cfg   $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)
	@echo "  --> Built $(ISO)"


# Create and populate the disk image
$(DISK):
	dd if=/dev/zero of=$(DISK) bs=1M count=64
	sudo mkfs.fat -F 16 -n "YOTOS" $(DISK)
	
# 	@mkdir -p $(DISK_MNT)
# 	sudo mount -o loop $(DISK) $(DISK_MNT)
# 	echo "Hello from YOTOS!" | sudo tee $(DISK_MNT)/README.TXT
# 	echo "x = 42"            | sudo tee $(DISK_MNT)/TEST.TXT
# 	sudo mkdir -p $(DISK_MNT)/SRC
# 	sudo umount $(DISK_MNT)

	@echo "  --> Built $(DISK)"


# Run in QEMU
run: $(ISO) $(DISK)
	$(QEMU) -boot d -cdrom $(ISO) -drive file=$(DISK),format=raw,index=1,media=disk -serial tcp::4444,server,nowait


# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	@echo "  --> Cleaned"


info:
	@echo "C sources:    $(C_SOURCES)"
	@echo "ASM sources:  $(ASM_SOURCES)"
	@echo "C objects:    $(C_OBJECTS)"
	@echo "ASM objects:  $(ASM_OBJECTS)"

# Build snake as a standalone user-space ELF (linked at 0x400000, no libc)
snake: $(SNAKE_ELF)

$(SNAKE_ELF): $(SNAKE_SRC) $(USER_LD)
	@mkdir -p $(dir $@)
	$(CC) -std=gnu99 -ffreestanding -nostdlib -nostartfiles -O2 \
		-T $(USER_LD) \
		-o $@ $<
	@echo "  --> Built $@"

# Copy snake.elf into the disk image (requires mtools)
disk-snake: $(SNAKE_ELF) $(DISK)
	mcopy -i $(DISK) -o $(SNAKE_ELF) ::SNAKE.ELF
	@echo "  --> Copied snake.elf to disk image"

hello: $(HELLO_ELF)

$(HELLO_ELF): $(HELLO_SRC) $(USER_LD)
	@mkdir -p $(dir $@)
	$(CC) -std=gnu99 -ffreestanding -nostdlib -nostartfiles -O2 \
		-T $(USER_LD) \
		-o $@ $<
	@echo "  --> Built $@"

disk-hello: $(HELLO_ELF) $(DISK)
	mcopy -i $(DISK) -o $(HELLO_ELF) ::HELLO.ELF
	@echo "  --> Copied hello.elf to disk image"
