CFLAGS += -I inc -nostdlib
CFLAGS += -Tstm32_flash.ld

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump

MACHINE = $(shell uname -m)
OS = $(shell uname -o)

ifeq ($(OS),GNU/Linux)
  MACHINE := "$(MACHINE)-linux"
endif

# this pushes the unchangable bootstub too
all: obj/bootstub.$(PROJ_NAME).bin obj/$(PROJ_NAME).bin
	./tools/enter_download_mode.py
	./tools/dfu-util-$(MACHINE) -a 0 -s 0x08000000 -D obj/bootstub.$(PROJ_NAME).bin
	./tools/dfu-util-$(MACHINE) -a 0 -s 0x08004000 -D obj/$(PROJ_NAME).bin
	./tools/dfu-util-$(MACHINE) --reset-stm32 -a 0 -s 0x08000000

ota: obj/$(PROJ_NAME).bin
	./tools/ota.py $<

ifneq ($(wildcard ../.git/HEAD),) 
obj/gitversion.h: ../.git/HEAD ../.git/index
	echo "const uint8_t gitversion[] = \"$(shell git rev-parse HEAD)\";" > $@
else
obj/gitversion.h: 
	echo "const uint8_t gitversion[] = \"RELEASE\";" > $@
endif

obj/bootstub.$(PROJ_NAME).o: bootstub.c early.h
	$(CC) $(CFLAGS) -o $@ -c $<

obj/main.$(PROJ_NAME).o: main.c *.h obj/gitversion.h
	$(CC) $(CFLAGS) -o $@ -c $<

obj/$(STARTUP_FILE).o: $(STARTUP_FILE).s
	mkdir -p obj
	$(CC) $(CFLAGS) -o $@ -c $<

obj/$(PROJ_NAME).bin: obj/$(STARTUP_FILE).o obj/main.$(PROJ_NAME).o
  # hack
	$(CC) -Wl,--section-start,.isr_vector=0x8004000 $(CFLAGS) -o obj/$(PROJ_NAME).elf $^
	$(OBJCOPY) -v -O binary obj/$(PROJ_NAME).elf $@

obj/bootstub.$(PROJ_NAME).bin: obj/$(STARTUP_FILE).o obj/bootstub.$(PROJ_NAME).o
	$(CC) $(CFLAGS) -o obj/bootstub.$(PROJ_NAME).elf $^
	$(OBJCOPY) -v -O binary obj/bootstub.$(PROJ_NAME).elf $@
	
clean:
	rm -f obj/*

