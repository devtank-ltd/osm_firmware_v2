env01c_SOURCES := \
    $(OSM_DIR)/src/core/main.c \
    $(OSM_DIR)/src/core/base.c \
    $(OSM_DIR)/src/core/log.c \
    $(OSM_DIR)/src/core/uart_rings.c \
    $(OSM_DIR)/src/core/cmd.c \
    $(OSM_DIR)/src/core/io.c \
    $(OSM_DIR)/src/core/ring.c \
    $(OSM_DIR)/src/core/modbus_mem.c \
    $(OSM_DIR)/src/core/persist_base.c \
    $(OSM_DIR)/src/core/modbus.c \
    $(OSM_DIR)/src/core/measurements.c \
    $(OSM_DIR)/src/core/measurements_mem.c \
    $(OSM_DIR)/src/core/modbus_measurements.c \
    $(OSM_DIR)/src/core/adcs.c \
    $(OSM_DIR)/src/core/common.c \
    $(OSM_DIR)/src/core/platform_common.c \
    $(OSM_DIR)/src/ports/stm/persist_config.c \
    $(OSM_DIR)/src/ports/stm/update.c \
    $(OSM_DIR)/src/ports/stm/comms_direct.c \
    $(OSM_DIR)/src/ports/stm/can_comm.c \
    $(OSM_DIR)/src/ports/stm/stm.c \
    $(OSM_DIR)/src/ports/stm/i2c.c \
    $(OSM_DIR)/src/ports/stm/sleep.c \
    $(OSM_DIR)/src/ports/stm/timers.c \
    $(OSM_DIR)/src/ports/stm/uarts.c \
    $(OSM_DIR)/src/ports/stm/w1.c \
    $(OSM_DIR)/src/ports/stm/bat.c \
    $(OSM_DIR)/src/protocols/hexblob.c \
    $(OSM_DIR)/src/protocols/comms_behind.c \
    $(OSM_DIR)/src/comms/common.c \
    $(OSM_DIR)/src/comms/lw.c \
    $(OSM_DIR)/src/comms/rak3172.c \
    $(OSM_DIR)/src/sensors/hpm.c \
    $(OSM_DIR)/src/sensors/htu21d.c \
    $(OSM_DIR)/src/sensors/ds18b20.c \
    $(OSM_DIR)/src/sensors/pulsecount.c \
    $(OSM_DIR)/src/sensors/veml7700.c \
    $(OSM_DIR)/src/sensors/sai.c \
    $(OSM_DIR)/src/sensors/cc.c \
    $(OSM_DIR)/src/sensors/can_impl.c \
    $(OSM_DIR)/src/sensors/fw.c \
    $(OSM_DIR)/src/sensors/io_watch.c \
    $(OSM_MODEL_DIR)/env01c/model.c \

env01c_LINK_SCRIPT := $(OSM_DIR)/src/ports/stm/stm32l4.ld

$(eval $(call STM_FIRMWARE,env01c))
