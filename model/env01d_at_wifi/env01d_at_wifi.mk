STM_INCLUDE_PATHS += -I$(OSM_LIB_DIR)/embedded-i2c-sen5x

env01d_at_wifi_SOURCES := \
           $(OSM_DIR)/core/src/main.c \
           $(OSM_DIR)/core/src/base.c \
           $(OSM_DIR)/core/src/log.c \
           $(OSM_DIR)/core/src/uart_rings.c \
           $(OSM_DIR)/core/src/cmd.c \
           $(OSM_DIR)/core/src/io.c \
           $(OSM_DIR)/core/src/ring.c \
           $(OSM_DIR)/core/src/modbus_mem.c \
           $(OSM_DIR)/ports/stm/src/persist_config.c \
           $(OSM_DIR)/core/src/persist_base.c \
           $(OSM_DIR)/core/src/modbus.c \
           $(OSM_DIR)/core/src/measurements.c \
           $(OSM_DIR)/core/src/measurements_mem.c \
           $(OSM_DIR)/core/src/modbus_measurements.c \
           $(OSM_DIR)/ports/stm/src/update.c \
           $(OSM_DIR)/core/src/adcs.c \
           $(OSM_DIR)/core/src/common.c \
           $(OSM_DIR)/core/src/platform_common.c \
           $(OSM_DIR)/ports/stm/src/comms_direct.c \
           $(OSM_DIR)/protocols/src/jsonblob.c \
           $(OSM_DIR)/protocols/src/comms_behind.c \
           $(OSM_DIR)/comms/src/at_base.c \
           $(OSM_DIR)/comms/src/at_mqtt.c \
           $(OSM_DIR)/comms/src/at_wifi.c \
           $(OSM_DIR)/comms/src/common.c \
           $(OSM_DIR)/sensors/src/sen54.c \
           $(OSM_DIR)/sensors/src/sensirion_i2c_hal.c \
           $(OSM_LIB_DIR)/embedded-i2c-sen5x/sensirion_common.c \
           $(OSM_LIB_DIR)/embedded-i2c-sen5x/sensirion_i2c.c \
           $(OSM_LIB_DIR)/embedded-i2c-sen5x/sen5x_i2c.c \
           $(OSM_DIR)/sensors/src/ds18b20.c \
           $(OSM_DIR)/sensors/src/pulsecount.c \
           $(OSM_DIR)/sensors/src/veml7700.c \
           $(OSM_DIR)/sensors/src/sai.c \
           $(OSM_DIR)/sensors/src/cc.c \
           $(OSM_DIR)/sensors/src/can_impl.c \
           $(OSM_DIR)/sensors/src/fw.c \
           $(OSM_DIR)/sensors/src/io_watch.c \
           $(OSM_MODEL_DIR)/env01d_at_wifi/model.c \
           $(OSM_DIR)/ports/stm/src/can_comm.c \
           $(OSM_DIR)/ports/stm/src/stm.c \
           $(OSM_DIR)/ports/stm/src/i2c.c \
           $(OSM_DIR)/ports/stm/src/sleep.c \
           $(OSM_DIR)/ports/stm/src/timers.c \
           $(OSM_DIR)/ports/stm/src/uarts.c \
           $(OSM_DIR)/ports/stm/src/w1.c

env01d_at_wifi_LINK_SCRIPT := $(OSM_DIR)/ports/stm/stm32l4.ld

$(eval $(call STM_FIRMWARE,env01d_at_wifi))
