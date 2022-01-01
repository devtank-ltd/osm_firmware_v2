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
           modbus_measurements.c \
           update.c

PROJECT_NAME := firmware
LINK_SCRIPT = stm32l452.ld

include Makefile.base

WHOLE_IMG := $(BUILD_DIR)/complete.bin

$(BUILD_DIR)/bootloader.bin: $(BUILD_DIR)/bootloader/bootloader.o

$(BUILD_DIR)/bootloader/bootloader.o:
	$(MAKE) -C bootloader

$(WHOLE_IMG) : $(BUILD_DIR)/$(PROJECT_NAME).bin $(BUILD_DIR)/bootloader.bin
	dd of=$@ if=$(BUILD_DIR)/bootloader.bin bs=2k
	dd of=$@ if=$(BUILD_DIR)/firmware.bin seek=2 conv=notrunc bs=2k

default: $(WHOLE_IMG)

serial_program: $(WHOLE_IMG)
	KEEPCONFIG=1 ./scripts/program.sh $<


flash: $(WHOLE_IMG)
	openocd -f interface/stlink-v2-1.cfg \
		    -f target/stm32l4x.cfg \
		    -c "init" -c "reset init" \
		    -c "flash write_image erase $(WHOLE_IMG)" \
		    -c "reset" \
		    -c "shutdown"

$(TARGET_DFU): $(WHOLE_IMG)
	cp $(WHOLE_IMG) $(TARGET_DFU)
	dfu-suffix -a $(TARGET_DFU)

dfu : $(TARGET_DFU)
	dfu-util -D $(TARGET_DFU) -a 0 -s 0x08000000:leave

