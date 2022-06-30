firmware_SOURCES := \
           core/src/main.c \
           core/src/base.c \
           core/src/log.c \
           core/src/uarts.c \
           core/src/uart_rings.c \
           core/src/cmd.c \
           core/src/io.c \
           core/src/ring.c \
           core/src/i2c.c \
           core/src/modbus_mem.c \
           core/src/persist_config.c \
           core/src/lorawan.c \
           core/src/measurements.c \
           core/src/measurements_mem.c \
           core/src/modbus_measurements.c \
           core/src/update.c \
           core/src/adcs.c \
           core/src/timers.c \
           core/src/common.c \
           core/src/w1.c \
           core/src/sleep.c \
           core/src/can_comm.c \
           core/src/debug_mode.c \
           sensors/src/hpm.c \
           sensors/src/htu21d.c \
           sensors/src/modbus.c \
           sensors/src/ds18b20.c \
           sensors/src/pulsecount.c \
           sensors/src/veml7700.c \
           sensors/src/sai.c \
           sensors/src/cc.c \
           sensors/src/bat.c \
           sensors/src/can_impl.c \
           sensors/src/fw.c

ifeq ($(DEV), STM32L451RE)
	firmware_LINK_SCRIPT := core/stm32l452.ld
else ifeq ($(DEV), STM32L433VTC6)
	firmware_LINK_SCRIPT := core/stm32l433.ld
else
	firmware_LINK_SCRIPT :=
endif
