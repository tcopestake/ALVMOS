CC = x86_64-elf-gcc
LD = x86_64-elf-ld
CFLAGS = -ffreestanding -Wall
LDFLAGS = -T linker.ld
OUTPUT_DIR = .out

SRC = $(wildcard src/kernel/*.c src/kernel/**/*.c)
OBJ = $(patsubst %.c, %.o, $(SRC))

all: kernel.elf kernel.bin clean

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJ)
	$(LD) $(LDFLAGS) -o $(OUTPUT_DIR)/kernel.elf $(OUTPUT_DIR)/kernel-entry.o $(OBJ)

kernel.bin: kernel.elf
	x86_64-elf-objcopy -O binary $(OUTPUT_DIR)/kernel.elf $(OUTPUT_DIR)/kernel.bin

clean:
	rm -f $(OBJ) $(OUTPUT_DIR)/kernel-entry.o
