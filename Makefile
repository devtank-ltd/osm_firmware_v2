#Toolchain settings
TOOLCHAIN := arm-none-eabi

CC = $(TOOLCHAIN)-gcc
OBJCOPY = $(TOOLCHAIN)-objcopy
OBJDUMP = $(TOOLCHAIN)-objdump
SIZE = $(TOOLCHAIN)-size

#Target CPU options
CPU_DEFINES =  -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -DSTM32F3

#Compiler options
CFLAGS		+= -Os -g -c -std=gnu99
CFLAGS		+= -Wall -Wextra -Werror 
CFLAGS		+= -MMD -MP
CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
CFLAGS		+= $(CPU_DEFINES)

INCLUDE_PATHS += -Ilibs/libopencm3/include -Ilibs/freertos/FreeRTOS/Source/include/ -I. -I./libs/freertos/FreeRTOS/Source/portable/GCC/ARM_CM4F/

LINK_SCRIPT = stm32f303re.ld

LINK_FLAGS =  -Llibs/libopencm3/lib --static -nostartfiles
LINK_FLAGS += -Llibs/libopencm3/lib/stm32/f3
LINK_FLAGS += -T$(LINK_SCRIPT) -lopencm3_stm32f3
LINK_FLAGS += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group -Wl,--gc-sections
LINK_FLAGS += $(CPU_DEFINES)

SOURCES += main.c \
           usb.c \
           cmd.c \
           log.c \
           libs/freertos/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c \
           libs/freertos/FreeRTOS/Source/portable/MemMang/heap_1.c \
           libs/freertos/FreeRTOS/Source/list.c \
           libs/freertos/FreeRTOS/Source/queue.c \
           libs/freertos/FreeRTOS/Source/tasks.c

BUILD_DIR := build/
PROJECT_NAME := test

OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)%.o)
DEPS = $(SOURCES:%.c=$(BUILD_DIR)%.d)
TARGET_ELF = $(BUILD_DIR)$(PROJECT_NAME).elf
TARGET_HEX = $(TARGET_ELF:%.elf=%.hex)

LIBOPENCM3 := libs/libopencm3/lib/libopencm3_stm32f3.a


default: $(TARGET_ELF)

$(BUILD_DIR)main.o : $(LIBOPENCM3)

$(TARGET_ELF): $(LIBS) $(OBJECTS) $(LINK_SCRIPT)
	$(CC) $(OBJECTS) $(LINK_FLAGS) -o $(TARGET_ELF)

$(OBJECTS): $(BUILD_DIR)%.o: %.c
	mkdir -p `dirname $@`
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $< -o $@

$(LIBOPENCM3) :
	$(MAKE) -C libs/libopencm3 TARGETS=stm32/f3

flash: $(TARGET_ELF)

	openocd -f interface/stlink-v2-1.cfg \
		    -f target/stm32f3x.cfg \
		    -c "init" -c "reset init" \
		    -c "flash write_image erase $(TARGET_ELF)" \
		    -c "reset" \
		    -c "shutdown"

clean:
	rm -rf $(BUILD_DIR)


debug_mon: $(TARGET_ELF)
	openocd -f board/st_nucleo_f3.cfg -f interface/stlink-v2-1.cfg -c "init" -c "reset init"

debug_gdb: $(TARGET_ELF)
	$(TOOLCHAIN)-gdb -ex "target remote localhost:3333" -ex "monitor reset halt" -ex load $(TARGET_ELF);

debug: $(TARGET_ELF)
	xterm -e openocd -f board/st_nucleo_f3.cfg -f interface/stlink-v2-1.cfg -c "init" -c "reset init" & \
	pid=$$!; \
	$(TOOLCHAIN)-gdb -ex "target remote localhost:3333" -ex "monitor reset halt" -ex load $(TARGET_ELF); \
	kill -9 $$pid


output:
	screen $$(ls /dev/serial/by-id/usb-STMicroelectronics_STM32_STLink* -1 | head -n 1) 115200 8n1


-include $(DEPS)
