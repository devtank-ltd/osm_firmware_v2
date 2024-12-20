LINUX_INCLUDE_PATHS += -I$(OSM_LIB_DIR)/embedded-i2c-sen5x

penguin_at_wifi_SOURCES := \
    $(OSM_DIR)/ports/linux/src/uarts.c \
    $(OSM_DIR)/ports/linux/src/linux_adc.c \
    $(OSM_DIR)/ports/linux/src/sleep.c \
    $(OSM_DIR)/ports/linux/src/platform_common.c \
    $(OSM_DIR)/ports/linux/src/can_comm.c \
    $(OSM_DIR)/ports/linux/src/linux.c \
    $(OSM_DIR)/ports/linux/src/timers.c \
    $(OSM_DIR)/ports/linux/src/i2c.c \
    $(OSM_DIR)/ports/linux/src/w1.c \
    $(OSM_DIR)/ports/linux/src/pulsecount.c \
    $(OSM_DIR)/ports/linux/src/sai.c \
    $(OSM_DIR)/ports/linux/src/peripherals.c \
    $(OSM_DIR)/ports/linux/src/io_watch.c \
    $(OSM_DIR)/core/src/main.c \
    $(OSM_DIR)/core/src/base.c \
    $(OSM_DIR)/core/src/log.c \
    $(OSM_DIR)/core/src/uart_rings.c \
    $(OSM_DIR)/core/src/cmd.c \
    $(OSM_DIR)/core/src/io.c \
    $(OSM_DIR)/core/src/ring.c \
    $(OSM_DIR)/core/src/modbus.c \
    $(OSM_DIR)/core/src/modbus_mem.c \
    $(OSM_DIR)/ports/linux/src/persist_config.c \
    $(OSM_DIR)/core/src/persist_base.c \
    $(OSM_DIR)/core/src/measurements.c \
    $(OSM_DIR)/core/src/measurements_mem.c \
    $(OSM_DIR)/core/src/modbus_measurements.c \
    $(OSM_DIR)/ports/linux/src/update.c \
    $(OSM_DIR)/core/src/adcs.c \
    $(OSM_DIR)/core/src/common.c \
    $(OSM_DIR)/sensors/src/example_rs232.c \
    $(OSM_DIR)/protocols/src/jsonblob.c \
    $(OSM_DIR)/protocols/src/comms_behind.c \
    $(OSM_DIR)/sensors/src/hpm.c \
    $(OSM_DIR)/sensors/src/htu21d.c \
    $(OSM_DIR)/sensors/src/ds18b20.c \
    $(OSM_DIR)/sensors/src/veml7700.c \
    $(OSM_DIR)/sensors/src/ftma.c \
    $(OSM_DIR)/sensors/src/sen5x.c \
    $(OSM_DIR)/sensors/src/sensirion_i2c_hal.c \
    $(OSM_LIB_DIR)/embedded-i2c-sen5x/sensirion_common.c \
    $(OSM_LIB_DIR)/embedded-i2c-sen5x/sensirion_i2c.c \
    $(OSM_LIB_DIR)/embedded-i2c-sen5x/sen5x_i2c.c \
    $(OSM_DIR)/ports/linux/src/bat.c \
    $(OSM_DIR)/sensors/src/cc.c \
    $(OSM_DIR)/sensors/src/can_impl.c \
    $(OSM_DIR)/sensors/src/fw.c \
    $(OSM_MODEL_DIR)/penguin_at_wifi/model.c \
    $(OSM_DIR)/comms/src/common.c \
    $(OSM_DIR)/comms/src/at_base.c \
    $(OSM_DIR)/comms/src/at_mqtt.c \
    $(OSM_DIR)/comms/src/at_wifi.c

$(eval $(call LINUX_FIRMWARE,penguin_at_wifi))
