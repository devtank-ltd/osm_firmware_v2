
BUILD_DIR := build

CFLAGS := -Iinclude -I../../core/include -I../../sensors/include -I../../comms/include -I../../model/env01/ -I../../model/sens01/ -I../../libs/libopencm3/include/ $(shell pkg-config --cflags json-c) -Wno-address-of-packed-member
CFLAGS += -MMD -MP -g -DSTM32L4 -D__CONFIGTOOL__
CFLAGS += -Dfw_name=tool -DFW_NAME=TOOL
LDFLAGS := $(shell pkg-config --libs json-c)

MODELS = $(shell find ../../model/* -type d -printf '%f\n')

MODELS_CONFIG_SRC = $(MODELS:%=../../model/%/config_tool.c)

SRC := src/main.c \
       src/base.c \
       src/modbus_mem.c \
       src/measurements_mem.c \
       src/read_json.c \
       src/write_json.c \
       src/common_json.c

OBJS := $(SRC:%.c=$(BUILD_DIR)/%.o)

OBJS += $(MODELS_CONFIG_SRC:../../model/%/config_tool.c=$(BUILD_DIR)/%/config_tool.o)

DEPS := $(OBJS:%.o=%.d)

default: $(BUILD_DIR)/json_x_img


$(BUILD_DIR)/%/config_tool.o : ../../model/%/config_tool.c
	mkdir -p "$(@D)"
	$(CC) -c $(CFLAGS) -I../../model/`basename $(@D)` $< -o $@

$(BUILD_DIR)/%.o: %.c Makefile
	mkdir -p "$(@D)"
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c Makefile
	mkdir -p "$(@D)"
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/json_x_img : $(OBJS)
	gcc $(OBJS) $(LDFLAGS)  -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEBS)
