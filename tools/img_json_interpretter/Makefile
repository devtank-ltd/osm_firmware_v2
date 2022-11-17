
BUILD_DIR := build

FW_NAME ?= sens01

CFLAGS := -I../../core/include -I../../sensors/include -I../../comms/include -I../../model/$(FW_NAME)/ -I../../libs/libopencm3/include/ -Iinclude $(shell pkg-config --cflags json-c) -Wno-address-of-packed-member
CFLAGS += -MMD -MP -g -DSTM32L4 -D__CONFIGTOOL__
LDFLAGS := $(shell pkg-config --libs json-c)

SRC := src/main.c \
       src/base.c \
       src/modbus_mem.c \
       src/measurements_mem.c \
       src/$(FW_NAME).c \
       src/read_json.c \
       src/write_json.c

OBJS := $(SRC:%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:%.o=%.d)

default: $(BUILD_DIR)/json_x_img

$(BUILD_DIR)/%.o: %.c Makefile
	mkdir -p "$(@D)"
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/json_x_img : $(OBJS)
	gcc $(OBJS) $(LDFLAGS)  -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEBS)
