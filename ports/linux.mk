#Toolchain settings

LINUXDIR := $(OSM_DIR)/ports/linux

OSM_LOC ?= /tmp/osm/

LINUX_CC = gcc


#Compiler options
LINUX_DEFINES := -D_GNU_SOURCE

LINUX_CFLAGS		+= -O0 -g -std=gnu11 -pedantic $(LINUX_DEFINES)
LINUX_CFLAGS		+= -Wall -Wextra -Werror -Wno-unused-parameter -Wno-address-of-packed-member
LINUX_CFLAGS		+= -fstack-usage
LINUX_CFLAGS		+= -MMD -MP
LINUX_CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
LINUX_CFLAGS		+= -fprofile-arcs -ftest-coverage


LINUX_INCLUDE_PATHS += -I$(LINUXDIR)/include -I$(OSM_DIR)/core/include -I$(OSM_DIR)/sensors/include -I$(OSM_DIR)/protocols/include -I$(OSM_DIR)/comms/include $(shell pkg-config --cflags json-c)

LINUX_LDFLAGS = -Wl,--start-group -lc -lgcc -lm -Wl,--end-group -Wl,--gc-sections $(shell pkg-config --libs json-c)
LINUX_LDFLAGS += -lgcov --coverage -pthread -lutil

SQLITE_DB ?= $(OSM_DIR)/config_gui/release/config_database/modbus_templates


#Linux Port Dependencies
LINUX_EXES     := gcc valgrind python3 pkg-config git js
PY_MODULES :=  idlelib PIL pymodbus serial scipy tkinter xml argparse csv ctypes \
               datetime errno fnmatch json logging math multiprocessing numpy random re select \
			   selectors signal socket sqlite3 \
			   string struct subprocess threading time traceback \
			   weakref webbrowser yaml
COVERAGE_EXES := lcov genhtml python3-coverage


$(BUILD_DIR)/.linux_build_env:
	mkdir -p "$(@D)"
	for i in $(LINUX_EXES) ; do \
		if ! which $$i; then echo MISSING EXECUTABLE: $$i; exit 1; fi; \
	done
	for i in $(PY_MODULES) ; do \
		if ! python3 -c "import $$i"; then echo MISSING PYMODULE: $$i; exit 1; fi; \
	done
	touch $@

$(BUILD_DIR)/.linux_coverage:
	mkdir -p "$(@D)"
	for i in $(COVERAGE_EXES) ; do \
		if ! which $$i; then echo MISSING EXECUTABLE: $$i; exit 1; fi; \
	done
	touch $@

define LINUX_FIRMWARE
$(call PORT_BASE_RULES,$(1),LINUX)

$(1)_PERIPHERALS_SRC:=$$(shell find $$(OSM_DIR)/ports/linux/peripherals -name "*.py")

$(1)_PERIPHERALS_DST:=$$($(1)_PERIPHERALS_SRC:$$(OSM_DIR)/ports/linux/%=$$(BUILD_DIR)/$(1)/%)

$$($(1)_PERIPHERALS_DST): $$(BUILD_DIR)/$(1)/% : $$(OSM_DIR)/ports/linux/% $$(BUILD_DIR)/.linux_build_env
	mkdir -p "$$(@D)"
	cp $$< $$@

$(1)_MODEL_PERIPHERALS_SRC:=$$(shell if [ -e "$$(MODEL_DIR)/$(1)/peripherals" ]; then find $$(MODEL_DIR)/$(1)/peripherals -name "*.py"; fi)

$(1)_MODEL_PERIPHERALS_DST:=$$($(1)_MODEL_PERIPHERALS_SRC:$$(MODEL_DIR)/$(1)/%=$$(BUILD_DIR)/$(1)/%)

$$($(1)_MODEL_PERIPHERALS_DST): $$(BUILD_DIR)/$(1)/% : $$(MODEL_DIR)/$(1)/% $$(BUILD_DIR)/.linux_build_env
	mkdir -p "$$(@D)"
	cp $$< $$@

$$($(1)_OBJS) : $$(BUILD_DIR)/.linux_build_env

$$(BUILD_DIR)/$(1)/firmware.elf: $$($(1)_OBJS) $$($(1)_PERIPHERALS_DST) $$($(1)_MODEL_PERIPHERALS_DST)
	$$(LINUX_CC) $$($(1)_OBJS) $$(LINUX_LDFLAGS) -o $$@

$$(BUILD_DIR)/$(1)/.complete: $$(BUILD_DIR)/$(1)/firmware.elf
	touch $$@

$(1): $$(BUILD_DIR)/$(1)/.complete


$(1)_test: $$(BUILD_DIR)/$(1)/firmware.elf
	mkdir -p $(OSM_LOC)
	cd $$(OSM_DIR)/python/; ./osm_test.py

$(1)_coverage: $$(BUILD_DIR)/.linux_coverage $$(BUILD_DIR)/$(1)/firmware.elf
	python3-coverage erase
	lcov --zerocounters -d $$(BUILD_DIR)/$(1)/
	lcov --capture --initial -d $$(BUILD_DIR)/$(1)/ --output-file $$(BUILD_DIR)/$(1)/coverage.info
	mkdir -p $(OSM_LOC)
	cd $$(OSM_DIR)/python;\
	python3-coverage run -a ./osm_test.py;\
	python3-coverage combine;\
	python3-coverage html
	lcov --capture -d $$(BUILD_DIR)/$(1)/ --output-file $$(BUILD_DIR)/$(1)/coverage.info
	mkdir -p $$(BUILD_DIR)/$(1)/coverage
	cd $$(BUILD_DIR)/$(1)/coverage && genhtml ../coverage.info
	if [ "$$$$NOBROWSER" != "1" ]; then sensible-browser $$(BUILD_DIR)/$(1)/coverage/index.html; fi
	if [ "$$$$NOBROWSER" != "1" ]; then sensible-browser python/htmlcov/index.html; fi


$(1)_soak: $$(BUILD_DIR)/$(1)/firmware.elf
	mkdir -p $(OSM_LOC)
	cd $$(OSM_DIR)/python; loop=0; while [ "$$?" = "0" ]; do loop=$$(($$loop + 1)); ./osm_test.py --run; done; date; echo "Loops:" $$loop


$(1)_valgrind: $$(BUILD_DIR)/$(1)/firmware.elf
	valgrind --leak-check=full $$(BUILD_DIR)/$(1)/firmware.elf

endef
