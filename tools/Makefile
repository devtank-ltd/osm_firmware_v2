
BUILD_DIR := build

CFLAGS := -I.. $(shell pkg-config --cflags json-c)
CFLAGS += -MMD -MP
LDFLAGS := $(shell pkg-config --libs json-c)

SRC := main.c \
       modbus_mem.c

OBJS := $(SRC:%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:%.o=%.d)

default: $(BUILD_DIR)/init_config

$(BUILD_DIR)/%.o: %.c Makefile
	mkdir -p "$(@D)"
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/init_config : $(OBJS)
	gcc $(OBJS) $(LDFLAGS)  -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEBS)
