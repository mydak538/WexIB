# Компиляторы
ASM = nasm
CC = gcc
LD = ld
OBJCOPY = objcopy
DD = dd

# Флаги
ASM_FLAGS = -f bin
CFLAGS = -m32 -ffreestanding -fno-stack-protector -nostdlib -fno-builtin -O0 -Iinclude
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# Файлы - ИСПРАВЛЕННЫЕ ПУТИ!
BOOT_SRC = scr/boot.asm
BIOSMENU_SRC = scr/biosmenu.c

# Выходные файлы
BIN_DIR = bin
BOOT_BIN = $(BIN_DIR)/boot.bin
BIOSMENU_BIN = $(BIN_DIR)/biosmenu.bin
BIOSMENU_ELF = $(BIN_DIR)/biosmenu.elf
BIOSMENU_O = $(BIN_DIR)/biosmenu.o
IMG = $(BIN_DIR)/bios.img

# Цели
all: $(IMG)

# Создание образа
$(IMG): $(BOOT_BIN) $(BIOSMENU_BIN)
	@mkdir -p $(BIN_DIR)
	$(DD) if=/dev/zero of=$(IMG) bs=512 count=2880 status=none
	$(DD) if=$(BOOT_BIN) of=$(IMG) conv=notrunc status=none
	$(DD) if=$(BIOSMENU_BIN) of=$(IMG) bs=512 seek=1 conv=notrunc status=none

# Загрузчик
$(BOOT_BIN): $(BOOT_SRC)
	@mkdir -p $(BIN_DIR)
	$(ASM) $(ASM_FLAGS) $(BOOT_SRC) -o $(BOOT_BIN)

# C код BIOS
$(BIOSMENU_BIN): $(BIOSMENU_ELF)
	$(OBJCOPY) -O binary $(BIOSMENU_ELF) $(BIOSMENU_BIN)

$(BIOSMENU_ELF): $(BIOSMENU_O) linker.ld
	$(LD) $(LDFLAGS) -o $(BIOSMENU_ELF) $(BIOSMENU_O)

$(BIOSMENU_O): $(BIOSMENU_SRC) include/stdint.h
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -c $(BIOSMENU_SRC) -o $(BIOSMENU_O)

# Очистка
clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
