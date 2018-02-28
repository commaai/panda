# :set noet
PROJ_NAME = comma

CFLAGS  = -O2 -Wall -std=gnu11
CFLAGS += -mlittle-endian -mthumb -mcpu=cortex-m3
CFLAGS += -msoft-float -DSTM32F2 -DSTM32F205xx
CFLAGS += -I ../inc -I ../ -nostdlib
CFLAGS += -T../stm32_flash.ld

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump

all: obj/$(PROJ_NAME).bin
	#$(OBJDUMP) -d obj/$(PROJ_NAME).elf
	dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D $<

obj/main.o: main.c ../*.h
	mkdir -p obj
	$(CC) $(CFLAGS) -o $@ -c $<

obj/startup_stm32f205xx.o: ../startup_stm32f205xx.s
	mkdir -p obj
	$(CC) $(CFLAGS) -o $@ -c $<

obj/$(PROJ_NAME).bin: obj/startup_stm32f205xx.o obj/main.o
	$(CC) $(CFLAGS) -o obj/$(PROJ_NAME).elf $^
	$(OBJCOPY) -v -O binary obj/$(PROJ_NAME).elf $@
	
clean:
	rm -f obj/*

