#Toolchain settings

LINXDIR := $(dir $(lastword $(MAKEFILE_LIST)))

CC = gcc
OBJCOPY = objcopy
OBJDUMP = objdump

OSM_DIR ?= $(LINXDIR)/..
BUILD_DIR ?= build
LINX_BUILD_DIR ?= $(BUILD_DIR)/linux

ifndef GIT_COMMITS
GIT_COMMITS := $(shell git rev-list --count HEAD)
GIT_COMMIT := $(shell git log -n 1 --format="%h-%f")
GIT_SHA1 := $(shell git log -n 1 --format="%h")
endif

CORE_DIR ?= $(OSM_DIR)/core
SENS_DIR ?= $(OSM_DIR)/sensors
COMM_DIR ?= $(OSM_DIR)/comms
MODL_DIR ?= $(OSM_DIR)/model

#Compiler options
LINX_CFLAGS		+= -O0 -g -c -std=gnu11 -pedantic
LINX_CFLAGS		+= -Wall -Wextra -Werror -fms-extensions -Wno-unused-parameter -Wno-address-of-packed-member
LINX_CFLAGS		+= -fstack-usage -Wstack-usage=500
LINX_CFLAGS		+= -MMD -MP
LINX_CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
LINX_CFLAGS		+= -DGIT_VERSION=\"[$(GIT_COMMITS)]-$(GIT_COMMIT)\" -DGIT_SHA1=\"$(GIT_SHA1)\" -D__LINUX__
LINX_CFLAGS		+= -g -fprofile-arcs -ftest-coverage


LINX_INCLUDE_PATHS += -I$(LINXDIR)/include -I$(CORE_DIR)/include -I$(SENS_DIR)/include -I$(COMM_DIR)/include $(shell pkg-config --cflags json-c)

LINX_LDFLAGS = -Wl,--start-group -lc -lgcc -lm -Wl,--end-group -Wl,--gc-sections $(shell pkg-config --libs json-c)
LINX_LDFLAGS += -lgcov --coverage -pthread -lutil


SQLITE_DB = $(OSM_DIR)/config_gui/release/config_database/modbus_templates


default: $(LINX_BUILD_DIR)/firmware.elf

$(LINX_BUILD_DIR)/.git.$(GIT_COMMIT):
	mkdir -p "$(@D)"
	rm -f $(LINX_BUILD_DIR)/.git.*
	touch $@

LINUX_SRC := \
    $(LINXDIR)/src/uarts.c \
    $(LINXDIR)/src/linux_adc.c \
    $(LINXDIR)/src/sleep.c \
    $(LINXDIR)/src/platform_common.c \
    $(LINXDIR)/src/can_comm.c \
    $(LINXDIR)/src/linux.c \
    $(LINXDIR)/src/timers.c \
    $(LINXDIR)/src/i2c.c \
    $(LINXDIR)/src/w1.c \
    $(LINXDIR)/src/version.c \
    $(LINXDIR)/src/pulsecount.c \
    $(LINXDIR)/src/sai.c \
    $(LINXDIR)/src/comms.c


LINX_OSM_SRC := \
           $(OSM_DIR)/core/src/main.c \
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

LINUX_OBJS=$(LINUX_SRC:$(LINXDIR)/%.c=$(LINX_BUILD_DIR)/%.o)

LINX_OSM_OBJS:=$(LINX_OSM_SRC:$(OSM_DIR)/%.c=$(LINX_BUILD_DIR)/%.o)

LINX_OBJS := $(LINUX_OBJS) $(LINX_OSM_OBJS)
LINX_DEPS := $(LINX_OBJS:%.o=%.d)

$(LINX_OSM_OBJS): $(LINX_BUILD_DIR)/%.o: $(OSM_DIR)/%.c $(LINX_BUILD_DIR)/.git.$(GIT_COMMIT)
	mkdir -p "$(@D)"
	$(CC) $(LINX_CFLAGS) $(LINX_INCLUDE_PATHS) $< -o $@

$(LINUX_OBJS): $(LINX_BUILD_DIR)/%.o: $(LINXDIR)/%.c $(LINX_BUILD_DIR)/.git.$(GIT_COMMIT)
	mkdir -p "$(@D)"
	$(CC) $(LINX_CFLAGS) $(LINX_INCLUDE_PATHS) $< -o $@

$(LINX_BUILD_DIR)/firmware.elf : $(LINX_OBJS)
	$(CC) $(LINX_OBJS) $(LINX_LDFLAGS) -o $@

clean:
	rm -rf $(LINX_BUILD_DIR)

cppcheck:
	cppcheck --enable=all --std=c11 --inline-suppr `find $(CORE_DIR) "*.[ch]"` `find $(SENS_DIR) -name "*.[ch]"`  -I $(CORE_DIR)/include -I $(SENS_DIR)/include

test: default
	mkdir -p /tmp/osm/
	$(OSM_DIR)/python/osm_test.py


coverage: default
	lcov --zerocounters -d $(LINX_BUILD_DIR)/
	lcov --capture --initial -d $(LINX_BUILD_DIR)/ --output-file $(LINX_BUILD_DIR)/coverage.info
	mkdir -p /tmp/osm/
	$(OSM_DIR)/python/osm_test.py
	lcov --capture -d $(LINX_BUILD_DIR)/ --output-file $(LINX_BUILD_DIR)/coverage.info
	mkdir -p $(LINX_BUILD_DIR)/coverage
	cd $(LINX_BUILD_DIR)/coverage && genhtml ../coverage.info
	sensible-browser $(LINX_BUILD_DIR)/coverage/index.html


run: default
	mkdir -p /tmp/osm/
	$(OSM_DIR)/python/osm_test.py --run


soak: default
	loop=0; while [ "$$?" = "0" ]; do loop=$$(($$loop + 1));make test; done; date; echo "Loops:" $$loop


valgrind: default
	valgrind --leak-check=full $(LINX_BUILD_DIR)/firmware.elf


-include $(LINX_DEPS)
