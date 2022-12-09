#Toolchain settings

LINUXDIR := $(OSM_DIR)/ports/linux

LINUX_CC = gcc


#Compiler options
LINUX_DEFINES := -D_GNU_SOURCE -DGIT_VERSION=\"[$(GIT_COMMITS)]-$(GIT_COMMIT)\" -DGIT_SHA1=\"$(GIT_SHA1)\"

LINUX_CFLAGS		+= -O0 -g -std=gnu11 -pedantic $(LINUX_DEFINES)
LINUX_CFLAGS		+= -Wall -Wextra -Werror -Wno-unused-parameter -Wno-address-of-packed-member
LINUX_CFLAGS		+= -fstack-usage
LINUX_CFLAGS		+= -MMD -MP
LINUX_CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
LINUX_CFLAGS		+= -fprofile-arcs -ftest-coverage


LINUX_INCLUDE_PATHS += -I$(LINUXDIR)/include -I$(OSM_DIR)/core/include -I$(OSM_DIR)/sensors/include -I$(OSM_DIR)/comms/include $(shell pkg-config --cflags json-c)

LINUX_LDFLAGS = -Wl,--start-group -lc -lgcc -lm -Wl,--end-group -Wl,--gc-sections $(shell pkg-config --libs json-c)
LINUX_LDFLAGS += -lgcov --coverage -pthread -lutil

SQLITE_DB ?= $(OSM_DIR)/config_gui/release/config_database/modbus_templates


#Linux Port Dependencies
GCC     := arm-none-eabi-gcc
VALG    := valgrind
PY      := python3
MODULES := basetypes i2c_device_t chirpstack_api.as_pb.external api collections OrderedDict crccheck.crc CrcModbus crccheck.crc CrcModbus __future__ print_function i2c_htu21d i2c_device_htu21d_t i2c_veml7700 i2c_device_veml7700_t idlelib.tooltip Hovertip imp reload influxdb.exceptions InfluxDBClientError influxdb InfluxDBClient modbus_db modb_database_t find_path modbus_funcs modbus_funcs_t PIL ImageTk Image pymodbus.constants Endian pymodbus.datastore ModbusSlaveContext ModbusServerContext ModbusSparseDataBlock pymodbus.device ModbusDeviceIdentification pymodbus.payload BinaryPayloadBuilder pymodbus.server StartSerialServer pymodbus.server.sync StartSerialServer pymodbus.transaction ModbusBinaryFramer pymodbus.version version scipy.interpolate interp1d setuptools setup stat tkinter.filedialog askopenfilename tkinter tkinter.ttk Combobox Notebook Progressbar Style xml.etree ElementTree argparse argparse array basetypes binding can comms_connection comms csv ctypes datetime errno fnmatch grpc io json libsocketcan logging math matplotlib.pyplot modbus_db modbus_server multiprocessing numpy os platform random re select selectors serial serial.tools.list_ports signal socket socket_server_base sqlite3 string struct subprocess sys sys threading time tkinter.messagebox traceback unittest usb.core usb.util weakref webbrowser xmlrunner yaml

define LINUX_MODEL_PERIPHERAL
$$(BUILD_DIR)/$(1)/peripherals/$(2) : $$(MODEL_DIR)/$(1)/peripherals/$(2)
	mkdir -p "$$(@D)"
	cp -v $$< $$@

$$(BUILD_DIR)/$(1)/firmware.elf : $$(BUILD_DIR)/$(1)/peripherals/$(2)
endef

define LINUX_FIRMWARE
$(call PORT_BASE_RULES,$(1),LINUX)

$(1)_PERIPHERALS_SRC:=$$(shell find $$(OSM_DIR)/ports/linux/peripherals -name "*.py")

$(1)_PERIPHERALS_DST:=$$($(1)_PERIPHERALS_SRC:$$(OSM_DIR)/ports/linux/%=$$(BUILD_DIR)/$(1)/%)

$$($(1)_PERIPHERALS_DST): $$(BUILD_DIR)/$(1)/% : $$(OSM_DIR)/ports/linux/% $(BUILD_DIR)/.linux_build_env
	mkdir -p "$$(@D)"
	cp $$< $$@

$(BUILD_DIR)/.linux_build_env:
	which $(GCC) || (echo EXITING.. MISSING PACKAGE: $(GCC); exit 1)
	which $(VALG) || (echo EXITING.. MISSING PACKAGE: $(VALG); exit 1)
	which $(PY) || (echo EXITING.. MISSING PACKAGE: $(PY); exit 1)
	(for i in $(MODULES); do \
		python -c "import $$i" || (echo MISSING PYMODULE: $$i; exit 1) ; \
	done)
$$(BUILD_DIR)/$(1)/firmware.elf: $$($(1)_OBJS) $$($(1)_PERIPHERALS_DST)
	$$(LINUX_CC) $$($(1)_OBJS) $$(LINUX_LDFLAGS) -o $$@

$$(BUILD_DIR)/$(1)/.complete: $$(BUILD_DIR)/$(1)/firmware.elf
	touch $$@

$(1): $$(BUILD_DIR)/$(1)/.complete


$(1)_test: $$(BUILD_DIR)/$(1)/firmware.elf
	mkdir -p /tmp/osm/
	cd $$(OSM_DIR)/python/; ./osm_test.py

$(1)_coverage: $$(BUILD_DIR)/$(1)/firmware.elf
	lcov --zerocounters -d $$(BUILD_DIR)/$(1)/
	lcov --capture --initial -d $$(BUILD_DIR)/$(1)/ --output-file $$(BUILD_DIR)/$(1)/coverage.info
	mkdir -p /tmp/osm/
	cd $$(OSM_DIR)/python; ./osm_test.py
	lcov --capture -d $$(BUILD_DIR)/$(1)/ --output-file $$(BUILD_DIR)/$(1)/coverage.info
	mkdir -p $$(BUILD_DIR)/$(1)/coverage
	cd $$(BUILD_DIR)/$(1)/coverage && genhtml ../coverage.info
	sensible-browser $$(BUILD_DIR)/$(1)/coverage/index.html


$(1)_soak: $$(BUILD_DIR)/$(1)/firmware.elf
	mkdir -p /tmp/osm/
	cd $$(OSM_DIR)/python; loop=0; while [ "$$?" = "0" ]; do loop=$$(($$loop + 1)); ./osm_test.py --run; done; date; echo "Loops:" $$loop


$(1)_valgrind: $$(BUILD_DIR)/$(1)/firmware.elf
	valgrind --leak-check=full $$(BUILD_DIR)/$(1)/firmware.elf

endef
