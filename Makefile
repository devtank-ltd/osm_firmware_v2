#Toolchain settings
TOOLCHAIN := arm-none-eabi

CC = $(TOOLCHAIN)-gcc
OBJCOPY = $(TOOLCHAIN)-objcopy
OBJDUMP = $(TOOLCHAIN)-objdump
SIZE = $(TOOLCHAIN)-size
NM =$(TOOLCHAIN)-nm

#Target CPU options
CPU_DEFINES = -mthumb -mcpu=cortex-m4 -DSTM32L4 -pedantic -mfloat-abi=hard -mfpu=fpv4-sp-d16

GIT_COMMITS := $(shell git rev-list --count HEAD)
GIT_COMMIT := $(shell git log -n 1 --format="%h-%f")
GIT_SHA1 := $(shell git log -n 1 --format="%h")
GIT_TAG ?= $(shell git tag --points-at HEAD)

#Compiler options
CFLAGS		+= -Os -g -c -std=gnu11
CFLAGS		+= -Wall -Wextra -Werror -fms-extensions -Wno-unused-parameter -Wno-address-of-packed-member
CFLAGS		+= -fstack-usage -Wstack-usage=100
CFLAGS		+= -MMD -MP
CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
CFLAGS		+= $(CPU_DEFINES) --specs=picolibc.specs
CFLAGS		+= -DGIT_VERSION=\"[$(GIT_COMMITS)]-$(GIT_COMMIT)\" -DGIT_SHA1=\"$(GIT_SHA1)\"
CFLAGS		+= -Dfw_name=$(FW_NAME)

FW_NAME ?= sens01


INCLUDE_PATHS += -Ilibs/libopencm3/include -Icore/include -Isensors/include -Icomms/include -Imodel/$(FW_NAME)

LINK_FLAGS =  -Llibs/libopencm3/lib --static -nostartfiles
LINK_FLAGS += -Llibs/libopencm3/lib/stm32/l4
LINK_FLAGS += -lopencm3_stm32l4
LINK_FLAGS += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group -Wl,--gc-sections
LINK_FLAGS += $(CPU_DEFINES) --specs=picolibc.specs


LIBOPENCM3 := libs/libopencm3/lib/libopencm3_stm32l4.a

BUILD_DIR := build/$(FW_NAME)
RELEASE_DIR := releases
JSON_CONV_DIR := tools/img_json_interpretter

MEM_JSON := $(JSON_CONV_DIR)/$(FW_NAME)_default_mem.json

RELEASE_NAME := $(GIT_TAG)_release_bundle

DEPS = $(shell find "$(BUILD_DIR)" -name "*.d")

FW_IMG := $(BUILD_DIR)/$(FW_NAME).bin
BL_IMG := $(BUILD_DIR)/bootloader.bin
WHOLE_IMG := $(BUILD_DIR)/complete.bin
JSON_CONV := $(JSON_CONV_DIR)/build/json_x_img
MEM_IMG := $(BUILD_DIR)/$(FW_NAME)_mem.bin

default: $(WHOLE_IMG)

$(JSON_CONV) : $(LIBOPENCM3)
	$(MAKE) -C $(JSON_CONV_DIR)

$(LIBOPENCM3) :
	$(MAKE) -C libs/libopencm3 TARGETS=stm32/l4

$(BUILD_DIR)/.git.$(GIT_COMMIT): $(LIBOPENCM3)
	mkdir -p "$(@D)"
	rm -f $(BUILD_DIR)/.git.*
	touch $@

$(MEM_IMG) : $(MEM_JSON) $(JSON_CONV)
	$(JSON_CONV) $@ < $(MEM_JSON)

$(WHOLE_IMG) : $(BL_IMG) $(FW_IMG) $(MEM_IMG)
	dd of=$@ if=$(BL_IMG) bs=2k
	dd of=$@ if=$(MEM_IMG) seek=2 conv=notrunc bs=2k
	dd of=$@ if=$(FW_IMG)  seek=4 conv=notrunc bs=2k

$(BUILD_DIR)/%.o: %.c $(BUILD_DIR)/.git.$(GIT_COMMIT)
	mkdir -p "$(@D)"
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $< -o $@


define PROGRAM_template
  include $(1)
  $(2)_OBJS=$$($(2)_SOURCES:%.c=$(BUILD_DIR)/%.o)
  $(BUILD_DIR)/$(2).elf: $$($(2)_OBJS) $$($(2)_LINK_SCRIPT)
	$(CC) $$($(2)_OBJS) $$(LINK_FLAGS) -T$$($(2)_LINK_SCRIPT) -o $$@
endef

PROGRAMS_MKS = $(shell find . -maxdepth 1 -name "*.mk" -printf "%f\n")

# To see what it's doing, just replace "eval" with "info"
$(foreach file_mk,$(PROGRAMS_MKS),$(eval $(call PROGRAM_template,$(file_mk),$(file_mk:%.mk=%))))


%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

serial_program: $(WHOLE_IMG)
	KEEPCONFIG=1 ./tools/config_scripts/program.sh $<


flash: $(BL_IMG) $(FW_IMG)
	openocd -f interface/stlink-v2-1.cfg \
		    -f target/stm32l4x.cfg \
		    -c "init" -c "halt" \
		    -c "program $(BL_IMG) 0x08000000 verify reset exit" \
		    -c "program $(FW_IMG) 0x08002000 verify reset exit" \
		    -c "reset" \
		    -c "shutdown"


clean:
	make -C libs/libopencm3 $@ TARGETS=stm32/l4
	make -C $(JSON_CONV_DIR) $@
	rm -rf $(BUILD_DIR)
	rm -rf $(RELEASE_DIR)

cppcheck:
	cppcheck --enable=all --std=c11 --inline-suppr `find core -name "*.[ch]"` `find sensors -name "*.[ch]"`  -I core/include -I sensors/include

debug_mon: $(WHOLE_IMG)
	openocd -f board/st_nucleo_l4.cfg -f interface/stlink-v2-1.cfg -c "init" -c "reset init"

debug_gdb: $(WHOLE_IMG)
	gdb-multiarch -ex "target remote localhost:3333" -ex load $(FW_IMG:%.bin=%.elf) -ex "monitor reset init"

size: $(WHOLE_IMG)
	$(SIZE) $(BUILD_DIR)/*.elf
	$(NM) -S --size-sort $(shell find $(BUILD_DIR) -name "*.o")

release_bundle : default
	@if [ -n "$$( echo \'"$(GIT_TAG)"\' | sed -n '/release/p')" ]; then \
	  mkdir -p $(RELEASE_DIR); \
	  cd $(RELEASE_DIR); \
	  mkdir -p $(RELEASE_NAME); \
	  cp -r ../$(WHOLE_IMG) ../$(JSON_CONV) ../tools/config_scripts $(RELEASE_NAME); \
	  tar -czf $(RELEASE_NAME).tar.gz $(RELEASE_NAME); \
	  rm -r $(RELEASE_NAME); \
	  echo "Made release bundle $(RELEASE_DIR)/$(RELEASE_NAME).tar.gz"; \
	else \
	  echo "NO RELEASE TAG CHECKED OUT"; \
	fi \

-include $(shell find "$(BUILD_DIR)" -name "*.d")
