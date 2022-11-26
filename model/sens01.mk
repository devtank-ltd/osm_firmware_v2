sens01_SOURCES := \
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
           sensors/src/ftma.c \
           sensors/src/bat.c \
           sensors/src/can_impl.c \
           sensors/src/fw.c \
           model/sens01/sens01.c \
           ports/stm/src/can_comm.c \
           ports/stm/src/stm.c \
           ports/stm/src/i2c.c \
           ports/stm/src/sleep.c \
           ports/stm/src/timers.c \
           ports/stm/src/uarts.c \
           ports/stm/src/version.c \
           ports/stm/src/w1.c

sens01_LINK_SCRIPT := ports/stm/stm32l4.ld

$(eval $(call STM_FIRMWARE,sens01))
