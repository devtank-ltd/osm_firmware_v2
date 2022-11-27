#Toolchain settings
STM_TOOLCHAIN := arm-none-eabi

STM_CC = $(STM_TOOLCHAIN)-gcc
STM_OBJCOPY = $(STM_TOOLCHAIN)-objcopy
STM_OBJDUMP = $(STM_TOOLCHAIN)-objdump
STM_SIZE = $(STM_TOOLCHAIN)-size
STM_NM =$(STM_TOOLCHAIN)-nm

#Target CPU options
STM_CPU_DEFINES = -mthumb -mcpu=cortex-m4 -DSTM32L4 -pedantic -mfloat-abi=hard -mfpu=fpv4-sp-d16

#Compiler options
STM_CFLAGS		:= -Os -g -std=gnu11
STM_CFLAGS		+= -Wall -Wextra -Werror -fms-extensions -Wno-unused-parameter -Wno-address-of-packed-member
STM_CFLAGS		+= -fstack-usage -Wstack-usage=100
STM_CFLAGS		+= -MMD -MP
STM_CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
STM_CFLAGS		+= $(STM_CPU_DEFINES) --specs=picolibc.specs
STM_CFLAGS		+= -DGIT_VERSION=\"[$(GIT_COMMITS)]-$(GIT_COMMIT)\" -DGIT_SHA1=\"$(GIT_SHA1)\"
STM_CFLAGS		+= 
STM_INCLUDE_PATHS += -I$(OSM_DIR)/ports/stm/include -I$(OSM_DIR)/libs/libopencm3/include -I$(OSM_DIR)/core/include -I$(OSM_DIR)/sensors/include -I$(OSM_DIR)/comms/include

STM_LINK_FLAGS =  -L$(OSM_DIR)/libs/libopencm3/lib --static -nostartfiles
STM_LINK_FLAGS += -L$(OSM_DIR)/libs/libopencm3/lib/stm32/l4
STM_LINK_FLAGS += -lopencm3_stm32l4
STM_LINK_FLAGS += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group -Wl,--gc-sections
STM_LINK_FLAGS += $(STM_CPU_DEFINES) --specs=picolibc.specs


LIBOPENCM3 := $(OSM_DIR)/libs/libopencm3/lib/libopencm3_stm32l4.a

$(LIBOPENCM3) :
	$(MAKE) -C $(OSM_DIR)/libs/libopencm3 TARGETS=stm32/l4

define STM_FIRMWARE
$(1)_UP_NAME=$$(shell echo $(1) | tr a-z A-Z)
$(1)_OBJS=$$($(1)_SOURCES:%.c=$$(BUILD_DIR)/$(1)/%.o)

$$(BUILD_DIR)/$(1)/%.o: $$(OSM_DIR)/%.c $$(BUILD_DIR)/.git.$$(GIT_COMMIT)
	mkdir -p "$$(@D)"
	$$(STM_CC) -c -Dfw_name=$(1) -DFW_NAME=$$($(1)_UP_NAME) $$(STM_CFLAGS) -I$$(OSM_DIR)/model/$(1) $$(STM_INCLUDE_PATHS) $$< -o $$@

$$(BUILD_DIR)/$(1)/firmware.elf: $$($(1)_OBJS) $$(OSM_DIR)/$$($(1)_LINK_SCRIPT)
	$$(STM_CC) $$($(1)_OBJS) $$(STM_LINK_FLAGS) -T$$(OSM_DIR)/$$($(1)_LINK_SCRIPT) -o $$@

$$(BUILD_DIR)/$(1)/config.bin : $$(JSON_CONV_DIR)/$(1)_default_mem.json $$(JSON_CONV)
	$$(JSON_CONV) $$@ < $$<

$$(BUILD_DIR)/$(1)/bootloader/%.o : $$(OSM_DIR)/ports/stm/bootloader/%.c $$(LIBOPENCM3)
	mkdir -p "$$(@D)"
	$$(STM_CC) -c -Dfw_name=$(1) -DFW_NAME=$$($(1)_UP_NAME) $$(STM_CFLAGS) -I$$(OSM_DIR)/model/$(1) $$(STM_INCLUDE_PATHS) $$< -o $$@

$$(BUILD_DIR)/$(1)/bootloader.elf : $$(BUILD_DIR)/$(1)/bootloader/bootloader.o
	$$(STM_CC) $$< $$(STM_LINK_FLAGS) -T$$(OSM_DIR)/$$($(1)_LINK_SCRIPT) -o $$@

$$(BUILD_DIR)/$(1)/%.bin: $$(BUILD_DIR)/$(1)/%.elf
	$$(STM_OBJCOPY) -O binary $$< $$@

$$(BUILD_DIR)/$(1)/complete.bin: $$(BUILD_DIR)/$(1)/bootloader.bin $$(BUILD_DIR)/$(1)/firmware.bin $$(BUILD_DIR)/$(1)/config.bin
	dd of=$$@ if=$$(BUILD_DIR)/$(1)/bootloader.bin bs=2k
	dd of=$$@ if=$$(BUILD_DIR)/$(1)/config.bin seek=2 conv=notrunc bs=2k
	dd of=$$@ if=$$(BUILD_DIR)/$(1)/firmware.bin seek=4 conv=notrunc bs=2k

$$(BUILD_DIR)/$(1)/.complete: $$(BUILD_DIR)/$(1)/complete.bin
	touch $$@

$(1)_serial_program: $$(BUILD_DIR)/$(1)/complete.bin
	KEEPCONFIG=1 ./tools/config_scripts/program.sh $$<

$(1)_flash: $$(BUILD_DIR)/$(1)/bootloader.bin $$(BUILD_DIR)/$(1)/firmware.bin
	openocd -f interface/stlink-v2-1.cfg \
	        -f target/stm32l4x.cfg \
	        -c "init" -c "halt" \
	        -c "program $$(BUILD_DIR)/$(1)/bootloader.bin 0x08000000 verify reset exit" \
	        -c "program $$(BUILD_DIR)/$(1)/firmware.bin 0x08002000 verify reset exit" \
	        -c "reset" \
	        -c "shutdown"


$(1)_debug_mon: $$(BUILD_DIR)/$(1)/complete.bin
	openocd -f board/st_nucleo_l4.cfg -f interface/stlink-v2-1.cfg -c "init" -c "reset init"

$(1)_debug_gdb: $$(BUILD_DIR)/$(1)/complete.bin
	gdb-multiarch -ex "target remote localhost:3333" -ex load $$(BUILD_DIR)/$(1)/firmware.elf  -ex "monitor reset init"

$(1)_size: $$(BUILD_DIR)/$(1)/complete.bin
	$$(STM_SIZE) $$(BUILD_DIR)/$(1)/*.elf
	$$(STM_NM) -S --size-sort $$(shell find $$(BUILD_DIR)/$(1) -name "*.o")

$(1)_release_bundle : $$(BUILD_DIR)/$(1)/complete.bin
	@if [ -n "$$( echo \'"$$(GIT_TAG)"\' | sed -n '/release/p')" ]; then \
	  mkdir -p $$(RELEASE_DIR)/$(1)/$$(RELEASE_NAME); \
	  cp -r $$(BUILD_DIR)/$(1)/complete.bin $$(JSON_CONV) $$(OSM_DIR)/tools/config_scripts $$(RELEASE_DIR)/$(1)/$$(RELEASE_NAME)/; \
	  cd $$(RELEASE_DIR)/$(1); \
	  tar -czf $$(RELEASE_NAME).tar.gz $$(RELEASE_NAME); \
	  rm -r $$(RELEASE_NAME); \
	  echo "Made release bundle $$(RELEASE_DIR)/$(1)/$$(RELEASE_NAME).tar.gz";\
	else \
	  echo "NO RELEASE TAG CHECKED OUT"; \
	fi
endef
