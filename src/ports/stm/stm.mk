#Toolchain settings
STM_TOOLCHAIN := arm-none-eabi

STM_CC = $(STM_TOOLCHAIN)-gcc
STM_OBJCOPY = $(STM_TOOLCHAIN)-objcopy
STM_OBJDUMP = $(STM_TOOLCHAIN)-objdump
STM_SIZE = $(STM_TOOLCHAIN)-size
STM_NM =$(STM_TOOLCHAIN)-nm

#Target CPU options
STM_DEFINES = -DSTM32L4
STM_CPU_DEFINES = -mthumb -mcpu=cortex-m4 -pedantic -mfloat-abi=hard -mfpu=fpv4-sp-d16

#Compiler options
STM_CFLAGS		:= -Os -g -std=gnu11 $(STM_DEFINES)
STM_CFLAGS		+= -Wall -Wextra -Werror -Wno-unused-parameter -Wno-address-of-packed-member -Wno-variadic-macros
STM_CFLAGS		+= -fstack-usage -Wstack-usage=100
STM_CFLAGS		+= -MMD -MP
STM_CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
STM_CFLAGS		+= $(STM_CPU_DEFINES) --specs=picolibc.specs -flto

STM_INCLUDE_PATHS += -I$(OSM_DIR)/include -I$(OSM_DIR)/include/osm/ports/stm -I$(OSM_DIR)/libs/libopencm3/include

STM_LINK_FLAGS =  -L$(OSM_DIR)/libs/libopencm3/lib --static -nostartfiles
STM_LINK_FLAGS += -L$(OSM_DIR)/libs/libopencm3/lib/stm32/l4
STM_LINK_FLAGS += -lopencm3_stm32l4
STM_LINK_FLAGS += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group -Wl,--gc-sections -flto
STM_LINK_FLAGS += $(STM_CPU_DEFINES) --specs=picolibc.specs

#STM Port Dependencies

STM_EXES := stm32flash arm-none-eabi-gcc pkg-config git
STM_PATHS := /usr/lib/picolibc/arm-none-eabi/picolibc.specs /usr/local/lib/picolibc/arm-none-eabi/picolibc.spec /usr/lib/gcc/arm-none-eabi/`arm-none-eabi-gcc  -dumpversion`/picolibc.specs /usr/local/lib/gcc/arm-none-eabi/`arm-none-eabi-gcc  -dumpversion`/picolibc.specs
STM_PKGS := json-c

STM_DBG_EXES := openocd gdb-multiarch

LIBOPENCM3 := $(OSM_DIR)/libs/libopencm3/lib/libopencm3_stm32l4.a

$(LIBOPENCM3) : $(OSM_BUILD_DIR)/.stm_build_env
	$(MAKE) -C $(OSM_DIR)/libs/libopencm3 TARGETS=stm32/l4

$(OSM_BUILD_DIR)/.stm_build_env:
	mkdir -p $(OSM_BUILD_DIR)
	FOUND=0; for P in $(STM_PATHS); do \
		if [ -e $$P ]; then FOUND=1; break; fi \
	done; \
	if [ "$$FOUND" = "0" ]; then echo "NO PICOLIB FOUND"; exit 1; fi
	for i in $(STM_EXES) ; do \
		if ! which $$i; then echo MISSING EXECUTABLE: $$i; exit 1; fi; \
	done
	for p in $(STM_PKGS) ; do \
		if ! pkg-config --cflags $$p; then echo MISSING PKF-CONFIG: $$p; exit 1; fi; \
	done
	touch $@

$(BUILD)/.stm_dbg:
	for i in $(STM_DBG_EXES) ; do \
		if ! which $$i; then echo MISSING EXECUTABLE: $$i; exit 1; fi; \
	done

define STM_FIRMWARE
$(call PORT_BASE_RULES,$(1),STM)

$$($(1)_OBJS): $(LIBOPENCM3)

$$(OSM_BUILD_DIR)/$(1)/firmware.elf: $$($(1)_OBJS) $$($(1)_LINK_SCRIPT)
	$$(STM_CC) $$($(1)_OBJS) $$(STM_LINK_FLAGS) -T$$($(1)_LINK_SCRIPT) -o $$@


$$(OSM_BUILD_DIR)/$(1)/bootloader/%.o : $$(OSM_DIR)/src/ports/stm/bootloader/%.c $$(LIBOPENCM3)
	mkdir -p "$$(@D)"
	$$(STM_CC) -c -Dfw_name=$(1) -DFW_NAME=$$($(1)_UP_NAME) $$(STM_CFLAGS) -I$$(OSM_MODEL_DIR)/$(1) $$(STM_INCLUDE_PATHS) $$< -o $$@

$$(OSM_BUILD_DIR)/$(1)/bootloader.elf : $$(OSM_BUILD_DIR)/$(1)/bootloader/bootloader.o
	echo $$(bootloader_LINK_SCRIPT)
	$$(STM_CC) $$< $$(STM_LINK_FLAGS) -T$$(OSM_DIR)/src/ports/stm/bootloader/stm32l4.ld -o $$@

$$(OSM_BUILD_DIR)/$(1)/%.bin: $$(OSM_BUILD_DIR)/$(1)/%.elf
	$$(STM_OBJCOPY) -O binary $$< $$@

$$(OSM_BUILD_DIR)/$(1)/complete.bin: $$(OSM_BUILD_DIR)/$(1)/bootloader.bin $$(OSM_BUILD_DIR)/$(1)/firmware.bin
	dd of=$$@ if=$$(OSM_BUILD_DIR)/$(1)/bootloader.bin bs=2k
	dd of=$$@ if=/dev/zero seek=2 conv=notrunc bs=2k count=2
	dd of=$$@ if=$$(OSM_BUILD_DIR)/$(1)/firmware.bin seek=4 conv=notrunc bs=2k

$$(OSM_BUILD_DIR)/$(1)/.complete: $$(OSM_BUILD_DIR)/$(1)/complete.bin
	touch $$@

$(1): $$(OSM_BUILD_DIR)/$(1)/.complete

$(1)_serial_program: $$(OSM_BUILD_DIR)/$(1)/complete.bin
	KEEPCONFIG=1 $(OSM_DIR)/tools/config_scripts/program.sh $$<

$(1)_flash: $$(OSM_BUILD_DIR)/$(1)/bootloader.bin $$(OSM_BUILD_DIR)/$(1)/firmware.bin
	openocd -f interface/stlink-v2-1.cfg \
	        -f target/stm32l4x.cfg \
	        -c "init" -c "reset init" \
	        -c "program $$(OSM_BUILD_DIR)/$(1)/bootloader.bin 0x08000000 verify reset" \
	        -c "program $$(OSM_BUILD_DIR)/$(1)/firmware.bin 0x08002000 verify reset exit" \
	        -c "reset" \
	        -c "shutdown"


$(1)_debug_mon: $(BUILD)/.stm_dbg $$(OSM_BUILD_DIR)/$(1)/complete.bin
	openocd -f board/st_nucleo_l4.cfg -f interface/stlink-v2-1.cfg -c "init" -c "reset init"

$(1)_debug_gdb: $(BUILD)/.stm_dbg $$(OSM_BUILD_DIR)/$(1)/complete.bin
	gdb-multiarch -ex "target remote localhost:3333" -ex load $$(OSM_BUILD_DIR)/$(1)/firmware.elf  -ex "monitor reset init"

$(1)_size: $$(OSM_BUILD_DIR)/$(1)/complete.bin
	$$(STM_SIZE) $$(OSM_BUILD_DIR)/$(1)/*.elf
	$$(STM_NM) -S --size-sort $$(shell find $$(OSM_BUILD_DIR)/$(1) -name "*.o")

$$(OSM_RELEASE_DIR)/$(1)_$$(OSM_RELEASE_NAME).tar.gz : $$(OSM_BUILD_DIR)/$(1)/complete.bin
	git_tag=$(OSM_GIT_TAG); \
	if [ "$$$${git_tag#*release}" != "$(OSM_GIT_TAG)" ]; then \
	  mkdir -p $$(OSM_RELEASE_DIR)/$(1)_$$(OSM_RELEASE_NAME); \
	  cp -r $$(OSM_BUILD_DIR)/$(1)/complete.bin $$(OSM_RELEASE_DIR)/$(1)_$$(OSM_RELEASE_NAME)/$(1)_$$(OSM_RELEASE_NAME).bin; \
	  cp -r $$(OSM_DIR)/tools/config_scripts/* $$(OSM_RELEASE_DIR)/$(1)_$$(OSM_RELEASE_NAME)/; \
	  cp -r $$(OSM_DIR)/tools/json_config_tool/json_config.py $$(OSM_RELEASE_DIR)/$(1)_$$(OSM_RELEASE_NAME)/; \
	  cp -r $$(OSM_DIR)/python/binding.py $$(OSM_RELEASE_DIR)/$(1)_$$(OSM_RELEASE_NAME)/; \
	  cd $$(OSM_RELEASE_DIR); \
	  tar -czf $(1)_$$(OSM_RELEASE_NAME).tar.gz $(1)_$$(OSM_RELEASE_NAME); \
	  rm -r $(1)_$$(OSM_RELEASE_NAME); \
	  echo "Made release bundle $$(OSM_RELEASE_DIR)/$(1)_$$(OSM_RELEASE_NAME).tar.gz";\
	else \
	  echo "NO RELEASE TAG CHECKED OUT"; \
	fi

$(1)_release_bundle : $$(OSM_RELEASE_DIR)/$(1)_$$(OSM_RELEASE_NAME).tar.gz

endef
