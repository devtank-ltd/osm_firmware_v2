LINUX_INCLUDE_PATHS += -I$(OSM_LIB_DIR)/embedded-i2c-sen5x

penguin_lw_SOURCES := \
    $(OSM_DIR)/src/core/main.c \
    $(OSM_DIR)/src/core/base.c \
    $(OSM_DIR)/src/core/log.c \
    $(OSM_DIR)/src/core/uart_rings.c \
    $(OSM_DIR)/src/core/cmd.c \
    $(OSM_DIR)/src/core/io.c \
    $(OSM_DIR)/src/core/ring.c \
    $(OSM_DIR)/src/core/modbus.c \
    $(OSM_DIR)/src/core/modbus_mem.c \
    $(OSM_DIR)/src/core/persist_base.c \
    $(OSM_DIR)/src/core/measurements.c \
    $(OSM_DIR)/src/core/measurements_mem.c \
    $(OSM_DIR)/src/core/modbus_measurements.c \
    $(OSM_DIR)/src/core/adcs.c \
    $(OSM_DIR)/src/core/common.c \
    $(OSM_DIR)/src/ports/linux/uarts.c \
    $(OSM_DIR)/src/ports/linux/linux_adc.c \
    $(OSM_DIR)/src/ports/linux/sleep.c \
    $(OSM_DIR)/src/ports/linux/platform_common.c \
    $(OSM_DIR)/src/ports/linux/can_comm.c \
    $(OSM_DIR)/src/ports/linux/linux.c \
    $(OSM_DIR)/src/ports/linux/timers.c \
    $(OSM_DIR)/src/ports/linux/i2c.c \
    $(OSM_DIR)/src/ports/linux/w1.c \
    $(OSM_DIR)/src/ports/linux/pulsecount.c \
    $(OSM_DIR)/src/ports/linux/sai.c \
    $(OSM_DIR)/src/ports/linux/peripherals.c \
    $(OSM_DIR)/src/ports/linux/io_watch.c \
    $(OSM_DIR)/src/ports/linux/persist_config.c \
    $(OSM_DIR)/src/ports/linux/update.c \
    $(OSM_DIR)/src/ports/linux/bat.c \
    $(OSM_DIR)/src/protocols/hexblob.c \
    $(OSM_DIR)/src/protocols/comms_behind.c \
    $(OSM_DIR)/src/comms/common.c \
    $(OSM_DIR)/src/comms/linux_comms.c \
    $(OSM_DIR)/src/sensors/hpm.c \
    $(OSM_DIR)/src/sensors/sen5x.c \
    $(OSM_DIR)/src/sensors/sensirion_i2c_hal.c \
    $(OSM_DIR)/src/sensors/example_rs232.c \
    $(OSM_DIR)/src/sensors/htu21d.c \
    $(OSM_DIR)/src/sensors/ds18b20.c \
    $(OSM_DIR)/src/sensors/veml7700.c \
    $(OSM_DIR)/src/sensors/ftma.c \
    $(OSM_DIR)/src/sensors/cc.c \
    $(OSM_DIR)/src/sensors/can_impl.c \
    $(OSM_DIR)/src/sensors/fw.c \
    $(OSM_LIB_DIR)/embedded-i2c-sen5x/sensirion_common.c \
    $(OSM_LIB_DIR)/embedded-i2c-sen5x/sensirion_i2c.c \
    $(OSM_LIB_DIR)/embedded-i2c-sen5x/sen5x_i2c.c \
    $(OSM_MODEL_DIR)/penguin_lw/model.c

$(eval $(call LINUX_FIRMWARE,penguin_lw))
