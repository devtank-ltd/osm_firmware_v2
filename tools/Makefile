
BUILD_DIR := build

CFLAGS := -I..

SRC := main.c \
       modbus_mem.c

OBJS:= $(SRC:%.c=$(BUILD_DIR)/%.o)
  
 
$(BUILD_DIR)/%.o: %.c
	mkdir -p "$(@D)"
	$(CC) -c $(CFLAGS) $< -o $@


init_config : $(OBJS)
	gcc $(OBJS) -o $@
