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

#Compiler options
CFLAGS		+= -Os -g -c -std=gnu11
CFLAGS		+= -Wall -Wextra -Werror -fms-extensions -Wno-unused-parameter -Wno-address-of-packed-member
CFLAGS		+= -fstack-usage -Wstack-usage=100
CFLAGS		+= -MMD -MP
CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
CFLAGS		+= $(CPU_DEFINES) --specs=picolibc.specs
CFLAGS		+= -DGIT_VERSION=\"[$(GIT_COMMITS)]-$(GIT_COMMIT)\" -DGIT_SHA1=\"$(GIT_SHA1)\"

BUILD_DIR := build

INCLUDE_PATHS += -Ilibs/libopencm3/include -Icore/include -Isensors/include

LINK_FLAGS =  -Llibs/libopencm3/lib --static -nostartfiles
LINK_FLAGS += -Llibs/libopencm3/lib/stm32/l4
LINK_FLAGS += -lopencm3_stm32l4
LINK_FLAGS += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group -Wl,--gc-sections
LINK_FLAGS += $(CPU_DEFINES) --specs=picolibc.specs


LIBOPENCM3 := libs/libopencm3/lib/libopencm3_stm32l4.a

BUILD_DIR := build

DEPS = $(shell find "$(BUILD_DIR)" -name "*.d")

FW_IMG := $(BUILD_DIR)/firmware.bin
BL_IMG := $(BUILD_DIR)/bootloader.bin
WHOLE_IMG := $(BUILD_DIR)/complete.bin

default: $(WHOLE_IMG)

$(LIBOPENCM3) :
	$(MAKE) -C libs/libopencm3 TARGETS=stm32/l4

$(BUILD_DIR)/.git.$(GIT_COMMIT): $(LIBOPENCM3)
	mkdir -p "$(@D)"
	rm -f $(BUILD_DIR)/.git.*
	touch $@

$(WHOLE_IMG) : $(BL_IMG) $(FW_IMG)
	dd of=$@ if=$(BL_IMG) bs=2k
	dd of=$@ if=$(FW_IMG) seek=2 conv=notrunc bs=2k

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


flash: $(WHOLE_IMG)
	openocd -f interface/stlink-v2-1.cfg \
		    -f target/stm32l4x.cfg \
		    -c "init" -c "halt" \
		    -c "program $(WHOLE_IMG) 0x08000000 verify reset exit" \
		    -c "reset" \
		    -c "shutdown"


clean:
	make -C libs/libopencm3 $@ TARGETS=stm32/l4
	rm -rf $(BUILD_DIR)

cppcheck:
	cppcheck --enable=all --std=c99 *.[ch]

debug_mon: $(WHOLE_IMG)
	openocd -f board/st_nucleo_l4.cfg -f interface/stlink-v2-1.cfg -c "init" -c "reset init"

debug_gdb: $(WHOLE_IMG)
	$(TOOLCHAIN)-gdb -ex "target remote localhost:3333" -ex load $(FW_IMG:%.bin=%.elf) -ex "monitor reset init"

size: $(WHOLE_IMG)
	$(SIZE) $(BUILD_DIR)/*.elf
	$(NM) -S --size-sort $(BUILD_DIR)/*.o


-include $(shell find "$(BUILD_DIR)" -name "*.d")
