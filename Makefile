SOURCES += main.c \
           log.c \
           sai.c \
           uarts.c \
           uart_rings.c \
           cmd.c \
           io.c \
           ring.c \
           hpm.c \
           modbus.c \
           value.c \
           persist_config.c \
           lorawan.c \
           measurements.c \
           modbus_measurements.c

PROJECT_NAME := firmware
LINK_SCRIPT = stm32l452.ld

include Makefile.base

$(BUILD_DIR)/bootloader.bin: bootloader/bootloader.c
	$(MAKE) -C bootloader

$(BUILD_DIR)/complete.bin : $(BUILD_DIR)/$(PROJECT_NAME).bin $(BUILD_DIR)/bootloader.bin
	dd of=$@ if=$(BUILD_DIR)/bootloader.bin bs=2k
	dd of=$@ if=$(BUILD_DIR)/firmware.bin seek=2 conv=notrunc bs=2k

prod: $(BUILD_DIR)/complete.bin
	KEEPCONFIG=1 ./scripts/program.sh $<
