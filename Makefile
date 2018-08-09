#Toolchain settings
TOOLCHAIN := arm-none-eabi

CC = $(TOOLCHAIN)-gcc
OBJCOPY = $(TOOLCHAIN)-objcopy
OBJDUMP = $(TOOLCHAIN)-objdump
SIZE = $(TOOLCHAIN)-size

#Target CPU options
CPU_DEFINES = -mthumb -mcpu=cortex-m0 -msoft-float -DSTM32F0

#Compiler options
CFLAGS		+= -Os -g -c -std=gnu99
CFLAGS		+= -Wall -Wextra -Werror 
CFLAGS		+= -MMD -MP
CFLAGS		+= -fno-common -ffunction-sections -fdata-sections
CFLAGS		+= $(CPU_DEFINES)

INCLUDE_PATHS += -Ilibs/libopencm3/include

LINK_SCRIPT = libs/libopencm3/lib/stm32/f0/stm32f07xzb.ld

LINK_FLAGS =  -Llibs/libopencm3/lib --static -nostartfiles
LINK_FLAGS += -Llibs/libopencm3/lib/stm32/f0 
LINK_FLAGS += -T$(LINK_SCRIPT) -lopencm3_stm32f0 
LINK_FLAGS += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group -Wl,--gc-sections
LINK_FLAGS += $(CPU_DEFINES)

SOURCES += main.c
BUILD_DIR := build/
PROJECT_NAME := test

OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)%.o)
DEPS = $(SOURCES:%.c=$(BUILD_DIR)%.d)
TARGET_ELF = $(BUILD_DIR)$(PROJECT_NAME).elf
TARGET_BIN = $(TARGET_ELF:%.elf=%.bin)
TARGET_HEX = $(TARGET_ELF:%.elf=%.hex)

default: $(TARGET_BIN)

$(TARGET_ELF) : $(OBJECTS) libs/libopencm3/lib/libopencm3_stm32f0.a

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -Obinary $(TARGET_ELF) $(TARGET_BIN)

$(TARGET_HEX): $(TARGET_ELF)
	$(OBJCOPY) -Oihex $(TARGET_ELF) $(TARGET_HEX)
	
$(TARGET_ELF): $(LIBS) $(OBJECTS) $(LINK_SCRIPT)
	$(CC) $(OBJECTS) $(LINK_FLAGS) -o $(TARGET_ELF)

$(OBJECTS): $(BUILD_DIR)%.o: %.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $< -o $@

libs/libopencm3/lib/libopencm3_stm32f0.a :
	$(MAKE) -C libs/libopencm3 TARGETS=stm32/f0

flash: $(TARGET_HEX)

	openocd -f interface/stlink-v2-1.cfg \
		    -f target/stm32f0x.cfg \
		    -c "init" -c "reset init" \
		    -c "flash write_image erase $(TARGET_HEX)" \
		    -c "reset" \
		    -c "shutdown"

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
