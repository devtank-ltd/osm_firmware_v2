#Toolchain settings

CC = gcc
OBJCOPY = objcopy
OBJDUMP = objdump

BUILD_DIR ?= build

GIT_COMMITS := $(shell git rev-list --count HEAD)
GIT_COMMIT := $(shell git log -n 1 --format="%h-%f")
GIT_SHA1 := $(shell git log -n 1 --format="%h")

OSM_DIR ?= ..

CORE_DIR ?= $(OSM_DIR)/core
SENS_DIR ?= $(OSM_DIR)/sensors
COMM_DIR ?= $(OSM_DIR)/comms
MODL_DIR ?= $(OSM_DIR)/model
LINU_DIR ?= ./

#Compiler options
CFLAGS		+= -O0 -g -c -std=gnu11 -pedantic
CFLAGS		+= -Wall -Wextra -Werror -fms-extensions -Wno-unused-parameter -Wno-address-of-packed-member
CFLAGS		+= -fstack-usage -Wstack-usage=500
CFLAGS		+= -MMD -MP
CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
CFLAGS		+= -DGIT_VERSION=\"[$(GIT_COMMITS)]-$(GIT_COMMIT)\" -DGIT_SHA1=\"$(GIT_SHA1)\" -D__LINUX__
CFLAGS		+= -g -fprofile-arcs -ftest-coverage


INCLUDE_PATHS += -I$(LINU_DIR)/include -I$(CORE_DIR)/include -I$(SENS_DIR)/include -I$(COMM_DIR)/include $(shell pkg-config --cflags json-c)

LINK_FLAGS = -Wl,--start-group -lc -lgcc -lm -Wl,--end-group -Wl,--gc-sections $(shell pkg-config --libs json-c)
LINK_FLAGS += -lgcov --coverage -pthread -lutil


SQLITE_DB = $(OSM_DIR)/config_gui/release/config_database/modbus_templates


default: $(BUILD_DIR)/firmware.elf

$(BUILD_DIR)/.git.$(GIT_COMMIT):
	mkdir -p "$(@D)"
	rm -f $(BUILD_DIR)/.git.*
	touch $@

LINUX_SRC := \
    $(LINU_DIR)/src/uarts.c \
    $(LINU_DIR)/src/linux_adc.c \
    $(LINU_DIR)/src/sleep.c \
    $(LINU_DIR)/src/platform_common.c \
    $(LINU_DIR)/src/can_comm.c \
    $(LINU_DIR)/src/linux.c \
    $(LINU_DIR)/src/timers.c \
    $(LINU_DIR)/src/i2c.c \
    $(LINU_DIR)/src/w1.c \
    $(LINU_DIR)/src/version.c \
    $(LINU_DIR)/src/pulsecount.c \
    $(LINU_DIR)/src/sai.c \
    $(LINU_DIR)/src/comms.c


OSM_SRC := $(OSM_DIR)/core/src/main.c \
           $(OSM_DIR)/core/src/base.c \
           $(OSM_DIR)/core/src/log.c \
           $(OSM_DIR)/core/src/uart_rings.c \
           $(OSM_DIR)/core/src/cmd.c \
           $(OSM_DIR)/core/src/io.c \
           $(OSM_DIR)/core/src/ring.c \
           $(OSM_DIR)/core/src/modbus.c \
           $(OSM_DIR)/core/src/modbus_mem.c \
           $(OSM_DIR)/core/src/persist_config.c \
           $(OSM_DIR)/core/src/measurements.c \
           $(OSM_DIR)/core/src/measurements_mem.c \
           $(OSM_DIR)/core/src/modbus_measurements.c \
           $(OSM_DIR)/core/src/update.c \
           $(OSM_DIR)/core/src/adcs.c \
           $(OSM_DIR)/core/src/common.c \
           $(OSM_DIR)/core/src/debug_mode.c \
           $(OSM_DIR)/sensors/src/hpm.c \
           $(OSM_DIR)/sensors/src/htu21d.c \
           $(OSM_DIR)/sensors/src/ds18b20.c \
           $(OSM_DIR)/sensors/src/veml7700.c \
           $(OSM_DIR)/sensors/src/ftma.c \
           $(OSM_DIR)/sensors/src/bat.c \
           $(OSM_DIR)/sensors/src/cc.c \
           $(OSM_DIR)/sensors/src/can_impl.c \
           $(OSM_DIR)/sensors/src/fw.c

LINUX_OBJS=$(LINUX_SRC:%.c=$(BUILD_DIR)/%.o)

OSM_OBJS:=$(OSM_SRC:$(OSM_DIR)/%.c=$(BUILD_DIR)/%.o)

OBJS := $(LINUX_OBJS) $(OSM_OBJS)

DEPS := $(OBJS:%.o=%.d)

$(OSM_OBJS): $(BUILD_DIR)/%.o: $(OSM_DIR)/%.c $(BUILD_DIR)/.git.$(GIT_COMMIT)
	mkdir -p "$(@D)"
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $< -o $@

$(LINUX_OBJS): $(BUILD_DIR)/%.o: $(LINU_DIR)/%.c $(BUILD_DIR)/.git.$(GIT_COMMIT)
	mkdir -p "$(@D)"
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $< -o $@

$(BUILD_DIR)/firmware.elf : $(OBJS)
	$(CC) $(OBJS) $(LINK_FLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)

cppcheck:
	cppcheck --enable=all --std=c11 --inline-suppr `find $(CORE_DIR) "*.[ch]"` `find $(SENS_DIR) -name "*.[ch]"`  -I $(CORE_DIR)/include -I $(SENS_DIR)/include

test: default
	mkdir -p /tmp/osm/
	$(OSM_DIR)/python/osm_test.py


coverage: default
	lcov --zerocounters -d $(BUILD_DIR)/
	lcov --capture --initial -d $(BUILD_DIR)/ --output-file $(BUILD_DIR)/coverage.info
	mkdir -p /tmp/osm/
	$(OSM_DIR)/python/osm_test.py
	lcov --capture -d $(BUILD_DIR)/ --output-file $(BUILD_DIR)/coverage.info
	mkdir -p $(BUILD_DIR)/coverage
	cd $(BUILD_DIR)/coverage && genhtml ../coverage.info
	sensible-browser $(BUILD_DIR)/coverage/index.html


run: default
	mkdir -p /tmp/osm/
	$(OSM_DIR)/python/osm_test.py --run


soak: default
	loop=0; while [ "$$?" = "0" ]; do loop=$$(($$loop + 1));make test; done; date; echo "Loops:" $$loop


valgrind: default
	valgrind --leak-check=full $(BUILD_DIR)/firmware.elf


-include $(DEPS)
