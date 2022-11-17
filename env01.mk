env01_SOURCES := \
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
           core/src/persist_config_base.c \
           core/src/modbus.c \
           core/src/measurements.c \
           core/src/measurements_mem.c \
           core/src/modbus_measurements.c \
           core/src/update.c \
           core/src/adcs.c \
           core/src/timers.c \
           core/src/common.c \
           core/src/platform_common.c \
           core/src/w1.c \
           core/src/sleep.c \
           core/src/can_comm.c \
           core/src/debug_mode.c \
           core/src/version.c \
           core/src/stm.c \
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
           model/env01/persist_config.c
env01_LINK_SCRIPT := core/stm32l4.ld
