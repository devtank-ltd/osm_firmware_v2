#Toolchain settings

CC = gcc
OBJCOPY = objcopy
OBJDUMP = objdump
SIZE = size
NM = nm

GIT_COMMITS := $(shell git rev-list --count HEAD)
GIT_COMMIT := $(shell git log -n 1 --format="%h-%f")
GIT_SHA1 := $(shell git log -n 1 --format="%h")

DEV ?= STM32L451RE

#Compiler options
CFLAGS		+= -Os -g -c -std=gnu11
CFLAGS		+= -Wall -Wextra -Werror -fms-extensions -Wno-unused-parameter -Wno-address-of-packed-member
CFLAGS		+= -fstack-usage -Wstack-usage=100
CFLAGS		+= -MMD -MP
CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
CFLAGS		+= -DGIT_VERSION=\"[$(GIT_COMMITS)]-$(GIT_COMMIT)\" -DGIT_SHA1=\"$(GIT_SHA1)\" -D$(DEV)

BUILD_DIR := build

INCLUDE_PATHS += -Icore/include -Isensors/include

LINK_FLAGS = -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group -Wl,--gc-sections

BUILD_DIR := build

DEPS = $(shell find "$(BUILD_DIR)" -name "*.d")

default: $(BUILD_DIR)/firmware.elf

$(BUILD_DIR)/.git.$(GIT_COMMIT):
	mkdir -p "$(@D)"
	rm -f $(BUILD_DIR)/.git.*
	touch $@

$(BUILD_DIR)/%.o: %.c $(BUILD_DIR)/.git.$(GIT_COMMIT)
	mkdir -p "$(@D)"
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $< -o $@


define PROGRAM_template
  include $(1)
  $(2)_OBJS=$$($(2)_SOURCES:%.c=$(BUILD_DIR)/%.o)
  $(info $(BUILD_DIR)/$(2).elf)
  $(BUILD_DIR)/$(2).elf: $$($(2)_OBJS) $$($(2)_LINK_SCRIPT)
	$(CC) $$($(2)_OBJS) $$(LINK_FLAGS) -T$$($(2)_LINK_SCRIPT) -o $$@
endef

$(eval $(call PROGRAM_template,../firmware.mk,firmware))

clean:
	rm -rf $(BUILD_DIR)

cppcheck:
	cppcheck --enable=all --std=c11 --inline-suppr `find core -name "*.[ch]"` `find sensors -name "*.[ch]"`  -I core/include -I sensors/include

size: $(WHOLE_IMG)
	$(SIZE) $(BUILD_DIR)/*.elf
	$(NM) -S --size-sort $(shell find $(BUILD_DIR) -name "*.o")


-include $(shell find "$(BUILD_DIR)" -name "*.d")
