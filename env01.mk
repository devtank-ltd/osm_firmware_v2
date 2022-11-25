env01_SOURCES := \
           core/src/main.c \
           core/src/base.c \
           core/src/log.c \
           core/src/uart_rings.c \
           core/src/cmd.c \
           core/src/io.c \
           core/src/ring.c \
           core/src/modbus_mem.c \
           core/src/persist_config.c \
           core/src/modbus.c \
           core/src/measurements.c \
           core/src/measurements_mem.c \
           core/src/modbus_measurements.c \
           core/src/update.c \
           core/src/adcs.c \
           core/src/common.c \
           core/src/platform_common.c \
           core/src/debug_mode.c \
           comms/src/comms.c \
           comms/src/lw.c \
           comms/src/rak4270.c \
           comms/src/rak3172.c \
           sensors/src/hpm.c \
           sensors/src/htu21d.c \
           sensors/src/ds18b20.c \
           sensors/src/pulsecount.c \
           sensors/src/veml7700.c \
           sensors/src/sai.c \
           sensors/src/cc.c \
           sensors/src/bat.c \
           sensors/src/can_impl.c \
           sensors/src/fw.c \
           model/env01/env01.c \
           stm/src/can_comm.c \
           stm/src/stm.c \
           stm/src/i2c.c \
           stm/src/sleep.c \
           stm/src/timers.c \
           stm/src/uarts.c \
           stm/src/version.c \
           stm/src/w1.c

env01_LINK_SCRIPT := stm/stm32l4.ld
