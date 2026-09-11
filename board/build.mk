.SECONDARY:
.DELETE_ON_ERROR:

PYTHON ?= python3
CROSS_COMPILE ?= arm-none-eabi-
ARM_CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
OPENDBC_INCLUDE ?= $(shell $(PYTHON) -c 'import opendbc; print(opendbc.INCLUDE_PATH)')
BUILD_TYPE := $(if $(RELEASE),RELEASE,DEBUG)
CERT := $(if $(RELEASE),$(CERT),board/certs/debug)

# V=1 prints full commands; the default keeps build progress easy to scan.
Q := $(if $(filter 1,$(V)),,@)
COMMON_FLAGS := -mcpu=cortex-m7 -mhard-float -mfpu=fpv5-d16 -DSTM32H7 -DSTM32H725xx \
  -Wall -Wextra -Wstrict-prototypes -Werror -mlittle-endian -mthumb -nostdlib \
  -fno-builtin -std=gnu11 -fmax-errors=1 -fsingle-precision-constant -Os -g
CPPFLAGS += -I. -Iboard/stm32h7/inc -I$(OPENDBC_INCLUDE) -include board/obj/packet_versions.h
ifeq ($(RELEASE),)
COMMON_FLAGS += -DALLOW_DEBUG $(if $(DEBUG),-DDEBUG)
endif
LINKER_SCRIPT := board/stm32h7/stm32h7x5_flash.ld
STARTUP := board/stm32h7/startup_stm32h7x5xx.s
PROJECTS := panda_h7 panda_jungle_h7 body_h7
panda_h7_MAIN := board/main.c
panda_jungle_h7_MAIN := board/jungle/main.c
body_h7_MAIN := board/body/main.c
panda_jungle_h7_FLAGS := -DPANDA_JUNGLE
body_h7_FLAGS := -DPANDA_BODY
GENERATED := board/obj/gitversion.h board/obj/cert.h board/obj/packet_versions.h board/obj/version

.PHONY: all firmware libpanda clean help FORCE metadata $(PROJECTS) board/obj
all: firmware libpanda compile_commands.json
firmware board/obj: $(PROJECTS)
libpanda: tests/libpanda/libpanda.so
FORCE:

# Write only changed metadata, so a no-op build leaves objects untouched.
metadata: FORCE
	$(Q)$(PYTHON) board/build.py generate $(BUILD_TYPE) "$(OPENDBC_INCLUDE)"
$(GENERATED): | metadata

board/obj/firmware.config: FORCE | metadata
	$(Q)$(PYTHON) board/build.py config $@ "$(ARM_CC) $(CPPFLAGS) $(COMMON_FLAGS) $(CFLAGS) $(LDFLAGS)" "$(OBJCOPY)"

board/obj/sign.config: FORCE | metadata
	$(Q)test -n "$(CERT)" && test -f "$(CERT)" || { echo 'Set CERT to an existing signing key for RELEASE builds.'; exit 1; }
	$(Q)$(PYTHON) board/build.py config $@ "$(CERT)"

# Each variant owns its objects, including its separately compiled bootstub.
define firmware_rules
$(1): board/obj/bootstub.$(1).bin board/obj/$(1).bin.signed
$(1)_OBJECTS := board/obj/$(1)/startup.o board/obj/$(1)/main.o \
  board/obj/$(1)/bootstub.o board/obj/$(1)/rsa.o board/obj/$(1)/sha.o
OBJECTS += $$($(1)_OBJECTS)
$$($(1)_OBJECTS): PRIVATE_FLAGS := $$($(1)_FLAGS)
$$($(1)_OBJECTS): $(GENERATED) board/obj/firmware.config Makefile board/build.mk board/build.py
board/obj/$(1)/bootstub.o board/obj/$(1)/rsa.o board/obj/$(1)/sha.o: PRIVATE_FLAGS += -DBOOTSTUB
board/obj/$(1)/startup.o: $(STARTUP)
board/obj/$(1)/main.o: $$($(1)_MAIN)
board/obj/$(1)/bootstub.o: board/bootstub.c
board/obj/$(1)/rsa.o: board/crypto/rsa.c
board/obj/$(1)/sha.o: board/crypto/sha.c
board/obj/$(1)/main.elf: board/obj/$(1)/startup.o board/obj/$(1)/main.o
board/obj/$(1)/main.elf: PRIVATE_FLAGS := $$($(1)_FLAGS) -Wl,--section-start,.isr_vector=0x8020000
board/obj/$(1)/bootstub.elf: board/obj/$(1)/startup.o board/obj/$(1)/rsa.o board/obj/$(1)/sha.o board/obj/$(1)/bootstub.o
board/obj/$(1)/bootstub.elf: PRIVATE_FLAGS := $$($(1)_FLAGS) -DBOOTSTUB
board/obj/bootstub.$(1).bin: board/obj/$(1)/bootstub.elf
	$$(Q)echo "  BIN     $$@"
	$$(Q)$$(OBJCOPY) -O binary $$< $$@
endef
$(foreach project,$(PROJECTS),$(eval $(call firmware_rules,$(project))))

$(OBJECTS):
	$(Q)mkdir -p $(@D)
	$(Q)echo "  CC      $@"
	$(Q)$(PYTHON) board/build.py compile $@ $(firstword $(filter %.c %.s,$^)) $(ARM_CC) $(CPPFLAGS) $(COMMON_FLAGS) $(CFLAGS) $(PRIVATE_FLAGS) -MMD -MP -c $(firstword $(filter %.c %.s,$^)) -o $@

board/obj/%.elf: $(LINKER_SCRIPT) board/obj/firmware.config
	$(Q)echo "  LINK    $@"
	$(Q)$(ARM_CC) $(COMMON_FLAGS) $(CFLAGS) $(PRIVATE_FLAGS) -T$(LINKER_SCRIPT) $(LDFLAGS) $(filter %.o,$^) -o $@

board/obj/%.bin: board/obj/%.elf
	$(Q)echo "  BIN     $@"
	$(Q)$(OBJCOPY) -O binary $< $@

board/obj/%.bin.signed: board/obj/%/main.bin board/crypto/sign.py board/obj/sign.config $(CERT)
	$(Q)echo "  SIGN    $@"
	$(Q)SETLEN=1 $(PYTHON) board/crypto/sign.py $< $@ "$(CERT)"

HOST_FLAGS := -nostdlib -fno-builtin -std=gnu11 -Wfatal-errors -Wno-pointer-to-int-cast -fPIC
HOST_INCLUDES := -I. -Iboard -Itests/libpanda -I$(OPENDBC_INCLUDE)
board/obj/host.config: FORCE
	$(Q)$(PYTHON) board/build.py config $@ "$(CC) $(HOST_FLAGS) $(HOST_INCLUDES) $(CFLAGS) $(LDFLAGS)"

tests/libpanda/panda.o: tests/libpanda/panda.c board/obj/host.config Makefile board/build.mk board/build.py
	$(Q)echo "  CC      $@"
	$(Q)$(PYTHON) board/build.py compile $@ $< $(CC) $(HOST_FLAGS) $(HOST_INCLUDES) $(CFLAGS) -MMD -MP -c $< -o $@

tests/libpanda/libpanda.so: tests/libpanda/panda.o
	$(Q)echo "  LINK    $@"
	$(Q)$(CC) -shared $(LDFLAGS) $< -o $@

compile_commands.json: $(OBJECTS) tests/libpanda/panda.o FORCE
	$(Q)$(PYTHON) board/build.py database $(OBJECTS) tests/libpanda/panda.o

clean:
	$(Q)rm -rf board/obj tests/libpanda/panda.o tests/libpanda/panda.d tests/libpanda/panda.o.json tests/libpanda/libpanda.so compile_commands.json

help:
	@echo 'make -j<N>       Build all firmware, libpanda, and compile_commands.json'
	@echo 'make firmware    Build all signed firmware and bootstubs'
	@echo 'make <variant>   Build panda_h7, panda_jungle_h7, or body_h7'
	@echo 'make libpanda    Build the host test library'
	@echo 'make clean       Remove generated build files'
	@echo 'Options: V=1, DEBUG=1, RELEASE=1 CERT=/path/to/key, CROSS_COMPILE=...'

-include $(OBJECTS:.o=.d) tests/libpanda/panda.d
