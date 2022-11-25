
BUILD_DIR ?= build

OSM_DIR ?= $(TOOL_DIR)/../..
TOOL_DIR ?= .

CORE_DIR ?= $(OSM_DIR)/core
SENS_DIR ?= $(OSM_DIR)/sensors
COMM_DIR ?= $(OSM_DIR)/comms
MODL_DIR ?= $(OSM_DIR)/model

CFLAGS := -I$(TOOL_DIR)/include -I$(CORE_DIR)/include -I$(SENS_DIR)/include -I$(COMM_DIR)/include -I$(OSM_DIR)/libs/libopencm3/include/ $(shell pkg-config --cflags json-c) -Wno-address-of-packed-member
CFLAGS += -MMD -MP -g -DSTM32L4 -D__CONFIGTOOL__
CFLAGS += -Dfw_name=tool -DFW_NAME=TOOL
LDFLAGS := $(shell pkg-config --libs json-c)

MODELS = $(shell find $(MODL_DIR)/* -type d -printf '%f\n')

CFLAGS += $(MODELS:%=-I$(MODL_DIR)/%/)

MODELS_CONFIG_SRC = $(MODELS:%=$(MODL_DIR)/%/config_tool.c)
MODELS_BASE_SRC = $(foreach model,$(MODELS),$(MODL_DIR)/$(model)/$(model).c)

MODELS_BASE_OBJS := $(MODELS_BASE_SRC:$(MODL_DIR)/%.c=$(BUILD_DIR)/%.o)
MODELS_CONFIG_OBJS := $(MODELS_CONFIG_SRC:$(MODL_DIR)/%/config_tool.c=$(BUILD_DIR)/%/config_tool.o)

SRC := $(TOOL_DIR)/src/main.c \
       $(TOOL_DIR)/src/read_json.c \
       $(TOOL_DIR)/src/write_json.c \
       $(TOOL_DIR)/src/common_json.c

OSM_SRC := $(OSM_DIR)/core/src/base.c \
           $(OSM_DIR)/core/src/modbus_mem.c \
           $(OSM_DIR)/core/src/measurements_mem.c
OSM_SRC += $(MODELS_CONFIG_SRC)
OSM_SRC += $(MODELS_BASE_SRC)
OSM_OBJS := $(OSM_SRC:$(OSM_DIR)/%.c=$(BUILD_DIR)/%.o)


OBJS := $(SRC:%.c=$(BUILD_DIR)/%.o)
OBJS += $(OSM_OBJS)

DEPS := $(OBJS:%.o=%.d)

default: $(BUILD_DIR)/json_x_img

$(OSM_OBJS): $(BUILD_DIR)/%.o: $(OSM_DIR)/%.c
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
