BUILD_DIR ?= build

TOOL_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
OSM_DIR ?= $(TOOL_DIR)/../..

CORE_DIR ?= $(OSM_DIR)/core
SENS_DIR ?= $(OSM_DIR)/sensors
COMM_DIR ?= $(OSM_DIR)/comms
STM_DIR ?= $(OSM_DIR)/ports/stm

MODL_DIR ?= $(OSM_DIR)/model

TOOL_CFLAGS := -I$(STM_DIR)/include -I$(TOOL_DIR)/include -I$(CORE_DIR)/include -I$(SENS_DIR)/include -I$(COMM_DIR)/include -I$(OSM_DIR)/libs/libopencm3/include/ $(shell pkg-config --cflags json-c) -Wno-address-of-packed-member
TOOL_CFLAGS += -MMD -MP -g -DSTM32L4 -D__CONFIGTOOL__
TOOL_CFLAGS += -Dfw_name=tool -DFW_NAME=TOOL
TOOL_LDFLAGS := $(shell pkg-config --libs json-c)

ifndef MODELS
MODELS = $(shell find $(MODL_DIR)/* -type d -printf '%f\n')
endif

TOOL_CFLAGS += $(MODELS:%=-I$(MODL_DIR)/%/)

TOOL_MODELS_CONFIG_SRC = $(MODELS:%=$(MODL_DIR)/%/config_tool.c)
TOOL_MODELS_BASE_SRC = $(foreach model,$(MODELS),$(MODL_DIR)/$(model)/$(model).c)

TOOL_MODELS_BASE_OBJS := $(TOOL_MODELS_BASE_SRC:$(MODL_DIR)/%.c=$(BUILD_DIR)/%.o)
TOOL_MODELS_CONFIG_OBJS := $(TOOL_MODELS_CONFIG_SRC:$(MODL_DIR)/%/config_tool.c=$(BUILD_DIR)/%/config_tool.o)

TOOL_SRC := \
       $(TOOL_DIR)/src/main.c \
       $(TOOL_DIR)/src/read_json.c \
       $(TOOL_DIR)/src/write_json.c \
       $(TOOL_DIR)/src/common_json.c

TOOL_OSM_SRC := \
           $(OSM_DIR)/core/src/base.c \
           $(OSM_DIR)/core/src/modbus_mem.c \
           $(OSM_DIR)/core/src/measurements_mem.c
TOOL_OSM_SRC += $(TOOL_MODELS_CONFIG_SRC)
TOOL_OSM_SRC += $(TOOL_MODELS_BASE_SRC)
TOOL_OSM_OBJS := $(TOOL_OSM_SRC:$(OSM_DIR)/%.c=$(BUILD_DIR)/tool/%.o)


TOOL_OBJS := $(TOOL_SRC:%.c=$(BUILD_DIR)/tool/%.o)
TOOL_OBJS += $(TOOL_OSM_OBJS)

TOOL_DEPS := $(TOOL_OBJS:%.o=%.d)

default: $(BUILD_DIR)/tool/json_x_img

$(TOOL_OSM_OBJS): $(BUILD_DIR)/tool/%.o: $(OSM_DIR)/%.c $(TOOL_DIR)/Makefile
	mkdir -p "$(@D)"
	$(CC) -c $(TOOL_CFLAGS) $< -o $@

$(BUILD_DIR)/tool/%.o: %.c $(TOOL_DIR)/Makefile
	mkdir -p "$(@D)"
	$(CC) -c $(TOOL_CFLAGS) $< -o $@


$(BUILD_DIR)/tool/json_x_img : $(TOOL_OBJS)
	gcc $(TOOL_OBJS) $(TOOL_LDFLAGS)  -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(TOOL_DEBS)
