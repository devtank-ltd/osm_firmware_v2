#Toolchain settings

CC = gcc
OBJCOPY = objcopy
OBJDUMP = objdump
SIZE = size
NM = nm

GIT_COMMITS := $(shell git rev-list --count HEAD)
GIT_COMMIT := $(shell git log -n 1 --format="%h-%f")
GIT_SHA1 := $(shell git log -n 1 --format="%h")

#Compiler options
CFLAGS		+= -O0 -g -c -std=gnu11 -pedantic
CFLAGS		+= -Wall -Wextra -Werror -fms-extensions -Wno-unused-parameter -Wno-address-of-packed-member
CFLAGS		+= -fstack-usage -Wstack-usage=500
CFLAGS		+= -MMD -MP
CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
CFLAGS		+= -DGIT_VERSION=\"[$(GIT_COMMITS)]-$(GIT_COMMIT)\" -DGIT_SHA1=\"$(GIT_SHA1)\"
CFLAGS		+= -g -fprofile-arcs -ftest-coverage

BUILD_DIR := build

INCLUDE_PATHS += -Icore/include -Isensors/include $(shell pkg-config --cflags json-c)

LINK_FLAGS = -Wl,--start-group -lc -lgcc -lm -Wl,--end-group -Wl,--gc-sections $(shell pkg-config --libs json-c)
LINK_FLAGS += -lgcov --coverage

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

firmware_SOURCES := \
           core/src/main.c \
           core/src/linux.c \
           core/src/base.c \
           core/src/log.c \
           core/src/uarts.c \
           core/src/uart_rings.c \
           core/src/cmd.c \
           core/src/io.c \
           core/src/ring.c \
           core/src/i2c.c \
           core/src/modbus_mem.c \
           core/src/persist_config.c \
           core/src/comms.c \
           core/src/measurements.c \
           core/src/measurements_mem.c \
           core/src/modbus_measurements.c \
           core/src/update.c \
           core/src/adcs.c \
           core/src/linux_adc.c \
           core/src/timers.c \
           core/src/common.c \
           core/src/platform_common.c \
           core/src/w1.c \
           core/src/sleep.c \
           core/src/can_comm.c \
           core/src/debug_mode.c \
           core/src/version.c \
           sensors/src/hpm.c \
           sensors/src/htu21d.c \
           sensors/src/modbus.c \
           sensors/src/ds18b20.c \
           sensors/src/pulsecount.c \
           sensors/src/veml7700.c \
           sensors/src/sai.c \
           sensors/src/cc.c \
           sensors/src/bat.c \
           sensors/src/can_impl.c \
           sensors/src/fw.c

firmware_OBJS=$(firmware_SOURCES:%.c=$(BUILD_DIR)/%.o)

$(BUILD_DIR)/firmware.elf : $(firmware_OBJS)
	$(CC) $(firmware_OBJS) $(LINK_FLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)

cppcheck:
	cppcheck --enable=all --std=c11 --inline-suppr `find core -name "*.[ch]"` `find sensors -name "*.[ch]"`  -I core/include -I sensors/include

size: $(WHOLE_IMG)
	$(SIZE) $(BUILD_DIR)/*.elf
	$(NM) -S --size-sort $(shell find $(BUILD_DIR) -name "*.o")


test: default
	mkdir -p /tmp/osm/
	../python/osm_test.py


coverage: default
	lcov --zerocounters -d $(BUILD_DIR)/
	lcov --capture --initial -d $(BUILD_DIR)/ --output-file $(BUILD_DIR)/coverage.info
	mkdir -p /tmp/osm/
	../python/osm_test.py
	lcov --capture -d $(BUILD_DIR)/ --output-file $(BUILD_DIR)/coverage.info
	mkdir -p $(BUILD_DIR)/coverage
	cd $(BUILD_DIR)/coverage && genhtml ../coverage.info
	sensible-browser $(BUILD_DIR)/coverage/index.html



-include $(shell find "$(BUILD_DIR)" -name "*.d")
