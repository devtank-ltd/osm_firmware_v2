#Toolchain settings

LINUXDIR := $(OSM_DIR)/ports/linux

LINUX_CC = gcc

SENS_DIR ?= $(OSM_DIR)/sensors
COMM_DIR ?= $(OSM_DIR)/comms
MODL_DIR ?= $(OSM_DIR)/model

#Compiler options
LINUX_CFLAGS		+= -O0 -g -c -std=gnu11 -pedantic
LINUX_CFLAGS		+= -Wall -Wextra -Werror -fms-extensions -Wno-unused-parameter -Wno-address-of-packed-member
LINUX_CFLAGS		+= -fstack-usage -Wstack-usage=500
LINUX_CFLAGS		+= -MMD -MP
LINUX_CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
LINUX_CFLAGS		+= -DGIT_VERSION=\"[$(GIT_COMMITS)]-$(GIT_COMMIT)\" -DGIT_SHA1=\"$(GIT_SHA1)\" -D__LINUX__
LINUX_CFLAGS		+= -g -fprofile-arcs -ftest-coverage


LINUX_INCLUDE_PATHS += -I$(LINUXDIR)/include -I$(CORE_DIR)/include -I$(SENS_DIR)/include -I$(COMM_DIR)/include $(shell pkg-config --cflags json-c)

LINUX_LDFLAGS = -Wl,--start-group -lc -lgcc -lm -Wl,--end-group -Wl,--gc-sections $(shell pkg-config --libs json-c)
LINUX_LDFLAGS += -lgcov --coverage -pthread -lutil


SQLITE_DB = $(OSM_DIR)/config_gui/release/config_database/modbus_templates

define LINUX_FIRMWARE
$(1)_UP_NAME=$$(shell echo $(1) | tr a-z A-Z)
$(1)_OBJS=$$($(1)_SOURCES:%.c=$$(BUILD_DIR)/$(1)/%.o)

$$(BUILD_DIR)/$(1)/%.o: $$(OSM_DIR)/%.c $$(BUILD_DIR)/.git.$$(GIT_COMMIT)
	mkdir -p "$$(@D)"
	$$(LINUX_CC) -c -Dfw_name=$(1) -DFW_NAME=$$($(1)_UP_NAME) $$(LINUX_CFLAGS) -I$$(OSM_DIR)/model/$(1) $$(LINUX_INCLUDE_PATHS) $$< -o $$@

$$(BUILD_DIR)/$(1)/firmware.elf: $$($(1)_OBJS)
	$$(LINUX_CC) $$($(1)_OBJS) $$(LINUX_LDFLAGS) -o $$@

$$(BUILD_DIR)/$(1)/.complete: $$(BUILD_DIR)/$(1)/firmware.elf
	touch $$@

$(1)_test: $$(BUILD_DIR)/$(1)/firmware.elf
	mkdir -p /tmp/osm/
	$$(OSM_DIR)/python/osm_test.py

$(1)_coverage: $$(BUILD_DIR)/$(1)/firmware.elf
	lcov --zerocounters -d $$(BUILD_DIR)/$(1)/
	lcov --capture --initial -d $$(BUILD_DIR)/$(1)/ --output-file $$(BUILD_DIR)/$(1)/coverage.info
	mkdir -p /tmp/osm/
	$$(OSM_DIR)/python/osm_test.py
	lcov --capture -d $$(BUILD_DIR)/$(1)/ --output-file $$(BUILD_DIR)/$(1)/coverage.info
	mkdir -p $$(BUILD_DIR)/$(1)/coverage
	cd $$(BUILD_DIR)/$(1)/coverage && genhtml ../coverage.info
	sensible-browser $$(BUILD_DIR)/$(1)/coverage/index.html


$(1)_run: $$(BUILD_DIR)/$(1)/firmware.elf
	mkdir -p /tmp/osm/
	$$(OSM_DIR)/python/osm_test.py --run


$(1)_soak: $$(BUILD_DIR)/$(1)/firmware.elf
	mkdir -p /tmp/osm/
	loop=0; while [ "$$?" = "0" ]; do loop=$$(($$loop + 1));$$(OSM_DIR)/python/osm_test.py --run; done; date; echo "Loops:" $$loop


$(1)_valgrind: $$(BUILD_DIR)/$(1)/firmware.elf
	valgrind --leak-check=full $$(BUILD_DIR)/$(1)/firmware.elf

endef
