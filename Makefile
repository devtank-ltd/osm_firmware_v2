BUILD_DIR := build

DEPS = $(shell find "$(BUILD_DIR)" -name "*.d")

FW_IMG := $(BUILD_DIR)/firmware.bin
BL_IMG := $(BUILD_DIR)/bootloader.bin
WHOLE_IMG := $(BUILD_DIR)/complete.bin

default: $(WHOLE_IMG)

$(BUILD_DIR)/%.o: %.c
	$(MAKE) -f Makefile.bootloader
	$(MAKE) -f Makefile.firmware

$(WHOLE_IMG) : $(BL_IMG) $(FW_IMG)
	dd of=$@ if=$(BL_IMG) bs=2k
	dd of=$@ if=$(FW_IMG) seek=2 conv=notrunc bs=2k

$(BL_IMG):
	$(MAKE) -f Makefile.bootloader

$(FW_IMG):
	$(MAKE) -f Makefile.firmware

serial_program: $(WHOLE_IMG)
	KEEPCONFIG=1 ./scripts/program.sh $<


flash: $(WHOLE_IMG)
	openocd -f interface/stlink-v2-1.cfg \
		    -f target/stm32l4x.cfg \
		    -c "init" -c "halt" \
		    -c "program $(WHOLE_IMG) 0x08000000 verify reset exit" \
		    -c "reset" \
		    -c "shutdown"


clean:
	make -C libs/libopencm3 $@ TARGETS=stm32/l4
	rm -rf $(BUILD_DIR)


size:
	$(MAKE) -f Makefile.bootloader size
	$(MAKE) -f Makefile.firmware size

cppcheck:
	cppcheck --enable=all --std=c99 *.[ch]


-include $(DEPS)
