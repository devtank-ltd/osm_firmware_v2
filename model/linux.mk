linux_SOURCES := \
    $(OSM_DIR)/ports/linux/src/uarts.c \
    $(OSM_DIR)/ports/linux/src/linux_adc.c \
    $(OSM_DIR)/ports/linux/src/sleep.c \
    $(OSM_DIR)/ports/linux/src/platform_common.c \
    $(OSM_DIR)/ports/linux/src/can_comm.c \
    $(OSM_DIR)/ports/linux/src/linux.c \
    $(OSM_DIR)/ports/linux/src/timers.c \
    $(OSM_DIR)/ports/linux/src/i2c.c \
    $(OSM_DIR)/ports/linux/src/w1.c \
    $(OSM_DIR)/ports/linux/src/version.c \
    $(OSM_DIR)/ports/linux/src/pulsecount.c \
    $(OSM_DIR)/ports/linux/src/sai.c \
    $(OSM_DIR)/ports/linux/src/comms.c \
    $(OSM_DIR)/core/src/main.c \
    $(OSM_DIR)/core/src/base.c \
    $(OSM_DIR)/core/src/log.c \
    $(OSM_DIR)/core/src/uart_rings.c \
    $(OSM_DIR)/core/src/cmd.c \
    $(OSM_DIR)/core/src/io.c \
    $(OSM_DIR)/core/src/ring.c \
    $(OSM_DIR)/core/src/modbus.c \
    $(OSM_DIR)/core/src/modbus_mem.c \
    $(OSM_DIR)/core/src/persist_config.c \
    $(OSM_DIR)/core/src/measurements.c \
    $(OSM_DIR)/core/src/measurements_mem.c \
    $(OSM_DIR)/core/src/modbus_measurements.c \
    $(OSM_DIR)/core/src/update.c \
    $(OSM_DIR)/core/src/adcs.c \
    $(OSM_DIR)/core/src/common.c \
    $(OSM_DIR)/core/src/debug_mode.c \
    $(OSM_DIR)/sensors/src/hpm.c \
    $(OSM_DIR)/sensors/src/htu21d.c \
    $(OSM_DIR)/sensors/src/ds18b20.c \
    $(OSM_DIR)/sensors/src/veml7700.c \
    $(OSM_DIR)/sensors/src/ftma.c \
    $(OSM_DIR)/sensors/src/bat.c \
    $(OSM_DIR)/sensors/src/cc.c \
    $(OSM_DIR)/sensors/src/can_impl.c \
    $(OSM_DIR)/sensors/src/fw.c

$(eval $(call LINUX_FIRMWARE,linux))
