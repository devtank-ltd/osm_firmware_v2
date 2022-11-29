TOOL_DIR := $(OSM_DIR)/tools/img_json_interpretter

TOOL_CFLAGS := -I$(OSM_DIR)/ports/stm/include -I$(TOOL_DIR)/include -I$(OSM_DIR)/core/include -I$(OSM_DIR)/sensors/include -I$(OSM_DIR)/comms/include -I$(OSM_DIR)/libs/libopencm3/include/ $(shell pkg-config --cflags json-c) -Wno-address-of-packed-member
TOOL_CFLAGS += -MMD -MP -g -DSTM32L4 -D__CONFIGTOOL__
TOOL_CFLAGS += -Dfw_name=tool -DFW_NAME=TOOL
TOOL_CFLAGS += -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Wno-address-of-packed-member
TOOL_LDFLAGS := $(shell pkg-config --libs json-c)

TOOL_CFLAGS += $(MODELS:%=-I$(MODEL_DIR)/%/)

TOOL_MODELS_CONFIG_SRC = $(MODELS:%=$(MODEL_DIR)/%/config_tool.c)
TOOL_MODELS_BASE_SRC = $(foreach model,$(MODELS),$(MODEL_DIR)/$(model)/$(model).c)

TOOL_MODELS_BASE_OBJS := $(TOOL_MODELS_BASE_SRC:$(MODEL_DIR)/%.c=$(BUILD_DIR)/tool/%.o)
TOOL_MODELS_CONFIG_OBJS := $(TOOL_MODELS_CONFIG_SRC:$(MODEL_DIR)/%/config_tool.c=$(BUILD_DIR)/tool/%/config_tool.o)

TOOL_MODEL_OBJS := $(TOOL_MODELS_BASE_OBJS) $(TOOL_MODELS_CONFIG_OBJS)

TOOL_SRC := \
       $(TOOL_DIR)/src/main.c \
       $(TOOL_DIR)/src/read_json.c \
       $(TOOL_DIR)/src/write_json.c \
       $(TOOL_DIR)/src/common_json.c

TOOL_OSM_SRC := \
           $(OSM_DIR)/core/src/base.c \
           $(OSM_DIR)/core/src/modbus_mem.c \
           $(OSM_DIR)/core/src/measurements_mem.c

TOOL_OBJS := $(TOOL_SRC:$(TOOL_DIR)/%.c=$(BUILD_DIR)/tool/%.o)
TOOL_OSM_OBJS := $(TOOL_OSM_SRC:$(OSM_DIR)/%.c=$(BUILD_DIR)/tool/%.o)

TOOL_ALL_OBJS := $(TOOL_OBJS) $(TOOL_OSM_OBJS) $(TOOL_MODEL_OBJS)

$(TOOL_OBJS): $(BUILD_DIR)/tool/%.o: $(TOOL_DIR)/%.c $(TOOL_DIR)/tool.mk $(LIBOPENCM3)
	mkdir -p "$(@D)"
	$(CC) -c $(TOOL_CFLAGS) $< -o $@

$(TOOL_OSM_OBJS): $(BUILD_DIR)/tool/%.o: $(OSM_DIR)/%.c $(TOOL_DIR)/tool.mk $(LIBOPENCM3)
	mkdir -p "$(@D)"
	$(CC) -c $(TOOL_CFLAGS) $< -o $@

$(TOOL_MODEL_OBJS): $(BUILD_DIR)/tool/%.o: $(MODEL_DIR)/%.c $(TOOL_DIR)/tool.mk $(LIBOPENCM3)
	mkdir -p "$(@D)"
	$(CC) -c $(TOOL_CFLAGS) $< -o $@

$(BUILD_DIR)/tool/json_x_img : $(TOOL_ALL_OBJS)
	gcc $(TOOL_ALL_OBJS) $(TOOL_LDFLAGS)  -o $@
